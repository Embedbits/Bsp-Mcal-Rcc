/*
 *    Mr.Nobody, COPYRIGHT (c) 2021
 *    ALL RIGHTS RESERVED
 *
 */

/**
 * \file Rcc.h
 * \ingroup Rcc
 * \brief Rcc module common functionality
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc.h"                            /* Self include                   */
#include "Rcc_Port.h"                       /* Self include                   */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Gpio_Port.h"                      /* GPIO module port include       */
#include "Stm32_rcc.h"                      /* RCC RAL functionality          */
#include "Stm32_bus.h"                      /* CLK Buses RAL functionality    */
#include "Stm32_utils.h"                    /* MCU utilities RAL functionality*/
#include "Stm32_icache.h"                   /* Instruction cache functionality*/
#include "Stm32_dcache.h"                   /* Data cache functionality       */
/* ============================== TYPEDEFS ================================== */

typedef struct
{
    rcc_PllId_t       PllId;            /**< Phase Locked Loop (PLL) ID                     */

    rcc_RegId_t       StateRegId;       /**< PLL activation state register ID               */
    volatile uint32_t StateMask;        /**< PLL activation state mask                      */

    rcc_RegId_t       ClkSrcRegId;      /**< PLL clock source register ID                   */
    volatile uint32_t ClkSrcMask;       /**< PLL clock source mask                          */

    rcc_RegId_t       RdyFlagRegId;     /**< PLL ready flag register ID                     */
    volatile uint32_t RdyFlagMask;      /**< PLL ready flag mask                            */

    rcc_RegId_t       MDivRegId;        /**< PLL M divider register ID                      */
    volatile uint32_t MDivMask;         /**< PLL M divider mask                             */

    rcc_RegId_t       NMultRegId;       /**< PLL N multiplier register ID                   */
    volatile uint32_t NMultMask;        /**< PLL N multiplier mask                          */

    rcc_RegId_t       FreqInRangeRegId; /** Input frequency range configuration register ID */
    volatile uint32_t FreqInRangeMask;  /** Input frequency range configuration mask        */

    rcc_RegId_t       OutPStateRegId;   /** PLL output P activation state register ID       */
    volatile uint32_t OutPStateMask;    /** PLL output P activation state mask              */

    rcc_RegId_t       OutPConfRegId;    /**< PLL output P configuration register ID         */
    volatile uint32_t OutPConfMask;     /**< PLL output P configuration mask                */

    rcc_RegId_t       OutQStateRegId;   /** PLL output Q activation state register ID       */
    volatile uint32_t OutQStateMask;    /** PLL output Q activation state mask              */

    rcc_RegId_t       OutQConfRegId;    /** PLL output Q configuration register ID          */
    volatile uint32_t OutQConfMask;     /** PLL output Q configuration mask                 */

    rcc_RegId_t       OutRStateRegId;   /** PLL output R activation state register ID       */
    volatile uint32_t OutRStateMask;    /** PLL output R activation state mask              */

    rcc_RegId_t       OutRConfRegId;    /** PLL output R configuration register ID          */
    volatile uint32_t OutRConfMask;     /** PLL output R configuration mask                 */
}   rcc_PllConfig_t;


/**
 * \brief Clock tree configuration structure
 */
typedef struct __attribute__((packed))
{
    rcc_ClkMuxId_t          ClkMuxId;         /**< Peripheral ID            */
    rcc_RegId_t             ClkMuxRegId;      /**< Clock MUX register ID.   */
    volatile uint32_t const ClkSrcMask;       /**< Clock MUX bit mask.      */
    volatile uint32_t const ClkSrcVal;        /**< Clock MUX value.         */
}   rcc_ClkMuxConfigStruct_t;


/**
 * \brief Clock tree configuration structure
 */
typedef struct __attribute__((packed))
{
    rcc_PeriphId_t PeriphId;    /**< Peripheral ID                          */
    rcc_ClkSrcId_t ClkSrcId;    /**< Clock source ID.                       */
    rcc_ClkBusId_t ClkBusId;    /**< Clock bus ID.                          */
    uint32_t const StateMask;   /**< Peripheral activation state bit mask.  */
    uint32_t const LpCtrlMask;  /**< Peripheral Low Power control bit mask. */
    uint32_t const RstCtrlMask; /**< Peripheral Reset control bit mask.     */
    rcc_ClkMuxId_t ClkMuxId;    /**< Peripheral ID with clock multiplexer.  */
}   rcc_PeriphConfigStruct_t;


/** \brief Peripherals configuration structure */
typedef struct
{
    rcc_ClkBusId_t ClkBusId;    /**< Peripheral interconnection bus ID         */
    rcc_RegId_t    EnableRegId; /**< Peripheral clock enabled register         */
    rcc_RegId_t    SleepRegId;  /**< Peripheral enabled in sleep mode register */
    rcc_RegId_t    ResetRegId;  /**< Reset request register                    */
}   rcc_ClkBusConfigStruct_t;


typedef rcc_RequestState_t (*rcc_ClkSrcCallback_t)( rcc_FreqHz_t * const clkFreq );


typedef struct
{
    rcc_ClkSrcId_t       PeriphClkSrcId; /**< Peripheral clock source ID */
    rcc_ClkSrcCallback_t ClkSrcCallback; /**< Callback function pointer */
}   rcc_ClkSrcConfigStruct_t;


typedef struct
{
    rcc_MsisClkFreq_t ClkFreqId;  /**< Enumeration representation of output frequency */
    uint32_t          RegRangeVal; /**< Binary representation of output frequency      */
    rcc_FreqHz_t      OutFreq;  /**< Numerical represenation of output frequency    */
}   rcc_MsiConfigStruct_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t *sysClk );

static rcc_RequestState_t Rcc_Get_Pll1_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll1_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll1_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll2_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll2_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll2_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll3_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll3_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll3_PClk( rcc_FreqHz_t * const clkFreq );

static rcc_RequestState_t Rcc_Get_MsisClk_S( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_MsisClk_K( rcc_FreqHz_t * const clkFreq );

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define RCC_MAJOR_VERSION                       ( 1u )
/** Value of minor version of SW module */
#define RCC_MINOR_VERSION                       ( 0u )
/** Value of patch version of SW module */
#define RCC_PATCH_VERSION                       ( 0u )

/** Maximal wait time for configuration request confirmation */
#define RCC_TIMEOUT_RAW                         ( 0x84FCB )

/** \brief Minimal value of PLL M divider. */
#define RCC_M_DIVIDER_MIN                       ( 1u )
/** \brief Maximal value of PLL M divider. */
#define RCC_M_DIVIDER_MAX                       ( 63u )

/** Minimal value of N multiplier in PLL */
#define RCC_N_MULTIPLIER_MIN                    ( 4u )
/** Maximal value of N multiplier in PLL */
#define RCC_N_MULTIPLIER_MAX                    ( 512u )

/** \brief Minimal value of PLL P divider.
 * Another ST bullshit, PLL1 has minimum value 2, and others have 1. To make it
 * easier for checking is minimal value set to 2. Congratulations ST, you sucks!
 */
#define RCC_P_DIVIDER_MIN                       ( 1u )
/** \brief Maximal value of PLL P divider. */
#define RCC_P_DIVIDER_MAX                       ( 128u )

/** \brief Minimal value of PLL P divider. */
#define RCC_Q_DIVIDER_MIN                       ( 1u )
/** \brief Maximal value of PLL Q divider. */
#define RCC_Q_DIVIDER_MAX                       ( 128u )

/** \brief Minimal value of PLL R divider. */
#define RCC_R_DIVIDER_MIN                       ( 1u )
/** \brief Maximal value of PLL R divider. */
#define RCC_R_DIVIDER_MAX                       ( 128u )

/** Minimal value of internal PLL frequency in Hz */
#define RCC_PLL_INT_FREQUENCY_MIN               ( 128000000u )
/** Maximal value of internal PLL frequency in Hz */
#define RCC_PLL_INT_FREQUENCY_MAX               ( 560000000u )


/** Default value of HSI trimming */
#define RCC_HSI_DEFAULT_TRIM_VALUE              ( 64u )

/** Threshold frequency of SCLK in Hz, when "boost" mode shall be activated */
#define RCC_PWR_RANGE_THRESHOLD_HZ              ( 80000000u )

/** Threshold frequency of SCLK in Hz, when wait state shall be incremented */
#define RCC_FLASH_WAIT_STATE_THRESHOLD_HZ       ( 16000000u )


#define RCC_UNSUPPORTED_FUNCTION                ( 0xFF )


/* Defines used for FLASH latency according to HCLK Frequency */
#define UTILS_SCALE1_LATENCY0_FREQ    (32000000U)      /*!< HCLK frequency to set FLASH latency 0 in power scale 1 */
#define UTILS_SCALE1_LATENCY1_FREQ    (64000000U)      /*!< HCLK frequency to set FLASH latency 1 in power scale 1 */
#define UTILS_SCALE1_LATENCY2_FREQ    (96000000U)      /*!< HCLK frequency to set FLASH latency 2 in power scale 1 */
#define UTILS_SCALE1_LATENCY3_FREQ    (128000000U)     /*!< HCLK frequency to set FLASH latency 3 in power scale 1 */
#define UTILS_SCALE1_LATENCY4_FREQ    (160000000U)     /*!< HCLK frequency to set FLASH latency 4 in power scale 1 */
#define UTILS_SCALE2_LATENCY0_FREQ    (25000000U)      /*!< HCLK frequency to set FLASH latency 0 in power scale 2 */
#define UTILS_SCALE2_LATENCY1_FREQ    (50000000U)      /*!< HCLK frequency to set FLASH latency 1 in power scale 2 */
#define UTILS_SCALE2_LATENCY2_FREQ    (75000000U)      /*!< HCLK frequency to set FLASH latency 2 in power scale 2 */
#define UTILS_SCALE2_LATENCY3_FREQ    (100000000U)     /*!< HCLK frequency to set FLASH latency 3 in power scale 2 */
#define UTILS_SCALE3_LATENCY0_FREQ    (12500000U)       /*!< HCLK frequency to set FLASH latency 0 in power scale 3 */
#define UTILS_SCALE3_LATENCY1_FREQ    (25000000U)      /*!< HCLK frequency to set FLASH latency 1 in power scale 3 */
#define UTILS_SCALE3_LATENCY2_FREQ    (37500000U)       /*!< HCLK frequency to set FLASH latency 2 in power scale 3 */
#define UTILS_SCALE3_LATENCY3_FREQ    (50000000U)      /*!< HCLK frequency to set FLASH latency 3 in power scale 3 */
#define UTILS_SCALE4_LATENCY0_FREQ    (8000000U)       /*!< HCLK frequency to set FLASH latency 0 in power scale 4 */
#define UTILS_SCALE4_LATENCY1_FREQ    (16000000U)      /*!< HCLK frequency to set FLASH latency 1 in power scale 4 */
#define UTILS_SCALE4_LATENCY2_FREQ    (24000000U)      /*!< HCLK frequency to set FLASH latency 2 in power scale 4 */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */


/** Local storing of High Speed External (HSE) source value in Hz */
static rcc_FreqHz_t                 rcc_HseFreqHz = 0u;

/* --------------------- Multipliers/Dividers arrays -------------------------*/

/* The most disgusting part in whole project. Definition of external variables
 * created by STM!!! Shame on you ST! */
uint32_t      SystemCoreClock    = 160000000U;
const uint8_t AHBPrescTable[16u] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8u]  = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

/* ------------------------- Peripherals arrays ----------------------------- */

const rcc_MsiConfigStruct_t rcc_MsiConfig[ RCC_MSI_CLK_CNT ] =
{
  { .ClkFreqId = RCC_MSI_CLK_48M   , .RegRangeVal = LL_RCC_MSIKRANGE_0  , .OutFreq = 48000000u },
  { .ClkFreqId = RCC_MSI_CLK_24M   , .RegRangeVal = LL_RCC_MSIKRANGE_1  , .OutFreq = 24000000u },
  { .ClkFreqId = RCC_MSI_CLK_16M   , .RegRangeVal = LL_RCC_MSIKRANGE_2  , .OutFreq = 16000000u },
  { .ClkFreqId = RCC_MSI_CLK_12M   , .RegRangeVal = LL_RCC_MSIKRANGE_3  , .OutFreq = 12000000u },
  { .ClkFreqId = RCC_MSI_CLK_4M    , .RegRangeVal = LL_RCC_MSIKRANGE_4  , .OutFreq = 4000000u  },
  { .ClkFreqId = RCC_MSI_CLK_3072K , .RegRangeVal = LL_RCC_MSIKRANGE_8  , .OutFreq = 3072000u  },
  { .ClkFreqId = RCC_MSI_CLK_2000K , .RegRangeVal = LL_RCC_MSIKRANGE_5  , .OutFreq = 2000000u  },
  { .ClkFreqId = RCC_MSI_CLK_1536K , .RegRangeVal = LL_RCC_MSIKRANGE_9  , .OutFreq = 1536000u  },
  { .ClkFreqId = RCC_MSI_CLK_1330K , .RegRangeVal = LL_RCC_MSIKRANGE_6  , .OutFreq = 1330000u  },
  { .ClkFreqId = RCC_MSI_CLK_1024K , .RegRangeVal = LL_RCC_MSIKRANGE_10 , .OutFreq = 1024000u  },
  { .ClkFreqId = RCC_MSI_CLK_1000K , .RegRangeVal = LL_RCC_MSIKRANGE_7  , .OutFreq = 1000000u  },
  { .ClkFreqId = RCC_MSI_CLK_768K  , .RegRangeVal = LL_RCC_MSIKRANGE_11 , .OutFreq = 768000u   },
  { .ClkFreqId = RCC_MSI_CLK_400K  , .RegRangeVal = LL_RCC_MSIKRANGE_12 , .OutFreq = 400000u   },
  { .ClkFreqId = RCC_MSI_CLK_200K  , .RegRangeVal = LL_RCC_MSIKRANGE_13 , .OutFreq = 200000u   },
  { .ClkFreqId = RCC_MSI_CLK_133K  , .RegRangeVal = LL_RCC_MSIKRANGE_14 , .OutFreq = 133000u   },
  { .ClkFreqId = RCC_MSI_CLK_100K  , .RegRangeVal = LL_RCC_MSIKRANGE_15 , .OutFreq = 100000u   },
};

/** \brief Phase Locked Loop's (PLL) configuration registry structure */
static const rcc_PllConfig_t            rcc_PllConfig[ RCC_PLL_CNT ] =
{
  {
    .PllId            = RCC_PLL_1,

    .StateRegId       = RCC_REG_CR,
    .StateMask        = RCC_CR_PLL1ON,

    .ClkSrcRegId      = RCC_REG_PLL1CFGR,
    .ClkSrcMask       = RCC_PLL1CFGR_PLL1SRC,

    .RdyFlagRegId     = RCC_REG_CR,
    .RdyFlagMask      = RCC_CR_PLL1RDY,

    .MDivRegId        = RCC_REG_PLL1CFGR,
    .MDivMask         = RCC_PLL1CFGR_PLL1M,

    .NMultRegId       = RCC_REG_PLL1DIVR,
    .NMultMask        = RCC_PLL1DIVR_PLL1N,

    .FreqInRangeRegId = RCC_REG_PLL1CFGR,
    .FreqInRangeMask  = RCC_PLL1CFGR_PLL1RGE,

    .OutPStateRegId   = RCC_REG_PLL1CFGR,
    .OutPStateMask    = RCC_PLL1CFGR_PLL1PEN,

    .OutPConfRegId    = RCC_REG_PLL1DIVR,
    .OutPConfMask     = RCC_PLL1DIVR_PLL1P,

    .OutQStateRegId   = RCC_REG_PLL1CFGR,
    .OutQStateMask    = RCC_PLL1CFGR_PLL1QEN,

    .OutQConfRegId    = RCC_REG_PLL1DIVR,
    .OutQConfMask     = RCC_PLL1DIVR_PLL1Q,

    .OutRStateRegId   = RCC_REG_PLL1CFGR,
    .OutRStateMask    = RCC_PLL1CFGR_PLL1REN,

    .OutRConfRegId    = RCC_REG_PLL1DIVR,
    .OutRConfMask     = RCC_PLL1DIVR_PLL1R,
  },
  {
    .PllId            = RCC_PLL_2,

    .StateRegId       = RCC_REG_CR,
    .StateMask        = RCC_CR_PLL2ON,

    .ClkSrcRegId      = RCC_REG_PLL2CFGR,
    .ClkSrcMask       = RCC_PLL2CFGR_PLL2SRC,

    .RdyFlagRegId     = RCC_REG_CR,
    .RdyFlagMask      = RCC_CR_PLL2RDY,

    .MDivRegId        = RCC_REG_PLL2CFGR,
    .MDivMask         = RCC_PLL2CFGR_PLL2M,

    .NMultRegId       = RCC_REG_PLL2DIVR,
    .NMultMask        = RCC_PLL2DIVR_PLL2N,

    .FreqInRangeRegId = RCC_REG_PLL2CFGR,
    .FreqInRangeMask  = RCC_PLL2CFGR_PLL2RGE,

    .OutPStateRegId   = RCC_REG_PLL2CFGR,
    .OutPStateMask    = RCC_PLL2CFGR_PLL2PEN,

    .OutPConfRegId    = RCC_REG_PLL2DIVR,
    .OutPConfMask     = RCC_PLL2DIVR_PLL2P,

    .OutQStateRegId   = RCC_REG_PLL2CFGR,
    .OutQStateMask    = RCC_PLL2CFGR_PLL2QEN,

    .OutQConfRegId    = RCC_REG_PLL2DIVR,
    .OutQConfMask     = RCC_PLL2DIVR_PLL2Q,

    .OutRStateRegId   = RCC_REG_PLL2CFGR,
    .OutRStateMask    = RCC_PLL2CFGR_PLL2REN,

    .OutRConfRegId    = RCC_REG_PLL2DIVR,
    .OutRConfMask     = RCC_PLL2DIVR_PLL2R,
  },
  {
    .PllId            = RCC_PLL_3,

    .StateRegId       = RCC_REG_CR,
    .StateMask        = RCC_CR_PLL3ON,

    .ClkSrcRegId      = RCC_REG_PLL3CFGR,
    .ClkSrcMask       = RCC_PLL3CFGR_PLL3SRC,

    .RdyFlagRegId     = RCC_REG_CR,
    .RdyFlagMask      = RCC_CR_PLL3RDY,

    .MDivRegId        = RCC_REG_PLL3CFGR,
    .MDivMask         = RCC_PLL3CFGR_PLL3M,

    .NMultRegId       = RCC_REG_PLL3DIVR,
    .NMultMask        = RCC_PLL3DIVR_PLL3N,

    .FreqInRangeRegId = RCC_REG_PLL3CFGR,
    .FreqInRangeMask  = RCC_PLL3CFGR_PLL3RGE,

    .OutPStateRegId   = RCC_REG_PLL3CFGR,
    .OutPStateMask    = RCC_PLL3CFGR_PLL3PEN,

    .OutPConfRegId    = RCC_REG_PLL3DIVR,
    .OutPConfMask     = RCC_PLL3DIVR_PLL3P,

    .OutQStateRegId   = RCC_REG_PLL3CFGR,
    .OutQStateMask    = RCC_PLL3CFGR_PLL3QEN,

    .OutQConfRegId    = RCC_REG_PLL3DIVR,
    .OutQConfMask     = RCC_PLL3DIVR_PLL3Q,

    .OutRStateRegId   = RCC_REG_PLL3CFGR,
    .OutRStateMask    = RCC_PLL3CFGR_PLL3REN,

    .OutRConfRegId    = RCC_REG_PLL3DIVR,
    .OutRConfMask     = RCC_PLL3DIVR_PLL3R,
  },
};


