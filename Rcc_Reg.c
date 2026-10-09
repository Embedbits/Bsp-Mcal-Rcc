/**
 * \author Mr.Nobody
 * \file Rcc_Reg.c
 * \ingroup Rcc
 * \brief Rcc module Reg component functionality.
 *
 * \note Backup domain control register (BDCR) is write protected after reset
 *       (PWR_CR1 DBP bit). Every write access to \ref RCC_REG_BDCR releases the
 *       write protection first (PWR interface clock and DBP bit are set).
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

static void Rcc_Reg_Set_BkUpAccess( rcc_RegId_t regId );

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for backup domain write access confirmation */
#define RCC_REG_TIMEOUT_RAW                 ( 0x84FCB )

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
    { .RegId = RCC_REG_CR         , .RegAddr = &(RCC->CR        ) }, /**< RCC clock control register                                 */
    { .RegId = RCC_REG_ICSCR      , .RegAddr = &(RCC->ICSCR     ) }, /**< RCC internal clock sources calibration register            */
    { .RegId = RCC_REG_CFGR       , .RegAddr = &(RCC->CFGR      ) }, /**< RCC clock configuration register                           */
    { .RegId = RCC_REG_PLLCFGR    , .RegAddr = &(RCC->PLLCFGR   ) }, /**< RCC PLL configuration register                             */
    { .RegId = RCC_REG_CIER       , .RegAddr = &(RCC->CIER      ) }, /**< RCC clock interrupt enable register                        */
    { .RegId = RCC_REG_CIFR       , .RegAddr = &(RCC->CIFR      ) }, /**< RCC clock interrupt flag register                          */
    { .RegId = RCC_REG_CICR       , .RegAddr = &(RCC->CICR      ) }, /**< RCC clock interrupt clear register                         */
    { .RegId = RCC_REG_AHB1RSTR   , .RegAddr = &(RCC->AHB1RSTR  ) }, /**< RCC AHB1 peripheral reset register                         */
    { .RegId = RCC_REG_AHB2RSTR   , .RegAddr = &(RCC->AHB2RSTR  ) }, /**< RCC AHB2 peripheral reset register                         */
    { .RegId = RCC_REG_AHB3RSTR   , .RegAddr = &(RCC->AHB3RSTR  ) }, /**< RCC AHB3 peripheral reset register                         */
    { .RegId = RCC_REG_APB1RSTR1  , .RegAddr = &(RCC->APB1RSTR1 ) }, /**< RCC APB1 peripheral reset register 1                       */
    { .RegId = RCC_REG_APB1RSTR2  , .RegAddr = &(RCC->APB1RSTR2 ) }, /**< RCC APB1 peripheral reset register 2                       */
    { .RegId = RCC_REG_APB2RSTR   , .RegAddr = &(RCC->APB2RSTR  ) }, /**< RCC APB2 peripheral reset register                         */
    { .RegId = RCC_REG_AHB1ENR    , .RegAddr = &(RCC->AHB1ENR   ) }, /**< RCC AHB1 peripheral clock enable register                  */
    { .RegId = RCC_REG_AHB2ENR    , .RegAddr = &(RCC->AHB2ENR   ) }, /**< RCC AHB2 peripheral clock enable register                  */
    { .RegId = RCC_REG_AHB3ENR    , .RegAddr = &(RCC->AHB3ENR   ) }, /**< RCC AHB3 peripheral clock enable register                  */
    { .RegId = RCC_REG_APB1ENR1   , .RegAddr = &(RCC->APB1ENR1  ) }, /**< RCC APB1 peripheral clock enable register 1                */
    { .RegId = RCC_REG_APB1ENR2   , .RegAddr = &(RCC->APB1ENR2  ) }, /**< RCC APB1 peripheral clock enable register 2                */
    { .RegId = RCC_REG_APB2ENR    , .RegAddr = &(RCC->APB2ENR   ) }, /**< RCC APB2 peripheral clock enable register                  */
    { .RegId = RCC_REG_AHB1SMENR  , .RegAddr = &(RCC->AHB1SMENR ) }, /**< RCC AHB1 peripheral clock enable in sleep / stop mode       */
    { .RegId = RCC_REG_AHB2SMENR  , .RegAddr = &(RCC->AHB2SMENR ) }, /**< RCC AHB2 peripheral clock enable in sleep / stop mode       */
    { .RegId = RCC_REG_AHB3SMENR  , .RegAddr = &(RCC->AHB3SMENR ) }, /**< RCC AHB3 peripheral clock enable in sleep / stop mode       */
    { .RegId = RCC_REG_APB1SMENR1 , .RegAddr = &(RCC->APB1SMENR1) }, /**< RCC APB1 peripheral clock enable in sleep / stop mode 1     */
    { .RegId = RCC_REG_APB1SMENR2 , .RegAddr = &(RCC->APB1SMENR2) }, /**< RCC APB1 peripheral clock enable in sleep / stop mode 2     */
    { .RegId = RCC_REG_APB2SMENR  , .RegAddr = &(RCC->APB2SMENR ) }, /**< RCC APB2 peripheral clock enable in sleep / stop mode       */
    { .RegId = RCC_REG_CCIPR      , .RegAddr = &(RCC->CCIPR     ) }, /**< RCC peripherals independent clock configuration register   */
    { .RegId = RCC_REG_BDCR       , .RegAddr = &(RCC->BDCR      ) }, /**< RCC backup domain control register                         */
    { .RegId = RCC_REG_CSR        , .RegAddr = &(RCC->CSR       ) }, /**< RCC clock control & status register                        */
    { .RegId = RCC_REG_CRRCR      , .RegAddr = &(RCC->CRRCR     ) }, /**< RCC clock recovery RC register (HSI48)                     */
    { .RegId = RCC_REG_CCIPR2     , .RegAddr = &(RCC->CCIPR2    ) }, /**< RCC peripherals independent clock configuration register 2 */
    { .RegId = RCC_REG_FLASH_ACR  , .RegAddr = &(FLASH->ACR     ) }, /**< Flash access control register                              */
    { .RegId = RCC_REG_PWR_CR1    , .RegAddr = &(PWR->CR1       ) }, /**< PWR power control register 1                               */
    { .RegId = RCC_REG_PWR_CR5    , .RegAddr = &(PWR->CR5       ) }, /**< PWR power control register 5                               */
    { .RegId = RCC_REG_PWR_SR2    , .RegAddr = &(PWR->SR2       ) }, /**< PWR power status register 2                                */
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
 *
 * This function shall call every necessary sub-module de-initialization function
 * and free all the resources allocated by the module. In case of failure, the
 * function shall handle it by itself and shall not be transferred to AppMain
 * layer.
 */
