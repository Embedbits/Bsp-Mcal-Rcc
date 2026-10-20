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
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Stm32_rcc.h"                      /* RCC module RAL layer           */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_OSC_TIMEOUT_RAW             ( 0x84FCB )

/** RCC_BDCR bits of LSE configuration (writable only with LSE disabled) */
#define RCC_LSE_CONFIG_MASK             ( RCC_BDCR_LSEDRV | RCC_BDCR_LSEBYP | RCC_BDCR_LSEEXT )

/** Default value of HSI trimming */
#define RCC_HSI_DEFAULT_TRIM_VALUE      ( 64u )

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

static uint32_t             Rcc_ClkSrc_Get_LseRegConfig ( void );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Local storing of High Speed External (HSE) source value in Hz */
static rcc_FreqHz_t                 rcc_HseFreqHz = 0u;

/** LSE input type applied at LSE activation (crystal - LSEBYP reset value) */
static rcc_LseType_t                rcc_LseType   = RCC_LSE_TYPE_CRYSTAL;

/** LSE driving capability applied at LSE activation (medium-low - LSEDRV reset value 00 is
 *  forbidden by device errata ES0561 2.2.5, ES0565 2.2.17, ES0621 2.2.8) */
static rcc_LseDrive_t               rcc_LseDrive  = RCC_LSE_DRIVE_MEDIUM_LOW;

/** RCC_BDCR.LSEDRV values of LSE driving capabilities */
static const uint32_t               rcc_ClkSrc_LseDriveLut[ RCC_LSE_DRIVE_CNT ] =
{
    [RCC_LSE_DRIVE_MEDIUM_LOW ] = LL_RCC_LSEDRIVE_MEDIUMLOW ,
    [RCC_LSE_DRIVE_MEDIUM_HIGH] = LL_RCC_LSEDRIVE_MEDIUMHIGH,
    [RCC_LSE_DRIVE_HIGH       ] = LL_RCC_LSEDRIVE_HIGH      ,
};


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
 * \param hseType [in]: Type of clock source. Can be crystal or external clock
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HseActive( rcc_HseType_t hseType)
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
                regValue = LL_RCC_HSE_ANALOG_TYPE;

                LL_RCC_HSE_EnableBypass();
                LL_RCC_HSE_SetExternalClockType( regValue );
            }
            else if( RCC_HSE_TYPE_SIG_DIGITAL_IN == hseType )
            {
                regValue = LL_RCC_HSE_DIGITAL_TYPE;

                LL_RCC_HSE_EnableBypass();
                LL_RCC_HSE_SetExternalClockType( regValue );
            }
            else
            {
                /* Crystal / ceramic resonator - oscillator without bypass. HSEBYP of
                 * a previous external clock configuration is cleared while HSE is off,
                 * HSEEXT is used in bypass mode only */
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
 * High Speed Internal (HSI) RC oscillator is connected through clock divider.
 * User must select desired output frequency. RC itself generate 64MHz frequency
 * which can be divided by 1/2/4/8.
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

        uint32_t hsiState = READ_BIT(RCC->CR, RCC_CR_HSION);

        if( ( 0u != regValue ) ||
            ( 0u == hsiState )    )
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

    LL_RCC_HSI_SetCalibTrimming( RCC_HSI_DEFAULT_TRIM_VALUE );

    return ( returnState );
}


