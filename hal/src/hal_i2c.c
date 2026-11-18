/**
 * @file    hal_i2c.c
 * @brief   Driver I2C1 Remap (PB8=SCL, PB9=SDA @ 400kHz) và Callback cho U8g2
 * @author  Dev 2 (UI & Flash) — Nhánh feature/hal-i2c-spi
 * @note    Sử dụng Thư viện chuẩn STM32F10x Standard Peripheral Library (STD/SPL v3.5.0)
 */

#include "hal_i2c.h"
#include "bsp_board.h"
#include "u8g2.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

/* =========================================================================
 * 1. HẰNG SỐ CẤU HÌNH I2C1, DMA1_CHANNEL6 & TIMEOUT CHỐNG TREO BUS
 * ========================================================================= */
#define HAL_I2C_CLOCK_SPEED         ( 400000U ) /* 400kHz Fast Mode */
#define HAL_I2C_TIMEOUT_MAX         ( 10000U )  /* Giới hạn vòng lặp chờ cờ phần cứng I2C1 */
#define HAL_I2C_DMA_BUF_SIZE        ( 520U )    /* Đủ chứa 1 byte điều khiển + 512B khung hình OLED */
#define HAL_I2C_DMA_THRESHOLD       ( 4U )      /* Gói > 4 bytes dùng DMA1_Channel6 nhường CPU */
#define HAL_I2C_DMA_TIMEOUT_MS      ( 25U )     /* Timeout chờ truyền xong qua Semaphore */

static uint8_t           s_i2c_tx_buf[HAL_I2C_DMA_BUF_SIZE];
static uint16_t          s_i2c_tx_len    = 0U;
static uint8_t           s_i2c_addr      = 0x78U;
static SemaphoreHandle_t s_i2c_dma_sem   = NULL;
static volatile bool     s_i2c_dma_error = false;

/* =========================================================================
 * 2. CÁC HÀM NỘI BỘ (STATIC HELPERS)
 * ========================================================================= */

/**
 * @brief Tạo trễ ngắn (~5us ở 72MHz) phục vụ xung phục hồi bus I2C thủ công
 */
static void prv_i2c_short_delay(void)
{
    volatile uint32_t cnt = 72U;

    while (cnt > 0U)
    {
        __NOP();
        cnt--;
    }
}

/**
 * @brief Phục hồi bus I2C1 khi đường SDA bị kẹt mức thấp do nhiễu đóng cắt MOSFET
 *        (Phát tối đa 9 xung SCL thủ công, tạo STOP, reset I2C1 và khởi tạo lại)
 */
static void prv_i2c_recover(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t          i;

    I2C_Cmd(I2C1, DISABLE);
    I2C_SoftwareResetCmd(I2C1, ENABLE);
    I2C_SoftwareResetCmd(I2C1, DISABLE);

    /* Chuyển tạm PB8 (SCL) và PB9 (SDA) sang GPIO Open-Drain để giải phóng Slave */
    GPIO_SetBits(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SCL_PIN | BSP_OLED_SDA_PIN);
    GPIO_InitStructure.GPIO_Pin   = BSP_OLED_SCL_PIN | BSP_OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_OLED_I2C_GPIO_PORT, &GPIO_InitStructure);

    for (i = 0U; i < 9U; i++)
    {
        if (GPIO_ReadInputDataBit(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SDA_PIN) != Bit_RESET)
        {
            break;
        }
        GPIO_ResetBits(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SCL_PIN);
        prv_i2c_short_delay();
        GPIO_SetBits(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SCL_PIN);
        prv_i2c_short_delay();
    }

    /* Tạo điều kiện STOP thủ công: SDA thấp -> SCL cao -> SDA cao */
    GPIO_ResetBits(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SDA_PIN);
    prv_i2c_short_delay();
    GPIO_SetBits(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SCL_PIN);
    prv_i2c_short_delay();
    GPIO_SetBits(BSP_OLED_I2C_GPIO_PORT, BSP_OLED_SDA_PIN);
    prv_i2c_short_delay();

    /* Khởi tạo lại ngoại vi I2C1 ở tốc độ 400kHz */
    hal_i2c_init();
}

/**
 * @brief Chờ sự kiện I2C1 kèm bộ đếm timeout và tự phục hồi bus nếu treo
 */
static bool prv_i2c_wait_event(uint32_t i2c_event)
{
    volatile uint32_t timeout = HAL_I2C_TIMEOUT_MAX;

    while (I2C_CheckEvent(I2C1, i2c_event) != SUCCESS)
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            prv_i2c_recover();
            return false;
        }
    }
    return true;
}

