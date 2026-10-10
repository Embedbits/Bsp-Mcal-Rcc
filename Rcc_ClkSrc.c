/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.c
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality.
 *
 * \note  Exception of MCAL layering rule: backup domain write protection (PWR
 *        DBP) has no MCAL module - LSE activation enables the write access by
 *        LL directly.
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkSrc.h"                     /* Self include                   */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Stm32_rcc.h"                      /* RCC module RAL layer           */
#include "Stm32_pwr.h"                      /* PWR module RAL layer           */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_OSC_TIMEOUT_RAW             ( 0x84FCB )

/** Division factor of CSI clock used as HDMI-CEC kernel clock */
#define RCC_CSI_CEC_DIVIDER             ( 122u )

/** RTCPRE values 0 and 1 - no clock is provided to RTC */
#define RCC_RTC_HSE_DIV_MIN             ( 2u )

#if defined(STM32H7RS)
/** HSE divider of the USB PHY reference clock source HSE / 2 (STM32H7R / H7S) */
#define RCC_HSE_USBPHY_DIVIDER          ( 2u )
#endif

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
    return;
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
    return;
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
    return;
}


/*------------------------ Clock sources configuration -----------------------*/

/**
 * \brief Activation of High Speed External (HSE) block
 *
 * External clock signals use the HSE bypass. The analog / digital type of the
 * external signal (HSEEXT) is selected on devices supporting it (STM32H72x /
 * H73x, STM32H7A3 / H7B0 / H7B3), the other devices accept both types in
 * bypass mode.
 *
 * \param hseType [in]: Type of clock source. Can be crystal or external clock
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseActive( rcc_HseType_t hseType )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    /* HSE must be inactive before configuration */
    returnState = Rcc_ClkSrc_Set_HseInactive();

    if( RCC_REQUEST_OK == returnState )
    {
        if( RCC_HSE_TYPE_NONE !=  hseType )
        {
            if( RCC_HSE_TYPE_SIG_ANALOG_IN == hseType )
            {
                LL_RCC_HSE_EnableBypass();
#if defined(RCC_CR_HSEEXT)
                LL_RCC_HSE_SelectAnalogClock();
#endif
            }
            else if( RCC_HSE_TYPE_SIG_DIGITAL_IN == hseType )
            {
                LL_RCC_HSE_EnableBypass();
#if defined(RCC_CR_HSEEXT)
                LL_RCC_HSE_SelectDigitalClock();
#endif
            }
            else
            {
                /* Crystal / ceramic resonator - oscillator without bypass */
                LL_RCC_HSE_DisableBypass();
            }

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
            returnState = RCC_REQUEST_OK;
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
        regValue = LL_RCC_HSE_IsReady();

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
rcc_RequestState_t Rcc_ClkSrc_Get_HseState( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = LL_RCC_HSE_IsReady();
        const uint32_t regValue   = READ_BIT( RCC->CR, RCC_CR_HSEON );

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

    if( 0u != clkFreq )
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
 * \brief Activation of High Speed Internal (HSI) block
 *
 * High Speed Internal (HSI) RC oscillator is connected through clock divider
 * (HSIDIV). RC itself generates 64MHz frequency which can be divided by
 * 1/2/4/8. The factory trimming of the oscillator is kept.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi64Active( void )
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
 * \brief De-activation of High Speed Internal (HSI) block
 *
 * \note HSI cannot be stopped while it is used as system clock or PLL clock
 *       source - the request returns error then (HSIRDY stays set).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi64Inactive( void )
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
 * \brief Reading status of High Speed Internal (HSI) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi64State( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = LL_RCC_HSI_IsReady();
        const uint32_t regValue   = READ_BIT( RCC->CR, RCC_CR_HSION );

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
 * \brief Reading High Speed Internal (HSI) frequency
 *
 * The frequency is the HSI oscillator frequency (\c HSI_VALUE) divided by the actual
 * HSI divider (HSIDIV, reset value /1).
 *
 * \param clkFreq [out]: Frequency of HSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi64Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        const uint32_t hsiDivShift = LL_RCC_HSI_GetDivider() >> RCC_CR_HSIDIV_Pos;

        *clkFreq = (rcc_FreqHz_t)( HSI_VALUE >> hsiDivShift );

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
 * \brief De-activation of High Speed Internal 48MHz (HSI48) block
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
 * \brief Reading status of High Speed Internal 48MHz (HSI48) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48State( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = LL_RCC_HSI48_IsReady();
        const uint32_t regValue   = READ_BIT( RCC->CR, RCC_CR_HSI48ON );

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
 * \brief Reading High Speed Internal 48MHz (HSI48) frequency
 *
 * \param clkFreq [out]: Frequency of HSI48 clock in Hz
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


/**
 * \brief Activation of Low Power RC oscillator (CSI) block.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_CsiActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_CSI_Enable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_CSI_IsReady();

        if( 0u != regValue )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Oscillator is not ready yet, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    return ( returnState );
}


/**
 * \brief De-activation of Low Power RC oscillator (CSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_CsiInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_CSI_Disable();

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_CSI_IsReady();

        if( 0u == regValue )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Oscillator is still running, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    return ( returnState );
}


/**
 * \brief Reading status of Low Power RC oscillator (CSI) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_CsiState( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = LL_RCC_CSI_IsReady();
        const uint32_t regValue   = READ_BIT( RCC->CR, RCC_CR_CSION );

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
 * \brief Reading Low power RC internal oscillator (CSI) frequency
 *
 * \param clkFreq [out]: Frequency of CSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_CsiClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = CSI_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading frequency of CSI clock divided by 122 (HDMI-CEC kernel clock)
 *
 * \param clkFreq [out]: Frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_CsiDiv122Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = CSI_VALUE / RCC_CSI_CEC_DIVIDER;

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
 * \brief Activation of Low Speed External (LSE) block
 *
 * Write access to the backup domain (PWR DBP) is enabled - LSE configuration
 * is part of the backup domain.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_PWR_EnableBkUpAccess();

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
 * \brief De-activation of Low Speed External (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_PWR_EnableBkUpAccess();

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
 * \brief Reading status of Low Speed External (LSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LseState( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = LL_RCC_LSE_IsReady();
        const uint32_t regValue   = READ_BIT( RCC->BDCR, RCC_BDCR_LSEON );

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
 * \param lseClk [out]: Pointer to store frequency of LSE clock in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LseClk( rcc_FreqHz_t * const lseClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != lseClk )
    {
        *lseClk = LSE_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
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
 * \note LSI cannot be stopped while the independent watchdog runs (hardware
 *       keeps it on) - the request returns OK, LSI state can be read by
 *       \ref Rcc_ClkSrc_Get_LsiState.
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
 * \brief Reading status of Low Speed Internal (LSI) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiState( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = LL_RCC_LSI_IsReady();
        const uint32_t regValue   = READ_BIT( RCC->CSR, RCC_CSR_LSION );

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

/*--------------------------- Derived clock sources --------------------------*/

/**
 * \brief Returns frequency of the peripheral clock per_ck (CKPER)
 *
 * per_ck is selected by CKPERSEL from HSI (after HSIDIV), CSI or HSE.
 *
 * \param clkFreq [out]: Frequency of per_ck in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when per_ck is disabled).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_PerClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        const uint32_t clkSource = LL_RCC_GetCLKPClockSource( LL_RCC_CLKP_CLKSOURCE );

        if( LL_RCC_CLKP_CLKSOURCE_HSI == clkSource )
        {
            returnState = Rcc_ClkSrc_Get_Hsi64Clk( clkFreq );
        }
        else if( LL_RCC_CLKP_CLKSOURCE_CSI == clkSource )
        {
            returnState = Rcc_ClkSrc_Get_CsiClk( clkFreq );
        }
        else if( LL_RCC_CLKP_CLKSOURCE_HSE == clkSource )
        {
            returnState = Rcc_ClkSrc_Get_HseClk( clkFreq );
        }
        else
        {
            /* per_ck disabled */
            *clkFreq    = 0u;
            returnState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Returns frequency of HSE divided by RTC prescaler (RTCPRE)
 *
 * \param clkFreq [out]: Frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when RTCPRE provides no clock).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_RtcHseClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hseClk      = 0u;

    if( RCC_NULL_PTR != clkFreq )
    {
        const uint32_t rtcDiv = LL_RCC_GetRTC_HSEPrescaler() >> RCC_CFGR_RTCPRE_Pos;

        returnState = Rcc_ClkSrc_Get_HseClk( &hseClk );

        if( ( RCC_REQUEST_OK      == returnState ) &&
            ( RCC_RTC_HSE_DIV_MIN <= rtcDiv      )    )
        {
            *clkFreq = hseClk / rtcDiv;
        }
        else
        {
            *clkFreq    = 0u;
            returnState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


#if defined(STM32H7RS)
/**
 * \brief Returns frequency of HSE divided by 2 (USB PHY reference clock source
 *        of STM32H7R / H7S).
 *
 * \param clkFreq [out]: Frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseDiv2Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hseClk      = 0u;

    if( RCC_NULL_PTR != clkFreq )
    {
        returnState = Rcc_ClkSrc_Get_HseClk( &hseClk );

        *clkFreq = hseClk / RCC_HSE_USBPHY_DIVIDER;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}
#endif


/**
 * \brief Returns frequency of an external clock input (I2S_CKIN, SPDIF symbol
 *        clock, DSI PHY clock, USB PHY 48 MHz clock).
 *
 * \note  Frequency of the external clock inputs is not known by the module.
 *
 * \param clkFreq [out]: Frequency in Hz (always 0)
 *
 * \return Always error - frequency not known.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_PinClk( rcc_FreqHz_t * const clkFreq )
{
    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = 0u;
    }
    else
    {
        /* No action required */
    }

    return ( RCC_REQUEST_ERROR );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
