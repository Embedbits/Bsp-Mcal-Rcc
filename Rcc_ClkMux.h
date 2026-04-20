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
 * - SysTick
 * - RTC
 * - Low-Power Peripheral Domain Clock (LPCLK)
 * - Low Power Timers (LPTIM1, LPTIM2, LPTIM3, LPTIM4)
 * - I2C (I2C1, I2C2, I2C3, I2C4)
 * - I3C (I3C1, I3C2)
 * - SPI (SPI1/I2S1, SPI2/I2S2, SPI3/I2S3, SPI4, SPI5, SPI6)
 * - USART (USART1, USART2, USART3, UART4, UART5, USART6, UART7, UART8, UART9, USART10, USART11, UART12)
 * - LPUART1
 * - FDCAN
 * - OCTOSPI1
 * - SDMMC (SDMMC1, SDMMC2)
 * - TIM (TIM1, TIM2, TIM3, TIM4, TIM5, TIM8, TIM15, TIM16, TIM17)
 * - SAI (SAI1, SAI2)
 * - USB
 * - RNG
 * - ADC (ADC1, ADC2, ADC3)
 * - DAC (DAC1, DAC2)
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
    /*------------------------------ System core -----------------------------*/

    RCC_CLK_MUX_RTC_HSE_DIV32 = 0u    , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source  */
    RCC_CLK_MUX_RTC_LSE               , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_CLK_MUX_RTC_LSI               , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

    /*-------------------------------- Timers --------------------------------*/

#if defined(LPTIM1)
    RCC_CLK_MUX_LPTIM1_APB1          , /**< Low Power Timer 1 clock enable with APB1 as clock source for LPTIM 1. */
    RCC_CLK_MUX_LPTIM1_LSI           , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) as clock source for LPTIM 1. */
    RCC_CLK_MUX_LPTIM1_HSI16         , /**< Low Power Timer 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 1. */
    RCC_CLK_MUX_LPTIM1_LSE           , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 1. */
#endif /* LPTIM1 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI2) || defined(SPI3)
    RCC_CLK_MUX_I2S_SYSCLK           , /**< I2S clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2S_PLLQ             , /**< I2S clock enable with Phase-Locked Loop output Q (PLLQ) as clock source */
    RCC_CLK_MUX_I2S_HSI16            , /**< I2S clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_I2S_CKIN             , /**< I2S clock enable with Clock Input pin (CKIN) as clock source */
#endif /* SPI2 */

#if defined(I2C1)
    RCC_CLK_MUX_I2C1_APB1            , /**< I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C1_SYSCLK          , /**< I2C 1 clock enable with SysClk (AHB) as clock source*/
    RCC_CLK_MUX_I2C1_HSI16           , /**< I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_CLK_MUX_I2C2_APB1            , /**< I2C 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C2_SYSCLK          , /**< I2C 2 clock enable with SysClk (AHB) as clock source*/
    RCC_CLK_MUX_I2C2_HSI16           , /**< I2C 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_CLK_MUX_I2C3_APB1            , /**< I2C 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C3_SYSCLK          , /**< I2C 3 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2C3_HSI16           , /**< I2C 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_CLK_MUX_I2C4_APB1            , /**< I2C 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C4_SYSCLK          , /**< I2C 4 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2C4_HSI16           , /**< I2C 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I2C4 */

#if defined(USART1)
    RCC_CLK_MUX_USART1_APB2          , /**< USART 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART1_SYSCLK        , /**< USART 1 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART1_HSI           , /**< USART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART1_LSE           , /**< USART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART1 */
#if defined(USART2)
    RCC_CLK_MUX_USART2_APB1          , /**< USART 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART2_SYSCLK        , /**< USART 2 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART2_HSI           , /**< USART 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART2_LSE           , /**< USART 2 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART2 */
#if defined(USART3)
    RCC_CLK_MUX_USART3_APB1          , /**< USART 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART3_SYSCLK        , /**< USART 3 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART3_HSI           , /**< USART 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART3_LSE           , /**< USART 3 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART3 */

