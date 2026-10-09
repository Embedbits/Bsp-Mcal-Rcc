/**
 * \author Mr.Nobody
 * \file Rcc_Pll.c
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality.
 *
 * STM32L4 / STM32L4+ family PLLs:
 * - Main PLL (\ref RCC_PLL_1) - PLLCFGR, outputs P (SAI), Q (48 MHz clock),
 *   R (system clock)
 * - PLLSAI1  (\ref RCC_PLL_2) - PLLSAI1CFGR, outputs P (SAI), Q (48 MHz clock),
 *   R (ADC) (MCUs with PLLSAI1)
 * - PLLSAI2  (\ref RCC_PLL_3) - PLLSAI2CFGR, outputs P (SAI), Q (STM32L4+),
 *   R (ADC / LTDC) (MCUs with PLLSAI2)
 *
 * All PLLs are clocked from the common PLL source multiplexer (PLLSRC). The
 * input divider M of main PLL (PLLM) is shared with PLLSAI1 / PLLSAI2 on MCUs
 * without own PLLSAIxM divider (STM32L4). Shared settings can be changed only
 * if no other PLL using them is active.
 *
 * Every output has its enable bit (PLLxEN). An output divider value 0 in the
 * configuration disables the output, an output clock of a disabled output is
 * not available (\ref Rcc_Pll_Get_Clk_OutP returns error).
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

/** Bit position of M divider field (same in PLLCFGR, PLLSAI1CFGR, PLLSAI2CFGR) */
#define RCC_PLL_M_POS                   ( RCC_PLLCFGR_PLLM_Pos )
/** Bit position of N multiplier field (same in PLLCFGR, PLLSAI1CFGR, PLLSAI2CFGR) */
#define RCC_PLL_N_POS                   ( RCC_PLLCFGR_PLLN_Pos )
/** Bit position of P divider bit (same in PLLCFGR, PLLSAI1CFGR, PLLSAI2CFGR) */
#define RCC_PLL_P_POS                   ( RCC_PLLCFGR_PLLP_Pos )
/** Bit position of Q divider field (same in PLLCFGR, PLLSAI1CFGR, PLLSAI2CFGR) */
#define RCC_PLL_Q_POS                   ( RCC_PLLCFGR_PLLQ_Pos )
/** Bit position of R divider field (same in PLLCFGR, PLLSAI1CFGR, PLLSAI2CFGR) */
#define RCC_PLL_R_POS                   ( RCC_PLLCFGR_PLLR_Pos )
/** Bit position of P divider field PDIV (same in PLLCFGR, PLLSAI1CFGR, PLLSAI2CFGR) */
#define RCC_PLL_PDIV_POS                ( 27u )

/** M divider minimal value */
#define RCC_PLL_M_DIV_MIN               ( 1u )
/** M divider maximal value of main PLL */
#if defined(RCC_PLLM_DIV_1_16_SUPPORT)
#define RCC_PLL_M_DIV_MAX               ( 16u )
#else
#define RCC_PLL_M_DIV_MAX               ( 8u )
#endif /* RCC_PLLM_DIV_1_16_SUPPORT */
/** M divider maximal value of own M dividers of PLLSAI1 / PLLSAI2 (STM32L4+) */
#define RCC_PLLSAI_M_DIV_MAX            ( 16u )
/** Offset of M divider field value (field = divider - 1) */
#define RCC_PLL_M_DIV_OFFSET            ( 1u )

/** N multiplier minimal value */
#define RCC_PLL_N_MULT_MIN              ( 8u )
/** N multiplier maximal value (main PLL, PLLSAI of STM32L4) */
#define RCC_PLL_N_MULT_MAX              ( 86u )
/** N multiplier maximal value of PLLSAI1 / PLLSAI2 of STM32L4+ */
#define RCC_PLLSAI_N_MULT_MAX_EXT       ( 127u )

