/**
 * \author Mr.Nobody
 * \file Rcc_ClkBus.c
 * \ingroup Rcc
 * \brief Rcc module ClkBus component functionality.
 *
 * STM32H7 clock tree of the buses:
 * - SYSCLK (HSI / CSI / HSE / PLL1 P) divided by system prescaler D1CPRE
 *   (CDCPRE) is the CPU clock,
 * - CPU clock divided by AHB prescaler HPRE is HCLK (AHB1 - AHB4, AXI),
 * - HCLK divided by D1PPRE (CDPPRE) is APB3, by D2PPRE1 / D2PPRE2 (CDPPRE1 /
 *   CDPPRE2) APB1 / APB2 and by D3PPRE (SRDPPRE) APB4.
 *
 * STM32H7R / H7S: CPU clock by CPRE (CDCFGR), HCLK (AHB1 - AHB5) by bus matrix
 * prescaler BMPRE (BMCFGR), APB1 / APB2 / APB4 / APB5 by PPRE1 / PPRE2 / PPRE4 /
 * PPRE5 (APBCFGR) - no APB3 bus.
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkBus.h"                     /* Self include                   */
#include "Rcc_ClkSrc.h"                     /* Clock source module            */
#include "Rcc_Pll.h"                        /* PLL module                     */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_CLKBUS_TIMEOUT_RAW              ( 0x84FCB )

/** Field mask of the system clock prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_SYS_DIV_MASK             ( LL_RCC_SYSCLK_DIV_512 )

/** Field mask of the AHB prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_AHB_DIV_MASK             ( LL_RCC_AHB_DIV_512 )

/** Field mask of the APB1 prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_APB1_DIV_MASK            ( LL_RCC_APB1_DIV_16 )

/** Field mask of the APB2 prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_APB2_DIV_MASK            ( LL_RCC_APB2_DIV_16 )

#if defined(STM32H7RS)
/** Field mask of the APB5 prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_APB5_DIV_MASK            ( LL_RCC_APB5_DIV_16 )
#else
/** Field mask of the APB3 prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_APB3_DIV_MASK            ( LL_RCC_APB3_DIV_16 )
#endif

/** Field mask of the APB4 prescaler (all bits of the maximal division) */
#define RCC_CLKBUS_APB4_DIV_MASK            ( LL_RCC_APB4_DIV_16 )

/** Count of bits in prescaler register */
#define RCC_CLKBUS_REG_BITS                 ( 32u )

/** Mask of prescaler table index (4-bit prescaler field) */
#define RCC_CLKBUS_PRESC_IDX_MASK           ( 0x0Fu )

/** Mask of shift value in prescaler table */
#define RCC_CLKBUS_PRESC_SHIFT_MASK         ( 0x1Fu )

/* ============================== TYPEDEFS ================================== */

/** Prescaler setter of LL */
typedef void     ( *rcc_ClkBus_SetPresc_t )( uint32_t prescaler );

/** Prescaler getter of LL */
typedef uint32_t ( *rcc_ClkBus_GetPresc_t )( void );

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_ClkBus_Set_Prescaler( rcc_ClkBus_SetPresc_t setPresc,
                                                    rcc_ClkBus_GetPresc_t getPresc,
                                                    uint32_t              fieldMask,
                                                    uint32_t              prescaler );
static rcc_FreqHz_t       Rcc_ClkBus_Get_TimClk   ( rcc_FreqHz_t hClk, rcc_FreqHz_t pClk );
static rcc_FreqHz_t       Rcc_ClkBus_Get_DividedClk( rcc_FreqHz_t clkFreq, uint32_t fieldMask, uint32_t prescaler );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Shift of the clock (division factor 2^shift) of a prescaler field value
 *  (CPU / AHB prescaler 4-bit field: 0xxx - 1, 1000 - 2 ... 1111 - 512, APB
 *  prescaler 3-bit field: 0xx - 1, 100 - 2 ... 111 - 16) */
