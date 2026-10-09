/**
 * \author Mr.Nobody
 * \file Rcc_Pll.c
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality.
 *
 * STM32U5 has three PLLs with identical register layout (PLLxCFGR, PLLxDIVR):
 * - input clock MSIS, HSI16 or HSE divided by M (1 - 16) must be 4 - 16 MHz,
 * - VCO frequency (input x N, N 4 - 512) must be 128 - 544 MHz in voltage
 *   range 1 and 2, 128 - 330 MHz in range 3 (PLL is not allowed in range 4),
 * - outputs P, Q, R are divided by 1 - 128 (PLL1 R: 1 or even values only).
 *
 * \note  PLL1 input clock is also the clock of embedded power distribution
 *        booster (EPOD). Booster prescaler PLL1MBOOST is configured
 *        automatically (booster clock at most 16 MHz). PWR has no MCAL module -
 *        booster enable bit is accessed by PWR LL functions directly (see
 *        Rcc.c).
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_Pll.h"                        /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
#include "Rcc_ClkSrc.h"                     /* Clock sources functionality    */
#include "Stm32_pwr.h"                      /* PWR RAL layer (EPOD booster)   */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_PLL_TIMEOUT_RAW             ( 0x84FCB )

/** PLL reference frequency (after M divider) - minimum of range 4 - 8 MHz */
#define RCC_PLL_REF_FREQ_MIN_HZ         ( 4000000u )

/** PLL reference frequency - maximum of range 4 - 8 MHz */
#define RCC_PLL_REF_RANGE_LOW_MAX_HZ    ( 8000000u )

/** PLL reference frequency - maximum of range 8 - 16 MHz */
#define RCC_PLL_REF_FREQ_MAX_HZ         ( 16000000u )

/** VCO frequency minimum */
#define RCC_PLL_VCO_MIN_HZ              ( 128000000u )

/** VCO frequency maximum in voltage range 1 and 2 */
#define RCC_PLL_VCO_MAX_HZ              ( 544000000u )

/** VCO frequency maximum in voltage range 3 */
#define RCC_PLL_VCO_RANGE3_MAX_HZ       ( 330000000u )

/** PLLxM register holds input divider decremented by 1 */
#define RCC_PLL_M_REG_OFFSET            ( 1u )

/** PLLxN register holds multiplication factor decremented by 1 */
#define RCC_PLL_N_REG_OFFSET            ( 1u )

/** PLLxP / PLLxQ / PLLxR registers hold output divider decremented by 1 */
#define RCC_PLL_OUT_DIV_REG_OFFSET      ( 1u )

/** Maximal EPOD booster input clock frequency */
#define RCC_PLL_BOOST_FREQ_MAX_HZ       ( 16000000u )

/** Value of flag function result when the flag is cleared */
#define RCC_PLL_FLAG_CLEARED            ( 0u )

/** PLL M divider minimum */
#define RCC_PLL_M_DIV_MIN_VALUE         ( 1u )

/** PLL M divider maximum */
#define RCC_PLL_M_DIV_MAX_VALUE         ( 16u )

/** PLL N multiplier minimum */
#define RCC_PLL_N_MULT_MIN_VALUE        ( 4u )

/** PLL N multiplier maximum */
#define RCC_PLL_N_MULT_MAX_VALUE        ( 512u )

/** PLL output P / Q / R divider minimum */
#define RCC_PLL_OUT_DIV_MIN_VALUE       ( 1u )

/** PLL output P / Q / R divider maximum */
#define RCC_PLL_OUT_DIV_MAX_VALUE       ( 128u )

/** Divider value accepted by PLL1 R output regardless of parity */
#define RCC_PLL_R_DIV_ODD_ALLOWED       ( 1u )

/** Divider parity check - even divider has remainder 0 */
#define RCC_PLL_EVEN_DIVISOR            ( 2u )

/** Count of EPOD booster prescaler values */
#define RCC_PLL_BOOST_DIV_CNT           ( sizeof( rcc_Pll_BoostDivLut ) / sizeof( rcc_Pll_BoostDivLut[ 0u ] ) )

/* ============================== TYPEDEFS ================================== */

