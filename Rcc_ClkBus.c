/**
 * \author Mr.Nobody
 * \file Rcc_ClkBus.c
 * \ingroup Rcc
 * \brief Rcc module ClkBus component functionality.
 *
 * System clock (SYSCLK) source selection (MSIS, HSI16, HSE, PLL1 output R)
 * and dividers of AHB, APB1, APB2 and APB3 buses of STM32U5.
 *
 * \note  Switching the system clock to PLL1 waits for the embedded power
 *        distribution booster (EPOD) when the booster is enabled (voltage
 *        range 1 and 2). PWR has no MCAL module - booster flags are read by
 *        PWR LL functions directly (see Rcc.c).
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkBus.h"                     /* Self include                   */
#include "Rcc_ClkSrc.h"                     /* Clock source module            */
#include "Rcc_Pll.h"                        /* PLL module                     */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Stm32_pwr.h"                      /* PWR RAL layer (EPOD booster)   */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_CLKBUS_TIMEOUT_RAW              ( 0x84FCB )

/** Value of flag function result when the flag is cleared */
#define RCC_CLKBUS_FLAG_CLEARED             ( 0u )

/** Multiplier of timer kernel clock for APB prescaler other than 1 */
#define RCC_CLKBUS_TIM_CLK_MULT             ( 2u )

/** Divider of HCLK for SysTick external reference clock */
#define RCC_CLKBUS_SYSTICK_HCLK_DIV         ( 8u )

/* ============================== TYPEDEFS ================================== */

/** \brief Function returning frequency of system clock source */
typedef rcc_RequestState_t ( *rcc_ClkBus_FreqFunc_t )( rcc_FreqHz_t * const clkFreq );

