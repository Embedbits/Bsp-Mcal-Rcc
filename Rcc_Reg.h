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
    RCC_REG_CR     = 0u, /**< RCC clock control register                           */
    RCC_REG_HSICFGR    , /**< RCC HSI Clock Calibration Register,                  */
    RCC_REG_CRRCR      , /**< RCC Clock Recovery RC Register,                      */
    RCC_REG_CSICFGR    , /**< RCC CSI Clock Calibration Register,                  */
    RCC_REG_CFGR1      , /**< RCC clock configuration register 1                   */
    RCC_REG_CFGR2      , /**< RCC clock configuration register 2                   */
    RCC_REG_PLL1CFGR   , /**< RCC PLL1 Configuration Register                      */
    RCC_REG_PLL2CFGR   , /**< RCC PLL2 Configuration Register                      */
    RCC_REG_PLL3CFGR   , /**< RCC PLL3 Configuration Register                      */
    RCC_REG_PLL1DIVR   , /**< RCC PLL1 Dividers Configuration Register             */
    RCC_REG_PLL1FRACR  , /**< RCC PLL1 Fractional Divider Configuration Register   */
    RCC_REG_PLL2DIVR   , /**< RCC PLL2 Dividers Configuration Register             */
    RCC_REG_PLL2FRACR  , /**< RCC PLL2 Fractional Divider Configuration Register   */
    RCC_REG_PLL3DIVR   , /**< RCC PLL3 Dividers Configuration Register             */
    RCC_REG_PLL3FRACR  , /**< RCC PLL3 Fractional Divider Configuration Register   */
    RCC_REG_CIER       , /**< RCC Clock Interrupt Enable Register                  */
    RCC_REG_CIFR       , /**< RCC Clock Interrupt Flag Register                    */
    RCC_REG_CICR       , /**< RCC Clock Interrupt Clear Register                   */
    RCC_REG_AHB1RSTR   , /**< RCC AHB1 Peripherals Reset Register                  */
    RCC_REG_AHB2RSTR   , /**< RCC AHB2 Peripherals Reset Register                  */
    RCC_REG_AHB4RSTR   , /**< RCC AHB4 Peripherals Reset Register                  */
    RCC_REG_APB1LRSTR  , /**< RCC APB1 Peripherals reset Low Word register         */
    RCC_REG_APB1HRSTR  , /**< RCC APB1 Peripherals reset High Word register        */
    RCC_REG_APB2RSTR   , /**< RCC APB2 Peripherals Reset Register                  */
    RCC_REG_APB3RSTR   , /**< RCC APB3 Peripherals Reset Register                  */
    RCC_REG_AHB1ENR    , /**< RCC AHB1 Peripherals Clock Enable Register           */
    RCC_REG_AHB2ENR    , /**< RCC AHB2 Peripherals Clock Enable Register           */
    RCC_REG_AHB4ENR    , /**< RCC AHB4 Peripherals Clock Enable Register           */
    RCC_REG_APB1LENR   , /**< RCC APB1 Peripherals clock Enable Low Word register  */
    RCC_REG_APB1HENR   , /**< RCC APB1 Peripherals clock Enable High Word register */
    RCC_REG_APB2ENR    , /**< RCC APB2 Peripherals Clock Enable Register           */
    RCC_REG_APB3ENR    , /**< RCC APB3 Peripherals Clock Enable Register           */
    RCC_REG_AHB1LPENR  , /**< RCC AHB1 Peripheral sleep clock Register             */
    RCC_REG_AHB2LPENR  , /**< RCC AHB2 Peripheral sleep clock Register             */
    RCC_REG_AHB4LPENR  , /**< RCC AHB4 Peripherals sleep clock Register            */
    RCC_REG_APB1LLPENR , /**< RCC APB1 Peripherals sleep clock Low Word Register   */
    RCC_REG_APB1HLPENR , /**< RCC APB1 Peripherals sleep clock High Word Register  */
    RCC_REG_APB2LPENR  , /**< RCC APB2 Peripherals sleep clock Register            */
    RCC_REG_APB3LPENR  , /**< RCC APB3 Peripherals Clock Low Power Enable Register */
    RCC_REG_CCIPR1     , /**< RCC IPs Clocks Configuration Register 1              */
    RCC_REG_CCIPR2     , /**< RCC IPs Clocks Configuration Register 2              */
    RCC_REG_CCIPR3     , /**< RCC IPs Clocks Configuration Register 3              */
    RCC_REG_CCIPR4     , /**< RCC IPs Clocks Configuration Register 4              */
    RCC_REG_CCIPR5     , /**< RCC IPs Clocks Configuration Register 5              */
    RCC_REG_BDCR       , /**< RCC VSW Backup Domain & V33 Domain Control Register  */
    RCC_REG_RSR        , /**< RCC Reset status Register                            */
    RCC_REG_SECCFGR    , /**< RCC Secure mode configuration register               */
    RCC_REG_PRIVCFGR   , /**< RCC Privilege configuration register                 */
    RCC_REG_FLASH_ACR  , /**< Flash configuration register                         */
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