/** \brief Configuration of one PLL (registers are selected by PLL ID, fields have the same position in all PLLs) */
typedef struct
{
    rcc_PllId_t       PllId;             /**< Phase Locked Loop (PLL) ID                   */
    uint32_t          StateMask;         /**< PLL enable bit (RCC CR)                      */
    uint32_t          RdyFlagMask;       /**< PLL ready flag (RCC CR)                      */
    rcc_RegId_t       CfgRegId;          /**< PLL configuration register ID (PLLxCFGR)     */
    rcc_RegId_t       DivRegId;          /**< PLL dividers register ID (PLLxDIVR)          */
    uint32_t          ClkSrcMask;        /**< PLL clock source field (PLLxSRC)             */
    uint32_t          FreqInRangeMask;   /**< Input frequency range field (PLLxRGE)        */
    uint32_t          M_DivMask;         /**< PLL M divider field                          */
    uint32_t          N_MultMask;        /**< PLL N multiplier field                       */
    uint32_t          Out_P_StateMask;   /**< PLL output P enable bit                      */
    uint32_t          Out_P_ConfMask;    /**< PLL output P divider field                   */
    uint32_t          Out_Q_StateMask;   /**< PLL output Q enable bit                      */
    uint32_t          Out_Q_ConfMask;    /**< PLL output Q divider field                   */
    uint32_t          Out_R_StateMask;   /**< PLL output R enable bit                      */
    uint32_t          Out_R_ConfMask;    /**< PLL output R divider field                   */
    rcc_FunctionState_t R_EvenOnly;      /**< Output R accepts 1 or even divider only      */
}   rcc_PllConfig_t;


/** \brief PLL clock source selection value (field value of PLLxSRC, same for all PLLs) */
typedef struct
{
    rcc_PllClkSrc_t ClkSrc;              /**< PLL clock source                             */
    uint32_t        RegValue;            /**< PLLxSRC field value                          */
}   rcc_PllSrcConfig_t;