/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Configuration array of registers used by peripheral buses */
const rcc_ClkBusConfigStruct_t         rcc_ClkBusConfigStruct[ RCC_CLK_BUS_CNT ] =
{
    { .ClkBusId = RCC_CLK_BUS_AHB1  , .EnableRegId = RCC_REG_AHB1ENR , .SleepRegId = RCC_REG_AHB1SMENR , .ResetRegId = RCC_REG_AHB1RSTR  },
    { .ClkBusId = RCC_CLK_BUS_AHB2_1, .EnableRegId = RCC_REG_AHB2ENR1, .SleepRegId = RCC_REG_AHB2SMENR1, .ResetRegId = RCC_REG_AHB2RSTR1 },
    { .ClkBusId = RCC_CLK_BUS_AHB2_2, .EnableRegId = RCC_REG_AHB2ENR2, .SleepRegId = RCC_REG_AHB2SMENR2, .ResetRegId = RCC_REG_AHB2RSTR2 },
    { .ClkBusId = RCC_CLK_BUS_AHB3  , .EnableRegId = RCC_REG_AHB3ENR , .SleepRegId = RCC_REG_AHB3SMENR , .ResetRegId = RCC_REG_AHB3RSTR  },
    { .ClkBusId = RCC_CLK_BUS_APB1_1, .EnableRegId = RCC_REG_APB1ENR1, .SleepRegId = RCC_REG_APB1SMENR1, .ResetRegId = RCC_REG_APB1RSTR1 },
    { .ClkBusId = RCC_CLK_BUS_APB1_2, .EnableRegId = RCC_REG_APB1ENR2, .SleepRegId = RCC_REG_APB1SMENR2, .ResetRegId = RCC_REG_APB1RSTR2 },
    { .ClkBusId = RCC_CLK_BUS_APB2  , .EnableRegId = RCC_REG_APB2ENR , .SleepRegId = RCC_REG_APB2SMENR , .ResetRegId = RCC_REG_APB2RSTR  },
    { .ClkBusId = RCC_CLK_BUS_APB3  , .EnableRegId = RCC_REG_APB3ENR , .SleepRegId = RCC_REG_APB3SMENR , .ResetRegId = RCC_REG_APB3RSTR  }
};


const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[RCC_CLK_SRC_CNT] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK   , .ClkSrcCallback = Rcc_Get_SysClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1RCLK , .ClkSrcCallback = Rcc_Get_Pll1_RClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1QCLK , .ClkSrcCallback = Rcc_Get_Pll1_QClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1PCLK , .ClkSrcCallback = Rcc_Get_Pll1_PClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2RCLK , .ClkSrcCallback = Rcc_Get_Pll2_RClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2QCLK , .ClkSrcCallback = Rcc_Get_Pll2_QClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2PCLK , .ClkSrcCallback = Rcc_Get_Pll2_PClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3RCLK , .ClkSrcCallback = Rcc_Get_Pll3_RClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3QCLK , .ClkSrcCallback = Rcc_Get_Pll3_QClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3PCLK , .ClkSrcCallback = Rcc_Get_Pll3_PClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK   , .ClkSrcCallback = Rcc_Get_AHBClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK  , .ClkSrcCallback = Rcc_Get_APB1Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK  , .ClkSrcCallback = Rcc_Get_APB2Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB3CLK  , .ClkSrcCallback = Rcc_Get_APB3Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI16CLK , .ClkSrcCallback = Rcc_Get_Hsi16Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48CLK , .ClkSrcCallback = Rcc_Get_Hsi48Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_SHSI48CLK, .ClkSrcCallback = Rcc_Get_Hsi48Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_MSICLK_K , .ClkSrcCallback = Rcc_Get_MsisClk_K },
  { .PeriphClkSrcId = RCC_CLK_SRC_MSICLK_S , .ClkSrcCallback = Rcc_Get_MsisClk_S },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK   , .ClkSrcCallback = Rcc_Get_HseClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK   , .ClkSrcCallback = Rcc_Get_LsiClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK   , .ClkSrcCallback = Rcc_Get_LseClk    },

//  { .PeriphClkSrcId = RCC_CLK_SRC_ICCLK    , .ClkSrcCallback = Rcc_Get_PerClk    },
};


