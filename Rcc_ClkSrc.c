/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.c
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality.
 *
 * Handles oscillators of STM32L4 / STM32L4+ family:
 * - HSE   - High Speed External oscillator (crystal or external clock signal)
 * - HSI16 - 16 MHz High Speed Internal RC oscillator
 * - MSI   - Multi Speed Internal RC oscillator (100 kHz - 48 MHz)
 * - HSI48 - 48 MHz High Speed Internal RC oscillator (MCUs with CRRCR register)
 * - LSE   - 32.768 kHz Low Speed External oscillator (backup domain)
 * - LSI   - 32 kHz Low Speed Internal RC oscillator
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

/** Maximal wait time for LSE oscillator start-up (crystal start-up up to 2 s) */
#define RCC_OSC_LSE_TIMEOUT_RAW         ( 8u * RCC_OSC_TIMEOUT_RAW )

/** Fixed HSE divider of RTC clock */
#define RCC_RTC_HSE_DIV                 ( 32u )

/** Count of MSI ranges (entries of MSIRangeTable) */
#define RCC_MSI_RANGE_CNT               ( 12u )

/** Lowest MSI range after standby (RCC_CSR MSISRANGE 0100 - 1 MHz) */
#define RCC_MSIS_RANGE_MIN              ( 4u )

/** Highest MSI range after standby (RCC_CSR MSISRANGE 0111 - 8 MHz) */
#define RCC_MSIS_RANGE_MAX              ( 7u )

/* ============================== TYPEDEFS ================================== */

/** \brief Oscillators handled by the component (indexes of \ref rcc_ClkSrcOscConfig) */
typedef enum
{
    RCC_CLKSRC_OSC_HSE = 0u, /**< High Speed External oscillator      */
    RCC_CLKSRC_OSC_HSI,      /**< 16 MHz High Speed Internal oscillator */
    RCC_CLKSRC_OSC_MSI,      /**< Multi Speed Internal oscillator      */
#if defined(RCC_CRRCR_HSI48ON)
    RCC_CLKSRC_OSC_HSI48,    /**< 48 MHz High Speed Internal oscillator */
#endif /* RCC_CRRCR_HSI48ON */
    RCC_CLKSRC_OSC_LSE,      /**< Low Speed External oscillator        */
    RCC_CLKSRC_OSC_LSI,      /**< Low Speed Internal oscillator        */
    RCC_CLKSRC_OSC_CNT       /**< Count of oscillators                 */
}   rcc_ClkSrcOscId_t;


/** \brief Oscillator control configuration */
typedef struct
{
    rcc_RegId_t RegId;      /**< Register with enable and ready bits   */
    uint32_t    EnableMask; /**< Oscillator enable bit                 */
    uint32_t    ReadyMask;  /**< Oscillator ready flag                 */
    uint32_t    Timeout;    /**< Maximal count of ready flag reads     */
}   rcc_ClkSrcOscConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_ClkSrc_Wait_Flag      ( rcc_RegId_t regId, uint32_t flagMask, uint32_t expectedVal, uint32_t timeout );
static rcc_RequestState_t Rcc_ClkSrc_Set_OscEnable  ( rcc_ClkSrcOscId_t oscId, rcc_FunctionState_t oscState );
static rcc_RequestState_t Rcc_ClkSrc_Get_OscEnable  ( rcc_ClkSrcOscId_t oscId, rcc_FunctionState_t * const retState );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Local storing of High Speed External (HSE) source value in Hz */
static rcc_FreqHz_t                 rcc_HseFreqHz = 0u;

