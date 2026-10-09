/**
 * \author Mr.Nobody
 * \file Rcc_ClkSrc.c
 * \ingroup Rcc
 * \brief Rcc module ClkSrc component functionality.
 *
 * Oscillators of STM32U5: High Speed External (HSE), 16 MHz High Speed
 * Internal (HSI16), 48 MHz High Speed Internal (HSI48), Multi-Speed Internal
 * oscillator outputs MSIS (system clock) and MSIK (kernel clock), Low Speed
 * Internal (LSI) and Low Speed External (LSE).
 *
 * \note  LSI and LSE are controlled by RCC backup domain control register
 *        (BDCR), which is write protected by PWR (DBP). The protection is
 *        released by \ref Rcc_ClkSrc_Set_BkUpAccess (PWR has no MCAL module -
 *        RCC accesses its LL functions directly, see Rcc.c).
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkSrc.h"                     /* Self include                   */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Stm32_rcc.h"                      /* RCC module RAL layer           */
#include "Stm32_bus.h"                      /* Clock buses RAL layer          */
#include "Stm32_pwr.h"                      /* PWR RAL layer (backup domain)  */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_OSC_TIMEOUT_RAW             ( 0x84FCB )

/** Value of flag function result when the flag is cleared */
#define RCC_OSC_FLAG_CLEARED            ( 0u )

/** HSE divider of RTC clock source (fixed) */
#define RCC_OSC_HSE_RTC_DIV             ( 32u )

/** HSI48 divider of RNG clock source (fixed) */
#define RCC_OSC_HSI48_RNG_DIV           ( 2u )

/** Count of MSI ranges selectable by divider of 48 MHz MSI reference */
#define RCC_OSC_MSI_DIV_CNT             ( sizeof( rcc_ClkSrc_MsiDivLut ) / sizeof( rcc_ClkSrc_MsiDivLut[ 0u ] ) )

/** Count of MSI ranges */
#define RCC_OSC_MSI_RANGE_CNT           ( 16u )

/** Divider of oscillators without divider */
#define RCC_OSC_DIV_NONE                ( 1u )

/** LSI prescaler (LSIPREDIV) divider */
#define RCC_OSC_LSI_PREDIV              ( 128u )

/* ============================== TYPEDEFS ================================== */

/** \brief LL function returning flag state (0 - cleared, other - set) */
typedef uint32_t ( *rcc_ClkSrc_FlagFunc_t )( void );

/** \brief LL function without parameters and return value (enable / disable) */
typedef void ( *rcc_ClkSrc_CtrlFunc_t )( void );

/** \brief MSI range selected by divider of 48 MHz MSI reference */
typedef struct
{
    rcc_OscDiv_t Divider;   /**< Divider of 48 MHz MSI reference           */
    uint32_t     RangeIdx;  /**< MSI range index (MSISRANGE / MSIKRANGE)    */
}   rcc_ClkSrc_MsiDiv_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_ClkSrc_Wait_Flag      ( rcc_ClkSrc_FlagFunc_t flagFunc, rcc_FlagState_t expectedState );
static rcc_RequestState_t Rcc_ClkSrc_Set_Osc        ( rcc_ClkSrc_CtrlFunc_t ctrlFunc, rcc_ClkSrc_FlagFunc_t readyFunc, rcc_FlagState_t expectedState );
static rcc_RequestState_t Rcc_ClkSrc_Get_OscState   ( rcc_ClkSrc_FlagFunc_t enableFunc, rcc_ClkSrc_FlagFunc_t readyFunc, rcc_FunctionState_t * const retState );
static rcc_RequestState_t Rcc_ClkSrc_Get_MsiRangeIdx( rcc_OscDiv_t oscDiv, uint32_t * const rangeIdx );
static rcc_RequestState_t Rcc_ClkSrc_Get_MsiDivider ( uint32_t rangeIdx, rcc_OscDiv_t * const oscDiv );
static uint32_t           Rcc_ClkSrc_Is_HseEnabled  ( void );
static uint32_t           Rcc_ClkSrc_Is_HsiEnabled  ( void );
static uint32_t           Rcc_ClkSrc_Is_Hsi48Enabled( void );
static uint32_t           Rcc_ClkSrc_Is_MsisEnabled ( void );
static uint32_t           Rcc_ClkSrc_Is_MsikEnabled ( void );
static uint32_t           Rcc_ClkSrc_Is_LseEnabled  ( void );
static uint32_t           Rcc_ClkSrc_Is_LsiEnabled  ( void );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Local storing of High Speed External (HSE) source value in Hz */
static rcc_FreqHz_t                 rcc_HseFreqHz = 0u;

