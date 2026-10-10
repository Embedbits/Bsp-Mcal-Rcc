/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.h
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality header file.
 *
 * This component contains functionality of the clock sources (oscillators)
 * and of the derived clocks (peripheral clock per_ck, CSI / 122, RTC HSE clock).
 *
 */

#ifndef RCC_CLKSRC_RCC_CLKSRC_H
#define RCC_CLKSRC_RCC_CLKSRC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Rcc_Types.h"                      /* Module types definition        */
/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ============================= TYPEDEFS =================================== */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

rcc_RequestState_t          Rcc_ClkSrc_Set_HseActive    ( rcc_HseType_t hseType );
rcc_RequestState_t          Rcc_ClkSrc_Set_HseInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_HseState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Set_HseClk       ( rcc_FreqHz_t clkFreq );
rcc_RequestState_t          Rcc_ClkSrc_Get_HseClk       ( rcc_FreqHz_t * const clkFreq );

rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi64Active  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi64Inactive( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi64State   ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi64Clk     ( rcc_FreqHz_t * const clkFreq );

rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi48Active  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi48Inactive( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi48State   ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi48Clk     ( rcc_FreqHz_t * const clkFreq );

rcc_RequestState_t          Rcc_ClkSrc_Set_CsiActive    ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_CsiInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_CsiState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_CsiClk       ( rcc_FreqHz_t * const clkFreq );
rcc_RequestState_t          Rcc_ClkSrc_Get_CsiDiv122Clk ( rcc_FreqHz_t * const clkFreq );

/*----------------------- Low Speed Clock configuration ----------------------*/

rcc_RequestState_t          Rcc_ClkSrc_Set_LseActive    ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_LseInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_LseState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_LseClk       ( rcc_FreqHz_t * const lseClk );

rcc_RequestState_t          Rcc_ClkSrc_Set_LsiActive    ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_LsiInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_LsiState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_LsiClk       ( rcc_FreqHz_t * const lsiClk );

/*--------------------------- Derived clock sources --------------------------*/

rcc_RequestState_t          Rcc_ClkSrc_Get_PerClk       ( rcc_FreqHz_t * const clkFreq );
rcc_RequestState_t          Rcc_ClkSrc_Get_RtcHseClk    ( rcc_FreqHz_t * const clkFreq );
rcc_RequestState_t          Rcc_ClkSrc_Get_PinClk       ( rcc_FreqHz_t * const clkFreq );
#if defined(STM32H7RS)
rcc_RequestState_t          Rcc_ClkSrc_Get_HseDiv2Clk   ( rcc_FreqHz_t * const clkFreq );
#endif

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKSRC_RCC_CLKSRC_H */