static const uint8_t rcc_ClkBus_PrescShiftTable[ RCC_CLKBUS_PRESC_IDX_MASK + 1u ] =
{
    0u, 0u, 0u, 0u, 1u, 2u, 3u, 4u, 1u, 2u, 3u, 4u, 6u, 7u, 8u, 9u
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
        else if( RCC_SYSTEM_CLOCK_SOURCE_CSI == systemClkSource )
        {
            clkSource = LL_RCC_SYS_CLKSOURCE_CSI;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == systemClkSource )
        {
            clkSource = LL_RCC_SYS_CLKSOURCE_HSE;
        }
        else
        {
            clkSource = LL_RCC_SYS_CLKSOURCE_PLL1;
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
rcc_RequestState_t Rcc_ClkBus_Get_SysClkSource( rcc_SystemClkSrc_t * const systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != systemClkSource )
    {
        const uint32_t regValue = LL_RCC_GetSysClkSource();

        *systemClkSource = (rcc_SystemClkSrc_t)( regValue >> RCC_CFGR_SWS_Pos );

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
    rcc_SystemClkSrc_t systemClkSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;

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
            retState = Rcc_ClkSrc_Get_Hsi64Clk( busClk );
        }
        else
        {
            retState = Rcc_ClkSrc_Get_CsiClk( busClk );
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of trace clock frequency (TRACECLKIN) in Hz
 *
 * The trace clock multiplexer is switched together with the system clock
 * multiplexer (RCC_CFGR SW): PLL1 R output if the system clock is PLL1 P output,
 * otherwise the system clock oscillator (HSI / CSI / HSE). The system clock
 * prescaler (D1CPRE / CDCPRE) does not divide the trace clock.
 *
 * \note PLL1 R output has to be enabled (R divider not 0) for trace with PLL1
 *       system clock.
 *
 * \param traceClk [out]: Pointer to trace clock value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_TraceClk( rcc_FreqHz_t * const traceClk )
{
    rcc_RequestState_t retState        = RCC_REQUEST_ERROR;
    rcc_SystemClkSrc_t systemClkSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;

    retState = Rcc_ClkBus_Get_SysClkSource( &systemClkSource );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != traceClk )    )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == systemClkSource )
        {
            retState = Rcc_Pll_Get_Clk_OutR( RCC_PLL_1, traceClk );
        }
        else
        {
            retState = Rcc_ClkBus_Get_SysClk( traceClk );
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure system clock prescaler (D1CPRE / CDCPRE) - CPU clock.
 *
 * \param dividerId [in] : Required system clock divider value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_SYSDivider( rcc_SYS_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetSysPrescaler, LL_RCC_GetSysPrescaler,
                                       RCC_CLKBUS_SYS_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read system clock prescaler (D1CPRE / CDCPRE).
 *
 * \param dividerId [out] : System clock divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_SYSDivider( rcc_SYS_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_SYS_Div_t)LL_RCC_GetSysPrescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        /* No action required */
    }

    return ( returnState );
}


/**
 * \brief Reading of CPU clock frequency (SYSCLK divided by D1CPRE / CDCPRE) in Hz
 *
 * \param busClk [out]: Pointer to CPU clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_CpuClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       systemClk  = 0u;
    rcc_SYS_Div_t      sysDivider = RCC_SYS_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_SysClk( &systemClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_SYSDivider( &sysDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Calc_CpuClk( systemClk, sysDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure AHB divider (HPRE).
 *
 * HCLK is clocked through AHB divider from CPU clock.
 *
 * \param dividerId [in] : Required AHB divider value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_AHBDivider( rcc_AHB_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetAHBPrescaler, LL_RCC_GetAHBPrescaler,
                                       RCC_CLKBUS_AHB_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read AHB divider.
 *
 * \param dividerId [out] : AHB divider
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
        /* No action required */
    }

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
    rcc_FreqHz_t       cpuClk     = 0u;
    rcc_AHB_Div_t      ahbDivider = RCC_AHB_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_CpuClk( &cpuClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_AHBDivider( &ahbDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Calc_AHBClk( cpuClk, ahbDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure APB1 divider (D2PPRE1 / CDPPRE1).
 *
 * \param dividerId [in] : Identification of divider configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB1Divider( rcc_APB1_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetAPB1Prescaler, LL_RCC_GetAPB1Prescaler,
                                       RCC_CLKBUS_APB1_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read APB1 divider.
 *
 * \param dividerId [out]: Divider ID
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
        /* No action required */
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB1 clock bus
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB1Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB1_Div_t     apbDivider  = RCC_APB1_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_APB1Divider( &apbDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Get_DividedClk( hClk, RCC_CLKBUS_APB1_DIV_MASK, (uint32_t)apbDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configure APB2 divider (D2PPRE2 / CDPPRE2).
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB2Divider( rcc_APB2_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetAPB2Prescaler, LL_RCC_GetAPB2Prescaler,
                                       RCC_CLKBUS_APB2_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read APB2 divider.
 *
 * \param dividerId [out] : Divider ID
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
        /* No action required */
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB2 clock bus
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB2Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB2_Div_t     apbDivider  = RCC_APB2_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_APB2Divider( &apbDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Get_DividedClk( hClk, RCC_CLKBUS_APB2_DIV_MASK, (uint32_t)apbDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


#if defined(STM32H7RS)
/**
 * \brief Configure APB5 divider (PPRE5, STM32H7R / H7S).
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB5Divider( rcc_APB5_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetAPB5Prescaler, LL_RCC_GetAPB5Prescaler,
                                       RCC_CLKBUS_APB5_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read APB5 divider.
 *
 * \param dividerId [out] : Divider ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB5Divider( rcc_APB5_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_APB5_Div_t)LL_RCC_GetAPB5Prescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        /* No action required */
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB5 clock bus
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB5Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB5_Div_t     apbDivider  = RCC_APB5_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_APB5Divider( &apbDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Get_DividedClk( hClk, RCC_CLKBUS_APB5_DIV_MASK, (uint32_t)apbDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}
#else
/**
 * \brief Configure APB3 divider (D1PPRE / CDPPRE).
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB3Divider( rcc_APB3_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetAPB3Prescaler, LL_RCC_GetAPB3Prescaler,
                                       RCC_CLKBUS_APB3_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read APB3 divider.
 *
 * \param dividerId [out] : Divider ID
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
        /* No action required */
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB3 clock bus
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB3Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB3_Div_t     apbDivider  = RCC_APB3_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_APB3Divider( &apbDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Get_DividedClk( hClk, RCC_CLKBUS_APB3_DIV_MASK, (uint32_t)apbDivider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}
#endif


/**
 * \brief Configure APB4 divider (D3PPRE / SRDPPRE).
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB4Divider( rcc_APB4_Div_t dividerId )
{
    return ( Rcc_ClkBus_Set_Prescaler( LL_RCC_SetAPB4Prescaler, LL_RCC_GetAPB4Prescaler,
                                       RCC_CLKBUS_APB4_DIV_MASK, (uint32_t)dividerId ) );
}


/**
 * \brief Read APB4 divider.
 *
 * \param dividerId [out] : Divider ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB4Divider( rcc_APB4_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId  = (rcc_APB4_Div_t)LL_RCC_GetAPB4Prescaler();
        returnState = RCC_REQUEST_OK;
    }
    else
    {
        /* No action required */
    }

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB4 clock bus
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Get_APB4Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB4_Div_t     apbDivider  = RCC_APB4_DIVIDER_1;

    const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hClk );
    const rcc_RequestState_t divState = Rcc_ClkBus_Get_APB4Divider( &apbDivider );

    if( ( RCC_REQUEST_OK == clkState ) &&
        ( RCC_REQUEST_OK == divState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        *busClk  = Rcc_ClkBus_Get_DividedClk( hClk, RCC_CLKBUS_APB4_DIV_MASK, (uint32_t)apbDivider );
        retState = RCC_REQUEST_OK;
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
 * \param timClk [out]: Pointer to timer kernel clock frequency in Hz
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
 * \param timClk [out]: Pointer to timer kernel clock frequency in Hz
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


/**
 * \brief Calculates CPU clock from system clock and system clock prescaler.
 *
 * \param sysClk     [in]: System clock frequency in Hz
 * \param sysDivider [in]: System clock prescaler (D1CPRE / CDCPRE)
 *
 * \return CPU clock frequency in Hz
 */
rcc_FreqHz_t Rcc_ClkBus_Calc_CpuClk( rcc_FreqHz_t sysClk, rcc_SYS_Div_t sysDivider )
{
    return ( Rcc_ClkBus_Get_DividedClk( sysClk, RCC_CLKBUS_SYS_DIV_MASK, (uint32_t)sysDivider ) );
}


/**
 * \brief Calculates AXI / AHB clock (HCLK) from CPU clock and AHB prescaler.
 *
 * \param cpuClk     [in]: CPU clock frequency in Hz
 * \param ahbDivider [in]: AHB prescaler (HPRE)
 *
 * \return AXI / AHB clock frequency in Hz
 */
rcc_FreqHz_t Rcc_ClkBus_Calc_AHBClk( rcc_FreqHz_t cpuClk, rcc_AHB_Div_t ahbDivider )
{
    return ( Rcc_ClkBus_Get_DividedClk( cpuClk, RCC_CLKBUS_AHB_DIV_MASK, (uint32_t)ahbDivider ) );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Divides clock by prescaler field value.
 *
 * Prescaler fields of STM32H7 use the same encoding (0xxx - not divided, 1xxx /
 * 1xx - divided by power of 2), D1CorePrescTable holds the shift of every field
 * value.
 *
 * \param clkFreq   [in]: Input clock frequency in Hz
 * \param fieldMask [in]: Field mask of the prescaler in its register
 * \param prescaler [in]: Prescaler value (LL value - field in register position)
 *
 * \return Divided clock frequency in Hz
 */
static rcc_FreqHz_t Rcc_ClkBus_Get_DividedClk( rcc_FreqHz_t clkFreq, uint32_t fieldMask, uint32_t prescaler )
{
    uint32_t fieldPos = 0u;

    for( uint32_t bitPos = 0u; RCC_CLKBUS_REG_BITS > bitPos; bitPos ++ )
    {
        if( 0u != ( fieldMask & ( 1uL << bitPos ) ) )
        {
            fieldPos = bitPos;
            break;
        }
        else
        {
            /* Field starts at higher bit */
        }
    }

    const uint32_t prescIdx = ( ( prescaler & fieldMask ) >> fieldPos ) & RCC_CLKBUS_PRESC_IDX_MASK;

    return ( clkFreq >> ( rcc_ClkBus_PrescShiftTable[ prescIdx ] & RCC_CLKBUS_PRESC_SHIFT_MASK ) );
}


/**
 * \brief Writes a bus prescaler and waits until the value is applied.
 *
 * \param setPresc  [in]: LL setter of the prescaler
 * \param getPresc  [in]: LL getter of the prescaler
 * \param fieldMask [in]: Field mask of the prescaler (value outside is rejected)
 * \param prescaler [in]: LL value of the prescaler
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_ClkBus_Set_Prescaler( rcc_ClkBus_SetPresc_t setPresc,
                                                    rcc_ClkBus_GetPresc_t getPresc,
                                                    uint32_t              fieldMask,
                                                    uint32_t              prescaler )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( 0u == ( prescaler & ~fieldMask ) )
    {
        setPresc( prescaler );

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t actualPresc = getPresc();

            if( prescaler == actualPresc )
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
    }
    else
    {
        /* Value is not a prescaler of the bus */
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Calculates timer kernel clock of APB1 / APB2 bus timers
 *
 * - APB prescaler 1:            timer clock = PCLK (= HCLK)
 * - TIMPRE = 0, prescaler > 1:  timer clock = 2 x PCLK
 * - TIMPRE = 1, prescaler 2/4:  timer clock = HCLK
 * - TIMPRE = 1, prescaler > 4:  timer clock = 4 x PCLK
 *
 * \param hClk [in]: AHB clock frequency in Hz
 * \param pClk [in]: APB clock frequency in Hz
 *
 * \return Timer kernel clock frequency in Hz
 */
static rcc_FreqHz_t Rcc_ClkBus_Get_TimClk( rcc_FreqHz_t hClk, rcc_FreqHz_t pClk )
{
    rcc_FreqHz_t   timClk   = pClk;
    const uint32_t timPresc = LL_RCC_GetTIMPrescaler();

    if( hClk == pClk )
    {
        /* APB prescaler 1 - timer clock is the APB clock */
        timClk = pClk;
    }
    else if( LL_RCC_TIM_PRESCALER_TWICE == timPresc )
    {
        timClk = 2u * pClk;
    }
    else if( hClk <= ( 4u * pClk ) )
    {
        /* TIMPRE = 1 and APB prescaler 2 or 4 */
        timClk = hClk;
    }
    else
    {
        timClk = 4u * pClk;
    }

    return ( timClk );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
