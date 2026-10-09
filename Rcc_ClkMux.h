/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.h
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Peripheral's Clock Multiplexer (ClkMux)
 *        component functionality header file.
 *
 * This file contains the interface functions and types used by the RCC
 * module Clock Multiplexer (ClkMux) component.
 * List of STM32U5 peripherals with configurable clock sources (RCC CCIPR1 /
 * CCIPR2 / CCIPR3 and BDCR):
 * - System Tick timer external reference (SYSTICKSEL)
 * - Real Time Clock (RTCSEL)
 * - Low Power Timers (LPTIM1, LPTIM2, LPTIM3 / LPTIM4 shared multiplexer)
 * - SPI (SPI1, SPI2, SPI3)
 * - I2C (I2C1, I2C2, I2C3, I2C4, I2C5, I2C6)
 * - USART (USART1, USART2, USART3, UART4, UART5, USART6), LPUART1
 * - FDCAN1, USB (ICLKSEL intermediate clock), OCTOSPI1 / OCTOSPI2 (shared)
 * - SAI (SAI1, SAI2), MDF1, ADF1
 * - RNG
 * - ADC1 / ADC2, ADC4 and DAC1 (ADCDACSEL shared), DAC1 sample and hold (DAC1SEL)
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
    RCC_CLK_MUX_SYSTICK_HCLK_DIV8 = 0u, /**< SYSTICK clock source: AHB clock (HCLK) divided by 8 */
    RCC_CLK_MUX_SYSTICK_LSI           , /**< SYSTICK clock source: Low Speed Internal oscillator (LSI) */
    RCC_CLK_MUX_SYSTICK_LSE           , /**< SYSTICK clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_RTC_NONE              , /**< RTC clock source: no clock */
    RCC_CLK_MUX_RTC_LSE               , /**< RTC clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_RTC_LSI               , /**< RTC clock source: Low Speed Internal oscillator (LSI) */
    RCC_CLK_MUX_RTC_HSE_DIV32         , /**< RTC clock source: High Speed External oscillator (HSE) divided by 32 */
    RCC_CLK_MUX_LPTIM1_MSIK           , /**< LPTIM1 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_LPTIM1_LSI            , /**< LPTIM1 clock source: Low Speed Internal oscillator (LSI) */
    RCC_CLK_MUX_LPTIM1_HSI            , /**< LPTIM1 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_LPTIM1_LSE            , /**< LPTIM1 clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_LPTIM2_PCLK1          , /**< LPTIM2 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_LPTIM2_LSI            , /**< LPTIM2 clock source: Low Speed Internal oscillator (LSI) */
    RCC_CLK_MUX_LPTIM2_HSI            , /**< LPTIM2 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_LPTIM2_LSE            , /**< LPTIM2 clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_LPTIM34_MSIK          , /**< LPTIM34 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_LPTIM34_LSI           , /**< LPTIM34 clock source: Low Speed Internal oscillator (LSI) */
    RCC_CLK_MUX_LPTIM34_HSI           , /**< LPTIM34 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_LPTIM34_LSE           , /**< LPTIM34 clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_SPI1_PCLK2            , /**< SPI1 clock source: APB2 (PCLK2) */
    RCC_CLK_MUX_SPI1_SYSCLK           , /**< SPI1 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_SPI1_HSI              , /**< SPI1 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_SPI1_MSIK             , /**< SPI1 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_SPI2_PCLK1            , /**< SPI2 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_SPI2_SYSCLK           , /**< SPI2 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_SPI2_HSI              , /**< SPI2 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_SPI2_MSIK             , /**< SPI2 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_SPI3_PCLK3            , /**< SPI3 clock source: APB3 (PCLK3) */
    RCC_CLK_MUX_SPI3_SYSCLK           , /**< SPI3 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_SPI3_HSI              , /**< SPI3 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_SPI3_MSIK             , /**< SPI3 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_I2C1_PCLK1            , /**< I2C1 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_I2C1_SYSCLK           , /**< I2C1 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C1_HSI              , /**< I2C1 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_I2C1_MSIK             , /**< I2C1 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_I2C2_PCLK1            , /**< I2C2 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_I2C2_SYSCLK           , /**< I2C2 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C2_HSI              , /**< I2C2 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_I2C2_MSIK             , /**< I2C2 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_I2C3_PCLK3            , /**< I2C3 clock source: APB3 (PCLK3) */
    RCC_CLK_MUX_I2C3_SYSCLK           , /**< I2C3 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C3_HSI              , /**< I2C3 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_I2C3_MSIK             , /**< I2C3 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_I2C4_PCLK1            , /**< I2C4 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_I2C4_SYSCLK           , /**< I2C4 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C4_HSI              , /**< I2C4 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_I2C4_MSIK             , /**< I2C4 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
#if defined(I2C5)
    RCC_CLK_MUX_I2C5_PCLK1            , /**< I2C5 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_I2C5_SYSCLK           , /**< I2C5 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C5_HSI              , /**< I2C5 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_I2C5_MSIK             , /**< I2C5 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
#endif /* I2C5 */
#if defined(I2C6)
    RCC_CLK_MUX_I2C6_PCLK1            , /**< I2C6 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_I2C6_SYSCLK           , /**< I2C6 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C6_HSI              , /**< I2C6 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_I2C6_MSIK             , /**< I2C6 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
#endif /* I2C6 */
    RCC_CLK_MUX_USART1_PCLK2          , /**< USART1 clock source: APB2 (PCLK2) */
    RCC_CLK_MUX_USART1_SYSCLK         , /**< USART1 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART1_HSI            , /**< USART1 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_USART1_LSE            , /**< USART1 clock source: Low Speed External oscillator (LSE) */
