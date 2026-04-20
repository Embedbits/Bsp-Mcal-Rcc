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
const rcc_RegList_t                     rcc_RegList[ RCC_REG_CNT ] =
{
    { .RegId = RCC_REG_CR         , .RegAddr = &(RCC->CR        ) }, /**< RCC clock control register                           */
    { .RegId = RCC_REG_HSICFGR    , .RegAddr = &(RCC->HSICFGR   ) }, /**< RCC HSI Clock Calibration Register,                  */
    { .RegId = RCC_REG_CRRCR      , .RegAddr = &(RCC->CRRCR     ) }, /**< RCC Clock Recovery RC Register,                      */
    { .RegId = RCC_REG_CSICFGR    , .RegAddr = &(RCC->CSICFGR   ) }, /**< RCC CSI Clock Calibration Register,                  */
    { .RegId = RCC_REG_CFGR1      , .RegAddr = &(RCC->CFGR1     ) }, /**< RCC clock configuration register 1                   */
    { .RegId = RCC_REG_CFGR2      , .RegAddr = &(RCC->CFGR2     ) }, /**< RCC clock configuration register 2                   */
    { .RegId = RCC_REG_PLL1CFGR   , .RegAddr = &(RCC->PLL1CFGR  ) }, /**< RCC PLL1 Configuration Register                      */
    { .RegId = RCC_REG_PLL2CFGR   , .RegAddr = &(RCC->PLL2CFGR  ) }, /**< RCC PLL2 Configuration Register                      */
    { .RegId = RCC_REG_PLL3CFGR   , .RegAddr = &(RCC->PLL3CFGR  ) }, /**< RCC PLL3 Configuration Register                      */
    { .RegId = RCC_REG_PLL1DIVR   , .RegAddr = &(RCC->PLL1DIVR  ) }, /**< RCC PLL1 Dividers Configuration Register             */
    { .RegId = RCC_REG_PLL1FRACR  , .RegAddr = &(RCC->PLL1FRACR ) }, /**< RCC PLL1 Fractional Divider Configuration Register   */
    { .RegId = RCC_REG_PLL2DIVR   , .RegAddr = &(RCC->PLL2DIVR  ) }, /**< RCC PLL2 Dividers Configuration Register             */
    { .RegId = RCC_REG_PLL2FRACR  , .RegAddr = &(RCC->PLL2FRACR ) }, /**< RCC PLL2 Fractional Divider Configuration Register   */
    { .RegId = RCC_REG_PLL3DIVR   , .RegAddr = &(RCC->PLL3DIVR  ) }, /**< RCC PLL3 Dividers Configuration Register             */
    { .RegId = RCC_REG_PLL3FRACR  , .RegAddr = &(RCC->PLL3FRACR ) }, /**< RCC PLL3 Fractional Divider Configuration Register   */
    { .RegId = RCC_REG_CIER       , .RegAddr = &(RCC->CIER      ) }, /**< RCC Clock Interrupt Enable Register                  */
    { .RegId = RCC_REG_CIFR       , .RegAddr = &(RCC->CIFR      ) }, /**< RCC Clock Interrupt Flag Register                    */
    { .RegId = RCC_REG_CICR       , .RegAddr = &(RCC->CICR      ) }, /**< RCC Clock Interrupt Clear Register                   */
    { .RegId = RCC_REG_AHB1RSTR   , .RegAddr = &(RCC->AHB1RSTR  ) }, /**< RCC AHB1 Peripherals Reset Register                  */
    { .RegId = RCC_REG_AHB2RSTR   , .RegAddr = &(RCC->AHB2RSTR  ) }, /**< RCC AHB2 Peripherals Reset Register                  */
    { .RegId = RCC_REG_AHB4RSTR   , .RegAddr = &(RCC->AHB4RSTR  ) }, /**< RCC AHB4 Peripherals Reset Register                  */
    { .RegId = RCC_REG_APB1LRSTR  , .RegAddr = &(RCC->APB1LRSTR ) }, /**< RCC APB1 Peripherals reset Low Word register         */
    { .RegId = RCC_REG_APB1HRSTR  , .RegAddr = &(RCC->APB1HRSTR ) }, /**< RCC APB1 Peripherals reset High Word register        */
    { .RegId = RCC_REG_APB2RSTR   , .RegAddr = &(RCC->APB2RSTR  ) }, /**< RCC APB2 Peripherals Reset Register                  */
    { .RegId = RCC_REG_APB3RSTR   , .RegAddr = &(RCC->APB3RSTR  ) }, /**< RCC APB3 Peripherals Reset Register                  */
    { .RegId = RCC_REG_AHB1ENR    , .RegAddr = &(RCC->AHB1ENR   ) }, /**< RCC AHB1 Peripherals Clock Enable Register           */
    { .RegId = RCC_REG_AHB2ENR    , .RegAddr = &(RCC->AHB2ENR   ) }, /**< RCC AHB2 Peripherals Clock Enable Register           */
    { .RegId = RCC_REG_AHB4ENR    , .RegAddr = &(RCC->AHB4ENR   ) }, /**< RCC AHB4 Peripherals Clock Enable Register           */
    { .RegId = RCC_REG_APB1LENR   , .RegAddr = &(RCC->APB1LENR  ) }, /**< RCC APB1 Peripherals clock Enable Low Word register  */
    { .RegId = RCC_REG_APB1HENR   , .RegAddr = &(RCC->APB1HENR  ) }, /**< RCC APB1 Peripherals clock Enable High Word register */
    { .RegId = RCC_REG_APB2ENR    , .RegAddr = &(RCC->APB2ENR   ) }, /**< RCC APB2 Peripherals Clock Enable Register           */
    { .RegId = RCC_REG_APB3ENR    , .RegAddr = &(RCC->APB3ENR   ) }, /**< RCC APB3 Peripherals Clock Enable Register           */
    { .RegId = RCC_REG_AHB1LPENR  , .RegAddr = &(RCC->AHB1LPENR ) }, /**< RCC AHB1 Peripheral sleep clock Register             */
    { .RegId = RCC_REG_AHB2LPENR  , .RegAddr = &(RCC->AHB2LPENR ) }, /**< RCC AHB2 Peripheral sleep clock Register             */
    { .RegId = RCC_REG_AHB4LPENR  , .RegAddr = &(RCC->AHB4LPENR ) }, /**< RCC AHB4 Peripherals sleep clock Register            */
    { .RegId = RCC_REG_APB1LLPENR , .RegAddr = &(RCC->APB1LLPENR) }, /**< RCC APB1 Peripherals sleep clock Low Word Register   */
    { .RegId = RCC_REG_APB1HLPENR , .RegAddr = &(RCC->APB1HLPENR) }, /**< RCC APB1 Peripherals sleep clock High Word Register  */
    { .RegId = RCC_REG_APB2LPENR  , .RegAddr = &(RCC->APB2LPENR ) }, /**< RCC APB2 Peripherals sleep clock Register            */
    { .RegId = RCC_REG_APB3LPENR  , .RegAddr = &(RCC->APB3LPENR ) }, /**< RCC APB3 Peripherals Clock Low Power Enable Register */
    { .RegId = RCC_REG_CCIPR1     , .RegAddr = &(RCC->CCIPR1    ) }, /**< RCC IPs Clocks Configuration Register 1              */
    { .RegId = RCC_REG_CCIPR2     , .RegAddr = &(RCC->CCIPR2    ) }, /**< RCC IPs Clocks Configuration Register 2              */
    { .RegId = RCC_REG_CCIPR3     , .RegAddr = &(RCC->CCIPR3    ) }, /**< RCC IPs Clocks Configuration Register 3              */
    { .RegId = RCC_REG_CCIPR4     , .RegAddr = &(RCC->CCIPR4    ) }, /**< RCC IPs Clocks Configuration Register 4              */
    { .RegId = RCC_REG_CCIPR5     , .RegAddr = &(RCC->CCIPR5    ) }, /**< RCC IPs Clocks Configuration Register 5              */
    { .RegId = RCC_REG_BDCR       , .RegAddr = &(RCC->BDCR      ) }, /**< RCC VSW Backup Domain & V33 Domain Control Register  */
    { .RegId = RCC_REG_RSR        , .RegAddr = &(RCC->RSR       ) }, /**< RCC Reset status Register                            */
    { .RegId = RCC_REG_SECCFGR    , .RegAddr = &(RCC->SECCFGR   ) }, /**< RCC Secure mode configuration register               */
    { .RegId = RCC_REG_PRIVCFGR   , .RegAddr = &(RCC->PRIVCFGR  ) }, /**< RCC Privilege configuration register                 */
    { .RegId = RCC_REG_FLASH_ACR  , .RegAddr = &(FLASH->ACR     ) }, /**< Flash configuration register                         */
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes module Rcc_Reg
 */
void Rcc_Reg_Init( void )
{

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

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