/** Enable / ready bits of oscillators, indexed by \ref rcc_ClkSrcOscId_t */
static const rcc_ClkSrcOscConfig_t  rcc_ClkSrcOscConfig[ RCC_CLKSRC_OSC_CNT ] =
{
    [RCC_CLKSRC_OSC_HSE  ] = { .RegId = RCC_REG_CR   , .EnableMask = RCC_CR_HSEON      , .ReadyMask = RCC_CR_HSERDY      , .Timeout = RCC_OSC_TIMEOUT_RAW     },
    [RCC_CLKSRC_OSC_HSI  ] = { .RegId = RCC_REG_CR   , .EnableMask = RCC_CR_HSION      , .ReadyMask = RCC_CR_HSIRDY      , .Timeout = RCC_OSC_TIMEOUT_RAW     },
    [RCC_CLKSRC_OSC_MSI  ] = { .RegId = RCC_REG_CR   , .EnableMask = RCC_CR_MSION      , .ReadyMask = RCC_CR_MSIRDY      , .Timeout = RCC_OSC_TIMEOUT_RAW     },
#if defined(RCC_CRRCR_HSI48ON)
    [RCC_CLKSRC_OSC_HSI48] = { .RegId = RCC_REG_CRRCR, .EnableMask = RCC_CRRCR_HSI48ON , .ReadyMask = RCC_CRRCR_HSI48RDY , .Timeout = RCC_OSC_TIMEOUT_RAW     },
#endif /* RCC_CRRCR_HSI48ON */
    [RCC_CLKSRC_OSC_LSE  ] = { .RegId = RCC_REG_BDCR , .EnableMask = RCC_BDCR_LSEON    , .ReadyMask = RCC_BDCR_LSERDY    , .Timeout = RCC_OSC_LSE_TIMEOUT_RAW },
    [RCC_CLKSRC_OSC_LSI  ] = { .RegId = RCC_REG_CSR  , .EnableMask = RCC_CSR_LSION     , .ReadyMask = RCC_CSR_LSIRDY     , .Timeout = RCC_OSC_TIMEOUT_RAW     },
};

/* ========================= EXPORTED FUNCTIONS ============================= */

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

    if( RCC_REQUEST_OK != returnState )
    {
        /* HSE de-activation was not successful */
    }
    else if( RCC_HSE_TYPE_NONE == hseType )
    {
        /* HSE is not used */
    }
    else if( ( RCC_HSE_TYPE_CRYSTAL == hseType ) ||
             ( RCC_HSE_TYPE_SIG_IN  == hseType )    )
    {
        if( RCC_HSE_TYPE_SIG_IN == hseType )
        {
            /* External clock signal - oscillator bypassed */
            Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_HSEBYP );
        }
        else
        {
            Rcc_Reset_RegBit( RCC_REG_CR, RCC_CR_HSEBYP );
        }

        returnState = Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_HSE, RCC_FUNCTION_ACTIVE );
    }
    else
    {
        /* Unsupported HSE type */
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
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_HSE, RCC_FUNCTION_INACTIVE ) );
}


/**
 * \brief Reading status of High Speed External (HSE) block
 *
 * \param retState [out]: Pointer to actual status value (active = enabled and ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscEnable( RCC_CLKSRC_OSC_HSE, retState ) );
}


/**
 * \brief Configuring High Speed External (HSE) frequency configured by user
 *
 * \param clkFreq [in]: Frequency of HSE clock in Hz, greater than 0
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
 * \brief Reading frequency of HSE divided for RTC (HSE / 32)
 *
 * \param clkFreq [out]: Frequency of divided HSE clock in Hz (0 if HSE frequency is not configured)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if HSE frequency is not configured).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseRtcClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR == clkFreq )
    {
        returnState = RCC_REQUEST_ERROR;
    }
    else if( 0u == rcc_HseFreqHz )
    {
        /* HSE frequency is not known */
        *clkFreq    = 0u;
        returnState = RCC_REQUEST_ERROR;
    }
    else
    {
        *clkFreq = rcc_HseFreqHz / RCC_RTC_HSE_DIV;

        returnState = RCC_REQUEST_OK;
    }

    return ( returnState );
}


