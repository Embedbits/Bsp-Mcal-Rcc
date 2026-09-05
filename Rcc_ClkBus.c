/**
 * \author Mr.Nobody
 * \file Rcc_ClkBus.h
 * \ingroup Rcc
 * \brief Rcc module ClkBus component functionality.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkBus.h"                     /* Self include                   */
#include "Rcc_ClkSrc.h"                     /* Clock source module            */
#include "Rcc_Pll.h"                        /* PLL module                     */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Stm32_utils.h"                    /* MCU utilities RAL functionality*/
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_CLKBUS_TIMEOUT_RAW              ( 0x84FCB )

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes clock buses module.
 */
rcc_RequestState_t Rcc_ClkBus_Init( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    return ( returnState );
}


/**
 * \brief De-initializes module Rcc_ClkBus
 */
void Rcc_ClkBus_Deinit( void )
{
    return;
}


/**
 * \brief Main task of clock buses module.
 */
void Rcc_ClkBus_Task( void )
{
    return;
}


/**
 * \brief Configure system clock source multiplexer.
 *
 * \param systemClkSource [in]: Selected clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_SysClkSource( rcc_SystemClkSrc_t systemClkSource )
{
    rcc_RequestState_t returnState  = RCC_REQUEST_ERROR;
    uint32_t           clkSource    = 0u;
    uint32_t           clkSourceRet = 0u;

    if( RCC_SYSTEM_CLOCK_SOURCE_CNT > systemClkSource )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_HSI == systemClkSource )
        {
            clkSource = LL_RCC_SYS_CLKSOURCE_HSI;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == systemClkSource )
        {
            clkSource = LL_RCC_SYS_CLKSOURCE_HSE;
        }
        else
        {
            clkSource = LL_RCC_SYS_CLKSOURCE_PLL;
        }

        LL_RCC_SetSysClkSource( clkSource );

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            clkSourceRet = LL_RCC_GetSysClkSource() >> RCC_CFGR_SWS_Pos;

            if( clkSource == clkSourceRet )
            {
                returnState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
                returnState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Read system clock source multiplexer.
 *
 * System clock source multiplexer is used for selection of main clock source.
 *
 * \param systemClkSource [out] : Actual clock source ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_SysClkSource( rcc_SystemClkSrc_t *systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != systemClkSource )
    {
        uint32_t regValue = LL_RCC_GetSysClkSource();

        if( LL_RCC_SYS_CLKSOURCE_STATUS_HSI == regValue)
        {
            *systemClkSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;
        }
        else if( LL_RCC_SYS_CLKSOURCE_STATUS_HSE == regValue )
        {
            *systemClkSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
        }
        else
        {
            *systemClkSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
        }

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading of system clock frequency (SYSCLK) in Hz
 *
 * \note This function reads real values from registers and calculate actual
 *       frequency.
 *
 * \param busClk [out]: Pointer to SYSCLK value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_SysClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState        = RCC_REQUEST_ERROR;
    rcc_SystemClkSrc_t systemClkSource = 0u;

    /* Read System Clock Multiplexer settings */
    retState = Rcc_ClkBus_Get_SysClkSource( &systemClkSource );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != busClk   )    )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == systemClkSource )
        {
            retState = Rcc_Pll_Get_Clk_OutP( RCC_PLL_1, busClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == systemClkSource )
        {
            retState = Rcc_ClkSrc_Get_HseClk( busClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == systemClkSource )
        {
            retState = Rcc_ClkSrc_Get_Hsi16Clk( busClk );
        }
        else
        {
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
 * \brief Configure AHB divider.
 *
 * HCLK is clocked through AHB divider.
 *
 * \param dividerId [in] : Required AHB divider value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_AHBDivider( rcc_AHB_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAHBPrescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAHBPrescaler();

        if( dividerId == regValue )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Register value is not as required, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    if( RCC_REQUEST_OK == returnState )
    {
        rcc_FreqHz_t ahbFrequency = 0u;
        returnState = Rcc_ClkBus_Get_AHBClk( &ahbFrequency );

        if( RCC_REQUEST_OK == returnState )
        {
            LL_SetSystemCoreClock( ahbFrequency );
        }
    }

    return ( returnState );
}


/**
 * \brief Read AHB divider.
 *
 * HCLK is clocked through AHB divider.
 *
 * \param dividerId [out] : AHB divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_AHBDivider( rcc_AHB_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAHBPrescaler();

    return ( returnState );
}


/**
 * \brief Reading of High performance clock frequency (HCLK) in Hz
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_AHBClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2  = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       systemClk  = 0u;
    rcc_AHB_Div_t      ahbDivider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_ClkBus_Get_SysClk( &systemClk );
    retState2 = Rcc_ClkBus_Get_AHBDivider( &ahbDivider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk    ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
    {
        *busClk = __LL_RCC_CALC_HCLK_FREQ( systemClk, ahbDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure APB1 divider.
 *
 * Clock bus APB1 is clocked through APB1 divider from HCLK (AHB bus).
 *
 * \param dividerId [in] : Identification of divider configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB1Divider( rcc_APB1_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAPB1Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAPB1Prescaler();

        if( dividerId == regValue )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Register value is not as required, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    return ( returnState );
}


/**
 * \brief Read APB1 divider.
 *
 * Clock bus APB1 is clocked through APB1 divider from HCLK (AHB bus).
 *
 * \param dividerId     [out] : Divider ID
 * \param dividerNumVal [out] : Divider numerical value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB1Divider( rcc_APB1_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAPB1Prescaler();

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB1 clock bus
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB1Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB1_Div_t     apb1Divider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB1Divider( &apb1Divider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk    ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
    {
        *busClk = __LL_RCC_CALC_PCLK1_FREQ( hClk, apb1Divider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure APB2 divider.
 *
 * Clock bus APB2 is clocked through APB2 divider from HCLK (AHB bus).
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB2Divider( rcc_APB2_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAPB2Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAPB2Prescaler();

        if( dividerId == regValue )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Register value is not as required, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    return ( returnState );
}


/**
 * \brief Read APB2 divider.
 *
 * Clock bus APB2 is clocked through APB2 divider from HCLK (AHB bus).
 *
 * \param dividerId [out] : Divider ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB2Divider( rcc_APB2_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAPB2Prescaler();

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB2 clock bus
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB2Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB2_Div_t     apb2Divider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB2Divider( &apb2Divider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk    ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
    {
        *busClk = __LL_RCC_CALC_PCLK2_FREQ( hClk, apb2Divider );
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
