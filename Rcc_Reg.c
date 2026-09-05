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
    { .RegId = RCC_REG_CR         , .RegAddr = &(RCC->CR        ) }, /**< RCC clock control register                                       */
    { .RegId = RCC_REG_ICSCR      , .RegAddr = &(RCC->ICSCR     ) }, /**< RCC internal clock sources calibration register                  */
    { .RegId = RCC_REG_CRRCR      , .RegAddr = &(RCC->CRRCR     ) }, /**< RCC Clock Recovery RC Register                                   */
    { .RegId = RCC_REG_CFGR       , .RegAddr = &(RCC->CFGR      ) }, /**< RCC clock configuration register 1                               */
    { .RegId = RCC_REG_PLLCFGR    , .RegAddr = &(RCC->PLLCFGR   ) }, /**< PLL1 Configuration Register                                      */
    { .RegId = RCC_REG_CIER       , .RegAddr = &(RCC->CIER      ) }, /**< Clock Interrupt Enable Register                                  */
    { .RegId = RCC_REG_CIFR       , .RegAddr = &(RCC->CIFR      ) }, /**< Clock Interrupt Flag Register                                    */
    { .RegId = RCC_REG_CICR       , .RegAddr = &(RCC->CICR      ) }, /**< Clock Interrupt Clear Register                                   */
    { .RegId = RCC_REG_AHB1RSTR   , .RegAddr = &(RCC->AHB1RSTR  ) }, /**< AHB1 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_AHB2RSTR   , .RegAddr = &(RCC->AHB2RSTR  ) }, /**< AHB2 Peripherals Reset Register 1                                */
    { .RegId = RCC_REG_AHB3RSTR   , .RegAddr = &(RCC->AHB3RSTR  ) }, /**< AHB3 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_APB1RSTR1  , .RegAddr = &(RCC->APB1RSTR1 ) }, /**< APB1 Peripherals Reset Register 1                                */
    { .RegId = RCC_REG_APB1RSTR2  , .RegAddr = &(RCC->APB1RSTR2 ) }, /**< APB1 Peripherals Reset Register 2                                */
    { .RegId = RCC_REG_APB2RSTR   , .RegAddr = &(RCC->APB2RSTR  ) }, /**< APB2 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_AHB1ENR    , .RegAddr = &(RCC->AHB1ENR   ) }, /**< AHB1 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_AHB2ENR    , .RegAddr = &(RCC->AHB2ENR   ) }, /**< AHB2 Peripherals Clock Enable Register 1                         */
    { .RegId = RCC_REG_AHB3ENR    , .RegAddr = &(RCC->AHB3ENR   ) }, /**< AHB3 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_APB1ENR1   , .RegAddr = &(RCC->APB1ENR1  ) }, /**< APB1 Peripherals Clock Enable Register 1                         */
    { .RegId = RCC_REG_APB1ENR2   , .RegAddr = &(RCC->APB1ENR2  ) }, /**< APB1 Peripherals Clock Enable Register 2                         */
    { .RegId = RCC_REG_APB2ENR    , .RegAddr = &(RCC->APB2ENR   ) }, /**< APB2 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_AHB1SMENR  , .RegAddr = &(RCC->AHB1SMENR ) }, /**< AHB1 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    { .RegId = RCC_REG_AHB2SMENR  , .RegAddr = &(RCC->AHB2SMENR ) }, /**< AHB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    { .RegId = RCC_REG_AHB3SMENR  , .RegAddr = &(RCC->AHB3SMENR ) }, /**< AHB3 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    { .RegId = RCC_REG_APB1SMENR1 , .RegAddr = &(RCC->APB1SMENR1) }, /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    { .RegId = RCC_REG_APB1SMENR2 , .RegAddr = &(RCC->APB1SMENR2) }, /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    { .RegId = RCC_REG_APB2SMENR  , .RegAddr = &(RCC->APB2SMENR ) }, /**< APB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    { .RegId = RCC_REG_CCIPR1     , .RegAddr = &(RCC->CCIPR     ) }, /**< IPs Clocks Configuration Register 1                              */
    { .RegId = RCC_REG_CCIPR2     , .RegAddr = &(RCC->CCIPR2    ) }, /**< IPs Clocks Configuration Register 2                              */
    { .RegId = RCC_REG_BDCR       , .RegAddr = &(RCC->BDCR      ) }, /**< Backup Domain Control Register                                   */
    { .RegId = RCC_REG_CSR        , .RegAddr = &(RCC->CSR       ) }, /**< V33 Clock Control & Status Register                              */
    { .RegId = RCC_REG_FLASH_ACR  , .RegAddr = &(FLASH->ACR     ) }  /**< Flash configuration register                                     */
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