/** \brief MSI ranges derived from 48 MHz MSI reference (MSIRC0 / MSIRC1 / MSIRC3) */
static const rcc_ClkSrc_MsiDiv_t    rcc_ClkSrc_MsiDivLut[] =
{
    { .Divider =   1u, .RangeIdx =  0u }, /**< 48 MHz     */
    { .Divider =   2u, .RangeIdx =  1u }, /**< 24 MHz     */
    { .Divider =   3u, .RangeIdx =  2u }, /**< 16 MHz     */
    { .Divider =   4u, .RangeIdx =  3u }, /**< 12 MHz     */
    { .Divider =  12u, .RangeIdx =  4u }, /**< 4 MHz      */
    { .Divider =  24u, .RangeIdx =  5u }, /**< 2 MHz      */
    { .Divider =  36u, .RangeIdx =  6u }, /**< 1.33 MHz   */
    { .Divider =  48u, .RangeIdx =  7u }, /**< 1 MHz      */
    { .Divider = 120u, .RangeIdx = 12u }, /**< 400 kHz    */
    { .Divider = 240u, .RangeIdx = 13u }, /**< 200 kHz    */
    { .Divider = 360u, .RangeIdx = 14u }, /**< 133 kHz    */
    { .Divider = 480u, .RangeIdx = 15u }, /**< 100 kHz    */
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
 */
void Rcc_ClkSrc_Deinit( void )
{
    return;
}


/**
 * \brief Main task of module Rcc_ClkSrc
 */
void Rcc_ClkSrc_Task( void )
{
    return;
}

/*------------------------ Clock sources configuration -----------------------*/

/**
 * \brief Activation of High Speed External (HSE) block
 *
 * HSE is switched off first (bypass and clock type can be changed only while
 * HSE is off), configured and started.
 *
 * \param hseType [in]: Type of clock source, value from \ref rcc_HseType_t.
 *                      \ref RCC_HSE_TYPE_NONE leaves HSE switched off.
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
        /* HSE de-activation was not successful. */
    }
    else if( RCC_HSE_TYPE_NONE == hseType )
    {
        /* HSE is not used */
        returnState = RCC_REQUEST_OK;
    }
    else if( RCC_HSE_TYPE_CRYSTAL == hseType )
    {
        LL_RCC_HSE_DisableBypass();

        returnState = Rcc_ClkSrc_Set_Osc( LL_RCC_HSE_Enable, LL_RCC_HSE_IsReady, RCC_FLAG_ACTIVE );
    }
    else if( RCC_HSE_TYPE_SIG_ANALOG_IN == hseType )
    {
        LL_RCC_HSE_EnableBypass();
        LL_RCC_HSE_SetClockMode( LL_RCC_HSE_ANALOG_MODE );

        returnState = Rcc_ClkSrc_Set_Osc( LL_RCC_HSE_Enable, LL_RCC_HSE_IsReady, RCC_FLAG_ACTIVE );
    }
    else if( RCC_HSE_TYPE_SIG_DIGITAL_IN == hseType )
    {
        LL_RCC_HSE_EnableBypass();
        LL_RCC_HSE_SetClockMode( LL_RCC_HSE_DIGITAL_MODE );

        returnState = Rcc_ClkSrc_Set_Osc( LL_RCC_HSE_Enable, LL_RCC_HSE_IsReady, RCC_FLAG_ACTIVE );
    }
    else
    {
        /* Unknown HSE type */
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
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_HSE_Disable, LL_RCC_HSE_IsReady, RCC_FLAG_INACTIVE ) );
}