/** \brief Configuration array of MCU peripherals. */
const rcc_PeriphConfigStruct_t          rcc_ConfigStruct[ RCC_PERIPH_LIST_CNT ] =
{
  { .PeriphId = RCC_PERIPH_SYSCFG            , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SYSCFGEN     , .LpCtrlMask = RCC_APB3SMENR_SYSCFGSMEN     , .RstCtrlMask = RCC_APB3RSTR_SYSCFGRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

  { .PeriphId = RCC_PERIPH_SYSTICK_HCLK_DIV8 , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },
  { .PeriphId = RCC_PERIPH_SYSTICK_LSI       , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI       },
  { .PeriphId = RCC_PERIPH_SYSTICK_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE       },

  { .PeriphId = RCC_PERIPH_PWR               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_PWREN        , .LpCtrlMask = RCC_AHB3SMENR_PWRSMEN        , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

  { .PeriphId = RCC_PERIPH_RTC_HSE_DIV32     , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_RTCAPBEN     , .LpCtrlMask = RCC_APB3SMENR_RTCAPBSMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32     },
  { .PeriphId = RCC_PERIPH_RTC_LSE           , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_RTCAPBEN     , .LpCtrlMask = RCC_APB3SMENR_RTCAPBSMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_RTC_LSE           },
  { .PeriphId = RCC_PERIPH_RTC_LSI           , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_RTCAPBEN     , .LpCtrlMask = RCC_APB3SMENR_RTCAPBSMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_RTC_LSI           },

  { .PeriphId = RCC_PERIPH_CRS               , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_CRSEN       , .LpCtrlMask = RCC_APB1SMENR1_CRSSMEN       , .RstCtrlMask = RCC_APB1RSTR1_CRSRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

#if defined(DCACHE1)
  { .PeriphId = RCC_PERIPH_DCACHE1           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DCACHE1EN    , .LpCtrlMask = RCC_AHB1SMENR_DCACHE1SMEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* DCACHE1 */
#if defined(DCACHE2)
  { .PeriphId = RCC_PERIPH_DCACHE2           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DCACHE2EN    , .LpCtrlMask = RCC_AHB1SMENR_DCACHE2SMEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* DCACHE2 */

  { .PeriphId = RCC_PERIPH_RAMCFG            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_RAMCFGEN     , .LpCtrlMask = RCC_AHB1SMENR_RAMCFGSMEN     , .RstCtrlMask = RCC_AHB1RSTR_RAMCFGRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

#if defined(RAMCFG_BKPRAM)
  { .PeriphId = RCC_PERIPH_BKPSRAM           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_BKPSRAMEN    , .LpCtrlMask = RCC_AHB1SMENR_BKPSRAMSMEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_BKPRAM */

#if defined(RAMCFG_SRAM1)
  { .PeriphId = RCC_PERIPH_SRAM1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_SRAM1EN      , .LpCtrlMask = RCC_AHB1SMENR_SRAM1SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM1 */
#if defined(RAMCFG_SRAM2)
  { .PeriphId = RCC_PERIPH_SRAM2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SRAM2EN     , .LpCtrlMask = RCC_AHB2SMENR1_SRAM2SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM2 */
#if defined(RAMCFG_SRAM3)
  { .PeriphId = RCC_PERIPH_SRAM3             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SRAM3EN     , .LpCtrlMask = RCC_AHB2SMENR1_SRAM3SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM3 */
#if defined(RAMCFG_SRAM4)
  { .PeriphId = RCC_PERIPH_SRAM4             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_SRAM4EN      , .LpCtrlMask = RCC_AHB3SMENR_SRAM4SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM4 */
#if defined(RAMCFG_SRAM5)
  { .PeriphId = RCC_PERIPH_SRAM5             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_SRAM5EN     , .LpCtrlMask = RCC_AHB2SMENR2_SRAM5SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM5 */
#if defined(RAMCFG_SRAM6)
  { .PeriphId = RCC_PERIPH_SRAM6             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_SRAM6EN     , .LpCtrlMask = RCC_AHB2SMENR2_SRAM6SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM6 */

  { .PeriphId = RCC_PERIPH_FLASH             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_FLASHEN      , .LpCtrlMask = RCC_AHB1SMENR_FLASHSMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

#if defined(GPDMA1)
  { .PeriphId = RCC_PERIPH_GPDMA1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GPDMA1EN     , .LpCtrlMask = RCC_AHB1SMENR_GPDMA1SMEN     , .RstCtrlMask = RCC_AHB1RSTR_GPDMA1RST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPDMA1 */

#if defined(LPDMA1)
  { .PeriphId = RCC_PERIPH_LPDMA1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_LPDMA1EN     , .LpCtrlMask = RCC_AHB3SMENR_LPDMA1SMEN     , .RstCtrlMask = RCC_AHB3RSTR_LPDMA1RST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* LPDMA1 */

#if defined(WWDG)
  { .PeriphId = RCC_PERIPH_WWDG              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_WWDGEN      , .LpCtrlMask = RCC_APB1SMENR1_WWDGSMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* WWDG */

  { .PeriphId = RCC_PERIPH_ICLK_PLL2Q        , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_ICLK_PLL2Q        },
  { .PeriphId = RCC_PERIPH_ICLK_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_ICLK_PLL1Q        },
  { .PeriphId = RCC_PERIPH_ICLK_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_ICLK_MSIK         },
  { .PeriphId = RCC_PERIPH_ICLK_HSI48        , .ClkSrcId = RCC_CLK_SRC_HSI48CLK, .ClkBusId = RCC_CLK_BUS_CNT   , .StateMask = RCC_UNSUPPORTED_FUNCTION , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_ICLK_HSI48        },


#if defined(LPGPIO1)
  { .PeriphId = RCC_PERIPH_LPGPIO1           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_LPGPIO1EN    , .LpCtrlMask = RCC_AHB3SMENR_LPGPIO1SMEN    , .RstCtrlMask = RCC_AHB3RSTR_LPGPIO1RST    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* LPGPIO1 */

#if defined(GPIOA)
  { .PeriphId = RCC_PERIPH_GPIOA             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOAEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOASMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOARST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOA */
#if defined(GPIOB)
  { .PeriphId = RCC_PERIPH_GPIOB             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOBEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOBSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOBRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOB */
#if defined(GPIOC)
  { .PeriphId = RCC_PERIPH_GPIOC             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOCEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOCSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOCRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOC */
#if defined(GPIOD)
  { .PeriphId = RCC_PERIPH_GPIOD             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIODEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIODSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIODRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOD */
#if defined(GPIOE)
  { .PeriphId = RCC_PERIPH_GPIOE             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOEEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOESMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOERST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOE */
#if defined(GPIOF)
  { .PeriphId = RCC_PERIPH_GPIOF             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOFEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOFSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOFRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOF */
#if defined(GPIOG)
  { .PeriphId = RCC_PERIPH_GPIOG             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOGEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOGSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOGRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */
#if defined(GPIOH)
  { .PeriphId = RCC_PERIPH_GPIOH             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOHEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOHSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOHRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */
#if defined(GPIOI)
  { .PeriphId = RCC_PERIPH_GPIOI             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOIEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOISMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOIRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */
#if defined(GPIOJ)
  { .PeriphId = RCC_PERIPH_GPIOJ             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOJEN     , .LpCtrlMask = RCC_AHB2SMENR1_GPIOJSMEN     , .RstCtrlMask = RCC_AHB2RSTR1_GPIOJRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */

#if defined(GFXTIM)
  { .PeriphId = RCC_PERIPH_GFXTIM            , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_GFXTIMEN     , .LpCtrlMask = RCC_APB2SMENR_GFXTIMSMEN     , .RstCtrlMask = RCC_APB2RSTR_GFXTIMRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GFXTIM */

#if defined(TIM1)
  { .PeriphId = RCC_PERIPH_TIM1              , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM1EN       , .LpCtrlMask = RCC_APB2SMENR_TIM1SMEN       , .RstCtrlMask = RCC_APB2RSTR_TIM1RST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM1 */
#if defined(TIM2)
  { .PeriphId = RCC_PERIPH_TIM2              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM2EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_TIM2RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM2 */
#if defined(TIM3)
  { .PeriphId = RCC_PERIPH_TIM3              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM3EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM3SMEN      , .RstCtrlMask = RCC_APB1RSTR1_TIM3RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM3 */
#if defined(TIM4)
  { .PeriphId = RCC_PERIPH_TIM4              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM4EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM4SMEN      , .RstCtrlMask = RCC_APB1RSTR1_TIM4RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM4 */
#if defined(TIM5)
  { .PeriphId = RCC_PERIPH_TIM5              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM5EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM5SMEN      , .RstCtrlMask = RCC_APB1RSTR1_TIM5RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM5 */
#if defined(TIM6)
  { .PeriphId = RCC_PERIPH_TIM6              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM6EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM6SMEN      , .RstCtrlMask = RCC_APB1RSTR1_TIM6RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM6 */
#if defined(TIM7)
  { .PeriphId = RCC_PERIPH_TIM7              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM7EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM7SMEN      , .RstCtrlMask = RCC_APB1RSTR1_TIM7RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM7 */
#if defined(TIM8)
  { .PeriphId = RCC_PERIPH_TIM8              , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM8EN       , .LpCtrlMask = RCC_APB2SMENR_TIM8SMEN       , .RstCtrlMask = RCC_APB2RSTR_TIM8RST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM8 */
#if defined(TIM15)
  { .PeriphId = RCC_PERIPH_TIM15             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM15EN      , .LpCtrlMask = RCC_APB2SMENR_TIM15SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM15RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM15 */
#if defined(TIM16)
  { .PeriphId = RCC_PERIPH_TIM16             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM16EN      , .LpCtrlMask = RCC_APB2SMENR_TIM16SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM16RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM16 */
#if defined(TIM17)
  { .PeriphId = RCC_PERIPH_TIM17             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM17EN      , .LpCtrlMask = RCC_APB2SMENR_TIM17SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM17RST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM17 */


#if defined(LPTIM1)
  { .PeriphId = RCC_PERIPH_LPTIM1_MSIK       , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM1EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM1SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM1RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK       },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM1EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM1SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM1RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM1_HSI16      , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM1EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM1SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM1RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI16      },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM1EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM1SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM1RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE        },
#endif /* LPTIM1 */
#if defined(LPTIM2)
  { .PeriphId = RCC_PERIPH_LPTIM2_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_LPTIM2EN    , .LpCtrlMask = RCC_APB1SMENR2_LPTIM2SMEN    , .RstCtrlMask = RCC_APB1RSTR2_LPTIM2RST    , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .PeriphId = RCC_PERIPH_LPTIM2_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_LPTIM2EN    , .LpCtrlMask = RCC_APB1SMENR2_LPTIM2SMEN    , .RstCtrlMask = RCC_APB1RSTR2_LPTIM2RST    , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM2_HSI16      , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_LPTIM2EN    , .LpCtrlMask = RCC_APB1SMENR2_LPTIM2SMEN    , .RstCtrlMask = RCC_APB1RSTR2_LPTIM2RST    , .ClkMuxId = RCC_CLK_MUX_LPTIM2_HSI16      },
  { .PeriphId = RCC_PERIPH_LPTIM2_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_LPTIM2EN    , .LpCtrlMask = RCC_APB1SMENR2_LPTIM2SMEN    , .RstCtrlMask = RCC_APB1RSTR2_LPTIM2RST    , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE        },
#endif /* LPTIM2 */
#if defined(LPTIM3)
  { .PeriphId = RCC_PERIPH_LPTIM3_MSIK       , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM3EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM3SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM3RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK      },
  { .PeriphId = RCC_PERIPH_LPTIM3_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM3EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM3SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM3RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSI       },
  { .PeriphId = RCC_PERIPH_LPTIM3_HSI16      , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM3EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM3SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM3RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_HSI16     },
  { .PeriphId = RCC_PERIPH_LPTIM3_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM3EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM3SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM3RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSE       },
#endif /* LPTIM3 */
#if defined(LPTIM4)
  { .PeriphId = RCC_PERIPH_LPTIM4_MSIK       , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM4EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM4SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM4RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK      },
  { .PeriphId = RCC_PERIPH_LPTIM4_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM4EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM4SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM4RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSI       },
  { .PeriphId = RCC_PERIPH_LPTIM4_HSI16      , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM4EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM4SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM4RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_HSI16     },
  { .PeriphId = RCC_PERIPH_LPTIM4_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM4EN     , .LpCtrlMask = RCC_APB3SMENR_LPTIM4SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM4RST     , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSE       },
#endif /* LPTIM4 */

#if defined(SPI1)
  { .PeriphId = RCC_PERIPH_SPI1_PCLK2        , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI1EN       , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       , .ClkMuxId = RCC_CLK_MUX_SPI1_PCLK2        },
  { .PeriphId = RCC_PERIPH_SPI1_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI1EN       , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       , .ClkMuxId = RCC_CLK_MUX_SPI1_SYSCLK       },
  { .PeriphId = RCC_PERIPH_SPI1_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI1EN       , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       , .ClkMuxId = RCC_CLK_MUX_SPI1_HSI          },
  { .PeriphId = RCC_PERIPH_SPI1_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI1EN       , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       , .ClkMuxId = RCC_CLK_MUX_SPI1_MSIK         },
#endif /* SPI1 */
#if defined(SPI2)
  { .PeriphId = RCC_PERIPH_SPI2_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI2EN      , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST      , .ClkMuxId = RCC_CLK_MUX_SPI2_PCLK1        },
  { .PeriphId = RCC_PERIPH_SPI2_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI2EN      , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST      , .ClkMuxId = RCC_CLK_MUX_SPI2_SYSCLK       },
  { .PeriphId = RCC_PERIPH_SPI2_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI2EN      , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST      , .ClkMuxId = RCC_CLK_MUX_SPI2_HSI          },
  { .PeriphId = RCC_PERIPH_SPI2_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI2EN      , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST      , .ClkMuxId = RCC_CLK_MUX_SPI2_MSIK         },
#endif /* SPI2 */
#if defined(SPI3)
  { .PeriphId = RCC_PERIPH_SPI3_PCLK3        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SPI3EN       , .LpCtrlMask = RCC_APB3SMENR_SPI3SMEN       , .RstCtrlMask = RCC_APB3RSTR_SPI3RST       , .ClkMuxId = RCC_CLK_MUX_SPI3_PCLK3        },
  { .PeriphId = RCC_PERIPH_SPI3_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SPI3EN       , .LpCtrlMask = RCC_APB3SMENR_SPI3SMEN       , .RstCtrlMask = RCC_APB3RSTR_SPI3RST       , .ClkMuxId = RCC_CLK_MUX_SPI3_SYSCLK       },
  { .PeriphId = RCC_PERIPH_SPI3_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SPI3EN       , .LpCtrlMask = RCC_APB3SMENR_SPI3SMEN       , .RstCtrlMask = RCC_APB3RSTR_SPI3RST       , .ClkMuxId = RCC_CLK_MUX_SPI3_HSI          },
  { .PeriphId = RCC_PERIPH_SPI3_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SPI3EN       , .LpCtrlMask = RCC_APB3SMENR_SPI3SMEN       , .RstCtrlMask = RCC_APB3RSTR_SPI3RST       , .ClkMuxId = RCC_CLK_MUX_SPI3_MSIK         },
#endif /* SPI3 */

#if defined(I2C1)
  { .PeriphId = RCC_PERIPH_I2C1_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C1EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST      , .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C1_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C1EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST      , .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK       },
  { .PeriphId = RCC_PERIPH_I2C1_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C1EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST      , .ClkMuxId = RCC_CLK_MUX_I2C1_HSI          },
  { .PeriphId = RCC_PERIPH_I2C1_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C1EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST      , .ClkMuxId = RCC_CLK_MUX_I2C1_MSIK         },
#endif /* I2C1 */
#if defined(I2C2)
  { .PeriphId = RCC_PERIPH_I2C2_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C2EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST      , .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C2_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C2EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST      , .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK       },
  { .PeriphId = RCC_PERIPH_I2C2_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C2EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST      , .ClkMuxId = RCC_CLK_MUX_I2C2_HSI          },
  { .PeriphId = RCC_PERIPH_I2C2_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C2EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST      , .ClkMuxId = RCC_CLK_MUX_I2C2_MSIK         },
#endif /* I2C2 */
#if defined(I2C3)
  { .PeriphId = RCC_PERIPH_I2C3_PCLK3        , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_I2C3EN       , .LpCtrlMask = RCC_APB3SMENR_I2C3SMEN       , .RstCtrlMask = RCC_APB3RSTR_I2C3RST       , .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
  { .PeriphId = RCC_PERIPH_I2C3_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_I2C3EN       , .LpCtrlMask = RCC_APB3SMENR_I2C3SMEN       , .RstCtrlMask = RCC_APB3RSTR_I2C3RST       , .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK       },
  { .PeriphId = RCC_PERIPH_I2C3_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_I2C3EN       , .LpCtrlMask = RCC_APB3SMENR_I2C3SMEN       , .RstCtrlMask = RCC_APB3RSTR_I2C3RST       , .ClkMuxId = RCC_CLK_MUX_I2C3_HSI          },
  { .PeriphId = RCC_PERIPH_I2C3_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_I2C3EN       , .LpCtrlMask = RCC_APB3SMENR_I2C3SMEN       , .RstCtrlMask = RCC_APB3RSTR_I2C3RST       , .ClkMuxId = RCC_CLK_MUX_I2C3_MSIK         },
#endif /* I2C3 */
#if defined(I2C4)
  { .PeriphId = RCC_PERIPH_I2C4_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C4EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST      , .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C4_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C4EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST      , .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK       },
  { .PeriphId = RCC_PERIPH_I2C4_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C4EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST      , .ClkMuxId = RCC_CLK_MUX_I2C4_HSI          },
  { .PeriphId = RCC_PERIPH_I2C4_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C4EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST      , .ClkMuxId = RCC_CLK_MUX_I2C4_MSIK         },
#endif /* I2C4 */
#if defined(I2C5)
  { .PeriphId = RCC_PERIPH_I2C5_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C5EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C5SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C5RST      , .ClkMuxId = RCC_CLK_MUX_I2C5_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C5_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C5EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C5SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C5RST      , .ClkMuxId = RCC_CLK_MUX_I2C5_SYSCLK       },
  { .PeriphId = RCC_PERIPH_I2C5_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C5EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C5SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C5RST      , .ClkMuxId = RCC_CLK_MUX_I2C5_HSI          },
  { .PeriphId = RCC_PERIPH_I2C5_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C5EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C5SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C5RST      , .ClkMuxId = RCC_CLK_MUX_I2C5_MSIK         },
#endif /* I2C5 */
#if defined(I2C6)
  { .PeriphId = RCC_PERIPH_I2C6_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C6EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C6SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C6RST      , .ClkMuxId = RCC_CLK_MUX_I2C6_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C6_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C6EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C6SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C6RST      , .ClkMuxId = RCC_CLK_MUX_I2C6_SYSCLK       },
  { .PeriphId = RCC_PERIPH_I2C6_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C6EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C6SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C6RST      , .ClkMuxId = RCC_CLK_MUX_I2C6_HSI          },
  { .PeriphId = RCC_PERIPH_I2C6_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_I2C6EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C6SMEN      , .RstCtrlMask = RCC_APB1RSTR2_I2C6RST      , .ClkMuxId = RCC_CLK_MUX_I2C6_MSIK         },
#endif /* I2C6 */

#if defined(USART1)
  { .PeriphId = RCC_PERIPH_USART1_APB2       , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USART1EN     , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN     , .RstCtrlMask = RCC_APB2RSTR_USART1RST     , .ClkMuxId = RCC_CLK_MUX_USART1_APB2       },
  { .PeriphId = RCC_PERIPH_USART1_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USART1EN     , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN     , .RstCtrlMask = RCC_APB2RSTR_USART1RST     , .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK     },
  { .PeriphId = RCC_PERIPH_USART1_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USART1EN     , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN     , .RstCtrlMask = RCC_APB2RSTR_USART1RST     , .ClkMuxId = RCC_CLK_MUX_USART1_HSI        },
  { .PeriphId = RCC_PERIPH_USART1_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USART1EN     , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN     , .RstCtrlMask = RCC_APB2RSTR_USART1RST     , .ClkMuxId = RCC_CLK_MUX_USART1_LSE        },
#endif /* USART1 */
#if defined(USART2)
  { .PeriphId = RCC_PERIPH_USART2_APB1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART2EN    , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART2RST    , .ClkMuxId = RCC_CLK_MUX_USART2_APB1       },
  { .PeriphId = RCC_PERIPH_USART2_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART2EN    , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART2RST    , .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK     },
  { .PeriphId = RCC_PERIPH_USART2_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART2EN    , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART2RST    , .ClkMuxId = RCC_CLK_MUX_USART2_HSI        },
  { .PeriphId = RCC_PERIPH_USART2_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART2EN    , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART2RST    , .ClkMuxId = RCC_CLK_MUX_USART2_LSE        },
#endif /* USART2 */
#if defined(USART3)
  { .PeriphId = RCC_PERIPH_USART3_APB1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART3EN    , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART3RST    , .ClkMuxId = RCC_CLK_MUX_USART3_APB1       },
  { .PeriphId = RCC_PERIPH_USART3_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART3EN    , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART3RST    , .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK     },
  { .PeriphId = RCC_PERIPH_USART3_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART3EN    , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART3RST    , .ClkMuxId = RCC_CLK_MUX_USART3_HSI        },
  { .PeriphId = RCC_PERIPH_USART3_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART3EN    , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART3RST    , .ClkMuxId = RCC_CLK_MUX_USART3_LSE        },
#endif /* USART3 */
#if defined(USART6)
  { .PeriphId = RCC_PERIPH_USART6_APB1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART6EN    , .LpCtrlMask = RCC_APB1SMENR1_USART6SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART6RST    , .ClkMuxId = RCC_CLK_MUX_USART6_APB1       },
  { .PeriphId = RCC_PERIPH_USART6_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART6EN    , .LpCtrlMask = RCC_APB1SMENR1_USART6SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART6RST    , .ClkMuxId = RCC_CLK_MUX_USART6_SYSCLK     },
  { .PeriphId = RCC_PERIPH_USART6_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART6EN    , .LpCtrlMask = RCC_APB1SMENR1_USART6SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART6RST    , .ClkMuxId = RCC_CLK_MUX_USART6_HSI        },
  { .PeriphId = RCC_PERIPH_USART6_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART6EN    , .LpCtrlMask = RCC_APB1SMENR1_USART6SMEN    , .RstCtrlMask = RCC_APB1RSTR1_USART6RST    , .ClkMuxId = RCC_CLK_MUX_USART6_LSE        },
#endif /* USART6 */

#if defined(UART4)
  { .PeriphId = RCC_PERIPH_UART4_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART4EN     , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART4RST     , .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .PeriphId = RCC_PERIPH_USART4_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART4EN     , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART4RST     , .ClkMuxId = RCC_CLK_MUX_USART4_SYSCLK     },
  { .PeriphId = RCC_PERIPH_UART4_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART4EN     , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART4RST     , .ClkMuxId = RCC_CLK_MUX_UART4_HSI         },
  { .PeriphId = RCC_PERIPH_UART4_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART4EN     , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART4RST     , .ClkMuxId = RCC_CLK_MUX_UART4_LSE         },
#endif /* UART4 */
#if defined(UART5)
  { .PeriphId = RCC_PERIPH_UART5_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART5EN     , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART5RST     , .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .PeriphId = RCC_PERIPH_USART5_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART5EN     , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART5RST     , .ClkMuxId = RCC_CLK_MUX_USART5_SYSCLK     },
  { .PeriphId = RCC_PERIPH_UART5_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART5EN     , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART5RST     , .ClkMuxId = RCC_CLK_MUX_UART5_HSI16       },
  { .PeriphId = RCC_PERIPH_UART5_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART5EN     , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN     , .RstCtrlMask = RCC_APB1RSTR1_UART5RST     , .ClkMuxId = RCC_CLK_MUX_UART5_LSE         },
#endif /* USART5 */

#if defined(LPUART1)
  { .PeriphId = RCC_PERIPH_LPUART1_PCLK3     , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPUART1EN    , .LpCtrlMask = RCC_APB3SMENR_LPUART1SMEN    , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST    , .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .PeriphId = RCC_PERIPH_LPUART1_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPUART1EN    , .LpCtrlMask = RCC_APB3SMENR_LPUART1SMEN    , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST    , .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK    },
  { .PeriphId = RCC_PERIPH_LPUART1_HSI16     , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPUART1EN    , .LpCtrlMask = RCC_APB3SMENR_LPUART1SMEN    , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST    , .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI16     },
  { .PeriphId = RCC_PERIPH_LPUART1_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPUART1EN    , .LpCtrlMask = RCC_APB3SMENR_LPUART1SMEN    , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST    , .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE       },
  { .PeriphId = RCC_PERIPH_LPUART1_MSIK      , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPUART1EN    , .LpCtrlMask = RCC_APB3SMENR_LPUART1SMEN    , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST    , .ClkMuxId = RCC_CLK_MUX_LPUART1_MSIK      },
#endif /* LPUART1 */

#if defined(FDCAN1)
  { .PeriphId = RCC_PERIPH_FDCAN_PLL1Q       , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_FDCAN1EN    , .LpCtrlMask = RCC_APB1SMENR2_FDCAN1SMEN    , .RstCtrlMask = RCC_APB1RSTR2_FDCAN1RST    , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL1Q       },
  { .PeriphId = RCC_PERIPH_FDCAN_PLL2P       , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_FDCAN1EN    , .LpCtrlMask = RCC_APB1SMENR2_FDCAN1SMEN    , .RstCtrlMask = RCC_APB1RSTR2_FDCAN1RST    , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL2P       },
  { .PeriphId = RCC_PERIPH_FDCAN_HSE         , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_FDCAN1EN    , .LpCtrlMask = RCC_APB1SMENR2_FDCAN1SMEN    , .RstCtrlMask = RCC_APB1RSTR2_FDCAN1RST    , .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
#endif /* FDCAN1 */

#if defined(SDMMC1)
  { .PeriphId = RCC_PERIPH_SDMMC1_ICLK       , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SDMMC1EN    , .LpCtrlMask = RCC_AHB2SMENR1_SDMMC1SMEN    , .RstCtrlMask = RCC_AHB2RSTR1_SDMMC1RST    , .ClkMuxId = RCC_CLK_MUX_SDMMC_ICLK        },
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL1P      , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SDMMC1EN    , .LpCtrlMask = RCC_AHB2SMENR1_SDMMC1SMEN    , .RstCtrlMask = RCC_AHB2RSTR1_SDMMC1RST    , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL1P       },
#endif /* SDMMC1 */
#if defined(SDMMC2)
  { .PeriphId = RCC_PERIPH_SDMMC2_ICLK       , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SDMMC2EN    , .LpCtrlMask = RCC_AHB2SMENR1_SDMMC2SMEN    , .RstCtrlMask = RCC_AHB2RSTR1_SDMMC2RST    , .ClkMuxId = RCC_CLK_MUX_SDMMC_ICLK        },
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL1P      , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SDMMC2EN    , .LpCtrlMask = RCC_AHB2SMENR1_SDMMC2SMEN    , .RstCtrlMask = RCC_AHB2RSTR1_SDMMC2RST    , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL1P       },
#endif /* SDMMC2 */

#if defined(FMC_Bank1_R)
  { .PeriphId = RCC_PERIPH_FMC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_FSMCEN      , .LpCtrlMask = RCC_AHB2SMENR2_FSMCSMEN      , .RstCtrlMask = RCC_AHB2RSTR2_FSMCRST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* FMC */

#if defined(OCTOSPIM)
  { .PeriphId = RCC_PERIPH_OCTOSPIM          , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OCTOSPIMEN  , .LpCtrlMask = RCC_AHB2SMENR1_OCTOSPIMSMEN  , .RstCtrlMask = RCC_AHB2RSTR1_OCTOSPIMRST  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* OCTOSPIM */
#if defined(OCTOSPI1)
  { .PeriphId = RCC_PERIPH_OCTOSPI1_SYSCLK   , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI1EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI1SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI1RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK    },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_MSIK     , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI1EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI1SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI1RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_MSIK      },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_PLL1Q    , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI1EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI1SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI1RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL1Q     },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_PLL2Q    , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI1EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI1SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI1RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL2Q     },
#endif /* OCTOSPI1 */
#if defined(OCTOSPI2)
  { .PeriphId = RCC_PERIPH_OCTOSPI2_SYSCLK   , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI2EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI2SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI2RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK    },
  { .PeriphId = RCC_PERIPH_OCTOSPI2_MSIK     , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI2EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI2SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI2RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_MSIK      },
  { .PeriphId = RCC_PERIPH_OCTOSPI2_PLL1Q    , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI2EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI2SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI2RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL1Q     },
  { .PeriphId = RCC_PERIPH_OCTOSPI2_PLL2Q    , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI2EN  , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI2SMEN  , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI2RST  , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL2Q     },
#endif /* OCTOSPI2 */

#if defined(HSPI1)
  { .PeriphId = RCC_PERIPH_HSPI1_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_HSPI1EN     , .LpCtrlMask = RCC_AHB2SMENR2_HSPI1SMEN     , .RstCtrlMask = RCC_AHB2RSTR2_HSPI1RST     , .ClkMuxId = RCC_CLK_MUX_HSPI1_SYSCLK      },
  { .PeriphId = RCC_PERIPH_HSPI1_PLL1Q       , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_HSPI1EN     , .LpCtrlMask = RCC_AHB2SMENR2_HSPI1SMEN     , .RstCtrlMask = RCC_AHB2RSTR2_HSPI1RST     , .ClkMuxId = RCC_CLK_MUX_HSPI1_PLL1Q       },
  { .PeriphId = RCC_PERIPH_HSPI1_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_HSPI1EN     , .LpCtrlMask = RCC_AHB2SMENR2_HSPI1SMEN     , .RstCtrlMask = RCC_AHB2RSTR2_HSPI1RST     , .ClkMuxId = RCC_CLK_MUX_HSPI1_PLL2Q       },
  { .PeriphId = RCC_PERIPH_HSPI1_PLL3R       , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_HSPI1EN     , .LpCtrlMask = RCC_AHB2SMENR2_HSPI1SMEN     , .RstCtrlMask = RCC_AHB2RSTR2_HSPI1RST     , .ClkMuxId = RCC_CLK_MUX_HSPI1_PLL3R       },
#endif /* HSPI1 */

#if defined(USB_OTG_HS)
  { .PeriphId = RCC_PERIPH_USBPHY_HSE        , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_USBPHYCEN   , .LpCtrlMask = RCC_AHB2SMENR1_USBPHYCSMEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_USBPHY_HSE        },
  { .PeriphId = RCC_PERIPH_USBPHY_HSE_DIV2   , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_USBPHYCEN   , .LpCtrlMask = RCC_AHB2SMENR1_USBPHYCSMEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_USBPHY_HSE_DIV2   },
  { .PeriphId = RCC_PERIPH_USBPHY_PLL1P      , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_USBPHYCEN   , .LpCtrlMask = RCC_AHB2SMENR1_USBPHYCSMEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_USBPHY_PLL1P      },
  { .PeriphId = RCC_PERIPH_USBPHY_PLL1P_DIV2 , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_USBPHYCEN   , .LpCtrlMask = RCC_AHB2SMENR1_USBPHYCSMEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_USBPHY_PLL1P_DIV2 },

  { .PeriphId = RCC_PERIPH_USB               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OTGEN       , .LpCtrlMask = RCC_AHB2SMENR1_OTGSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_OTGRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* USB_OTG_HS */

#if defined(UCPD1)
  { .PeriphId = RCC_PERIPH_UCPD1             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB1ENR2_UCPD1EN     , .LpCtrlMask = RCC_APB1SMENR2_UCPD1SMEN     , .RstCtrlMask = RCC_APB1RSTR2_UCPD1RST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* UCPD1 */

#if defined(PSSI) || defined(DCMI)
  { .PeriphId = RCC_PERIPH_DCMI_PSSI         , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_DCMI_PSSIEN , .LpCtrlMask = RCC_AHB2SMENR1_DCMI_PSSISMEN , .RstCtrlMask = RCC_AHB2RSTR1_DCMI_PSSIRST , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* PSSI OR DCMI */

#if defined(DSI)
  { .PeriphId = RCC_PERIPH_DSIHOST           , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_DSIHOSTEN    , .LpCtrlMask = RCC_APB2SMENR_DSIHOSTSMEN    , .RstCtrlMask = RCC_APB2RSTR_DSIHOSTRST    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* DSI */

#if defined(GFXMMU)
  { .PeriphId = RCC_PERIPH_GFXMMU            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GFXMMUEN     , .LpCtrlMask = RCC_AHB1SMENR_GFXMMUSMEN     , .RstCtrlMask = RCC_AHB1RSTR_GFXMMURST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GFXMMU */

#if defined(JPEG)
  { .PeriphId = RCC_PERIPH_JPEG              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_JPEGEN       , .LpCtrlMask = RCC_AHB1SMENR_JPEGSMEN       , .RstCtrlMask = RCC_AHB1RSTR_JPEGRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* JPEG */

#if defined(LTDC)
  { .PeriphId = RCC_PERIPH_LTDC_PLL2R        , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_LTDCEN       , .LpCtrlMask = RCC_APB2SMENR_LTDCSMEN       , .RstCtrlMask = RCC_APB2RSTR_LTDCRST       , .ClkMuxId = RCC_CLK_MUX_LTDC_PLL2R        },
  { .PeriphId = RCC_PERIPH_LTDC_PLL3R        , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_LTDCEN       , .LpCtrlMask = RCC_APB2SMENR_LTDCSMEN       , .RstCtrlMask = RCC_APB2RSTR_LTDCRST       , .ClkMuxId = RCC_CLK_MUX_LTDC_PLL3R        },
#endif /* LTDC */

#if defined(GPU2D)
  { .PeriphId = RCC_PERIPH_GPU2D             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GPU2DEN      , .LpCtrlMask = RCC_AHB1SMENR_GPU2DSMEN      , .RstCtrlMask = RCC_AHB1RSTR_GPU2DRST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPU2D */

#if defined(DMA2D)
  { .PeriphId = RCC_PERIPH_DMA2D             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DMA2DEN      , .LpCtrlMask = RCC_AHB1SMENR_DMA2DSMEN      , .RstCtrlMask = RCC_AHB1RSTR_DMA2DRST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* DMA2D */

#if defined(SAI1)
  { .PeriphId = RCC_PERIPH_SAI1_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P        },
  { .PeriphId = RCC_PERIPH_SAI1_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P        },
  { .PeriphId = RCC_PERIPH_SAI1_PLL1P        , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1P        },
  { .PeriphId = RCC_PERIPH_SAI1_CKIN         , .ClkSrcId = RCC_CLK_SRC_CNT     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       , .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN         },
  { .PeriphId = RCC_PERIPH_SAI1_HSI16        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       , .ClkMuxId = RCC_CLK_MUX_SAI1_HSI16        },
#endif /* SAI1 */
#if defined(SAI2)
  { .PeriphId = RCC_PERIPH_SAI2_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI2EN       , .LpCtrlMask = RCC_APB2SMENR_SAI2SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P        },
  { .PeriphId = RCC_PERIPH_SAI2_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI2EN       , .LpCtrlMask = RCC_APB2SMENR_SAI2SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P        },
  { .PeriphId = RCC_PERIPH_SAI2_PLL1P        , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI2EN       , .LpCtrlMask = RCC_APB2SMENR_SAI2SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1P        },
  { .PeriphId = RCC_PERIPH_SAI2_CKIN         , .ClkSrcId = RCC_CLK_SRC_CNT     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI2EN       , .LpCtrlMask = RCC_APB2SMENR_SAI2SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       , .ClkMuxId = RCC_CLK_MUX_SAI2_CKIN         },
  { .PeriphId = RCC_PERIPH_SAI2_HSI16        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI2EN       , .LpCtrlMask = RCC_APB2SMENR_SAI2SMEN       , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       , .ClkMuxId = RCC_CLK_MUX_SAI2_HSI16        },
#endif /* SAI2 */

#if defined(OPAMP)
  { .PeriphId = RCC_PERIPH_OPAMP             , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_OPAMPEN      , .LpCtrlMask = RCC_APB3SMENR_OPAMPSMEN      , .RstCtrlMask = RCC_APB3RSTR_OPAMPRST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* OPAMP */

#if defined(COMP1)
  { .PeriphId = RCC_PERIPH_COMP              , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_COMPEN       , .LpCtrlMask = RCC_APB3SMENR_COMPSMEN       , .RstCtrlMask = RCC_APB3RSTR_COMPRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* COMP1 */

#if defined(VREFBUF)
  { .PeriphId = RCC_PERIPH_VREF              , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_VREFEN       , .LpCtrlMask = RCC_APB3SMENR_VREFSMEN       , .RstCtrlMask = RCC_APB3RSTR_VREFRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* VREFBUF */

#if defined(TSC)
  { .PeriphId = RCC_PERIPH_TSC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_TSCEN        , .LpCtrlMask = RCC_AHB1SMENR_TSCSMEN        , .RstCtrlMask = RCC_AHB1RSTR_TSCRST        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TSC1 */

#if defined(ADC12_COMMON)
  { .PeriphId = RCC_PERIPH_ADC12_HCLK        , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN     , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST     , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .PeriphId = RCC_PERIPH_ADC12_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN     , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST     , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    },
  { .PeriphId = RCC_PERIPH_ADC12_PLL2R       , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN     , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST     , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     },
  { .PeriphId = RCC_PERIPH_ADC12_HSE         , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN     , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST     , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       },
  { .PeriphId = RCC_PERIPH_ADC12_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN     , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST     , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI16     },
  { .PeriphId = RCC_PERIPH_ADC12_MSIK        , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN     , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST     , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK      },
#endif /* ADC1 */

#if defined(ADC4)
  { .PeriphId = RCC_PERIPH_ADC4_HCLK         , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN       , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .PeriphId = RCC_PERIPH_ADC4_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN       , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    },
  { .PeriphId = RCC_PERIPH_ADC4_PLL2R        , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN       , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     },
  { .PeriphId = RCC_PERIPH_ADC4_HSE          , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN       , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       },
  { .PeriphId = RCC_PERIPH_ADC4_HSI16        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN       , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI16     },
  { .PeriphId = RCC_PERIPH_ADC4_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN       , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK      },
#endif /* ADC4 */

#if defined(DAC1)
  { .PeriphId = RCC_PERIPH_DAC_HCLK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN       , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .PeriphId = RCC_PERIPH_DAC_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN       , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    },
  { .PeriphId = RCC_PERIPH_DAC_PLL2R         , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN       , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     },
  { .PeriphId = RCC_PERIPH_DAC_HSE           , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN       , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       },
  { .PeriphId = RCC_PERIPH_DAC_HSI16         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN       , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI16     },
  { .PeriphId = RCC_PERIPH_DAC_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN       , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK      },
#endif /* DAC1 */

#if defined(AES)
  { .PeriphId = RCC_PERIPH_AES               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_AESEN       , .LpCtrlMask = RCC_AHB2SMENR1_AESSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_AESRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* AES */

#if defined(GTZC_TZSC1)
  { .PeriphId = RCC_PERIPH_GTZC1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GTZC1EN      , .LpCtrlMask = RCC_AHB1SMENR_GTZC1SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GTZC1 */
#if defined(GTZC_TZSC2)
  { .PeriphId = RCC_PERIPH_GTZC2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_GTZC2EN      , .LpCtrlMask = RCC_AHB3SMENR_GTZC2SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GTZC2 */

#if defined(HASH)
  { .PeriphId = RCC_PERIPH_HASH              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_HASHEN      , .LpCtrlMask = RCC_AHB2SMENR1_HASHSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_HASHRST      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* HASH */

#if defined(OTFDEC1)
  { .PeriphId = RCC_PERIPH_OTFDEC1           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OTFDEC1EN   , .LpCtrlMask = RCC_AHB2SMENR1_OTFDEC1SMEN   , .RstCtrlMask = RCC_AHB2RSTR1_OTFDEC1RST   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* OTFDEC1 */
#if defined(OTFDEC2)
  { .PeriphId = RCC_PERIPH_OTFDEC2           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OTFDEC2EN   , .LpCtrlMask = RCC_AHB2SMENR1_OTFDEC2SMEN   , .RstCtrlMask = RCC_AHB2RSTR1_OTFDEC2RST   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* OTFDEC2 */

#if defined(PKA)
  { .PeriphId = RCC_PERIPH_PKA               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_PKAEN       , .LpCtrlMask = RCC_AHB2SMENR1_PKASMEN       , .RstCtrlMask = RCC_AHB2RSTR1_PKARST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* PKA */

#if defined(RNG)
  { .PeriphId = RCC_PERIPH_RNG_HSI48         , .ClkSrcId = RCC_CLK_SRC_HSI48CLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_RNGEN       , .LpCtrlMask = RCC_AHB2SMENR1_RNGSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_RNGRST       , .ClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .PeriphId = RCC_PERIPH_RNG_HSI48_DIV2    , .ClkSrcId = RCC_CLK_SRC_HSI48CLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_RNGEN       , .LpCtrlMask = RCC_AHB2SMENR1_RNGSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_RNGRST       , .ClkMuxId = RCC_CLK_MUX_RNG_HSI48_DIV2    },
  { .PeriphId = RCC_PERIPH_RNG_HSI16         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_RNGEN       , .LpCtrlMask = RCC_AHB2SMENR1_RNGSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_RNGRST       , .ClkMuxId = RCC_CLK_MUX_RNG_HSI16         },
#endif /* RNG */

#if defined(SAES)
  { .PeriphId = RCC_PERIPH_SAES_SHSI         , .ClkSrcId = RCC_CLK_SRC_SHSI48CLK,.ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SAESEN      , .LpCtrlMask = RCC_AHB2SMENR1_SAESSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_SAESRST      , .ClkMuxId = RCC_CLK_MUX_SAES_SHSI         },
  { .PeriphId = RCC_PERIPH_SAES_SHSI_DIV2    , .ClkSrcId = RCC_CLK_SRC_SHSI48CLK,.ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SAESEN      , .LpCtrlMask = RCC_AHB2SMENR1_SAESSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_SAESRST      , .ClkMuxId = RCC_CLK_MUX_SAES_SHSI_DIV2    },
#endif /* SAES */

#if defined(ADF1)
  { .PeriphId = RCC_PERIPH_ADF1_HCLK         , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADF1EN       , .LpCtrlMask = RCC_AHB3SMENR_ADF1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADF1RST       , .ClkMuxId = RCC_CLK_MUX_ADF1_HCLK         },
  { .PeriphId = RCC_PERIPH_ADF1_PLL1P        , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADF1EN       , .LpCtrlMask = RCC_AHB3SMENR_ADF1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADF1RST       , .ClkMuxId = RCC_CLK_MUX_ADF1_PLL1P        },
  { .PeriphId = RCC_PERIPH_ADF1_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADF1EN       , .LpCtrlMask = RCC_AHB3SMENR_ADF1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADF1RST       , .ClkMuxId = RCC_CLK_MUX_ADF1_PLL3Q        },
  { .PeriphId = RCC_PERIPH_ADF1_CKIN         , .ClkSrcId = RCC_CLK_SRC_CNT     , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADF1EN       , .LpCtrlMask = RCC_AHB3SMENR_ADF1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADF1RST       , .ClkMuxId = RCC_CLK_MUX_ADF1_CKIN         },
  { .PeriphId = RCC_PERIPH_ADF1_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADF1EN       , .LpCtrlMask = RCC_AHB3SMENR_ADF1SMEN       , .RstCtrlMask = RCC_AHB3RSTR_ADF1RST       , .ClkMuxId = RCC_CLK_MUX_ADF1_MSIK         },
#endif /* ADF1 */

#if defined(CORDIC)
  { .PeriphId = RCC_PERIPH_CORDIC            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_CORDICEN     , .LpCtrlMask = RCC_AHB1SMENR_CORDICSMEN     , .RstCtrlMask = RCC_AHB1RSTR_CORDICRST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* CORDIC */

#if defined(CRC)
  { .PeriphId = RCC_PERIPH_CRC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_CRCEN        , .LpCtrlMask = RCC_AHB1SMENR_CRCSMEN        , .RstCtrlMask = RCC_AHB1RSTR_CRCRST        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* CRC */

#if defined(FMAC)
  { .PeriphId = RCC_PERIPH_FMAC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_FMACEN       , .LpCtrlMask = RCC_AHB1SMENR_FMACSMEN       , .RstCtrlMask = RCC_AHB1RSTR_FMACRST       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* FMAC */

#if defined(MDF1)
  { .PeriphId = RCC_PERIPH_MDF1_HCLK         , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_MDF1EN       , .LpCtrlMask = RCC_AHB1SMENR_MDF1SMEN       , .RstCtrlMask = RCC_AHB1RSTR_MDF1RST       , .ClkMuxId = RCC_CLK_MUX_MDF1_HCLK         },
  { .PeriphId = RCC_PERIPH_MDF1_PLL1P        , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_MDF1EN       , .LpCtrlMask = RCC_AHB1SMENR_MDF1SMEN       , .RstCtrlMask = RCC_AHB1RSTR_MDF1RST       , .ClkMuxId = RCC_CLK_MUX_MDF1_PLL1P        },
  { .PeriphId = RCC_PERIPH_MDF1_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_MDF1EN       , .LpCtrlMask = RCC_AHB1SMENR_MDF1SMEN       , .RstCtrlMask = RCC_AHB1RSTR_MDF1RST       , .ClkMuxId = RCC_CLK_MUX_MDF1_PLL3Q        },
  { .PeriphId = RCC_PERIPH_MDF1_CKIN         , .ClkSrcId = RCC_CLK_SRC_CNT     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_MDF1EN       , .LpCtrlMask = RCC_AHB1SMENR_MDF1SMEN       , .RstCtrlMask = RCC_AHB1RSTR_MDF1RST       , .ClkMuxId = RCC_CLK_MUX_MDF1_CKIN         },
  { .PeriphId = RCC_PERIPH_MDF1_MSIK         , .ClkSrcId = RCC_CLK_SRC_MSICLK_K, .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_MDF1EN       , .LpCtrlMask = RCC_AHB1SMENR_MDF1SMEN       , .RstCtrlMask = RCC_AHB1RSTR_MDF1RST       , .ClkMuxId = RCC_CLK_MUX_MDF1_MSIK         },
#endif /* MDF1 */
};



/** \brief Configuration array of MCU peripherals with available clock MUX. */
const rcc_ClkMuxConfigStruct_t          rcc_ClkMuxConfig[ RCC_CLK_MUX_LIST_CNT ] =
{
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_HCLKDIV8    },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_LSI         },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_LSE         },

  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32     , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_HSE_DIV32       },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE           , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSE             },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI           , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSI             },


  { .ClkMuxId = RCC_CLK_MUX_ICLK_PLL2Q        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_ICLKSEL     , .ClkSrcVal = LL_RCC_SDMMC12_CLKSOURCE_PLL2        },
  { .ClkMuxId = RCC_CLK_MUX_ICLK_PLL1Q        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_ICLKSEL     , .ClkSrcVal = LL_RCC_SDMMC12_CLKSOURCE_PLL1        },
  { .ClkMuxId = RCC_CLK_MUX_ICLK_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_ICLKSEL     , .ClkSrcVal = LL_RCC_SDMMC12_CLKSOURCE_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_ICLK_HSI48        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_ICLKSEL     , .ClkSrcVal = LL_RCC_SDMMC12_CLKSOURCE_HSI48       },

    /*-------------------------------- Timers --------------------------------*/

#if defined(LPTIM1)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_MSIK         },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSI          },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI16      , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSE          },
#endif /* LPTIM1 */
#if defined(LPTIM2)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_LSI          },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_HSI16      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_LSE          },
#endif /* LPTIM2 */
#if defined(LPTIM3) || defined(LPTIM4)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK      , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL  , .ClkSrcVal = LL_RCC_LPTIM34_CLKSOURCE_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSI       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL  , .ClkSrcVal = LL_RCC_LPTIM34_CLKSOURCE_LSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_HSI16     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL  , .ClkSrcVal = LL_RCC_LPTIM34_CLKSOURCE_HSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSE       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL  , .ClkSrcVal = LL_RCC_LPTIM34_CLKSOURCE_LSE         },
#endif /* LPTIM3 || LPTIM4 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PCLK2        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PCLK2          },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_HSI          , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_MSIK           },
#endif /* SPI1 */
#if defined(SPI2)
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PCLK1          },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_HSI          , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_MSIK           },
#endif /* SPI2 */
#if defined(SPI3)
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PCLK3        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PCLK3          },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_HSI          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_MSIK           },
#endif /* SPI3 */

#if defined(I2C1)
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PCLK1          },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI          , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_MSIK           },
#endif /* I2C1 */
#if defined(I2C2)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PCLK1          },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI          , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_MSIK           },
#endif /* I2C2 */
#if defined(I2C3)
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PCLK3          },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_MSIK           },
#endif /* I2C3 */
#if defined(I2C4)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_PCLK1          },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI          , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_MSIK           },
#endif /* I2C4 */
#if defined(I2C5)
  { .ClkMuxId = RCC_CLK_MUX_I2C5_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C5SEL     , .ClkSrcVal = LL_RCC_I2C5_CLKSOURCE_PCLK1          },
  { .ClkMuxId = RCC_CLK_MUX_I2C5_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C5SEL     , .ClkSrcVal = LL_RCC_I2C5_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_I2C5_HSI          , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C5SEL     , .ClkSrcVal = LL_RCC_I2C5_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_I2C5_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C5SEL     , .ClkSrcVal = LL_RCC_I2C5_CLKSOURCE_MSIK           },
#endif /* I2C5 */
#if defined(I2C6)
  { .ClkMuxId = RCC_CLK_MUX_I2C6_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C6SEL     , .ClkSrcVal = LL_RCC_I2C6_CLKSOURCE_PCLK1          },
  { .ClkMuxId = RCC_CLK_MUX_I2C6_SYSCLK       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C6SEL     , .ClkSrcVal = LL_RCC_I2C6_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_I2C6_HSI          , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C6SEL     , .ClkSrcVal = LL_RCC_I2C6_CLKSOURCE_HSI            },
  { .ClkMuxId = RCC_CLK_MUX_I2C6_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_I2C6SEL     , .ClkSrcVal = LL_RCC_I2C6_CLKSOURCE_MSIK           },
#endif /* I2C6 */

#if defined(USART1)
  { .ClkMuxId = RCC_CLK_MUX_USART1_APB2       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_SYSCLK       },
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_LSE          },
#endif /* USART1 */
#if defined(USART2)
  { .ClkMuxId = RCC_CLK_MUX_USART2_APB1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_SYSCLK       },
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_LSE          },
#endif /* USART2 */
#if defined(USART3)
  { .ClkMuxId = RCC_CLK_MUX_USART3_APB1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_SYSCLK       },
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_LSE          },
#endif /* USART3 */
#if defined(USART6)
  { .ClkMuxId = RCC_CLK_MUX_USART6_APB1       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_USART6_SYSCLK     , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_SYSCLK       },
  { .ClkMuxId = RCC_CLK_MUX_USART6_HSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_USART6_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_LSE          },
#endif /* USART6 */

#if defined(UART4)
  { .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_USART4_SYSCLK     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_SYSCLK        },
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_HSI           },
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_LSE           },
#endif /* UART4 */
#if defined(UART5)
  { .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_USART5_SYSCLK     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_SYSCLK        },
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI16       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_HSI           },
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_LSE           },
#endif /* USART5 */

#if defined(LPUART1)
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PCLK3       },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK    , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_SYSCLK      },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI16     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_HSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_LSE         },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_MSIK      , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_MSIK        },
#endif /* LPUART1 */

#if defined(FDCAN1)
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL1Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL1          },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL2P       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL2          },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_HSE           },
#endif /* FDCAN1 */

#if defined(SDMMC1)
  { .ClkMuxId = RCC_CLK_MUX_SDMMC_ICLK        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SDMMCSEL    , .ClkSrcVal = LL_RCC_SDMMC12_KERNELCLKSOURCE_48CLK },
  { .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL1P       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SDMMCSEL    , .ClkSrcVal = LL_RCC_SDMMC12_KERNELCLKSOURCE_PLL1  },
#endif /* SDMMC1 */

#if defined(OCTOSPI1) || defined(OCTOSPI2)
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK    , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OCTOSPI_CLKSOURCE_SYSCLK      },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_MSIK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OCTOSPI_CLKSOURCE_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL1Q     , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OCTOSPI_CLKSOURCE_PLL1        },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL2Q     , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OCTOSPI_CLKSOURCE_PLL2        },
#endif /* OCTOSPI1 || OCTOSPI2 */

#if defined(HSPI1)
  { .ClkMuxId = RCC_CLK_MUX_HSPI1_SYSCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_HSPISEL     , .ClkSrcVal = LL_RCC_HSPI_CLKSOURCE_SYSCLK         },
  { .ClkMuxId = RCC_CLK_MUX_HSPI1_PLL1Q       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_HSPISEL     , .ClkSrcVal = LL_RCC_HSPI_CLKSOURCE_PLL1           },
  { .ClkMuxId = RCC_CLK_MUX_HSPI1_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_HSPISEL     , .ClkSrcVal = LL_RCC_HSPI_CLKSOURCE_PLL2           },
  { .ClkMuxId = RCC_CLK_MUX_HSPI1_PLL3R       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_HSPISEL     , .ClkSrcVal = LL_RCC_HSPI_CLKSOURCE_PLL3           },
#endif /* HSPI1 */

#if defined(USB_OTG_HS)
  { .ClkMuxId = RCC_CLK_MUX_USBPHY_HSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USBPHYCSEL  , .ClkSrcVal = LL_RCC_USBPHYCLKSOURCE_HSE           },
  { .ClkMuxId = RCC_CLK_MUX_USBPHY_HSE_DIV2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USBPHYCSEL  , .ClkSrcVal = LL_RCC_USBPHYCLKSOURCE_HSE_DIV2      },
  { .ClkMuxId = RCC_CLK_MUX_USBPHY_PLL1P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USBPHYCSEL  , .ClkSrcVal = LL_RCC_USBPHYCLKSOURCE_PLL1          },
  { .ClkMuxId = RCC_CLK_MUX_USBPHY_PLL1P_DIV2 , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USBPHYCSEL  , .ClkSrcVal = LL_RCC_USBPHYCLKSOURCE_PLL1_DIV2     },
#endif /* USB_OTG_HS */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(LTDC)
    { .ClkMuxId = RCC_CLK_MUX_LTDC_PLL2R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LTDCSEL     , .ClkSrcVal = LL_RCC_LTDC_CLKSOURCE_PLL2           },
    { .ClkMuxId = RCC_CLK_MUX_LTDC_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LTDCSEL     , .ClkSrcVal = LL_RCC_LTDC_CLKSOURCE_PLL3           },
#endif /* LTDC */

#if defined(SAI1)
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL2           },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL3           },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL1           },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PIN            },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_HSI16        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_HSI            },
#endif /* SAI1 */
#if defined(SAI2)
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL2           },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL3           },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL1           },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_CKIN         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PIN            },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_HSI16        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_HSI            },
#endif /* SAI2 */

    /*-------------------------------- Analog --------------------------------*/

#if defined(ADC12_COMMON) || defined(ADC4) || defined(DAC1)
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HCLK         },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_SYSCLK       },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_PLL2         },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HSE          },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI16     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HSI          },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK      , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_MSIK         },
#endif /* ADC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(RNG)
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_HSI48           },
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48_DIV2    , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_HSI48_DIV2      },
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI16         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_HSI             },
#endif /* RNG */

#if defined(SAES)
  { .ClkMuxId = RCC_CLK_MUX_SAES_SHSI         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAESSEL     , .ClkSrcVal = LL_RCC_SAES_CLKSOURCE_SHSI           },
  { .ClkMuxId = RCC_CLK_MUX_SAES_SHSI_DIV2    , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_SAESSEL     , .ClkSrcVal = LL_RCC_SAES_CLKSOURCE_SHSI_DIV2      },
#endif /* SAES */

#if defined(ADF1)
  { .ClkMuxId = RCC_CLK_MUX_ADF1_HCLK         , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADF1SEL     , .ClkSrcVal = LL_RCC_ADF1_CLKSOURCE_HCLK           },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_PLL1P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADF1SEL     , .ClkSrcVal = LL_RCC_ADF1_CLKSOURCE_PLL1           },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_PLL3Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADF1SEL     , .ClkSrcVal = LL_RCC_ADF1_CLKSOURCE_PLL3           },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_CKIN         , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADF1SEL     , .ClkSrcVal = LL_RCC_ADF1_CLKSOURCE_PIN            },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_ADF1SEL     , .ClkSrcVal = LL_RCC_ADF1_CLKSOURCE_MSIK           },
#endif /* ADF1 */

#if defined(MDF1)
  { .ClkMuxId = RCC_CLK_MUX_MDF1_HCLK         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_MDF1SEL     , .ClkSrcVal = LL_RCC_MDF1_CLKSOURCE_HCLK           },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_PLL1P        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_MDF1SEL     , .ClkSrcVal = LL_RCC_MDF1_CLKSOURCE_PLL1           },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_PLL3Q        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_MDF1SEL     , .ClkSrcVal = LL_RCC_MDF1_CLKSOURCE_PLL3           },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_CKIN         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_MDF1SEL     , .ClkSrcVal = LL_RCC_MDF1_CLKSOURCE_PIN            },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_MSIK         , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_MDF1SEL     , .ClkSrcVal = LL_RCC_MDF1_CLKSOURCE_MSIK           },
#endif /* MDF1 */
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Returns module SW version
 *
 * \return Module SW version
 */
rcc_ModuleVersion_t Rcc_Get_ModuleVersion( void )
{
    rcc_ModuleVersion_t retVersion;

    retVersion.Major = RCC_MAJOR_VERSION;
    retVersion.Minor = RCC_MINOR_VERSION;
    retVersion.Patch = RCC_PATCH_VERSION;

    return (retVersion);
}


/**
 * \brief Initializes module Rcc
 *
 * This function shall call every necessary sub-module initialization function 
 * and set up all the necessary resources for the module to work. In case of
 * failure, the function shall handle it by itself and shall not be transferred
 * to AppMain layer.
 */
rcc_RequestState_t Rcc_Init( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    if( RCC_NULL_PTR != clockConfig )
    {
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_PWR );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_FLASH );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_SYSCFG );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashPrefetchActive();
        }

        /* Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral */
        LL_PWR_DisableUCPDDeadBattery();

        /* Switch to SMPS regulator instead of LDO */
        LL_PWR_SetRegulatorSupply(LL_PWR_SMPS_SUPPLY);

        while(LL_PWR_IsActiveFlag_REGULATOR()!=1)
        {
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PwrRange( clockConfig );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashLatency( clockConfig );
        }

        LL_ICACHE_SetMode( LL_ICACHE_1WAY );

        LL_ICACHE_Enable();

        LL_DCACHE_SetReadBurstType( DCACHE1, LL_DCACHE_READ_BURST_INCR );

        LL_DCACHE_Enable( DCACHE1 );

        LL_DCACHE_SetReadBurstType( DCACHE1, LL_DCACHE_READ_BURST_INCR );

        LL_DCACHE_Enable( DCACHE1 );

        CLEAR_BIT(SYSCFG->CFGR1, SYSCFG_CFGR1_SRAMCACHED);


        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_HseClk( clockConfig->HSE_Frequency_Hz );
        }

        for( rcc_MsisOutId_t msiId = (rcc_MsisOutId_t)0u; RCC_MSIS_OUT_CNT > msiId; msiId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Set_MsisActive( msiId, clockConfig->MsisConfig[ msiId ] );
            }
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Set_PllConfig( pllId, &clockConfig->Pll_Config[ pllId ] );
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_AHBDivider( clockConfig->AHB_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_APB1Divider( clockConfig->APB1_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_APB2Divider( clockConfig->APB2_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_APB3Divider( clockConfig->APB3_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_SysClkSource( clockConfig->SystemClockSource );
        }

//        /* Configure RTC clock path */
//        if( RCC_LSC_SOURCE_LSI == clockConfig->RTC_Source )
//        {
//            retState = Rcc_Set_LsiActive();
//            if( RCC_REQUEST_OK == retState )
//            {
//                return ( retState );
//            }
//        }
//        else if( RCC_LSC_SOURCE_LSE == clockConfig->RTC_Source )
//        {
//            retState = Rcc_Set_LseActive();
//            if( RCC_REQUEST_OK == retState )
//            {
//                return ( retState );
//            }
//        }
//        else
//        {
//            /* No component activation needed */
//        }
//
//        retState = Rcc_Set_RtcClkSource( clockConfig->RTC_Source );
//        if( RCC_REQUEST_OK == retState )
//        {
//            return ( retState );
//        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_FLASH );
        }

        if( RCC_REQUEST_OK == retState )
        {
            rcc_FreqHz_t sysClk = 0u;

            Rcc_Get_SysClk( &sysClk );

            LL_SetFlashLatency( sysClk );

            Rcc_Set_SysTickInterval( clockConfig->SysTickInterval );

            LL_SetSystemCoreClock( sysClk );
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* MCO configuration (return states are not needed to be checked) */
            (void)Rcc_Set_McoClockSource( clockConfig->MCO_ClockSource );

            (void)Rcc_Set_McoClockDivider( clockConfig->MCO_ClockDivider );
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Deinitializes module Rcc
 *
 * This function shall call every necessary sub-module deinitialization function 
 * and free all the resources allocated by the module. In case of failure, the 
 * function shall handle it by itself and shall not be transferred to AppMain 
 * layer.
 */
void Rcc_Deinit( rcc_ConfigStruct_t * const clockConfig )
{
    (void) clockConfig;
}


/**
 * \brief Main task of module Rcc
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's 
 * requirements.
 */
void Rcc_Task( void )
{
    return;
}


/**
 * \brief Clock configuration structure default value initialization
 *
 * \param ClockConfig [out]: Pointer to clock configuration structure.
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_DefaultConfig( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clockConfig )
    {
        clockConfig->HSE_ClockType     = RCC_HSE_TYPE_CRYSTAL;
        clockConfig->HSE_Frequency_Hz  = 8000000u;
        clockConfig->SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
        clockConfig->CSS_Enable        = RCC_FUNCTION_INACTIVE;
        clockConfig->AHB_Divider       = RCC_AHB_DIVIDER_1;
        clockConfig->APB1_Divider      = RCC_APB1_DIVIDER_1;
        clockConfig->APB2_Divider      = RCC_APB2_DIVIDER_1;
        clockConfig->APB3_Divider      = RCC_APB3_DIVIDER_1;
        clockConfig->FlashLatency      = RCC_FLASH_LATENCY_4_WS;
        clockConfig->VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_0;
        clockConfig->MCO_ClockSource   = RCC_MCO_CLOCK_SOURCE_NONE;
        clockConfig->MCO_ClockDivider  = 1u;
        clockConfig->SysTickInterval   = 1u;

        clockConfig->MsisConfig[ RCC_MSIS_OUT_K ] = RCC_MSI_CLK_16M;
        clockConfig->MsisConfig[ RCC_MSIS_OUT_S ] = RCC_MSI_CLK_16M;

        clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSI16;
        clockConfig->Pll_Config[ RCC_PLL_1 ].M_Divider    = 1u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].N_Multiplier = 20u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].P_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].Q_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].R_Divider    = 2u;

        clockConfig->Pll_Config[ RCC_PLL_2 ].Pll_Source   = RCC_PLL_SRC_HSI16;
        clockConfig->Pll_Config[ RCC_PLL_2 ].M_Divider    = 1u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].N_Multiplier = 20u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].P_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].Q_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].R_Divider    = 2u;

        clockConfig->Pll_Config[ RCC_PLL_3 ].Pll_Source   = RCC_PLL_SRC_HSI16;
        clockConfig->Pll_Config[ RCC_PLL_3 ].M_Divider    = 1u;
        clockConfig->Pll_Config[ RCC_PLL_3 ].N_Multiplier = 20u;
        clockConfig->Pll_Config[ RCC_PLL_3 ].P_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_3 ].Q_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_3 ].R_Divider    = 2u;

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*---------------------- Peripheral clock configuration ----------------------*/

/**
 * \brief Peripheral clock enable request
 *
 * User can request activation of clock for required peripheral. If required
 * peripheral is correctly activated, and required peripheral ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * The enumeration consist of all possible peripheral clock sources. User can
 * request activation of required peripheral clock source.
 *
 * \warning Some peripherals have connected clock source. (e.g. USB and RNG, all
 *          I2S's ...)
 *
 * \param periphId (in): ID of required peripheral to activate clock source
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PeriphActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;

    if( ( RCC_PERIPH_LIST_CNT                    > periphId ) &&
        ( rcc_ConfigStruct[ periphId ].PeriphId == periphId )    )
    {
        rcc_ClkMuxId_t clkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;

        /* Configure peripheral clock source */
        if( RCC_CLK_MUX_LIST_CNT != clkMuxId )
        {
            uint32_t    ctrlRegVal  = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal;
            uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
            rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

            Rcc_Set_RegVal( ctrlRegId, ctrlRegMask, ctrlRegVal );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                registerValue = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask);

                if( registerValue == ctrlRegVal )
                {
                    retrunState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retrunState = RCC_REQUEST_ERROR;
                }
            }
        }

        rcc_ClkBusId_t clkBusId  = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       stateMask = rcc_ConfigStruct[ periphId ].StateMask;

        if( RCC_CLK_BUS_CNT > clkBusId )
        {
            rcc_RegId_t stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

            /* Activate peripheral by setting "1" to corresponding register */
            Rcc_Set_RegBit( stateRegId, stateMask );

            /* Activate peripheral clock */
            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                registerValue = Rcc_Get_RegBit( stateRegId, stateMask);

                if( 0u != ( registerValue & stateMask ) )
                {
                    retrunState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retrunState = RCC_REQUEST_ERROR;
                }
            }
        }
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Peripheral clock disable request
 *
 * User can request de-activation of clock for required peripheral. If required
 * peripheral is correctly de-activated, and required peripheral ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to de-activate clock source
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PeriphInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId ) &&
        ( periphIdCheck       == periphId )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       stateMask  = rcc_ConfigStruct[ periphId ].StateMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

        Rcc_Reset_RegBit( stateRegId, stateMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            registerValue = Rcc_Get_RegBit( stateRegId, stateMask);

            if( 0u == registerValue )
            {
                retrunState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                retrunState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Reading state of peripheral reset state
 *
 * Peripherals clocks can activated or deactivated. This function can be used
 * by user to read this activation status.
 *
 * \param periphId   (in): ID of required peripheral to be inactive in low power mode
 * \param funcState (out): State of peripheral activation
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId  ) &&
        ( periphIdCheck       == periphId  ) &&
        ( RCC_NULL_PTR        != funcState )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       stateMask  = rcc_ConfigStruct[ periphId ].StateMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

        registerValue = Rcc_Get_RegBit( stateRegId, stateMask);

        if( 0u == registerValue )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            *funcState = RCC_FUNCTION_ACTIVE;
        }

        retrunState = RCC_REQUEST_OK;
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Returns clock frequency for selected peripheral.
 *
 * \param periphId   [in]: ID of required peripheral
 * \param periphClk [out]: Value of frequency for selected peripheral in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphClk( rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_LIST_CNT > periphId  ) &&
        ( RCC_NULL_PTR       != periphClk )    )
    {
        const rcc_ClkSrcId_t periphClkSrcId = rcc_ConfigStruct[ periphId ].ClkSrcId;

        if ( ( RCC_NULL_PTR   != rcc_PeriphClkSrcConfig[ periphClkSrcId ].ClkSrcCallback ) &&
             ( RCC_CLK_SRC_CNT > periphClkSrcId                                          )    )
        {
            returnState = rcc_PeriphClkSrcConfig[ periphClkSrcId ].ClkSrcCallback( periphClk );
        }
        else
        {
            *periphClk = 0u;

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
 * \brief Peripheral reset enable request
 *
 * User can request activation of reset for required peripheral. If required
 * peripheral is correctly switched to reset state, and required peripheral ID
 * is correct, returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to activate reset
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ResetActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId ) &&
        ( periphIdCheck       == periphId )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       resetMask  = rcc_ConfigStruct[ periphId ].RstCtrlMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        Rcc_Set_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            registerValue = Rcc_Get_RegBit( stateRegId, resetMask);

            if( 0u != registerValue )
            {
                retrunState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                retrunState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Peripheral reset disable request
 *
 * User can request activation of reset for required peripheral. If required
 * peripheral is correctly switched from reset state, and required peripheral ID
 * is correct, returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to deactivate reset
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ResetInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId ) &&
        ( periphIdCheck       == periphId )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       resetMask  = rcc_ConfigStruct[ periphId ].RstCtrlMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        Rcc_Reset_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            registerValue = Rcc_Get_RegBit( stateRegId, resetMask);

            if( 0u == registerValue )
            {
                retrunState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                retrunState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Reading state of peripheral reset state
 *
 * Peripherals can be reset. This function can be used by user to read this
 * reset status.
 *
 * \param periphId   (in): ID of required peripheral to be inactive in low power mode
 * \param funcState (out): State of peripheral activation
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ResetState( rcc_PeriphId_t periphId, rcc_FunctionState_t *funcState )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId  ) &&
        ( periphIdCheck       == periphId  ) &&
        ( RCC_NULL_PTR        != funcState )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       stateMask  = rcc_ConfigStruct[ periphId ].RstCtrlMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        registerValue = Rcc_Get_RegBit( stateRegId, stateMask);

        if( 0u == registerValue )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            *funcState = RCC_FUNCTION_ACTIVE;
        }

        retrunState = RCC_REQUEST_OK;
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/*------------------------- Power mode configuration -------------------------*/

