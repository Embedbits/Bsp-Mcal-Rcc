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
 * - RTC
 * - Low Power Timer 1 (LPTIM1)
 * - Fast-mode Plus I2C 1 (FMPI2C1)
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
    RCC_CLK_MUX_RTC_NONE           = 0u, /**< RTC clock source: no clock (reset value) */
    RCC_CLK_MUX_RTC_LSE                , /**< RTC clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_RTC_LSI                , /**< RTC clock source: Low Speed Internal (LSI) oscillator */
    RCC_CLK_MUX_RTC_HSE_DIV            , /**< RTC clock source: High Speed External (HSE) oscillator divided by RTCPRE */
    RCC_CLK_MUX_LPTIM1_PCLK1           , /**< LPTIM1 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_LPTIM1_LSI             , /**< LPTIM1 clock source: Low Speed Internal (LSI) oscillator */
    RCC_CLK_MUX_LPTIM1_HSI             , /**< LPTIM1 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_LPTIM1_LSE             , /**< LPTIM1 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_I2C1_PCLK1             , /**< I2C1 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_I2C1_SYSCLK            , /**< I2C1 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C1_HSI               , /**< I2C1 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_I2C2_PCLK1             , /**< I2C2 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_I2C2_SYSCLK            , /**< I2C2 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C2_HSI               , /**< I2C2 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_I2C3_PCLK1             , /**< I2C3 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_I2C3_SYSCLK            , /**< I2C3 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C3_HSI               , /**< I2C3 clock source: 16MHz High Speed Internal (HSI) oscillator */
#if defined(RCC_APB1ENR_I2C4EN)
    RCC_CLK_MUX_I2C4_PCLK1             , /**< I2C4 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_I2C4_SYSCLK            , /**< I2C4 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_I2C4_HSI               , /**< I2C4 clock source: 16MHz High Speed Internal (HSI) oscillator */
#endif /* RCC_APB1ENR_I2C4EN */
    RCC_CLK_MUX_USART1_PCLK2           , /**< USART1 clock source: APB2 clock (PCLK2) */
    RCC_CLK_MUX_USART1_SYSCLK          , /**< USART1 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART1_HSI             , /**< USART1 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_USART1_LSE             , /**< USART1 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_USART2_PCLK1           , /**< USART2 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_USART2_SYSCLK          , /**< USART2 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART2_HSI             , /**< USART2 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_USART2_LSE             , /**< USART2 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_USART3_PCLK1           , /**< USART3 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_USART3_SYSCLK          , /**< USART3 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART3_HSI             , /**< USART3 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_USART3_LSE             , /**< USART3 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_UART4_PCLK1            , /**< UART4 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_UART4_SYSCLK           , /**< UART4 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_UART4_HSI              , /**< UART4 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_UART4_LSE              , /**< UART4 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_UART5_PCLK1            , /**< UART5 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_UART5_SYSCLK           , /**< UART5 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_UART5_HSI              , /**< UART5 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_UART5_LSE              , /**< UART5 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_USART6_PCLK2           , /**< USART6 clock source: APB2 clock (PCLK2) */
    RCC_CLK_MUX_USART6_SYSCLK          , /**< USART6 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_USART6_HSI             , /**< USART6 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_USART6_LSE             , /**< USART6 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_UART7_PCLK1            , /**< UART7 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_UART7_SYSCLK           , /**< UART7 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_UART7_HSI              , /**< UART7 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_UART7_LSE              , /**< UART7 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_UART8_PCLK1            , /**< UART8 clock source: APB1 clock (PCLK1) */
    RCC_CLK_MUX_UART8_SYSCLK           , /**< UART8 clock source: system clock (SYSCLK) */
    RCC_CLK_MUX_UART8_HSI              , /**< UART8 clock source: 16MHz High Speed Internal (HSI) oscillator */
    RCC_CLK_MUX_UART8_LSE              , /**< UART8 clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_SDMMC1_PLL48           , /**< SDMMC1 clock source: 48 MHz clock (PLL48CLK) */
    RCC_CLK_MUX_SDMMC1_SYSCLK          , /**< SDMMC1 clock source: system clock (SYSCLK) */
#if defined(RCC_APB2ENR_SDMMC2EN)
    RCC_CLK_MUX_SDMMC2_PLL48           , /**< SDMMC2 clock source: 48 MHz clock (PLL48CLK) */
    RCC_CLK_MUX_SDMMC2_SYSCLK          , /**< SDMMC2 clock source: system clock (SYSCLK) */
#endif /* RCC_APB2ENR_SDMMC2EN */
    RCC_CLK_MUX_SAI1_PLLSAI            , /**< SAI1 clock source: PLLSAI output Q divided by PLLSAIDIVQ */
    RCC_CLK_MUX_SAI1_PLLI2S            , /**< SAI1 clock source: PLLI2S output Q divided by PLLI2SDIVQ */
    RCC_CLK_MUX_SAI1_PIN               , /**< SAI1 clock source: external clock pin I2S_CKIN */
#if defined(RCC_SAI1SEL_PLLSRC_SUPPORT)
    RCC_CLK_MUX_SAI1_PLLSRC            , /**< SAI1 clock source: PLL source clock (HSI or HSE) */
#endif /* RCC_SAI1SEL_PLLSRC_SUPPORT */
    RCC_CLK_MUX_SAI2_PLLSAI            , /**< SAI2 clock source: PLLSAI output Q divided by PLLSAIDIVQ */
    RCC_CLK_MUX_SAI2_PLLI2S            , /**< SAI2 clock source: PLLI2S output Q divided by PLLI2SDIVQ */
    RCC_CLK_MUX_SAI2_PIN               , /**< SAI2 clock source: external clock pin I2S_CKIN */
#if defined(RCC_SAI2SEL_PLLSRC_SUPPORT)
    RCC_CLK_MUX_SAI2_PLLSRC            , /**< SAI2 clock source: PLL source clock (HSI or HSE) */
#endif /* RCC_SAI2SEL_PLLSRC_SUPPORT */
#if defined(RCC_APB1ENR_CECEN)
    RCC_CLK_MUX_CEC_LSE                , /**< CEC clock source: Low Speed External (LSE) oscillator */
    RCC_CLK_MUX_CEC_HSI_DIV488         , /**< CEC clock source: HSI divided by 488 */
#endif /* RCC_APB1ENR_CECEN */
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_CLK_MUX_DFSDM1_PCLK2           , /**< DFSDM1 clock source: APB2 clock (PCLK2) */
    RCC_CLK_MUX_DFSDM1_SYSCLK          , /**< DFSDM1 clock source: system clock (SYSCLK) */
#endif /* RCC_APB2ENR_DFSDM1EN */
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