/**
 * \brief Reading status of High Speed External (HSE) block
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_HseEnabled, LL_RCC_HSE_IsReady, retState ) );
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
 * \param clkFreq [out]: Pointer to store frequency of HSE clock in Hz. Must not be NULL.
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
 * \brief Reading High Speed External (HSE) frequency divided by 32 (RTC clock source)
 *
 * \param clkFreq [out]: Pointer to store frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_HseDiv32Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = rcc_HseFreqHz / RCC_OSC_HSE_RTC_DIV;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of 16 MHz High Speed Internal (HSI16) oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi16Active( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_HSI_Enable, LL_RCC_HSI_IsReady, RCC_FLAG_ACTIVE ) );
}


/**
 * \brief De-activation of 16 MHz High Speed Internal (HSI16) oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi16Inactive( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_HSI_Disable, LL_RCC_HSI_IsReady, RCC_FLAG_INACTIVE ) );
}


/**
 * \brief Reading status of 16 MHz High Speed Internal (HSI16) oscillator
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi16State( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_HsiEnabled, LL_RCC_HSI_IsReady, retState ) );
}


/**
 * \brief Reading 16 MHz High Speed Internal (HSI16) oscillator frequency
 *
 * \param clkFreq [out]: Pointer to store frequency of HSI16 clock in Hz. Must not be NULL.
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
 * \brief Activation of 48 MHz High Speed Internal (HSI48) oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Active( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_HSI48_Enable, LL_RCC_HSI48_IsReady, RCC_FLAG_ACTIVE ) );
}


/**
 * \brief De-activation of 48 MHz High Speed Internal (HSI48) oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_Hsi48Inactive( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_HSI48_Disable, LL_RCC_HSI48_IsReady, RCC_FLAG_INACTIVE ) );
}


/**
 * \brief Reading status of 48 MHz High Speed Internal (HSI48) oscillator
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48State( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_Hsi48Enabled, LL_RCC_HSI48_IsReady, retState ) );
}


/**
 * \brief Reading 48 MHz High Speed Internal (HSI48) oscillator frequency
 *
 * \param clkFreq [out]: Pointer to store frequency of HSI48 clock in Hz. Must not be NULL.
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
 * \brief Reading 48 MHz High Speed Internal (HSI48) oscillator frequency divided by 2 (RNG clock source)
 *
 * \param clkFreq [out]: Pointer to store frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_Hsi48Div2Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = HSI48_VALUE / RCC_OSC_HSI48_RNG_DIV;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of Multi-Speed Internal oscillator system clock output (MSIS)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsisActive( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_MSIS_Enable, LL_RCC_MSIS_IsReady, RCC_FLAG_ACTIVE ) );
}


/**
 * \brief De-activation of Multi-Speed Internal oscillator system clock output (MSIS)
 *
 * \warning MSIS used as system clock or PLL source must not be deactivated.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsisInactive( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_MSIS_Disable, LL_RCC_MSIS_IsReady, RCC_FLAG_INACTIVE ) );
}


/**
 * \brief Reading status of Multi-Speed Internal oscillator system clock output (MSIS)
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsisState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_MsisEnabled, LL_RCC_MSIS_IsReady, retState ) );
}


/**
 * \brief Reading Multi-Speed Internal oscillator system clock output (MSIS) frequency
 *
 * Range is taken from ICSCR1 (MSIRGSEL = 1) or from the range after Standby
 * mode in CSR (MSIRGSEL = 0, reset state).
 *
 * \param clkFreq [out]: Pointer to store frequency of MSIS clock in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsisClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        const uint32_t rangeSel = LL_RCC_MSI_IsEnabledRangeSelect();
        uint32_t       rangeIdx = LL_RCC_MSIS_GetRangeAfterStandby() >> RCC_CSR_MSISSRANGE_Pos;

        if( RCC_OSC_FLAG_CLEARED != rangeSel )
        {
            rangeIdx = LL_RCC_MSIS_GetRange() >> RCC_ICSCR1_MSISRANGE_Pos;
        }
        else
        {
            /* Range after Standby is used (MSIRGSEL = 0) */
        }

        *clkFreq = MSIRangeTable[ rangeIdx ];

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Configures Multi-Speed Internal oscillator system clock output (MSIS) range
 *
 * \pre   MSIS is switched off or ready (range can not be changed while MSIS
 *        is starting); otherwise \ref RCC_REQUEST_ERROR and no register is changed.
 *
 * \param oscDiv [in]: Divider of 48 MHz MSI reference, see \ref rcc_OscDiv_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsisDiv( rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           rangeIdx    = 0u;
    const uint32_t     msisOn      = Rcc_ClkSrc_Is_MsisEnabled();
    const uint32_t     msisReady   = LL_RCC_MSIS_IsReady();

    returnState = Rcc_ClkSrc_Get_MsiRangeIdx( oscDiv, &rangeIdx );

    if( ( RCC_REQUEST_OK       == returnState ) &&
        ( RCC_OSC_FLAG_CLEARED != msisOn      ) &&
        ( RCC_OSC_FLAG_CLEARED == msisReady   )    )
    {
        /* MSIS is starting - range can not be changed */
        returnState = RCC_REQUEST_ERROR;
    }
    else if( RCC_REQUEST_OK == returnState )
    {
        const uint32_t llRange = rangeIdx << RCC_ICSCR1_MSISRANGE_Pos;

        LL_RCC_MSI_EnableRangeSelection();
        LL_RCC_MSIS_SetRange( llRange );

        returnState = RCC_REQUEST_ERROR;

        for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t actualRange = LL_RCC_MSIS_GetRange();
            const uint32_t rangeSel    = LL_RCC_MSI_IsEnabledRangeSelect();

            if( ( llRange              == actualRange ) &&
                ( RCC_OSC_FLAG_CLEARED != rangeSel    )    )
            {
                returnState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Range has not been applied yet */
            }
        }

        if( ( RCC_REQUEST_OK       == returnState ) &&
            ( RCC_OSC_FLAG_CLEARED != msisOn      )    )
        {
            /* Oscillator is running - wait until it is stable with the new range */
            returnState = Rcc_ClkSrc_Wait_Flag( LL_RCC_MSIS_IsReady, RCC_FLAG_ACTIVE );
        }
        else
        {
            /* Oscillator is off or range was not applied */
        }
    }
    else
    {
        /* Unsupported divider */
    }

    return ( returnState );
}


