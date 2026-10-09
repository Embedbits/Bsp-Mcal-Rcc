/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.c
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality.
 *
 * Handles oscillators of STM32G4 family:
 * - HSE   - High Speed External oscillator (crystal or external clock signal)
 * - HSI   - 16 MHz High Speed Internal RC oscillator (HSI16)
 * - HSI48 - 48 MHz High Speed Internal RC oscillator (USB, RNG)
 * - LSE   - 32.768 kHz Low Speed External oscillator (backup domain)
 * - LSI   - 32 kHz Low Speed Internal RC oscillator
 *
 * HSE clock for RTC is divided by fixed divider 32.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkSrc.h"                     /* Self include                   */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Stm32_rcc.h"                      /* RCC module RAL layer           */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_OSC_TIMEOUT_RAW             ( 0x84FCB )

/** LSE crystal start-up time (STM32G4 datasheet tSU(LSE) up to 2 s) [ms] */
#define RCC_OSC_LSE_STARTUP_MS          ( 2000u )

/** Lower bound of processor cycles of one wait loop iteration - LSE start-up
 *  timeout is never shorter than \ref RCC_OSC_LSE_STARTUP_MS */
#define RCC_WAIT_LOOP_MIN_CYCLES        ( 8u )

/** Count of milliseconds in one second */
#define RCC_MS_IN_SECOND                ( 1000u )

/** Fixed value of HSE divider for RTC */
#define RCC_RTC_HSE_DIV                 ( 32u )

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_ClkSrc_Wait_Flag( rcc_RegId_t regId, uint32_t flagMask, uint32_t expectedVal, uint32_t timeout );
static uint32_t           Rcc_ClkSrc_Get_LseTimeout( void );

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
 * HSE is de-activated first (bypass can be changed only when HSE is off), then
 * configured and activated.
 *
 * \warning HSE must not be used as system clock or PLL source (HSE can not be
 *          de-activated in such a case).
 *
 * \param hseType [in]: Type of clock source. Can be crystal or external clock
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseActive( rcc_HseType_t hseType )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    /* HSE must be inactive before configuration */
    returnState = Rcc_ClkSrc_Set_HseInactive();

    if( RCC_REQUEST_OK == returnState )
    {
        if( RCC_HSE_TYPE_SIG_IN == hseType )
        {
            /* External clock signal - oscillator bypassed */
            Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_HSEBYP );
        }
        else if( RCC_HSE_TYPE_CRYSTAL == hseType )
        {
            Rcc_Reset_RegBit( RCC_REG_CR, RCC_CR_HSEBYP );
        }
        else
        {
            /* HSE is not used */
        }

        if( ( RCC_HSE_TYPE_CRYSTAL == hseType ) ||
            ( RCC_HSE_TYPE_SIG_IN  == hseType )    )
        {
            Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_HSEON );

            returnState = Rcc_ClkSrc_Wait_Flag( RCC_REG_CR, RCC_CR_HSERDY, RCC_CR_HSERDY, RCC_OSC_TIMEOUT_RAW );
        }
        else if( RCC_HSE_TYPE_NONE == hseType )
        {
            returnState = RCC_REQUEST_OK;
        }
        else
        {
            /* Unsupported HSE type */
            returnState = RCC_REQUEST_ERROR;
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
    Rcc_Reset_RegBit( RCC_REG_CR, RCC_CR_HSEON );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CR, RCC_CR_HSERDY, 0u, RCC_OSC_TIMEOUT_RAW ) );
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
        const uint32_t readyState = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSERDY );
        const uint32_t regValue   = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSEON );

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
 * \brief Configures HSE divider for RTC clock
 *
 * STM32G4 HSE clock for RTC is divided by fixed divider 32 - only this value
 * is accepted.
 *
 * \param hseDiv [in]: HSE divider value, 32 only
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseRtcDiv( rcc_Rtc_HseDiv_t hseDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_RTC_HSE_DIV == hseDiv )
    {
        /* Fixed divider - nothing to configure */
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Prepares divided HSE clock for RTC
 *
 * STM32G4 HSE clock for RTC is divided by fixed divider 32, the clock is
 * available whenever HSE frequency is configured.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (HSE frequency is not configured).
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseRtcActive( void )
{
    rcc_FreqHz_t rtcClk = 0u;

    return ( Rcc_ClkSrc_Get_HseRtcClk( &rtcClk ) );
}


/**
 * \brief Reads HSE divider for RTC clock
 *
 * \param hseDiv [out]: HSE divider value (always 32)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseRtcDiv( rcc_Rtc_HseDiv_t * const hseDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != hseDiv )
    {
        *hseDiv = (rcc_Rtc_HseDiv_t)RCC_RTC_HSE_DIV;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading frequency of HSE divided for RTC (HSE / 32)
 *
 * \param clkFreq [out]: Frequency of divided HSE clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when HSE frequency is not configured).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseRtcClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = rcc_HseFreqHz / RCC_RTC_HSE_DIV;

        if( 0u != *clkFreq )
        {
            returnState = RCC_REQUEST_OK;
        }
        else
        {
            /* HSE frequency is not configured */
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
 * \brief Activation of High Speed Internal (HSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HsiActive( void )
{
    Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_HSION );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY, RCC_OSC_TIMEOUT_RAW ) );
}


