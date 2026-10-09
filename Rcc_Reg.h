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
    RCC_REG_CR         = 0u, /**< RCC clock control register                                     */
    RCC_REG_ICSCR1    ,      /**< RCC internal clock sources calibration register 1              */
    RCC_REG_ICSCR2    ,      /**< RCC internal clock sources calibration register 2              */
    RCC_REG_ICSCR3    ,      /**< RCC internal clock sources calibration register 3              */
    RCC_REG_CRRCR     ,      /**< RCC clock recovery RC register                                 */
    RCC_REG_CFGR1     ,      /**< RCC clock configuration register 1                             */
    RCC_REG_CFGR2     ,      /**< RCC clock configuration register 2                             */
    RCC_REG_CFGR3     ,      /**< RCC clock configuration register 3                             */
    RCC_REG_PLL1CFGR  ,      /**< RCC PLL1 configuration register                                */
    RCC_REG_PLL2CFGR  ,      /**< RCC PLL2 configuration register                                */
    RCC_REG_PLL3CFGR  ,      /**< RCC PLL3 configuration register                                */
    RCC_REG_PLL1DIVR  ,      /**< RCC PLL1 dividers register                                     */
    RCC_REG_PLL1FRACR ,      /**< RCC PLL1 fractional divider register                           */
    RCC_REG_PLL2DIVR  ,      /**< RCC PLL2 dividers register                                     */
    RCC_REG_PLL2FRACR ,      /**< RCC PLL2 fractional divider register                           */
    RCC_REG_PLL3DIVR  ,      /**< RCC PLL3 dividers register                                     */
    RCC_REG_PLL3FRACR ,      /**< RCC PLL3 fractional divider register                           */
    RCC_REG_CIER      ,      /**< RCC clock interrupt enable register                            */
    RCC_REG_CIFR      ,      /**< RCC clock interrupt flag register                              */
    RCC_REG_CICR      ,      /**< RCC clock interrupt clear register                             */
    RCC_REG_AHB1RSTR  ,      /**< RCC AHB1 peripheral reset register                             */
    RCC_REG_AHB2RSTR1 ,      /**< RCC AHB2 peripheral reset register 1                           */
    RCC_REG_AHB2RSTR2 ,      /**< RCC AHB2 peripheral reset register 2                           */
    RCC_REG_AHB3RSTR  ,      /**< RCC AHB3 peripheral reset register                             */
    RCC_REG_APB1RSTR1 ,      /**< RCC APB1 peripheral reset register 1                           */
    RCC_REG_APB1RSTR2 ,      /**< RCC APB1 peripheral reset register 2                           */
    RCC_REG_APB2RSTR  ,      /**< RCC APB2 peripheral reset register                             */
    RCC_REG_APB3RSTR  ,      /**< RCC APB3 peripheral reset register                             */
    RCC_REG_AHB1ENR   ,      /**< RCC AHB1 peripheral clock enable register                      */
    RCC_REG_AHB2ENR1  ,      /**< RCC AHB2 peripheral clock enable register 1                    */
    RCC_REG_AHB2ENR2  ,      /**< RCC AHB2 peripheral clock enable register 2                    */
    RCC_REG_AHB3ENR   ,      /**< RCC AHB3 peripheral clock enable register                      */
    RCC_REG_APB1ENR1  ,      /**< RCC APB1 peripheral clock enable register 1                    */
    RCC_REG_APB1ENR2  ,      /**< RCC APB1 peripheral clock enable register 2                    */
    RCC_REG_APB2ENR   ,      /**< RCC APB2 peripheral clock enable register                      */
    RCC_REG_APB3ENR   ,      /**< RCC APB3 peripheral clock enable register                      */
    RCC_REG_AHB1SMENR ,      /**< RCC AHB1 Sleep / Stop mode clock enable register               */
    RCC_REG_AHB2SMENR1,      /**< RCC AHB2 Sleep / Stop mode clock enable register 1             */
    RCC_REG_AHB2SMENR2,      /**< RCC AHB2 Sleep / Stop mode clock enable register 2             */
    RCC_REG_AHB3SMENR ,      /**< RCC AHB3 Sleep / Stop mode clock enable register               */
    RCC_REG_APB1SMENR1,      /**< RCC APB1 Sleep / Stop mode clock enable register 1             */
    RCC_REG_APB1SMENR2,      /**< RCC APB1 Sleep / Stop mode clock enable register 2             */
    RCC_REG_APB2SMENR ,      /**< RCC APB2 Sleep / Stop mode clock enable register               */
    RCC_REG_APB3SMENR ,      /**< RCC APB3 Sleep / Stop mode clock enable register               */
    RCC_REG_SRDAMR    ,      /**< RCC SmartRun domain peripheral autonomous mode register        */
    RCC_REG_CCIPR1    ,      /**< RCC peripherals independent clock configuration register 1     */
    RCC_REG_CCIPR2    ,      /**< RCC peripherals independent clock configuration register 2     */
    RCC_REG_CCIPR3    ,      /**< RCC peripherals independent clock configuration register 3     */
    RCC_REG_BDCR      ,      /**< RCC backup domain control register                             */
    RCC_REG_CSR       ,      /**< RCC control / status register                                  */
    RCC_REG_SECCFGR   ,      /**< RCC secure configuration register                              */
    RCC_REG_PRIVCFGR  ,      /**< RCC privilege configuration register                           */
    RCC_REG_FLASH_ACR ,      /**< FLASH access control register                                  */
    RCC_REG_CNT              /**< Count of available RCC registers                               */
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
