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
    /*------------------------------ System core -----------------------------*/

    RCC_CLK_MUX_RTC_NONE      = 0u, /**< Real Time Clock disabled */
    RCC_CLK_MUX_RTC_LSE           , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_CLK_MUX_RTC_LSI           , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */
    RCC_CLK_MUX_RTC_HSE_DIV       , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by RTCPRE used as clock source */

    /*-------------------------------- Timers --------------------------------*/

#if defined(RCC_APB1ENR_LPTIM1EN)
    RCC_CLK_MUX_LPTIM1_PCLK1      , /**< Low Power Timer 1 clock enable with APB1 (PCLK1) as clock source. */
    RCC_CLK_MUX_LPTIM1_HSI        , /**< Low Power Timer 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM1_LSI        , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_CLK_MUX_LPTIM1_LSE        , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source. */
#endif /* RCC_APB1ENR_LPTIM1EN */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(RCC_APB1ENR_FMPI2C1EN)
    RCC_CLK_MUX_FMPI2C1_PCLK1     , /**< Fast-mode Plus I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_FMPI2C1_SYSCLK    , /**< Fast-mode Plus I2C 1 clock enable with system clock (SYSCLK) as clock source */
    RCC_CLK_MUX_FMPI2C1_HSI       , /**< Fast-mode Plus I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB1ENR_FMPI2C1EN */

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