/**
 * @brief Chờ cờ I2C_FLAG_BUSY được xóa trước khi phát xung START mới; tự phục hồi nếu kẹt bus
 */
static bool prv_i2c_wait_not_busy(void)
{
    volatile uint32_t timeout = HAL_I2C_TIMEOUT_MAX;

    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET)
    {
        if (--timeout == 0U)
        {
            prv_i2c_recover();
            return false;
        }
    }
    return true;
}

/**
 * @brief Chờ cờ I2C_FLAG_BTF (Byte Transfer Finished) sau khi DMA nạp xong byte cuối vào DR
 */
static bool prv_i2c_wait_btf(void)
{
    volatile uint32_t timeout = HAL_I2C_TIMEOUT_MAX;

    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF) == RESET)
    {
        if (--timeout == 0U)
        {
            I2C_GenerateSTOP(I2C1, ENABLE);
            prv_i2c_recover();
            return false;
        }
    }
    return true;
}

/**
 * @brief Truyền khối dữ liệu từ s_i2c_tx_buf qua I2C1 (tự động dùng DMA1_Channel6 nếu gói lớn & RTOS đang chạy)
 */
static bool prv_i2c_flush_tx(void)
{
    uint16_t i;

    if (s_i2c_tx_len == 0U)
    {
        return true;
    }

    if (!prv_i2c_wait_not_busy())
    {
        s_i2c_tx_len = 0U;
        return false;
    }

    I2C_GenerateSTART(I2C1, ENABLE);
    if (!prv_i2c_wait_event(I2C_EVENT_MASTER_MODE_SELECT))
    {
        s_i2c_tx_len = 0U;
        return false;
    }

    I2C_Send7bitAddress(I2C1, s_i2c_addr, I2C_Direction_Transmitter);
    if (!prv_i2c_wait_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
    {
        s_i2c_tx_len = 0U;
        return false;
    }

    if ((s_i2c_tx_len > HAL_I2C_DMA_THRESHOLD) &&
        (s_i2c_dma_sem != NULL) &&
        (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
    {
        DMA_InitTypeDef DMA_InitStructure;

        s_i2c_dma_error = false;
        (void)xSemaphoreTake(s_i2c_dma_sem, 0U);

        DMA_Cmd(DMA1_Channel6, DISABLE);
        DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, DISABLE);
        DMA_ClearITPendingBit(DMA1_IT_GL6);

        DMA_StructInit(&DMA_InitStructure);
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(I2C1->DR);
        DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)s_i2c_tx_buf;
        DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
        DMA_InitStructure.DMA_BufferSize         = (uint32_t)s_i2c_tx_len;
        DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
        DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
        DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
        DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
        DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
        DMA_Init(DMA1_Channel6, &DMA_InitStructure);

        DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, ENABLE);
        I2C_DMACmd(I2C1, ENABLE);
        DMA_Cmd(DMA1_Channel6, ENABLE);

        if ((xSemaphoreTake(s_i2c_dma_sem, pdMS_TO_TICKS(HAL_I2C_DMA_TIMEOUT_MS)) != pdTRUE) || s_i2c_dma_error)
        {
            DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, DISABLE);
            DMA_Cmd(DMA1_Channel6, DISABLE);
            I2C_DMACmd(I2C1, DISABLE);
            I2C_GenerateSTOP(I2C1, ENABLE);
            prv_i2c_recover();
            s_i2c_tx_len = 0U;
            return false;
        }

        if (!prv_i2c_wait_btf())
        {
            s_i2c_tx_len = 0U;
            return false;
        }
    }
    else
    {
        for (i = 0U; i < s_i2c_tx_len; i++)
        {
            I2C_SendData(I2C1, s_i2c_tx_buf[i]);
            if (!prv_i2c_wait_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED))
            {
                s_i2c_tx_len = 0U;
                return false;
            }
        }
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
    s_i2c_tx_len = 0U;
    return true;
}

/**
 * @brief Trình phục vụ ngắt DMA1_Channel6 hoàn tất truyền khung hình I2C1_TX
 */