#if defined(USART2)
    RCC_CLK_MUX_USART2_PCLK1          , /**< USART2 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_USART2_SYSCLK         , /**< USART2 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART2_HSI            , /**< USART2 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_USART2_LSE            , /**< USART2 clock source: Low Speed External oscillator (LSE) */
#endif /* USART2 */
    RCC_CLK_MUX_USART3_PCLK1          , /**< USART3 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_USART3_SYSCLK         , /**< USART3 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART3_HSI            , /**< USART3 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_USART3_LSE            , /**< USART3 clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_UART4_PCLK1           , /**< UART4 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_UART4_SYSCLK          , /**< UART4 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_UART4_HSI             , /**< UART4 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_UART4_LSE             , /**< UART4 clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_UART5_PCLK1           , /**< UART5 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_UART5_SYSCLK          , /**< UART5 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_UART5_HSI             , /**< UART5 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_UART5_LSE             , /**< UART5 clock source: Low Speed External oscillator (LSE) */
#if defined(USART6)
    RCC_CLK_MUX_USART6_PCLK1          , /**< USART6 clock source: APB1 (PCLK1) */
    RCC_CLK_MUX_USART6_SYSCLK         , /**< USART6 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART6_HSI            , /**< USART6 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_USART6_LSE            , /**< USART6 clock source: Low Speed External oscillator (LSE) */
#endif /* USART6 */
    RCC_CLK_MUX_LPUART1_PCLK3         , /**< LPUART1 clock source: APB3 (PCLK3) */
    RCC_CLK_MUX_LPUART1_SYSCLK        , /**< LPUART1 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_LPUART1_HSI           , /**< LPUART1 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_LPUART1_LSE           , /**< LPUART1 clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_LPUART1_MSIK          , /**< LPUART1 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_FDCAN1_HSE            , /**< FDCAN1 clock source: High Speed External oscillator (HSE) */
    RCC_CLK_MUX_FDCAN1_PLL1Q          , /**< FDCAN1 clock source: PLL1 output Q */
    RCC_CLK_MUX_FDCAN1_PLL2P          , /**< FDCAN1 clock source: PLL2 output P */
#if defined(RCC_APB2ENR_USBEN)
    RCC_CLK_MUX_USB_HSI48             , /**< USB clock source: 48 MHz High Speed Internal oscillator (HSI48) */
    RCC_CLK_MUX_USB_PLL2Q             , /**< USB clock source: PLL2 output Q */
    RCC_CLK_MUX_USB_PLL1Q             , /**< USB clock source: PLL1 output Q */
    RCC_CLK_MUX_USB_MSIK              , /**< USB clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
    RCC_CLK_MUX_USB_HSI48             , /**< USB clock source: 48 MHz High Speed Internal oscillator (HSI48) */
    RCC_CLK_MUX_USB_PLL2Q             , /**< USB clock source: PLL2 output Q */
    RCC_CLK_MUX_USB_PLL1Q             , /**< USB clock source: PLL1 output Q */
    RCC_CLK_MUX_USB_MSIK              , /**< USB clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
#endif /* USB */
    RCC_CLK_MUX_OCTOSPI_SYSCLK        , /**< OCTOSPI clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_OCTOSPI_MSIK          , /**< OCTOSPI clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_OCTOSPI_PLL1Q         , /**< OCTOSPI clock source: PLL1 output Q */
    RCC_CLK_MUX_OCTOSPI_PLL2Q         , /**< OCTOSPI clock source: PLL2 output Q */
    RCC_CLK_MUX_SAI1_PLL2P            , /**< SAI1 clock source: PLL2 output P */
    RCC_CLK_MUX_SAI1_PLL3P            , /**< SAI1 clock source: PLL3 output P */
    RCC_CLK_MUX_SAI1_PLL1P            , /**< SAI1 clock source: PLL1 output P */
    RCC_CLK_MUX_SAI1_PIN              , /**< SAI1 clock source: external clock input pin */
    RCC_CLK_MUX_SAI1_HSI              , /**< SAI1 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
#if defined(SAI2)
    RCC_CLK_MUX_SAI2_PLL2P            , /**< SAI2 clock source: PLL2 output P */
    RCC_CLK_MUX_SAI2_PLL3P            , /**< SAI2 clock source: PLL3 output P */
    RCC_CLK_MUX_SAI2_PLL1P            , /**< SAI2 clock source: PLL1 output P */
    RCC_CLK_MUX_SAI2_PIN              , /**< SAI2 clock source: external clock input pin */
    RCC_CLK_MUX_SAI2_HSI              , /**< SAI2 clock source: 16 MHz High Speed Internal oscillator (HSI16) */