/**
 * \brief Reads Multi-Speed Internal oscillator system clock output (MSIS) divider
 *
 * \param oscDiv [out]: Pointer to store divider of 48 MHz MSI reference. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for ranges of 3.072 MHz MSI reference).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsisDiv( rcc_OscDiv_t * const oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    const uint32_t     rangeSel    = LL_RCC_MSI_IsEnabledRangeSelect();
    uint32_t           rangeIdx    = LL_RCC_MSIS_GetRangeAfterStandby() >> RCC_CSR_MSISSRANGE_Pos;

    if( RCC_OSC_FLAG_CLEARED != rangeSel )
    {
        rangeIdx = LL_RCC_MSIS_GetRange() >> RCC_ICSCR1_MSISRANGE_Pos;
    }
    else
    {
        /* Range after Standby is used (MSIRGSEL = 0) */
    }

    returnState = Rcc_ClkSrc_Get_MsiDivider( rangeIdx, oscDiv );

    return ( returnState );
}


/**
 * \brief Activation of Multi-Speed Internal oscillator kernel clock output (MSIK)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsikActive( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_MSIK_Enable, LL_RCC_MSIK_IsReady, RCC_FLAG_ACTIVE ) );
}


/**
 * \brief De-activation of Multi-Speed Internal oscillator kernel clock output (MSIK)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsikInactive( void )
{
    return ( Rcc_ClkSrc_Set_Osc( LL_RCC_MSIK_Disable, LL_RCC_MSIK_IsReady, RCC_FLAG_INACTIVE ) );
}


/**
 * \brief Reading status of Multi-Speed Internal oscillator kernel clock output (MSIK)
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsikState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_MsikEnabled, LL_RCC_MSIK_IsReady, retState ) );
}


/**
 * \brief Reading Multi-Speed Internal oscillator kernel clock output (MSIK) frequency
 *
 * \param clkFreq [out]: Pointer to store frequency of MSIK clock in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsikClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        const uint32_t rangeSel = LL_RCC_MSI_IsEnabledRangeSelect();
        uint32_t       rangeIdx = LL_RCC_MSIK_GetRangeAfterStandby() >> RCC_CSR_MSIKSRANGE_Pos;

        if( RCC_OSC_FLAG_CLEARED != rangeSel )
        {
            rangeIdx = LL_RCC_MSIK_GetRange() >> RCC_ICSCR1_MSIKRANGE_Pos;
        }
        else
        {
            /* Range after Standby is used (MSIRGSEL = 0) */
        }

        *clkFreq = MSIRangeTable[ rangeIdx ];

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Configures Multi-Speed Internal oscillator kernel clock output (MSIK) range
 *
 * \pre   MSIK is switched off or ready; otherwise \ref RCC_REQUEST_ERROR and no
 *        register is changed.
 *
 * \param oscDiv [in]: Divider of 48 MHz MSI reference, see \ref rcc_OscDiv_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_MsikDiv( rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           rangeIdx    = 0u;
    const uint32_t     msikOn      = Rcc_ClkSrc_Is_MsikEnabled();
    const uint32_t     msikReady   = LL_RCC_MSIK_IsReady();

    returnState = Rcc_ClkSrc_Get_MsiRangeIdx( oscDiv, &rangeIdx );

    if( ( RCC_REQUEST_OK       == returnState ) &&
        ( RCC_OSC_FLAG_CLEARED != msikOn      ) &&
        ( RCC_OSC_FLAG_CLEARED == msikReady   )    )
    {
        /* MSIK is starting - range can not be changed */
        returnState = RCC_REQUEST_ERROR;
    }
    else if( RCC_REQUEST_OK == returnState )
    {
        const uint32_t llRange = rangeIdx << RCC_ICSCR1_MSIKRANGE_Pos;

        LL_RCC_MSI_EnableRangeSelection();
        LL_RCC_MSIK_SetRange( llRange );

        returnState = RCC_REQUEST_ERROR;

        for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t actualRange = LL_RCC_MSIK_GetRange();
            const uint32_t rangeSel    = LL_RCC_MSI_IsEnabledRangeSelect();

            if( ( llRange              == actualRange ) &&
                ( RCC_OSC_FLAG_CLEARED != rangeSel    )    )
            {
                returnState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Range has not been applied yet */
            }
        }

        if( ( RCC_REQUEST_OK       == returnState ) &&
            ( RCC_OSC_FLAG_CLEARED != msikOn      )    )
        {
            /* Oscillator is running - wait until it is stable with the new range */
            returnState = Rcc_ClkSrc_Wait_Flag( LL_RCC_MSIK_IsReady, RCC_FLAG_ACTIVE );
        }
        else
        {
            /* Oscillator is off or range was not applied */
        }
    }
    else
    {
        /* Unsupported divider */
    }

    return ( returnState );
}


