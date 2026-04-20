/**
 * \author Mr.Nobody
 * \file Rcc_Port.h
 * \ingroup Rcc
 * \brief Rcc module public functionality
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
rcc_RequestState_t          Rcc_Get_PllState            ( rcc_PllId_t pllId, rcc_FunctionState_t * const retState );

rcc_RequestState_t          Rcc_Set_PllsSource          ( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource );
rcc_RequestState_t          Rcc_Get_PllsSource          ( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource );

rcc_RequestState_t          Rcc_Get_PllClk_OutP         ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk );
rcc_RequestState_t          Rcc_Get_PllClk_OutQ         ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk );
rcc_RequestState_t          Rcc_Get_PllClk_OutR         ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk );

/*------------------------ Clock sources configuration -----------------------*/

rcc_RequestState_t          Rcc_Set_HseActive           ( rcc_HseType_t hseType );
rcc_RequestState_t          Rcc_Set_HseInactive         ( void );
rcc_RequestState_t          Rcc_Get_HseState            ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_Set_HseClk              ( rcc_FreqHz_t hseFreq );
rcc_RequestState_t          Rcc_Get_HseClk              ( rcc_FreqHz_t * const hseFreq );

rcc_RequestState_t          Rcc_Set_Hsi16Active         ( void );
rcc_RequestState_t          Rcc_Set_Hsi16Inactive       ( void );
rcc_RequestState_t          Rcc_Get_Hsi16State          ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_Get_Hsi16Clk            ( rcc_FreqHz_t * const hsiClk );

rcc_RequestState_t          Rcc_Set_Hsi48Active         ( void );
rcc_RequestState_t          Rcc_Set_Hsi48Inactive       ( void );
rcc_RequestState_t          Rcc_Get_Hsi48State          ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_Get_Hsi48Clk            ( rcc_FreqHz_t * const hsiClk );

rcc_RequestState_t          Rcc_Set_MsisActive          ( rcc_MsisOutId_t msisId, rcc_MsisClkFreq_t clkFreq );
rcc_RequestState_t          Rcc_Set_MsisInactive        ( rcc_MsisOutId_t msisId );
rcc_RequestState_t          Rcc_Get_MsisState           ( rcc_MsisOutId_t msisId, rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_Get_MsisClk             ( rcc_MsisOutId_t msisId, rcc_FreqHz_t * const msiClk );

/*----------------------- Low Speed Clock configuration ----------------------*/

rcc_RequestState_t          Rcc_Set_LseActive           ( void );
rcc_RequestState_t          Rcc_Set_LseInactive         ( void );
rcc_RequestState_t          Rcc_Get_LseState            ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_Get_LseClk              ( rcc_FreqHz_t * const lseClk );

rcc_RequestState_t          Rcc_Set_LsiActive           ( void );
rcc_RequestState_t          Rcc_Set_LsiInactive         ( void );
rcc_RequestState_t          Rcc_Get_LsiState            ( rcc_FunctionState_t * const retState );
rcc_RequestState_t          Rcc_Get_LsiClk              ( rcc_FreqHz_t * const lsiClk );

rcc_RequestState_t          Rcc_Set_RtcClkSource        ( rcc_Rtc_ClkSource_t clkSource );
rcc_RequestState_t          Rcc_Get_RtcClkSource        ( rcc_Rtc_ClkSource_t * const clkSource );

/*------------------------- Clock buses configuration ------------------------*/

rcc_RequestState_t          Rcc_Set_SysClkSource        ( rcc_SystemClkSrc_t systemClkSource );
rcc_RequestState_t          Rcc_Get_SysClkSource        ( rcc_SystemClkSrc_t * const systemClkSource );
rcc_RequestState_t          Rcc_Get_SysClk              ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_Set_AHBDivider          ( rcc_AHB_Div_t dividerId );
rcc_RequestState_t          Rcc_Get_AHBDivider          ( rcc_AHB_Div_t * const dividerId );
rcc_RequestState_t          Rcc_Get_AHBClk              ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_Set_APB1Divider         ( rcc_APB1_Div_t dividerId );
rcc_RequestState_t          Rcc_Get_APB1Divider         ( rcc_APB1_Div_t * const dividerId );
rcc_RequestState_t          Rcc_Get_APB1Clk             ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_Set_APB2Divider         ( rcc_APB2_Div_t dividerId );
rcc_RequestState_t          Rcc_Get_APB2Divider         ( rcc_APB2_Div_t * const dividerId );
rcc_RequestState_t          Rcc_Get_APB2Clk             ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_Set_APB3Divider         ( rcc_APB3_Div_t dividerId );
rcc_RequestState_t          Rcc_Get_APB3Divider         ( rcc_APB3_Div_t * const dividerId );
rcc_RequestState_t          Rcc_Get_APB3Clk             ( rcc_FreqHz_t * const busClk );

rcc_RequestState_t          Rcc_Set_PwrRange            ( rcc_ConfigStruct_t * const clockConfig );

rcc_RequestState_t          Rcc_Set_FlashLatency        ( rcc_ConfigStruct_t * const clockConfig );

rcc_RequestState_t          Rcc_Set_FlashPrefetchActive ( void );
rcc_RequestState_t          Rcc_Set_FlashPrefetchInactive( void );

rcc_RequestState_t          Rcc_Set_SysTickInterval     ( rcc_Time_ms_t sysTickInterval );
rcc_RequestState_t          Rcc_Get_SysTickInterval     ( rcc_Time_ms_t * const sysTickInterval );

/*----------------------- Clock outputs configuration ------------------------*/

rcc_RequestState_t          Rcc_Set_McoClockSource      ( rcc_Mco_ClkSource_t clkSource );
rcc_RequestState_t          Rcc_Get_McoClockSource      ( rcc_Mco_ClkSource_t * const clkSource );

rcc_RequestState_t          Rcc_Set_McoClockDivider     ( rcc_Mco_Divider_t clkDivider );
rcc_RequestState_t          Rcc_Get_McoClockDivider     ( rcc_Mco_Divider_t * const clkDivider );


#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_PORT_H */

