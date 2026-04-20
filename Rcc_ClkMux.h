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

    RCC_CLK_MUX_SYSTICK_HCLK_DIV8 = 0u, /**< SysTick clock enable with AHB clock (HCLK) divided by 8 as clock source */
    RCC_CLK_MUX_SYSTICK_LSI           , /**< SysTick clock enable with Low Speed Internal (LSI) oscillator as clock source */
    RCC_CLK_MUX_SYSTICK_LSE           , /**< SysTick clock enable with Low Speed External (LSE) oscillator as clock source */

    RCC_CLK_MUX_RTC_NONE              , /**< Real Time Clock disabled */
    RCC_CLK_MUX_RTC_HSE_DIV32         , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source  */
    RCC_CLK_MUX_RTC_LSE               , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_CLK_MUX_RTC_LSI               , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

    RCC_CLK_MUX_LPCLK_HSI             , /**< Low-Power Peripheral Domain Clock (PER) from 64MHz High Speed Internal (HSI48) oscillator Clock Enable */
    RCC_CLK_MUX_LPCLK_HSE             , /**< Low-Power Peripheral Domain Clock (PER) from PLL 1 Q output Clock Enable */
    RCC_CLK_MUX_LPCLK_CSI             , /**< Low-Power Peripheral Domain Clock (PER) from Multi-Speed Internal Oscillator output K (MSIK) Clock Enable */

    /*-------------------------------- Timers --------------------------------*/

#if defined(LPTIM1)
    RCC_CLK_MUX_LPTIM1_PCLK3          , /**< Low Power Timer 1 clock enable with APB3 as clock source. */
    RCC_CLK_MUX_LPTIM1_PLL2P          , /**< Low Power Timer 1 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source. */
    RCC_CLK_MUX_LPTIM1_PLL3R          , /**< Low Power Timer 1 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source. */
    RCC_CLK_MUX_LPTIM1_LSE            , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM1_LSI            , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM1_LPCLK          , /**< Low Power Timer 1 clock enable with Low-Power Clock domain (PER) oscillator as clock source. */
#endif /* LPTIM1 */
#if defined(LPTIM2)
    RCC_CLK_MUX_LPTIM2_PCLK1          , /**< Low Power Timer 2 clock enable with APB1 as clock source. */
    RCC_CLK_MUX_LPTIM2_PLL2P          , /**< Low Power Timer 2 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source. */
    RCC_CLK_MUX_LPTIM2_PLL3R          , /**< Low Power Timer 2 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source. */
    RCC_CLK_MUX_LPTIM2_LSE            , /**< Low Power Timer 2 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM2_LSI            , /**< Low Power Timer 2 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM2_LPCLK          , /**< Low Power Timer 2 clock enable with Low-Power Clock domain (PER) oscillator as clock source. */
#endif /* LPTIM2 */
#if defined(LPTIM3)
    RCC_CLK_MUX_LPTIM3_PCLK3          , /**< Low Power Timer 3 clock enable with APB3 as clock source. */
    RCC_CLK_MUX_LPTIM3_PLL2P          , /**< Low Power Timer 3 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source. */
    RCC_CLK_MUX_LPTIM3_PLL3R          , /**< Low Power Timer 3 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source. */
    RCC_CLK_MUX_LPTIM3_LSE            , /**< Low Power Timer 3 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM3_LSI            , /**< Low Power Timer 3 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM3_LPCLK          , /**< Low Power Timer 3 clock enable with Low-Power Clock domain (PER) oscillator as clock source. */
#endif /* LPTIM3 */
#if defined(LPTIM4)
    RCC_CLK_MUX_LPTIM4_PCLK3          , /**< Low Power Timer 4 clock enable with APB3 as clock source. */
    RCC_CLK_MUX_LPTIM4_PLL2P          , /**< Low Power Timer 4 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source. */
    RCC_CLK_MUX_LPTIM4_PLL3R          , /**< Low Power Timer 4 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source. */
    RCC_CLK_MUX_LPTIM4_LSE            , /**< Low Power Timer 4 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM4_LSI            , /**< Low Power Timer 4 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM4_LPCLK          , /**< Low Power Timer 4 clock enable with Low-Power Clock domain (PER) oscillator as clock source. */
