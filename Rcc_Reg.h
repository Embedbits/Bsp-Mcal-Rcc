/**
 * \author Mr.Nobody
 * \file Rcc.h
 * \ingroup Rcc
 * \brief Rcc module common functionality header file.
 *
 * This component contains functionality for register operations.
 *
 */

#ifndef RCC_REG_RCC_REG_H
#define RCC_REG_RCC_REG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Rcc_Types.h"                      /* Module types definition        */
/* ============================= TYPEDEFS =================================== */

/** \brief List of RCC registers */
typedef enum
{
    RCC_REG_CR = 0u    , /**< RCC clock control register                                       */
    RCC_REG_ICSCR      , /**< RCC internal clock sources calibration register 1                */
    RCC_REG_CRRCR      , /**< RCC Clock Recovery RC Register                                   */
    RCC_REG_CFGR       , /**< RCC clock configuration register                                 */
    RCC_REG_PLLCFGR    , /**< PLL Configuration Register                                       */
    RCC_REG_CIER       , /**< Clock Interrupt Enable Register                                  */
    RCC_REG_CIFR       , /**< Clock Interrupt Flag Register                                    */
    RCC_REG_CICR       , /**< Clock Interrupt Clear Register                                   */
    RCC_REG_AHB1RSTR   , /**< AHB1 Peripherals Reset Register                                  */
    RCC_REG_AHB2RSTR   , /**< AHB2 Peripherals Reset Register                                  */
    RCC_REG_AHB3RSTR   , /**< AHB3 Peripherals Reset Register                                  */
    RCC_REG_APB1RSTR1  , /**< APB1 Peripherals Reset Register                                  */
    RCC_REG_APB1RSTR2  , /**< APB1 Peripherals Reset Register                                  */
    RCC_REG_APB2RSTR   , /**< APB2 Peripherals Reset Register                                  */
    RCC_REG_AHB1ENR    , /**< AHB1 Peripherals Clock Enable Register                           */
    RCC_REG_AHB2ENR    , /**< AHB2 Peripherals Clock Enable Register                           */
    RCC_REG_AHB3ENR    , /**< AHB3 Peripherals Clock Enable Register                           */
    RCC_REG_APB1ENR1   , /**< APB1 Peripherals Clock Enable Register 1                         */
    RCC_REG_APB1ENR2   , /**< APB1 Peripherals Clock Enable Register 2                         */
    RCC_REG_APB2ENR    , /**< APB2 Peripherals Clock Enable Register                           */
    RCC_REG_AHB1SMENR  , /**< AHB1 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    RCC_REG_AHB2SMENR  , /**< AHB2 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    RCC_REG_AHB3SMENR  , /**< AHB3 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    RCC_REG_APB1SMENR1 , /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    RCC_REG_APB1SMENR2 , /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    RCC_REG_APB2SMENR  , /**< APB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    RCC_REG_CCIPR1     , /**< IPs Clocks Configuration Register 1                              */
    RCC_REG_CCIPR2     , /**< IPs Clocks Configuration Register 2                              */
    RCC_REG_BDCR       , /**< Backup Domain Control Register                                   */
    RCC_REG_CSR        , /**< V33 Clock Control & Status Register                              */
    RCC_REG_FLASH_ACR  , /**< Flash configuration register                                     */
    RCC_REG_CNT          /**< Count of available RCC registers                                 */
}   rcc_RegId_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

void                        Rcc_Set_RegBit              ( rcc_RegId_t regId, uint32_t bitMask );
void                        Rcc_Reset_RegBit            ( rcc_RegId_t regId, uint32_t bitMask );
uint32_t                    Rcc_Get_RegBit              ( rcc_RegId_t regId, uint32_t bitMask );

void                        Rcc_Set_RegVal              ( rcc_RegId_t regId, uint32_t regMask, uint32_t regValue );
uint32_t                    Rcc_Get_RegVal              ( rcc_RegId_t regId, uint32_t regMask );

#ifdef __cplusplus
}
#endif

#endif /* RCC_REG_RCC_REG_H */
