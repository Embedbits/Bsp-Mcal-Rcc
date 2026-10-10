/**
 * \author Mr.Nobody
 * \file Rcc_Pll.c
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality.
 *
 * STM32H7 PLL structure:
 * - one clock source multiplexer common for all PLLs (PLLCKSELR PLLSRC),
 * - input dividers DIVM1 - DIVM3 in PLLCKSELR,
 * - input range, VCO range, fractional enable and output enables of all PLLs
 *   in PLLCFGR,
 * - multiplier and output dividers in PLLxDIVR (same layout for all PLLs).
 * The fractional part of the multiplier is not used (FRACEN = 0).
 *
 * STM32H7R / H7S: output enables PLLxPEN / QEN / REN / SEN / TEN in PLLCFGR,
 * multiplier and P / Q / R dividers in PLLxDIVR1, S / T dividers in PLLxDIVR2
 * (outputs S of all PLLs, output T of PLL2 only).
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_Pll.h"                        /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
#include "Rcc_ClkSrc.h"                     /* Clock sources functionality    */
#include "Stm32_pwr.h"                      /* PWR RAL functionality          */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_PLL_TIMEOUT_RAW             ( 0x84FCB )

/** PLL reference frequency (after DIVM) - minimum of range 1 (1 - 2 MHz) */
#define RCC_PLL_REF_FREQ_MIN_HZ         ( 1000000u )
/** PLL reference frequency - maximum of range 1 (1 - 2 MHz) */
#define RCC_PLL_REF_RANGE1_MAX_HZ       ( 2000000u )
/** PLL reference frequency - maximum of range 2 (2 - 4 MHz) */
#define RCC_PLL_REF_RANGE2_MAX_HZ       ( 4000000u )
/** PLL reference frequency - maximum of range 3 (4 - 8 MHz) */
#define RCC_PLL_REF_RANGE3_MAX_HZ       ( 8000000u )
/** PLL reference frequency - maximum of range 4 (8 - 16 MHz) */
#define RCC_PLL_REF_RANGE4_MAX_HZ       ( 16000000u )

/** Medium VCO range (PLLxVCOSEL = 1, reference 1 - 2 MHz) minimum frequency */
#define RCC_PLL_VCO_MEDIUM_MIN_HZ       ( 150000000u )
/** Medium VCO range (PLLxVCOSEL = 1, reference 1 - 2 MHz) maximum frequency */
#define RCC_PLL_VCO_MEDIUM_MAX_HZ       ( 420000000u )

#if defined(STM32H7RS)
/** Wide VCO range (PLLxVCOSEL = 0) minimum frequency - STM32H7R / H7S (LL / HAL limit) */
#define RCC_PLL_VCO_WIDE_MIN_HZ         ( 400000000u )
/** Wide VCO range (PLLxVCOSEL = 0) maximum frequency - STM32H7R / H7S (LL / HAL limit) */
#define RCC_PLL_VCO_WIDE_MAX_HZ         ( 1600000000u )
/** Minimal value of PLL multiplier - STM32H7R / H7S */
#define RCC_PLL_N_MULT_MIN              ( 8u )
/** Maximal value of PLL multiplier - STM32H7R / H7S */
#define RCC_PLL_N_MULT_MAX              ( 420u )
#elif defined(RCC_VER_2_0)
/** Wide VCO range (PLLxVCOSEL = 0) minimum frequency - STM32H7A3 / H7B0 / H7B3 */
#define RCC_PLL_VCO_WIDE_MIN_HZ         ( 128000000u )
/** Wide VCO range (PLLxVCOSEL = 0) maximum frequency - STM32H7A3 / H7B0 / H7B3 */
#define RCC_PLL_VCO_WIDE_MAX_HZ         ( 544000000u )
/** Minimal value of PLL multiplier - STM32H7A3 / H7B0 / H7B3 */
#define RCC_PLL_N_MULT_MIN              ( 8u )
/** Maximal value of PLL multiplier - STM32H7A3 / H7B0 / H7B3 */
#define RCC_PLL_N_MULT_MAX              ( 420u )
#else
/** Wide VCO range (PLLxVCOSEL = 0) minimum frequency */
#define RCC_PLL_VCO_WIDE_MIN_HZ         ( 192000000u )
#if (STM32H7_DEV_ID == 0x450UL)
/** Wide VCO range maximum frequency - STM32H742 / H743 / H745 / H747 / H750 /
 *  H753 / H755 / H757 (960 MHz needed for 480 MHz system clock in VOS0) */
#define RCC_PLL_VCO_WIDE_MAX_HZ         ( 960000000u )
#else
/** Wide VCO range (PLLxVCOSEL = 0) maximum frequency */
#define RCC_PLL_VCO_WIDE_MAX_HZ         ( 836000000u )
#endif
/** Minimal value of PLL multiplier */
#define RCC_PLL_N_MULT_MIN              ( 4u )
/** Maximal value of PLL multiplier */
#define RCC_PLL_N_MULT_MAX              ( 512u )
#endif

#if (STM32H7_DEV_ID == 0x450UL)
/** PLL1 P output - minimal division factor (even factors only) */
#define RCC_PLL_P1_DIV_MIN              ( 2u )
#else
/** PLL1 P output - minimal division factor (1 or even factors) */
#define RCC_PLL_P1_DIV_MIN              ( 1u )
#endif

