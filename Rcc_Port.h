/**
 * \author Mr.Nobody
 * \file Rcc_Port.h
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module public functionality
 *
 * This file contains all available public functionality, any other files shall 
 * not used outside of the module.
 *
 */

#ifndef RCC_RCC_PORT_H
#define RCC_RCC_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Rcc_Types.h"                      /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS =============================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */

rcc_ModuleVersion_t         Rcc_Get_ModuleVersion       ( void );

rcc_RequestState_t          Rcc_Init                    ( rcc_ConfigStruct_t * const clockConfig );
void                        Rcc_Deinit                  ( rcc_ConfigStruct_t * const clockConfig );
void                        Rcc_Task                    ( void );

rcc_RequestState_t          Rcc_Get_DefaultConfig       ( rcc_ConfigStruct_t * const clockConfig );

/*---------------------- Peripheral clock configuration ----------------------*/

rcc_RequestState_t          Rcc_Set_PeriphActive        ( rcc_PeriphId_t periphId );
rcc_RequestState_t          Rcc_Set_PeriphInactive      ( rcc_PeriphId_t periphId );
rcc_RequestState_t          Rcc_Get_PeriphState         ( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState );

rcc_RequestState_t          Rcc_Get_PeriphClk           ( rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk );

rcc_RequestState_t          Rcc_Set_ResetActive         ( rcc_PeriphId_t periphId );
rcc_RequestState_t          Rcc_Set_ResetInactive       ( rcc_PeriphId_t periphId );
rcc_RequestState_t          Rcc_Get_ResetState          ( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState );

/*------------------------- Power mode configuration -------------------------*/

rcc_RequestState_t          Rcc_Set_SleepActive         ( rcc_PeriphId_t periphId );
rcc_RequestState_t          Rcc_Set_SleepInactive       ( rcc_PeriphId_t periphId );
rcc_RequestState_t          Rcc_Get_SleepState          ( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState );

/*---------------------------- Pll's configuration ---------------------------*/

rcc_RequestState_t          Rcc_Set_PllConfig           ( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct );
rcc_RequestState_t          Rcc_Get_PllInternalClk      ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk );

rcc_RequestState_t          Rcc_Set_PllActive           ( rcc_PllId_t pllId );
rcc_RequestState_t          Rcc_Set_PllInactive         ( rcc_PllId_t pllId );
rcc_RequestState_t          Rcc_Get_PllState            ( rcc_PllId_t pllId, rcc_FunctionState_t * const pllRetState );

rcc_RequestState_t          Rcc_Set_PllsSource          ( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource );
rcc_RequestState_t          Rcc_Get_PllsSource          ( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource );

rcc_RequestState_t          Rcc_Get_PllClk_OutP         ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk );
rcc_RequestState_t          Rcc_Get_PllClk_OutQ         ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk );
rcc_RequestState_t          Rcc_Get_PllClk_OutR         ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk );

/*------------------------ Clock sources configuration -----------------------*/

rcc_RequestState_t          Rcc_Set_OscActive           ( rcc_OscId_t oscId );
rcc_RequestState_t          Rcc_Set_OscInactive         ( rcc_OscId_t oscId );
rcc_RequestState_t          Rcc_Get_OscState            ( rcc_OscId_t oscId, rcc_FunctionState_t * const oscRetState );

rcc_RequestState_t          Rcc_Set_OscDiv              ( rcc_OscId_t oscId, rcc_OscDiv_t oscDiv );
rcc_RequestState_t          Rcc_Get_OscDiv              ( rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv );

/*----------------------- Low Speed Clock configuration ----------------------*/

rcc_RequestState_t          Rcc_Set_RtcClkSource        ( rcc_Rtc_ClkSource_t clkSource );
rcc_RequestState_t          Rcc_Get_RtcClkSource        ( rcc_Rtc_ClkSource_t * const clkSource );

/*------------------------- Clock buses configuration ------------------------*/

rcc_RequestState_t          Rcc_Set_ClkBusDivider       ( rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider );
rcc_RequestState_t          Rcc_Get_ClkBusDivider       ( rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider );

rcc_RequestState_t          Rcc_Get_ClkFreq             ( rcc_ClkSrcId_t clkId, rcc_FreqHz_t * const clkBusFreq );

rcc_RequestState_t          Rcc_Set_PwrRange            ( rcc_PwrVoltageScale_t voltageScale );

rcc_RequestState_t          Rcc_Get_ExpectedFlashLatency( rcc_FreqHz_t hclkClk, rcc_FlashLatency_t * const retLatency );
rcc_RequestState_t          Rcc_Set_FlashLatency        ( rcc_FlashLatency_t flashLatency );

rcc_RequestState_t          Rcc_Set_FlashPrefetchActive ( void );
rcc_RequestState_t          Rcc_Set_FlashPrefetchInactive( void );

rcc_RequestState_t          Rcc_Set_SysTickInterval     ( rcc_Time_ms_t sysTickInterval );
rcc_RequestState_t          Rcc_Get_SysTickInterval     ( rcc_Time_ms_t * const sysTickInterval );

/*----------------------- Clock outputs configuration ------------------------*/

rcc_RequestState_t          Rcc_Set_ClkOutSource        ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource );
rcc_RequestState_t          Rcc_Get_ClkOutSource        ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource );

rcc_RequestState_t          Rcc_Set_ClkOutDivider       ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider );
rcc_RequestState_t          Rcc_Get_ClkOutDivider       ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider );

rcc_RequestState_t          Rcc_Set_ClkOutPin           ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Pin_t clkOutPin );
#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_PORT_H */

