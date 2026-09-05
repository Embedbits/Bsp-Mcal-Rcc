/**
 * \author Mr.Nobody
 * \file Rcc_Pll.h
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality header file.
 */

#ifndef RCC_PLL_RCC_PLL_H
#define RCC_PLL_RCC_PLL_H

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

rcc_RequestState_t          Rcc_Pll_Init                ( void );
rcc_RequestState_t          Rcc_Pll_Deinit              ( void );
void                        Rcc_Pll_Task                ( void );

rcc_RequestState_t          Rcc_Pll_Set_Config          ( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct );
rcc_RequestState_t          Rcc_Pll_Get_InternalClk     ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk );

rcc_RequestState_t          Rcc_Pll_Set_Active          ( rcc_PllId_t pllId );
rcc_RequestState_t          Rcc_Pll_Set_Inactive        ( rcc_PllId_t pllId );
rcc_RequestState_t          Rcc_Pll_Get_State           ( rcc_PllId_t pllId, rcc_FunctionState_t * const state );

rcc_RequestState_t          Rcc_Pll_Set_Source          ( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource );
rcc_RequestState_t          Rcc_Pll_Get_Source          ( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource );

rcc_RequestState_t          Rcc_Pll_Set_OutP            ( rcc_PllId_t pllId, rcc_PllPDivider_t divider );
rcc_RequestState_t          Rcc_Pll_Get_Clk_OutP        ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk );

rcc_RequestState_t          Rcc_Pll_Set_OutQ            ( rcc_PllId_t pllId, rcc_PllQDivider_t divider );
rcc_RequestState_t          Rcc_Pll_Get_Clk_OutQ        ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk );

rcc_RequestState_t          Rcc_Pll_Set_OutR            ( rcc_PllId_t pllId, rcc_PllRDivider_t divider );
rcc_RequestState_t          Rcc_Pll_Get_Clk_OutR        ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk );

/*----------------------- Low Speed Clock configuration ----------------------*/

rcc_RequestState_t          Rcc_Pll_Set_RtcClkSource    ( rcc_Rtc_ClkSource_t clkSource );
rcc_RequestState_t          Rcc_Pll_Get_RtcClkSource    ( rcc_Rtc_ClkSource_t * const clkSource );


#ifdef __cplusplus
}
#endif

#endif /* RCC_PLL_RCC_PLL_H */
