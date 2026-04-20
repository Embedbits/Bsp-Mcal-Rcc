/**
 * \author Mr.Nobody
 * \file Rcc_ClkBus.h
 * \ingroup Rcc
 * \brief Rcc module ClkBus component functionality header file.
 */

#ifndef RCC_CLKBUS_RCC_CLKBUS_H
#define RCC_CLKBUS_RCC_CLKBUS_H

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

rcc_RequestState_t          Rcc_ClkBus_Init             ( void );
void                        Rcc_ClkBus_Deinit           ( void );
void                        Rcc_ClkBus_Task             ( void );

rcc_RequestState_t          Rcc_ClkBus_Set_SysClkSource ( rcc_SystemClkSrc_t systemClkSource );
rcc_RequestState_t          Rcc_ClkBus_Get_SysClkSource ( rcc_SystemClkSrc_t * const systemClkSource );
rcc_RequestState_t          Rcc_ClkBus_Get_SysClk       ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_ClkBus_Set_AHBDivider   ( rcc_AHB_Div_t dividerId );
rcc_RequestState_t          Rcc_ClkBus_Get_AHBDivider   ( rcc_AHB_Div_t * const dividerId );
rcc_RequestState_t          Rcc_ClkBus_Get_AHBClk       ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_ClkBus_Set_APB1Divider  ( rcc_APB1_Div_t dividerId );
rcc_RequestState_t          Rcc_ClkBus_Get_APB1Divider  ( rcc_APB1_Div_t * const dividerId );
rcc_RequestState_t          Rcc_ClkBus_Get_APB1Clk      ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_ClkBus_Set_APB2Divider  ( rcc_APB2_Div_t dividerId );
rcc_RequestState_t          Rcc_ClkBus_Get_APB2Divider  ( rcc_APB2_Div_t * const dividerId );
rcc_RequestState_t          Rcc_ClkBus_Get_APB2Clk      ( rcc_FreqHz_t * const busClk );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKBUS_RCC_CLKBUS_H */
