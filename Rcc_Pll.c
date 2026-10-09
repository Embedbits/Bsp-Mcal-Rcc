/**
 * \author Mr.Nobody
 * \file Rcc_Pll.c
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality.
 *
 * STM32G4 family has one PLL (RCC_PLLCFGR):
 * - input divider M (1 - 16), PLL input frequency 2.66 - 16 MHz
 * - multiplier N (8 - 127), VCO frequency 96 - 344 MHz
 * - output P (2 - 31)   - ADC kernel clock                    (PLLPEN)
 * - output Q (2/4/6/8)  - 48 MHz clock, FDCAN, QSPI, I2S, SAI (PLLQEN)
 * - output R (2/4/6/8)  - system clock                        (PLLREN)
 *
 * Output divider value 0 in the configuration structure disables the output.
 * Frequency of a disabled output is not available (error is returned).
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_Pll.h"                        /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
#include "Rcc_ClkSrc.h"                     /* Clock sources functionality    */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_PLL_TIMEOUT_RAW             ( 0x84FCB )

/** M divider minimal value */
#define RCC_PLL_M_DIV_MIN               ( 1u )

/** M divider maximal value */
#define RCC_PLL_M_DIV_MAX               ( 16u )

/** N multiplier minimal value */
#define RCC_PLL_N_MULT_MIN              ( 8u )

/** N multiplier maximal value */
#define RCC_PLL_N_MULT_MAX              ( 127u )

/** P divider minimal value (PLLPDIV) */
#define RCC_PLL_P_DIV_MIN               ( 2u )

/** P divider maximal value (PLLPDIV) */
#define RCC_PLL_P_DIV_MAX               ( 31u )

/** P divider selected by PLLP bit when PLLPDIV is 0 - PLLP = 0 */
#define RCC_PLL_P_DIV_LEGACY_7          ( 7u )

/** P divider selected by PLLP bit when PLLPDIV is 0 - PLLP = 1 */
#define RCC_PLL_P_DIV_LEGACY_17         ( 17u )

/** Q and R divider minimal value */
#define RCC_PLL_QR_DIV_MIN              ( 2u )

/** Q and R divider maximal value */
#define RCC_PLL_QR_DIV_MAX              ( 8u )

/** Q and R divider step size */
#define RCC_PLL_QR_DIV_STEP             ( 2u )

/** PLL input (reference) minimal frequency in Hz */
#define RCC_PLL_INPUT_MIN_HZ            ( 2660000u )

/** PLL input (reference) maximal frequency in Hz */
#define RCC_PLL_INPUT_MAX_HZ            ( 16000000u )

/** PLL VCO minimal frequency in Hz */
#define RCC_PLL_VCO_MIN_HZ              ( 96000000u )

/** PLL VCO maximal frequency in Hz */
#define RCC_PLL_VCO_MAX_HZ              ( 344000000u )

/** Value of output divider meaning "output is not used" */
#define RCC_PLL_OUT_DIV_UNUSED          ( 0u )