/**
 * \brief Enable peripheral in sleep mode
 *
 * User can request peripheral active state in sleep mode.
 * If required peripheral is correctly configured, and required peripheral ID
 * is correct, returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to be active in low power mode
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_SleepActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId ) &&
        ( periphIdCheck       == periphId )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       resetMask  = rcc_ConfigStruct[ periphId ].LpCtrlMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        Rcc_Set_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            registerValue = Rcc_Get_RegBit( stateRegId, resetMask);

            if( 0u != registerValue )
            {
                retrunState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                retrunState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Disable peripheral in low power mode
 *
 * User can request peripheral inactive state in sleep mode.
 * If required peripheral is correctly configured, and required peripheral ID
 * is correct, returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to be inactive in low power mode
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_SleepInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId ) &&
        ( periphIdCheck       == periphId )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       resetMask  = rcc_ConfigStruct[ periphId ].LpCtrlMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        Rcc_Reset_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            registerValue = Rcc_Get_RegBit( stateRegId, resetMask);

            if( 0u == registerValue )
            {
                retrunState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                retrunState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}


/**
 * \brief Reading state of activation peripheral in sleep mode
 *
 * Peripherals can be configured to be active in sleep mode. This function can
 * be used by user to read this activation status.
 *
 * \param periphId   (in): ID of required peripheral to be inactive in low power mode
 * \param funcState (out): State of peripheral activation
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SleepState( rcc_PeriphId_t periphId, rcc_FunctionState_t *funcState )
{
    rcc_RequestState_t retrunState   = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;
    rcc_PeriphId_t     periphIdCheck = rcc_ConfigStruct[ periphId ].PeriphId;

    if( ( RCC_PERIPH_LIST_CNT  > periphId  ) &&
        ( periphIdCheck       == periphId  ) &&
        ( RCC_NULL_PTR        != funcState )    )
    {
        rcc_ClkBusId_t clkBusId   = rcc_ConfigStruct[ periphId ].ClkBusId;
        uint32_t       stateMask  = rcc_ConfigStruct[ periphId ].LpCtrlMask;
        rcc_RegId_t    stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        registerValue = Rcc_Get_RegBit( stateRegId, stateMask);

        if( 0u == registerValue )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            *funcState = RCC_FUNCTION_ACTIVE;
        }

        retrunState = RCC_REQUEST_OK;
    }
    else
    {
        retrunState = RCC_REQUEST_ERROR;
    }

    return ( retrunState );
}

/*---------------------------- Pll's configuration ---------------------------*/