#endif /* LPTIM4 */
#if defined(LPTIM5)
    RCC_CLK_MUX_LPTIM5_PCLK3          , /**< Low Power Timer 5 clock enable with APB3 as clock source. */
    RCC_CLK_MUX_LPTIM5_PLL2P          , /**< Low Power Timer 5 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source. */
    RCC_CLK_MUX_LPTIM5_PLL3R          , /**< Low Power Timer 5 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source. */
    RCC_CLK_MUX_LPTIM5_LSE            , /**< Low Power Timer 5 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM5_LSI            , /**< Low Power Timer 5 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM5_LPCLK          , /**< Low Power Timer 5 clock enable with Low-Power Clock domain (PER) oscillator as clock source. */
#endif /* LPTIM5 */
#if defined(LPTIM6)
    RCC_CLK_MUX_LPTIM6_PCLK3          , /**< Low Power Timer 6 clock enable with APB3 as clock source. */
    RCC_CLK_MUX_LPTIM6_PLL2P          , /**< Low Power Timer 6 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source. */
    RCC_CLK_MUX_LPTIM6_PLL3R          , /**< Low Power Timer 6 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source. */
    RCC_CLK_MUX_LPTIM6_LSE            , /**< Low Power Timer 6 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM6_LSI            , /**< Low Power Timer 6 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM6_LPCLK          , /**< Low Power Timer 6 clock enable with Low-Power Clock domain (PER) oscillator as clock source. */
#endif /* LPTIM6 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
    RCC_CLK_MUX_SPI1_PLL1Q            , /**< SPI/I2S 1 clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_SPI1_PLL2P            , /**< SPI/I2S 1 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source */
    RCC_CLK_MUX_SPI1_PLL3P            , /**< SPI/I2S 1 clock enable with Phase Locked Loop 3 output P (PLL3P) as clock source */
    RCC_CLK_MUX_SPI1_PIN              , /**< SPI/I2S 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_SPI1_LPCLK            , /**< SPI/I2S 1 clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* SPI1 */
#if defined(SPI2)
    RCC_CLK_MUX_SPI2_PLL1Q            , /**< SPI/I2S 2 clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_SPI2_PLL2P            , /**< SPI/I2S 2 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source */
    RCC_CLK_MUX_SPI2_PLL3P            , /**< SPI/I2S 2 clock enable with Phase Locked Loop 3 output P (PLL3P) as clock source */
    RCC_CLK_MUX_SPI2_PIN              , /**< SPI/I2S 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_SPI2_LPCLK            , /**< SPI/I2S 2 clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* SPI2 */
#if defined(SPI3)
    RCC_CLK_MUX_SPI3_PLL1Q            , /**< SPI/I2S 3 clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_SPI3_PLL2P            , /**< SPI/I2S 3 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source */
    RCC_CLK_MUX_SPI3_PLL3P            , /**< SPI/I2S 3 clock enable with Phase Locked Loop 3 output P (PLL3P) as clock source */
    RCC_CLK_MUX_SPI3_PIN              , /**< SPI/I2S 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_SPI3_LPCLK            , /**< SPI/I2S 3 clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* SPI3 */
#if defined(SPI4)
    RCC_CLK_MUX_SPI4_PCLK2            , /**< SPI 4 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_SPI4_PLL2Q            , /**< SPI 4 clock enable with Phase Locked Loop 2 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_SPI4_PLL3Q            , /**< SPI 4 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_SPI4_HSI64            , /**< SPI 4 clock enable with 64MHz High Speed Internal oscillator (HSI64) as clock source */
    RCC_CLK_MUX_SPI4_CSI              , /**< SPI 4 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_CLK_MUX_SPI4_HSE              , /**< SPI 4 clock enable with High Speed External clock (HSE) as clock source */
#endif /* SPI4 */
#if defined(SPI5)
    RCC_CLK_MUX_SPI5_PCLK3            , /**< SPI 5 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_SPI5_PLL2Q            , /**< SPI 5 clock enable with Phase Locked Loop 2 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_SPI5_PLL3Q            , /**< SPI 5 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_SPI5_HSI64            , /**< SPI 5 clock enable with 64MHz High Speed Internal oscillator (HSI64) as clock source */
    RCC_CLK_MUX_SPI5_CSI              , /**< SPI 5 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_CLK_MUX_SPI5_HSE              , /**< SPI 5 clock enable with High Speed External clock (HSE) as clock source */