void Rcc_Reg_Deinit( void )
{
    return;
}


/**
 * \brief Main task of module Rcc_Reg
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's
 * requirements.
 */
void Rcc_Reg_Task( void )
{
    return;
}


/**
 * \brief Sets the register bit with given mask.
 *
 * \param regId   [in]: RCC register ID
 * \param bitMask [in]: Mask to apply to the register
 */
void Rcc_Set_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        Rcc_Reg_Set_BkUpAccess( regId );

        *rcc_RegList[regId].RegAddr |= bitMask;
    }
}


/**
 * \brief Resets the register bit with given mask.
 *
 * \param regId   [in]: RCC register ID
 * \param bitMask [in]: Mask to apply to the register
 */
void Rcc_Reset_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        Rcc_Reg_Set_BkUpAccess( regId );

        *rcc_RegList[regId].RegAddr &= ~bitMask;
    }
}


/**
 * \brief Gets the register bit value with given mask.
 *
 * \param regId   [in]: RCC register ID
 * \param bitMask [in]: Mask to apply to the register
 * \return Register bit value with applied mask
 */
uint32_t Rcc_Get_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        return ( *rcc_RegList[regId].RegAddr & bitMask );
    }
    else
    {
        return 0u;
    }
}


/**
 * \brief Updates the register value with given mask and value.
 *
 * \param regId   [in]: RCC register ID
 * \param regMask [in]: Mask to apply to the register
 * \param regValue Value to set in the register
 */
void Rcc_Set_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t regValue )
{
    if( RCC_REG_CNT > regId )
    {
        Rcc_Reg_Set_BkUpAccess( regId );

        /* Set the register value with mask */
        *rcc_RegList[regId].RegAddr = ( ( *rcc_RegList[regId].RegAddr & ~regMask ) |
                                        ( regValue & regMask ) );
    }
}


/**
 * \brief Gets the register value with given mask.
 *
 * \param regId   [in]: RCC register ID.
 * \param regMask [in]: Mask to apply to the register
 * \return Register value with applied mask
 */
uint32_t Rcc_Get_RegVal( rcc_RegId_t regId, uint32_t regMask )
{
    if( RCC_REG_CNT > regId )
    {
        /* Get the register value with mask */
        return ( *rcc_RegList[regId].RegAddr & regMask );
    }
    else
    {
        return 0u;
    }
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Releases backup domain write protection before write to BDCR register.
 *
 * PWR interface clock is enabled and DBP bit is set (if not set yet). Other
 * registers are not affected.
 *
 * \note The clock enable is read back before the first PWR access (clock is
 *       effective after the enable register write is completed).
 *
 * \param regId [in]: ID of register to be written
 */
static void Rcc_Reg_Set_BkUpAccess( rcc_RegId_t regId )
{
    if( ( RCC_REG_BDCR == regId                                                 ) &&
        ( 0u           == ( *rcc_RegList[ RCC_REG_PWR_CR1 ].RegAddr & PWR_CR1_DBP ) )    )
    {
        /* PWR registers are accessible only with enabled interface clock */
        *rcc_RegList[ RCC_REG_APB1ENR1 ].RegAddr |= RCC_APB1ENR1_PWREN;

        for( uint32_t iterationCnt = 0u; RCC_REG_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( 0u != ( *rcc_RegList[ RCC_REG_APB1ENR1 ].RegAddr & RCC_APB1ENR1_PWREN ) )
            {
                break;
            }
            else
            {
                /* Clock enable not visible yet */
            }
        }

        *rcc_RegList[ RCC_REG_PWR_CR1 ].RegAddr |= PWR_CR1_DBP;

        for( uint32_t iterationCnt = 0u; RCC_REG_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( 0u != ( *rcc_RegList[ RCC_REG_PWR_CR1 ].RegAddr & PWR_CR1_DBP ) )
            {
                break;
            }
            else
            {
                /* Write access not released yet */
            }
        }
    }
    else
    {
        /* Register is not write protected or protection already released */
    }
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