#if defined(STM32H7RS)
/** Step of PLL1 P output division factors (all factors on STM32H7R / H7S) */
#define RCC_PLL_P1_DIV_STEP             ( 1u )
#else
/** Step of PLL1 P output division factors (even factors only) */
#define RCC_PLL_P1_DIV_STEP             ( 2u )
#endif

/** Minimal value of PLL input divider (DIVMx = 0 disables the prescaler) */
#define RCC_PLL_M_DIV_MIN               ( 1u )
/** Maximal value of PLL input divider */
#define RCC_PLL_M_DIV_MAX               ( 63u )

/** Minimal value of PLL output dividers */
#define RCC_PLL_OUT_DIV_MIN             ( 1u )
/** Maximal value of PLL output dividers */
#define RCC_PLL_OUT_DIV_MAX             ( 128u )

#if defined(STM32H7RS)
/** Maximal value of PLL output S / T dividers (STM32H7R / H7S) */
#define RCC_PLL_OUT_ST_DIV_MAX          ( 8u )

/* Output enables (PLLCFGR) and divider fields (PLLxDIVR1 / PLLxDIVR2) - STM32H7R / H7S */
#define RCC_PLL1_P_EN                   ( RCC_PLLCFGR_PLL1PEN )
#define RCC_PLL1_Q_EN                   ( RCC_PLLCFGR_PLL1QEN )
#define RCC_PLL1_R_EN                   ( RCC_PLLCFGR_PLL1REN )
#define RCC_PLL1_S_EN                   ( RCC_PLLCFGR_PLL1SEN )
#define RCC_PLL1_N_DIV                  ( RCC_PLL1DIVR1_DIVN )
#define RCC_PLL1_P_DIV                  ( RCC_PLL1DIVR1_DIVP )
#define RCC_PLL1_Q_DIV                  ( RCC_PLL1DIVR1_DIVQ )
#define RCC_PLL1_R_DIV                  ( RCC_PLL1DIVR1_DIVR )
#define RCC_PLL1_S_DIV                  ( RCC_PLL1DIVR2_DIVS )
#define RCC_PLL2_P_EN                   ( RCC_PLLCFGR_PLL2PEN )
#define RCC_PLL2_Q_EN                   ( RCC_PLLCFGR_PLL2QEN )
#define RCC_PLL2_R_EN                   ( RCC_PLLCFGR_PLL2REN )
#define RCC_PLL2_S_EN                   ( RCC_PLLCFGR_PLL2SEN )
#define RCC_PLL2_T_EN                   ( RCC_PLLCFGR_PLL2TEN )
#define RCC_PLL2_N_DIV                  ( RCC_PLL2DIVR1_DIVN )
#define RCC_PLL2_P_DIV                  ( RCC_PLL2DIVR1_DIVP )
#define RCC_PLL2_Q_DIV                  ( RCC_PLL2DIVR1_DIVQ )
#define RCC_PLL2_R_DIV                  ( RCC_PLL2DIVR1_DIVR )
#define RCC_PLL2_S_DIV                  ( RCC_PLL2DIVR2_DIVS )
#define RCC_PLL2_T_DIV                  ( RCC_PLL2DIVR2_DIVT )
#define RCC_PLL3_P_EN                   ( RCC_PLLCFGR_PLL3PEN )
#define RCC_PLL3_Q_EN                   ( RCC_PLLCFGR_PLL3QEN )
#define RCC_PLL3_R_EN                   ( RCC_PLLCFGR_PLL3REN )
#define RCC_PLL3_S_EN                   ( RCC_PLLCFGR_PLL3SEN )
#define RCC_PLL3_N_DIV                  ( RCC_PLL3DIVR1_DIVN )
#define RCC_PLL3_P_DIV                  ( RCC_PLL3DIVR1_DIVP )
#define RCC_PLL3_Q_DIV                  ( RCC_PLL3DIVR1_DIVQ )
#define RCC_PLL3_R_DIV                  ( RCC_PLL3DIVR1_DIVR )
#define RCC_PLL3_S_DIV                  ( RCC_PLL3DIVR2_DIVS )
/** Output not available on the PLL (output T of PLL1 / PLL3) */
#define RCC_PLL_OUT_NOT_AVAILABLE       ( 0u )
#else
/* Output enables (PLLCFGR) and divider fields (PLLxDIVR) */
#define RCC_PLL1_P_EN                   ( RCC_PLLCFGR_DIVP1EN )
#define RCC_PLL1_Q_EN                   ( RCC_PLLCFGR_DIVQ1EN )
#define RCC_PLL1_R_EN                   ( RCC_PLLCFGR_DIVR1EN )
#define RCC_PLL1_N_DIV                  ( RCC_PLL1DIVR_N1 )
#define RCC_PLL1_P_DIV                  ( RCC_PLL1DIVR_P1 )
#define RCC_PLL1_Q_DIV                  ( RCC_PLL1DIVR_Q1 )
#define RCC_PLL1_R_DIV                  ( RCC_PLL1DIVR_R1 )
#define RCC_PLL2_P_EN                   ( RCC_PLLCFGR_DIVP2EN )
#define RCC_PLL2_Q_EN                   ( RCC_PLLCFGR_DIVQ2EN )
#define RCC_PLL2_R_EN                   ( RCC_PLLCFGR_DIVR2EN )
#define RCC_PLL2_N_DIV                  ( RCC_PLL2DIVR_N2 )
#define RCC_PLL2_P_DIV                  ( RCC_PLL2DIVR_P2 )
#define RCC_PLL2_Q_DIV                  ( RCC_PLL2DIVR_Q2 )
#define RCC_PLL2_R_DIV                  ( RCC_PLL2DIVR_R2 )
#define RCC_PLL3_P_EN                   ( RCC_PLLCFGR_DIVP3EN )
#define RCC_PLL3_Q_EN                   ( RCC_PLLCFGR_DIVQ3EN )
#define RCC_PLL3_R_EN                   ( RCC_PLLCFGR_DIVR3EN )
#define RCC_PLL3_N_DIV                  ( RCC_PLL3DIVR_N3 )
#define RCC_PLL3_P_DIV                  ( RCC_PLL3DIVR_P3 )
#define RCC_PLL3_Q_DIV                  ( RCC_PLL3DIVR_Q3 )
#define RCC_PLL3_R_DIV                  ( RCC_PLL3DIVR_R3 )
#endif