/**
 * \brief Configuration of main Phase Locked Loop (PLL)
 *
 * User can configure dividers and multipliers of main PLL through PLL
 * configuration structure.
 *
 * \warning User must configure clock source before PLL configuration!
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 * \param configStruct [in]: PLL configuration structure
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllConfig( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != configStruct )
    {
        if( RCC_PLL_SRC_NONE == configStruct->Pll_Source )
        {
            Rcc_Set_PllInactive( pllId );
        }
        else
        {
            /* PLL must be inactive during configuration */
            returnState = Rcc_Set_PllInactive( pllId );

            returnState = Rcc_Set_PllsSource( pllId, configStruct->Pll_Source );

            /*------------- Configure PLL internal clock ---------------------*/
            if( ( RCC_M_DIVIDER_MIN    <= configStruct->M_Divider    ) &&
                ( RCC_M_DIVIDER_MAX    >= configStruct->M_Divider    ) &&
                ( RCC_N_MULTIPLIER_MIN <= configStruct->N_Multiplier ) &&
                ( RCC_N_MULTIPLIER_MAX >= configStruct->N_Multiplier )    )
            {
                Rcc_Set_RegVal( rcc_PllConfig[ pllId ].MDivRegId,
                                rcc_PllConfig[ pllId ].MDivMask,
                                ( configStruct->M_Divider - 1u ) << RCC_PLL1CFGR_PLL1M_Pos );

                Rcc_Set_RegVal( rcc_PllConfig[ pllId ].NMultRegId,
                                rcc_PllConfig[ pllId ].NMultMask,
                                configStruct->N_Multiplier << RCC_PLL1DIVR_PLL1N_Pos );

                rcc_FreqHz_t freqInHz = 0u;

                if( RCC_PLL_SRC_MSIS == configStruct->Pll_Source )
                {
                    returnState = Rcc_Get_MsisClk( RCC_MSIS_OUT_S, &freqInHz );
                }
                else if (RCC_PLL_SRC_HSE == configStruct->Pll_Source)
                {
                    returnState = Rcc_Get_HseClk( &freqInHz );
                }
                else if (RCC_PLL_SRC_HSI16 == configStruct->Pll_Source)
                {
                    returnState = Rcc_Get_Hsi16Clk( &freqInHz );
                }
                else
                {
                    /* Unsupported PLL source */
                    returnState = RCC_REQUEST_ERROR;
                }

                uint32_t regValue = 0u;

                if( RCC_REQUEST_ERROR != returnState )
                {
                    if( ( 8000000u  <= freqInHz ) &&
                        ( 16000000u >= freqInHz )    )
                    {
                        regValue = RCC_PLL1CFGR_PLL1RGE_1;
                    }
                    else if( ( 4000000u <= freqInHz ) &&
                             ( 8000000u >  freqInHz )    )
                    {
                        regValue = RCC_PLL1CFGR_PLL1RGE_0;
                    }
                    else
                    {
                        /* Incorrect input frequency */
                        returnState = RCC_REQUEST_ERROR;
                    }
                }

                if( RCC_REQUEST_ERROR != returnState )
                {
                    Rcc_Set_RegVal( rcc_PllConfig[ pllId ].FreqInRangeRegId,
                                    rcc_PllConfig[ pllId ].FreqInRangeMask,
                                    regValue );
                }
            }
            else
            {
                /* Incorrect PLL internal configuration */
                returnState = RCC_REQUEST_ERROR;
            }


            /*-------------------- Configure PLL P output --------------------*/
            if( ( 0u             != configStruct->P_Divider ) &&
                ( RCC_REQUEST_OK == returnState             )    )
            {
                if( ( RCC_P_DIVIDER_MIN <= configStruct->P_Divider ) &&
                    ( RCC_P_DIVIDER_MAX >= configStruct->P_Divider )    )
                {
                    uint32_t regValue = configStruct->P_Divider - 1u;

                    /* Bit shift "RCC_PLL1DIVR_PLL1P_Pos" is used for all PLL's.
                                         * At least this ST did not screw up. */
                    Rcc_Set_RegVal( rcc_PllConfig[ pllId ].OutPConfRegId,
                                    rcc_PllConfig[ pllId ].OutPConfMask,
                                    regValue << RCC_PLL1DIVR_PLL1P_Pos );

                    /* Activate output P */
                    Rcc_Set_RegVal( rcc_PllConfig[ pllId ].OutPStateRegId,
                                    rcc_PllConfig[ pllId ].OutPStateMask,
                                    1u << RCC_PLL1CFGR_PLL1PEN_Pos );
                }
                else
                {
                    /* Output P divider has incorrect value */
                    returnState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* PLL output P is not used. */
            }

            /*-------------------- Configure PLL Q output --------------------*/
            if( ( 0u             != configStruct->Q_Divider ) &&
                ( RCC_REQUEST_OK == returnState             )    )
            {
                if( ( RCC_Q_DIVIDER_MIN <= configStruct->Q_Divider ) &&
                    ( RCC_Q_DIVIDER_MAX >= configStruct->Q_Divider )    )
                {
                    uint32_t regValue = configStruct->Q_Divider - 1u;

                    /* Bit shift "RCC_PLL1DIVR_PLL1Q_Pos" is used for all PLL's.
                     * At least this ST did not screw up. */
                    Rcc_Set_RegVal( rcc_PllConfig[ pllId ].OutQConfRegId,
                                    rcc_PllConfig[ pllId ].OutQConfMask,
                                    regValue << RCC_PLL1DIVR_PLL1Q_Pos );

                    /* Activate output Q */
                    Rcc_Set_RegVal( rcc_PllConfig[ pllId ].OutQStateRegId,
                                    rcc_PllConfig[ pllId ].OutQStateMask,
                                    1u << RCC_PLL1CFGR_PLL1QEN_Pos );
                }
                else
                {
                    /* Output Q divider has incorrect value */
                    returnState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* PLL output Q is not used. */
            }

            /*-------------------- Configure PLL R output --------------------*/
            if( ( 0u             != configStruct->R_Divider ) &&
                ( RCC_REQUEST_OK == returnState             )    )
            {
                if( ( RCC_R_DIVIDER_MIN <= configStruct->R_Divider ) &&
                    ( RCC_R_DIVIDER_MAX >= configStruct->R_Divider )    )
                {
                    if( ( RCC_PLL_1 == pllId                     ) &&
                        ( 0u != ( configStruct->R_Divider % 2u ) ) &&
                        ( 1u != configStruct->R_Divider          )    )
                    {
                        /* PLL1 divider R can be one and even division factors. */
                        returnState = RCC_REQUEST_ERROR;
                    }
                    else
                    {
                        uint32_t regValue = configStruct->R_Divider - 1u;

                        /* Bit shift "RCC_PLL1DIVR_PLL1P_Pos" is used for all PLL's.
                         * At least this ST did not screw up. */
                        Rcc_Set_RegVal( rcc_PllConfig[ pllId ].OutRConfRegId,
                                        rcc_PllConfig[ pllId ].OutRConfMask,
                                        regValue << RCC_PLL1DIVR_PLL1R_Pos );

                        /* Activate output R */
                        Rcc_Set_RegVal( rcc_PllConfig[ pllId ].OutRStateRegId,
                                        rcc_PllConfig[ pllId ].OutRStateMask,
                                        1u << RCC_PLL1CFGR_PLL1REN_Pos );
                    }
                }
                else
                {
                    /* Output R divider has incorrect value */
                    returnState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* PLL output Q is not used. */
            }

            /* Activate PLL. This must be the last step! */
            Rcc_Set_PllActive( pllId );
        }
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Returns PLL internal frequency
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \param busClk [out]: Pointer to PLL internal frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllInternalClk( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState     = RCC_REQUEST_ERROR;
    rcc_PllClkSrc_t    pllClkSource = 0u;
    rcc_FreqHz_t       pllIntFreq   = 0u;
    uint32_t           pllNMult     = 0u;
    uint32_t           pllMDiv      = 0u;
    uint32_t           inputClkFreq = 0u;

    retState = Rcc_Get_PllsSource( pllId, &pllClkSource );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        rcc_PllClkSrc_t pllClkSource = RCC_PLL_SRC_NONE;

        Rcc_Get_PllsSource( pllId, &pllClkSource );

        if( RCC_PLL_SRC_HSE == pllClkSource )
        {
            retState     = RCC_REQUEST_OK;
            inputClkFreq = rcc_HseFreqHz;
        }
        if( RCC_PLL_SRC_MSIS == pllClkSource )
        {
            retState = RCC_REQUEST_OK;

            Rcc_Get_MsisClk( RCC_MSIS_OUT_S, &inputClkFreq );
        }
        else if( RCC_PLL_SRC_HSI16 == pllClkSource )
        {
            retState = RCC_REQUEST_OK;

            Rcc_Get_Hsi16Clk( &inputClkFreq );
        }
        else
        {
            retState     = RCC_REQUEST_ERROR;
            inputClkFreq = 0u;
        }

        uint32_t mDivRegValue  = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].MDivRegId , rcc_PllConfig[ pllId ].MDivMask  );
        uint32_t nMultRegValue = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].NMultRegId, rcc_PllConfig[ pllId ].NMultMask );

        pllNMult = ( nMultRegValue >> RCC_PLL1DIVR_PLL1N_Pos );
        pllMDiv  = ( mDivRegValue  >> RCC_PLL1CFGR_PLL1M_Pos );


        if( 0u != pllMDiv )
        {
            pllIntFreq = ( ( inputClkFreq / pllMDiv ) * pllNMult );
        }
        else
        {
            pllIntFreq = 0u;
        }

        if( RCC_REQUEST_ERROR != retState )
        {
            *pllClk = pllIntFreq;

            retState = RCC_REQUEST_OK;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activation of Phase Locked Loop (LSI) block
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllActive( rcc_PllId_t pllId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Set_RegBit( rcc_PllConfig[ pllId ].StateRegId, rcc_PllConfig[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( rcc_PllConfig[ pllId ].StateRegId, rcc_PllConfig[ pllId ].RdyFlagMask );

            if( 0u != regValue )
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
        /* Incorrect PLL Id */
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief De-activation of Phase Locked Loop (LSI) block
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllInactive( rcc_PllId_t pllId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Reset_RegBit( rcc_PllConfig[ pllId ].StateRegId, rcc_PllConfig[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( rcc_PllConfig[ pllId ].StateRegId, rcc_PllConfig[ pllId ].StateMask );

            if( 0u == regValue )
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
        /* Incorrect PLL Id */
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Reading status of Phase Locked Loop (PLL) block
 *
 * \param retState [out]: Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllState( rcc_PllId_t pllId, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState    = RCC_REQUEST_ERROR;
    uint32_t           readyRegVal    = 0u;
    uint32_t           pllStateRegVal = 0u;

    if( ( RCC_NULL_PTR != retState ) &&
        ( RCC_PLL_CNT   > pllId    )    )
    {
        readyRegVal = Rcc_Get_RegBit( rcc_PllConfig[ pllId ].RdyFlagRegId, rcc_PllConfig[ pllId ].RdyFlagMask);

        pllStateRegVal = Rcc_Get_RegBit( rcc_PllConfig[ pllId ].StateRegId, rcc_PllConfig[ pllId ].StateMask);;

        if ( ( 0u != readyRegVal    ) &&
             ( 0u != pllStateRegVal )    )
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
 * \brief Selection of clock source for Phase Locked Loop's multiplexer
 *
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_NONE : PLL is inactive
 *  - \ref RCC_PLL_SRC_CSI  : PLL will be clocked by CSI oscillator
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllsSource( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    rcc_RequestState_t returnState  = RCC_REQUEST_ERROR;
    uint32_t           targetRegVal = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        if( RCC_PLL_SRC_MSIS == clkSource )
        {
            targetRegVal = LL_RCC_PLL1SOURCE_MSIS;

            returnState = RCC_REQUEST_OK;
        }
        else if( RCC_PLL_SRC_HSE == clkSource )
        {
            targetRegVal = LL_RCC_PLL1SOURCE_HSE;

            returnState = RCC_REQUEST_OK;
        }
        else if( RCC_PLL_SRC_HSI16  == clkSource )
        {
            targetRegVal = LL_RCC_PLL1SOURCE_HSI;

            returnState = Rcc_Set_Hsi16Active();
        }
        else
        {
            returnState = RCC_REQUEST_ERROR;

            targetRegVal = 0u;
        }

        if( RCC_REQUEST_ERROR != returnState )
        {
            Rcc_Set_RegVal( rcc_PllConfig[ pllId ].ClkSrcRegId,
                            rcc_PllConfig[ pllId ].ClkSrcMask,
                            targetRegVal );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                uint32_t regValue = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].ClkSrcRegId,
                                                    rcc_PllConfig[ pllId ].ClkSrcMask );

                regValue = ( regValue & RCC_PLL1CFGR_PLL1SRC ) >> RCC_PLL1CFGR_PLL1SRC_Pos;

                if( regValue == targetRegVal )
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
            /* Incorrect clock source was requested */
        }
    }
    else
    {
        /* Incorrect PLL Id */
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Selection of clock source for Phase Locked Loop's multiplexer
 *
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_NONE : PLL is inactive
 *  - \ref RCC_PLL_SRC_CSI  : PLL will be clocked by CSI oscillator
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllsSource( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != clkSource ) &&
        ( RCC_PLL_CNT   > pllId     )    )
    {
        uint32_t regValue = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].ClkSrcRegId,
                                            rcc_PllConfig[ pllId ].ClkSrcMask );

        regValue = ( regValue & rcc_PllConfig[ pllId ].ClkSrcMask ) >> RCC_PLL1CFGR_PLL1SRC_Pos;

        if( LL_RCC_PLL1SOURCE_MSIS == regValue )
        {
            *clkSource = RCC_PLL_SRC_MSIS;
        }
        else if( LL_RCC_PLL1SOURCE_HSI == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSI16;
        }
        else if( LL_RCC_PLL1SOURCE_HSE == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSE;
        }
        else
        {
            *clkSource = RCC_PLL_SRC_NONE;
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
 * \brief Reading of output frequency from main PLL from output P
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Get_PllInternalClk( pllId, &pllIntFreq );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        uint32_t regVal = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].OutPConfRegId,
                                          rcc_PllConfig[ pllId ].OutPConfMask );

        regVal = ( ( regVal & rcc_PllConfig[ pllId ].OutPConfMask ) >> RCC_PLL1DIVR_PLL1P_Pos ) + 1u;

        if( 0u != regVal )
        {
            *pllClk = ( pllIntFreq / regVal );
        }
        else
        {
            *pllClk = 0u;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Reading of output frequency from main PLL from output Q
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Get_PllInternalClk( pllId, &pllIntFreq );

    if ((RCC_REQUEST_ERROR != retState) && ( RCC_NULL_PTR != pllClk))
    {
        uint32_t regVal = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].OutQConfRegId,
                                          rcc_PllConfig[ pllId ].OutQConfMask );

        regVal = ( ( regVal & rcc_PllConfig[ pllId ].OutQConfMask ) >> RCC_PLL1DIVR_PLL1Q_Pos ) + 1u;

        pllIntFreq = ( pllIntFreq / regVal );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Reading of output frequency from main PLL from output R
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Get_PllInternalClk( pllId, &pllIntFreq );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        uint32_t regVal = Rcc_Get_RegVal( rcc_PllConfig[ pllId ].OutRConfRegId,
                                          rcc_PllConfig[ pllId ].OutRConfMask );

        regVal = ( ( regVal & rcc_PllConfig[ pllId ].OutRConfMask ) >> RCC_PLL1DIVR_PLL1R_Pos ) + 1u;

        pllIntFreq = ( pllIntFreq / regVal );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------------ Clock sources configuration -----------------------*/

/**
 * \brief Activation of High Speed External (HSE) block
 *
 * \param hseType [in]: Type of clock source. Can be crystal or external clock
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_HseActive( rcc_HseType_t hseType)
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    /* HSE must be inactive before configuration */
    returnState = Rcc_Set_HseInactive();

    if( RCC_REQUEST_OK == returnState )
    {
        if( RCC_HSE_TYPE_SIG_ANALOG_IN == hseType )
        {
            regValue = LL_RCC_HSE_ANALOG_MODE;

            LL_RCC_HSE_EnableBypass();
            LL_RCC_HSE_SetClockMode( regValue );
        }
        else if( RCC_HSE_TYPE_SIG_DIGITAL_IN == hseType )
        {
            regValue = LL_RCC_HSE_DIGITAL_MODE;

            LL_RCC_HSE_EnableBypass();
            LL_RCC_HSE_SetClockMode( regValue );
        }
        else
        {
            /* Other clock types do not need extra functionality activation */
        }

        LL_RCC_HSE_Enable();

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_RCC_HSE_IsReady();

            if( 0u != regValue )
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
        /* HSE de-activation was not successful. */
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
rcc_RequestState_t Rcc_Set_HseInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSE_Disable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSE_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External (HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_HseState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSE_IsReady();

        regValue = READ_BIT(RCC->CR, RCC_CR_HSEON);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
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
 * \brief Configuring High Speed External (HSE) frequency configured by user
 *
 * \param hseFreq [in]: Frequency of HSE clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_HseClk( rcc_FreqHz_t hseFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( 0u != hseFreq )
    {
        rcc_HseFreqHz = hseFreq;

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
 * \param hseFreq [out]: Frequency of HSE clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_HseClk( rcc_FreqHz_t * const hseFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != hseFreq )
    {
        *hseFreq = rcc_HseFreqHz;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of High Speed Internal (HSI) block
 *
 * High Speed Internal (HSI) RC oscillator is connected through clock divider.
 * User must select desired output frequency. RC itself generate 64MHz frequency
 * which can be divided by 1/2/4/8.
 *
 * \param hsiConfig [in]: Required frequency output from enumeration:
 *  - \ref RCC_HSI64_INACTIVE   : 64MHz HSI oscillator is inactive
 *  - \ref RCC_HSI64_FREQ_64MHZ : 64Mhz HSI oscillator not divided
 *  - \ref RCC_HSI64_FREQ_32MHZ : 64Mhz HSI oscillator divided by 2
 *  - \ref RCC_HSI64_FREQ_16MHZ : 64Mhz HSI oscillator divided by 4
 *  - \ref RCC_HSI64_FREQ_8MHZ  : 64Mhz HSI oscillator divided by 8
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_Hsi16Active( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI_Enable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI_IsReady();

        uint32_t hsiState = READ_BIT(RCC->CR, RCC_CR_HSION);

        if( ( 0u != regValue ) ||
            ( 0u == hsiState )    )
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

    LL_RCC_HSI_SetCalibTrimming( RCC_HSI_DEFAULT_TRIM_VALUE );

    return ( returnState );
}


/**
 * \brief De-activation of High Speed Internal (HSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_Hsi16Inactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI_Disable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External (HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_Hsi16State( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSI_IsReady();

        regValue = READ_BIT(RCC->CR, RCC_CR_HSION);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
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
 * \brief Reading High Speed Internal (HSI) frequency configured by user
 *
 * \param hsiClk [out]: Frequency of HSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_Hsi16Clk( rcc_FreqHz_t * const hsiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != hsiClk )
    {
        *hsiClk = HSI_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}




/**
 * \brief Activation of High Speed Internal 48MHz (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_Hsi48Active( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI48_Enable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI48_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of High Speed Internal  48MHz (HSI48) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_Hsi48Inactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_HSI48_Disable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_HSI48_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External 48MHz (HSI48HSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_Hsi48State( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_HSI48_IsReady();

        regValue = READ_BIT( RCC->CR, RCC_CR_HSI48ON );

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
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
 * \brief Reading High Speed Internal 48MHz (HSI48) frequency configured by user
 *
 * \param hsiClk [out]: Frequency of HSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_Hsi48Clk( rcc_FreqHz_t * const hsiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != hsiClk )
    {
        *hsiClk = HSI48_VALUE;

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}


/**
 * \brief Activation of Medium Speed Internal (MSIS) block.
 *
 * This function activates either MSIS or MSISK clock source.
 *
 * \param msisId [in]: Required MSIS clock source identification.
 * \param clkFreq [in]: Required MSIS clock frequency.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_MsisActive( rcc_MsisOutId_t msisId, rcc_MsisClkFreq_t clkFreq )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    if( ( RCC_MSIS_OUT_CNT > msisId  ) &&
        ( RCC_MSI_CLK_CNT  > clkFreq )    )
    {

        if( RCC_MSIS_OUT_K == msisId )
        {
            LL_RCC_MSIK_Enable();

            LL_RCC_MSI_EnableRangeSelection();

            LL_RCC_MSIK_SetRange( rcc_MsiConfig[ clkFreq ].RegRangeVal );
        }
        else
        {
            LL_RCC_MSIS_Enable();

            LL_RCC_MSI_EnableRangeSelection();

            LL_RCC_MSIS_SetRange( rcc_MsiConfig[ clkFreq ].RegRangeVal );
        }

        LL_RCC_MSI_SetCalibTrimming( 16u, LL_RCC_MSI_OSCILLATOR_3 );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( RCC_MSIS_OUT_K == msisId )
            {
                regValue = LL_RCC_MSIK_IsReady();
            }
            else
            {
                regValue = LL_RCC_MSIS_IsReady();
            }

            if( 0u != regValue )
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
 * \brief De-activation of High Speed Internal  48MHz (CSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_MsisInactive( rcc_MsisOutId_t msisId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_MSIS_OUT_K == msisId )
    {
        LL_RCC_MSIK_Disable();
    }
    else
    {
        LL_RCC_MSIS_Disable();
    }

    returnState = RCC_REQUEST_OK;

    return ( returnState );
}


/**
 * \brief Reading status of High Speed External 48MHz (CSIHSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_MsisState( rcc_MsisOutId_t msisId, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    if( ( RCC_NULL_PTR     != retState ) &&
        ( RCC_MSIS_OUT_CNT  > msisId   )    )
    {
        if( RCC_MSIS_OUT_K == msisId )
        {
            regValue = LL_RCC_MSIK_IsReady();
        }
        else
        {
            regValue = LL_RCC_MSIS_IsReady();
        }

        if( 0u != regValue )
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
 * \brief Reading Low power RC internal oscillator (CSI) frequency
 *
 * \param csiClk [out]: Frequency of CSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_MsisClk( rcc_MsisOutId_t msisId, rcc_FreqHz_t * const msiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           msiRange    = 0u;

    if( ( RCC_NULL_PTR    != msiClk ) &&
        ( RCC_MSIS_OUT_CNT > msisId )    )
    {
        if( RCC_MSIS_OUT_K == msisId )
        {
            msiRange = LL_RCC_MSIK_GetRange();
        }
        else
        {
            msiRange = LL_RCC_MSIS_GetRange();
        }

        for( rcc_MsisClkFreq_t freqId = 0u; RCC_MSI_CLK_CNT > freqId; freqId ++ )
        {
            if( msiRange == rcc_MsiConfig[ freqId ].RegRangeVal )
            {
                *msiClk = rcc_MsiConfig[ freqId ].OutFreq;

                returnState = RCC_REQUEST_OK;

                break;
            }
            else
            {
                /* Actual value not found yet */
            }
        }
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Activation of Low Speed Internal (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_LseActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_LSE_Enable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_LSE_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of Low Speed Internal (LSE) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_LseInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_LSE_Disable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_LSE_IsReady();

        if( 0u == regValue )
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

    return ( returnState );
}


/**
 * \brief Reading status of Low Speed Internal (LSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_LseState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_LSE_IsReady();

        regValue = READ_BIT(RCC->BDCR, RCC_BDCR_LSEON);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
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
 * \brief Returns value of Low Speed External (LSE) frequency
 *
 * \param lseClk [out]: Frequency of LSE clock in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
rcc_RequestState_t Rcc_Get_LseClk( rcc_FreqHz_t * const csiClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != csiClk )
    {
        *csiClk = LSE_VALUE;

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
rcc_RequestState_t Rcc_Set_LsiActive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_LSI_Enable();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_LSI_IsReady();

        if( 0u != regValue )
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

    return ( returnState );
}


/**
 * \brief De-activation of Low Speed Internal (LSI) block
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_LsiInactive( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_LSI_Disable();

    returnState = RCC_REQUEST_OK;

    return ( returnState );
}


/**
 * \brief Reading status of Low Speed Internal (LSE) block
 *
 * \param retState [out] : Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_LsiState( rcc_FunctionState_t *retState )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           readyState  = 0u;
    uint32_t           regValue    = 0u;

    if( RCC_NULL_PTR != retState )
    {
        readyState = LL_RCC_LSI_IsReady();

        regValue = READ_BIT(RCC->BDCR, RCC_BDCR_LSION);

        if( ( 0u != readyState ) &&
            ( 0u != regValue   )    )
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
 * \brief Returns value of Low Speed Internal (LSI) frequency
 *
 * \param lsiClk [out]: Frequency of LSI clock in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_LsiClk( rcc_FreqHz_t * const lsiClk )
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


/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * \param clkSource [in]: RTC clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_RtcClkSource( rcc_Rtc_ClkSource_t clkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    LL_RCC_SetRTCClockSource( clkSource );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        uint32_t registerValue = LL_RCC_GetRTCClockSource();

        if( registerValue == clkSource )
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

    return ( returnState );
}


/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * \param clkSource [in]: RTC clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_RtcClkSource( rcc_Rtc_ClkSource_t * const clkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkSource )
    {
        *clkSource = LL_RCC_GetRTCClockSource();

        returnState = RCC_REQUEST_OK;
    }
    else
    {
        returnState = RCC_REQUEST_ERROR;
    }

    return ( returnState );
}

/*------------------------- Clock buses configuration ------------------------*/

/**
 * \brief Configure system clock source multiplexer.
 *
 * System clock source multiplexer is used for selection of main clock source.
 *
 * \param systemClkSource [in] : Selected clock source ID
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_SysClkSource( rcc_SystemClkSrc_t systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           clkSource   = 0u;

    if( ( RCC_SYSTEM_CLOCK_SOURCE_HSI == systemClkSource ) ||
        ( RCC_SYSTEM_CLOCK_SOURCE_HSE == systemClkSource ) ||
        ( RCC_SYSTEM_CLOCK_SOURCE_PLL == systemClkSource )    )
    {
        LL_RCC_SetSysClkSource( systemClkSource );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            clkSource = LL_RCC_GetSysClkSource() >> RCC_CFGR1_SWS_Pos;

            if( systemClkSource == clkSource )
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
rcc_RequestState_t Rcc_Get_SysClkSource( rcc_SystemClkSrc_t *systemClkSource )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != systemClkSource )
    {
        uint32_t regValue = LL_RCC_GetSysClkSource();
        *systemClkSource = regValue >> RCC_CFGR1_SWS_Pos;

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
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param busClk [out]: Pointer to SYSCLK value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SysClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState        = RCC_REQUEST_ERROR;
    rcc_SystemClkSrc_t systemClkSource = 0u;

    /* Read System Clock Multiplexer settings */
    retState = Rcc_Get_SysClkSource( &systemClkSource );

    if( ( RCC_REQUEST_ERROR != retState    ) &&
        ( RCC_NULL_PTR      != busClk      )    )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == systemClkSource )
        {
            retState = Rcc_Get_PllClk_OutP( RCC_PLL_1, busClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == systemClkSource )
        {
            *busClk = rcc_HseFreqHz;
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == systemClkSource )
        {
            *busClk = HSI_VALUE;
            retState = RCC_REQUEST_OK;
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
 * \param dividerId [in] : Required AHB divider value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_AHBDivider( rcc_AHB_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAHBPrescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAHBPrescaler();

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
 * HCLK is clocked through AHB divider.
 *
 * \param dividerId [out] : AHB divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_AHBDivider( rcc_AHB_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAHBPrescaler();

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
rcc_RequestState_t Rcc_Get_AHBClk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2  = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       systemClk  = 0u;
    rcc_AHB_Div_t      ahbDivider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_Get_SysClk( &systemClk );
    retState2 = Rcc_Get_AHBDivider( &ahbDivider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk      ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
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
 * \param dividerId [in] : Identification of divider configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_APB1Divider( rcc_APB1_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAPB1Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAPB1Prescaler();

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
 * Clock bus APB1 is clocked through APB1 divider from HCLK (AHB bus).
 *
 * \param dividerId     [out] : Divider ID
 * \param dividerNumVal [out] : Divider numerical value
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_APB1Divider( rcc_APB1_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAPB1Prescaler();

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
rcc_RequestState_t Rcc_Get_APB1Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB1_Div_t     apb1Divider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_Get_AHBClk( &hClk );
    retState2 = Rcc_Get_APB1Divider( &apb1Divider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk    ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
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
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_APB2Divider( rcc_APB2_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAPB2Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAPB2Prescaler();

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
 * Clock bus APB2 is clocked through APB2 divider from HCLK (AHB bus).
 *
 * \param dividerId [out] : Divider ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_APB2Divider( rcc_APB2_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAPB2Prescaler();

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
rcc_RequestState_t Rcc_Get_APB2Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB2_Div_t     apb2Divider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_Get_AHBClk( &hClk );
    retState2 = Rcc_Get_APB2Divider( &apb2Divider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk    ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
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
 * \brief Configure APB3 divider.
 *
 * Clock bus APB3 is clocked through APB3 divider from HCLK (AHB bus).
 *
 * \param dividerId [in] : Identification of divider configuration
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_APB3Divider( rcc_APB3_Div_t dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    LL_RCC_SetAPB3Prescaler( dividerId );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = LL_RCC_GetAPB3Prescaler();

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
 * Clock bus APB3 is clocked through APB3 divider from HCLK (AHB bus).
 *
 * \param dividerId [out] : Divider ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_APB3Divider( rcc_APB3_Div_t *dividerId )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    *dividerId = LL_RCC_GetAPB3Prescaler();

    return ( returnState );
}


/**
 * \brief Reading of output frequency of APB3 clock bus
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param busClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_APB3Clk( rcc_FreqHz_t * const busClk )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    rcc_RequestState_t retState2   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hClk        = 0u;
    rcc_APB3_Div_t     apb3Divider = 0u;

    /* Get system clock frequency */
    retState  = Rcc_Get_AHBClk( &hClk );
    retState2 = Rcc_Get_APB3Divider( &apb3Divider );

    if( ( RCC_REQUEST_ERROR != retState  ) &&
        ( RCC_NULL_PTR      != busClk    ) &&
        ( RCC_REQUEST_ERROR != retState2 )    )
    {
        *busClk = __LL_RCC_CALC_PCLK3_FREQ( hClk, apb3Divider );
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Function used for power range configuration
 *
 * Power range configuration is dependent on expected core clock (SYSCLK) and
 * must be updated before setting this clock over 80MHz.
 *
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ===============================================================================================================
 * | Number of wait states | Programming delay |   VOS3 range   |   VOS2 range   |   VOS1 range   |   VOS0 range   |
 * |        (LATENCY)      |    (WRHIGHFREQ)   | 0.95 to 1.05 V | 1.05 to 1.15 V | 1.15 to 1.26 V | 1.30 to 1.40 V |
 * |=======================|===================|================|================|================|================|
 * |          0 WS         |                   | 0 to 20 MHz    | 0 to 30 MHz    | 0 to 34 MHz    | 0 to 42 MHz    |
 * | (1 FLASH clock cycle) |                   |                |                |                |                |
 * |-----------------------|        00         |----------------|----------------|----------------|----------------|
 * |          1 WS         |                   | 20 to 40 MHz   | 30 to 60 MHz   | 34 to 68 MHz   | 42 to 84 MHz   |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          2 WS         |                   | 40 to 60 MHz   | 60 to 90 MHz   | 68 to 102 MHz  | 84 to 126 MHz  |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        01         |----------------|----------------|----------------|----------------|
 * |          3 WS         |                   | 60 to 80 MHz   | 90 to 120 MHz  | 102 to 136 MHz | 126 to 168 MHz |
 * | (3 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          4 WS         |                   | 80 to 100 MHz  | 120 to 150 MHz | 136 to 170 MHz | 168 to 210 MHz |
 * | (4 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        10         |----------------|----------------|----------------|----------------|
 * |          5 WS         |                   | N/A            | N/A            | 170 to 200 MHz | 210 to 250 MHz |
 * | (5 FLASH clock cycles)|                   |                |                |                |                |
 *  ===============================================================================================================
 * \endcond
 *
* \param clockConfig [in]: Configuration structure
* \return State of request execution. Returns "OK" if request was success,
*         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PwrRange( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState               = RCC_REQUEST_ERROR;
    uint32_t           voltageScalingRegValue = 0u;

    LL_PWR_SetRegulVoltageScaling( clockConfig->VoltageScaling );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        voltageScalingRegValue = LL_PWR_GetRegulVoltageScaling();

        LL_PWR_IsActiveFlag_VOS();

        if( clockConfig->VoltageScaling == voltageScalingRegValue )
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

    return ( retState );
}


/**
 * \brief Function used to flash wait states ("delay") configuration.
 *
 * The flash wait state is dependent on expected core frequency (SYSCLK). The
 * purpose of this functionality is to delay between accessing FLASH memory.
 * The range of wait states can be seen below:
 *
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ===============================================================================================================
 * | Number of wait states | Programming delay |   VOS3 range   |   VOS2 range   |   VOS1 range   |   VOS0 range   |
 * |        (LATENCY)      |    (WRHIGHFREQ)   | 0.95 to 1.05 V | 1.05 to 1.15 V | 1.15 to 1.26 V | 1.30 to 1.40 V |
 * |=======================|===================|================|================|================|================|
 * |          0 WS         |                   | 0 to 20 MHz    | 0 to 30 MHz    | 0 to 34 MHz    | 0 to 42 MHz    |
 * | (1 FLASH clock cycle) |                   |                |                |                |                |
 * |-----------------------|        00         |----------------|----------------|----------------|----------------|
 * |          1 WS         |                   | 20 to 40 MHz   | 30 to 60 MHz   | 34 to 68 MHz   | 42 to 84 MHz   |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          2 WS         |                   | 40 to 60 MHz   | 60 to 90 MHz   | 68 to 102 MHz  | 84 to 126 MHz  |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        01         |----------------|----------------|----------------|----------------|
 * |          3 WS         |                   | 60 to 80 MHz   | 90 to 120 MHz  | 102 to 136 MHz | 126 to 168 MHz |
 * | (3 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          4 WS         |                   | 80 to 100 MHz  | 120 to 150 MHz | 136 to 170 MHz | 168 to 210 MHz |
 * | (4 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        10         |----------------|----------------|----------------|----------------|
 * |          5 WS         |                   | N/A            | N/A            | 170 to 200 MHz | 210 to 250 MHz |
 * | (5 FLASH clock cycles)|                   |                |                |                |                |
 *  ===============================================================================================================
 * \endcond
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashLatency( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState       = RCC_REQUEST_OK;
    rcc_FreqHz_t       expectedSysClk = 0u;

    retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );

    uint32_t timeout;
    uint32_t getlatency;
    uint32_t latency = LL_FLASH_LATENCY_0;  /* default value 0WS */

    /* Frequency cannot be equal to 0 */
    if( ( 0u           == expectedSysClk ) ||
        ( RCC_NULL_PTR == clockConfig    )    )
    {
        retState = RCC_REQUEST_ERROR;
    }
    else
    {
        if (LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE1)
        {
            if (expectedSysClk <= UTILS_SCALE1_LATENCY0_FREQ)
            {
                /* 0 < HCLK <= 32 => 0WS (1 CPU cycles) : Do nothing, keep latency to default  LL_FLASH_LATENCY_0 */
            }
            else if ((expectedSysClk <= UTILS_SCALE1_LATENCY1_FREQ))
            {
                /* 32 < HCLK <=64  => 1WS (2 CPU cycles) */
                latency = LL_FLASH_LATENCY_1;
            }
            else if (expectedSysClk <= UTILS_SCALE1_LATENCY2_FREQ)
            {
                /* 64 < HCLK <= 96 => 2WS (3 CPU cycles) */
                latency = LL_FLASH_LATENCY_2;
            }
            else if (expectedSysClk <= UTILS_SCALE1_LATENCY3_FREQ)
            {
                /* 96 < HCLK <= 128 => 3WS (4 CPU cycles) */
                latency = LL_FLASH_LATENCY_3;
            }
            else if (expectedSysClk <= UTILS_SCALE1_LATENCY4_FREQ)
            {
                /* 128 < HCLK <= 160 => 4WS (5 CPU cycles) */
                latency = LL_FLASH_LATENCY_4;
            }
            else
            {
                retState = RCC_REQUEST_ERROR;
            }
        /* else expectedSysClk <= 10MHz default LL_FLASH_LATENCY_0 0WS */
        }
        else if (LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE2)
        {
            if (expectedSysClk <= UTILS_SCALE2_LATENCY0_FREQ)
            {
                /* 0 < HCLK <= 25 => 0WS (1 CPU cycles) : Do nothing, keep latency to default  LL_FLASH_LATENCY_0 */
            }
            else if (expectedSysClk <= UTILS_SCALE2_LATENCY1_FREQ)
            {
                /* 25 < HCLK <= 50 => 1WS (2 CPU cycles) */
                latency = LL_FLASH_LATENCY_1;
            }
            else if (expectedSysClk <= UTILS_SCALE2_LATENCY2_FREQ)
            {
                /* 50 < HCLK <= 75 => 2WS (3 CPU cycles) */
                latency = LL_FLASH_LATENCY_2;
            }
            else if (expectedSysClk <= UTILS_SCALE2_LATENCY3_FREQ)
            {
                /* 75 < HCLK <= 100 => 3WS (4 CPU cycles) */
                latency = LL_FLASH_LATENCY_3;
            }
            else
            {
                retState = RCC_REQUEST_ERROR;
            }
        /* else expectedSysClk <= 10MHz default LL_FLASH_LATENCY_0 0WS */
        }
        else if (LL_PWR_GetRegulVoltageScaling() == LL_PWR_REGU_VOLTAGE_SCALE3)
        {
            if (expectedSysClk  <= UTILS_SCALE3_LATENCY0_FREQ)
            {
                /* 0 < HCLK <= 12.5 => 0WS (1 CPU cycles) : Do nothing, keep latency to default  LL_FLASH_LATENCY_0 */
            }
            else if (expectedSysClk <= UTILS_SCALE3_LATENCY1_FREQ)
            {
                /* 12.5 < HCLK <= 25 => 1WS (2 CPU cycles) */
                latency = LL_FLASH_LATENCY_1;
            }
            else if (expectedSysClk <= UTILS_SCALE3_LATENCY2_FREQ)
            {
                /* 25 < HCLK <= 37.5 => 2WS (3 CPU cycles) */
                latency = LL_FLASH_LATENCY_2;
            }
            else if (expectedSysClk <= UTILS_SCALE3_LATENCY3_FREQ)
            {
                /* 37.5 < HCLK <= 50 => 3WS (4 CPU cycles) */
                latency = LL_FLASH_LATENCY_3;
            }
            else
            {
                retState = RCC_REQUEST_ERROR;
            }
        /* else expectedSysClk <= 10MHz default LL_FLASH_LATENCY_0 0WS */
        }
        else
        {
            if (expectedSysClk <= UTILS_SCALE4_LATENCY0_FREQ)
            {
                /* 0 < HCLK <= 8 => 0WS (1 CPU cycles) : Do nothing, keep latency to default  LL_FLASH_LATENCY_0 */
            }
            else if (expectedSysClk <= UTILS_SCALE4_LATENCY1_FREQ)
            {
                /* 8 < HCLK <= 16 => 1WS (2 CPU cycles) */
                latency = LL_FLASH_LATENCY_1;
            }
            else if (expectedSysClk <= UTILS_SCALE4_LATENCY2_FREQ)
            {
                /* 16 < HCLK <= 24 => 2WS (3 CPU cycles) */
                latency = LL_FLASH_LATENCY_2;
            }
            else
            {
                retState = RCC_REQUEST_ERROR;
            }
        /* else expectedSysClk <= 10MHz default LL_FLASH_LATENCY_0 0WS */
        }

        if ( RCC_REQUEST_OK == retState )
        {
            LL_FLASH_SetLatency(latency);

            /* Check that the new number of wait states is taken into account to access the Flash
            memory by reading the FLASH_ACR register */
            timeout = 2;
            do
            {
            /* Wait for Flash latency to be updated */
            getlatency = LL_FLASH_GetLatency();
            timeout--;
            } while ((getlatency != latency) && (timeout > 0U));

            if (getlatency != latency)
            {
                retState = RCC_REQUEST_ERROR;
            }
            else
            {
                retState = RCC_REQUEST_OK;
            }
        }
    }
//    /* Division by zero protection */
//    if( 0u != RCC_FLASH_WAIT_STATE_THRESHOLD_HZ )
//    {
//        uint32_t waitCycles = ( ( expectedSysClk - 1u ) / RCC_FLASH_WAIT_STATE_THRESHOLD_HZ );
//
//        LL_FLASH_SetLatency( waitCycles );
//
//        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
//        {
//            regValue = LL_FLASH_GetLatency();
//
//            if( waitCycles == regValue )
//            {
//                retState = RCC_REQUEST_OK;
//                break;
//            }
//            else
//            {
//                /* Clock source has not yet been changed, keep return state as error */
//                retState = RCC_REQUEST_ERROR;
//            }
//        }
//    }
//    else
//    {
//        retState = RCC_REQUEST_ERROR;
//    }

    return ( retState );
}


/**
 * \brief Function used to flash prefetch buffer activation.
 *
 * Flash prefetch buffer is used to increase performance of FLASH memory access.
 * It is recommended to activate this feature if SYSCLK is higher than 80MHz.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchActive( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    Rcc_Set_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk );

    for (uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt++)
    {
        if( 0u != Rcc_Get_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk ) )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Prefetch buffer has not yet been activated, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return (retState);
}


/**
 * \brief Function used to flash prefetch buffer de-activation.
 *
 * Flash prefetch buffer is used to increase performance of FLASH memory access.
 * It is recommended to activate this feature if SYSCLK is higher than 80MHz.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchInactive( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    Rcc_Reset_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk );

    for (uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt++)
    {
        if( 0u == Rcc_Get_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk ) )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Prefetch buffer has not yet been activated, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return (retState);
}


/**
 * \brief Configures the interval between SysTick's in ms [0.001s]
 *
 * \param sysTickInterval [in]: Interval value between ticks in ms [0.001s]
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_SysTickInterval( rcc_Time_ms_t sysTickInterval )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( 0u != sysTickInterval )
    {
        rcc_FreqHz_t systemFreq = 0u;

        uint32_t ticksCnt = 1000u / sysTickInterval;

        retState = Rcc_Get_SysClk( &systemFreq );

        SysTick_Config( systemFreq / ( 8u * ticksCnt ) );

//        LL_InitTick( systemFreq, ticksCnt / 8u );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns the interval between SysTick's in ms [0.001s]
 *
 * \param sysTickInterval [out]: Pointer to value between ticks in ms [0.001s]
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SysTickInterval( rcc_Time_ms_t * const sysTickInterval )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != sysTickInterval )
    {
        rcc_FreqHz_t systemFreq = 0u;

        retState = Rcc_Get_SysClk( &systemFreq );

        uint32_t reloadVal = SysTick->LOAD;

        reloadVal += 1u;

        if( 0u != reloadVal )
        {
            reloadVal = systemFreq / reloadVal;
        }
        else
        {
            reloadVal = 0u;
        }

        *sysTickInterval = reloadVal;

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*----------------------- Clock outputs configuration ------------------------*/

/**
 * \brief Function used to set MCO clock source.
 *
 * \param clkSource [in]: MCO clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_McoClockSource( rcc_Mco_ClkSource_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCOSEL_Msk, ( uint32_t )clkSource );

    if( RCC_MCO_CLOCK_SOURCE_NONE != clkSource )
    {
        gpio_Config_t gpioConfig;

        gpioConfig.PortId           = GPIO_PORT_A;
        gpioConfig.PinId            = GPIO_PIN_ID_8;
        gpioConfig.PinMode          = GPIO_PIN_MODE_ALTERNATE;;
        gpioConfig.PinPull          = GPIO_PIN_PULL_NONE;
        gpioConfig.PinSpeed         = GPIO_PIN_SPEED_VERY_HIGH;
        gpioConfig.PinOutType       = GPIO_PIN_OUTPUT_PUSHPULL;
        gpioConfig.PinAltFunction   = GPIO_ALT_FUNC_0;
        gpioConfig.PinActiveLevel   = GPIO_PIN_LEVEL_HIGH;

        Gpio_Init( &gpioConfig );
    }

    return ( retState );
}


/**
 * \brief Function used to get MCO clock source.
 *
 * \param clkSource [out]: Pointer to MCO clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_McoClockSource( rcc_Mco_ClkSource_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    *clkSource = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCOSEL_Msk );

    return ( retState );
}


/**
 * \brief Function used to set MCO clock divider.
 *
 * \param clkDivider [in]: MCO clock divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_McoClockDivider( rcc_Mco_Divider_t clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCOPRE_Msk, ( uint32_t )clkDivider );

    return ( retState );
}


/**
 * \brief Function used to get MCO clock divider.
 *
 * \param clkDivider [out]: Pointer to MCO clock divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_McoClockDivider( rcc_Mco_Divider_t * const clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    *clkDivider = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCOPRE_Msk );

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Function used to wrap PLL1 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll1_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutR( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL1 clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll1_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutQ( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL1 clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll1_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutP( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL2 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll2_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutR( RCC_PLL_2, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL2 clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll2_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutQ( RCC_PLL_2, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL2 clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll2_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutP(RCC_PLL_2, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL3 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll3_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutR( RCC_PLL_3, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL3 clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll3_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutQ(RCC_PLL_3, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL3 clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll3_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_PllClk_OutP(RCC_PLL_3, clkFreq ) );
}


/**
 * \brief Reading Multi-Speed Low power RC internal oscillator output K frequency
 *
 * \param msiClk [out]: Frequency of MSIS clock output K in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_MsisClk_K( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_MsisClk( RCC_MSIS_OUT_K, clkFreq ) );
}



/**
 * \brief Reading Multi-Speed Low power RC internal oscillator output S frequency
 *
 * \param msiClk [out]: Frequency of MSIS clock output S in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_MsisClk_S( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Get_MsisClk( RCC_MSIS_OUT_S, clkFreq ) );
}


/**
 * \brief Function used for system clock calculation from configuration setting
 *
 * This Function don't use register as reference for calculation of system
 * frequency. Instead of that, use configuration from configuration structure.
 *
 * \param clockConfig [in]: Configuration structure
 * \param sysClk [out]: Pointer to bus clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t *sysClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllSrcFreq = 0u;

    if( ( RCC_NULL_PTR != clockConfig ) &&
        ( RCC_NULL_PTR != sysClk      )    )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource )
        {
            if( RCC_PLL_SRC_HSI16 == clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source )
            {
                pllSrcFreq = HSI_VALUE;
                retState = RCC_REQUEST_OK;
            }
            else if( RCC_PLL_SRC_HSE == clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source )
            {
                pllSrcFreq = clockConfig->HSE_Frequency_Hz;
                retState = RCC_REQUEST_OK;
            }
            else
            {
                /* Invalid option */
                retState = RCC_REQUEST_ERROR;
            }

            if( RCC_REQUEST_ERROR != retState )
            {
                 *sysClk = __LL_RCC_CALC_PLL1CLK_FREQ( pllSrcFreq,
                                                       clockConfig->Pll_Config[ RCC_PLL_1 ].M_Divider,
                                                       clockConfig->Pll_Config[ RCC_PLL_1 ].N_Multiplier,
                                                       clockConfig->Pll_Config[ RCC_PLL_1 ].R_Divider );
            }
            else
            {
                /* Reached error state */
            }

        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == clockConfig->SystemClockSource )
        {
            *sysClk = HSI_VALUE;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == clockConfig->SystemClockSource )
        {
            *sysClk = clockConfig->HSE_Frequency_Hz;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {

    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
