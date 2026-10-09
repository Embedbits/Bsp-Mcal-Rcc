/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.h
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Peripheral's Clock Multiplexer (ClkMux)
 *        component functionality header file.
 *
 * This file contains the interface functions and types used by the RCC
 * module Clock Multiplexer (ClkMux) component.
 * List of available peripherals with configurable clock sources:
 * - RTC (RTCSEL, write once after backup domain reset)
 * - LPTIM1, LPTIM2 (PCLK1, LSI, HSI16, LSE)
 * - I2C1 - I2C4 (PCLK1, SYSCLK, HSI16)
 * - USART1 - USART3, UART4, UART5, LPUART1 (PCLKx, SYSCLK, HSI16, LSE)
 * - USB and RNG 48 MHz clock (CLK48SEL - HSI48, PLLSAI1Q, PLLQ, MSI)
 * - SWPMI1 (PCLK1, HSI16), DFSDM1 (PCLK2, SYSCLK)
 * - ADC asynchronous clock (none - HCLK, PLLSAI1R, PLLSAI2R, SYSCLK)
 *
 */

#ifndef RCC_CLKMUX_RCC_CLKMUX_H
#define RCC_CLKMUX_RCC_CLKMUX_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Rcc_Types.h"                      /* Module types definition        */
/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ============================= TYPEDEFS =================================== */

