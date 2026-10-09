/**
 * \author Mr.Nobody
 * \file Rcc_Reg.h
 * \ingroup Rcc
 * \brief Rcc module Reg component functionality.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_Reg.h"                        /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
/* ============================== TYPEDEFS ================================== */

/** \brief Structure type used to store physical address of RCC registers.
 * This array is used to reduce size of configuration array. With this style
 * the registers are referenced through enumeration.
 *
 */
typedef struct
{
    rcc_RegId_t         RegId;   /**< Peripheral register ID      */
    volatile uint32_t * RegAddr; /**< Peripheral register address */
}   rcc_RegList_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief RCC registers configuration structure
 *
 * This structure is used to store addresses of RCC registers.
 * It is used to reduce code size while referring to RCC registers.
 */
const rcc_RegList_t                     rcc_RegList[] =
{
    { .RegId = RCC_REG_CR        , .RegAddr = &(RCC->CR)       }, /**< RCC clock control register                                     */
    { .RegId = RCC_REG_ICSCR1    , .RegAddr = &(RCC->ICSCR1)   }, /**< RCC internal clock sources calibration register 1              */
    { .RegId = RCC_REG_ICSCR2    , .RegAddr = &(RCC->ICSCR2)   }, /**< RCC internal clock sources calibration register 2              */
    { .RegId = RCC_REG_ICSCR3    , .RegAddr = &(RCC->ICSCR3)   }, /**< RCC internal clock sources calibration register 3              */
    { .RegId = RCC_REG_CRRCR     , .RegAddr = &(RCC->CRRCR)    }, /**< RCC clock recovery RC register                                 */
    { .RegId = RCC_REG_CFGR1     , .RegAddr = &(RCC->CFGR1)    }, /**< RCC clock configuration register 1                             */
    { .RegId = RCC_REG_CFGR2     , .RegAddr = &(RCC->CFGR2)    }, /**< RCC clock configuration register 2                             */
    { .RegId = RCC_REG_CFGR3     , .RegAddr = &(RCC->CFGR3)    }, /**< RCC clock configuration register 3                             */
    { .RegId = RCC_REG_PLL1CFGR  , .RegAddr = &(RCC->PLL1CFGR) }, /**< RCC PLL1 configuration register                                */
    { .RegId = RCC_REG_PLL2CFGR  , .RegAddr = &(RCC->PLL2CFGR) }, /**< RCC PLL2 configuration register                                */
    { .RegId = RCC_REG_PLL3CFGR  , .RegAddr = &(RCC->PLL3CFGR) }, /**< RCC PLL3 configuration register                                */
    { .RegId = RCC_REG_PLL1DIVR  , .RegAddr = &(RCC->PLL1DIVR) }, /**< RCC PLL1 dividers register                                     */
    { .RegId = RCC_REG_PLL1FRACR , .RegAddr = &(RCC->PLL1FRACR) }, /**< RCC PLL1 fractional divider register                           */
    { .RegId = RCC_REG_PLL2DIVR  , .RegAddr = &(RCC->PLL2DIVR) }, /**< RCC PLL2 dividers register                                     */
    { .RegId = RCC_REG_PLL2FRACR , .RegAddr = &(RCC->PLL2FRACR) }, /**< RCC PLL2 fractional divider register                           */
    { .RegId = RCC_REG_PLL3DIVR  , .RegAddr = &(RCC->PLL3DIVR) }, /**< RCC PLL3 dividers register                                     */
    { .RegId = RCC_REG_PLL3FRACR , .RegAddr = &(RCC->PLL3FRACR) }, /**< RCC PLL3 fractional divider register                           */
    { .RegId = RCC_REG_CIER      , .RegAddr = &(RCC->CIER)     }, /**< RCC clock interrupt enable register                            */
    { .RegId = RCC_REG_CIFR      , .RegAddr = &(RCC->CIFR)     }, /**< RCC clock interrupt flag register                              */
    { .RegId = RCC_REG_CICR      , .RegAddr = &(RCC->CICR)     }, /**< RCC clock interrupt clear register                             */
    { .RegId = RCC_REG_AHB1RSTR  , .RegAddr = &(RCC->AHB1RSTR) }, /**< RCC AHB1 peripheral reset register                             */
    { .RegId = RCC_REG_AHB2RSTR1 , .RegAddr = &(RCC->AHB2RSTR1) }, /**< RCC AHB2 peripheral reset register 1                           */
    { .RegId = RCC_REG_AHB2RSTR2 , .RegAddr = &(RCC->AHB2RSTR2) }, /**< RCC AHB2 peripheral reset register 2                           */
    { .RegId = RCC_REG_AHB3RSTR  , .RegAddr = &(RCC->AHB3RSTR) }, /**< RCC AHB3 peripheral reset register                             */
    { .RegId = RCC_REG_APB1RSTR1 , .RegAddr = &(RCC->APB1RSTR1) }, /**< RCC APB1 peripheral reset register 1                           */
    { .RegId = RCC_REG_APB1RSTR2 , .RegAddr = &(RCC->APB1RSTR2) }, /**< RCC APB1 peripheral reset register 2                           */
    { .RegId = RCC_REG_APB2RSTR  , .RegAddr = &(RCC->APB2RSTR) }, /**< RCC APB2 peripheral reset register                             */
    { .RegId = RCC_REG_APB3RSTR  , .RegAddr = &(RCC->APB3RSTR) }, /**< RCC APB3 peripheral reset register                             */
    { .RegId = RCC_REG_AHB1ENR   , .RegAddr = &(RCC->AHB1ENR)  }, /**< RCC AHB1 peripheral clock enable register                      */
    { .RegId = RCC_REG_AHB2ENR1  , .RegAddr = &(RCC->AHB2ENR1) }, /**< RCC AHB2 peripheral clock enable register 1                    */
    { .RegId = RCC_REG_AHB2ENR2  , .RegAddr = &(RCC->AHB2ENR2) }, /**< RCC AHB2 peripheral clock enable register 2                    */
    { .RegId = RCC_REG_AHB3ENR   , .RegAddr = &(RCC->AHB3ENR)  }, /**< RCC AHB3 peripheral clock enable register                      */
    { .RegId = RCC_REG_APB1ENR1  , .RegAddr = &(RCC->APB1ENR1) }, /**< RCC APB1 peripheral clock enable register 1                    */
    { .RegId = RCC_REG_APB1ENR2  , .RegAddr = &(RCC->APB1ENR2) }, /**< RCC APB1 peripheral clock enable register 2                    */
    { .RegId = RCC_REG_APB2ENR   , .RegAddr = &(RCC->APB2ENR)  }, /**< RCC APB2 peripheral clock enable register                      */
    { .RegId = RCC_REG_APB3ENR   , .RegAddr = &(RCC->APB3ENR)  }, /**< RCC APB3 peripheral clock enable register                      */
    { .RegId = RCC_REG_AHB1SMENR , .RegAddr = &(RCC->AHB1SMENR) }, /**< RCC AHB1 Sleep / Stop mode clock enable register               */
    { .RegId = RCC_REG_AHB2SMENR1, .RegAddr = &(RCC->AHB2SMENR1) }, /**< RCC AHB2 Sleep / Stop mode clock enable register 1             */
    { .RegId = RCC_REG_AHB2SMENR2, .RegAddr = &(RCC->AHB2SMENR2) }, /**< RCC AHB2 Sleep / Stop mode clock enable register 2             */
    { .RegId = RCC_REG_AHB3SMENR , .RegAddr = &(RCC->AHB3SMENR) }, /**< RCC AHB3 Sleep / Stop mode clock enable register               */
    { .RegId = RCC_REG_APB1SMENR1, .RegAddr = &(RCC->APB1SMENR1) }, /**< RCC APB1 Sleep / Stop mode clock enable register 1             */
    { .RegId = RCC_REG_APB1SMENR2, .RegAddr = &(RCC->APB1SMENR2) }, /**< RCC APB1 Sleep / Stop mode clock enable register 2             */
    { .RegId = RCC_REG_APB2SMENR , .RegAddr = &(RCC->APB2SMENR) }, /**< RCC APB2 Sleep / Stop mode clock enable register               */
    { .RegId = RCC_REG_APB3SMENR , .RegAddr = &(RCC->APB3SMENR) }, /**< RCC APB3 Sleep / Stop mode clock enable register               */
    { .RegId = RCC_REG_SRDAMR    , .RegAddr = &(RCC->SRDAMR)   }, /**< RCC SmartRun domain peripheral autonomous mode register        */
    { .RegId = RCC_REG_CCIPR1    , .RegAddr = &(RCC->CCIPR1)   }, /**< RCC peripherals independent clock configuration register 1     */
    { .RegId = RCC_REG_CCIPR2    , .RegAddr = &(RCC->CCIPR2)   }, /**< RCC peripherals independent clock configuration register 2     */
    { .RegId = RCC_REG_CCIPR3    , .RegAddr = &(RCC->CCIPR3)   }, /**< RCC peripherals independent clock configuration register 3     */
    { .RegId = RCC_REG_BDCR      , .RegAddr = &(RCC->BDCR)     }, /**< RCC backup domain control register                             */
    { .RegId = RCC_REG_CSR       , .RegAddr = &(RCC->CSR)      }, /**< RCC control / status register                                  */
    { .RegId = RCC_REG_SECCFGR   , .RegAddr = &(RCC->SECCFGR)  }, /**< RCC secure configuration register                              */
    { .RegId = RCC_REG_PRIVCFGR  , .RegAddr = &(RCC->PRIVCFGR) }, /**< RCC privilege configuration register                           */
    { .RegId = RCC_REG_FLASH_ACR , .RegAddr = &(FLASH->ACR)    }, /**< FLASH access control register                                  */
};

