/**
 * @file    portmacro.h
 * @brief   Wrapper tự động chọn portmacro.h phù hợp giữa Keil ARMCC v5 (__CC_ARM)
 *          và Keil ARMCLANG v6 / GCC (__GNUC__ / __clang__)
 */

#ifndef PORTMACRO_AUTO_WRAPPER_H
#define PORTMACRO_AUTO_WRAPPER_H

#if defined(__CC_ARM)
    /* Keil MDK Compiler Version 5 (ARMCC) */
    #include "RVDS/ARM_CM3/portmacro.h"
#elif defined(__GNUC__) || defined(__clang__)
    /* Keil MDK Compiler Version 6 (ARMCLANG) hoặc Arm GNU Toolchain (GCC) */
    #include "GCC/ARM_CM3/portmacro.h"
#else
    #error "Unsupported compiler for FreeRTOS ARM_CM3 port!"
#endif

#endif /* PORTMACRO_AUTO_WRAPPER_H */

