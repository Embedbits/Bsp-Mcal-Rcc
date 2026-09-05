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

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        if( RCC_CLKOUT_SOURCE_MCO1_LSE == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_LSE );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_MCO1_LSI == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_LSI );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_MCO1_HSE == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_HSE );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_MCO1_HSI16 == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_HSI );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_MCO1_HSI48 == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_HSI48 );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_MCO1_PLL1R == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_PLLCLK );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_MCO1_SYSCLK == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk, LL_RCC_MCO1SOURCE_SYSCLK );
            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else if ( RCC_CLK_OUT_LSCO == outId )
    {
        if( RCC_CLKOUT_SOURCE_LSCO_LSI == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_BDCR, RCC_BDCR_LSCOSEL_Msk, LL_RCC_LSCO_CLKSOURCE_LSI );
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_SOURCE_LSCO_LSE == clkSource )
        {
            Rcc_Set_RegVal( RCC_REG_BDCR, RCC_BDCR_LSCOSEL_Msk, LL_RCC_LSCO_CLKSOURCE_LSE );
            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* Incorrect output ID */
        retState = RCC_REQUEST_ERROR;
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regVal   = 0u;

    if( RCC_NULL_PTR != clkSource )
    {
        if( RCC_CLK_OUT_MCO1 == outId )
        {
            regVal = Rcc_Get_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOSEL_Msk );

            if( LL_RCC_MCO1SOURCE_SYSCLK == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_SYSCLK;
            }
            else if( LL_RCC_MCO1SOURCE_HSI == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_HSI16;
            }
            else if( LL_RCC_MCO1SOURCE_HSE == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_HSE;
            }
            else if( LL_RCC_MCO1SOURCE_PLLCLK == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_PLL1R;
            }
            else if( LL_RCC_MCO1SOURCE_LSI == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_LSI;
            }
            else if( LL_RCC_MCO1SOURCE_LSE == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_LSE;
            }
            else if( LL_RCC_MCO1SOURCE_HSI48 == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_MCO1_HSI48;
            }
            else
            {
                *clkSource = RCC_CLKOUT_SOURCE_NONE;
            }

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLK_OUT_LSCO == outId )
        {
            regVal = Rcc_Get_RegVal( RCC_REG_BDCR, RCC_BDCR_LSCOSEL_Msk );

            if( LL_RCC_LSCO_CLKSOURCE_LSI == regVal )
            {
                *clkSource = RCC_CLKOUT_SOURCE_LSCO_LSI;
            }
            else
            {
                *clkSource = RCC_CLKOUT_SOURCE_LSCO_LSE;
            }

            retState = RCC_REQUEST_OK;
        }
        else
        {
            *clkSource = RCC_CLKOUT_SOURCE_NONE;

            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
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

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        if( 16u == clkDivider )
        {
            clkDivider = LL_RCC_MCO1_DIV_16;

            retState = RCC_REQUEST_OK;
        }
        else if( 8u == clkDivider )
        {
            clkDivider = LL_RCC_MCO1_DIV_8;

            retState = RCC_REQUEST_OK;
        }
        else if( 4u == clkDivider )
        {
            clkDivider = LL_RCC_MCO1_DIV_4;

            retState = RCC_REQUEST_OK;
        }
        else if( 2u == clkDivider )
        {
            clkDivider = LL_RCC_MCO1_DIV_2;

            retState = RCC_REQUEST_OK;
        }
        else if( 1u == clkDivider )
        {
            clkDivider = LL_RCC_MCO1_DIV_1;

            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }

        if( RCC_REQUEST_ERROR != retState )
        {
            Rcc_Set_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOPRE_Msk, clkDivider );

            retState = RCC_REQUEST_OK;
        }
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
            regVal = Rcc_Get_RegVal( RCC_REG_CFGR, RCC_CFGR_MCOPRE_Msk );

            regVal = regVal >> RCC_CFGR_MCOPRE_Pos;
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


/**
 * \brief Function used to set clock output pin.
 *
 * \param outId [in]: Clock output identification
 * \param pinId [in]: Clock output pin identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockPin( rcc_ClkOut_Id_t outId, rcc_ClkOut_Pin_t pinId )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;
    gpio_Config_t      gpioConfig;

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        if( RCC_CLKOUT_PIN_MCO1_PA8 == pinId )
        {
            gpioConfig.PortId         = GPIO_PORT_A;
            gpioConfig.PinId          = GPIO_PIN_ID_8;
            gpioConfig.PinAltFunction = GPIO_ALT_FUNC_0;
            gpioConfig.PinMode        = GPIO_PIN_MODE_ALTERNATE;

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_CLKOUT_PIN_MCO1_PG10 == pinId )
        {
            gpioConfig.PortId         = GPIO_PORT_G;
            gpioConfig.PinId          = GPIO_PIN_ID_10;
            gpioConfig.PinAltFunction = GPIO_ALT_FUNC_0;
            gpioConfig.PinMode        = GPIO_PIN_MODE_ALTERNATE;

            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else if ( RCC_CLK_OUT_LSCO == outId )
    {
        if( RCC_CLKOUT_PIN_LSCO_PA2 == pinId )
        {
            gpioConfig.PortId         = GPIO_PORT_A;
            gpioConfig.PinId          = GPIO_PIN_ID_8;
            gpioConfig.PinAltFunction = GPIO_ALT_FUNC_0;
            gpioConfig.PinMode        = GPIO_PIN_MODE_ANALOG;

            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* Incorrect output ID */
        retState = RCC_REQUEST_ERROR;
    }

    if( RCC_REQUEST_OK == retState )
    {
        gpioConfig.PinPull        = GPIO_PIN_PULL_NONE;
        gpioConfig.PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH;
        gpioConfig.PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL;
        gpioConfig.PinActiveLevel = GPIO_PIN_LEVEL_HIGH;

        Gpio_Init( &gpioConfig );
    }
    else
    {
        /* Incorrect configuration. No pin configuration necesary */
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