/* ============================== TYPEDEFS ================================== */

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t  Rcc_Pll_Get_SourceClk   ( rcc_PllClkSrc_t clkSource, rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Set_OutQR       ( rcc_PllId_t pllId, uint32_t divider, uint32_t divMask, uint32_t divPos, uint32_t enMask );
static rcc_RequestState_t  Rcc_Pll_Get_OutQRClk    ( rcc_PllId_t pllId, uint32_t divMask, uint32_t divPos, uint32_t enMask, rcc_FreqHz_t * const pllClk );
static rcc_RequestState_t  Rcc_Pll_Wait_RegVal     ( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief RTCSEL values of RTC clock sources, indexed by \ref rcc_Rtc_ClkSource_t */
static const uint32_t rcc_Pll_RtcClkSrcLut[ RCC_RTC_CLK_SOURCE_CNT ] =
{
    [RCC_RTC_CLK_SOURCE_HSE_DIV] = LL_RCC_RTC_CLKSOURCE_HSE_DIV32,
    [RCC_RTC_CLK_SOURCE_LSE]     = LL_RCC_RTC_CLKSOURCE_LSE,
    [RCC_RTC_CLK_SOURCE_LSI]     = LL_RCC_RTC_CLKSOURCE_LSI,
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes Phase Locked Loop (PLL) handler module.
 *
 * STM32G4 has one PLL described by register field definitions - no
 * configuration array has to be checked.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK.
 */
rcc_RequestState_t Rcc_Pll_Init( void )
{
    return ( RCC_REQUEST_OK );
}


/**
 * \brief De-initializes Phase Locked Loop (PLL) handler module.
 *
 * De-initialization process disables the Phase Locked Loop (PLL).
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Pll_Deinit( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        retState = Rcc_Pll_Set_Inactive( pllId );

        if( retState != RCC_REQUEST_OK )
        {
            break;
        }
    }

    return ( retState );
}


/**
 * \brief Main task of module Rcc_Pll
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's
 * requirements.
 */
void Rcc_Pll_Task( void )
{
    return;
}

/*---------------------------- Pll's configuration ---------------------------*/

/**
 * \brief Configuration of Phase Locked Loop (PLL)
 *
 * User can configure dividers and multipliers of PLL through PLL configuration
 * structure. PLL is de-activated during configuration and activated at the end.
 *
 * \warning User must configure clock source (HSE) before PLL configuration!
 * \warning PLL can not be configured while it is used as system clock.
 *
 * \param pllId        [in]: Required Phase Locked Loop (PLL) identification.
 * \param configStruct [in]: PLL configuration structure
 *
 * \return State of request execution. Returns "OK" if request was success
 *         (also if the PLL is not used - \ref RCC_PLL_SRC_NONE), otherwise
 *         return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Config( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != configStruct ) &&
        ( RCC_PLL_CNT   >  pllId       )    )
    {
        /* PLL must be inactive during configuration (or is not used) */
        retState = Rcc_Pll_Set_Inactive( pllId );

        if( ( RCC_PLL_SRC_NONE == configStruct->Pll_Source ) ||
            ( RCC_REQUEST_OK   != retState                 )    )
        {
            /* PLL is not used or can not be de-activated */
        }
        else if( ( RCC_PLL_M_DIV_MIN  > configStruct->M_Divider    ) ||
                 ( RCC_PLL_M_DIV_MAX  < configStruct->M_Divider    ) ||
                 ( RCC_PLL_N_MULT_MIN > configStruct->N_Multiplier ) ||
                 ( RCC_PLL_N_MULT_MAX < configStruct->N_Multiplier )    )
        {
            /* Incorrect PLL internal configuration */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            rcc_FreqHz_t freqInHz = 0u;

            retState = Rcc_Pll_Set_Source( pllId, configStruct->Pll_Source );

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Get_SourceClk( configStruct->Pll_Source, &freqInHz );
            }
            else
            {
                /* Error during configuration process */
            }

            /* ---------- Check PLL input and VCO frequency ranges ----------- */
            if( RCC_REQUEST_OK == retState )
            {
                const rcc_FreqHz_t refFreqHz = freqInHz / configStruct->M_Divider;
                const uint64_t     vcoFreqHz = (uint64_t)refFreqHz * configStruct->N_Multiplier;

                if( ( RCC_PLL_INPUT_MIN_HZ > refFreqHz ) ||
                    ( RCC_PLL_INPUT_MAX_HZ < refFreqHz ) ||
                    ( RCC_PLL_VCO_MIN_HZ   > vcoFreqHz ) ||
                    ( RCC_PLL_VCO_MAX_HZ   < vcoFreqHz )    )
                {
                    /* Reference or VCO frequency out of range */
                    retState = RCC_REQUEST_ERROR;
                }
                else
                {
                    /* Frequencies are in range */
                }
            }
            else
            {
                /* Error during configuration process */
            }

            /* ------------- Configure PLL M divider and N multiplier -------- */
            if( RCC_REQUEST_OK == retState )
            {
                Rcc_Set_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLM,
                                ( configStruct->M_Divider - 1u ) << RCC_PLLCFGR_PLLM_Pos );

                Rcc_Set_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLN,
                                configStruct->N_Multiplier << RCC_PLLCFGR_PLLN_Pos );
            }
            else
            {
                /* Error during configuration process */
            }

            /*-------------------- Configure PLL outputs ---------------------*/
            if( RCC_REQUEST_OK == retState )
            {
                if( RCC_PLL_OUT_DIV_UNUSED != configStruct->P_Divider )
                {
                    retState = Rcc_Pll_Set_OutP( pllId, configStruct->P_Divider );
                }
                else
                {
                    /* PLL output P is not used */
                    Rcc_Reset_RegBit( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLPEN );
                }
            }

            if( RCC_REQUEST_OK == retState )
            {
                if( RCC_PLL_OUT_DIV_UNUSED != configStruct->Q_Divider )
                {
                    retState = Rcc_Pll_Set_OutQ( pllId, configStruct->Q_Divider );
                }
                else
                {
                    /* PLL output Q is not used */
                    Rcc_Reset_RegBit( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLQEN );
                }
            }

            if( RCC_REQUEST_OK == retState )
            {
                if( RCC_PLL_OUT_DIV_UNUSED != configStruct->R_Divider )
                {
                    retState = Rcc_Pll_Set_OutR( pllId, configStruct->R_Divider );
                }
                else
                {
                    /* PLL output R is not used */
                    Rcc_Reset_RegBit( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLREN );
                }
            }

            /* ------------- Activate PLL if no error occurred -------------- */
            if( RCC_REQUEST_OK == retState )
            {
                /* Activate PLL. This must be the last step! */
                retState = Rcc_Pll_Set_Active( pllId );
            }
            else
            {
                /* Some error occurred during PLL configuration. Do not activate
                 * the PLL. */
            }
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns PLL internal (VCO) frequency
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param pllClk [out]: Pointer to PLL internal frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_InternalClk( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState     = RCC_REQUEST_ERROR;
    rcc_PllClkSrc_t    pllClkSource = RCC_PLL_SRC_NONE;
    rcc_FreqHz_t       inputClkFreq = 0u;

    if( RCC_NULL_PTR != pllClk )
    {
        retState = Rcc_Pll_Get_Source( pllId, &pllClkSource );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Pll_Get_SourceClk( pllClkSource, &inputClkFreq );
        }
        else
        {
            /* Incorrect PLL ID */
        }

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t pllMDiv  = ( Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLM ) >> RCC_PLLCFGR_PLLM_Pos ) + 1u;
            const uint32_t pllNMult =   Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLN ) >> RCC_PLLCFGR_PLLN_Pos;

            *pllClk = (rcc_FreqHz_t)( ( (uint64_t)inputClkFreq * pllNMult ) / pllMDiv );
        }
        else
        {
            /* PLL source frequency is not available */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activation of Phase Locked Loop (PLL) block
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Active( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_PLLON );

        retState = Rcc_Pll_Wait_RegVal( RCC_REG_CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY );
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief De-activation of Phase Locked Loop (PLL) block
 *
 * \note PLL can not be de-activated while it is used as system clock.
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Inactive( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Reset_RegBit( RCC_REG_CR, RCC_CR_PLLON );

        retState = Rcc_Pll_Wait_RegVal( RCC_REG_CR, RCC_CR_PLLRDY, 0u );
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading status of Phase Locked Loop (PLL) block
 *
 * \param pllId  [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param state [out]: Pointer to store actual PLL state. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_State( rcc_PllId_t pllId, rcc_FunctionState_t * const state )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != state ) &&
        ( RCC_PLL_CNT   > pllId )    )
    {
        const uint32_t readyRegVal    = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_PLLRDY );
        const uint32_t pllStateRegVal = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_PLLON  );

        if ( ( 0u != readyRegVal    ) &&
             ( 0u != pllStateRegVal )    )
        {
            *state = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *state = RCC_FUNCTION_INACTIVE;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Selection of clock source for Phase Locked Loop's multiplexer
 *
 * The source can be changed only while the PLL is inactive. HSI is activated
 * if selected, HSE must be activated by user.
 *
 * \param pllId     [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI16 oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Source( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    rcc_RequestState_t retState     = RCC_REQUEST_ERROR;
    uint32_t           targetRegVal = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        if( RCC_PLL_SRC_HSE == clkSource )
        {
            targetRegVal = LL_RCC_PLLSOURCE_HSE;

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_PLL_SRC_HSI == clkSource )
        {
            targetRegVal = LL_RCC_PLLSOURCE_HSI;

            retState = Rcc_ClkSrc_Set_HsiActive();
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t actualRegVal = Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC );

            if( targetRegVal == actualRegVal )
            {
                /* Clock source is already selected */
            }
            else if( 0u == Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_PLLON ) )
            {
                Rcc_Set_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC, targetRegVal );

                retState = Rcc_Pll_Wait_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC, targetRegVal );
            }
            else
            {
                /* Source can not be changed while PLL is active */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Incorrect clock source was requested */
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns clock source selected by Phase Locked Loop (PLL) multiplexer
 *
 * \param pllId      [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [out]: Pointer to store actual clock source of PLL, value from \ref rcc_PllClkSrc_t. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Source( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != clkSource ) &&
        ( RCC_PLL_CNT   > pllId     )    )
    {
        const uint32_t regValue = Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC );

        if( LL_RCC_PLLSOURCE_HSE == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSE;
        }
        else if( LL_RCC_PLLSOURCE_HSI == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSI;
        }
        else
        {
            /* No clock selected */
            *clkSource = RCC_PLL_SRC_NONE;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Phase Locked Loop (PLL) output P divider configuration and output activation.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output P divider value (2 - 31).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutP( rcc_PllId_t pllId, rcc_PllPDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT       >  pllId   ) &&
        ( RCC_PLL_P_DIV_MIN <= divider ) &&
        ( RCC_PLL_P_DIV_MAX >= divider )    )
    {
        /* PLLPDIV holds the divider value directly */
        Rcc_Set_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLPDIV, divider << RCC_PLLCFGR_PLLPDIV_Pos );

        Rcc_Set_RegBit( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLPEN );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect PLL Id or output P divider value */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency of PLL output P
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is disabled).
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    if( ( RCC_PLL_CNT   > pllId  ) &&
        ( RCC_NULL_PTR != pllClk )    )
    {
        retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

        if( ( RCC_REQUEST_OK == retState                                            ) &&
            ( 0u             != Rcc_Get_RegBit( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLPEN ) )    )
        {
            uint32_t divider = Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLPDIV ) >> RCC_PLLCFGR_PLLPDIV_Pos;

            if( 0u == divider )
            {
                /* PLLPDIV not used - divider selected by PLLP bit */
                divider = ( 0u != Rcc_Get_RegBit( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLP ) ) ? RCC_PLL_P_DIV_LEGACY_17 :
                                                                                          RCC_PLL_P_DIV_LEGACY_7;
            }
            else
            {
                /* PLLPDIV holds the divider */
            }

            *pllClk = pllIntFreq / divider;
        }
        else
        {
            /* Output is disabled or PLL frequency is not available */
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
 * \brief Phase Locked Loop (PLL) output Q divider configuration and output activation.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output Q divider value (2, 4, 6, 8).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutQ( rcc_PllId_t pllId, rcc_PllQDivider_t divider )
{
    return ( Rcc_Pll_Set_OutQR( pllId, divider, RCC_PLLCFGR_PLLQ, RCC_PLLCFGR_PLLQ_Pos, RCC_PLLCFGR_PLLQEN ) );
}


/**
 * \brief Reading of output frequency of PLL output Q
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is disabled).
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    return ( Rcc_Pll_Get_OutQRClk( pllId, RCC_PLLCFGR_PLLQ, RCC_PLLCFGR_PLLQ_Pos, RCC_PLLCFGR_PLLQEN, pllClk ) );
}


/**
 * \brief Phase Locked Loop (PLL) output R divider configuration and output activation.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output R divider value (2, 4, 6, 8).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutR( rcc_PllId_t pllId, rcc_PllRDivider_t divider )
{
    return ( Rcc_Pll_Set_OutQR( pllId, divider, RCC_PLLCFGR_PLLR, RCC_PLLCFGR_PLLR_Pos, RCC_PLLCFGR_PLLREN ) );
}


/**
 * \brief Reading of output frequency of PLL output R
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is disabled).
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    return ( Rcc_Pll_Get_OutQRClk( pllId, RCC_PLLCFGR_PLLR, RCC_PLLCFGR_PLLR_Pos, RCC_PLLCFGR_PLLREN, pllClk ) );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * RTC clock source is located in backup domain - write protection is released
 * automatically. HSE clock is divided by fixed divider 32.
 *
 * \warning RTC clock source can be selected only once. It can be changed only
 *          after backup domain reset - error is returned in such a case.
 *
 * \note Oscillator of the selected source must be activated by user.
 *
 * \param clkSource [in]: RTC clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_RtcClkSource( rcc_Rtc_ClkSource_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_RTC_CLK_SOURCE_CNT > clkSource )
    {
        const uint32_t llClkSource  = rcc_Pll_RtcClkSrcLut[ clkSource ];
        const uint32_t actualSource = Rcc_Get_RegVal( RCC_REG_BDCR, RCC_BDCR_RTCSEL );

        if( llClkSource == actualSource )
        {
            /* Clock source already selected */
            retState = RCC_REQUEST_OK;
        }
        else if( LL_RCC_RTC_CLKSOURCE_NONE == actualSource )
        {
            Rcc_Set_RegVal( RCC_REG_BDCR, RCC_BDCR_RTCSEL, llClkSource );

            retState = Rcc_Pll_Wait_RegVal( RCC_REG_BDCR, RCC_BDCR_RTCSEL, llClkSource );
        }
        else
        {
            /* Other clock source already selected - backup domain reset required */
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
 * \brief Reading of clock source of Real Time Clock (RTC) multiplexer
 *
 * \param clkSource [out]: RTC clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if no clock source is selected).
 */