/**
 * \brief Reads Multi-Speed Internal oscillator kernel clock output (MSIK) divider
 *
 * \param oscDiv [out]: Pointer to store divider of 48 MHz MSI reference. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for ranges of 3.072 MHz MSI reference).
 */
rcc_RequestState_t Rcc_ClkSrc_Get_MsikDiv( rcc_OscDiv_t * const oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    const uint32_t     rangeSel    = LL_RCC_MSI_IsEnabledRangeSelect();
    uint32_t           rangeIdx    = LL_RCC_MSIK_GetRangeAfterStandby() >> RCC_CSR_MSIKSRANGE_Pos;

    if( RCC_OSC_FLAG_CLEARED != rangeSel )
    {
        rangeIdx = LL_RCC_MSIK_GetRange() >> RCC_ICSCR1_MSIKRANGE_Pos;
    }
    else
    {
        /* Range after Standby is used (MSIRGSEL = 0) */
    }

    returnState = Rcc_ClkSrc_Get_MsiDivider( rangeIdx, oscDiv );

    return ( returnState );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Activation of Low Speed External (LSE) oscillator
 *
 * Backup domain write protection is released before LSE is enabled.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseActive( void )
{
    rcc_RequestState_t returnState = Rcc_ClkSrc_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
        returnState = Rcc_ClkSrc_Set_Osc( LL_RCC_LSE_Enable, LL_RCC_LSE_IsReady, RCC_FLAG_ACTIVE );
    }
    else
    {
        /* Backup domain is write protected */
    }

    return ( returnState );
}


