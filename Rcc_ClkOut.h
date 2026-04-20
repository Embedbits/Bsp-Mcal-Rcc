/**
 * \author Mr.Nobody
 * \file Rcc_ClkOut.h
 * \ingroup Rcc
 * \brief Rcc module ClkOut component functionality header file.
 */

#ifndef RCC_CLKOUT_RCC_CLKOUT_H
#define RCC_CLKOUT_RCC_CLKOUT_H

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

rcc_RequestState_t          Rcc_ClkOut_Set_ClockSource  ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource );
rcc_RequestState_t          Rcc_ClkOut_Get_ClockSource  ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource );

rcc_RequestState_t          Rcc_ClkOut_Set_ClockDivider ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider );
rcc_RequestState_t          Rcc_ClkOut_Get_ClockDivider ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider );

rcc_RequestState_t          Rcc_ClkOut_Set_ClockPin     ( rcc_ClkOut_Id_t outId, rcc_ClkOut_Pin_t pinId );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKOUT_RCC_CLKOUT_H */
