/**
 * @file    stm32f103c8t6.ld
 * @brief   GNU LD Linker Script cho STM32F103C8T6 (64KB Flash, 20KB SRAM)
 * @note    Page 63 (0x0800FC00 - 0x0800FFFF, 1KB) được dành riêng làm vùng lưu trữ cấu hình (Emulated EEPROM).
 */

ENTRY(Reset_Handler)

_estack = 0x20005000; /* Đỉnh của 20KB SRAM (0x20000000 + 20KB) */

MEMORY
{
    /* Chừa 1KB cuối (Page 63: 0x0800FC00) cho hal_flash.c lưu SystemConfig_t */
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 63K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP (*(.init))
        KEEP (*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + 0x400; /* Tối thiểu 1KB Main Stack (MSP) cho ISR và khởi tạo ban đầu */
        . = ALIGN(8);
    } >RAM
}