#endif /* SAI2 */
    RCC_CLK_MUX_MDF1_HCLK             , /**< MDF1 clock source: AHB (HCLK) */
    RCC_CLK_MUX_MDF1_PLL1P            , /**< MDF1 clock source: PLL1 output P */
    RCC_CLK_MUX_MDF1_PLL3Q            , /**< MDF1 clock source: PLL3 output Q */
    RCC_CLK_MUX_MDF1_PIN              , /**< MDF1 clock source: external clock input pin */
    RCC_CLK_MUX_MDF1_MSIK             , /**< MDF1 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_ADF1_HCLK             , /**< ADF1 clock source: AHB (HCLK) */
    RCC_CLK_MUX_ADF1_PLL1P            , /**< ADF1 clock source: PLL1 output P */
    RCC_CLK_MUX_ADF1_PLL3Q            , /**< ADF1 clock source: PLL3 output Q */
    RCC_CLK_MUX_ADF1_PIN              , /**< ADF1 clock source: external clock input pin */
    RCC_CLK_MUX_ADF1_MSIK             , /**< ADF1 clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_ADC_DAC_HCLK          , /**< ADC_DAC clock source: AHB (HCLK) */
    RCC_CLK_MUX_ADC_DAC_SYSCLK        , /**< ADC_DAC clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_ADC_DAC_PLL2R         , /**< ADC_DAC clock source: PLL2 output R */
    RCC_CLK_MUX_ADC_DAC_HSE           , /**< ADC_DAC clock source: High Speed External oscillator (HSE) */
    RCC_CLK_MUX_ADC_DAC_HSI           , /**< ADC_DAC clock source: 16 MHz High Speed Internal oscillator (HSI16) */
    RCC_CLK_MUX_ADC_DAC_MSIK          , /**< ADC_DAC clock source: Multi-Speed Internal oscillator kernel output (MSIK) */
    RCC_CLK_MUX_DAC_SAH_LSE           , /**< DAC_SAH clock source: Low Speed External oscillator (LSE) */
    RCC_CLK_MUX_DAC_SAH_LSI           , /**< DAC_SAH clock source: Low Speed Internal oscillator (LSI) */
    RCC_CLK_MUX_RNG_HSI48             , /**< RNG clock source: 48 MHz High Speed Internal oscillator (HSI48) */
    RCC_CLK_MUX_RNG_HSI48_DIV2        , /**< RNG clock source: 48 MHz High Speed Internal oscillator (HSI48) divided by 2 */
    RCC_CLK_MUX_RNG_HSI               , /**< RNG clock source: 16 MHz High Speed Internal oscillator (HSI16) */

    RCC_CLK_MUX_LIST_CNT
}   rcc_ClkMuxId_t;

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

rcc_RequestState_t          Rcc_ClkMux_Init             ( void );
void                        Rcc_ClkMux_Deinit           ( void );

rcc_RequestState_t          Rcc_ClkMux_Set_ClkActive    ( rcc_ClkMuxId_t clkMuxId  );
rcc_RequestState_t          Rcc_ClkMux_Set_ClkInactive  ( rcc_ClkMuxId_t clkMuxId  );

rcc_RequestState_t          Rcc_ClkMux_Get_ClkSrc       ( rcc_ClkMuxId_t clkMuxIdIn, rcc_ClkMuxId_t * const clkMuxId );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKMUX_RCC_CLKMUX_H */