/** \brief List of peripherals with configurable clock sources */
typedef enum
{
    RCC_CLK_MUX_RTC_NONE      , /**< Real Time Clock without clock (reset selection) */
    RCC_CLK_MUX_RTC_LSE       , /**< Real Time Clock clocked by Low Speed External (LSE) */
    RCC_CLK_MUX_RTC_LSI       , /**< Real Time Clock clocked by Low Speed Internal (LSI) */
    RCC_CLK_MUX_RTC_HSE_DIV32 , /**< Real Time Clock clocked by High Speed External (HSE) divided by 32 */
    RCC_CLK_MUX_LPTIM1_PCLK1  , /**< Low Power Timer 1 clocked by APB1 (PCLK1) */
    RCC_CLK_MUX_LPTIM1_LSI    , /**< Low Power Timer 1 clocked by Low Speed Internal (LSI) */
    RCC_CLK_MUX_LPTIM1_HSI    , /**< Low Power Timer 1 clocked by 16 MHz High Speed Internal (HSI16) */
    RCC_CLK_MUX_LPTIM1_LSE    , /**< Low Power Timer 1 clocked by Low Speed External (LSE) */
    RCC_CLK_MUX_LPTIM2_PCLK1  , /**< Low Power Timer 2 clocked by APB1 (PCLK1) */
    RCC_CLK_MUX_LPTIM2_LSI    , /**< Low Power Timer 2 clocked by Low Speed Internal (LSI) */
    RCC_CLK_MUX_LPTIM2_HSI    , /**< Low Power Timer 2 clocked by 16 MHz High Speed Internal (HSI16) */
    RCC_CLK_MUX_LPTIM2_LSE    , /**< Low Power Timer 2 clocked by Low Speed External (LSE) */
    RCC_CLK_MUX_I2C1_PCLK1    , /**< I2C1 clocked by APB1 (PCLK1) */
    RCC_CLK_MUX_I2C1_SYSCLK   , /**< I2C1 clocked by system clock (SYSCLK) */
    RCC_CLK_MUX_I2C1_HSI      , /**< I2C1 clocked by 16 MHz High Speed Internal (HSI16) */
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_CLK_MUX_I2C2_PCLK1    , /**< I2C2 clocked by APB1 (PCLK1) */
#endif
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_CLK_MUX_I2C2_SYSCLK   , /**< I2C2 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_CLK_MUX_I2C2_HSI      , /**< I2C2 clocked by 16 MHz High Speed Internal (HSI16) */
#endif
    RCC_CLK_MUX_I2C3_PCLK1    , /**< I2C3 clocked by APB1 (PCLK1) */
    RCC_CLK_MUX_I2C3_SYSCLK   , /**< I2C3 clocked by system clock (SYSCLK) */
    RCC_CLK_MUX_I2C3_HSI      , /**< I2C3 clocked by 16 MHz High Speed Internal (HSI16) */
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_CLK_MUX_I2C4_PCLK1    , /**< I2C4 clocked by APB1 (PCLK1) */
#endif
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_CLK_MUX_I2C4_SYSCLK   , /**< I2C4 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_CLK_MUX_I2C4_HSI      , /**< I2C4 clocked by 16 MHz High Speed Internal (HSI16) */
#endif
    RCC_CLK_MUX_USART1_PCLK2  , /**< USART1 clocked by APB2 (PCLK2) */
    RCC_CLK_MUX_USART1_SYSCLK , /**< USART1 clocked by system clock (SYSCLK) */
    RCC_CLK_MUX_USART1_HSI    , /**< USART1 clocked by 16 MHz High Speed Internal (HSI16) */
    RCC_CLK_MUX_USART1_LSE    , /**< USART1 clocked by Low Speed External (LSE) */
    RCC_CLK_MUX_USART2_PCLK1  , /**< USART2 clocked by APB1 (PCLK1) */
    RCC_CLK_MUX_USART2_SYSCLK , /**< USART2 clocked by system clock (SYSCLK) */
    RCC_CLK_MUX_USART2_HSI    , /**< USART2 clocked by 16 MHz High Speed Internal (HSI16) */
    RCC_CLK_MUX_USART2_LSE    , /**< USART2 clocked by Low Speed External (LSE) */
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_CLK_MUX_USART3_PCLK1  , /**< USART3 clocked by APB1 (PCLK1) */
#endif
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_CLK_MUX_USART3_SYSCLK , /**< USART3 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_CLK_MUX_USART3_HSI    , /**< USART3 clocked by 16 MHz High Speed Internal (HSI16) */
#endif
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_CLK_MUX_USART3_LSE    , /**< USART3 clocked by Low Speed External (LSE) */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_CLK_MUX_UART4_PCLK1   , /**< UART4 clocked by APB1 (PCLK1) */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_CLK_MUX_UART4_SYSCLK  , /**< UART4 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_CLK_MUX_UART4_HSI     , /**< UART4 clocked by 16 MHz High Speed Internal (HSI16) */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_CLK_MUX_UART4_LSE     , /**< UART4 clocked by Low Speed External (LSE) */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_CLK_MUX_UART5_PCLK1   , /**< UART5 clocked by APB1 (PCLK1) */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_CLK_MUX_UART5_SYSCLK  , /**< UART5 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_CLK_MUX_UART5_HSI     , /**< UART5 clocked by 16 MHz High Speed Internal (HSI16) */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_CLK_MUX_UART5_LSE     , /**< UART5 clocked by Low Speed External (LSE) */
#endif
    RCC_CLK_MUX_LPUART1_PCLK1 , /**< LPUART1 clocked by APB1 (PCLK1) */
    RCC_CLK_MUX_LPUART1_SYSCLK, /**< LPUART1 clocked by system clock (SYSCLK) */
    RCC_CLK_MUX_LPUART1_HSI   , /**< LPUART1 clocked by 16 MHz High Speed Internal (HSI16) */
    RCC_CLK_MUX_LPUART1_LSE   , /**< LPUART1 clocked by Low Speed External (LSE) */
#if defined(RCC_TYPES_USB_SUPPORT) && \
    !defined(RCC_CRRCR_HSI48ON)
    RCC_CLK_MUX_USB_NONE      , /**< 48 MHz clock not selected (reset selection without HSI48) */
#endif
#if defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CRRCR_HSI48ON)
    RCC_CLK_MUX_USB_HSI48     , /**< 48 MHz clock from 48 MHz High Speed Internal (HSI48) */
#endif
#if defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CR_PLLSAI1ON)
    RCC_CLK_MUX_USB_PLLSAI1Q  , /**< 48 MHz clock from PLLSAI1 output Q */
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
    RCC_CLK_MUX_USB_PLLQ      , /**< 48 MHz clock from main PLL output Q */
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
    RCC_CLK_MUX_USB_MSI       , /**< 48 MHz clock from Multi Speed Internal (MSI) */
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
    RCC_CLK_MUX_SWPMI1_PCLK1  , /**< SWPMI1 clocked by APB1 (PCLK1) */
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
    RCC_CLK_MUX_SWPMI1_HSI    , /**< SWPMI1 clocked by 16 MHz High Speed Internal (HSI16) */
#endif
#if defined(RCC_CCIPR_DFSDM1SEL)
    RCC_CLK_MUX_DFSDM1_PCLK2  , /**< DFSDM1 clocked by APB2 (PCLK2) */
#endif
#if defined(RCC_CCIPR_DFSDM1SEL)
    RCC_CLK_MUX_DFSDM1_SYSCLK , /**< DFSDM1 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_CCIPR2_DFSDM1SEL)
    RCC_CLK_MUX_DFSDM1_PCLK2  , /**< DFSDM1 clocked by APB2 (PCLK2) */
#endif
#if defined(RCC_CCIPR2_DFSDM1SEL)
    RCC_CLK_MUX_DFSDM1_SYSCLK , /**< DFSDM1 clocked by system clock (SYSCLK) */
#endif
#if defined(RCC_CCIPR_ADCSEL)
    RCC_CLK_MUX_ADC_HCLK      , /**< No asynchronous ADC clock - ADC clocked by HCLK (synchronous mode, reset selection) */
#endif
#if defined(RCC_CCIPR_ADCSEL) && \
    defined(RCC_CR_PLLSAI1ON)
    RCC_CLK_MUX_ADC_PLLSAI1R  , /**< ADC clocked by PLLSAI1 output R */
#endif
#if defined(RCC_CCIPR_ADCSEL) && \
    defined(RCC_TYPES_ADC_PLLSAI2R_SUPPORT)
    RCC_CLK_MUX_ADC_PLLSAI2R  , /**< ADC clocked by PLLSAI2 output R */
#endif
#if defined(RCC_CCIPR_ADCSEL)
    RCC_CLK_MUX_ADC_SYSCLK    , /**< ADC clocked by system clock (SYSCLK) */
#endif
#if !defined(RCC_CRRCR_HSI48ON)
    RCC_CLK_MUX_RNG_NONE      , /**< 48 MHz clock not selected (reset selection without HSI48) */
#endif
#if defined(RCC_CRRCR_HSI48ON)
    RCC_CLK_MUX_RNG_HSI48     , /**< 48 MHz clock from 48 MHz High Speed Internal (HSI48) */
#endif
#if defined(RCC_CR_PLLSAI1ON)
    RCC_CLK_MUX_RNG_PLLSAI1Q  , /**< 48 MHz clock from PLLSAI1 output Q */
#endif
    RCC_CLK_MUX_RNG_PLLQ      , /**< 48 MHz clock from main PLL output Q */
    RCC_CLK_MUX_RNG_MSI       , /**< 48 MHz clock from Multi Speed Internal (MSI) */

    RCC_CLK_MUX_LIST_CNT
}   rcc_ClkMuxId_t;

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

rcc_RequestState_t          Rcc_ClkMux_Init             ( void );
void                        Rcc_ClkMux_Deinit           ( void );
void                        Rcc_ClkMux_Task             ( void );

rcc_RequestState_t          Rcc_ClkMux_Set_ClkActive    ( rcc_ClkMuxId_t clkMuxId  );
rcc_RequestState_t          Rcc_ClkMux_Set_ClkInactive  ( rcc_ClkMuxId_t clkMuxId  );

rcc_RequestState_t          Rcc_ClkMux_Get_ClkSrc       ( rcc_ClkMuxId_t clkMuxIdIn, rcc_ClkMuxId_t * const clkMuxId );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKMUX_RCC_CLKMUX_H */
