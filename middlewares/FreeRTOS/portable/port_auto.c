/**
 * @file    port_auto.c
 * @brief   Wrapper tự động chọn port.c phù hợp giữa Keil ARMCC v5 (__CC_ARM)
 *          và Keil ARMCLANG v6 / GCC (__GNUC__ / __clang__)
 */

#if defined(__CC_ARM)
    /* Keil MDK Compiler Version 5 (ARMCC) */
    #include "RVDS/ARM_CM3/port.c"
#elif defined(__GNUC__) || defined(__clang__)
    /* Keil MDK Compiler Version 6 (ARMCLANG) hoặc Arm GNU Toolchain (GCC) */
    #include "GCC/ARM_CM3/port.c"
#else
    #error "Unsupported compiler for FreeRTOS ARM_CM3 port!"
#endif