/* PLLxRGE / PLLxVCOSEL field values (RM encoding - the LL values are field values on
 * STM32H72x / H74x / H7A3, but values in PLL1 field position on STM32H7R / H7S) */
/** PLLxRGE field value - reference frequency 1 - 2 MHz */
#define RCC_PLL_RGE_1_2_MHZ             ( 0u )
/** PLLxRGE field value - reference frequency 2 - 4 MHz */
#define RCC_PLL_RGE_2_4_MHZ             ( 1u )
/** PLLxRGE field value - reference frequency 4 - 8 MHz */
#define RCC_PLL_RGE_4_8_MHZ             ( 2u )
/** PLLxRGE field value - reference frequency 8 - 16 MHz */
#define RCC_PLL_RGE_8_16_MHZ            ( 3u )
/** PLLxVCOSEL field value - wide VCO range */
#define RCC_PLL_VCOSEL_WIDE             ( 0u )
/** PLLxVCOSEL field value - medium VCO range */
#define RCC_PLL_VCOSEL_MEDIUM           ( 1u )

/** PLLxN register holds multiplication factor decremented by 1 */
#define RCC_PLL_N_REG_OFFSET            ( 1u )

/** PLLxP / PLLxQ / PLLxR registers hold output divider decremented by 1 */
#define RCC_PLL_OUT_DIV_REG_OFFSET      ( 1u )

/* ============================== TYPEDEFS ================================== */

typedef struct
{
    rcc_PllId_t       PllId;             /**< Phase Locked Loop (PLL) ID                           */

    uint32_t          StateMask;         /**< PLL activation bit in CR                             */
    uint32_t          RdyFlagMask;       /**< PLL ready flag in CR                                 */

    uint32_t          M_DivMask;         /**< PLL input divider DIVMx field in PLLCKSELR           */

    uint32_t          FracEnMask;        /**< Fractional divider enable in PLLCFGR                 */
    uint32_t          VcoRangeMask;      /**< VCO frequency range selection in PLLCFGR             */
    uint32_t          FreqInRangeMask;   /**< Input frequency range selection in PLLCFGR           */
    uint32_t          Out_P_StateMask;   /**< Output P enable in PLLCFGR                           */
    uint32_t          Out_Q_StateMask;   /**< Output Q enable in PLLCFGR                           */
    uint32_t          Out_R_StateMask;   /**< Output R enable in PLLCFGR                           */

    rcc_RegId_t       DivRegId;          /**< PLLxDIVR register (multiplier and output dividers)   */
    uint32_t          N_MultMask;        /**< Multiplier field in PLLxDIVR                         */
    uint32_t          Out_P_ConfMask;    /**< Output P divider field in PLLxDIVR                   */
    uint32_t          Out_Q_ConfMask;    /**< Output Q divider field in PLLxDIVR                   */
    uint32_t          Out_R_ConfMask;    /**< Output R divider field in PLLxDIVR                   */

    rcc_PllPDivider_t Out_P_DivStepsize; /**< PLL P divider step size                              */
    rcc_PllPDivider_t Out_P_DivMinValue; /**< PLL P divider minimal value (allowed also off-step) */

#if defined(STM32H7RS)
    uint32_t          Out_S_StateMask;   /**< Output S enable in PLLCFGR                           */
    uint32_t          Out_T_StateMask;   /**< Output T enable in PLLCFGR (0 - output not available) */
    rcc_RegId_t       Div2RegId;         /**< PLLxDIVR2 register (output S / T dividers)           */
    uint32_t          Out_S_ConfMask;    /**< Output S divider field in PLLxDIVR2                  */
    uint32_t          Out_T_ConfMask;    /**< Output T divider field in PLLxDIVR2                  */
#endif

}   rcc_PllConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static uint32_t           Rcc_Pll_Get_FieldPos   ( uint32_t fieldMask );
static rcc_RequestState_t Rcc_Pll_Set_OutDivider ( rcc_RegId_t divRegId, uint32_t confMask, uint32_t stateMask, uint32_t divider, uint32_t dividerMax );
static rcc_RequestState_t Rcc_Pll_Get_OutClk     ( rcc_PllId_t pllId, rcc_RegId_t divRegId, uint32_t confMask, rcc_FreqHz_t * const pllClk );

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
    .M_DivMask         = RCC_PLLCKSELR_DIVM1,
    .FracEnMask        = RCC_PLLCFGR_PLL1FRACEN,
    .VcoRangeMask      = RCC_PLLCFGR_PLL1VCOSEL,
    .FreqInRangeMask   = RCC_PLLCFGR_PLL1RGE,
    .Out_P_StateMask   = RCC_PLL1_P_EN,
    .Out_Q_StateMask   = RCC_PLL1_Q_EN,
    .Out_R_StateMask   = RCC_PLL1_R_EN,
    .DivRegId          = RCC_REG_PLL1DIVR,
    .N_MultMask        = RCC_PLL1_N_DIV,
    .Out_P_ConfMask    = RCC_PLL1_P_DIV,
    .Out_Q_ConfMask    = RCC_PLL1_Q_DIV,
    .Out_R_ConfMask    = RCC_PLL1_R_DIV,
    .Out_P_DivStepsize = RCC_PLL_P1_DIV_STEP,
    .Out_P_DivMinValue = RCC_PLL_P1_DIV_MIN,