/**
 * \brief Activation of 16 MHz High Speed Internal (HSI16) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HsiActive( void )
{
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_HSI, RCC_FUNCTION_ACTIVE ) );
}


/**
 * \brief De-activation of 16 MHz High Speed Internal (HSI16) block
 *
 * \warning HSI16 can not be de-activated while it is used as system clock or PLL
 *          source - error is returned in such a case.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_HsiInactive( void )
{
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_HSI, RCC_FUNCTION_INACTIVE ) );
}


/**
 * \brief Reading status of 16 MHz High Speed Internal (HSI16) block
 *
 * \param retState [out]: Pointer to actual status value (active = enabled and ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HsiState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscEnable( RCC_CLKSRC_OSC_HSI, retState ) );
}


/**
 * \brief Reading 16 MHz High Speed Internal (HSI16) frequency
 *
 * \param clkFreq [out]: Frequency of HSI16 clock in Hz
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
 * \brief Activation of Multi Speed Internal (MSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsiActive( void )
{
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_MSI, RCC_FUNCTION_ACTIVE ) );
}


/**
 * \brief De-activation of Multi Speed Internal (MSI) block
 *
 * \warning MSI can not be de-activated while it is used as system clock or PLL
 *          source - error is returned in such a case.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsiInactive( void )
{
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_MSI, RCC_FUNCTION_INACTIVE ) );
}


/**
 * \brief Reading status of Multi Speed Internal (MSI) block
 *
 * \param retState [out]: Pointer to actual status value (active = enabled and ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsiState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscEnable( RCC_CLKSRC_OSC_MSI, retState ) );
}


/**
 * \brief Configures frequency range of Multi Speed Internal (MSI) oscillator
 *
 * The range is selected by RCC_CR MSIRANGE (MSIRGSEL is set).
 *
 * \pre   MSI is off, or MSI is on and ready - otherwise \ref RCC_REQUEST_ERROR is
 *        returned and no register is changed.
 * \warning If MSI is the system clock, the flash latency has to be configured
 *          for the new frequency first.
 *
 * \param msiRange [in]: Required MSI range, value from \ref rcc_MsiRange_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsiRange( rcc_MsiRange_t msiRange )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    const uint32_t     msiOn       = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_MSION );
    const uint32_t     msiReady    = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_MSIRDY );

    if( ( 0u != ( (uint32_t)msiRange & ~RCC_CR_MSIRANGE ) ) ||
        ( RCC_MSI_RANGE_48MHZ < msiRange                    )    )
    {
        /* Value does not belong to MSI range field */
        returnState = RCC_REQUEST_ERROR;
    }
    else if( ( 0u != msiOn    ) &&
             ( 0u == msiReady )    )
    {
        /* MSI range can not be changed while MSI is starting */
        returnState = RCC_REQUEST_ERROR;
    }
    else
    {
        Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_MSIRGSEL );
        Rcc_Set_RegVal( RCC_REG_CR, RCC_CR_MSIRANGE, (uint32_t)msiRange );

        returnState = Rcc_ClkSrc_Wait_Flag( RCC_REG_CR, RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE,
                                            RCC_CR_MSIRGSEL | (uint32_t)msiRange, RCC_OSC_TIMEOUT_RAW );
    }

    return ( returnState );
}


/**
 * \brief Reading Multi Speed Internal (MSI) frequency
 *
 * The frequency is given by RCC_CR MSIRANGE (MSIRGSEL set) or by the range after
 * standby RCC_CSR MSISRANGE (MSIRGSEL cleared, reset state).
 *
 * \param clkFreq [out]: Frequency of MSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsiClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        const uint32_t rangeSel = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_MSIRGSEL );
        uint32_t       rangeIdx = 0u;
        uint32_t       rangeMin = 0u;
        uint32_t       rangeMax = RCC_MSI_RANGE_CNT - 1u;

        if( 0u != rangeSel )
        {
            rangeIdx = Rcc_Get_RegVal( RCC_REG_CR, RCC_CR_MSIRANGE ) >> RCC_CR_MSIRANGE_Pos;
        }
        else
        {
            /* Range after standby supports 1, 2, 4 and 8 MHz only */
            rangeIdx = Rcc_Get_RegVal( RCC_REG_CSR, RCC_CSR_MSISRANGE ) >> RCC_CSR_MSISRANGE_Pos;
            rangeMin = RCC_MSIS_RANGE_MIN;
            rangeMax = RCC_MSIS_RANGE_MAX;
        }

        if( ( rangeMin <= rangeIdx ) &&
            ( rangeMax >= rangeIdx )    )
        {
            *clkFreq = MSIRangeTable[ rangeIdx ];

            returnState = RCC_REQUEST_OK;
        }
        else
        {
            /* Reserved range value */
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
 * \brief Activation of 48 MHz High Speed Internal (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also on MCUs without HSI48).
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Active( void )
{
#if defined(RCC_CRRCR_HSI48ON)
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_HSI48, RCC_FUNCTION_ACTIVE ) );
#else
    return ( RCC_REQUEST_ERROR );
#endif /* RCC_CRRCR_HSI48ON */
}