rcc_RequestState_t Rcc_Pll_Get_RtcClkSource( rcc_Rtc_ClkSource_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkSource )
    {
        const uint32_t llClkSource = Rcc_Get_RegVal( RCC_REG_BDCR, RCC_BDCR_RTCSEL );

        /* RTCSEL = 0 (no clock) has no source identification - error is returned */
        for( rcc_Rtc_ClkSource_t srcIdx = RCC_RTC_CLK_SOURCE_HSE_DIV; RCC_RTC_CLK_SOURCE_CNT > srcIdx; srcIdx++ )
        {
            if( rcc_Pll_RtcClkSrcLut[ srcIdx ] == llClkSource )
            {
                *clkSource = srcIdx;
                retState   = RCC_REQUEST_OK;
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
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Returns frequency of PLL source clock.
 *
 * \param clkSource [in]: PLL clock source
 * \param clkFreq  [out]: Frequency of the source clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_SourceClk( rcc_PllClkSrc_t clkSource, rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_SRC_HSE == clkSource )
    {
        retState = Rcc_ClkSrc_Get_HseClk( clkFreq );
    }
    else if( RCC_PLL_SRC_HSI == clkSource )
    {
        retState = Rcc_ClkSrc_Get_HsiClk( clkFreq );
    }
    else
    {
        /* PLL source is not selected */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures PLL output Q or R divider (2, 4, 6, 8) and activates the output.
 *
 * \param pllId   [in]: PLL identification
 * \param divider [in]: Required divider value
 * \param divMask [in]: Divider field mask
 * \param divPos  [in]: Divider field position
 * \param enMask  [in]: Output enable bit mask
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Set_OutQR( rcc_PllId_t pllId, uint32_t divider, uint32_t divMask, uint32_t divPos, uint32_t enMask )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT        >  pllId                             ) &&
        ( RCC_PLL_QR_DIV_MIN <= divider                           ) &&
        ( RCC_PLL_QR_DIV_MAX >= divider                           ) &&
        ( 0u                 == ( divider % RCC_PLL_QR_DIV_STEP ) )    )
    {
        /* Field: 0 - divider 2, 1 - divider 4, 2 - divider 6, 3 - divider 8 */
        const uint32_t regValue = ( divider / RCC_PLL_QR_DIV_STEP ) - 1u;

        Rcc_Set_RegVal( RCC_REG_PLLCFGR, divMask, regValue << divPos );

        Rcc_Set_RegBit( RCC_REG_PLLCFGR, enMask );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect PLL Id or divider value */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Calculates frequency of PLL output Q or R.
 *
 * \param pllId   [in]: PLL identification
 * \param divMask [in]: Divider field mask
 * \param divPos  [in]: Divider field position
 * \param enMask  [in]: Output enable bit mask
 * \param pllClk [out]: Pointer to PLL output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is disabled).
 */
static rcc_RequestState_t Rcc_Pll_Get_OutQRClk( rcc_PllId_t pllId, uint32_t divMask, uint32_t divPos, uint32_t enMask, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    if( ( RCC_PLL_CNT   > pllId  ) &&
        ( RCC_NULL_PTR != pllClk )    )
    {
        retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

        if( ( RCC_REQUEST_OK == retState                                ) &&
            ( 0u             != Rcc_Get_RegBit( RCC_REG_PLLCFGR, enMask ) )    )
        {
            const uint32_t divider = ( ( Rcc_Get_RegVal( RCC_REG_PLLCFGR, divMask ) >> divPos ) + 1u ) * RCC_PLL_QR_DIV_STEP;

            *pllClk = pllIntFreq / divider;
        }
        else
        {
            /* Output is disabled or PLL frequency is not available */
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
 * \brief Waits until register field reaches expected value.
 *
 * \param regId       [in]: Register identification
 * \param regMask     [in]: Mask of the checked field
 * \param expectedVal [in]: Expected value of the masked field
 *
 * \return Returns "OK" if the field reached expected value in time, otherwise
 *         returns error.
 */
static rcc_RequestState_t Rcc_Pll_Wait_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        if( expectedVal == Rcc_Get_RegVal( regId, regMask ) )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Register has not reached expected value yet, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