#endif /* SPI5 */
#if defined(SPI6)
    RCC_CLK_MUX_SPI6_PCLK2            , /**< SPI 6 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_SPI6_PLL2Q            , /**< SPI 6 clock enable with Phase Locked Loop 2 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_SPI6_PLL3Q            , /**< SPI 6 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_SPI6_HSI64            , /**< SPI 6 clock enable with 64MHz High Speed Internal oscillator (HSI64) as clock source */
    RCC_CLK_MUX_SPI6_CSI              , /**< SPI 6 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_CLK_MUX_SPI6_HSE              , /**< SPI 6 clock enable with High Speed External clock (HSE) as clock source */
#endif /* SPI6 */


#if defined(I2C1)
    RCC_CLK_MUX_I2C1_PCLK1            , /**< I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C1_PLL3R            , /**< I2C 1 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_CLK_MUX_I2C1_HSI64            , /**< I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C1_CSI              , /**< I2C 1 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_CLK_MUX_I2C2_PCLK1            , /**< I2C 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C2_PLL3R            , /**< I2C 2 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_CLK_MUX_I2C2_HSI64            , /**< I2C 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C2_CSI              , /**< I2C 2 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_CLK_MUX_I2C3_PCLK3            , /**< I2C 3 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_I2C3_PLL3R            , /**< I2C 3 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_CLK_MUX_I2C3_HSI64            , /**< I2C 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C3_CSI              , /**< I2C 3 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_CLK_MUX_I2C4_PCLK3            , /**< I2C 4 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_I2C4_PLL3R            , /**< I2C 4 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_CLK_MUX_I2C4_HSI64            , /**< I2C 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C4_CSI              , /**< I2C 4 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C4 */


#if defined(I3C1)
    RCC_CLK_MUX_I3C1_PCLK1            , /**< I3C 1 Clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I3C1_PLL3R            , /**< I3C 1 Clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_CLK_MUX_I3C1_HSI64            , /**< I3C 1 Clock enable with 64MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I3C1 */
#if defined(I3C2)
    RCC_CLK_MUX_I3C2_PCLK1            , /**< I3C 2 Clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I3C2_PLL3R            , /**< I3C 2 Clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_CLK_MUX_I3C2_HSI64            , /**< I3C 2 Clock enable with 64MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I3C2 */


#if defined(USART1)
    RCC_CLK_MUX_USART1_PCLK2          , /**< USART 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART1_PLL2Q          , /**< USART 1 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_USART1_PLL3Q          , /**< USART 1 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USART1_HSI            , /**< USART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART1_LSE            , /**< USART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_USART1_CSI            , /**< USART 1 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART1 */
#if defined(USART2)
    RCC_CLK_MUX_USART2_PCLK1          , /**< USART 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART2_PLL2Q          , /**< USART 2 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_USART2_PLL3Q          , /**< USART 2 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USART2_HSI            , /**< USART 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART2_LSE            , /**< USART 2 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_USART2_CSI            , /**< USART 2 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART2 */
#if defined(USART3)
    RCC_CLK_MUX_USART3_PCLK1          , /**< USART 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART3_PLL2Q          , /**< USART 3 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_USART3_PLL3Q          , /**< USART 3 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USART3_HSI            , /**< USART 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART3_LSE            , /**< USART 3 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_USART3_CSI            , /**< USART 3 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART3 */
#if defined(USART6)
    RCC_CLK_MUX_USART6_PCLK1          , /**< USART 6 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART6_PLL2Q          , /**< USART 6 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_USART6_PLL3Q          , /**< USART 6 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USART6_HSI            , /**< USART 6 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART6_LSE            , /**< USART 6 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_USART6_CSI            , /**< USART 6 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART6 */
#if defined(USART10)
    RCC_CLK_MUX_USART10_PCLK1         , /**< USART 10 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART10_PLL2Q         , /**< USART 10 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_USART10_PLL3Q         , /**< USART 10 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USART10_HSI           , /**< USART 10 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART10_LSE           , /**< USART 10 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_USART10_CSI           , /**< USART 10 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART10 */
#if defined(USART11)
    RCC_CLK_MUX_USART11_PCLK1         , /**< USART 11 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART11_PLL2Q         , /**< USART 11 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_USART11_PLL3Q         , /**< USART 11 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USART11_HSI           , /**< USART 11 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART11_LSE           , /**< USART 11 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_USART11_CSI           , /**< USART 11 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART11 */


