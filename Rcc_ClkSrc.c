/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.h
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkSrc.h"                     /* Self include                   */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Stm32_rcc.h"                      /* RCC module RAL layer           */
#include "Rcc_Reg.h"                        /* Registry operations include    */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_OSC_TIMEOUT_RAW             ( 0x84FCB )

/** Default value of HSI trimming */
#define RCC_HSI_DEFAULT_TRIM_VALUE      ( 64u )

/* Defines used for HSE range */
/** Frequency min for HSE frequency, in Hz */
#define RCC_HSE_FREQUENCY_MIN           ( 4000000u )
/** Frequency max for HSE frequency, in Hz */
#define RCC_HSE_FREQUENCY_MAX           ( 48000000u )

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Local storing of High Speed External (HSE) source value in Hz */
static rcc_FreqHz_t                 rcc_HseFreqHz = 0u;


/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes module Rcc_ClkSrc
 */
void Rcc_ClkSrc_Init( void )
{

}


/**
 * \brief De-initializes module Rcc_ClkSrc
 *
 * This function shall call every necessary sub-module de-initialization function 
 * and free all the resources allocated by the module. In case of failure, the 
 * function shall handle it by itself and shall not be transferred to AppMain 
 * layer.
 */
void Rcc_ClkSrc_Deinit( void )
{

}


/**
 * \brief Main task of module Rcc_ClkSrc
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's 
 * requirements.
 */
void Rcc_ClkSrc_Task( void )
{

}


/*------------------------ Clock sources configuration -----------------------*/