/** \brief EPOD booster prescaler (PLL1MBOOST) value */
typedef struct
{
    uint32_t Divider;                    /**< Booster input clock divider                  */
    uint32_t RegValue;                   /**< PLL1MBOOST field value                       */
}   rcc_PllBoostDiv_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_Pll_Get_SrcFreq       ( rcc_PllClkSrc_t clkSource, rcc_FreqHz_t * const srcFreq );
static rcc_RequestState_t Rcc_Pll_Set_BoostDivider  ( rcc_FreqHz_t srcFreq );
static rcc_RequestState_t Rcc_Pll_Set_OutDivider    ( rcc_PllId_t pllId, uint32_t confMask, uint32_t fieldPos, uint32_t stateMask, uint32_t divider );
static rcc_RequestState_t Rcc_Pll_Get_OutClk        ( rcc_PllId_t pllId, uint32_t confMask, uint32_t fieldPos, rcc_FreqHz_t * const pllClk );
static rcc_RequestState_t Rcc_Pll_Wait_Flag         ( uint32_t flagMask, rcc_FlagState_t expectedState );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Phase Locked Loop's (PLL) configuration registry structure */
static const rcc_PllConfig_t            rcc_Pll_Config[] =
{
  {
    .PllId             = RCC_PLL_1,
    .StateMask         = RCC_CR_PLL1ON,
    .RdyFlagMask       = RCC_CR_PLL1RDY,
    .CfgRegId          = RCC_REG_PLL1CFGR,
    .DivRegId          = RCC_REG_PLL1DIVR,
    .ClkSrcMask        = RCC_PLL1CFGR_PLL1SRC,
    .FreqInRangeMask   = RCC_PLL1CFGR_PLL1RGE,
    .M_DivMask         = RCC_PLL1CFGR_PLL1M,
    .N_MultMask        = RCC_PLL1DIVR_PLL1N,
    .Out_P_StateMask   = RCC_PLL1CFGR_PLL1PEN,
    .Out_P_ConfMask    = RCC_PLL1DIVR_PLL1P,
    .Out_Q_StateMask   = RCC_PLL1CFGR_PLL1QEN,
    .Out_Q_ConfMask    = RCC_PLL1DIVR_PLL1Q,
    .Out_R_StateMask   = RCC_PLL1CFGR_PLL1REN,
    .Out_R_ConfMask    = RCC_PLL1DIVR_PLL1R,
    .R_EvenOnly        = RCC_FUNCTION_ACTIVE,
  },
  {
    .PllId             = RCC_PLL_2,
    .StateMask         = RCC_CR_PLL2ON,
    .RdyFlagMask       = RCC_CR_PLL2RDY,
    .CfgRegId          = RCC_REG_PLL2CFGR,
    .DivRegId          = RCC_REG_PLL2DIVR,
    .ClkSrcMask        = RCC_PLL2CFGR_PLL2SRC,
    .FreqInRangeMask   = RCC_PLL2CFGR_PLL2RGE,
    .M_DivMask         = RCC_PLL2CFGR_PLL2M,
    .N_MultMask        = RCC_PLL2DIVR_PLL2N,
    .Out_P_StateMask   = RCC_PLL2CFGR_PLL2PEN,
    .Out_P_ConfMask    = RCC_PLL2DIVR_PLL2P,
    .Out_Q_StateMask   = RCC_PLL2CFGR_PLL2QEN,
    .Out_Q_ConfMask    = RCC_PLL2DIVR_PLL2Q,
    .Out_R_StateMask   = RCC_PLL2CFGR_PLL2REN,
    .Out_R_ConfMask    = RCC_PLL2DIVR_PLL2R,
    .R_EvenOnly        = RCC_FUNCTION_INACTIVE,
  },
  {
    .PllId             = RCC_PLL_3,
    .StateMask         = RCC_CR_PLL3ON,
    .RdyFlagMask       = RCC_CR_PLL3RDY,
    .CfgRegId          = RCC_REG_PLL3CFGR,
    .DivRegId          = RCC_REG_PLL3DIVR,
    .ClkSrcMask        = RCC_PLL3CFGR_PLL3SRC,
    .FreqInRangeMask   = RCC_PLL3CFGR_PLL3RGE,
    .M_DivMask         = RCC_PLL3CFGR_PLL3M,
    .N_MultMask        = RCC_PLL3DIVR_PLL3N,
    .Out_P_StateMask   = RCC_PLL3CFGR_PLL3PEN,
    .Out_P_ConfMask    = RCC_PLL3DIVR_PLL3P,
    .Out_Q_StateMask   = RCC_PLL3CFGR_PLL3QEN,
    .Out_Q_ConfMask    = RCC_PLL3DIVR_PLL3Q,
    .Out_R_StateMask   = RCC_PLL3CFGR_PLL3REN,
    .Out_R_ConfMask    = RCC_PLL3DIVR_PLL3R,
    .R_EvenOnly        = RCC_FUNCTION_INACTIVE,
  },
};

_Static_assert( (sizeof(rcc_Pll_Config) / sizeof(rcc_PllConfig_t)) == RCC_PLL_CNT, "Rcc_Pll: rcc_Pll_Config has incorrect size." );


/** \brief PLLxSRC field values, indexed by \ref rcc_PllClkSrc_t (same field position in all PLLs) */
static const rcc_PllSrcConfig_t         rcc_Pll_SrcLut[ RCC_PLL_SRC_CNT ] =
{
    [RCC_PLL_SRC_NONE] = { .ClkSrc = RCC_PLL_SRC_NONE, .RegValue = LL_RCC_PLL1SOURCE_NONE },
    [RCC_PLL_SRC_MSIS] = { .ClkSrc = RCC_PLL_SRC_MSIS, .RegValue = LL_RCC_PLL1SOURCE_MSIS },
    [RCC_PLL_SRC_HSE]  = { .ClkSrc = RCC_PLL_SRC_HSE,  .RegValue = LL_RCC_PLL1SOURCE_HSE  },
    [RCC_PLL_SRC_HSI]  = { .ClkSrc = RCC_PLL_SRC_HSI,  .RegValue = LL_RCC_PLL1SOURCE_HSI  },
};


