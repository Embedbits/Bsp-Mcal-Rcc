/**
 * \author Mr.Nobody
 * \file Rcc_ClkOut.h
 * \ingroup Rcc
 * \brief Rcc module ClkOut component functionality.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkOut.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Gpio_Port.h"                      /* GPIO functionality include     */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes clock output module
 */
rcc_RequestState_t Rcc_ClkOut_Init( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    return ( returnState );
}


/**
 * \brief De-initializes clock output module
 */
void Rcc_ClkOut_Deinit( void )
{

}


/**
 * \brief Main task of clock output module
 */
void Rcc_ClkOut_Task( void )
{

}

/*----------------------- Clock outputs configuration ------------------------*/

/**
 * \brief Function used to set clock output signal source.
 *
 * \param outId     [in]: Clock output identification
 * \param clkSource [in]: Clock output signal source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;
    gpio_Config_t      gpioConfig;

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        if( RCC_CLK_SOURCE_MCO1_LSE == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1SEL_Msk, LL_RCC_MCO1SOURCE_LSE );
        }
        else if( RCC_CLK_SOURCE_MCO1_HSE == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1SEL_Msk, LL_RCC_MCO1SOURCE_HSE );
        }
        else if( RCC_CLK_SOURCE_MCO1_HSI64 == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1SEL_Msk, LL_RCC_MCO1SOURCE_HSI );
        }
        else if( RCC_CLK_SOURCE_MCO1_HSI48 == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1SEL_Msk, LL_RCC_MCO1SOURCE_HSI48 );
        }
        else if( RCC_CLK_SOURCE_MCO1_PLL1Q == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1SEL_Msk, LL_RCC_MCO1SOURCE_PLL1Q );
        }
        else
        {

        }

        gpioConfig.PortId = GPIO_PORT_A;
        gpioConfig.PinId  = GPIO_PIN_ID_8;
    }
    else if( RCC_CLK_OUT_MCO2 == outId )
    {
        if( RCC_CLK_SOURCE_MCO2_LSI == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk, LL_RCC_MCO2SOURCE_LSI );

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLK_SOURCE_MCO2_HSE == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk, LL_RCC_MCO2SOURCE_HSE );

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLK_SOURCE_MCO2_CSI == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk, LL_RCC_MCO2SOURCE_CSI );

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLK_SOURCE_MCO2_PLL1P == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk, LL_RCC_MCO2SOURCE_PLL1P );

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLK_SOURCE_MCO2_PLL2P == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk, LL_RCC_MCO2SOURCE_PLL2P );

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLK_SOURCE_MCO2_SYSCLK == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk, LL_RCC_MCO2SOURCE_SYSCLK );

            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }

        gpioConfig.PortId = GPIO_PORT_C;
        gpioConfig.PinId  = GPIO_PIN_ID_9;
    }
    else if ( RCC_CLK_OUT_LSCO == outId )
    {
        if( RCC_CLK_SOURCE_LSCO_LSI == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_BDCR, RCC_BDCR_LSCOSEL_Msk, LL_RCC_LSCO_CLKSOURCE_LSI );
        }
        else
        {
            Rcc_Set_RegVal( RCC_REG_BDCR, RCC_BDCR_LSCOSEL_Msk, LL_RCC_LSCO_CLKSOURCE_LSE );
        }


        gpioConfig.PortId = GPIO_PORT_B;
        gpioConfig.PinId  = GPIO_PIN_ID_2;

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect output ID */
        retState = RCC_REQUEST_ERROR;
    }

    if( RCC_REQUEST_OK == retState )
    {
        gpioConfig.PinMode          = GPIO_PIN_MODE_ALTERNATE;;
        gpioConfig.PinPull          = GPIO_PIN_PULL_NONE;
        gpioConfig.PinSpeed         = GPIO_PIN_SPEED_VERY_HIGH;
        gpioConfig.PinOutType       = GPIO_PIN_OUTPUT_PUSHPULL;
        gpioConfig.PinAltFunction   = GPIO_ALT_FUNC_0;
        gpioConfig.PinActiveLevel   = GPIO_PIN_LEVEL_HIGH;

        Gpio_Init( &gpioConfig );
    }
    else
    {
        /* Incorrect configuration. No pin configuration necesary */
    }

    return ( retState );
}


/**
 * \brief Function used to get clock output signal source.
 *
 * \param outId      [in]: Clock output identification
 * \param clkSource [out]: Pointer to MCO clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Get_ClockSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        *clkSource = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1SEL_Msk );
    }
    else if( RCC_CLK_OUT_MCO2 == outId )
    {
        *clkSource = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2SEL_Msk );
    }
    else
    {
        *clkSource = Rcc_Get_RegVal( RCC_REG_BDCR, RCC_BDCR_LSCOSEL_Msk );
    }

    return ( retState );
}


/**
 * \brief Function used to set clock output divider.
 *
 * \param outId      [in]: Clock output identification
 * \param clkDivider [in]: Clock output divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;
    uint32_t           regVal   = 0u;

    regVal = clkDivider;

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        regVal = regVal << RCC_CFGR1_MCO1PRE_Pos;

        Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1PRE_Msk, regVal );
    }
    else if( RCC_CLK_OUT_MCO2 == outId )
    {
        regVal = regVal << RCC_CFGR1_MCO2PRE_Pos;

        Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2PRE_Msk, regVal );
    }
    else
    {
        /* LSCO does not have prescaller */
    }

    return ( retState );
}


/**
 * \brief Function used to get MCO clock divider.
 *
 * \param outId      [in] : Clock output identification
 * \param clkDivider [out]: Pointer to MCO clock divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Get_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regVal   = 0u;

    if( ( RCC_NULL_PTR   != clkDivider ) &&
        ( RCC_CLK_OUT_CNT > outId      )    )
    {
        if( RCC_CLK_OUT_MCO1 == outId )
        {
            regVal = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1PRE_Msk );

            regVal = regVal >> RCC_CFGR1_MCO1PRE_Pos;
        }
        else if( RCC_CLK_OUT_MCO2 == outId )
        {
            regVal = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2PRE_Msk );

            regVal = regVal >> RCC_CFGR1_MCO2PRE_Pos;
        }
        else
        {
            /* LSCO does not have prescaller */
            regVal = 0u;
        }

        *clkDivider = regVal;

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
