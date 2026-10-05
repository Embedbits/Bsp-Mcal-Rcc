/**
 * \author Mr.Nobody
 * \file Rcc_Reg.c
 * \ingroup Rcc
 * \brief Rcc module Reg component functionality.
 *
 * \note Backup domain control register (BDCR) is write protected after reset
 *       (PWR_CR DBP bit). Every write access to \ref RCC_REG_BDCR releases the
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
    { .RegId = RCC_REG_CR         , .RegAddr = &(RCC->CR        ) }, /**< RCC clock control register                           */
    { .RegId = RCC_REG_PLLCFGR    , .RegAddr = &(RCC->PLLCFGR   ) }, /**< RCC PLL configuration register                       */
    { .RegId = RCC_REG_CFGR       , .RegAddr = &(RCC->CFGR      ) }, /**< RCC clock configuration register                     */
    { .RegId = RCC_REG_CIR        , .RegAddr = &(RCC->CIR       ) }, /**< RCC clock interrupt register                         */
    { .RegId = RCC_REG_AHB1RSTR   , .RegAddr = &(RCC->AHB1RSTR  ) }, /**< RCC AHB1 peripheral reset register                   */
#if defined(RCC_AHB2_SUPPORT)
    { .RegId = RCC_REG_AHB2RSTR   , .RegAddr = &(RCC->AHB2RSTR  ) }, /**< RCC AHB2 peripheral reset register                   */
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    { .RegId = RCC_REG_AHB3RSTR   , .RegAddr = &(RCC->AHB3RSTR  ) }, /**< RCC AHB3 peripheral reset register                   */
#endif /* RCC_AHB3_SUPPORT */
    { .RegId = RCC_REG_APB1RSTR   , .RegAddr = &(RCC->APB1RSTR  ) }, /**< RCC APB1 peripheral reset register                   */
    { .RegId = RCC_REG_APB2RSTR   , .RegAddr = &(RCC->APB2RSTR  ) }, /**< RCC APB2 peripheral reset register                   */
    { .RegId = RCC_REG_AHB1ENR    , .RegAddr = &(RCC->AHB1ENR   ) }, /**< RCC AHB1 peripheral clock enable register            */
#if defined(RCC_AHB2_SUPPORT)
    { .RegId = RCC_REG_AHB2ENR    , .RegAddr = &(RCC->AHB2ENR   ) }, /**< RCC AHB2 peripheral clock enable register            */
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    { .RegId = RCC_REG_AHB3ENR    , .RegAddr = &(RCC->AHB3ENR   ) }, /**< RCC AHB3 peripheral clock enable register            */
#endif /* RCC_AHB3_SUPPORT */
    { .RegId = RCC_REG_APB1ENR    , .RegAddr = &(RCC->APB1ENR   ) }, /**< RCC APB1 peripheral clock enable register            */
    { .RegId = RCC_REG_APB2ENR    , .RegAddr = &(RCC->APB2ENR   ) }, /**< RCC APB2 peripheral clock enable register            */
    { .RegId = RCC_REG_AHB1LPENR  , .RegAddr = &(RCC->AHB1LPENR ) }, /**< RCC AHB1 peripheral clock enable in low power mode   */
#if defined(RCC_AHB2_SUPPORT)
    { .RegId = RCC_REG_AHB2LPENR  , .RegAddr = &(RCC->AHB2LPENR ) }, /**< RCC AHB2 peripheral clock enable in low power mode   */
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    { .RegId = RCC_REG_AHB3LPENR  , .RegAddr = &(RCC->AHB3LPENR ) }, /**< RCC AHB3 peripheral clock enable in low power mode   */
#endif /* RCC_AHB3_SUPPORT */
    { .RegId = RCC_REG_APB1LPENR  , .RegAddr = &(RCC->APB1LPENR ) }, /**< RCC APB1 peripheral clock enable in low power mode   */
    { .RegId = RCC_REG_APB2LPENR  , .RegAddr = &(RCC->APB2LPENR ) }, /**< RCC APB2 peripheral clock enable in low power mode   */
    { .RegId = RCC_REG_BDCR       , .RegAddr = &(RCC->BDCR      ) }, /**< RCC backup domain control register                   */
    { .RegId = RCC_REG_CSR        , .RegAddr = &(RCC->CSR       ) }, /**< RCC clock control & status register                  */
    { .RegId = RCC_REG_SSCGR      , .RegAddr = &(RCC->SSCGR     ) }, /**< RCC spread spectrum clock generation register        */