/** P divider value selected by P bit cleared */
#define RCC_PLL_P_DIV_7                 ( 7u )
/** P divider value selected by P bit set */
#define RCC_PLL_P_DIV_17                ( 17u )
/** P divider minimal value of PDIV field */
#define RCC_PLL_PDIV_MIN                ( 2u )
/** P divider maximal value of PDIV field */
#define RCC_PLL_PDIV_MAX                ( 31u )

/** Q / R divider minimal value */
#define RCC_PLL_QR_DIV_MIN              ( 2u )
/** Q / R divider maximal value */
#define RCC_PLL_QR_DIV_MAX              ( 8u )
/** Q / R divider step (field = divider / 2 - 1) */
#define RCC_PLL_QR_DIV_STEP             ( 2u )
/** Offset of Q / R divider field value */
#define RCC_PLL_QR_DIV_OFFSET           ( 1u )

/** Value of output divider meaning "output is not used" */
#define RCC_PLL_OUT_DIV_UNUSED          ( 0u )

/** Minimal PLL input (VCO input) frequency in Hz */
#define RCC_PLL_VCO_INPUT_MIN_HZ        ( 4000000u )
/** Maximal PLL input (VCO input) frequency in Hz */
#define RCC_PLL_VCO_INPUT_MAX_HZ        ( 16000000u )
/** Minimal VCO output frequency in Hz */
#define RCC_PLL_VCO_OUTPUT_MIN_HZ       ( 64000000u )
/** Maximal VCO output frequency in Hz */
#define RCC_PLL_VCO_OUTPUT_MAX_HZ       ( 344000000u )

/* ============================== TYPEDEFS ================================== */

