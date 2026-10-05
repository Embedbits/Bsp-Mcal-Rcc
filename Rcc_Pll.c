/**
 * \author Mr.Nobody
 * \file Rcc_Pll.c
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality.
 *
 * STM32F4 family PLLs:
 * - Main PLL (PLL)   - PLLCFGR, outputs P (system clock), Q (48 MHz), R
 * - PLLI2S           - PLLI2SCFGR, outputs P, Q, R (selected MCUs)
 * - PLLSAI           - PLLSAICFGR, outputs P, Q, R (selected MCUs)
 *
 * All PLLs are clocked from the common PLL source multiplexer (PLLSRC). The
 * input divider M of main PLL (PLLM) is shared with PLLSAI and with PLLI2S on
 * MCUs without own PLLI2SM divider. Shared settings can be changed only if
 * no other PLL using them is active.
 *
 * Output dividers have no output enable bits - outputs are always active.
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

/** Bit position of M divider field (same in PLLCFGR, PLLI2SCFGR, PLLSAICFGR) */
#define RCC_PLL_M_POS                   ( RCC_PLLCFGR_PLLM_Pos )
/** Bit position of N multiplier field (same in PLLCFGR, PLLI2SCFGR, PLLSAICFGR) */
#define RCC_PLL_N_POS                   ( RCC_PLLCFGR_PLLN_Pos )
/** Bit position of P divider field (same in PLLCFGR, PLLI2SCFGR, PLLSAICFGR) */
#define RCC_PLL_P_POS                   ( RCC_PLLCFGR_PLLP_Pos )
/** Bit position of Q divider field (same in PLLCFGR, PLLI2SCFGR, PLLSAICFGR) */
#define RCC_PLL_Q_POS                   ( RCC_PLLCFGR_PLLQ_Pos )
/** Bit position of R divider field (same in PLLCFGR, PLLI2SCFGR, PLLSAICFGR) */
#define RCC_PLL_R_POS                   ( 28u )

/** M divider minimal value */
#define RCC_PLL_M_DIV_MIN               ( 2u )
/** M divider maximal value */
#define RCC_PLL_M_DIV_MAX               ( 63u )

/** N multiplier minimal value */
#define RCC_PLL_N_MULT_MIN              ( RCC_PLLN_MIN_VALUE )
/** N multiplier maximal value */
#define RCC_PLL_N_MULT_MAX              ( RCC_PLLN_MAX_VALUE )

/** P divider minimal value */
#define RCC_PLL_P_DIV_MIN               ( 2u )
/** P divider maximal value */
#define RCC_PLL_P_DIV_MAX               ( 8u )
/** P divider step size */
#define RCC_PLL_P_DIV_STEP              ( 2u )

/** Q divider minimal value */
#define RCC_PLL_Q_DIV_MIN               ( 2u )
/** Q divider maximal value */
#define RCC_PLL_Q_DIV_MAX               ( 15u )

/** R divider minimal value */
#define RCC_PLL_R_DIV_MIN               ( 2u )
/** R divider maximal value */
#define RCC_PLL_R_DIV_MAX               ( 7u )

/** Value of output divider meaning "output divider is not configured" */
#define RCC_PLL_OUT_DIV_UNUSED          ( 0u )

/* ============================== TYPEDEFS ================================== */