/**
 * \brief Configures High Speed External (HSE) clock source
 *
 * The High Speed External (HSE) clock source has to be inactive for
 * configuration. The HSE will be de-activated, input type will be configured
 * and HSE will be activated again.
 *
 * \param hseType [in]: Type of clock source. Can be crystal or external clock
 * \param clkFreq [in]: Frequency of HSE clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseConfig( rcc_HseType_t hseType, rcc_FreqHz_t clkFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_ClkSrc_Set_HseInactive();

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_ClkSrc_Set_HseType( hseType );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_HseClk( clkFreq );

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_ClkSrc_Set_HseActive();
            }
            else
            {
                /* HSE clock frequency configuration failed. */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else
        {
            /* HSE type configuration failed. */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* HSE de-activation failed. */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activation of High Speed External (HSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    /* HSE must be inactive before configuration */
    returnState = Rcc_ClkSrc_Set_HseInactive();

    if( RCC_REQUEST_OK == returnState )
    {
        LL_RCC_HSE_Enable();

        for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_RCC_HSE_IsReady();

            if( 0u != regValue )
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
        /* HSE de-activation was not successful. */
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief De-activation of High Speed External (HSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSE_Disable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSEON );

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External (HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSE_IsReady();

        regValue = READ_BIT(RCC->CR, RCC_CR_HSEON);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
        {
            *retState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *retState = RCC_FUNCTION_INACTIVE;
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
 * \brief Configures High Speed External (HSE) type
 *
 * \param hseType [in]: Type of clock source. Can be crystal or external clock
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseType( rcc_HseType_t hseType )
{
    rcc_RequestState_t  retState = RCC_REQUEST_ERROR;
    rcc_FunctionState_t hseState = RCC_FUNCTION_INACTIVE;

    retState = Rcc_ClkSrc_Get_HseState( &hseState );

    if( ( RCC_REQUEST_OK        == retState ) &&
        ( RCC_FUNCTION_INACTIVE == hseState )    )
    {
        if( RCC_HSE_TYPE_SIG_DIGITAL_IN == hseType )
        {
            LL_RCC_HSE_EnableBypass();
        }
        else if( RCC_HSE_TYPE_CRYSTAL == hseType )
        {
            /* Other clock types do not need extra functionality activation */
            LL_RCC_HSE_DisableBypass();
        }
        else
        {
            /* No HSE used */
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* HSE must be inactive to configure type */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configuring High Speed External (HSE) frequency configured by user
 *
 * \param clkFreq [in]: Frequency of HSE clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseClk( rcc_FreqHz_t clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_HSE_FREQUENCY_MIN <= clkFreq ) &&
        ( RCC_HSE_FREQUENCY_MAX >= clkFreq )    )
    {
        rcc_HseFreqHz = clkFreq;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading High Speed External (HSE) frequency configured by user
 *
 * \param clkFreq [out]: Frequency of HSE clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = rcc_HseFreqHz;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of High Speed Internal 16MHz (HSI16) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi16Active( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI_Enable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of High Speed Internal  16MHz (HSI16) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi16Inactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI_Disable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External 16MHz (HSI16HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi16State( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSI_IsReady();

        regValue = READ_BIT( RCC->CR, RCC_CR_HSION );

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
        {
            *retState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *retState = RCC_FUNCTION_INACTIVE;
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
 * \brief Reading High Speed Internal 16MHz (HSI16) frequency configured by user
 *
 * \param clkFreq [out]: Frequency of HSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi16Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = HSI_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of High Speed Internal 48MHz (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Active( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI48_Enable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI48_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of High Speed Internal  48MHz (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Inactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI48_Disable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI48_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External 48MHz (HSI48HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48State( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSI48_IsReady();

        regValue = READ_BIT( RCC->CRRCR, RCC_CRRCR_HSI48ON );

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
        {
            *retState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *retState = RCC_FUNCTION_INACTIVE;
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
 * \brief Reading High Speed Internal 48MHz (HSI48) frequency configured by user
 *
 * \param clkFreq [out]: Frequency of HSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = HSI48_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Activation of Low Speed Internal (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_LSE_Enable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_LSE_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of Low Speed Internal (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_LSE_Disable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_LSE_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of Low Speed Internal (LSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LseState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_LSE_IsReady();

        regValue = READ_BIT(RCC->BDCR, RCC_BDCR_LSEON);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
        {
            *retState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *retState = RCC_FUNCTION_INACTIVE;
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
 * \brief Returns value of Low Speed External (LSE) frequency
 *
 * \param lseClk [out]: Frequency of LSE clock in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LseClk( rcc_FreqHz_t * const csiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != csiClk )
    {
        *csiClk = LSE_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}

rcc_RequestState_t Rcc_ClkSrc_Set_LseType( rcc_LseType_t lseType )
{
    rcc_RequestState_t  retState = RCC_REQUEST_ERROR;
    rcc_FunctionState_t hseState = RCC_FUNCTION_INACTIVE;

    retState = Rcc_ClkSrc_Get_LseState( &hseState );

    if( ( RCC_REQUEST_OK        == retState ) &&
        ( RCC_FUNCTION_INACTIVE == hseState )    )
    {
        if( RCC_LSE_TYPE_SIG_DIGITAL_IN == lseType )
        {
            LL_RCC_LSE_EnableBypass();
        }
        else
        {
            /* Other clock types do not need extra functionality activation */
            LL_RCC_LSE_DisableBypass();
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* HSE must be inactive to configure type */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activation of Low Speed Internal (LSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LsiActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_LSI_Enable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_LSI_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of Low Speed Internal (LSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LsiInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_LSI_Disable();

    returnState = RCC_REQUEST_OK;

    return ( returnState );
}


/**
 * \brief Reading status of Low Speed Internal (LSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_LSI_IsReady();

        regValue = READ_BIT(RCC->CSR, RCC_CSR_LSION);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
        {
            *retState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *retState = RCC_FUNCTION_INACTIVE;
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
 * \brief Returns value of Low Speed Internal (LSI) frequency
 *
 * \param lsiClk [out]: Frequency of LSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiClk( rcc_FreqHz_t * const lsiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != lsiClk )
    {
        *lsiClk = LSI_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