/**
 * \brief De-activation of 48 MHz High Speed Internal (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also on MCUs without HSI48).
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Inactive( void )
{
#if defined(RCC_CRRCR_HSI48ON)
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_HSI48, RCC_FUNCTION_INACTIVE ) );
#else
    return ( RCC_REQUEST_ERROR );
#endif /* RCC_CRRCR_HSI48ON */
}


/**
 * \brief Reading status of 48 MHz High Speed Internal (HSI48) block
 *
 * \param retState [out]: Pointer to actual status value (active = enabled and ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also on MCUs without HSI48).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48State( rcc_FunctionState_t * const retState )
{
#if defined(RCC_CRRCR_HSI48ON)
    return ( Rcc_ClkSrc_Get_OscEnable( RCC_CLKSRC_OSC_HSI48, retState ) );
#else
    (void)retState;

    return ( RCC_REQUEST_ERROR );
#endif /* RCC_CRRCR_HSI48ON */
}


/**
 * \brief Reading 48 MHz High Speed Internal (HSI48) frequency
 *
 * \param clkFreq [out]: Frequency of HSI48 clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also on MCUs without HSI48).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

#if defined(RCC_CRRCR_HSI48ON)
    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = HSI48_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }
#else
    (void)clkFreq;
#endif /* RCC_CRRCR_HSI48ON */

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
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_LSE, RCC_FUNCTION_ACTIVE ) );
}


/**
 * \brief De-activation of Low Speed External (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseInactive( void )
{
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_LSE, RCC_FUNCTION_INACTIVE ) );
}


/**
 * \brief Reading status of Low Speed External (LSE) block
 *
 * \param retState [out]: Pointer to actual status value (active = enabled and ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LseState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscEnable( RCC_CLKSRC_OSC_LSE, retState ) );
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
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_LSI, RCC_FUNCTION_ACTIVE ) );
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
    return ( Rcc_ClkSrc_Set_OscEnable( RCC_CLKSRC_OSC_LSI, RCC_FUNCTION_INACTIVE ) );
}


/**
 * \brief Reading status of Low Speed Internal (LSI) block
 *
 * \param retState [out]: Pointer to actual status value (active = enabled and ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscEnable( RCC_CLKSRC_OSC_LSI, retState ) );
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
 * \brief Enables or disables an oscillator and waits for its ready flag.
 *
 * \param oscId    [in]: Oscillator identification (valid)
 * \param oscState [in]: Required state (active - ready flag set, inactive - cleared)
 *
 * \return Returns "OK" if the ready flag reached the required state in time,
 *         otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Set_OscEnable( rcc_ClkSrcOscId_t oscId, rcc_FunctionState_t oscState )
{
    const rcc_ClkSrcOscConfig_t * const oscConfig = &rcc_ClkSrcOscConfig[ oscId ];
    uint32_t                            readyVal  = 0u;

    if( RCC_FUNCTION_ACTIVE == oscState )
    {
        Rcc_Set_RegBit( oscConfig->RegId, oscConfig->EnableMask );
        readyVal = oscConfig->ReadyMask;
    }
    else
    {
        Rcc_Reset_RegBit( oscConfig->RegId, oscConfig->EnableMask );
        readyVal = 0u;
    }

    return ( Rcc_ClkSrc_Wait_Flag( oscConfig->RegId, oscConfig->ReadyMask, readyVal, oscConfig->Timeout ) );
}


/**
 * \brief Reads state of an oscillator (active only if enabled and ready).
 *
 * \param oscId     [in]: Oscillator identification (valid)
 * \param retState [out]: Pointer to store oscillator state. Must not be NULL.
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Get_OscEnable( rcc_ClkSrcOscId_t oscId, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const rcc_ClkSrcOscConfig_t * const oscConfig  = &rcc_ClkSrcOscConfig[ oscId ];
        const uint32_t                      readyState = Rcc_Get_RegBit( oscConfig->RegId, oscConfig->ReadyMask );
        const uint32_t                      enabled    = Rcc_Get_RegBit( oscConfig->RegId, oscConfig->EnableMask );

        if( ( 0u != readyState ) &&
            ( 0u != enabled    )    )
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
        const uint32_t regValue = Rcc_Get_RegVal( regId, flagMask );

        if( expectedVal == regValue )
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

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
