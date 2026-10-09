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
 * - RTC (BDCR RTCSEL)
 * - LPTIM1, I2S of SPI2 / SPI3, I2C1 - I2C3, USART1 - USART3, UART4, UART5,
 *   LPUART1, FDCAN, 48 MHz clock (RNG, USB), SAI1, ADC12, ADC345 (CCIPR)
 * - I2C4, QSPI (CCIPR2)
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
    RCC_CLK_MUX_RTC_NONE      = 0u, /**< Real Time Clock disabled */
    RCC_CLK_MUX_RTC_LSE           , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_CLK_MUX_RTC_LSI           , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */
    RCC_CLK_MUX_RTC_HSE_DIV32     , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source */

    /*-------------------------------- Timers --------------------------------*/
    RCC_CLK_MUX_LPTIM1_PCLK1      , /**< Low Power Timer 1 with APB1 (PCLK1) as clock source. */
    RCC_CLK_MUX_LPTIM1_LSI        , /**< Low Power Timer 1 with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM1_HSI        , /**< Low Power Timer 1 with 16MHz High Speed Internal (HSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM1_LSE        , /**< Low Power Timer 1 with Low Speed External (LSE) oscillator as clock source. */

    /*----------------------------- Connectivity -----------------------------*/
#if defined(RCC_CCIPR_I2S23SEL)
    RCC_CLK_MUX_I2S23_SYSCLK      , /**< I2S of SPI2 / SPI3 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_I2S23_PLLQ        , /**< I2S of SPI2 / SPI3 with PLL output Q as clock source */
    RCC_CLK_MUX_I2S23_I2S_CKIN    , /**< I2S of SPI2 / SPI3 with external I2S_CKIN pin as clock source */
    RCC_CLK_MUX_I2S23_HSI         , /**< I2S of SPI2 / SPI3 with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_CCIPR_I2S23SEL */

    RCC_CLK_MUX_I2C1_PCLK1        , /**< I2C 1 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C1_SYSCLK       , /**< I2C 1 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_I2C1_HSI          , /**< I2C 1 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_I2C2_PCLK1        , /**< I2C 2 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C2_SYSCLK       , /**< I2C 2 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_I2C2_HSI          , /**< I2C 2 with 16MHz High Speed Internal (HSI) oscillator as clock source */
#if defined(RCC_APB1ENR1_I2C3EN)
    RCC_CLK_MUX_I2C3_PCLK1        , /**< I2C 3 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C3_SYSCLK       , /**< I2C 3 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_I2C3_HSI          , /**< I2C 3 with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB1ENR1_I2C3EN */
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_CLK_MUX_I2C4_PCLK1        , /**< I2C 4 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C4_SYSCLK       , /**< I2C 4 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_I2C4_HSI          , /**< I2C 4 with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB1ENR2_I2C4EN */

    RCC_CLK_MUX_USART1_PCLK2      , /**< USART 1 with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART1_SYSCLK     , /**< USART 1 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_USART1_HSI        , /**< USART 1 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_USART1_LSE        , /**< USART 1 with Low Speed External (LSE) oscillator as clock source */
    RCC_CLK_MUX_USART2_PCLK1      , /**< USART 2 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART2_SYSCLK     , /**< USART 2 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_USART2_HSI        , /**< USART 2 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_USART2_LSE        , /**< USART 2 with Low Speed External (LSE) oscillator as clock source */
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_CLK_MUX_USART3_PCLK1      , /**< USART 3 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART3_SYSCLK     , /**< USART 3 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_USART3_HSI        , /**< USART 3 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_USART3_LSE        , /**< USART 3 with Low Speed External (LSE) oscillator as clock source */
#endif /* RCC_APB1ENR1_USART3EN */
    RCC_CLK_MUX_UART4_PCLK1       , /**< UART 4 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_UART4_SYSCLK      , /**< UART 4 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_UART4_HSI         , /**< UART 4 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_UART4_LSE         , /**< UART 4 with Low Speed External (LSE) oscillator as clock source */
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_CLK_MUX_UART5_PCLK1       , /**< UART 5 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_UART5_SYSCLK      , /**< UART 5 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_UART5_HSI         , /**< UART 5 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_UART5_LSE         , /**< UART 5 with Low Speed External (LSE) oscillator as clock source */
#endif /* RCC_APB1ENR1_UART5EN */
    RCC_CLK_MUX_LPUART1_PCLK1     , /**< Low-Power UART 1 with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_LPUART1_SYSCLK    , /**< Low-Power UART 1 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_LPUART1_HSI       , /**< Low-Power UART 1 with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_LPUART1_LSE       , /**< Low-Power UART 1 with Low Speed External (LSE) oscillator as clock source */

    RCC_CLK_MUX_FDCAN_HSE         , /**< FDCAN with High Speed External oscillator (HSE) as clock source */
    RCC_CLK_MUX_FDCAN_PLLQ        , /**< FDCAN with PLL output Q as clock source */
    RCC_CLK_MUX_FDCAN_PCLK1       , /**< FDCAN with APB1 (PCLK1) as clock source */

    RCC_CLK_MUX_CLK48_HSI48       , /**< 48 MHz clock (RNG, USB) - 48MHz High Speed Internal oscillator (HSI48) */
    RCC_CLK_MUX_CLK48_PLLQ        , /**< 48 MHz clock (RNG, USB) - PLL output Q */

#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_CLK_MUX_QSPI_SYSCLK       , /**< Quad SPI with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_QSPI_HSI          , /**< Quad SPI with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_QSPI_PLLQ         , /**< Quad SPI with PLL output Q as clock source */
#endif /* RCC_AHB3ENR_QSPIEN */

    /*------------------------------ Multimedia ------------------------------*/
#if defined(RCC_APB2ENR_SAI1EN)
    RCC_CLK_MUX_SAI1_SYSCLK       , /**< Serial Audio Interface 1 with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_SAI1_PLLQ         , /**< Serial Audio Interface 1 with PLL output Q as clock source */
    RCC_CLK_MUX_SAI1_I2S_CKIN     , /**< Serial Audio Interface 1 with external I2S_CKIN pin as clock source */
    RCC_CLK_MUX_SAI1_HSI          , /**< Serial Audio Interface 1 with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB2ENR_SAI1EN */

    /*-------------------------------- Analog --------------------------------*/
    RCC_CLK_MUX_ADC12_HCLK        , /**< ADC1 / ADC2 without asynchronous kernel clock (synchronous HCLK clock) */
    RCC_CLK_MUX_ADC12_PLLP        , /**< ADC1 / ADC2 with PLL output P as asynchronous kernel clock */
    RCC_CLK_MUX_ADC12_SYSCLK      , /**< ADC1 / ADC2 with system clock (SYSCLK) as asynchronous kernel clock */
#if defined(RCC_AHB2ENR_ADC345EN)
    RCC_CLK_MUX_ADC345_HCLK       , /**< ADC3 / ADC4 / ADC5 without asynchronous kernel clock (synchronous HCLK clock) */
    RCC_CLK_MUX_ADC345_PLLP       , /**< ADC3 / ADC4 / ADC5 with PLL output P as asynchronous kernel clock */
    RCC_CLK_MUX_ADC345_SYSCLK     , /**< ADC3 / ADC4 / ADC5 with system clock (SYSCLK) as asynchronous kernel clock */
#endif /* RCC_AHB2ENR_ADC345EN */

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
