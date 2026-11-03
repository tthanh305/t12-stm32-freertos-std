/**
 * @file    FreeRTOSConfig.h
 * @brief   Cấu hình chuẩn FreeRTOS Kernel cho STM32F103C8T6 (72MHz, 20KB SRAM, Thư viện STD/SPL)
 * @note    File nền tảng do Leader (Dev 1) thiết lập độc quyền.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
#include <stdint.h>
extern uint32_t SystemCoreClock;
#endif

/* =========================================================================
 * 1. THÔNG SỐ CƠ BẢN CỦA KERNEL
 * ========================================================================= */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ( SystemCoreClock ) /* 72,000,000 Hz */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 ) /* 1ms Tick */
#define configMAX_PRIORITIES                    ( 6 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* =========================================================================
 * 2. CẤU HÌNH ĐỒNG BỘ HÓA & QUẢN LÝ BỘ NHỚ (10KB HEAP TRÊN 20KB SRAM)
 * ========================================================================= */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1

#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 10 * 1024 ) ) /* 10KB Heap */

/* =========================================================================
 * 3. HOOK FUNCTIONS & KIỂM TRA AN TOÀN STACK / HEAP
 * ========================================================================= */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2 /* Bật kiểm tra tràn Stack mức 2 */
#define configUSE_MALLOC_FAILED_HOOK            1 /* Bắt lỗi thiếu Heap */

/* Bật hàm đo lượng Stack trống tối thiểu (High Water Mark) phục vụ Giai đoạn 3 */
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* =========================================================================
 * 4. SOFTWARE TIMERS
 * ========================================================================= */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 2 )
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE )

/* =========================================================================
 * 5. CÁC API BẮT BUỘC INCLUDE
 * ========================================================================= */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1

/* =========================================================================
 * 6. CẤU HÌNH ƯU TIÊN NGẮT CORTEX-M3 (4 BITS PREEMPTION PRIORITY)
 * ========================================================================= */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS                     __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS                     4 /* 16 mức ưu tiên (0..15) trên STM32F103 */
#endif

/* Mức ưu tiên thấp nhất cho SysTick và PendSV (15) */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         0x0F

/* Các ngắt có gọi API FreeRTOS (...FromISR) phải có mức ưu tiên số học từ 5 đến 15 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Macro kiểm tra lỗi lúc phát triển */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* =========================================================================
 * 7. ÁNH XẠ TRÌNH PHỤC VỤ NGẮT KERNEL VÀO VECTOR BẢNG NGẮT CMSIS/SPL
 * ========================================================================= */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */

