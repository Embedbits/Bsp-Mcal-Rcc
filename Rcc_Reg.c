/**
 * \author Mr.Nobody
 * \file Rcc_Reg.c
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

#if defined(RCC_VER_2_0)
/* STM32H7A3 / H7B0 / H7B3 - CD (CPU) and SRD (smart run) domain registers */
#define RCC_REG_D1CFGR_ADDR             ( &(RCC->CDCFGR1  ) )
#define RCC_REG_D2CFGR_ADDR             ( &(RCC->CDCFGR2  ) )
#define RCC_REG_D3CFGR_ADDR             ( &(RCC->SRDCFGR  ) )
#define RCC_REG_D1CCIPR_ADDR            ( &(RCC->CDCCIPR  ) )
#define RCC_REG_D2CCIP1R_ADDR           ( &(RCC->CDCCIP1R ) )
#define RCC_REG_D2CCIP2R_ADDR           ( &(RCC->CDCCIP2R ) )
#define RCC_REG_D3CCIPR_ADDR            ( &(RCC->SRDCCIPR ) )
#elif defined(STM32H7RS)
/* STM32H7R / H7S - CPU, bus matrix and APB prescalers, kernel clock selection CCIPR1 - CCIPR4 */
#define RCC_REG_D1CFGR_ADDR             ( &(RCC->CDCFGR   ) )
#define RCC_REG_D2CFGR_ADDR             ( &(RCC->BMCFGR   ) )
#define RCC_REG_D3CFGR_ADDR             ( &(RCC->APBCFGR  ) )
#define RCC_REG_D1CCIPR_ADDR            ( &(RCC->CCIPR1   ) )
#define RCC_REG_D2CCIP1R_ADDR           ( &(RCC->CCIPR2   ) )
#define RCC_REG_D2CCIP2R_ADDR           ( &(RCC->CCIPR3   ) )
#define RCC_REG_D3CCIPR_ADDR            ( &(RCC->CCIPR4   ) )
#else
/* D1 / D2 / D3 domain registers */
#define RCC_REG_D1CFGR_ADDR             ( &(RCC->D1CFGR   ) )
#define RCC_REG_D2CFGR_ADDR             ( &(RCC->D2CFGR   ) )
#define RCC_REG_D3CFGR_ADDR             ( &(RCC->D3CFGR   ) )
#define RCC_REG_D1CCIPR_ADDR            ( &(RCC->D1CCIPR  ) )
#define RCC_REG_D2CCIP1R_ADDR           ( &(RCC->D2CCIP1R ) )
#define RCC_REG_D2CCIP2R_ADDR           ( &(RCC->D2CCIP2R ) )
#define RCC_REG_D3CCIPR_ADDR            ( &(RCC->D3CCIPR  ) )
#endif

#if defined(STM32H7RS)
/* STM32H7R / H7S - PLL dividers register 1 (N, P, Q, R), APB1 registers 1 / 2 */
#define RCC_REG_PLL1DIVR_ADDR           ( &(RCC->PLL1DIVR1  ) )
#define RCC_REG_PLL2DIVR_ADDR           ( &(RCC->PLL2DIVR1  ) )
#define RCC_REG_PLL3DIVR_ADDR           ( &(RCC->PLL3DIVR1  ) )
#define RCC_REG_APB1LRSTR_ADDR          ( &(RCC->APB1RSTR1  ) )
#define RCC_REG_APB1HRSTR_ADDR          ( &(RCC->APB1RSTR2  ) )
#define RCC_REG_APB1LENR_ADDR           ( &(RCC->APB1ENR1   ) )
#define RCC_REG_APB1HENR_ADDR           ( &(RCC->APB1ENR2   ) )
#define RCC_REG_APB1LLPENR_ADDR         ( &(RCC->APB1LPENR1 ) )
#define RCC_REG_APB1HLPENR_ADDR         ( &(RCC->APB1LPENR2 ) )
#else
#define RCC_REG_PLL1DIVR_ADDR           ( &(RCC->PLL1DIVR   ) )
#define RCC_REG_PLL2DIVR_ADDR           ( &(RCC->PLL2DIVR   ) )
#define RCC_REG_PLL3DIVR_ADDR           ( &(RCC->PLL3DIVR   ) )
#define RCC_REG_APB1LRSTR_ADDR          ( &(RCC->APB1LRSTR  ) )
#define RCC_REG_APB1HRSTR_ADDR          ( &(RCC->APB1HRSTR  ) )
#define RCC_REG_APB1LENR_ADDR           ( &(RCC->APB1LENR   ) )
#define RCC_REG_APB1HENR_ADDR           ( &(RCC->APB1HENR   ) )
#define RCC_REG_APB1LLPENR_ADDR         ( &(RCC->APB1LLPENR ) )
#define RCC_REG_APB1HLPENR_ADDR         ( &(RCC->APB1HLPENR ) )
#endif

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
    { .RegId = RCC_REG_HSICFGR    , .RegAddr = &(RCC->HSICFGR   ) }, /**< RCC HSI calibration register                         */
    { .RegId = RCC_REG_CRRCR      , .RegAddr = &(RCC->CRRCR     ) }, /**< RCC clock recovery RC register                       */
    { .RegId = RCC_REG_CSICFGR    , .RegAddr = &(RCC->CSICFGR   ) }, /**< RCC CSI calibration register                         */
    { .RegId = RCC_REG_CFGR       , .RegAddr = &(RCC->CFGR      ) }, /**< RCC clock configuration register                     */
    { .RegId = RCC_REG_D1CFGR     , .RegAddr = RCC_REG_D1CFGR_ADDR  }, /**< RCC domain 1 (CD) clock configuration register     */
    { .RegId = RCC_REG_D2CFGR     , .RegAddr = RCC_REG_D2CFGR_ADDR  }, /**< RCC domain 2 (CD) clock configuration register     */
    { .RegId = RCC_REG_D3CFGR     , .RegAddr = RCC_REG_D3CFGR_ADDR  }, /**< RCC domain 3 (SRD) clock configuration register    */
    { .RegId = RCC_REG_PLLCKSELR  , .RegAddr = &(RCC->PLLCKSELR ) }, /**< RCC PLLs clock source selection register             */
    { .RegId = RCC_REG_PLLCFGR    , .RegAddr = &(RCC->PLLCFGR   ) }, /**< RCC PLLs configuration register                      */
    { .RegId = RCC_REG_PLL1DIVR   , .RegAddr = RCC_REG_PLL1DIVR_ADDR }, /**< RCC PLL1 dividers configuration register           */
    { .RegId = RCC_REG_PLL1FRACR  , .RegAddr = &(RCC->PLL1FRACR ) }, /**< RCC PLL1 fractional divider configuration register   */
    { .RegId = RCC_REG_PLL2DIVR   , .RegAddr = RCC_REG_PLL2DIVR_ADDR }, /**< RCC PLL2 dividers configuration register           */
    { .RegId = RCC_REG_PLL2FRACR  , .RegAddr = &(RCC->PLL2FRACR ) }, /**< RCC PLL2 fractional divider configuration register   */
    { .RegId = RCC_REG_PLL3DIVR   , .RegAddr = RCC_REG_PLL3DIVR_ADDR }, /**< RCC PLL3 dividers configuration register           */
    { .RegId = RCC_REG_PLL3FRACR  , .RegAddr = &(RCC->PLL3FRACR ) }, /**< RCC PLL3 fractional divider configuration register   */
#if defined(STM32H7RS)
    { .RegId = RCC_REG_PLL1DIVR2  , .RegAddr = &(RCC->PLL1DIVR2 ) }, /**< RCC PLL1 dividers configuration register 2 (S)       */
    { .RegId = RCC_REG_PLL2DIVR2  , .RegAddr = &(RCC->PLL2DIVR2 ) }, /**< RCC PLL2 dividers configuration register 2 (S, T)    */
    { .RegId = RCC_REG_PLL3DIVR2  , .RegAddr = &(RCC->PLL3DIVR2 ) }, /**< RCC PLL3 dividers configuration register 2 (S)       */
#endif
    { .RegId = RCC_REG_D1CCIPR    , .RegAddr = RCC_REG_D1CCIPR_ADDR  }, /**< RCC domain 1 (CD) kernel clock configuration register   */
    { .RegId = RCC_REG_D2CCIP1R   , .RegAddr = RCC_REG_D2CCIP1R_ADDR }, /**< RCC domain 2 (CD) kernel clock configuration register 1 */
    { .RegId = RCC_REG_D2CCIP2R   , .RegAddr = RCC_REG_D2CCIP2R_ADDR }, /**< RCC domain 2 (CD) kernel clock configuration register 2 */
    { .RegId = RCC_REG_D3CCIPR    , .RegAddr = RCC_REG_D3CCIPR_ADDR  }, /**< RCC domain 3 (SRD) kernel clock configuration register  */
    { .RegId = RCC_REG_CIER       , .RegAddr = &(RCC->CIER      ) }, /**< RCC clock source interrupt enable register           */
    { .RegId = RCC_REG_CIFR       , .RegAddr = &(RCC->CIFR      ) }, /**< RCC clock source interrupt flag register             */
    { .RegId = RCC_REG_CICR       , .RegAddr = &(RCC->CICR      ) }, /**< RCC clock source interrupt clear register            */
    { .RegId = RCC_REG_BDCR       , .RegAddr = &(RCC->BDCR      ) }, /**< RCC backup domain control register                   */
    { .RegId = RCC_REG_CSR        , .RegAddr = &(RCC->CSR       ) }, /**< RCC clock control and status register                */
    { .RegId = RCC_REG_AHB3RSTR   , .RegAddr = &(RCC->AHB3RSTR  ) }, /**< RCC AHB3 peripheral reset register                   */
    { .RegId = RCC_REG_AHB1RSTR   , .RegAddr = &(RCC->AHB1RSTR  ) }, /**< RCC AHB1 peripheral reset register                   */
    { .RegId = RCC_REG_AHB2RSTR   , .RegAddr = &(RCC->AHB2RSTR  ) }, /**< RCC AHB2 peripheral reset register                   */
    { .RegId = RCC_REG_AHB4RSTR   , .RegAddr = &(RCC->AHB4RSTR  ) }, /**< RCC AHB4 peripheral reset register                   */
#if defined(STM32H7RS)
    { .RegId = RCC_REG_AHB5RSTR   , .RegAddr = &(RCC->AHB5RSTR  ) }, /**< RCC AHB5 peripheral reset register                   */
    { .RegId = RCC_REG_APB5RSTR   , .RegAddr = &(RCC->APB5RSTR  ) }, /**< RCC APB5 peripheral reset register                   */
#else
    { .RegId = RCC_REG_APB3RSTR   , .RegAddr = &(RCC->APB3RSTR  ) }, /**< RCC APB3 peripheral reset register                   */
#endif
    { .RegId = RCC_REG_APB1LRSTR  , .RegAddr = RCC_REG_APB1LRSTR_ADDR }, /**< RCC APB1 peripheral reset low word register        */
    { .RegId = RCC_REG_APB1HRSTR  , .RegAddr = RCC_REG_APB1HRSTR_ADDR }, /**< RCC APB1 peripheral reset high word register       */
    { .RegId = RCC_REG_APB2RSTR   , .RegAddr = &(RCC->APB2RSTR  ) }, /**< RCC APB2 peripheral reset register                   */
    { .RegId = RCC_REG_APB4RSTR   , .RegAddr = &(RCC->APB4RSTR  ) }, /**< RCC APB4 peripheral reset register                   */
    { .RegId = RCC_REG_RSR        , .RegAddr = &(RCC->RSR       ) }, /**< RCC reset status register                            */
    { .RegId = RCC_REG_AHB3ENR    , .RegAddr = &(RCC->AHB3ENR   ) }, /**< RCC AHB3 clock enable register                       */
    { .RegId = RCC_REG_AHB1ENR    , .RegAddr = &(RCC->AHB1ENR   ) }, /**< RCC AHB1 clock enable register                       */
    { .RegId = RCC_REG_AHB2ENR    , .RegAddr = &(RCC->AHB2ENR   ) }, /**< RCC AHB2 clock enable register                       */
    { .RegId = RCC_REG_AHB4ENR    , .RegAddr = &(RCC->AHB4ENR   ) }, /**< RCC AHB4 clock enable register                       */
#if defined(STM32H7RS)
    { .RegId = RCC_REG_AHB5ENR    , .RegAddr = &(RCC->AHB5ENR   ) }, /**< RCC AHB5 clock enable register                       */
    { .RegId = RCC_REG_APB5ENR    , .RegAddr = &(RCC->APB5ENR   ) }, /**< RCC APB5 clock enable register                       */