typedef struct
{
    rcc_PllId_t       PllId;             /**< Phase Locked Loop (PLL) ID                              */

    uint32_t          StateMask;         /**< PLL activation bit mask (RCC_CR)                         */
    uint32_t          RdyFlagMask;       /**< PLL ready flag mask (RCC_CR)                             */

    rcc_RegId_t       CfgRegId;          /**< PLL configuration register ID (N, P, Q, R)               */

    rcc_RegId_t       M_DivRegId;        /**< PLL M divider register ID (own or shared with main PLL)  */
    uint32_t          M_DivMask;         /**< PLL M divider mask                                       */

    uint32_t          N_MultMask;        /**< PLL N multiplier mask                                    */

    uint32_t          Out_P_Mask;        /**< PLL output P divider mask (0 - output not available)     */
    uint32_t          Out_Q_Mask;        /**< PLL output Q divider mask (0 - output not available)     */
    uint32_t          Out_R_Mask;        /**< PLL output R divider mask (0 - output not available)     */

}   rcc_PllConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_FunctionState_t Rcc_Pll_Get_SharedInUse ( rcc_PllId_t pllId, rcc_RegId_t regId, uint32_t regMask );
static rcc_RequestState_t  Rcc_Pll_Get_SourceClk   ( rcc_PllClkSrc_t clkSource, rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_OutClk      ( rcc_PllId_t pllId, uint32_t outMask, uint32_t outPos, rcc_FreqHz_t * const pllClk );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */


/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Phase Locked Loop's (PLL) configuration registry structure */
static const rcc_PllConfig_t            rcc_Pll_Config[] =
{
  {
    .PllId             = RCC_PLL_MAIN,

    .StateMask         = RCC_CR_PLLON,
    .RdyFlagMask       = RCC_CR_PLLRDY,

    .CfgRegId          = RCC_REG_PLLCFGR,

    .M_DivRegId        = RCC_REG_PLLCFGR,
    .M_DivMask         = RCC_PLLCFGR_PLLM,

    .N_MultMask        = RCC_PLLCFGR_PLLN,

    .Out_P_Mask        = RCC_PLLCFGR_PLLP,
    .Out_Q_Mask        = RCC_PLLCFGR_PLLQ,
#if defined(RCC_PLLCFGR_PLLR)
    .Out_R_Mask        = RCC_PLLCFGR_PLLR,
#else
    .Out_R_Mask        = 0u,
#endif /* RCC_PLLCFGR_PLLR */
  },
#if defined(RCC_CR_PLLI2SON)
  {
    .PllId             = RCC_PLL_I2S,

    .StateMask         = RCC_CR_PLLI2SON,
    .RdyFlagMask       = RCC_CR_PLLI2SRDY,

    .CfgRegId          = RCC_REG_PLLI2SCFGR,

#if defined(RCC_PLLI2SCFGR_PLLI2SM)
    .M_DivRegId        = RCC_REG_PLLI2SCFGR,
    .M_DivMask         = RCC_PLLI2SCFGR_PLLI2SM,
#else
    .M_DivRegId        = RCC_REG_PLLCFGR,
    .M_DivMask         = RCC_PLLCFGR_PLLM,
#endif /* RCC_PLLI2SCFGR_PLLI2SM */

    .N_MultMask        = RCC_PLLI2SCFGR_PLLI2SN,

#if defined(RCC_PLLI2SCFGR_PLLI2SP)
    .Out_P_Mask        = RCC_PLLI2SCFGR_PLLI2SP,
#else
    .Out_P_Mask        = 0u,
#endif /* RCC_PLLI2SCFGR_PLLI2SP */
#if defined(RCC_PLLI2SCFGR_PLLI2SQ)
    .Out_Q_Mask        = RCC_PLLI2SCFGR_PLLI2SQ,
#else
    .Out_Q_Mask        = 0u,
#endif /* RCC_PLLI2SCFGR_PLLI2SQ */
    .Out_R_Mask        = RCC_PLLI2SCFGR_PLLI2SR,
  },
#endif /* RCC_CR_PLLI2SON */
#if defined(RCC_CR_PLLSAION)
  {
    .PllId             = RCC_PLL_SAI,

    .StateMask         = RCC_CR_PLLSAION,
    .RdyFlagMask       = RCC_CR_PLLSAIRDY,

    .CfgRegId          = RCC_REG_PLLSAICFGR,

#if defined(RCC_PLLSAICFGR_PLLSAIM)
    .M_DivRegId        = RCC_REG_PLLSAICFGR,
    .M_DivMask         = RCC_PLLSAICFGR_PLLSAIM,
#else
    .M_DivRegId        = RCC_REG_PLLCFGR,
    .M_DivMask         = RCC_PLLCFGR_PLLM,
#endif /* RCC_PLLSAICFGR_PLLSAIM */

    .N_MultMask        = RCC_PLLSAICFGR_PLLSAIN,

#if defined(RCC_PLLSAICFGR_PLLSAIP)
    .Out_P_Mask        = RCC_PLLSAICFGR_PLLSAIP,
#else
    .Out_P_Mask        = 0u,
#endif /* RCC_PLLSAICFGR_PLLSAIP */
    .Out_Q_Mask        = RCC_PLLSAICFGR_PLLSAIQ,
#if defined(RCC_PLLSAICFGR_PLLSAIR)
    .Out_R_Mask        = RCC_PLLSAICFGR_PLLSAIR,
#else
    .Out_R_Mask        = 0u,
#endif /* RCC_PLLSAICFGR_PLLSAIR */
  },
#endif /* RCC_CR_PLLSAION */
};

_Static_assert( (sizeof(rcc_Pll_Config) / sizeof(rcc_PllConfig_t)) == RCC_PLL_CNT, "Rcc_Pll: rcc_Pll_Config has incorrect size." );


/** \brief RTCSEL values of RTC clock sources, indexed by \ref rcc_Rtc_ClkSource_t */
static const uint32_t rcc_Pll_RtcClkSrcLut[ RCC_RTC_CLK_SOURCE_CNT ] =
{
    [RCC_RTC_CLK_SOURCE_HSE_DIV] = LL_RCC_RTC_CLKSOURCE_HSE,
    [RCC_RTC_CLK_SOURCE_LSE]     = LL_RCC_RTC_CLKSOURCE_LSE,
    [RCC_RTC_CLK_SOURCE_LSI]     = LL_RCC_RTC_CLKSOURCE_LSI,
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes Phase Locked Loop (PLL) handler module.
 *
 * During initialization process, module checks correctness of Phase Locked
 * Loop (PLL) configuration structure.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Pll_Init( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_PllId_t pllId = RCC_PLL_MAIN; RCC_PLL_CNT > pllId; pllId++ )
    {
        if( pllId != rcc_Pll_Config[ pllId ].PllId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
    }

    return ( retState );
}


/**
 * \brief De-initializes Phase Locked Loop (PLL) handler module.
 *
 * De-initialization process disables all Phase Locked Loop (PLL) blocks.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Pll_Deinit( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_PllId_t pllId = RCC_PLL_MAIN; RCC_PLL_CNT > pllId; pllId++ )
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
 * \warning Main PLL can not be configured while it is used as system clock.
 * \note PLL source and shared M divider can be changed only if no other PLL
 *       using them is active.
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 * \param configStruct [in]: PLL configuration structure
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Config( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != configStruct ) &&
        ( RCC_PLL_CNT   >  pllId       )    )
    {
        const rcc_PllConfig_t * const pllConfig = &rcc_Pll_Config[ pllId ];

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
            rcc_FreqHz_t   freqInHz  = 0u;
            const uint32_t mRegValue = configStruct->M_Divider << RCC_PLL_M_POS;

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

                if( ( RCC_PLLVCO_INPUT_MIN  > refFreqHz ) ||
                    ( RCC_PLLVCO_INPUT_MAX  < refFreqHz ) ||
                    ( RCC_PLLVCO_OUTPUT_MIN > vcoFreqHz ) ||
                    ( RCC_PLLVCO_OUTPUT_MAX < vcoFreqHz )    )
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

            /* ------------------- Configure PLL M divider ------------------- */

            if( RCC_REQUEST_OK == retState )
            {
                const uint32_t actualMRegValue = Rcc_Get_RegVal( pllConfig->M_DivRegId, pllConfig->M_DivMask );

                if( mRegValue == actualMRegValue )
                {
                    /* Divider is already configured */
                }
                else if( RCC_FUNCTION_INACTIVE == Rcc_Pll_Get_SharedInUse( pllId, pllConfig->M_DivRegId, pllConfig->M_DivMask ) )
                {
                    Rcc_Set_RegVal( pllConfig->M_DivRegId, pllConfig->M_DivMask, mRegValue );
                }
                else
                {
                    /* M divider is shared with other active PLL */
                    retState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* Error during configuration process */
            }

            /* ------------------ Configure PLL N multiplier ----------------- */

            if( RCC_REQUEST_OK == retState )
            {
                Rcc_Set_RegVal( pllConfig->CfgRegId,
                                pllConfig->N_MultMask,
                                configStruct->N_Multiplier << RCC_PLL_N_POS );
            }
            else
            {
                /* Error during configuration process */
            }

            /*-------------------- Configure PLL outputs ---------------------*/

            if( ( RCC_PLL_OUT_DIV_UNUSED != configStruct->P_Divider ) &&
                ( RCC_REQUEST_OK         == retState                )    )
            {
                retState = Rcc_Pll_Set_OutP( pllId, configStruct->P_Divider );
            }
            else
            {
                /* PLL output P is not configured. */
            }

            if( ( RCC_PLL_OUT_DIV_UNUSED != configStruct->Q_Divider ) &&
                ( RCC_REQUEST_OK         == retState                )    )
            {
                retState = Rcc_Pll_Set_OutQ( pllId, configStruct->Q_Divider );
            }
            else
            {
                /* PLL output Q is not configured. */
            }

            if( ( RCC_PLL_OUT_DIV_UNUSED != configStruct->R_Divider ) &&
                ( RCC_REQUEST_OK         == retState                )    )
            {
                retState = Rcc_Pll_Set_OutR( pllId, configStruct->R_Divider );
            }
            else
            {
                /* PLL output R is not configured. */
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
            const uint32_t pllMDiv  = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].M_DivRegId, rcc_Pll_Config[ pllId ].M_DivMask  ) >> RCC_PLL_M_POS;
            const uint32_t pllNMult = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId  , rcc_Pll_Config[ pllId ].N_MultMask ) >> RCC_PLL_N_POS;

            if( 0u != pllMDiv )
            {
                *pllClk = (rcc_FreqHz_t)( ( (uint64_t)inputClkFreq * pllNMult ) / pllMDiv );
            }
            else
            {
                *pllClk = 0u;
            }
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
        Rcc_Set_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( 0u != Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].RdyFlagMask ) )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* PLL is not locked yet, keep return state as error */
                retState = RCC_REQUEST_ERROR;
            }
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
 * \brief De-activation of Phase Locked Loop (PLL) block
 *
 * \note Main PLL can not be de-activated while it is used as system clock.
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
        Rcc_Reset_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( 0u == Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].RdyFlagMask ) )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* PLL is still locked, keep return state as error */
                retState = RCC_REQUEST_ERROR;
            }
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
        const uint32_t readyRegVal    = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].RdyFlagMask );
        const uint32_t pllStateRegVal = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask   );

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
 * The PLL source multiplexer is common for all PLLs. The source can be
 * changed only if no other PLL is active, the PLL itself must be inactive.
 * HSI is activated if selected, HSE must be activated by user.
 *
 * \param pllId     [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI oscillator
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
            else if( RCC_FUNCTION_INACTIVE == Rcc_Pll_Get_SharedInUse( pllId, RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC ) )
            {
                Rcc_Set_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC, targetRegVal );

                retState = RCC_REQUEST_ERROR;

                for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
                {
                    if( targetRegVal == Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC ) )
                    {
                        retState = RCC_REQUEST_OK;
                        break;
                    }
                    else
                    {
                        /* Clock source has not yet been changed, keep return state as error */
                        retState = RCC_REQUEST_ERROR;
                    }
                }
            }
            else
            {
                /* Source multiplexer is used by other active PLL */
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
        else
        {
            *clkSource = RCC_PLL_SRC_HSI;
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
 * \brief Phase Locked Loop (PLL) output P divider configuration.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output P divider value (2, 4, 6, 8).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is not available).
 */
rcc_RequestState_t Rcc_Pll_Set_OutP( rcc_PllId_t pllId, rcc_PllPDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT       >  pllId                             ) &&
        ( 0u                != rcc_Pll_Config[ pllId ].Out_P_Mask ) &&
        ( RCC_PLL_P_DIV_MIN <= divider                           ) &&
        ( RCC_PLL_P_DIV_MAX >= divider                           ) &&
        ( 0u                == ( divider % RCC_PLL_P_DIV_STEP )  )    )
    {
        /* P field: 0 - divider 2, 1 - divider 4, 2 - divider 6, 3 - divider 8 */
        const uint32_t regValue = ( divider / RCC_PLL_P_DIV_STEP ) - 1u;

        Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].CfgRegId,
                        rcc_Pll_Config[ pllId ].Out_P_Mask,
                        regValue << RCC_PLL_P_POS );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect PLL Id, output P divider value or output not available */
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
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    if( ( RCC_PLL_CNT   > pllId  ) &&
        ( RCC_NULL_PTR != pllClk )    )
    {
        const uint32_t outMask = rcc_Pll_Config[ pllId ].Out_P_Mask;

        retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

        if( ( RCC_REQUEST_OK == retState ) &&
            ( 0u             != outMask  )    )
        {
            const uint32_t regVal  = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, outMask ) >> RCC_PLL_P_POS;
            const uint32_t divider = ( regVal + 1u ) * RCC_PLL_P_DIV_STEP;

            *pllClk = pllIntFreq / divider;
        }
        else
        {
            /* Output is not available */
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
 * \brief Phase Locked Loop (PLL) output Q divider configuration.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output Q divider value (2 - 15).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is not available).
 */