#if defined(UART4)
    RCC_CLK_MUX_UART4_APB1           , /**< UART 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART4_SYSCLK        , /**< UART 4 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_UART4_HSI            , /**< UART 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART4_LSE            , /**< UART 4 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* UART4 */
#if defined(UART5)
    RCC_CLK_MUX_UART5_APB1           , /**< UART 5 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART5_SYSCLK        , /**< UART 5 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_UART5_HSI16          , /**< UART 5 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART5_LSE            , /**< UART 5 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART5 */

#if defined(LPUART1)
    RCC_CLK_MUX_LPUART1_APB1         , /**< Low-Power UART 1 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_LPUART1_SYSCLK       , /**< Low-Power UART 1 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_LPUART1_HSI16        , /**< Low-Power UART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_LPUART1_LSE          , /**< Low-Power UART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* LPUART1 */

#if defined(FDCAN1)
    RCC_CLK_MUX_FDCAN_APB1           , /**< FDCAN clock enable with PCLK1 (AHB1) used as clock source */
    RCC_CLK_MUX_FDCAN_PLLQ           , /**< FDCAN clock enable with Phase Locked Loop output Q (PLLQ) used as clock source */
    RCC_CLK_MUX_FDCAN_HSE            , /**< FDCAN clock enable with High Speed External oscillator (HSE) used as clock source */
#endif /* FDCAN1 */

#if defined(QUADSPI)
    RCC_CLK_MUX_QUADSPI_SYSCLK       , /**< Quad SPI (QUADSPI) system clock clock source for QSPI */
    RCC_CLK_MUX_QUADSPI_PLLQ         , /**< Quad SPI (QUADSPI) Phase Locked Loop output Q (PLLQ) clock source for QSPI */
    RCC_CLK_MUX_QUADSPI_HSI16        , /**< Quad SPI (QUADSPI) 16MHz High Speed Internal (HSI16) clock source for QSPI */
#endif /* OCTOSPI1 */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(SAI1)
    RCC_CLK_MUX_SAI1_SYSCLK          , /**< Serial Audio Interface 1 (SAI1) clock enable with System clock (SYSCLK) as clock source */
    RCC_CLK_MUX_SAI1_PLLQ            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL Q output as clock source */
    RCC_CLK_MUX_SAI1_HSI16           , /**< Serial Audio Interface 1 (SAI1) clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source*/
    RCC_CLK_MUX_SAI1_CKIN            , /**< Serial Audio Interface 1 (SAI1) clock enable with clock input through pin as clock source*/
#endif /* SAI1 */

    /*-------------------------------- Analog --------------------------------*/

#if defined(ADC12_COMMON)
    RCC_CLK_MUX_ADC12_SYSCLK         , /**< ADC's 1&2 SysClk clock source */
    RCC_CLK_MUX_ADC12_PLLP           , /**< ADC's 1&2 PLL output P clock source */
#endif /* ADC1 */

#if defined(ADC345_COMMON)
    RCC_CLK_MUX_ADC345_SYSCLK        , /**< ADC's 3&4&5 SysClk clock source */
    RCC_CLK_MUX_ADC345_PLLP          , /**< ADC's 3&4&5 PLL output P clock source */
#endif /* ADC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(RNG) || defined(USB)
    RCC_CLK_MUX_USB_RNG_PLLQ         , /**< Random Number Generator (RNG) and USB clock enable with Phase-Locked Loop output Q (PLLQ) used as clock source */
    RCC_CLK_MUX_USB_RNG_HSI48        , /**< Random Number Generator (RNG) and USB clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
#endif /* RNG */

    RCC_CLK_MUX_LIST_CNT
}   rcc_ClkMuxId_t;

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

rcc_RequestState_t          Rcc_ClkMux_Init             ( void );
void                        Rcc_ClkMux_Deinit           ( void );

rcc_RequestState_t          Rcc_ClkMux_Set_ClkActive    ( rcc_ClkMuxId_t clkMuxId  );
rcc_RequestState_t          Rcc_ClkMux_Set_ClkInactive  ( rcc_ClkMuxId_t clkMuxId  );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKMUX_RCC_CLKMUX_H */