/**
 * \brief De-activation of High Speed Internal (HSI) block
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
 * \brief Reading status of High Speed External (HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi64State( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSI_IsReady();

        regValue = READ_BIT(RCC->CR, RCC_CR_HSION);

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
 * \brief Reading High Speed Internal (HSI) frequency configured by user
 *
 * The frequency is the HSI oscillator frequency (\c HSI_VALUE) divided by the actual
 * HSI divider (HSIDIV, reset value /2).
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

        regValue = READ_BIT( RCC->CR, RCC_CR_HSI48ON );

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


/**
 * \brief Activation of Low Power RC oscillator (CSI) block.
 *
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

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Configuration of Low Speed External (LSE) input type and driving capability
 *
 * The configuration is stored and applied to RCC_BDCR (LSEBYP, LSEEXT, LSEDRV) by
 * \ref Rcc_ClkSrc_Set_LseActive. Running LSE is not changed by this function.
 *
 * \param lseType  [in]: Type of LSE input (none, crystal or digital bypass)
 * \param lseDrive [in]: Driving capability of LSE crystal oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseConfig( rcc_LseType_t lseType, rcc_LseDrive_t lseDrive )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_LSE_TYPE_CNT  > lseType  ) &&
        ( RCC_LSE_DRIVE_CNT > lseDrive )    )
    {
        rcc_LseType  = lseType;
        rcc_LseDrive = lseDrive;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of Low Speed External (LSE) block
 *
 * LSE input type and driving capability of \ref Rcc_ClkSrc_Set_LseConfig are applied
 * and LSE is enabled. The start-up of the crystal is not awaited - LSE ready state is
 * reported by \ref Rcc_ClkSrc_Get_LseState.
 *
 * LSE located in backup domain keeps running over system resets - enabled LSE (running
 * or starting) with the requested configuration is kept. Otherwise LSE is disabled first
 * (LSEBYP, LSEEXT and LSEDRV are writable only with LSE disabled), configured and enabled.
 * Backup domain write protection is released before RCC_BDCR is written.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (no LSE input configured or configuration not applied).
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_LSE_TYPE_NONE != rcc_LseType )
    {
        const uint32_t requiredConfig = Rcc_ClkSrc_Get_LseRegConfig();
        const uint32_t actualConfig   = READ_BIT( RCC->BDCR, RCC_LSE_CONFIG_MASK );
        const uint32_t enableState    = READ_BIT( RCC->BDCR, RCC_BDCR_LSEON );

        if( ( 0u             != enableState  ) &&
            ( requiredConfig == actualConfig )    )
        {
            /* LSE is enabled with the requested configuration (running or starting) */
            returnState = RCC_REQUEST_OK;
        }
        else
        {
            /* Releases the backup domain write access and stops LSE */
            returnState = Rcc_ClkSrc_Set_LseInactive();

            if( RCC_REQUEST_OK == returnState )
            {
                LL_RCC_LSE_SetDriveCapability( rcc_ClkSrc_LseDriveLut[ rcc_LseDrive ] );

                if( RCC_LSE_TYPE_SIG_DIGITAL_IN == rcc_LseType )
                {
                    /* External digital signal connected to OSC32_IN */
                    LL_RCC_LSE_SetExternalClockType( LL_RCC_LSE_DIGITAL_TYPE );
                    LL_RCC_LSE_EnableBypass();
                }
                else
                {
                    /* Crystal - LSEEXT is cleared, its reset value is 1 on STM32H543 / H553 */
                    LL_RCC_LSE_SetExternalClockType( LL_RCC_LSE_ANALOG_TYPE );
                    LL_RCC_LSE_DisableBypass();
                }

                /* Start-up of the crystal is not awaited */
                LL_RCC_LSE_Enable();

                const uint32_t appliedConfig = READ_BIT( RCC->BDCR, RCC_LSE_CONFIG_MASK | RCC_BDCR_LSEON );

                if( ( requiredConfig | RCC_BDCR_LSEON ) == appliedConfig )
                {
                    /* LSE is enabled with the requested configuration */
                    returnState = RCC_REQUEST_OK;
                }
                else
                {
                    /* Configuration was not applied (backup domain write protected) */
                    returnState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* LSE de-activation failed, keep return state as error */
            }
        }
    }
    else
    {
        /* No LSE input connected */
        returnState = RCC_REQUEST_ERROR;
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

    /* LSEON is in the write protected backup domain (RCC_BDCR) */
    returnState = Rcc_Reg_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
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
    }
    else
    {
        /* Backup domain write access not released, keep return state as error */
        returnState = RCC_REQUEST_ERROR;
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

    /* LSION is in the write protected backup domain (RCC_BDCR) */
    returnState = Rcc_Reg_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
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
    }
    else
    {
        /* Backup domain write access not released, keep return state as error */
        returnState = RCC_REQUEST_ERROR;
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

    /* LSION is in the write protected backup domain (RCC_BDCR) */
    returnState = Rcc_Reg_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
        LL_RCC_LSI_Disable();
    }
    else
    {
        /* Backup domain write access not released, keep return state as error */
        returnState = RCC_REQUEST_ERROR;
    }

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
rcc_RequestState_t Rcc_ClkSrc_Get_LsiState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_LSI_IsReady();

        regValue = READ_BIT(RCC->BDCR, RCC_BDCR_LSION);

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
 * \brief Returns RCC_BDCR LSE configuration bits (LSEDRV, LSEBYP, LSEEXT) of the stored
 *        LSE input type and driving capability.
 *
 * \return Value of RCC_BDCR bits masked by \ref RCC_LSE_CONFIG_MASK.
 */
static uint32_t Rcc_ClkSrc_Get_LseRegConfig( void )
{
    uint32_t regConfig = rcc_ClkSrc_LseDriveLut[ rcc_LseDrive ];

    if( RCC_LSE_TYPE_SIG_DIGITAL_IN == rcc_LseType )
    {
        regConfig |= ( RCC_BDCR_LSEBYP | RCC_BDCR_LSEEXT );
    }
    else
    {
        /* Crystal - oscillator not bypassed, LSEEXT cleared */
    }

    return ( regConfig );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
