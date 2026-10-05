/**
 * \author Mr.Nobody
 * \file Rcc_ClkBus.c
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
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_CLKBUS_TIMEOUT_RAW              ( 0x84FCB )

#if defined(STM32F405xx) || defined(STM32F407xx) || defined(STM32F415xx) || defined(STM32F417xx)
/** Device errata "Slowing down APB clock during a DMA transfer" (ES0182): a DMA transfer is
 *  blocked (only reset recovers) when the APB clock is slowed down during its write access */
#define RCC_CLKBUS_ERRATA_DMA_SLOWDOWN

/** Count of DMA streams checked before a bus clock is slowed down (DMA1 and DMA2, 8 each) */
#define RCC_CLKBUS_DMA_STREAM_CNT           ( 16u )
#endif /* STM32F405xx || STM32F407xx || STM32F415xx || STM32F417xx */

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_FreqHz_t       Rcc_ClkBus_Get_TimClk     ( rcc_FreqHz_t hClk, rcc_FreqHz_t pClk );
static rcc_RequestState_t Rcc_ClkBus_Check_SlowDown ( uint32_t currentDiv, uint32_t requiredDiv );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

#if defined(RCC_CLKBUS_ERRATA_DMA_SLOWDOWN)
/** DMA streams whose transfers block the bus clock slow down (device errata) */
static DMA_Stream_TypeDef * const rcc_ClkBusDmaStreams[ RCC_CLKBUS_DMA_STREAM_CNT ] =
{
    DMA1_Stream0, DMA1_Stream1, DMA1_Stream2, DMA1_Stream3,
    DMA1_Stream4, DMA1_Stream5, DMA1_Stream6, DMA1_Stream7,
    DMA2_Stream0, DMA2_Stream1, DMA2_Stream2, DMA2_Stream3,
    DMA2_Stream4, DMA2_Stream5, DMA2_Stream6, DMA2_Stream7,
};
#endif /* RCC_CLKBUS_ERRATA_DMA_SLOWDOWN */

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
 * \note Selected clock source must be ready, otherwise the system clock is not
 *       switched by hardware and error is returned.
 *
 * \param systemClkSource [in]: Selected clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_SysClkSource( rcc_SystemClkSrc_t systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_SYSTEM_CLOCK_SOURCE_CNT > systemClkSource )
    {
        /* Enumeration values are equal to SW field values */
        const uint32_t clkSource = (uint32_t)systemClkSource << RCC_CFGR_SW_Pos;

        LL_RCC_SetSysClkSource( clkSource );

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t clkSourceRet = LL_RCC_GetSysClkSource() >> RCC_CFGR_SWS_Pos;

            if( (uint32_t)systemClkSource == clkSourceRet )
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
        const uint32_t regValue = LL_RCC_GetSysClkSource() >> RCC_CFGR_SWS_Pos;

        if( RCC_SYSTEM_CLOCK_SOURCE_CNT > regValue )
        {
            *systemClkSource = (rcc_SystemClkSrc_t)regValue;

            returnState = RCC_REQUEST_OK;
        }
        else
        {
            /* Reserved value */
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

    if( ( RCC_REQUEST_OK == retState ) &&
        ( RCC_NULL_PTR   != busClk   )    )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == systemClkSource )
        {
            retState = Rcc_Pll_Get_Clk_OutP( RCC_PLL_MAIN, busClk );
        }
#if defined(RCC_CFGR_SW_PLLR)
        else if( RCC_SYSTEM_CLOCK_SOURCE_PLLR == systemClkSource )
        {
            retState = Rcc_Pll_Get_Clk_OutR( RCC_PLL_MAIN, busClk );
        }
#endif /* RCC_CFGR_SW_PLLR */
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == systemClkSource )
        {
            retState = Rcc_ClkSrc_Get_HseClk( busClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == systemClkSource )
        {
            retState = Rcc_ClkSrc_Get_HsiClk( busClk );
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
 * \note STM32F405 / 407 / 415 / 417 device errata "Slowing down APB clock during a DMA
 *       transfer": HCLK (and so the APB clocks) is not slowed down while a DMA stream is
 *       enabled - error is returned, the divider is not changed.
 *
 * \param dividerId [in] : Required AHB divider value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_AHBDivider( rcc_AHB_Div_t dividerId )
{
    rcc_RequestState_t       returnState = RCC_REQUEST_ERROR;
    const rcc_RequestState_t dmaState    = Rcc_ClkBus_Check_SlowDown( LL_RCC_GetAHBPrescaler(), (uint32_t)dividerId );

    if( ( 0u             == ( (uint32_t)dividerId & ~RCC_CFGR_HPRE ) ) &&
        ( RCC_REQUEST_OK == dmaState                                 )    )
    {
        LL_RCC_SetAHBPrescaler( dividerId );

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( (uint32_t)dividerId == LL_RCC_GetAHBPrescaler() )
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
        /* Value does not belong to AHB prescaler field */
        returnState = RCC_REQUEST_ERROR;
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
rcc_RequestState_t Rcc_ClkBus_Get_AHBDivider( rcc_AHB_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId = (rcc_AHB_Div_t)LL_RCC_GetAHBPrescaler();

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
    rcc_AHB_Div_t      ahbDivider = RCC_AHB_DIVIDER_1;

    /* Get system clock frequency */
    retState  = Rcc_ClkBus_Get_SysClk( &systemClk );
    retState2 = Rcc_ClkBus_Get_AHBDivider( &ahbDivider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
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
 * \note STM32F405 / 407 / 415 / 417 device errata "Slowing down APB clock during a DMA
 *       transfer": APB1 is not slowed down while a DMA stream is enabled - error is
 *       returned, the divider is not changed.
 *
 * \param dividerId [in] : Identification of divider configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB1Divider( rcc_APB1_Div_t dividerId )
{
    rcc_RequestState_t       returnState = RCC_REQUEST_ERROR;
    const rcc_RequestState_t dmaState    = Rcc_ClkBus_Check_SlowDown( LL_RCC_GetAPB1Prescaler(), (uint32_t)dividerId );

    if( ( 0u             == ( (uint32_t)dividerId & ~RCC_CFGR_PPRE1 ) ) &&
        ( RCC_REQUEST_OK == dmaState                                  )    )
    {
        LL_RCC_SetAPB1Prescaler( dividerId );

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( (uint32_t)dividerId == LL_RCC_GetAPB1Prescaler() )
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
        /* Value does not belong to APB1 prescaler field */
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Read APB1 divider.
 *
 * Clock bus APB1 is clocked through APB1 divider from HCLK (AHB bus).
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
        *dividerId = (rcc_APB1_Div_t)LL_RCC_GetAPB1Prescaler();

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
    rcc_APB1_Div_t     apb1Divider = RCC_APB1_DIVIDER_1;

    /* Get system clock frequency */
    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB1Divider( &apb1Divider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
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
 * \note STM32F405 / 407 / 415 / 417 device errata "Slowing down APB clock during a DMA
 *       transfer": APB2 is not slowed down while a DMA stream is enabled - error is
 *       returned, the divider is not changed.
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkBus_Set_APB2Divider( rcc_APB2_Div_t dividerId )
{
    rcc_RequestState_t       returnState = RCC_REQUEST_ERROR;
    const rcc_RequestState_t dmaState    = Rcc_ClkBus_Check_SlowDown( LL_RCC_GetAPB2Prescaler(), (uint32_t)dividerId );

    if( ( 0u             == ( (uint32_t)dividerId & ~RCC_CFGR_PPRE2 ) ) &&
        ( RCC_REQUEST_OK == dmaState                                  )    )
    {
        LL_RCC_SetAPB2Prescaler( dividerId );

        for( uint32_t iterationCnt = 0u; RCC_CLKBUS_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( (uint32_t)dividerId == LL_RCC_GetAPB2Prescaler() )
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
        /* Value does not belong to APB2 prescaler field */
        returnState = RCC_REQUEST_ERROR;
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
rcc_RequestState_t Rcc_ClkBus_Get_APB2Divider( rcc_APB2_Div_t * const dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != dividerId )
    {
        *dividerId = (rcc_APB2_Div_t)LL_RCC_GetAPB2Prescaler();

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
    rcc_APB2_Div_t     apb2Divider = RCC_APB2_DIVIDER_1;

    /* Get system clock frequency */
    retState  = Rcc_ClkBus_Get_AHBClk( &hClk );
    retState2 = Rcc_ClkBus_Get_APB2Divider( &apb2Divider );

    if( ( RCC_REQUEST_OK == retState  ) &&
        ( RCC_NULL_PTR   != busClk    ) &&
        ( RCC_REQUEST_OK == retState2 )    )
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

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Calculates timer kernel clock of APB bus timers (timer clock selection)
 *
 * - APB prescaler 1:                     timer clock = PCLK
 * - TIMPRE = 0 (or not available), prescaler > 1: timer clock = 2 x PCLK
 * - TIMPRE = 1, prescaler 2/4:           timer clock = HCLK
 * - TIMPRE = 1, prescaler > 4:           timer clock = 4 x PCLK
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
#if defined(RCC_DCKCFGR_TIMPRE)
    else if( LL_RCC_TIM_PRESCALER_TWICE != LL_RCC_GetTIMPrescaler() )
    {
        if( hClk <= ( 4u * pClk ) )
        {
            /* TIMPRE = 1 and APB prescaler 2 or 4 */
            timClk = hClk;
        }
        else
        {
            timClk = 4u * pClk;
        }
    }
#endif /* RCC_DCKCFGR_TIMPRE */
    else
    {
        timClk = 2u * pClk;
    }

    return ( timClk );
}


/**
 * \brief Checks that a bus clock can be slowed down (STM32F405 / 407 / 415 / 417)
 *
 * Device errata "Slowing down APB clock during a DMA transfer": when the APB clock is slowed
 * down while a DMA performs a write access to a peripheral of the bus, the DMA transfer is
 * blocked and only a system reset recovers. A divider increase (AHB / APB prescaler register
 * values grow with the division) is refused while any DMA1 / DMA2 stream is enabled. Divider
 * decrease, unchanged divider and other devices are always accepted.
 *
 * \param currentDiv  [in]: Current prescaler register field value
 * \param requiredDiv [in]: Required prescaler register field value
 *
 * \return Returns \ref RCC_REQUEST_OK if the divider can be changed, otherwise
 *         \ref RCC_REQUEST_ERROR.
 */
static rcc_RequestState_t Rcc_ClkBus_Check_SlowDown( uint32_t currentDiv, uint32_t requiredDiv )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

#if defined(RCC_CLKBUS_ERRATA_DMA_SLOWDOWN)
    if( requiredDiv > currentDiv )
    {
        for( uint32_t streamIdx = 0u; RCC_CLKBUS_DMA_STREAM_CNT > streamIdx; streamIdx ++ )
        {
            if( 0u != READ_BIT( rcc_ClkBusDmaStreams[ streamIdx ]->CR, DMA_SxCR_EN ) )
            {
                /* DMA transfer is running - it would be blocked */
                retState = RCC_REQUEST_ERROR;
                break;
            }
            else
            {
                /* Stream is disabled */
            }
        }
    }
    else
    {
        /* Bus clock is not slowed down */
    }
#else
    (void)currentDiv;
    (void)requiredDiv;
#endif /* RCC_CLKBUS_ERRATA_DMA_SLOWDOWN */

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
