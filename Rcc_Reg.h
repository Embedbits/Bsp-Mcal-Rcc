/**
 * \author Mr.Nobody
 * \file Rcc_Reg.h
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

/** \brief List of RCC registers (and registers of other peripherals used by
 *         the clock configuration sequence) */
typedef enum
{
    RCC_REG_CR     = 0u, /**< RCC clock control register                           */
    RCC_REG_PLLCFGR    , /**< RCC PLL configuration register                       */
    RCC_REG_CFGR       , /**< RCC clock configuration register                     */
    RCC_REG_CIR        , /**< RCC clock interrupt register                         */
    RCC_REG_AHB1RSTR   , /**< RCC AHB1 peripheral reset register                   */
    RCC_REG_AHB2RSTR   , /**< RCC AHB2 peripheral reset register                   */
    RCC_REG_AHB3RSTR   , /**< RCC AHB3 peripheral reset register                   */
    RCC_REG_APB1RSTR   , /**< RCC APB1 peripheral reset register                   */
    RCC_REG_APB2RSTR   , /**< RCC APB2 peripheral reset register                   */
    RCC_REG_AHB1ENR    , /**< RCC AHB1 peripheral clock enable register            */
    RCC_REG_AHB2ENR    , /**< RCC AHB2 peripheral clock enable register            */
    RCC_REG_AHB3ENR    , /**< RCC AHB3 peripheral clock enable register            */
    RCC_REG_APB1ENR    , /**< RCC APB1 peripheral clock enable register            */
    RCC_REG_APB2ENR    , /**< RCC APB2 peripheral clock enable register            */
    RCC_REG_AHB1LPENR  , /**< RCC AHB1 peripheral clock enable in low power mode   */
    RCC_REG_AHB2LPENR  , /**< RCC AHB2 peripheral clock enable in low power mode   */
    RCC_REG_AHB3LPENR  , /**< RCC AHB3 peripheral clock enable in low power mode   */
    RCC_REG_APB1LPENR  , /**< RCC APB1 peripheral clock enable in low power mode   */
    RCC_REG_APB2LPENR  , /**< RCC APB2 peripheral clock enable in low power mode   */
    RCC_REG_BDCR       , /**< RCC backup domain control register                   */
    RCC_REG_CSR        , /**< RCC clock control & status register                  */
    RCC_REG_SSCGR      , /**< RCC spread spectrum clock generation register        */
    RCC_REG_PLLI2SCFGR , /**< RCC PLLI2S configuration register                    */
    RCC_REG_PLLSAICFGR , /**< RCC PLLSAI configuration register                    */
    RCC_REG_DCKCFGR1   , /**< RCC dedicated clocks configuration register 1        */
    RCC_REG_DCKCFGR2   , /**< RCC dedicated clocks configuration register 2        */
    RCC_REG_FLASH_ACR  , /**< Flash access control register                        */
    RCC_REG_PWR_CR1    , /**< PWR power control register 1                         */
    RCC_REG_PWR_CSR1   , /**< PWR power control/status register 1                  */
    RCC_REG_CNT          /**< Count of available RCC registers                     */
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