rcc_RequestState_t Rcc_Pll_Set_OutQ( rcc_PllId_t pllId, rcc_PllQDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT       >  pllId                             ) &&
        ( 0u                != rcc_Pll_Config[ pllId ].Out_Q_Mask ) &&
        ( RCC_PLL_Q_DIV_MIN <= divider                           ) &&
        ( RCC_PLL_Q_DIV_MAX >= divider                           )    )
    {
        Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].CfgRegId,
                        rcc_Pll_Config[ pllId ].Out_Q_Mask,
                        divider << RCC_PLL_Q_POS );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect PLL Id, output Q divider value or output not available */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
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
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Out_Q_Mask, RCC_PLL_Q_POS, pllClk );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Phase Locked Loop (PLL) output R divider configuration.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output R divider value (2 - 7).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is not available).
 */
rcc_RequestState_t Rcc_Pll_Set_OutR( rcc_PllId_t pllId, rcc_PllRDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT       >  pllId                             ) &&
        ( 0u                != rcc_Pll_Config[ pllId ].Out_R_Mask ) &&
        ( RCC_PLL_R_DIV_MIN <= divider                           ) &&
        ( RCC_PLL_R_DIV_MAX >= divider                           )    )
    {
        Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].CfgRegId,
                        rcc_Pll_Config[ pllId ].Out_R_Mask,
                        divider << RCC_PLL_R_POS );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect PLL Id, output R divider value or output not available */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
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
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Out_R_Mask, RCC_PLL_R_POS, pllClk );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * RTC clock source is located in backup domain - write protection is released
 * automatically. For HSE source the RTC prescaler (RTCPRE) is configured to
 * the lowest divider giving RTC clock lower or equal to 1 MHz, if not
 * configured yet.
 *
 * \warning RTC clock source can be selected only once. It can be changed only
 *          after backup domain reset - error is returned in such a case.
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

        retState = RCC_REQUEST_OK;

        if( RCC_RTC_CLK_SOURCE_HSE_DIV == clkSource )
        {
            /* HSE prescaler for RTC must provide clock before selection */
            retState = Rcc_ClkSrc_Set_HseRtcActive();
        }
        else
        {
            /* No prescaler for other sources */
        }

        if( RCC_REQUEST_OK != retState )
        {
            /* Prescaler configuration failed */
        }
        else if( llClkSource == actualSource )
        {
            /* Clock source already selected */
        }
        else if( LL_RCC_RTC_CLKSOURCE_NONE == actualSource )
        {
            Rcc_Set_RegVal( RCC_REG_BDCR, RCC_BDCR_RTCSEL, llClkSource );

            retState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                if( llClkSource == Rcc_Get_RegVal( RCC_REG_BDCR, RCC_BDCR_RTCSEL ) )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Clock source has not yet been changed, keep return state as error */
                    retState = RCC_REQUEST_ERROR;
                }
            }
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
 * \brief Checks if a register field shared by PLLs is used by other active PLL.
 *
 * PLL source multiplexer (PLLSRC) is used by all PLLs, M divider of the main
 * PLL is used by PLLs without own M divider.
 *
 * \param pllId   [in]: PLL which requests the change of the field
 * \param regId   [in]: Register of the field
 * \param regMask [in]: Mask of the field
 *
 * \return Returns \ref RCC_FUNCTION_ACTIVE if any other active PLL uses the field.
 */