#if defined(STM32H7RS)
    .Out_S_StateMask   = RCC_PLL1_S_EN,
    .Out_T_StateMask   = RCC_PLL_OUT_NOT_AVAILABLE,
    .Div2RegId         = RCC_REG_PLL1DIVR2,
    .Out_S_ConfMask    = RCC_PLL1_S_DIV,
    .Out_T_ConfMask    = RCC_PLL_OUT_NOT_AVAILABLE,
#endif
  },
  {
    .PllId             = RCC_PLL_2,
    .StateMask         = RCC_CR_PLL2ON,
    .RdyFlagMask       = RCC_CR_PLL2RDY,
    .M_DivMask         = RCC_PLLCKSELR_DIVM2,
    .FracEnMask        = RCC_PLLCFGR_PLL2FRACEN,
    .VcoRangeMask      = RCC_PLLCFGR_PLL2VCOSEL,
    .FreqInRangeMask   = RCC_PLLCFGR_PLL2RGE,
    .Out_P_StateMask   = RCC_PLL2_P_EN,
    .Out_Q_StateMask   = RCC_PLL2_Q_EN,
    .Out_R_StateMask   = RCC_PLL2_R_EN,
    .DivRegId          = RCC_REG_PLL2DIVR,
    .N_MultMask        = RCC_PLL2_N_DIV,
    .Out_P_ConfMask    = RCC_PLL2_P_DIV,
    .Out_Q_ConfMask    = RCC_PLL2_Q_DIV,
    .Out_R_ConfMask    = RCC_PLL2_R_DIV,
    .Out_P_DivStepsize = 1u,
    .Out_P_DivMinValue = RCC_PLL_OUT_DIV_MIN,
#if defined(STM32H7RS)
    .Out_S_StateMask   = RCC_PLL2_S_EN,
    .Out_T_StateMask   = RCC_PLL2_T_EN,
    .Div2RegId         = RCC_REG_PLL2DIVR2,
    .Out_S_ConfMask    = RCC_PLL2_S_DIV,
    .Out_T_ConfMask    = RCC_PLL2_T_DIV,
#endif
  },
  {
    .PllId             = RCC_PLL_3,
    .StateMask         = RCC_CR_PLL3ON,
    .RdyFlagMask       = RCC_CR_PLL3RDY,
    .M_DivMask         = RCC_PLLCKSELR_DIVM3,
    .FracEnMask        = RCC_PLLCFGR_PLL3FRACEN,
    .VcoRangeMask      = RCC_PLLCFGR_PLL3VCOSEL,
    .FreqInRangeMask   = RCC_PLLCFGR_PLL3RGE,
    .Out_P_StateMask   = RCC_PLL3_P_EN,
    .Out_Q_StateMask   = RCC_PLL3_Q_EN,
    .Out_R_StateMask   = RCC_PLL3_R_EN,
    .DivRegId          = RCC_REG_PLL3DIVR,
    .N_MultMask        = RCC_PLL3_N_DIV,
    .Out_P_ConfMask    = RCC_PLL3_P_DIV,
    .Out_Q_ConfMask    = RCC_PLL3_Q_DIV,
    .Out_R_ConfMask    = RCC_PLL3_R_DIV,
    .Out_P_DivStepsize = 1u,
    .Out_P_DivMinValue = RCC_PLL_OUT_DIV_MIN,
#if defined(STM32H7RS)
    .Out_S_StateMask   = RCC_PLL3_S_EN,
    .Out_T_StateMask   = RCC_PLL_OUT_NOT_AVAILABLE,
    .Div2RegId         = RCC_REG_PLL3DIVR2,
    .Out_S_ConfMask    = RCC_PLL3_S_DIV,
    .Out_T_ConfMask    = RCC_PLL_OUT_NOT_AVAILABLE,
#endif
  },
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

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        if( pllId != rcc_Pll_Config[ pllId ].PllId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* No action required */
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

        if( retState != RCC_REQUEST_OK )
        {
            break;
        }
        else
        {
            /* No action required */
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
 * The PLL is stopped, configured (source, input divider M, multiplier N,
 * input and VCO range, output dividers P / Q / R, S / T on STM32H7R / H7S) and
 * started. Output dividers with value 0 disable the output.
 *
 * Range selection: reference frequency (source / M) above 2 MHz uses the wide VCO
 * range (192 - 836 MHz, up to 960 MHz on STM32H742 / H743 / H745 / H747 / H750 /
 * H753 / H755 / H757, 128 - 544 MHz on STM32H7A3 / H7B0 / H7B3, 400 - 1600 MHz
 * on STM32H7R / H7S) if the VCO frequency fits it, otherwise the medium VCO
 * range (150 - 420 MHz) is used. Wide range is not allowed with reference
 * frequency 1 - 2 MHz.
 *
 * \warning The source of all PLLs is common - the source can be changed only
 *          while all PLLs are stopped.
 *
 * \param pllId        [in]: Required Phase Locked Loop (PLL) identification.
 * \param configStruct [in]: PLL configuration structure
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (the PLL stays stopped).
 */
rcc_RequestState_t Rcc_Pll_Set_Config( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != configStruct ) &&
        ( RCC_PLL_CNT   >  pllId       )    )
    {
        /* PLL must be inactive during configuration (and is stopped if not used) */
        retState = Rcc_Pll_Set_Inactive( pllId );

        if( ( RCC_REQUEST_OK   == retState                 ) &&
            ( RCC_PLL_SRC_NONE != configStruct->Pll_Source )    )
        {
            rcc_FreqHz_t freqInHz  = 0u;
            rcc_FreqHz_t refFreqHz = 0u;
            rcc_FreqHz_t vcoFreqHz = 0u;
            uint32_t     rangeVal  = 0u;
            uint32_t     vcoSelVal = 0u;

            retState = Rcc_Pll_Set_Source( pllId, configStruct->Pll_Source );

            /*------------- Check PLL internal configuration -----------------*/

            if( ( RCC_REQUEST_OK    == retState                   ) &&
                ( RCC_PLL_M_DIV_MIN <= configStruct->M_Divider    ) &&
                ( RCC_PLL_M_DIV_MAX >= configStruct->M_Divider    ) &&
                ( RCC_PLL_N_MULT_MIN <= configStruct->N_Multiplier ) &&
                ( RCC_PLL_N_MULT_MAX >= configStruct->N_Multiplier )    )
            {
                if( RCC_PLL_SRC_CSI == configStruct->Pll_Source )
                {
                    retState = Rcc_ClkSrc_Get_CsiClk( &freqInHz );
                }
                else if( RCC_PLL_SRC_HSE == configStruct->Pll_Source )
                {
                    retState = Rcc_ClkSrc_Get_HseClk( &freqInHz );
                }
                else
                {
                    retState = Rcc_ClkSrc_Get_Hsi64Clk( &freqInHz );
                }

                /* Input range is defined for reference frequency after DIVM divider */
                refFreqHz = freqInHz / configStruct->M_Divider;
                vcoFreqHz = refFreqHz * configStruct->N_Multiplier;
            }
            else
            {
                /* Incorrect PLL source or internal configuration */
                retState = RCC_REQUEST_ERROR;
            }

            /* ------------ Select PLL input frequency range --------------- */

            if( RCC_REQUEST_OK == retState )
            {
                if( ( RCC_PLL_REF_FREQ_MIN_HZ   <= refFreqHz ) &&
                    ( RCC_PLL_REF_RANGE1_MAX_HZ >= refFreqHz )    )
                {
                    rangeVal = RCC_PLL_RGE_1_2_MHZ;
                }
                else if( ( RCC_PLL_REF_RANGE1_MAX_HZ <  refFreqHz ) &&
                         ( RCC_PLL_REF_RANGE2_MAX_HZ >= refFreqHz )    )
                {
                    rangeVal = RCC_PLL_RGE_2_4_MHZ;
                }
                else if( ( RCC_PLL_REF_RANGE2_MAX_HZ <  refFreqHz ) &&
                         ( RCC_PLL_REF_RANGE3_MAX_HZ >= refFreqHz )    )
                {
                    rangeVal = RCC_PLL_RGE_4_8_MHZ;
                }
                else if( ( RCC_PLL_REF_RANGE3_MAX_HZ <  refFreqHz ) &&
                         ( RCC_PLL_REF_RANGE4_MAX_HZ >= refFreqHz )    )
                {
                    rangeVal = RCC_PLL_RGE_8_16_MHZ;
                }
                else
                {
                    /* Reference frequency out of PLL input range */
                    retState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* Error during configuration process */
            }

            /* ---------------- Select VCO frequency range ------------------ */

            if( RCC_REQUEST_OK == retState )
            {
                /* Wide VCO range is not allowed with reference frequency 1 - 2 MHz (PLLxRGE = 0) */
                if( ( RCC_PLL_REF_RANGE1_MAX_HZ <  refFreqHz ) &&
                    ( RCC_PLL_VCO_WIDE_MIN_HZ   <= vcoFreqHz ) &&
                    ( RCC_PLL_VCO_WIDE_MAX_HZ   >= vcoFreqHz )    )
                {
                    vcoSelVal = RCC_PLL_VCOSEL_WIDE;
                }
                else if( ( RCC_PLL_VCO_MEDIUM_MIN_HZ <= vcoFreqHz ) &&
                         ( RCC_PLL_VCO_MEDIUM_MAX_HZ >= vcoFreqHz )    )
                {
                    vcoSelVal = RCC_PLL_VCOSEL_MEDIUM;
                }
                else
                {
                    /* VCO frequency out of the VCO range of the reference frequency */
                    retState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* Error during configuration process */
            }

            /* ------------ Write PLL internal configuration --------------- */

            if( RCC_REQUEST_OK == retState )
            {
                const rcc_PllConfig_t * const pllCfg = &rcc_Pll_Config[ pllId ];

                Rcc_Set_RegVal( RCC_REG_PLLCKSELR, pllCfg->M_DivMask,
                                configStruct->M_Divider << Rcc_Pll_Get_FieldPos( pllCfg->M_DivMask ) );

                Rcc_Set_RegVal( pllCfg->DivRegId, pllCfg->N_MultMask,
                                ( configStruct->N_Multiplier - RCC_PLL_N_REG_OFFSET ) << Rcc_Pll_Get_FieldPos( pllCfg->N_MultMask ) );

                Rcc_Reset_RegBit( RCC_REG_PLLCFGR, pllCfg->FracEnMask );

                Rcc_Set_RegVal( RCC_REG_PLLCFGR, pllCfg->FreqInRangeMask,
                                rangeVal << Rcc_Pll_Get_FieldPos( pllCfg->FreqInRangeMask ) );

                Rcc_Set_RegVal( RCC_REG_PLLCFGR, pllCfg->VcoRangeMask,
                                vcoSelVal << Rcc_Pll_Get_FieldPos( pllCfg->VcoRangeMask ) );
            }
            else
            {
                /* Error during configuration process */
            }

            /*----------------- Configure PLL outputs P / Q / R --------------*/

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutP( pllId, configStruct->P_Divider );
            }
            else
            {
                /* Previous step failed */
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutQ( pllId, configStruct->Q_Divider );
            }
            else
            {
                /* Previous step failed */
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutR( pllId, configStruct->R_Divider );
            }
            else
            {
                /* Previous step failed */
            }

#if defined(STM32H7RS)
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutS( pllId, configStruct->S_Divider );
            }
            else
            {
                /* Previous step failed */
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_OutT( pllId, configStruct->T_Divider );
            }
            else
            {
                /* Previous step failed */
            }
#endif

            /* ------------- Activate PLL if no error occurred -------------- */

            if( RCC_REQUEST_OK == retState )
            {
                /* Activate PLL and wait for the lock. This must be the last step! */
                retState = Rcc_Pll_Set_Active( pllId );
            }
            else
            {
                /* Some error occurred during PLL configuration. Do not activate
                 * the PLL. */
            }
        }
        else
        {
            /* PLL is not used (stopped) or could not be stopped */
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

    retState = Rcc_Pll_Get_Source( pllId, &pllClkSource );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        if( RCC_PLL_SRC_HSE == pllClkSource )
        {
            retState = Rcc_ClkSrc_Get_HseClk( &inputClkFreq );
        }
        else if( RCC_PLL_SRC_CSI == pllClkSource )
        {
            retState = Rcc_ClkSrc_Get_CsiClk( &inputClkFreq );
        }
        else if( RCC_PLL_SRC_HSI == pllClkSource )
        {
            retState = Rcc_ClkSrc_Get_Hsi64Clk( &inputClkFreq );
        }
        else
        {
            retState     = RCC_REQUEST_ERROR;
            inputClkFreq = 0u;
        }

        const rcc_PllConfig_t * const pllCfg = &rcc_Pll_Config[ pllId ];

        const uint32_t mDivRegValue  = Rcc_Get_RegVal( RCC_REG_PLLCKSELR, pllCfg->M_DivMask  );
        const uint32_t nMultRegValue = Rcc_Get_RegVal( pllCfg->DivRegId,  pllCfg->N_MultMask );

        /* PLLxN register holds multiplier decremented by 1, DIVMx = 0 - prescaler disabled */
        const uint32_t pllNMult = ( nMultRegValue >> Rcc_Pll_Get_FieldPos( pllCfg->N_MultMask ) ) + RCC_PLL_N_REG_OFFSET;
        const uint32_t pllMDiv  = ( mDivRegValue  >> Rcc_Pll_Get_FieldPos( pllCfg->M_DivMask  ) );

        if( RCC_REQUEST_ERROR != retState )
        {
            if( 0u != pllMDiv )
            {
                *pllClk = ( ( inputClkFreq / pllMDiv ) * pllNMult );
            }
            else
            {
                *pllClk = 0u;
            }

            retState = RCC_REQUEST_OK;
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
 * \return State of request execution. Returns "OK" if request was success
 *         (PLL locked), otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Active( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Set_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].RdyFlagMask );

            if( 0u != regValue )
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
 * \note PLL1 cannot be stopped while it drives the system clock - the request
 *       returns error then.
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Inactive( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Reset_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

            if( 0u == regValue )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* PLL is still running, keep return state as error */
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
        const uint32_t pllStateRegVal = Rcc_Get_RegBit( RCC_REG_CR, rcc_Pll_Config[ pllId ].StateMask );

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
 * The multiplexer is common for all PLLs. The source is changed only if all
 * PLLs are stopped; selection of the already selected source is accepted
 * also with running PLLs.
 *
 * \param pllId     [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_CSI  : PLL will be clocked by CSI oscillator (started)
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI oscillator (started)
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
        if( RCC_PLL_SRC_CSI == clkSource )
        {
            targetRegVal = LL_RCC_PLLSOURCE_CSI;

            retState = Rcc_ClkSrc_Set_CsiActive();
        }
        else if( RCC_PLL_SRC_HSE == clkSource )
        {
            targetRegVal = LL_RCC_PLLSOURCE_HSE;

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_PLL_SRC_HSI == clkSource )
        {
            targetRegVal = LL_RCC_PLLSOURCE_HSI;

            retState = Rcc_ClkSrc_Set_Hsi64Active();
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t actualRegVal = Rcc_Get_RegVal( RCC_REG_PLLCKSELR, RCC_PLLCKSELR_PLLSRC );
            const uint32_t pllsRunning  = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_PLL1ON |
                                                                      RCC_CR_PLL2ON |
                                                                      RCC_CR_PLL3ON );

            if( actualRegVal == targetRegVal )
            {
                /* Required source is already selected */
                retState = RCC_REQUEST_OK;
            }
            else if( 0u != pllsRunning )
            {
                /* Common source can not be changed while any PLL runs */
                retState = RCC_REQUEST_ERROR;
            }
            else
            {
                Rcc_Set_RegVal( RCC_REG_PLLCKSELR, RCC_PLLCKSELR_PLLSRC, targetRegVal );

                for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
                {
                    const uint32_t regValue = Rcc_Get_RegVal( RCC_REG_PLLCKSELR, RCC_PLLCKSELR_PLLSRC );

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
        }
        else
        {
            /* Incorrect clock source was requested or oscillator did not start */
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
        const uint32_t regValue = Rcc_Get_RegVal( RCC_REG_PLLCKSELR, RCC_PLLCKSELR_PLLSRC );

        if( LL_RCC_PLLSOURCE_CSI == regValue )
        {
            *clkSource = RCC_PLL_SRC_CSI;
        }
        else if( LL_RCC_PLLSOURCE_HSI == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSI;
        }
        else if( LL_RCC_PLLSOURCE_HSE == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSE;
        }
        else
        {
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
 * \brief Phase Locked Loop (PLL) output P divider configuration.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output P divider value, 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutP( rcc_PllId_t pllId, rcc_PllPDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        const uint32_t modulo = divider % rcc_Pll_Config[ pllId ].Out_P_DivStepsize;

        if( ( 0u                                        != divider ) &&
            ( rcc_Pll_Config[ pllId ].Out_P_DivMinValue != divider ) &&
            ( 0u                                        != modulo  )    )
        {
            /* Division factor out of step (odd factor of PLL1 P) */
            retState = RCC_REQUEST_ERROR;
        }
        else if( ( 0u                                        != divider ) &&
                 ( rcc_Pll_Config[ pllId ].Out_P_DivMinValue >  divider )    )
        {
            /* Division factor below minimum */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            retState = Rcc_Pll_Set_OutDivider( rcc_Pll_Config[ pllId ].DivRegId,
                                               rcc_Pll_Config[ pllId ].Out_P_ConfMask,
                                               rcc_Pll_Config[ pllId ].Out_P_StateMask,
                                               divider,
                                               RCC_PLL_OUT_DIV_MAX );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].DivRegId, rcc_Pll_Config[ pllId ].Out_P_ConfMask, pllClk );
    }
    else
    {
        /* No action required */
    }

    return ( retState );
}


/**
 * \brief Phase Locked Loop (PLL) output Q divider configuration.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output Q divider value, 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutQ( rcc_PllId_t pllId, rcc_PllQDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutDivider( rcc_Pll_Config[ pllId ].DivRegId,
                                           rcc_Pll_Config[ pllId ].Out_Q_ConfMask,
                                           rcc_Pll_Config[ pllId ].Out_Q_StateMask,
                                           divider,
                                           RCC_PLL_OUT_DIV_MAX );
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
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].DivRegId, rcc_Pll_Config[ pllId ].Out_Q_ConfMask, pllClk );
    }
    else
    {
        /* No action required */
    }

    return ( retState );
}


/**
 * \brief Phase Locked Loop (PLL) output R divider configuration.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output R divider value, 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutR( rcc_PllId_t pllId, rcc_PllRDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutDivider( rcc_Pll_Config[ pllId ].DivRegId,
                                           rcc_Pll_Config[ pllId ].Out_R_ConfMask,
                                           rcc_Pll_Config[ pllId ].Out_R_StateMask,
                                           divider,
                                           RCC_PLL_OUT_DIV_MAX );
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
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].DivRegId, rcc_Pll_Config[ pllId ].Out_R_ConfMask, pllClk );
    }
    else
    {
        /* No action required */
    }

    return ( retState );
}

#if defined(STM32H7RS)

/**
 * \brief Phase Locked Loop (PLL) output S divider configuration (STM32H7R / H7S).
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output S divider value (1 - 8), 0 disables the output.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutS( rcc_PllId_t pllId, rcc_PllSDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Set_OutDivider( rcc_Pll_Config[ pllId ].Div2RegId,
                                           rcc_Pll_Config[ pllId ].Out_S_ConfMask,
                                           rcc_Pll_Config[ pllId ].Out_S_StateMask,
                                           divider,
                                           RCC_PLL_OUT_ST_DIV_MAX );
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency of PLL output S (STM32H7R / H7S)
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutS( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Div2RegId, rcc_Pll_Config[ pllId ].Out_S_ConfMask, pllClk );
    }
    else
    {
        /* No action required */
    }

    return ( retState );
}