_Static_assert( (sizeof(rcc_RegList) / sizeof(rcc_RegList_t)) == RCC_REG_CNT, "Rcc_Reg: rcc_RegList has incorrect size." );

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes module Rcc_Reg
 */
void Rcc_Reg_Init( void )
{
    return;
}


/**
 * \brief De-initializes module Rcc_Reg
 */
void Rcc_Reg_Deinit( void )
{
    return;
}


/**
 * \brief Main task of module Rcc_Reg
 */
void Rcc_Reg_Task( void )
{
    return;
}


/**
 * \brief Sets the register bits given by mask.
 *
 * \param regId   [in]: RCC register ID, value from \ref rcc_RegId_t. Invalid ID is ignored.
 * \param bitMask [in]: Mask of the bits to set
 */
void Rcc_Set_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        *rcc_RegList[ regId ].RegAddr |= bitMask;
    }
    else
    {
        /* Invalid register ID - nothing is written */
    }
}


/**
 * \brief Clears the register bits given by mask.
 *
 * \param regId   [in]: RCC register ID, value from \ref rcc_RegId_t. Invalid ID is ignored.
 * \param bitMask [in]: Mask of the bits to clear
 */
void Rcc_Reset_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        *rcc_RegList[ regId ].RegAddr &= ~bitMask;
    }
    else
    {
        /* Invalid register ID - nothing is written */
    }
}