void DMA1_Channel6_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (DMA_GetITStatus(DMA1_IT_TE6) != RESET)
    {
        s_i2c_dma_error = true;
        DMA_ClearITPendingBit(DMA1_IT_TE6);
    }
    if (DMA_GetITStatus(DMA1_IT_TC6) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC6);
    }

    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, DISABLE);
    DMA_Cmd(DMA1_Channel6, DISABLE);
    I2C_DMACmd(I2C1, DISABLE);

    if (s_i2c_dma_sem != NULL)
    {
        (void)xSemaphoreGiveFromISR(s_i2c_dma_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* =========================================================================
 * 3. CÀI ĐẶT CÁC API CÔNG KHAI (PUBLIC FUNCTIONS)
 * ========================================================================= */

void hal_i2c_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef  I2C_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 1. Bật xung nhịp cho DMA1 (AHB), I2C1 (APB1), GPIOB và AFIO (APB2) */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_APB2PeriphClockCmd(BSP_OLED_I2C_GPIO_CLK | RCC_APB2Periph_AFIO, ENABLE);

    /* 2. Ánh xạ lại (Remap) chân I2C1 từ PB6/PB7 sang PB8 (SCL) và PB9 (SDA)
     *    để dành PB6/PB7 cho TIM4 Hardware Encoder */
    GPIO_PinRemapConfig(GPIO_Remap_I2C1, ENABLE);

    /* 3. Cấu hình PB8 (SCL) và PB9 (SDA) ở chế độ Alternate Function Open-Drain 50MHz */
    GPIO_InitStructure.GPIO_Pin   = BSP_OLED_SCL_PIN | BSP_OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_OLED_I2C_GPIO_PORT, &GPIO_InitStructure);

    /* 4. Khởi tạo ngoại vi I2C1 ở tốc độ Fast Mode 400kHz */
    I2C_DeInit(I2C1);
    I2C_StructInit(&I2C_InitStructure);
    I2C_InitStructure.I2C_ClockSpeed          = HAL_I2C_CLOCK_SPEED;
    I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1         = 0x00U;
    I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStructure);

    /* 5. Chuẩn bị DMA1_Channel6 (I2C1_TX) & Binary Semaphore đồng bộ FreeRTOS */
    DMA_Cmd(DMA1_Channel6, DISABLE);
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC | DMA_IT_TE, DISABLE);
    DMA_ClearITPendingBit(DMA1_IT_GL6);
    I2C_DMACmd(I2C1, DISABLE);

    if (s_i2c_dma_sem == NULL)
    {
        s_i2c_dma_sem = xSemaphoreCreateBinary();
    }

    /* Mức ưu tiên 8 (>= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5) để gọi FromISR an toàn */
    NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel6_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 8U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0U;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 6. Kích hoạt ngoại vi I2C1 */
    I2C_Cmd(I2C1, ENABLE);
}

uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
        case U8X8_MSG_BYTE_INIT:
            /* Ngoại vi I2C1 & DMA1_Channel6 đã được khởi tạo tại hal_i2c_init() trong main() */
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            s_i2c_addr   = u8x8_GetI2CAddress(u8x8);
            s_i2c_tx_len = 0U;
            break;

        case U8X8_MSG_BYTE_SEND:
            if ((arg_int > 0U) && (arg_ptr != NULL))
            {
                if ((uint32_t)s_i2c_tx_len + (uint32_t)arg_int > HAL_I2C_DMA_BUF_SIZE)
                {
                    if (!prv_i2c_flush_tx())
                    {
                        return 0U;
                    }
                }
                if ((uint32_t)arg_int <= HAL_I2C_DMA_BUF_SIZE)
                {
                    memcpy(&s_i2c_tx_buf[s_i2c_tx_len], arg_ptr, (size_t)arg_int);
                    s_i2c_tx_len += (uint16_t)arg_int;
                }
            }
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
            if (!prv_i2c_flush_tx())
            {
                return 0U;
            }
            break;

        case U8X8_MSG_BYTE_SET_DC:
            /* Giao thức I2C SSD1306 dùng byte điều khiển 0x00/0x40, không dùng chân DC */
            break;

        default:
            return 0U;
    }

    return 1U;
}

uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    volatile uint32_t delay_cnt;

    (void)u8x8;
    (void)arg_ptr;

    switch (msg)
    {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;

        case U8X8_MSG_DELAY_MILLI:
            if (arg_int > 0U)
            {
                /* Nếu bộ lập lịch FreeRTOS đã chạy, bắt buộc nhường CPU bằng vTaskDelay() */
                if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
                {
                    vTaskDelay(pdMS_TO_TICKS((TickType_t)arg_int));
                }
                else
                {
                    delay_cnt = (uint32_t)arg_int * 7200U;
                    while (delay_cnt > 0U)
                    {
                        __NOP();
                        delay_cnt--;
                    }
                }
            }
            break;

        case U8X8_MSG_DELAY_10MICRO:
            delay_cnt = (uint32_t)arg_int * 72U;
            while (delay_cnt > 0U)
            {
                __NOP();
                delay_cnt--;
            }
            break;

        case U8X8_MSG_DELAY_100NANO:
        case U8X8_MSG_DELAY_NANO:
        case U8X8_MSG_DELAY_I2C:
            __NOP();
            break;

        default:
            break;
    }

    return 1U;
}