/**
 * \brief Phase Locked Loop (PLL) output T divider configuration (STM32H7R / H7S,
 *        PLL2 only).
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output T divider value (1 - 8), 0 disables
 *                      the output (the only value accepted for PLL1 / PLL3).
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutT( rcc_PllId_t pllId, rcc_PllTDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT <= pllId )
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }
    else if( RCC_PLL_OUT_NOT_AVAILABLE != rcc_Pll_Config[ pllId ].Out_T_StateMask )
    {
        retState = Rcc_Pll_Set_OutDivider( rcc_Pll_Config[ pllId ].Div2RegId,
                                           rcc_Pll_Config[ pllId ].Out_T_ConfMask,
                                           rcc_Pll_Config[ pllId ].Out_T_StateMask,
                                           divider,
                                           RCC_PLL_OUT_ST_DIV_MAX );
    }
    else if( 0u == divider )
    {
        /* PLL without output T - output not used */
        retState = RCC_REQUEST_OK;
    }
    else
    {
        /* PLL without output T */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency of PLL output T (STM32H7R / H7S, PLL2 only)
 *
 * \param pllId   [in]: PLL identification, value from \ref rcc_PllId_t.
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for PLL without output T).
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutT( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PLL_CNT               >  pllId                                     ) &&
        ( RCC_PLL_OUT_NOT_AVAILABLE != rcc_Pll_Config[ pllId ].Out_T_ConfMask    )    )
    {
        retState = Rcc_Pll_Get_OutClk( pllId, rcc_Pll_Config[ pllId ].Div2RegId, rcc_Pll_Config[ pllId ].Out_T_ConfMask, pllClk );
    }
    else
    {
        /* No action required */
    }

    return ( retState );
}