/**
 * \brief Reads the register bits given by mask.
 *
 * \param regId   [in]: RCC register ID, value from \ref rcc_RegId_t
 * \param bitMask [in]: Mask of the bits to read
 *
 * \return Register value with applied mask (0 for invalid register ID)
 */
uint32_t Rcc_Get_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    uint32_t regValue = 0u;

    if( RCC_REG_CNT > regId )
    {
        regValue = *rcc_RegList[ regId ].RegAddr & bitMask;
    }
    else
    {
        /* Invalid register ID - zero is returned */
    }

    return ( regValue );
}


/**
 * \brief Updates the register field given by mask.
 *
 * \param regId    [in]: RCC register ID, value from \ref rcc_RegId_t. Invalid ID is ignored.
 * \param regMask  [in]: Mask of the register field
 * \param regValue [in]: Value of the field (masked, at field position)
 */
void Rcc_Set_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t regValue )
{
    if( RCC_REG_CNT > regId )
    {
        *rcc_RegList[ regId ].RegAddr = ( ( *rcc_RegList[ regId ].RegAddr & ~regMask ) |
                                          ( regValue & regMask ) );
    }
    else
    {
        /* Invalid register ID - nothing is written */
    }
}


/**
 * \brief Reads the register field given by mask.
 *
 * \param regId   [in]: RCC register ID, value from \ref rcc_RegId_t
 * \param regMask [in]: Mask of the register field
 *
 * \return Register value with applied mask (0 for invalid register ID)
 */
uint32_t Rcc_Get_RegVal( rcc_RegId_t regId, uint32_t regMask )
{
    uint32_t regValue = 0u;

    if( RCC_REG_CNT > regId )
    {
        regValue = *rcc_RegList[ regId ].RegAddr & regMask;
    }
    else
    {
        /* Invalid register ID - zero is returned */
    }

    return ( regValue );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