#else
    { .RegId = RCC_REG_APB3ENR    , .RegAddr = &(RCC->APB3ENR   ) }, /**< RCC APB3 clock enable register                       */
#endif
    { .RegId = RCC_REG_APB1LENR   , .RegAddr = RCC_REG_APB1LENR_ADDR }, /**< RCC APB1 clock enable low word register            */
    { .RegId = RCC_REG_APB1HENR   , .RegAddr = RCC_REG_APB1HENR_ADDR }, /**< RCC APB1 clock enable high word register           */
    { .RegId = RCC_REG_APB2ENR    , .RegAddr = &(RCC->APB2ENR   ) }, /**< RCC APB2 clock enable register                       */
    { .RegId = RCC_REG_APB4ENR    , .RegAddr = &(RCC->APB4ENR   ) }, /**< RCC APB4 clock enable register                       */
    { .RegId = RCC_REG_AHB3LPENR  , .RegAddr = &(RCC->AHB3LPENR ) }, /**< RCC AHB3 sleep clock register                        */
    { .RegId = RCC_REG_AHB1LPENR  , .RegAddr = &(RCC->AHB1LPENR ) }, /**< RCC AHB1 sleep clock register                        */
    { .RegId = RCC_REG_AHB2LPENR  , .RegAddr = &(RCC->AHB2LPENR ) }, /**< RCC AHB2 sleep clock register                        */
    { .RegId = RCC_REG_AHB4LPENR  , .RegAddr = &(RCC->AHB4LPENR ) }, /**< RCC AHB4 sleep clock register                        */
#if defined(STM32H7RS)
    { .RegId = RCC_REG_AHB5LPENR  , .RegAddr = &(RCC->AHB5LPENR ) }, /**< RCC AHB5 sleep clock register                        */
    { .RegId = RCC_REG_APB5LPENR  , .RegAddr = &(RCC->APB5LPENR ) }, /**< RCC APB5 sleep clock register                        */
#else
    { .RegId = RCC_REG_APB3LPENR  , .RegAddr = &(RCC->APB3LPENR ) }, /**< RCC APB3 sleep clock register                        */
#endif
    { .RegId = RCC_REG_APB1LLPENR , .RegAddr = RCC_REG_APB1LLPENR_ADDR }, /**< RCC APB1 sleep clock low word register           */
    { .RegId = RCC_REG_APB1HLPENR , .RegAddr = RCC_REG_APB1HLPENR_ADDR }, /**< RCC APB1 sleep clock high word register          */
    { .RegId = RCC_REG_APB2LPENR  , .RegAddr = &(RCC->APB2LPENR ) }, /**< RCC APB2 sleep clock register                        */
    { .RegId = RCC_REG_APB4LPENR  , .RegAddr = &(RCC->APB4LPENR ) }, /**< RCC APB4 sleep clock register                        */
    { .RegId = RCC_REG_FLASH_ACR  , .RegAddr = &(FLASH->ACR     ) }, /**< Flash access control register                        */
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
        *rcc_RegList[regId].RegAddr |= bitMask;
    }
    else
    {
        /* No action required */
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
        *rcc_RegList[regId].RegAddr &= ~bitMask;
    }
    else
    {
        /* No action required */
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
    uint32_t regValue = 0u;

    if( RCC_REG_CNT > regId )
    {
        regValue = *rcc_RegList[regId].RegAddr & bitMask;
    }
    else
    {
        /* No action required */
    }

    return ( regValue );
}


/**
 * \brief Updates the register value with given mask and value.
 *
 * \param regId    [in]: RCC register ID
 * \param regMask  [in]: Mask to apply to the register
 * \param regValue [in]: Value to set in the register
 */
void Rcc_Set_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t regValue )
{
    if( RCC_REG_CNT > regId )
    {
        /* Set the register value with mask */
        *rcc_RegList[regId].RegAddr = ( ( *rcc_RegList[regId].RegAddr & ~regMask ) |
                                        ( regValue & regMask ) );
    }
    else
    {
        /* No action required */
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
    uint32_t regValue = 0u;

    if( RCC_REG_CNT > regId )
    {
        /* Get the register value with mask */
        regValue = *rcc_RegList[regId].RegAddr & regMask;
    }
    else
    {
        /* No action required */
    }

    return ( regValue );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