/**
 * \brief De-activation of Low Speed External (LSE) oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LseInactive( void )
{
    rcc_RequestState_t returnState = Rcc_ClkSrc_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
        returnState = Rcc_ClkSrc_Set_Osc( LL_RCC_LSE_Disable, LL_RCC_LSE_IsReady, RCC_FLAG_INACTIVE );
    }
    else
    {
        /* Backup domain is write protected */
    }

    return ( returnState );
}


/**
 * \brief Reading status of Low Speed External (LSE) oscillator
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LseState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_LseEnabled, LL_RCC_LSE_IsReady, retState ) );
}


/**
 * \brief Returns value of Low Speed External (LSE) frequency
 *
 * \param lseClk [out]: Pointer to store frequency of LSE clock in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
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
 * \brief Activation of Low Speed Internal (LSI) oscillator
 *
 * Backup domain write protection is released before LSI is enabled.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LsiActive( void )
{
    rcc_RequestState_t returnState = Rcc_ClkSrc_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
        returnState = Rcc_ClkSrc_Set_Osc( LL_RCC_LSI_Enable, LL_RCC_LSI_IsReady, RCC_FLAG_ACTIVE );
    }
    else
    {
        /* Backup domain is write protected */
    }

    return ( returnState );
}


/**
 * \brief De-activation of Low Speed Internal (LSI) oscillator
 *
 * \note  LSI used by running independent watchdog is kept running by hardware.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LsiInactive( void )
{
    rcc_RequestState_t returnState = Rcc_ClkSrc_Set_BkUpAccess();

    if( RCC_REQUEST_OK == returnState )
    {
        LL_RCC_LSI_Disable();
    }
    else
    {
        /* Backup domain is write protected */
    }

    return ( returnState );
}


/**
 * \brief Reading status of Low Speed Internal (LSI) oscillator
 *
 * \param retState [out]: Pointer to store actual status (enabled and ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiState( rcc_FunctionState_t * const retState )
{
    return ( Rcc_ClkSrc_Get_OscState( Rcc_ClkSrc_Is_LsiEnabled, LL_RCC_LSI_IsReady, retState ) );
}


/**
 * \brief Returns value of Low Speed Internal (LSI) frequency (after LSI prescaler)
 *
 * \param lsiClk [out]: Pointer to store frequency of LSI clock in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiClk( rcc_FreqHz_t * const lsiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    rcc_OscDiv_t       lsiDiv      = RCC_OSC_DIV_NONE;

    returnState = Rcc_ClkSrc_Get_LsiDiv( &lsiDiv );

    if( ( RCC_NULL_PTR   != lsiClk      ) &&
        ( RCC_REQUEST_OK == returnState )    )
    {
        *lsiClk = LSI_VALUE / lsiDiv;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Configures Low Speed Internal (LSI) prescaler.
 *
 * \pre   LSI is switched off (prescaler can be changed only while LSI is
 *        disabled), unless the divider is already selected; otherwise
 *        \ref RCC_REQUEST_ERROR and no register is changed.
 *
 * \param oscDiv [in]: Divider value 1 or 128
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_LsiDiv( rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           llDivider   = LL_RCC_LSI_DIV_1;
    const uint32_t     lsiOn       = Rcc_ClkSrc_Is_LsiEnabled();
    const uint32_t     lsiReady    = LL_RCC_LSI_IsReady();
    const uint32_t     actualDiv   = LL_RCC_LSI_GetPrescaler();

    if( RCC_OSC_DIV_NONE == oscDiv )
    {
        llDivider   = LL_RCC_LSI_DIV_1;
        returnState = RCC_REQUEST_OK;
    }
    else if( RCC_OSC_LSI_PREDIV == oscDiv )
    {
        llDivider   = LL_RCC_LSI_DIV_128;
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        /* Unsupported divider */
        returnState = RCC_REQUEST_ERROR;
    }

    if( ( RCC_REQUEST_OK != returnState ) ||
        ( llDivider      == actualDiv   )    )
    {
        /* Unsupported divider or divider already selected */
    }
    else if( ( RCC_OSC_FLAG_CLEARED != lsiOn    ) ||
             ( RCC_OSC_FLAG_CLEARED != lsiReady )    )
    {
        /* LSI is running - prescaler can not be changed */
        returnState = RCC_REQUEST_ERROR;
    }
    else
    {
        returnState = Rcc_ClkSrc_Set_BkUpAccess();

        if( RCC_REQUEST_OK == returnState )
        {
            LL_RCC_LSI_SetPrescaler( llDivider );

            returnState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = LL_RCC_LSI_GetPrescaler();

                if( llDivider == regValue )
                {
                    returnState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Prescaler has not been applied yet */
                }
            }
        }
        else
        {
            /* Backup domain is write protected */
        }
    }

    return ( returnState );
}