/** \brief System clock source configuration */
typedef struct
{
    uint32_t              SwValue;   /**< SW field value (LL_RCC_SYS_CLKSOURCE_xxx)          */
    uint32_t              SwsValue;  /**< SWS field value (LL_RCC_SYS_CLKSOURCE_STATUS_xxx)  */
    rcc_ClkBus_FreqFunc_t FreqFunc;  /**< Frequency of the source                            */
}   rcc_ClkBus_SysClkConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_FreqHz_t       Rcc_ClkBus_Get_TimClk         ( rcc_FreqHz_t hClk, rcc_FreqHz_t pClk );
static rcc_RequestState_t Rcc_ClkBus_Get_Pll1RClk       ( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_ClkBus_Wait_Booster       ( void );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief System clock sources, indexed by \ref rcc_SystemClkSrc_t */
static const rcc_ClkBus_SysClkConfig_t  rcc_ClkBus_SysClkLut[ RCC_SYSTEM_CLOCK_SOURCE_CNT ] =
{
    [RCC_SYSTEM_CLOCK_SOURCE_MSIS] = { .SwValue = LL_RCC_SYS_CLKSOURCE_MSIS, .SwsValue = LL_RCC_SYS_CLKSOURCE_STATUS_MSIS, .FreqFunc = Rcc_ClkSrc_Get_MsisClk  },
    [RCC_SYSTEM_CLOCK_SOURCE_HSI]  = { .SwValue = LL_RCC_SYS_CLKSOURCE_HSI,  .SwsValue = LL_RCC_SYS_CLKSOURCE_STATUS_HSI,  .FreqFunc = Rcc_ClkSrc_Get_Hsi16Clk },
    [RCC_SYSTEM_CLOCK_SOURCE_HSE]  = { .SwValue = LL_RCC_SYS_CLKSOURCE_HSE,  .SwsValue = LL_RCC_SYS_CLKSOURCE_STATUS_HSE,  .FreqFunc = Rcc_ClkSrc_Get_HseClk   },
    [RCC_SYSTEM_CLOCK_SOURCE_PLL]  = { .SwValue = LL_RCC_SYS_CLKSOURCE_PLL1, .SwsValue = LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, .FreqFunc = Rcc_ClkBus_Get_Pll1RClk },
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes clock buses module.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
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
 * Switching to PLL1 waits for EPOD booster ready flag if the booster is enabled.
 *
 * \param systemClkSource [in]: Selected clock source ID, value from \ref rcc_SystemClkSrc_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_SysClkSource( rcc_SystemClkSrc_t systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_SYSTEM_CLOCK_SOURCE_PLL == systemClkSource )
    {
        returnState = Rcc_ClkBus_Wait_Booster();
    }
    else if( RCC_SYSTEM_CLOCK_SOURCE_CNT > systemClkSource )
    {
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    if( RCC_REQUEST_OK == returnState )
    {
        const uint32_t swsValue = rcc_ClkBus_SysClkLut[ systemClkSource ].SwsValue;

        LL_RCC_SetSysClkSource( rcc_ClkBus_SysClkLut[ systemClkSource ].SwValue );

        returnState = RCC_REQUEST_ERROR;

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t clkSourceRet = LL_RCC_GetSysClkSource();

            if( swsValue == clkSourceRet )
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
        /* Incorrect source or booster not ready */
    }

    return ( returnState );
}


/**
 * \brief Read system clock source multiplexer.
 *
 * System clock source multiplexer is used for selection of main clock source.
 *
 * \param systemClkSource [out]: Pointer to store actual clock source ID. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_SysClkSource( rcc_SystemClkSrc_t * const systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != systemClkSource )
    {
        const uint32_t regValue = LL_RCC_GetSysClkSource();

        for( rcc_SystemClkSrc_t srcIdx = RCC_SYSTEM_CLOCK_SOURCE_MSIS; RCC_SYSTEM_CLOCK_SOURCE_CNT > srcIdx; srcIdx++ )
        {
            if( regValue == rcc_ClkBus_SysClkLut[ srcIdx ].SwsValue )
            {
                *systemClkSource = srcIdx;
                returnState      = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next source */
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
 * \brief Reading of system clock frequency (SYSCLK) in Hz
 *
 * \note This function reads real values from registers and calculate actual
 *       frequency.
 *
 * \param busClk [out]: Pointer to store SYSCLK value. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_SysClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState        = RCC_REQUEST_ERROR;
    rcc_SystemClkSrc_t systemClkSource = RCC_SYSTEM_CLOCK_SOURCE_MSIS;

    /* Read System Clock Multiplexer settings */
    retState = Rcc_ClkBus_Get_SysClkSource( &systemClkSource );

    if( ( RCC_REQUEST_OK == retState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        retState = rcc_ClkBus_SysClkLut[ systemClkSource ].FreqFunc( busClk );
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
 * \param dividerId [in]: Required AHB divider value, value from \ref rcc_AHB_Div_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_AHBDivider( rcc_AHB_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_SetAHBPrescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = LL_RCC_GetAHBPrescaler();

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
 * \brief Read AHB divider.
 *
 * \param dividerId [out]: Pointer to store AHB divider. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_AHBDivider( rcc_AHB_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_AHB_Div_t)LL_RCC_GetAHBPrescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading of High performance clock frequency (HCLK) in Hz
 *
 * \param busClk [out]: Pointer to store bus clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_AHBClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2  = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       systemClk  = 0u;
    rcc_AHB_Div_t      ahbDivider = RCC_AHB_DIVIDER_1;

    retState  = Rcc_ClkBus_Get_SysClk( &systemClk );
    retState2 = Rcc_ClkBus_Get_AHBDivider( &ahbDivider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
    {
        *busClk = __LL_RCC_CALC_HCLK_FREQ( systemClk, ahbDivider );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of AHB clock divided by 8 (SysTick external reference clock) in Hz
 *
 * \param busClk [out]: Pointer to store clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_HclkDiv8Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk     = 0u;

    retState = Rcc_ClkBus_Get_AHBClk( &hClk );

    if( ( RCC_REQUEST_OK == retState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk = hClk / RCC_CLKBUS_SYSTICK_HCLK_DIV;
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
 * \param dividerId [in]: Required APB1 divider, value from \ref rcc_APB1_Div_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB1Divider( rcc_APB1_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_SetAPB1Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = LL_RCC_GetAPB1Prescaler();

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
 * \param dividerId [out]: Pointer to store APB1 divider. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB1Divider( rcc_APB1_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_APB1_Div_t)LL_RCC_GetAPB1Prescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB1 clock bus
 *
 * \param busClk [out]: Pointer to store bus clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB1Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB1_Div_t     apb1Divider = RCC_APB1_DIVIDER_1;

    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB1Divider( &apb1Divider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
    {
        *busClk = __LL_RCC_CALC_PCLK1_FREQ( hClk, apb1Divider );
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
 * \param dividerId [in]: Required APB2 divider, value from \ref rcc_APB2_Div_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB2Divider( rcc_APB2_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_SetAPB2Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = LL_RCC_GetAPB2Prescaler();

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
 * \param dividerId [out]: Pointer to store APB2 divider. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB2Divider( rcc_APB2_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_APB2_Div_t)LL_RCC_GetAPB2Prescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB2 clock bus
 *
 * \param busClk [out]: Pointer to store bus clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB2Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB2_Div_t     apb2Divider = RCC_APB2_DIVIDER_1;

    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB2Divider( &apb2Divider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
    {
        *busClk = __LL_RCC_CALC_PCLK2_FREQ( hClk, apb2Divider );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure APB3 divider.
 *
 * Clock bus APB3 is clocked through APB3 divider from HCLK (AHB bus).
 *
 * \param dividerId [in]: Required APB3 divider, value from \ref rcc_APB3_Div_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB3Divider( rcc_APB3_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_SetAPB3Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = LL_RCC_GetAPB3Prescaler();

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
 * \brief Read APB3 divider.
 *
 * \param dividerId [out]: Pointer to store APB3 divider. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB3Divider( rcc_APB3_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_APB3_Div_t)LL_RCC_GetAPB3Prescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB3 clock bus
 *
 * \param busClk [out]: Pointer to store bus clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB3Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB3_Div_t     apb3Divider = RCC_APB3_DIVIDER_1;

    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB3Divider( &apb3Divider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
    {
        *busClk = __LL_RCC_CALC_PCLK3_FREQ( hClk, apb3Divider );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of kernel clock frequency of timers on APB1 bus
 *
 * \param timClk [out]: Pointer to store timer kernel clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB1TimClk( rcc_FreqHz_t * const timClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk     = 0u;
    rcc_FreqHz_t       pClk     = 0u;

    const rcc_RequestState_t hClkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t pClkState = Rcc_ClkBus_Get_APB1Clk( &pClk );

    if( ( RCC_REQUEST_OK == hClkState ) &&
        ( RCC_REQUEST_OK == pClkState ) &&
        ( RCC_NULL_PTR   != timClk    )    )
    {
        *timClk  = Rcc_ClkBus_Get_TimClk( hClk, pClk );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of kernel clock frequency of timers on APB2 bus
 *
 * \param timClk [out]: Pointer to store timer kernel clock frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB2TimClk( rcc_FreqHz_t * const timClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk     = 0u;
    rcc_FreqHz_t       pClk     = 0u;

    const rcc_RequestState_t hClkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t pClkState = Rcc_ClkBus_Get_APB2Clk( &pClk );

    if( ( RCC_REQUEST_OK == hClkState ) &&
        ( RCC_REQUEST_OK == pClkState ) &&
        ( RCC_NULL_PTR   != timClk    )    )
    {
        *timClk  = Rcc_ClkBus_Get_TimClk( hClk, pClk );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Calculates timer kernel clock of APB bus timers (RM0456, timer clock)
 *
 * - APB prescaler 1:     timer clock = PCLK
 * - APB prescaler > 1:   timer clock = 2 x PCLK
 *
 * \param hClk [in]: AHB clock frequency in Hz
 * \param pClk [in]: APB clock frequency in Hz
 *
 * \return Timer kernel clock frequency in Hz
 */
static rcc_FreqHz_t Rcc_ClkBus_Get_TimClk( rcc_FreqHz_t hClk, rcc_FreqHz_t pClk )
{
    rcc_FreqHz_t timClk = pClk;

    if( hClk == pClk )
    {
        /* APB prescaler 1 - timer clock is the APB clock */
        timClk = pClk;
    }
    else
    {
        timClk = RCC_CLKBUS_TIM_CLK_MULT * pClk;
    }

    return ( timClk );
}


/**
 * \brief Returns frequency of PLL1 output R (system clock source).
 *
 * \param clkFreq [out]: Pointer to store frequency in Hz
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkBus_Get_Pll1RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutR( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Waits for embedded power distribution booster (EPOD) when it is enabled.
 *
 * \return Returns "OK" if the booster is disabled or ready, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkBus_Wait_Booster( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;
    const uint32_t     boostActive = LL_PWR_IsEnabledEPODBooster();

    if( RCC_CLKBUS_FLAG_CLEARED != boostActive )
    {
        returnState = RCC_REQUEST_ERROR;

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t boostReady = LL_PWR_IsActiveFlag_BOOST();

            if( RCC_CLKBUS_FLAG_CLEARED != boostReady )
            {
                returnState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Booster is not ready yet */
            }
        }
    }
    else
    {
        /* Booster is not used */
    }

    return ( returnState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