#if defined(UART4)
    RCC_CLK_MUX_UART4_PCLK1           , /**< UART 4 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_UART4_PLL2Q           , /**< UART 4 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_UART4_PLL3Q           , /**< UART 4 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_UART4_HSI             , /**< UART 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART4_LSE             , /**< UART 4 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_UART4_CSI             , /**< UART 4 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART4 */
#if defined(UART5)
    RCC_CLK_MUX_UART5_PCLK1           , /**< UART 5 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_UART5_PLL2Q           , /**< UART 5 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_UART5_PLL3Q           , /**< UART 5 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_UART5_HSI             , /**< UART 5 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART5_LSE             , /**< UART 5 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_UART5_CSI             , /**< UART 5 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART5 */
#if defined(UART7)
    RCC_CLK_MUX_UART7_PCLK1           , /**< UART 7 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_UART7_PLL2Q           , /**< UART 7 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_UART7_PLL3Q           , /**< UART 7 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_UART7_HSI             , /**< UART 7 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART7_LSE             , /**< UART 7 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_UART7_CSI             , /**< UART 7 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART7 */
#if defined(UART8)
    RCC_CLK_MUX_UART8_PCLK1           , /**< UART 8 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_UART8_PLL2Q           , /**< UART 8 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_UART8_PLL3Q           , /**< UART 8 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_UART8_HSI             , /**< UART 8 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART8_LSE             , /**< UART 8 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_UART8_CSI             , /**< UART 8 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART8 */
#if defined(UART9)
    RCC_CLK_MUX_UART9_PCLK1           , /**< UART 9 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_UART9_PLL2Q           , /**< UART 9 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_UART9_PLL3Q           , /**< UART 9 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_UART9_HSI             , /**< UART 9 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART9_LSE             , /**< UART 9 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_UART9_CSI             , /**< UART 9 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART9 */
#if defined(UART12)
    RCC_CLK_MUX_UART12_PCLK1          , /**< UART 12 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_UART12_PLL2Q          , /**< UART 12 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_UART12_PLL3Q          , /**< UART 12 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_UART12_HSI            , /**< UART 12 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART12_LSE            , /**< UART 12 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_UART12_CSI            , /**< UART 12 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART12 */


#if defined(LPUART1)
    RCC_CLK_MUX_LPUART1_PCLK3         , /**< Low-Power UART 1 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_LPUART1_PLL2Q         , /**< Low-Power UART 1 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_LPUART1_PLL3Q         , /**< Low-Power UART 1 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_LPUART1_HSI           , /**< Low-Power UART 1 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_LPUART1_LSE           , /**< Low-Power UART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_LPUART1_CSI           , /**< Low-Power UART 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* LPUART1 */

#if defined(FDCAN1)
    RCC_CLK_MUX_FDCAN_PLL1Q           , /**< FDCAN clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source */
    RCC_CLK_MUX_FDCAN_PLL2Q           , /**< FDCAN clock enable with Phase Locked Loop 2 output Q (PLL2Q) used as clock source */
    RCC_CLK_MUX_FDCAN_HSE             , /**< FDCAN clock enable with High Speed External oscillator (HSE) used as clock source */
#endif /* FDCAN1 */

#if defined(SDMMC1)
    RCC_CLK_MUX_SDMMC1_PLL1Q          , /**< SDMMC1 clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source for SDMMC 1 */
    RCC_CLK_MUX_SDMMC1_PLL2R          , /**< SDMMC1 clock enable with Phase Locked Loop 2 output R (PLL2R) used as clock source for SDMMC 1 */
#endif /* SDMMC1 */
#if defined(SDMMC2)
    RCC_CLK_MUX_SDMMC2_PLL1Q          , /**< SDMMC1 clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source for SDMMC 2 */
    RCC_CLK_MUX_SDMMC2_PLL2R          , /**< SDMMC1 clock enable with Phase Locked Loop 2 output R (PLL2R) used as clock source for SDMMC 2 */
#endif /* SDMMC2 */

#if defined(OCTOSPI1)
    RCC_CLK_MUX_OCTOSPI1_HCLK         , /**< Octal SPI 1 (OCTOSPI) clock enable with HCLK clock as clock source for QSPI 1 */
    RCC_CLK_MUX_OCTOSPI1_PLL1Q        , /**< Octal SPI 1 (OCTOSPI) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source for QSPI 1 */
    RCC_CLK_MUX_OCTOSPI1_PLL2R        , /**< Octal SPI 1 (OCTOSPI) clock enable with Phase Locked Loop 2 output R (PLL2R) as clock source for QSPI 1 */
    RCC_CLK_MUX_OCTOSPI1_LPCLK        , /**< Octal SPI 1 (OCTOSPI) clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* OCTOSPI1 */

