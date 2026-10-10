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

/** \brief List of RCC registers
 *
 * Registers of the domains are named according to STM32H72x / H74x lines
 * (D1 / D2 / D3 domains), on STM32H7A3 / H7B0 / H7B3 lines they are the
 * registers of CD / SRD domains (D1CFGR = CDCFGR1, D2CFGR = CDCFGR2,
 * D3CFGR = SRDCFGR, D1CCIPR = CDCCIPR, D2CCIP1R = CDCCIP1R,
 * D2CCIP2R = CDCCIP2R, D3CCIPR = SRDCCIPR). On STM32H7R / H7S lines:
 * D1CFGR = CDCFGR (CPU prescaler), D2CFGR = BMCFGR (bus matrix prescaler),
 * D3CFGR = APBCFGR (APB prescalers), D1CCIPR - D3CCIPR = CCIPR1 - CCIPR4,
 * PLLxDIVR = PLLxDIVR1, APB1Lxxx = APB1xxx1 and APB1Hxxx = APB1xxx2; the
 * APB3 registers are replaced by the APB5 registers, AHB5 and PLLxDIVR2
 * registers are added.
 *
 * \note Kernel clock selection registers D1CCIPR - D3CCIPR must stay
 *       consecutive - register of LL_CLKSOURCE() encoded clock sources is
 *       RCC_REG_D1CCIPR + register offset / 4.
 */
typedef enum
{
    RCC_REG_CR     = 0u, /**< RCC clock control register                           */
    RCC_REG_HSICFGR    , /**< RCC HSI calibration register                         */
    RCC_REG_CRRCR      , /**< RCC clock recovery RC register                       */
    RCC_REG_CSICFGR    , /**< RCC CSI calibration register                         */
    RCC_REG_CFGR       , /**< RCC clock configuration register                     */
    RCC_REG_D1CFGR     , /**< RCC domain 1 (CD) clock configuration register       */
    RCC_REG_D2CFGR     , /**< RCC domain 2 (CD) clock configuration register       */
    RCC_REG_D3CFGR     , /**< RCC domain 3 (SRD) clock configuration register      */
    RCC_REG_PLLCKSELR  , /**< RCC PLLs clock source selection register             */
    RCC_REG_PLLCFGR    , /**< RCC PLLs configuration register                      */
    RCC_REG_PLL1DIVR   , /**< RCC PLL1 dividers configuration register             */
    RCC_REG_PLL1FRACR  , /**< RCC PLL1 fractional divider configuration register   */
    RCC_REG_PLL2DIVR   , /**< RCC PLL2 dividers configuration register             */
    RCC_REG_PLL2FRACR  , /**< RCC PLL2 fractional divider configuration register   */
    RCC_REG_PLL3DIVR   , /**< RCC PLL3 dividers configuration register             */
    RCC_REG_PLL3FRACR  , /**< RCC PLL3 fractional divider configuration register   */
#if defined(STM32H7RS)
    RCC_REG_PLL1DIVR2  , /**< RCC PLL1 dividers configuration register 2 (S)       */
    RCC_REG_PLL2DIVR2  , /**< RCC PLL2 dividers configuration register 2 (S, T)    */
    RCC_REG_PLL3DIVR2  , /**< RCC PLL3 dividers configuration register 2 (S)       */
#endif
    RCC_REG_D1CCIPR    , /**< RCC domain 1 (CD) kernel clock configuration register         */
    RCC_REG_D2CCIP1R   , /**< RCC domain 2 (CD) kernel clock configuration register 1       */
    RCC_REG_D2CCIP2R   , /**< RCC domain 2 (CD) kernel clock configuration register 2       */
    RCC_REG_D3CCIPR    , /**< RCC domain 3 (SRD) kernel clock configuration register        */
    RCC_REG_CIER       , /**< RCC clock source interrupt enable register           */
    RCC_REG_CIFR       , /**< RCC clock source interrupt flag register             */
    RCC_REG_CICR       , /**< RCC clock source interrupt clear register            */
    RCC_REG_BDCR       , /**< RCC backup domain control register                   */
    RCC_REG_CSR        , /**< RCC clock control and status register                */
    RCC_REG_AHB3RSTR   , /**< RCC AHB3 peripheral reset register                   */
    RCC_REG_AHB1RSTR   , /**< RCC AHB1 peripheral reset register                   */
    RCC_REG_AHB2RSTR   , /**< RCC AHB2 peripheral reset register                   */
    RCC_REG_AHB4RSTR   , /**< RCC AHB4 peripheral reset register                   */
#if defined(STM32H7RS)
    RCC_REG_AHB5RSTR   , /**< RCC AHB5 peripheral reset register                   */
    RCC_REG_APB5RSTR   , /**< RCC APB5 peripheral reset register                   */
#else
    RCC_REG_APB3RSTR   , /**< RCC APB3 peripheral reset register                   */
#endif
    RCC_REG_APB1LRSTR  , /**< RCC APB1 peripheral reset low word register          */
    RCC_REG_APB1HRSTR  , /**< RCC APB1 peripheral reset high word register         */
    RCC_REG_APB2RSTR   , /**< RCC APB2 peripheral reset register                   */
    RCC_REG_APB4RSTR   , /**< RCC APB4 peripheral reset register                   */
    RCC_REG_RSR        , /**< RCC reset status register                            */
    RCC_REG_AHB3ENR    , /**< RCC AHB3 clock enable register                       */
    RCC_REG_AHB1ENR    , /**< RCC AHB1 clock enable register                       */
    RCC_REG_AHB2ENR    , /**< RCC AHB2 clock enable register                       */
    RCC_REG_AHB4ENR    , /**< RCC AHB4 clock enable register                       */
#if defined(STM32H7RS)
    RCC_REG_AHB5ENR    , /**< RCC AHB5 clock enable register                       */
    RCC_REG_APB5ENR    , /**< RCC APB5 clock enable register                       */
#else
    RCC_REG_APB3ENR    , /**< RCC APB3 clock enable register                       */
#endif
    RCC_REG_APB1LENR   , /**< RCC APB1 clock enable low word register              */
    RCC_REG_APB1HENR   , /**< RCC APB1 clock enable high word register             */
    RCC_REG_APB2ENR    , /**< RCC APB2 clock enable register                       */
    RCC_REG_APB4ENR    , /**< RCC APB4 clock enable register                       */
    RCC_REG_AHB3LPENR  , /**< RCC AHB3 sleep clock register                        */
    RCC_REG_AHB1LPENR  , /**< RCC AHB1 sleep clock register                        */
    RCC_REG_AHB2LPENR  , /**< RCC AHB2 sleep clock register                        */
    RCC_REG_AHB4LPENR  , /**< RCC AHB4 sleep clock register                        */
#if defined(STM32H7RS)
    RCC_REG_AHB5LPENR  , /**< RCC AHB5 sleep clock register                        */
    RCC_REG_APB5LPENR  , /**< RCC APB5 sleep clock register                        */
#else
    RCC_REG_APB3LPENR  , /**< RCC APB3 sleep clock register                        */
#endif
    RCC_REG_APB1LLPENR , /**< RCC APB1 sleep clock low word register               */
    RCC_REG_APB1HLPENR , /**< RCC APB1 sleep clock high word register              */
    RCC_REG_APB2LPENR  , /**< RCC APB2 sleep clock register                        */
    RCC_REG_APB4LPENR  , /**< RCC APB4 sleep clock register                        */
    RCC_REG_FLASH_ACR  , /**< Flash access control register                        */
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