/**
 * \brief Reads Low Speed Internal (LSI) prescaler.
 *
 * \param oscDiv [out]: Pointer to store divider value (1 or 128). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Get_LsiDiv( rcc_OscDiv_t * const oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != oscDiv )
    {
        const uint32_t llDivider = LL_RCC_LSI_GetPrescaler();

        if( LL_RCC_LSI_DIV_128 == llDivider )
        {
            *oscDiv = RCC_OSC_LSI_PREDIV;
        }
        else
        {
            *oscDiv = RCC_OSC_DIV_NONE;
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
 * \brief Releases backup domain write protection (RCC BDCR register).
 *
 * PWR bus clock is enabled and PWR DBP bit is set (verified by read-back).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkSrc_Set_BkUpAccess( void )
{
    LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_PWR );
    LL_PWR_EnableBkUpAccess();

    return ( Rcc_ClkSrc_Wait_Flag( LL_PWR_IsEnabledBkUpAccess, RCC_FLAG_ACTIVE ) );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Waits until flag reaches expected state.
 *
 * \param flagFunc      [in]: LL function returning flag state
 * \param expectedState [in]: Expected flag state
 *
 * \return Returns "OK" if the flag reached the state before timeout, otherwise
 *         returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Wait_Flag( rcc_ClkSrc_FlagFunc_t flagFunc, rcc_FlagState_t expectedState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; RCC_OSC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t  regValue  = flagFunc();
        rcc_FlagState_t flagState = RCC_FLAG_INACTIVE;

        if( RCC_OSC_FLAG_CLEARED != regValue )
        {
            flagState = RCC_FLAG_ACTIVE;
        }
        else
        {
            flagState = RCC_FLAG_INACTIVE;
        }

        if( expectedState == flagState )
        {
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Flag has not reached expected state yet, keep return state as error */
            returnState = RCC_REQUEST_ERROR;
        }
    }

    return ( returnState );
}