static rcc_FunctionState_t Rcc_Pll_Get_SharedInUse( rcc_PllId_t pllId, rcc_RegId_t regId, uint32_t regMask )
{
    rcc_FunctionState_t inUse = RCC_FUNCTION_INACTIVE;

    for( rcc_PllId_t otherPllId = RCC_PLL_MAIN; RCC_PLL_CNT > otherPllId; otherPllId++ )
    {
        rcc_FunctionState_t usesField = RCC_FUNCTION_INACTIVE;

        if( ( RCC_REG_PLLCFGR    == regId   ) &&
            ( RCC_PLLCFGR_PLLSRC == regMask )    )
        {
            /* PLL source multiplexer is common for all PLLs */
            usesField = RCC_FUNCTION_ACTIVE;
        }
        else if( ( rcc_Pll_Config[ otherPllId ].M_DivRegId == regId   ) &&
                 ( rcc_Pll_Config[ otherPllId ].M_DivMask  == regMask )    )
        {
            /* M divider of main PLL used by this PLL */
            usesField = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            /* Field is not used by this PLL */
        }

        if( ( otherPllId           != pllId                                                                ) &&
            ( RCC_FUNCTION_ACTIVE  == usesField                                                            ) &&
            ( 0u                   != Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ otherPllId ].StateMask ) )    )
        {
            inUse = RCC_FUNCTION_ACTIVE;
            break;
        }
        else
        {
            /* Field not used by this PLL or PLL is inactive */
        }
    }

    return ( inUse );
}


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
        /* Unsupported PLL source */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Calculates frequency of PLL output Q or R (divider field holds divider value).
 *
 * \param pllId   [in]: PLL identification (must be valid)
 * \param outMask [in]: Output divider field mask (0 - output not available)
 * \param outPos  [in]: Output divider field position
 * \param pllClk [out]: Pointer to PLL output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_OutClk( rcc_PllId_t pllId, uint32_t outMask, uint32_t outPos, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    if( ( RCC_NULL_PTR != pllClk  ) &&
        ( 0u           != outMask )    )
    {
        retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t divider = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, outMask ) >> outPos;

            if( 0u != divider )
            {
                *pllClk = pllIntFreq / divider;
            }
            else
            {
                /* Divider value 0 is not allowed - output has no valid frequency */
                *pllClk  = 0u;
                retState = RCC_REQUEST_ERROR;
            }
        }
        else
        {
            /* PLL internal frequency is not available */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