/** \brief EPOD booster prescaler values (PLL1MBOOST), ascending dividers */
static const rcc_PllBoostDiv_t          rcc_Pll_BoostDivLut[] =
{
    { .Divider =  1u, .RegValue = LL_RCC_PLL1MBOOST_DIV_1  },
    { .Divider =  2u, .RegValue = LL_RCC_PLL1MBOOST_DIV_2  },
    { .Divider =  4u, .RegValue = LL_RCC_PLL1MBOOST_DIV_4  },
    { .Divider =  6u, .RegValue = LL_RCC_PLL1MBOOST_DIV_6  },
    { .Divider =  8u, .RegValue = LL_RCC_PLL1MBOOST_DIV_8  },
    { .Divider = 10u, .RegValue = LL_RCC_PLL1MBOOST_DIV_10 },
    { .Divider = 12u, .RegValue = LL_RCC_PLL1MBOOST_DIV_12 },
    { .Divider = 14u, .RegValue = LL_RCC_PLL1MBOOST_DIV_14 },
    { .Divider = 16u, .RegValue = LL_RCC_PLL1MBOOST_DIV_16 },
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
 */
void Rcc_Pll_Task( void )
{
    return;
}

/*---------------------------- Pll's configuration ---------------------------*/

/**
 * \brief Configuration of Phase Locked Loop (PLL)
 *
 * PLL is switched off, clock source (internal oscillator is started), input
 * divider, input frequency range, multiplier and outputs are configured and
 * PLL is switched on. PLL with source \ref RCC_PLL_SRC_NONE is switched off
 * only.
 *
 * \pre   PLL is not used as system clock (PLL can not be switched off).
 *
 * \param pllId        [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 * \param configStruct [in]: PLL configuration structure. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (PLL is not switched on then).
 */
rcc_RequestState_t Rcc_Pll_Set_Config( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    rcc_RequestState_t retState  = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       srcFreqHz = 0u;

    if( ( RCC_NULL_PTR != configStruct ) &&
        ( RCC_PLL_CNT   > pllId        )    )
    {
        /* PLL must be inactive during configuration */
        retState = Rcc_Pll_Set_Inactive( pllId );

        if( ( RCC_REQUEST_OK   == retState                 ) &&
            ( RCC_PLL_SRC_NONE != configStruct->Pll_Source )    )
        {
            retState = Rcc_Pll_Set_Source( pllId, configStruct->Pll_Source );
        }
        else
        {
            /* PLL is not used or can not be switched off */
        }

        if( ( RCC_REQUEST_OK   == retState                 ) &&
            ( RCC_PLL_SRC_NONE != configStruct->Pll_Source )    )
        {
            retState = Rcc_Pll_Get_SrcFreq( configStruct->Pll_Source, &srcFreqHz );
        }
        else
        {
            /* PLL is not used or source was not selected */
        }

        if( ( RCC_REQUEST_OK   == retState                 ) &&
            ( RCC_PLL_SRC_NONE != configStruct->Pll_Source )    )
        {
            const rcc_PllConfig_t * const pllConfig = &rcc_Pll_Config[ pllId ];
            const uint32_t                vosScale  = LL_PWR_GetRegulVoltageScaling();
            rcc_FreqHz_t                  vcoMaxHz  = RCC_PLL_VCO_MAX_HZ;
            rcc_FreqHz_t                  refFreqHz = 0u;
            rcc_FreqHz_t                  vcoFreqHz = 0u;

            if( LL_PWR_REGU_VOLTAGE_SCALE3 == vosScale )
            {
                vcoMaxHz = RCC_PLL_VCO_RANGE3_MAX_HZ;
            }
            else if( LL_PWR_REGU_VOLTAGE_SCALE4 == vosScale )
            {
                /* PLL is not allowed in voltage range 4 */
                vcoMaxHz = 0u;
            }
            else
            {
                /* Voltage range 1 or 2 */
            }

            if( ( RCC_PLL_M_DIV_MIN_VALUE  <= configStruct->M_Divider    ) &&
                ( RCC_PLL_M_DIV_MAX_VALUE  >= configStruct->M_Divider    ) &&
                ( RCC_PLL_N_MULT_MIN_VALUE <= configStruct->N_Multiplier ) &&
                ( RCC_PLL_N_MULT_MAX_VALUE >= configStruct->N_Multiplier )    )
            {
                refFreqHz = srcFreqHz / configStruct->M_Divider;
                vcoFreqHz = refFreqHz * configStruct->N_Multiplier;
            }
            else
            {
                /* Incorrect PLL internal configuration - frequencies stay 0 */
            }

            if( ( RCC_PLL_REF_FREQ_MIN_HZ > refFreqHz ) ||
                ( RCC_PLL_REF_FREQ_MAX_HZ < refFreqHz ) ||
                ( RCC_PLL_VCO_MIN_HZ      > vcoFreqHz ) ||
                ( vcoMaxHz                < vcoFreqHz )    )
            {
                /* Divider / multiplier out of range, reference or VCO frequency out of range */
                retState = RCC_REQUEST_ERROR;
            }
            else
            {
                uint32_t rangeValue = 0u;

                if( RCC_PLL_REF_RANGE_LOW_MAX_HZ < refFreqHz )
                {
                    /* Input range 8 - 16 MHz */
                    rangeValue = pllConfig->FreqInRangeMask;
                }
                else
                {
                    /* Input range 4 - 8 MHz */
                    rangeValue = 0u;
                }

                Rcc_Set_RegVal( pllConfig->CfgRegId, pllConfig->FreqInRangeMask, rangeValue );

                Rcc_Set_RegVal( pllConfig->CfgRegId,
                                pllConfig->M_DivMask,
                                ( configStruct->M_Divider - RCC_PLL_M_REG_OFFSET ) << RCC_PLL1CFGR_PLL1M_Pos );

                Rcc_Set_RegVal( pllConfig->DivRegId,
                                pllConfig->N_MultMask,
                                ( configStruct->N_Multiplier - RCC_PLL_N_REG_OFFSET ) << RCC_PLL1DIVR_PLL1N_Pos );

                if( RCC_PLL_1 == pllId )
                {
                    retState = Rcc_Pll_Set_BoostDivider( srcFreqHz );
                }
                else
                {
                    /* Only PLL1 input drives EPOD booster */
                }
            }

            /*----------------------- Configure outputs ----------------------*/
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
                /* Some error occurred during PLL configuration. Do not activate the PLL. */
            }
        }
        else
        {
            /* PLL is not used, or source / PLL switch off failed */
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
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to store PLL internal frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error (also if PLL has no clock source).
 */
rcc_RequestState_t Rcc_Pll_Get_InternalClk( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState     = RCC_REQUEST_ERROR;
    rcc_PllClkSrc_t    pllClkSource = RCC_PLL_SRC_NONE;
    rcc_FreqHz_t       inputClkFreq = 0u;

    retState = Rcc_Pll_Get_Source( pllId, &pllClkSource );

    if( ( RCC_REQUEST_OK == retState ) &&
        ( RCC_NULL_PTR   != pllClk   )    )
    {
        retState = Rcc_Pll_Get_SrcFreq( pllClkSource, &inputClkFreq );

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t mDivRegValue  = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, rcc_Pll_Config[ pllId ].M_DivMask  );
            const uint32_t nMultRegValue = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].DivRegId, rcc_Pll_Config[ pllId ].N_MultMask );

            /* PLLxM / PLLxN registers hold divider / multiplier decremented by 1 */
            const uint32_t pllMDiv  = ( mDivRegValue  >> RCC_PLL1CFGR_PLL1M_Pos ) + RCC_PLL_M_REG_OFFSET;
            const uint32_t pllNMult = ( nMultRegValue >> RCC_PLL1DIVR_PLL1N_Pos ) + RCC_PLL_N_REG_OFFSET;

            *pllClk = ( inputClkFreq / pllMDiv ) * pllNMult;
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
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 *
 * \return State of request execution. Returns "OK" if the PLL locked (ready
 *         flag set), otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Active( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Set_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        retState = Rcc_Pll_Wait_Flag( rcc_Pll_Config[ pllId ].RdyFlagMask, RCC_FLAG_ACTIVE );
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
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 *
 * \return State of request execution. Returns "OK" if the PLL stopped (ready
 *         flag cleared), otherwise return error (e.g. PLL used as system clock).
 */
rcc_RequestState_t Rcc_Pll_Set_Inactive( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Reset_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        retState = Rcc_Pll_Wait_Flag( rcc_Pll_Config[ pllId ].RdyFlagMask, RCC_FLAG_INACTIVE );
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
 * \param state [out]: Pointer to store actual PLL state (enabled and locked). Must not be NULL.
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

        if( ( RCC_PLL_FLAG_CLEARED != readyRegVal    ) &&
            ( RCC_PLL_FLAG_CLEARED != pllStateRegVal )    )
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
 * Internal oscillator selected as the source (MSIS, HSI16) is started. HSE
 * is not started (configured by \ref Rcc_Init).
 *
 * \param pllId     [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [in]: Phase Locked Loop's clock source ID, value from \ref rcc_PllClkSrc_t.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Source( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT     > pllId     ) &&
        ( RCC_PLL_SRC_CNT > clkSource )    )
    {
        const uint32_t targetRegVal = rcc_Pll_SrcLut[ clkSource ].RegValue;

        if( RCC_PLL_SRC_MSIS == clkSource )
        {
            retState = Rcc_ClkSrc_Set_MsisActive();
        }
        else if( RCC_PLL_SRC_HSI == clkSource )
        {
            retState = Rcc_ClkSrc_Set_Hsi16Active();
        }
        else
        {
            /* HSE (configured by Rcc_Init) or no source */
            retState = RCC_REQUEST_OK;
        }

        if( RCC_REQUEST_OK == retState )
        {
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, rcc_Pll_Config[ pllId ].ClkSrcMask, targetRegVal );

            retState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, rcc_Pll_Config[ pllId ].ClkSrcMask );

                if( regValue == targetRegVal )
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
        const uint32_t regValue = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].CfgRegId, rcc_Pll_Config[ pllId ].ClkSrcMask );

        for( rcc_PllClkSrc_t srcIdx = RCC_PLL_SRC_NONE; RCC_PLL_SRC_CNT > srcIdx; srcIdx++ )
        {
            if( regValue == rcc_Pll_SrcLut[ srcIdx ].RegValue )
            {
                *clkSource = rcc_Pll_SrcLut[ srcIdx ].ClkSrc;
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


/**
 * \brief Phase Locked Loop (PLL) output P divider configuration.
 *
 * \pre   PLL is switched off (output dividers can be changed only while the
 *        PLL is disabled).
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 * \param divider [in]: Required PLL output P divider value 1 - 128, 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutP( rcc_PllId_t pllId, rcc_PllPDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutDivider( pllId, rcc_Pll_Config[ pllId ].Out_P_ConfMask, RCC_PLL1DIVR_PLL1P_Pos, rcc_Pll_Config[ pllId ].Out_P_StateMask, divider );
    }
    else
    {
        /* Incorrect PLL Id */
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
 * \param pllClk [out]: Pointer to store PLL clock output frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Out_P_ConfMask, RCC_PLL1DIVR_PLL1P_Pos, pllClk );
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
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 * \param divider [in]: Required PLL output Q divider value 1 - 128, 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutQ( rcc_PllId_t pllId, rcc_PllQDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutDivider( pllId, rcc_Pll_Config[ pllId ].Out_Q_ConfMask, RCC_PLL1DIVR_PLL1Q_Pos, rcc_Pll_Config[ pllId ].Out_Q_StateMask, divider );
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency of PLL output Q
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to store PLL clock output frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Out_Q_ConfMask, RCC_PLL1DIVR_PLL1Q_Pos, pllClk );
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
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification, value from \ref rcc_PllId_t.
 * \param divider [in]: Required PLL output R divider value 1 - 128 (PLL1: 1 or
 *                      even value), 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutR( rcc_PllId_t pllId, rcc_PllRDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        const uint32_t remainder = divider % RCC_PLL_EVEN_DIVISOR;

        if( ( RCC_FUNCTION_ACTIVE       == rcc_Pll_Config[ pllId ].R_EvenOnly ) &&
            ( RCC_PLL_R_DIV_ODD_ALLOWED != divider                            ) &&
            ( 0u                        != remainder                          )    )
        {
            /* Odd divider of PLL1 R output is not allowed */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            retState = Rcc_Pll_Set_OutDivider( pllId, rcc_Pll_Config[ pllId ].Out_R_ConfMask, RCC_PLL1DIVR_PLL1R_Pos, rcc_Pll_Config[ pllId ].Out_R_StateMask, divider );
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
 * \brief Reading of output frequency of PLL output R
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to store PLL clock output frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Out_R_ConfMask, RCC_PLL1DIVR_PLL1R_Pos, pllClk );
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
 * \note  RTC clock source can be selected only once - it can be changed only
 *        after backup domain reset. Selection of the already selected source
 *        is accepted.
 *
 * \param clkSource [in]: RTC clock source ID, value from \ref rcc_Rtc_ClkSource_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when other source is already selected).
 */
rcc_RequestState_t Rcc_Pll_Set_RtcClkSource( rcc_Rtc_ClkSource_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_RTC_CLK_SOURCE_CNT > clkSource )
    {
        const uint32_t llClkSource  = rcc_Pll_RtcClkSrcLut[ clkSource ];
        const uint32_t actualSource = LL_RCC_GetRTCClockSource();

        if( llClkSource == actualSource )
        {
            /* Source is already selected */
            retState = RCC_REQUEST_OK;
        }
        else if( LL_RCC_RTC_CLKSOURCE_NONE != actualSource )
        {
            /* Other source is selected - backup domain reset is needed */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            retState = Rcc_ClkSrc_Set_BkUpAccess();

            if( RCC_REQUEST_OK == retState )
            {
                LL_RCC_SetRTCClockSource( llClkSource );

                retState = RCC_REQUEST_ERROR;

                for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
                {
                    const uint32_t registerValue = LL_RCC_GetRTCClockSource();

                    if( registerValue == llClkSource )
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
                /* Backup domain is write protected */
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
 * \brief Reading of clock source of Real Time Clock (RTC) multiplexer
 *
 * \param clkSource [out]: Pointer to store RTC clock source ID. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when no source is selected).
 */
rcc_RequestState_t Rcc_Pll_Get_RtcClkSource( rcc_Rtc_ClkSource_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkSource )
    {
        const uint32_t llClkSource = LL_RCC_GetRTCClockSource();

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
 * \brief Returns frequency of PLL clock source.
 *
 * \param clkSource [in]: PLL clock source, value from \ref rcc_PllClkSrc_t
 * \param srcFreq  [out]: Pointer to store frequency in Hz
 *
 * \return Returns "OK" for MSIS, HSI16 and HSE, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Pll_Get_SrcFreq( rcc_PllClkSrc_t clkSource, rcc_FreqHz_t * const srcFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_SRC_MSIS == clkSource )
    {
        retState = Rcc_ClkSrc_Get_MsisClk( srcFreq );
    }
    else if( RCC_PLL_SRC_HSI == clkSource )
    {
        retState = Rcc_ClkSrc_Get_Hsi16Clk( srcFreq );
    }
    else if( RCC_PLL_SRC_HSE == clkSource )
    {
        retState = Rcc_ClkSrc_Get_HseClk( srcFreq );
    }
    else
    {
        /* PLL has no clock source */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures EPOD booster prescaler (PLL1MBOOST) for PLL1 input clock.
 *
 * The smallest divider giving booster clock at most 16 MHz is selected.
 * Booster (BOOSTEN) is disabled while the prescaler is written and enabled
 * again if it was enabled.
 *
 * \pre   PLL1 is switched off.
 *
 * \param srcFreq [in]: PLL1 input clock frequency in Hz
 *
 * \return Returns "OK" if the prescaler was written, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Pll_Set_BoostDivider( rcc_FreqHz_t srcFreq )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    uint32_t           boostValue  = rcc_Pll_BoostDivLut[ RCC_PLL_BOOST_DIV_CNT - 1u ].RegValue;
    const uint32_t     boostActive = LL_PWR_IsEnabledEPODBooster();

    for( uint32_t lutIdx = 0u; RCC_PLL_BOOST_DIV_CNT > lutIdx; lutIdx++ )
    {
        const rcc_FreqHz_t boostFreq = srcFreq / rcc_Pll_BoostDivLut[ lutIdx ].Divider;

        if( RCC_PLL_BOOST_FREQ_MAX_HZ >= boostFreq )
        {
            boostValue = rcc_Pll_BoostDivLut[ lutIdx ].RegValue;
            break;
        }
        else
        {
            /* Continue with higher divider */
        }
    }

    /* Prescaler can be written only while the booster is disabled */
    LL_PWR_DisableEPODBooster();

    LL_RCC_SetPll1EPodPrescaler( boostValue );

    for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = LL_RCC_GetPll1EPodPrescaler();

        if( boostValue == regValue )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Prescaler has not been applied yet */
            retState = RCC_REQUEST_ERROR;
        }
    }

    if( RCC_PLL_FLAG_CLEARED != boostActive )
    {
        LL_PWR_EnableEPODBooster();
    }
    else
    {
        /* Booster was disabled */
    }

    return ( retState );
}


/**
 * \brief Configures PLL output divider and enables / disables the output.
 *
 * \param pllId     [in]: PLL identification (valid)
 * \param confMask  [in]: Divider field mask (PLLxP / PLLxQ / PLLxR)
 * \param fieldPos  [in]: Divider field position (same in all PLLs)
 * \param stateMask [in]: Output enable bit (PLLxPEN / PLLxQEN / PLLxREN)
 * \param divider   [in]: Divider 1 - 128, 0 disables the output
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Pll_Set_OutDivider( rcc_PllId_t pllId, uint32_t confMask, uint32_t fieldPos, uint32_t stateMask, uint32_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( 0u == divider )
    {
        /* Output is not used */
        Rcc_Reset_RegBit( rcc_Pll_Config[ pllId ].CfgRegId, stateMask );

        retState = RCC_REQUEST_OK;
    }
    else if( ( RCC_PLL_OUT_DIV_MIN_VALUE <= divider ) &&
             ( RCC_PLL_OUT_DIV_MAX_VALUE >= divider )    )
    {
        Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].DivRegId, confMask, ( divider - RCC_PLL_OUT_DIV_REG_OFFSET ) << fieldPos );
        Rcc_Set_RegBit( rcc_Pll_Config[ pllId ].CfgRegId, stateMask );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* Incorrect PLL output divider value */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads PLL output frequency.
 *
 * \param pllId    [in]: PLL identification (valid)
 * \param confMask [in]: Divider field mask (PLLxP / PLLxQ / PLLxR)
 * \param fieldPos [in]: Divider field position (same in all PLLs)
 * \param pllClk  [out]: Pointer to store output frequency in Hz. Must not be NULL.
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Pll_Get_OutClk( rcc_PllId_t pllId, uint32_t confMask, uint32_t fieldPos, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

    if( ( RCC_REQUEST_OK == retState ) &&
        ( RCC_NULL_PTR   != pllClk   )    )
    {
        const uint32_t regVal   = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].DivRegId, confMask );
        const uint32_t divider  = ( regVal >> fieldPos ) + RCC_PLL_OUT_DIV_REG_OFFSET;

        *pllClk = pllIntFreq / divider;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Waits until RCC CR flag reaches expected state.
 *
 * \param flagMask      [in]: Flag mask in RCC CR register
 * \param expectedState [in]: Expected flag state
 *
 * \return Returns "OK" if the flag reached the state before timeout, otherwise
 *         returns error.
 */
static rcc_RequestState_t Rcc_Pll_Wait_Flag( uint32_t flagMask, rcc_FlagState_t expectedState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t  regValue  = Rcc_Get_RegBit( RCC_REG_CR, flagMask );
        rcc_FlagState_t flagState = RCC_FLAG_INACTIVE;

        if( RCC_PLL_FLAG_CLEARED != regValue )
        {
            flagState = RCC_FLAG_ACTIVE;
        }
        else
        {
            flagState = RCC_FLAG_INACTIVE;
        }

        if( expectedState == flagState )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Flag has not reached expected state yet, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