/**
 * \brief Enables / disables oscillator and waits for its ready flag.
 *
 * \param ctrlFunc      [in]: LL enable or disable function of the oscillator
 * \param readyFunc     [in]: LL ready flag function of the oscillator
 * \param expectedState [in]: Expected ready flag state (active after enable)
 *
 * \return Returns "OK" if the ready flag reached the state, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Set_Osc( rcc_ClkSrc_CtrlFunc_t ctrlFunc, rcc_ClkSrc_FlagFunc_t readyFunc, rcc_FlagState_t expectedState )
{
    ctrlFunc();

    return ( Rcc_ClkSrc_Wait_Flag( readyFunc, expectedState ) );
}


/**
 * \brief Reads oscillator state - active if the oscillator is enabled and ready.
 *
 * \param enableFunc [in]: Function returning oscillator enable bit
 * \param readyFunc  [in]: LL ready flag function of the oscillator
 * \param retState  [out]: Pointer to store oscillator state. Must not be NULL.
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Get_OscState( rcc_ClkSrc_FlagFunc_t enableFunc, rcc_ClkSrc_FlagFunc_t readyFunc, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        const uint32_t enableState = enableFunc();
        const uint32_t readyState  = readyFunc();

        if( ( RCC_OSC_FLAG_CLEARED != enableState ) &&
            ( RCC_OSC_FLAG_CLEARED != readyState  )    )
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
 * \brief Converts divider of 48 MHz MSI reference to MSI range index.
 *
 * \param oscDiv    [in]: Divider of 48 MHz MSI reference
 * \param rangeIdx [out]: Pointer to store MSI range index
 *
 * \return Returns "OK" if the divider is supported, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Get_MsiRangeIdx( rcc_OscDiv_t oscDiv, uint32_t * const rangeIdx )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    for( uint32_t lutIdx = 0u; RCC_OSC_MSI_DIV_CNT > lutIdx; lutIdx++ )
    {
        if( oscDiv == rcc_ClkSrc_MsiDivLut[ lutIdx ].Divider )
        {
            *rangeIdx   = rcc_ClkSrc_MsiDivLut[ lutIdx ].RangeIdx;
            returnState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Continue with next divider */
        }
    }

    return ( returnState );
}


/**
 * \brief Converts MSI range index to divider of 48 MHz MSI reference.
 *
 * \param rangeIdx [in]: MSI range index
 * \param oscDiv  [out]: Pointer to store divider. Must not be NULL.
 *
 * \return Returns "OK" if the range is derived from 48 MHz MSI reference,
 *         otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkSrc_Get_MsiDivider( uint32_t rangeIdx, rcc_OscDiv_t * const oscDiv )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != oscDiv )
    {
        for( uint32_t lutIdx = 0u; RCC_OSC_MSI_DIV_CNT > lutIdx; lutIdx++ )
        {
            if( rangeIdx == rcc_ClkSrc_MsiDivLut[ lutIdx ].RangeIdx )
            {
                *oscDiv     = rcc_ClkSrc_MsiDivLut[ lutIdx ].Divider;
                returnState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next range */
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
 * \brief Returns HSE enable bit (HSEON).
 *
 * \return Value of HSEON bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_HseEnabled( void )
{
    return ( READ_BIT( RCC->CR, RCC_CR_HSEON ) );
}


/**
 * \brief Returns HSI16 enable bit (HSION).
 *
 * \return Value of HSION bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_HsiEnabled( void )
{
    return ( READ_BIT( RCC->CR, RCC_CR_HSION ) );
}


/**
 * \brief Returns HSI48 enable bit (HSI48ON).
 *
 * \return Value of HSI48ON bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_Hsi48Enabled( void )
{
    return ( READ_BIT( RCC->CR, RCC_CR_HSI48ON ) );
}


/**
 * \brief Returns MSIS enable bit (MSISON).
 *
 * \return Value of MSISON bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_MsisEnabled( void )
{
    return ( READ_BIT( RCC->CR, RCC_CR_MSISON ) );
}


/**
 * \brief Returns MSIK enable bit (MSIKON).
 *
 * \return Value of MSIKON bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_MsikEnabled( void )
{
    return ( READ_BIT( RCC->CR, RCC_CR_MSIKON ) );
}


/**
 * \brief Returns LSE enable bit (LSEON).
 *
 * \return Value of LSEON bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_LseEnabled( void )
{
    return ( READ_BIT( RCC->BDCR, RCC_BDCR_LSEON ) );
}


/**
 * \brief Returns LSI enable bit (LSION).
 *
 * \return Value of LSION bit (0 - disabled).
 */
static uint32_t Rcc_ClkSrc_Is_LsiEnabled( void )
{
    return ( READ_BIT( RCC->BDCR, RCC_BDCR_LSION ) );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