/** \brief Phase Locked Loop register description */
typedef struct
{
    rcc_PllId_t       PllId;             /**< Phase Locked Loop (PLL) ID                                  */

    uint32_t          StateMask;         /**< PLL activation bit mask (RCC_CR)                             */
    uint32_t          RdyFlagMask;       /**< PLL ready flag mask (RCC_CR)                                 */

    rcc_RegId_t       CfgRegId;          /**< PLL configuration register ID (N, P, Q, R)                   */

    rcc_RegId_t       M_DivRegId;        /**< PLL M divider register ID (own or shared with main PLL)      */
    uint32_t          M_DivMask;         /**< PLL M divider mask                                           */
    uint32_t          M_DivMax;          /**< PLL M divider maximal value                                  */

    uint32_t          N_MultMask;        /**< PLL N multiplier mask                                        */
    uint32_t          N_MultMax;         /**< PLL N multiplier maximal value                               */

    uint32_t          Out_P_EnMask;      /**< Output P enable bit (0 - output not available)               */
    uint32_t          Out_P_Mask;        /**< Output P divider bit (7 / 17)                                */
    uint32_t          Out_P_DivMask;     /**< Output P divider field PDIV (0 - not available)              */
    uint32_t          Out_Q_EnMask;      /**< Output Q enable bit (0 - output not available)               */
    uint32_t          Out_Q_Mask;        /**< Output Q divider mask                                        */
    uint32_t          Out_R_EnMask;      /**< Output R enable bit (0 - output not available)               */
    uint32_t          Out_R_Mask;        /**< Output R divider mask                                        */

}   rcc_PllConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_FunctionState_t Rcc_Pll_Get_SharedInUse ( rcc_PllId_t pllId, rcc_RegId_t regId, uint32_t regMask );
static rcc_RequestState_t  Rcc_Pll_Get_SourceClk   ( rcc_PllClkSrc_t clkSource, rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Set_OutQR       ( rcc_PllId_t pllId, uint32_t enMask, uint32_t divMask, uint32_t divPos, uint32_t divider );
static rcc_RequestState_t  Rcc_Pll_Get_OutQRClk    ( rcc_PllId_t pllId, uint32_t enMask, uint32_t divMask, uint32_t divPos, rcc_FreqHz_t * const pllClk );
static rcc_RequestState_t  Rcc_Pll_Wait_RegVal     ( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Phase Locked Loop's (PLL) configuration registry structure */
static const rcc_PllConfig_t            rcc_Pll_Config[] =
{
  {
    .PllId             = RCC_PLL_1,

    .StateMask         = RCC_CR_PLLON,
    .RdyFlagMask       = RCC_CR_PLLRDY,

    .CfgRegId          = RCC_REG_PLLCFGR,

    .M_DivRegId        = RCC_REG_PLLCFGR,
    .M_DivMask         = RCC_PLLCFGR_PLLM,
    .M_DivMax          = RCC_PLL_M_DIV_MAX,

    .N_MultMask        = RCC_PLLCFGR_PLLN,
    .N_MultMax         = RCC_PLL_N_MULT_MAX,

#if defined(RCC_PLLCFGR_PLLPEN)
    .Out_P_EnMask      = RCC_PLLCFGR_PLLPEN,
    .Out_P_Mask        = RCC_PLLCFGR_PLLP,
#else
    .Out_P_EnMask      = 0u,
    .Out_P_Mask        = 0u,
#endif /* RCC_PLLCFGR_PLLPEN */
#if defined(RCC_PLLCFGR_PLLPDIV)
    .Out_P_DivMask     = RCC_PLLCFGR_PLLPDIV,
#else
    .Out_P_DivMask     = 0u,
#endif /* RCC_PLLCFGR_PLLPDIV */
    .Out_Q_EnMask      = RCC_PLLCFGR_PLLQEN,
    .Out_Q_Mask        = RCC_PLLCFGR_PLLQ,
    .Out_R_EnMask      = RCC_PLLCFGR_PLLREN,
    .Out_R_Mask        = RCC_PLLCFGR_PLLR,
  },
#if defined(RCC_CR_PLLSAI1ON)
  {
    .PllId             = RCC_PLL_2,

    .StateMask         = RCC_CR_PLLSAI1ON,
    .RdyFlagMask       = RCC_CR_PLLSAI1RDY,

    .CfgRegId          = RCC_REG_PLLSAI1CFGR,

#if defined(RCC_PLLSAI1CFGR_PLLSAI1M)
    .M_DivRegId        = RCC_REG_PLLSAI1CFGR,
    .M_DivMask         = RCC_PLLSAI1CFGR_PLLSAI1M,
    .M_DivMax          = RCC_PLLSAI_M_DIV_MAX,
#else
    .M_DivRegId        = RCC_REG_PLLCFGR,
    .M_DivMask         = RCC_PLLCFGR_PLLM,
    .M_DivMax          = RCC_PLL_M_DIV_MAX,
#endif /* RCC_PLLSAI1CFGR_PLLSAI1M */

    .N_MultMask        = RCC_PLLSAI1CFGR_PLLSAI1N,
#if defined(RCC_PLLSAI1N_MUL_8_127_SUPPORT)
    .N_MultMax         = RCC_PLLSAI_N_MULT_MAX_EXT,
#else
    .N_MultMax         = RCC_PLL_N_MULT_MAX,
#endif /* RCC_PLLSAI1N_MUL_8_127_SUPPORT */

    .Out_P_EnMask      = RCC_PLLSAI1CFGR_PLLSAI1PEN,
    .Out_P_Mask        = RCC_PLLSAI1CFGR_PLLSAI1P,
#if defined(RCC_PLLSAI1CFGR_PLLSAI1PDIV)
    .Out_P_DivMask     = RCC_PLLSAI1CFGR_PLLSAI1PDIV,
#else
    .Out_P_DivMask     = 0u,
#endif /* RCC_PLLSAI1CFGR_PLLSAI1PDIV */
    .Out_Q_EnMask      = RCC_PLLSAI1CFGR_PLLSAI1QEN,
    .Out_Q_Mask        = RCC_PLLSAI1CFGR_PLLSAI1Q,
    .Out_R_EnMask      = RCC_PLLSAI1CFGR_PLLSAI1REN,
    .Out_R_Mask        = RCC_PLLSAI1CFGR_PLLSAI1R,
  },
#endif /* RCC_CR_PLLSAI1ON */
#if defined(RCC_CR_PLLSAI2ON)
  {
    .PllId             = RCC_PLL_3,

    .StateMask         = RCC_CR_PLLSAI2ON,
    .RdyFlagMask       = RCC_CR_PLLSAI2RDY,

    .CfgRegId          = RCC_REG_PLLSAI2CFGR,

#if defined(RCC_PLLSAI2CFGR_PLLSAI2M)
    .M_DivRegId        = RCC_REG_PLLSAI2CFGR,
    .M_DivMask         = RCC_PLLSAI2CFGR_PLLSAI2M,
    .M_DivMax          = RCC_PLLSAI_M_DIV_MAX,
#else
    .M_DivRegId        = RCC_REG_PLLCFGR,
    .M_DivMask         = RCC_PLLCFGR_PLLM,
    .M_DivMax          = RCC_PLL_M_DIV_MAX,
#endif /* RCC_PLLSAI2CFGR_PLLSAI2M */

    .N_MultMask        = RCC_PLLSAI2CFGR_PLLSAI2N,
#if defined(RCC_PLLSAI2N_MUL_8_127_SUPPORT)
    .N_MultMax         = RCC_PLLSAI_N_MULT_MAX_EXT,
#else
    .N_MultMax         = RCC_PLL_N_MULT_MAX,
#endif /* RCC_PLLSAI2N_MUL_8_127_SUPPORT */

    .Out_P_EnMask      = RCC_PLLSAI2CFGR_PLLSAI2PEN,
    .Out_P_Mask        = RCC_PLLSAI2CFGR_PLLSAI2P,
#if defined(RCC_PLLSAI2CFGR_PLLSAI2PDIV)
    .Out_P_DivMask     = RCC_PLLSAI2CFGR_PLLSAI2PDIV,
#else
    .Out_P_DivMask     = 0u,
#endif /* RCC_PLLSAI2CFGR_PLLSAI2PDIV */
#if defined(RCC_PLLSAI2CFGR_PLLSAI2Q)
    .Out_Q_EnMask      = RCC_PLLSAI2CFGR_PLLSAI2QEN,
    .Out_Q_Mask        = RCC_PLLSAI2CFGR_PLLSAI2Q,
#else
    .Out_Q_EnMask      = 0u,
    .Out_Q_Mask        = 0u,
#endif /* RCC_PLLSAI2CFGR_PLLSAI2Q */
    .Out_R_EnMask      = RCC_PLLSAI2CFGR_PLLSAI2REN,
    .Out_R_Mask        = RCC_PLLSAI2CFGR_PLLSAI2R,
  },
#endif /* RCC_CR_PLLSAI2ON */
};

_Static_assert( (sizeof(rcc_Pll_Config) / sizeof(rcc_PllConfig_t)) == RCC_PLL_CNT, "Rcc_Pll: rcc_Pll_Config has incorrect size." );


/** \brief PLLSRC field values of PLL clock sources, indexed by \ref rcc_PllClkSrc_t */
static const uint32_t rcc_Pll_SrcLut[ RCC_PLL_SRC_CNT ] =
{
    [RCC_PLL_SRC_NONE] = LL_RCC_PLLSOURCE_NONE,
    [RCC_PLL_SRC_MSI]  = LL_RCC_PLLSOURCE_MSI,
    [RCC_PLL_SRC_HSI]  = LL_RCC_PLLSOURCE_HSI,
    [RCC_PLL_SRC_HSE]  = LL_RCC_PLLSOURCE_HSE,
};


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
 * During initialization process, module checks correctness of Phase Locked
 * Loop (PLL) configuration structure.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Pll_Init( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        if( pllId != rcc_Pll_Config[ pllId ].PllId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* Record is valid */
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

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        retState = Rcc_Pll_Set_Inactive( pllId );

        if( RCC_REQUEST_OK != retState )
        {
            break;
        }
        else
        {
            /* Continue with next PLL */
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
 * Outputs with divider 0 are disabled.
 *
 * \warning User must configure clock source (HSE) before PLL configuration!
 * \warning Main PLL can not be configured while it is used as system clock.
 * \note PLL source and shared M divider can be changed only if no other PLL
 *       using them is active.
 *
 * \param pllId        [in]: Required Phase Locked Loop (PLL) identification.
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
                 ( pllConfig->M_DivMax < configStruct->M_Divider    ) ||
                 ( RCC_PLL_N_MULT_MIN > configStruct->N_Multiplier ) ||
                 ( pllConfig->N_MultMax < configStruct->N_Multiplier )    )
        {
            /* Incorrect PLL internal configuration */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            rcc_FreqHz_t   freqInHz  = 0u;
            const uint32_t mRegValue = ( configStruct->M_Divider - RCC_PLL_M_DIV_OFFSET ) << RCC_PLL_M_POS;

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

                if( ( RCC_PLL_VCO_INPUT_MIN_HZ  > refFreqHz ) ||
                    ( RCC_PLL_VCO_INPUT_MAX_HZ  < refFreqHz ) ||
                    ( RCC_PLL_VCO_OUTPUT_MIN_HZ > vcoFreqHz ) ||
                    ( RCC_PLL_VCO_OUTPUT_MAX_HZ < vcoFreqHz )    )
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
                const uint32_t            actualMRegValue = Rcc_Get_RegVal( pllConfig->M_DivRegId, pllConfig->M_DivMask );
                const rcc_FunctionState_t sharedInUse     = Rcc_Pll_Get_SharedInUse( pllId, pllConfig->M_DivRegId, pllConfig->M_DivMask );

                if( mRegValue == actualMRegValue )
                {
                    /* Divider is already configured */
                }
                else if( RCC_FUNCTION_INACTIVE == sharedInUse )
                {
                    Rcc_Set_RegVal( pllConfig->M_DivRegId, pllConfig->M_DivMask, mRegValue );

                    retState = Rcc_Pll_Wait_RegVal( pllConfig->M_DivRegId, pllConfig->M_DivMask, mRegValue );
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
                const uint32_t nRegValue = configStruct->N_Multiplier << RCC_PLL_N_POS;

                Rcc_Set_RegVal( pllConfig->CfgRegId, pllConfig->N_MultMask, nRegValue );

                retState = Rcc_Pll_Wait_RegVal( pllConfig->CfgRegId, pllConfig->N_MultMask, nRegValue );
            }
            else
            {
                /* Error during configuration process */
            }

            /*-------------------- Configure PLL outputs ---------------------*/

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutP( pllId, configStruct->P_Divider );
            }
            else
            {
                /* Error during configuration process */
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutQ( pllId, configStruct->Q_Divider );
            }
            else
            {
                /* Error during configuration process */
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutR( pllId, configStruct->R_Divider );
            }
            else
            {
                /* Error during configuration process */
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
 *        otherwise return error (also if no PLL source is selected).
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
            const uint32_t pllMDiv  = ( Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].M_DivRegId, rcc_Pll_Config[ pllId ].M_DivMask  ) >> RCC_PLL_M_POS ) + RCC_PLL_M_DIV_OFFSET;
            const uint32_t pllNMult = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId  , rcc_Pll_Config[ pllId ].N_MultMask ) >> RCC_PLL_N_POS;

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
        Rcc_Set_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        retState = Rcc_Pll_Wait_RegVal( RCC_REG_CR, rcc_Pll_Config[ pllId ].RdyFlagMask, rcc_Pll_Config[ pllId ].RdyFlagMask );
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

        retState = Rcc_Pll_Wait_RegVal( RCC_REG_CR, rcc_Pll_Config[ pllId ].RdyFlagMask, 0u );
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
 * \param state [out]: Pointer to store actual PLL state (active = enabled and locked). Must not be NULL.
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

        if( ( 0u != readyRegVal    ) &&
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
 * HSI16 and MSI are activated if selected, HSE must be activated by user.
 *
 * \param pllId     [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_MSI  : PLL will be clocked by MSI oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI16 oscillator
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Source( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT      >  pllId     ) &&
        ( RCC_PLL_SRC_NONE <  clkSource ) &&
        ( RCC_PLL_SRC_CNT  >  clkSource )    )
    {
        const uint32_t targetRegVal = rcc_Pll_SrcLut[ clkSource ];

        if( RCC_PLL_SRC_HSI == clkSource )
        {
            retState = Rcc_ClkSrc_Set_HsiActive();
        }
        else if( RCC_PLL_SRC_MSI == clkSource )
        {
            retState = Rcc_ClkSrc_Set_MsiActive();
        }
        else
        {
            /* HSE is activated by user */
            retState = RCC_REQUEST_OK;
        }

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t            actualRegVal = Rcc_Get_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC );
            const uint32_t            ownPllOn     = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );
            const rcc_FunctionState_t sharedInUse  = Rcc_Pll_Get_SharedInUse( pllId, RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC );

            if( targetRegVal == actualRegVal )
            {
                /* Clock source is already selected */
            }
            else if( ( 0u                    == ownPllOn    ) &&
                     ( RCC_FUNCTION_INACTIVE == sharedInUse )    )
            {
                Rcc_Set_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC, targetRegVal );

                retState = Rcc_Pll_Wait_RegVal( RCC_REG_PLLCFGR, RCC_PLLCFGR_PLLSRC, targetRegVal );
            }
            else
            {
                /* Source multiplexer is used by the PLL itself or other active PLL */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Oscillator could not be started */
        }
    }
    else
    {
        /* Incorrect PLL Id or clock source */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns clock source selected by Phase Locked Loop (PLL) multiplexer
 *
 * \param pllId      [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [out]: Pointer to store actual clock source of PLL, value from \ref rcc_PllClkSrc_t
 *                         (\ref RCC_PLL_SRC_NONE - no clock selected). Must not be NULL.
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

        *clkSource = RCC_PLL_SRC_NONE;

        for( rcc_PllClkSrc_t srcIdx = RCC_PLL_SRC_NONE; RCC_PLL_SRC_CNT > srcIdx; srcIdx++ )
        {
            if( rcc_Pll_SrcLut[ srcIdx ] == regValue )
            {
                *clkSource = srcIdx;
                break;
            }
            else
            {
                /* Continue with next source */
            }
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
 * The divider is written into PDIV field (2 - 31) on MCUs with PDIV, otherwise
 * into P bit (7 or 17). Divider 0 disables the output.
 *
 * \warning PLL must be inactive.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output P divider value (2 - 31 or 7 / 17, 0 - output disabled).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the divider is not supported).
 */
rcc_RequestState_t Rcc_Pll_Set_OutP( rcc_PllId_t pllId, rcc_PllPDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT <= pllId )
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }
    else if( RCC_PLL_OUT_DIV_UNUSED == divider )
    {
        /* Output is not used (no write if the output is not available) */
        Rcc_Reset_RegBit( rcc_Pll_Config[ pllId ].CfgRegId, rcc_Pll_Config[ pllId ].Out_P_EnMask );

        retState = Rcc_Pll_Wait_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, rcc_Pll_Config[ pllId ].Out_P_EnMask, 0u );
    }
    else if( 0u == rcc_Pll_Config[ pllId ].Out_P_EnMask )
    {
        /* Output P is not available (STM32L41x / L42x) */
        retState = RCC_REQUEST_ERROR;
    }
    else
    {
        const rcc_PllConfig_t * const pllConfig = &rcc_Pll_Config[ pllId ];
        const uint32_t                cfgMask   = pllConfig->Out_P_EnMask | pllConfig->Out_P_Mask | pllConfig->Out_P_DivMask;
        uint32_t                      cfgValue  = pllConfig->Out_P_EnMask;

        retState = RCC_REQUEST_OK;

        if( ( 0u               != pllConfig->Out_P_DivMask ) &&
            ( RCC_PLL_PDIV_MIN <= divider                  ) &&
            ( RCC_PLL_PDIV_MAX >= divider                  )    )
        {
            cfgValue |= divider << RCC_PLL_PDIV_POS;
        }
        else if( RCC_PLL_P_DIV_17 == divider )
        {
            cfgValue |= pllConfig->Out_P_Mask;
        }
        else if( RCC_PLL_P_DIV_7 == divider )
        {
            /* P bit cleared selects divider 7 */
        }
        else
        {
            /* Divider is not supported */
            retState = RCC_REQUEST_ERROR;
        }

        if( RCC_REQUEST_OK == retState )
        {
            Rcc_Set_RegVal( pllConfig->CfgRegId, cfgMask, cfgValue );

            retState = Rcc_Pll_Wait_RegVal( pllConfig->CfgRegId, cfgMask, cfgValue );
        }
        else
        {
            /* Nothing is written */
        }
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
        const rcc_PllConfig_t * const pllConfig = &rcc_Pll_Config[ pllId ];
        const uint32_t                enabled   = Rcc_Get_RegBit( pllConfig->CfgRegId, pllConfig->Out_P_EnMask );

        retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

        if( ( RCC_REQUEST_OK == retState ) &&
            ( 0u             != enabled  )    )
        {
            uint32_t divider = Rcc_Get_RegVal( pllConfig->CfgRegId, pllConfig->Out_P_DivMask ) >> RCC_PLL_PDIV_POS;

            if( RCC_PLL_PDIV_MIN > divider )
            {
                /* PDIV not used - divider given by P bit */
                const uint32_t pBit = Rcc_Get_RegBit( pllConfig->CfgRegId, pllConfig->Out_P_Mask );

                if( 0u != pBit )
                {
                    divider = RCC_PLL_P_DIV_17;
                }
                else
                {
                    divider = RCC_PLL_P_DIV_7;
                }
            }
            else
            {
                /* Divider given by PDIV field */
            }

            *pllClk = pllIntFreq / divider;
        }
        else
        {
            /* Output is disabled or PLL source is not available */
            *pllClk  = 0u;
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
 * \param divider [in]: Required PLL output Q divider value (2, 4, 6, 8, 0 - output disabled).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is not available).
 */
rcc_RequestState_t Rcc_Pll_Set_OutQ( rcc_PllId_t pllId, rcc_PllQDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutQR( pllId, rcc_Pll_Config[ pllId ].Out_Q_EnMask, rcc_Pll_Config[ pllId ].Out_Q_Mask, RCC_PLL_Q_POS, divider );
    }
    else
    {
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
 *         otherwise return error (also if the output is disabled).
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutQRClk( pllId, rcc_Pll_Config[ pllId ].Out_Q_EnMask, rcc_Pll_Config[ pllId ].Out_Q_Mask, RCC_PLL_Q_POS, pllClk );
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
 * \param divider [in]: Required PLL output R divider value (2, 4, 6, 8, 0 - output disabled).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutR( rcc_PllId_t pllId, rcc_PllRDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutQR( pllId, rcc_Pll_Config[ pllId ].Out_R_EnMask, rcc_Pll_Config[ pllId ].Out_R_Mask, RCC_PLL_R_POS, divider );
    }
    else
    {
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
 *         otherwise return error (also if the output is disabled).
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutQRClk( pllId, rcc_Pll_Config[ pllId ].Out_R_EnMask, rcc_Pll_Config[ pllId ].Out_R_Mask, RCC_PLL_R_POS, pllClk );
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
 * automatically.
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

    for( rcc_PllId_t otherPllId = RCC_PLL_1; RCC_PLL_CNT > otherPllId; otherPllId++ )
    {
        rcc_FunctionState_t usesField = RCC_FUNCTION_INACTIVE;
        const uint32_t      otherOn   = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ otherPllId ].StateMask );

        if( ( RCC_REG_PLLCFGR    == regId   ) &&
            ( RCC_PLLCFGR_PLLSRC == regMask )    )
        {
            /* PLL source multiplexer is common for all PLLs */
            usesField = RCC_FUNCTION_ACTIVE;
        }
        else if( ( rcc_Pll_Config[ otherPllId ].M_DivRegId == regId   ) &&
                 ( rcc_Pll_Config[ otherPllId ].M_DivMask  == regMask )    )
        {
            /* M divider used by this PLL */
            usesField = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            /* Field is not used by this PLL */
        }

        if( ( otherPllId           != pllId     ) &&
            ( RCC_FUNCTION_ACTIVE  == usesField ) &&
            ( 0u                   != otherOn   )    )
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
 *         otherwise return error (also for \ref RCC_PLL_SRC_NONE).
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
    else if( RCC_PLL_SRC_MSI == clkSource )
    {
        retState = Rcc_ClkSrc_Get_MsiClk( clkFreq );
    }
    else
    {
        /* No PLL source */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures PLL output Q or R (enable bit and divider field).
 *
 * \param pllId   [in]: PLL identification (valid)
 * \param enMask  [in]: Output enable bit (0 - output not available)
 * \param divMask [in]: Output divider field mask
 * \param divPos  [in]: Output divider field position
 * \param divider [in]: Divider value (2, 4, 6, 8, 0 - output disabled)
 *
 * \return Returns "OK" if the output was configured, otherwise returns error
 *         (nothing is written).
 */
static rcc_RequestState_t Rcc_Pll_Set_OutQR( rcc_PllId_t pllId, uint32_t enMask, uint32_t divMask, uint32_t divPos, uint32_t divider )
{
    rcc_RequestState_t retState  = RCC_REQUEST_ERROR;
    const rcc_RegId_t  cfgRegId  = rcc_Pll_Config[ pllId ].CfgRegId;

    if( RCC_PLL_OUT_DIV_UNUSED == divider )
    {
        /* Output is not used (or not available) */
        if( 0u != enMask )
        {
            Rcc_Reset_RegBit( cfgRegId, enMask );

            retState = Rcc_Pll_Wait_RegVal( cfgRegId, enMask, 0u );
        }
        else
        {
            retState = RCC_REQUEST_OK;
        }
    }
    else if( ( 0u                 != enMask                            ) &&
             ( RCC_PLL_QR_DIV_MIN <= divider                           ) &&
             ( RCC_PLL_QR_DIV_MAX >= divider                           ) &&
             ( 0u                 == ( divider % RCC_PLL_QR_DIV_STEP ) )    )
    {
        /* Field: 0 - divider 2, 1 - divider 4, 2 - divider 6, 3 - divider 8 */
        const uint32_t cfgMask  = enMask | divMask;
        const uint32_t cfgValue = enMask | ( ( ( divider / RCC_PLL_QR_DIV_STEP ) - RCC_PLL_QR_DIV_OFFSET ) << divPos );

        Rcc_Set_RegVal( cfgRegId, cfgMask, cfgValue );

        retState = Rcc_Pll_Wait_RegVal( cfgRegId, cfgMask, cfgValue );
    }
    else
    {
        /* Output not available or divider not supported */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Calculates frequency of PLL output Q or R.
 *
 * \param pllId   [in]: PLL identification (valid)
 * \param enMask  [in]: Output enable bit (0 - output not available)
 * \param divMask [in]: Output divider field mask
 * \param divPos  [in]: Output divider field position
 * \param pllClk [out]: Pointer to PLL output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the output is disabled).
 */
static rcc_RequestState_t Rcc_Pll_Get_OutQRClk( rcc_PllId_t pllId, uint32_t enMask, uint32_t divMask, uint32_t divPos, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    if( ( RCC_NULL_PTR != pllClk ) &&
        ( 0u           != enMask )    )
    {
        const rcc_RegId_t cfgRegId = rcc_Pll_Config[ pllId ].CfgRegId;
        const uint32_t    enabled  = Rcc_Get_RegBit( cfgRegId, enMask );

        retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

        if( ( RCC_REQUEST_OK == retState ) &&
            ( 0u             != enabled  )    )
        {
            const uint32_t fieldVal = Rcc_Get_RegVal( cfgRegId, divMask ) >> divPos;
            const uint32_t divider  = ( fieldVal + RCC_PLL_QR_DIV_OFFSET ) * RCC_PLL_QR_DIV_STEP;

            *pllClk = pllIntFreq / divider;
        }
        else
        {
            /* Output is disabled or PLL source is not available */
            *pllClk  = 0u;
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
        const uint32_t regValue = Rcc_Get_RegVal( regId, regMask );

        if( expectedVal == regValue )
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