#endif

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * Write access to the backup domain (PWR DBP) is enabled - RTCSEL is part of
 * the backup domain.
 *
 * \warning RTCSEL can be changed only after backup domain reset - once a source
 *          is selected, request of another source returns error.
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
        const uint32_t llClkSource = rcc_Pll_RtcClkSrcLut[ clkSource ];

        LL_PWR_EnableBkUpAccess();

        LL_RCC_SetRTCClockSource( llClkSource );

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
 *         otherwise return error (also when no clock is selected).
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
 * \brief Returns position of the lowest bit of a register field mask.
 *
 * \param fieldMask [in]: Field mask (not 0)
 *
 * \return Bit position of the field.
 */
static uint32_t Rcc_Pll_Get_FieldPos( uint32_t fieldMask )
{
    uint32_t fieldPos = 0u;
    uint32_t mask     = fieldMask;

    for( uint32_t bitIdx = 0u; 32u > bitIdx; bitIdx++ )
    {
        if( 0u != ( mask & 1u ) )
        {
            fieldPos = bitIdx;
            break;
        }
        else
        {
            mask >>= 1u;
        }
    }

    return ( fieldPos );
}


/**
 * \brief Configures one PLL output divider and enables / disables the output.
 *
 * \param divRegId   [in]: Divider register (PLLxDIVR, PLLxDIVR2 on STM32H7R / H7S)
 * \param confMask   [in]: Divider field in the divider register
 * \param stateMask  [in]: Output enable bit in PLLCFGR
 * \param divider    [in]: Division factor 1 - dividerMax, 0 disables the output
 * \param dividerMax [in]: Maximal division factor of the output
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Set_OutDivider( rcc_RegId_t divRegId, uint32_t confMask, uint32_t stateMask, uint32_t divider, uint32_t dividerMax )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( 0u == divider )
    {
        /* Output not used - disabled */
        Rcc_Reset_RegBit( RCC_REG_PLLCFGR, stateMask );

        retState = RCC_REQUEST_OK;
    }
    else if( ( RCC_PLL_OUT_DIV_MIN <= divider ) &&
             ( dividerMax          >= divider )    )
    {
        const uint32_t regValue = divider - RCC_PLL_OUT_DIV_REG_OFFSET;

        Rcc_Set_RegVal( divRegId, confMask, regValue << Rcc_Pll_Get_FieldPos( confMask ) );

        /* Activate output */
        Rcc_Set_RegBit( RCC_REG_PLLCFGR, stateMask );

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
 * \brief Calculates frequency of one PLL output.
 *
 * \param pllId     [in]: PLL identification (valid)
 * \param divRegId  [in]: Divider register of the output (PLLxDIVR, PLLxDIVR2 on STM32H7R / H7S)
 * \param confMask  [in]: Divider field of the output in the divider register
 * \param pllClk   [out]: Frequency of the output in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_OutClk( rcc_PllId_t pllId, rcc_RegId_t divRegId, uint32_t confMask, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        const uint32_t regVal  = Rcc_Get_RegVal( divRegId, confMask );
        const uint32_t divider = ( regVal >> Rcc_Pll_Get_FieldPos( confMask ) ) + RCC_PLL_OUT_DIV_REG_OFFSET;

        *pllClk = ( pllIntFreq / divider );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