/**
 * \brief De-activation of High Speed Internal (HSI) block
 *
 * \warning HSI can not be de-activated while it is used as system clock or PLL
 *          source - error is returned in such a case.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HsiInactive( void )
{
    Rcc_Reset_RegBit( RCC_REG_CR, RCC_CR_HSION );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CR, RCC_CR_HSIRDY, 0u, RCC_OSC_TIMEOUT_RAW ) );
}


/**
 * \brief Reading status of High Speed Internal (HSI) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HsiState( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t readyState = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSIRDY );
        const uint32_t regValue   = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSION );

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
 * \param clkFreq [out]: Frequency of HSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HsiClk( rcc_FreqHz_t * const clkFreq )
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
 * \brief Activation of 48 MHz High Speed Internal (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Active( void )
{
    Rcc_Set_RegBit( RCC_REG_CRRCR, RCC_CRRCR_HSI48ON );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CRRCR, RCC_CRRCR_HSI48RDY, RCC_CRRCR_HSI48RDY, RCC_OSC_TIMEOUT_RAW ) );
}


/**
 * \brief De-activation of 48 MHz High Speed Internal (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Inactive( void )
{
    Rcc_Reset_RegBit( RCC_REG_CRRCR, RCC_CRRCR_HSI48ON );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CRRCR, RCC_CRRCR_HSI48RDY, 0u, RCC_OSC_TIMEOUT_RAW ) );
}


/**
 * \brief Reading status of 48 MHz High Speed Internal (HSI48) block
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
        const uint32_t readyState = Rcc_Get_RegBit( RCC_REG_CRRCR, RCC_CRRCR_HSI48RDY );
        const uint32_t regValue   = Rcc_Get_RegBit( RCC_REG_CRRCR, RCC_CRRCR_HSI48ON );

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
 * \brief Reading 48 MHz High Speed Internal (HSI48) frequency
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

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Activation of Low Speed External (LSE) block
 *
 * LSE is located in backup domain - write protection is released automatically.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseActive( void )
{
    Rcc_Set_RegBit( RCC_REG_BDCR, RCC_BDCR_LSEON );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_BDCR, RCC_BDCR_LSERDY, RCC_BDCR_LSERDY, Rcc_ClkSrc_Get_LseTimeout() ) );
}


/**
 * \brief De-activation of Low Speed External (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseInactive( void )
{
    Rcc_Reset_RegBit( RCC_REG_BDCR, RCC_BDCR_LSEON );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_BDCR, RCC_BDCR_LSERDY, 0u, RCC_OSC_TIMEOUT_RAW ) );
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
        const uint32_t readyState = Rcc_Get_RegBit( RCC_REG_BDCR, RCC_BDCR_LSERDY );
        const uint32_t regValue   = Rcc_Get_RegBit( RCC_REG_BDCR, RCC_BDCR_LSEON );

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
    Rcc_Set_RegBit( RCC_REG_CSR, RCC_CSR_LSION );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CSR, RCC_CSR_LSIRDY, RCC_CSR_LSIRDY, RCC_OSC_TIMEOUT_RAW ) );
}


/**
 * \brief De-activation of Low Speed Internal (LSI) block
 *
 * \note LSI can not be de-activated while independent watchdog is running.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LsiInactive( void )
{
    Rcc_Reset_RegBit( RCC_REG_CSR, RCC_CSR_LSION );

    return ( Rcc_ClkSrc_Wait_Flag( RCC_REG_CSR, RCC_CSR_LSIRDY, 0u, RCC_OSC_TIMEOUT_RAW ) );
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
        const uint32_t readyState = Rcc_Get_RegBit( RCC_REG_CSR, RCC_CSR_LSIRDY );
        const uint32_t regValue   = Rcc_Get_RegBit( RCC_REG_CSR, RCC_CSR_LSION );

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

/**
 * \brief Waits until register field reaches expected value.
 *
 * \param regId       [in]: Register identification
 * \param flagMask    [in]: Mask of the checked field
 * \param expectedVal [in]: Expected value of the masked field
 * \param timeout     [in]: Maximal count of read iterations
 *
 * \return Returns "OK" if the field reached expected value in time, otherwise
 *         returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Wait_Flag( rcc_RegId_t regId, uint32_t flagMask, uint32_t expectedVal, uint32_t timeout )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; timeout > iterationCnt; iterationCnt ++ )
    {
        if( expectedVal == Rcc_Get_RegVal( regId, flagMask ) )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Flag has not reached expected value yet, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    return ( returnState );
}


/**
 * \brief Returns LSE start-up timeout as count of wait loop iterations.
 *
 * The count is calculated from the actual processor clock (CMSIS
 * SystemCoreClock). A fixed count would shorten the timeout below the LSE
 * start-up time at high clock (170 MHz) and make it unnecessarily long at
 * low clock.
 *
 * \return Count of wait loop iterations
 */
static uint32_t Rcc_ClkSrc_Get_LseTimeout( void )
{
    return ( ( SystemCoreClock / RCC_MS_IN_SECOND / RCC_WAIT_LOOP_MIN_CYCLES ) * RCC_OSC_LSE_STARTUP_MS );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