#if defined(USB_DRD_FS)
    RCC_CLK_MUX_USB_NONE              , /**< USB Full Speed (12Mbps) clock disable */
    RCC_CLK_MUX_USB_PLL1Q             , /**< USB Full Speed (12Mbps) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_USB_PLL3Q             , /**< USB Full Speed (12Mbps) clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_CLK_MUX_USB_HSI48             , /**< USB Full Speed (12Mbps) clock enable with 48MHz High Speed Internal (HSE48) as clock source */
#endif /* USB_OTG_HS */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(SAI1)
    RCC_CLK_MUX_SAI1_PLL2P            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_CLK_MUX_SAI1_PLL3P            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_CLK_MUX_SAI1_PLL1Q            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 Q output as clock source */
    RCC_CLK_MUX_SAI1_CKIN             , /**< Serial Audio Interface 1 (SAI1) clock enable with clock input through pin as clock source */
    RCC_CLK_MUX_SAI1_LPCLK            , /**< Serial Audio Interface 1 (SAI1) clock enable with Low-Power Clock Domain (PER) as clock source*/
#endif /* SAI1 */
#if defined(SAI2)
    RCC_CLK_MUX_SAI2_PLL2P            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_CLK_MUX_SAI2_PLL3P            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_CLK_MUX_SAI2_PLL1Q            , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 Q output as clock source */
    RCC_CLK_MUX_SAI2_CKIN             , /**< Serial Audio Interface 2 (SAI2) clock enable with clock input through pin as clock source */
    RCC_CLK_MUX_SAI2_LPCLK            , /**< Serial Audio Interface 2 (SAI2) clock enable with Low-Power Clock Domain (PER) as clock source*/
#endif /* SAI2 */

#if defined(CEC)
    RCC_CLK_MUX_CEC_LSE               , /**< Consumer Electronics Control (CEC) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_CLK_MUX_CEC_CSI_DIV           , /**< Consumer Electronics Control (CEC) clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_CLK_MUX_CEC_LSI               , /**< Consumer Electronics Control (CEC) clock enable with Low Speed Internal oscillator (LSI) as clock source */
#endif /* CEC */

    /*-------------------------------- Analog --------------------------------*/

#if defined(ADC1) || defined(ADC2) || defined(DAC1)
    RCC_CLK_MUX_ADC_DAC_HCLK          , /**< ADC & DAC with HCLK as clock source. */
    RCC_CLK_MUX_ADC_DAC_SYSCLK        , /**< ADC & DAC with SysClk as clock source. */
    RCC_CLK_MUX_ADC_DAC_PLL2R         , /**< ADC & DAC with PLL2 output R as clock source. */
    RCC_CLK_MUX_ADC_DAC_HSE           , /**< ADC & DAC with High Speed External oscillator (HSE) as clock source. */
    RCC_CLK_MUX_ADC_DAC_HSI           , /**< ADC & DAC with 64MHz High Speed Internal oscillator (HSI) as clock source. */
    RCC_CLK_MUX_ADC_DAC_CSI           , /**< ADC & DAC with 4MHz Low Power Internal oscillator (CSI) as clock source. */
#endif /* ADC1 */

#if defined(DAC1)
    RCC_CLK_MUX_DAC_SAH_LSE           , /**< DAC Sample & Hold with Low Speed External oscillator (LSE) as clock source. */
    RCC_CLK_MUX_DAC_SAH_LSI           , /**< DAC Sample & Hold with Low Speed Internal oscillator (LSI) as clock source. */
#endif /* DAC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(RNG)
    RCC_CLK_MUX_RNG_HSI48             , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
    RCC_CLK_MUX_RNG_PLL1Q             , /**< Random Number Generator (RNG) clock enable with PLL 1 Q output used as clock source */
    RCC_CLK_MUX_RNG_LSE               , /**< Random Number Generator (RNG) clock enable with Low Speed External oscillator (LSE) used as clock source */
    RCC_CLK_MUX_RNG_LSI               , /**< Random Number Generator (RNG) clock enable with Low Speed Internal oscillator (LSI) used as clock source */
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
