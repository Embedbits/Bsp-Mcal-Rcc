/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.h
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality header file.
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

rcc_RequestState_t          Rcc_ClkSrc_Set_HseConfig    ( rcc_HseType_t hseType, rcc_FreqHz_t clkFreq );

rcc_RequestState_t          Rcc_ClkSrc_Set_HseActive    ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_HseInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_HseState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Set_HseType      ( rcc_HseType_t hseType );
rcc_RequestState_t          Rcc_ClkSrc_Set_HseClk       ( rcc_FreqHz_t clkFreq );
rcc_RequestState_t          Rcc_ClkSrc_Get_HseClk       ( rcc_FreqHz_t * const clkFreq );

rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi16Active  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi16Inactive( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi16State   ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi16Clk     ( rcc_FreqHz_t * const clkFreq );

rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi48Active  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_Hsi48Inactive( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi48State   ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_Hsi48Clk     ( rcc_FreqHz_t * const clkFreq );

/*----------------------- Low Speed Clock configuration ----------------------*/

rcc_RequestState_t          Rcc_ClkSrc_Set_LseActive    ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_LseInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_LseState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Set_LseType      ( rcc_LseType_t lseType );
rcc_RequestState_t          Rcc_ClkSrc_Get_LseClk       ( rcc_FreqHz_t * const lseClk );

rcc_RequestState_t          Rcc_ClkSrc_Set_LsiActive    ( void );
rcc_RequestState_t          Rcc_ClkSrc_Set_LsiInactive  ( void );
rcc_RequestState_t          Rcc_ClkSrc_Get_LsiState     ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_ClkSrc_Get_LsiClk       ( rcc_FreqHz_t * const lsiClk );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKSRC_RCC_CLKSRC_H */