#if defined(RCC_CR_PLLI2SON)
    { .RegId = RCC_REG_PLLI2SCFGR , .RegAddr = &(RCC->PLLI2SCFGR) }, /**< RCC PLLI2S configuration register                    */
#endif /* RCC_CR_PLLI2SON */
#if defined(RCC_CR_PLLSAION)
    { .RegId = RCC_REG_PLLSAICFGR , .RegAddr = &(RCC->PLLSAICFGR) }, /**< RCC PLLSAI configuration register                    */
#endif /* RCC_CR_PLLSAION */
#if defined(RCC_DCKCFGR_TIMPRE)
    { .RegId = RCC_REG_DCKCFGR    , .RegAddr = &(RCC->DCKCFGR   ) }, /**< RCC dedicated clocks configuration register          */
#endif /* RCC_DCKCFGR_TIMPRE */
#if defined(RCC_CKGATENR_AHB2APB1_CKEN)
    { .RegId = RCC_REG_CKGATENR   , .RegAddr = &(RCC->CKGATENR  ) }, /**< RCC clocks gated enable register                     */
#endif /* RCC_CKGATENR_AHB2APB1_CKEN */
#if defined(RCC_DCKCFGR2_FMPI2C1SEL)
    { .RegId = RCC_REG_DCKCFGR2   , .RegAddr = &(RCC->DCKCFGR2  ) }, /**< RCC dedicated clocks configuration register 2        */
#endif /* RCC_DCKCFGR2_FMPI2C1SEL */
    { .RegId = RCC_REG_FLASH_ACR  , .RegAddr = &(FLASH->ACR     ) }, /**< Flash access control register                        */
    { .RegId = RCC_REG_PWR_CR     , .RegAddr = &(PWR->CR        ) }, /**< PWR power control register                           */
    { .RegId = RCC_REG_PWR_CSR    , .RegAddr = &(PWR->CSR       ) }, /**< PWR power control/status register                    */
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
 * \note Device errata "Delay after an RCC peripheral clock enabling": PWREN is read
 *       back before PWR_CR is accessed.
 *
 * \param regId [in]: ID of register to be written
 */
static void Rcc_Reg_Set_BkUpAccess( rcc_RegId_t regId )
{
    if( ( RCC_REG_BDCR == regId                                           ) &&
        ( 0u           == ( *rcc_RegList[ RCC_REG_PWR_CR ].RegAddr & PWR_CR_DBP ) )    )
    {
        /* PWR registers are accessible only with enabled interface clock */
        *rcc_RegList[ RCC_REG_APB1ENR ].RegAddr |= RCC_APB1ENR_PWREN;

        /* Device errata "Delay after an RCC peripheral clock enabling": the clock is
           effective 1 + AHB/APB prescaler cycles later - the enable register is read back
           before the first PWR access */
        for( uint32_t iterationCnt = 0u; RCC_REG_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( 0u != ( *rcc_RegList[ RCC_REG_APB1ENR ].RegAddr & RCC_APB1ENR_PWREN ) )
            {
                break;
            }
            else
            {
                /* Clock enable not visible yet */
            }
        }

        *rcc_RegList[ RCC_REG_PWR_CR ].RegAddr |= PWR_CR_DBP;

        for( uint32_t iterationCnt = 0u; RCC_REG_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( 0u != ( *rcc_RegList[ RCC_REG_PWR_CR ].RegAddr & PWR_CR_DBP ) )
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
