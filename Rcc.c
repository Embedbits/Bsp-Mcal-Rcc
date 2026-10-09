/**
 * \author Mr.Nobody
 * \file Rcc.c
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module common functionality
 *
 * \note  Exception of MCAL layering rule: PWR (voltage scaling, EPOD booster,
 *        backup domain access, UCPD dead battery), FLASH (latency, prefetch)
 *        and ICACHE have no MCAL module. Their configuration is part of the
 *        clock configuration sequence, therefore RCC accesses their LL
 *        functions directly (\ref Rcc_Init, \ref Rcc_Set_PwrRange,
 *        \ref Rcc_Set_FlashLatency, \ref Rcc_Set_FlashPrefetchActive,
 *        \ref Rcc_Set_PeriphActive / \ref Rcc_Set_PwrSupplyActive - independent
 *        supplies). New accesses shall be moved into a dedicated MCAL module once
 *        it exists. The clock recovery system (CRS) serves only the HSI48
 *        oscillator and is therefore a part of the RCC module
 *        (\ref Rcc_Set_Hsi48TrimActive).
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc.h"                            /* Self include                   */
#include "Rcc_Pll.h"                        /* PLL config. functionality      */
#include "Rcc_ClkBus.h"                     /* Clock Buses functionality      */
#include "Rcc_ClkMux.h"                     /* Clock MUX functionality        */
#include "Rcc_ClkSrc.h"                     /* Clock source handler include   */
#include "Rcc_ClkOut.h"                     /* Clock output handler include   */
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Stm32_rcc.h"                      /* RCC RAL functionality          */
#include "Stm32_bus.h"                      /* CLK Buses RAL functionality    */
#include "Stm32_utils.h"                    /* MCU utilities RAL functionality*/
#include "Stm32_icache.h"                   /* Instruction cache functionality*/
#include "Stm32_pwr.h"                      /* PWR RAL functionality          */
#include "Stm32_crs.h"                      /* Clock recovery system RAL      */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define RCC_MAJOR_VERSION                       ( 1u )
/** Value of minor version of SW module */
#define RCC_MINOR_VERSION                       ( 0u )
/** Value of patch version of SW module */
#define RCC_PATCH_VERSION                       ( 0u )

/** Maximal wait time for configuration request confirmation */
#define RCC_TIMEOUT_RAW                         ( 0x84FCB )

/** Target frequency of the HSI48 oscillator in Hz (clock recovery system) */
#define RCC_HSI48_TARGET_HZ                     ( 48000000u )

/** Frequency of the USB start of frame synchronization signal in Hz */
#define RCC_HSI48_SYNC_USB_SOF_HZ               ( 1000u )

/** Frequency of the LSE synchronization signal in Hz */
#define RCC_HSI48_SYNC_LSE_HZ                   ( 32768u )

/** Mask value marking peripheral block without enable / sleep / reset control */
#define RCC_UNSUPPORTED_FUNCTION                ( 0xFF )

/** Mask of all reset source flags in RCC control / status register (CSR) */
#define RCC_CSR_RESET_SRC_MASK                  ( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_SFTRSTF | \
                                                  RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF | \
                                                  RCC_CSR_OBLRSTF )

/** Value of CSR register bits in cleared state */
#define RCC_CSR_BITS_CLEARED                    ( 0u )

/** Value of flag function result when the flag is cleared */
#define RCC_FLAG_CLEARED                        ( 0u )

/** Default HSE frequency used by \ref Rcc_Get_DefaultConfig (HSE not used by default) */
#define RCC_DEFAULT_HSE_FREQ_HZ                 ( 16000000u )

/** Default PLL1 input divider - MSIS 4 MHz (reset range) / 1 = 4 MHz PLL reference */
#define RCC_DEFAULT_PLL_M_DIV                   ( 1u )

/** Default PLL1 multiplier - VCO 320 MHz */
#define RCC_DEFAULT_PLL_N_MULT                  ( 80u )

/** Default PLL1 output dividers (P, Q, R) - 160 MHz */
#define RCC_DEFAULT_PLL_OUT_DIV                 ( 2u )

/** Default SysTick interval in ms */
#define RCC_DEFAULT_SYSTICK_INTERVAL_MS         ( 1u )

/** Default clock output divider (not divided) */
#define RCC_DEFAULT_CLK_OUT_DIV                 ( 1u )

/** System clock after reset - MSIS 4 MHz, updated by \ref Rcc_Init */
#define RCC_SYSCLK_RESET_FREQ_HZ                ( 4000000u )

/** Count of milliseconds in one second */
#define RCC_MS_IN_SECOND                        ( 1000u )

/** Minimum SysTick ticks count per interval (reload register value 1) */
#define RCC_SYSTICK_TICKS_MIN                   ( 2u )

/** Maximum SysTick ticks count per interval (24-bit reload register + 1) */
#define RCC_SYSTICK_TICKS_MAX                   ( SysTick_LOAD_RELOAD_Msk + 1u )

/** SysTick reload register holds ticks count decremented by 1 */
#define RCC_SYSTICK_RELOAD_OFFSET               ( 1u )

/** Count of flash latency thresholds per voltage range */
#define RCC_FLASH_LATENCY_THR_CNT               ( 5u )

/** Count of voltage ranges */
#define RCC_PWR_VOLTAGE_SCALE_CNT               ( 4u )

/** Count of peripheral blocks with independent supply */
#define RCC_SUPPLY_LIST_CNT                     ( sizeof( rcc_SupplyLut ) / sizeof( rcc_SupplyLut[ 0u ] ) )

/** Divider of oscillators without divider */
#define RCC_OSC_DIV_NONE                        ( 1u )

/* ============================== TYPEDEFS ================================== */

/**
 * \brief Peripheral block configuration structure
 */
typedef struct __attribute__((packed))
{
    rcc_BlockList_t BlockId;     /**< Peripheral block ID                    */
    rcc_ClkBusId_t  ClkBusId;    /**< Clock bus ID.                          */
    uint32_t const  StateMask;   /**< Peripheral activation state bit mask.  */
    uint32_t const  LpCtrlMask;  /**< Peripheral Low Power control bit mask. */
    uint32_t const  RstCtrlMask; /**< Peripheral Reset control bit mask.     */
}   rcc_BlockConfigStruct_t;


/**
 * \brief Peripheral (clock source) configuration structure
 */
typedef struct __attribute__((packed))
{
    rcc_PeriphId_t  PeriphId;    /**< Peripheral ID                         */
    rcc_ClkSrcId_t  ClkSrcId;    /**< Clock source ID.                      */
    rcc_BlockList_t BlockId;     /**< Peripheral block ID                   */
    rcc_ClkMuxId_t  ClkMuxId;    /**< Peripheral ID with clock multiplexer. */
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


/** \brief Oscillator control functions */
typedef struct
{
    rcc_RequestState_t ( *SetActive   )( void );                                /**< Oscillator activation               */
    rcc_RequestState_t ( *SetInactive )( void );                                /**< Oscillator de-activation            */
    rcc_RequestState_t ( *GetState    )( rcc_FunctionState_t * const retState ); /**< Oscillator state                    */
    rcc_RequestState_t ( *SetDiv      )( rcc_OscDiv_t oscDiv );                 /**< Divider setter (NULL - no divider)  */
    rcc_RequestState_t ( *GetDiv      )( rcc_OscDiv_t * const oscDiv );         /**< Divider getter (NULL - no divider)  */
}   rcc_OscCtrl_t;


/** \brief Independent supply of peripheral block validated in PWR (SVMCR) */
typedef struct
{
    rcc_BlockList_t BlockId;                     /**< Peripheral block                          */
    void     ( *EnableFunc    )( void );         /**< LL function setting supply valid bit      */
    uint32_t ( *IsEnabledFunc )( void );         /**< LL function reading supply valid bit      */
}   rcc_SupplyConfig_t;


/** \brief Independent supply validated by software (\ref Rcc_Set_PwrSupplyActive) */
typedef struct
{
    void     ( *EnableFunc    )( void );         /**< LL function setting supply valid bit      */
    void     ( *DisableFunc   )( void );         /**< LL function clearing supply valid bit     */
    uint32_t ( *IsEnabledFunc )( void );         /**< LL function reading supply valid bit      */
}   rcc_PwrSupplyConfig_t;


/** \brief Synchronization source of the HSI48 automatic trimming (clock recovery system) */
typedef struct
{
    uint32_t LlSource;  /**< Synchronization source selection (LL_CRS_SYNC_SOURCE_x) */
    uint32_t SyncFreq;  /**< Frequency of the synchronization signal in Hz           */
}   rcc_Hsi48TrimConfigStruct_t;


/** \brief Flash latency thresholds of one voltage range */
typedef struct
{
    uint32_t     VoltageScale;                                /**< PWR voltage range (LL value)                       */
    uint32_t     ThresholdCnt;                                /**< Count of valid thresholds                          */
    rcc_FreqHz_t MaxFreqHz[ RCC_FLASH_LATENCY_THR_CNT ];      /**< Maximal HCLK for 0, 1, 2 ... wait states           */
}   rcc_FlashLatencyConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t  Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const sysClk );
static rcc_RequestState_t  Rcc_Get_FlashLatencyFreq       ( rcc_FreqHz_t hclkFreq, rcc_FlashLatency_t minLatency, rcc_FlashLatency_t * const latency );
static rcc_RequestState_t  Rcc_Set_FlashLatencyValue      ( rcc_FlashLatency_t latency );
static rcc_RequestState_t  Rcc_Set_FlashLatencyRaise      ( rcc_FreqHz_t hclkFreq );
static rcc_RequestState_t  Rcc_Set_FlashLatencyExact      ( rcc_FlashLatency_t minLatency );
static rcc_RequestState_t  Rcc_Set_ClkSrcOscActive        ( rcc_ClkSrcId_t clkSrcId );
static rcc_FunctionState_t Rcc_Get_ClkMuxShared           ( rcc_PeriphId_t periphId );
static rcc_RequestState_t  Rcc_Set_SysClkOscActive        ( rcc_SystemClkSrc_t systemClkSource );
static rcc_RequestState_t  Rcc_Set_SupplyValid            ( rcc_BlockList_t blockId );

static rcc_RequestState_t Rcc_Pll_Get_1_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_1_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_1_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_2_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_2_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_2_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_3_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_3_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_3_PClk( rcc_FreqHz_t * const clkFreq );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* --------------------- Multipliers/Dividers arrays -------------------------*/

/* CMSIS / LL variables (system_stm32u5xx.h) - the module does not use
 * system_stm32u5xx.c, the variables are defined here. */
uint32_t       SystemCoreClock     = RCC_SYSCLK_RESET_FREQ_HZ;
const uint8_t  AHBPrescTable[16u]  = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t  APBPrescTable[8u]   = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};
const uint32_t MSIRangeTable[16u]  = {48000000U, 24000000U, 16000000U, 12000000U, 4000000U, 2000000U, 1330000U, 1000000U,
                                       3072000U,  1536000U,  1024000U,   768000U,  400000U,  200000U,  133000U,  100000U};

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Configuration array of registers used by peripheral buses */
const rcc_ClkBusConfigStruct_t         rcc_ClkBusConfigStruct[] =
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

_Static_assert( (sizeof(rcc_ClkBusConfigStruct) / sizeof(rcc_ClkBusConfigStruct_t)) == RCC_CLK_BUS_CNT, "Rcc: rcc_ClkBusConfigStruct has incorrect size." );


const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK      , .ClkSrcCallback = Rcc_ClkBus_Get_SysClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1RCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_RClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1QCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_QClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1PCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_PClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2RCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_RClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2QCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_QClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2PCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_PClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3RCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_RClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3QCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_QClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3PCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_PClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK      , .ClkSrcCallback = Rcc_ClkBus_Get_AHBClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_HCLKDIV8CLK , .ClkSrcCallback = Rcc_ClkBus_Get_HclkDiv8Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB1Clk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB2Clk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB3CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB3Clk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1TIMCLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB1TimClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2TIMCLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB2TimClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI16CLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi16Clk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_MSISCLK     , .ClkSrcCallback = Rcc_ClkSrc_Get_MsisClk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_MSIKCLK     , .ClkSrcCallback = Rcc_ClkSrc_Get_MsikClk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48CLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi48Clk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48DIV2CLK, .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi48Div2Clk },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_HseClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSEDIV32CLK , .ClkSrcCallback = Rcc_ClkSrc_Get_HseDiv32Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_LsiClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_LseClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PINCLK      , .ClkSrcCallback = RCC_NULL_PTR                },
};

_Static_assert( (sizeof(rcc_PeriphClkSrcConfig) / sizeof(rcc_ClkSrcConfigStruct_t)) == RCC_CLK_SRC_CNT, "Rcc: rcc_PeriphClkSrcConfig has incorrect size." );


/** \brief Configuration array of MCU peripherals, indexed by \ref rcc_PeriphId_t */
const rcc_PeriphConfigStruct_t          rcc_ConfigStruct[] =
{
  /*------------------------------ System core -------------------------------*/

  { .PeriphId = RCC_PERIPH_FLASH              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_FLASH       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_SYSCFG             , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_SYSCFG      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_PWR                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_PWR         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_SYSTICK_HCLK_DIV8  , .ClkSrcId = RCC_CLK_SRC_HCLKDIV8CLK   , .BlockId = RCC_BLOCK_SYSTICK     , .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8      },
  { .PeriphId = RCC_PERIPH_SYSTICK_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_SYSTICK     , .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI            },
  { .PeriphId = RCC_PERIPH_SYSTICK_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_SYSTICK     , .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE            },
  { .PeriphId = RCC_PERIPH_IWDG               , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_IWDG        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_RTC_LSE            , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_RTC         , .ClkMuxId = RCC_CLK_MUX_RTC_LSE                },
  { .PeriphId = RCC_PERIPH_RTC_LSI            , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_RTC         , .ClkMuxId = RCC_CLK_MUX_RTC_LSI                },
  { .PeriphId = RCC_PERIPH_RTC_HSE_DIV32      , .ClkSrcId = RCC_CLK_SRC_HSEDIV32CLK   , .BlockId = RCC_BLOCK_RTC         , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32          },
  { .PeriphId = RCC_PERIPH_CRS                , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_CRS         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_WWDG               , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_WWDG        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_RAMCFG             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_RAMCFG      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_BKPSRAM            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_BKPSRAM     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_SRAM1              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_SRAM2              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_AHB2ENR1_SRAM3EN)
  { .PeriphId = RCC_PERIPH_SRAM3              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM3       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* SRAM3 */
  { .PeriphId = RCC_PERIPH_SRAM4              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM4       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_AHB2ENR2_SRAM5EN)
  { .PeriphId = RCC_PERIPH_SRAM5              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM5       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* SRAM5 */
#if defined(RCC_AHB2ENR2_SRAM6EN)
  { .PeriphId = RCC_PERIPH_SRAM6              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM6       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* SRAM6 */
  { .PeriphId = RCC_PERIPH_DCACHE1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DCACHE1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(DCACHE2)
  { .PeriphId = RCC_PERIPH_DCACHE2            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DCACHE2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* DCACHE2 */
  { .PeriphId = RCC_PERIPH_GTZC1              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GTZC1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GTZC2              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GTZC2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPDMA1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPDMA1      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_LPDMA1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_LPDMA1      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOA              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOA       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOB              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOB       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOC       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOD              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOD       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOE              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOE       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(GPIOF)
  { .PeriphId = RCC_PERIPH_GPIOF              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOF       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* GPIOF */
  { .PeriphId = RCC_PERIPH_GPIOG              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOG       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOH              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOH       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(GPIOI)
  { .PeriphId = RCC_PERIPH_GPIOI              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOI       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* GPIOI */
#if defined(GPIOJ)
  { .PeriphId = RCC_PERIPH_GPIOJ              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOJ       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* GPIOJ */
  { .PeriphId = RCC_PERIPH_LPGPIO1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_LPGPIO1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  /*--------------------------------- Timers ---------------------------------*/

  { .PeriphId = RCC_PERIPH_TIM1               , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM1        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM2               , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM2        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM3               , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM3        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM4               , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM4        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM5               , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM5        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM6               , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM6        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM7               , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM7        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM8               , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM8        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM15              , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM15       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM16              , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM16       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM17              , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM17       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_LPTIM1_MSIK        , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK            },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSI         , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI             },
  { .PeriphId = RCC_PERIPH_LPTIM1_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI             },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE             },
  { .PeriphId = RCC_PERIPH_LPTIM2_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1           },
  { .PeriphId = RCC_PERIPH_LPTIM2_LSI         , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI             },
  { .PeriphId = RCC_PERIPH_LPTIM2_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxId = RCC_CLK_MUX_LPTIM2_HSI             },
  { .PeriphId = RCC_PERIPH_LPTIM2_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE             },
  { .PeriphId = RCC_PERIPH_LPTIM3_MSIK        , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK           },
  { .PeriphId = RCC_PERIPH_LPTIM3_LSI         , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSI            },
  { .PeriphId = RCC_PERIPH_LPTIM3_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_HSI            },
  { .PeriphId = RCC_PERIPH_LPTIM3_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSE            },
  { .PeriphId = RCC_PERIPH_LPTIM4_MSIK        , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_LPTIM4      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK           },
  { .PeriphId = RCC_PERIPH_LPTIM4_LSI         , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM4      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSI            },
  { .PeriphId = RCC_PERIPH_LPTIM4_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_LPTIM4      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_HSI            },
  { .PeriphId = RCC_PERIPH_LPTIM4_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM4      , .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSE            },

  /*------------------------------ Connectivity ------------------------------*/

  { .PeriphId = RCC_PERIPH_SPI1_PCLK2         , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxId = RCC_CLK_MUX_SPI1_PCLK2             },
  { .PeriphId = RCC_PERIPH_SPI1_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxId = RCC_CLK_MUX_SPI1_SYSCLK            },
  { .PeriphId = RCC_PERIPH_SPI1_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxId = RCC_CLK_MUX_SPI1_HSI               },
  { .PeriphId = RCC_PERIPH_SPI1_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxId = RCC_CLK_MUX_SPI1_MSIK              },
  { .PeriphId = RCC_PERIPH_SPI2_PCLK1         , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxId = RCC_CLK_MUX_SPI2_PCLK1             },
  { .PeriphId = RCC_PERIPH_SPI2_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxId = RCC_CLK_MUX_SPI2_SYSCLK            },
  { .PeriphId = RCC_PERIPH_SPI2_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxId = RCC_CLK_MUX_SPI2_HSI               },
  { .PeriphId = RCC_PERIPH_SPI2_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxId = RCC_CLK_MUX_SPI2_MSIK              },
  { .PeriphId = RCC_PERIPH_SPI3_PCLK3         , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxId = RCC_CLK_MUX_SPI3_PCLK3             },
  { .PeriphId = RCC_PERIPH_SPI3_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxId = RCC_CLK_MUX_SPI3_SYSCLK            },
  { .PeriphId = RCC_PERIPH_SPI3_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxId = RCC_CLK_MUX_SPI3_HSI               },
  { .PeriphId = RCC_PERIPH_SPI3_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxId = RCC_CLK_MUX_SPI3_MSIK              },
  { .PeriphId = RCC_PERIPH_I2C1_PCLK1         , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1             },
  { .PeriphId = RCC_PERIPH_I2C1_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK            },
  { .PeriphId = RCC_PERIPH_I2C1_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxId = RCC_CLK_MUX_I2C1_HSI               },
  { .PeriphId = RCC_PERIPH_I2C1_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxId = RCC_CLK_MUX_I2C1_MSIK              },
  { .PeriphId = RCC_PERIPH_I2C2_PCLK1         , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1             },
  { .PeriphId = RCC_PERIPH_I2C2_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK            },
  { .PeriphId = RCC_PERIPH_I2C2_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxId = RCC_CLK_MUX_I2C2_HSI               },
  { .PeriphId = RCC_PERIPH_I2C2_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxId = RCC_CLK_MUX_I2C2_MSIK              },
  { .PeriphId = RCC_PERIPH_I2C3_PCLK3         , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3             },
  { .PeriphId = RCC_PERIPH_I2C3_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK            },
  { .PeriphId = RCC_PERIPH_I2C3_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxId = RCC_CLK_MUX_I2C3_HSI               },
  { .PeriphId = RCC_PERIPH_I2C3_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxId = RCC_CLK_MUX_I2C3_MSIK              },
  { .PeriphId = RCC_PERIPH_I2C4_PCLK1         , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1             },
  { .PeriphId = RCC_PERIPH_I2C4_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK            },
  { .PeriphId = RCC_PERIPH_I2C4_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxId = RCC_CLK_MUX_I2C4_HSI               },
  { .PeriphId = RCC_PERIPH_I2C4_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxId = RCC_CLK_MUX_I2C4_MSIK              },
#if defined(I2C5)
  { .PeriphId = RCC_PERIPH_I2C5_PCLK1         , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxId = RCC_CLK_MUX_I2C5_PCLK1             },
  { .PeriphId = RCC_PERIPH_I2C5_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxId = RCC_CLK_MUX_I2C5_SYSCLK            },
  { .PeriphId = RCC_PERIPH_I2C5_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxId = RCC_CLK_MUX_I2C5_HSI               },
  { .PeriphId = RCC_PERIPH_I2C5_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxId = RCC_CLK_MUX_I2C5_MSIK              },
#endif /* I2C5 */
#if defined(I2C6)
  { .PeriphId = RCC_PERIPH_I2C6_PCLK1         , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxId = RCC_CLK_MUX_I2C6_PCLK1             },
  { .PeriphId = RCC_PERIPH_I2C6_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxId = RCC_CLK_MUX_I2C6_SYSCLK            },
  { .PeriphId = RCC_PERIPH_I2C6_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxId = RCC_CLK_MUX_I2C6_HSI               },
  { .PeriphId = RCC_PERIPH_I2C6_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxId = RCC_CLK_MUX_I2C6_MSIK              },
#endif /* I2C6 */
  { .PeriphId = RCC_PERIPH_USART1_PCLK2       , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_USART1      , .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2           },
  { .PeriphId = RCC_PERIPH_USART1_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_USART1      , .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK          },
  { .PeriphId = RCC_PERIPH_USART1_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_USART1      , .ClkMuxId = RCC_CLK_MUX_USART1_HSI             },
  { .PeriphId = RCC_PERIPH_USART1_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART1      , .ClkMuxId = RCC_CLK_MUX_USART1_LSE             },
#if defined(USART2)
  { .PeriphId = RCC_PERIPH_USART2_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_USART2      , .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1           },
  { .PeriphId = RCC_PERIPH_USART2_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_USART2      , .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK          },
  { .PeriphId = RCC_PERIPH_USART2_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_USART2      , .ClkMuxId = RCC_CLK_MUX_USART2_HSI             },
  { .PeriphId = RCC_PERIPH_USART2_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART2      , .ClkMuxId = RCC_CLK_MUX_USART2_LSE             },
#endif /* USART2 */
  { .PeriphId = RCC_PERIPH_USART3_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_USART3      , .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1           },
  { .PeriphId = RCC_PERIPH_USART3_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_USART3      , .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK          },
  { .PeriphId = RCC_PERIPH_USART3_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_USART3      , .ClkMuxId = RCC_CLK_MUX_USART3_HSI             },
  { .PeriphId = RCC_PERIPH_USART3_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART3      , .ClkMuxId = RCC_CLK_MUX_USART3_LSE             },
  { .PeriphId = RCC_PERIPH_UART4_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UART4       , .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1            },
  { .PeriphId = RCC_PERIPH_UART4_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_UART4       , .ClkMuxId = RCC_CLK_MUX_UART4_SYSCLK           },
  { .PeriphId = RCC_PERIPH_UART4_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_UART4       , .ClkMuxId = RCC_CLK_MUX_UART4_HSI              },
  { .PeriphId = RCC_PERIPH_UART4_LSE          , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART4       , .ClkMuxId = RCC_CLK_MUX_UART4_LSE              },
  { .PeriphId = RCC_PERIPH_UART5_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UART5       , .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1            },
  { .PeriphId = RCC_PERIPH_UART5_SYSCLK       , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_UART5       , .ClkMuxId = RCC_CLK_MUX_UART5_SYSCLK           },
  { .PeriphId = RCC_PERIPH_UART5_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_UART5       , .ClkMuxId = RCC_CLK_MUX_UART5_HSI              },
  { .PeriphId = RCC_PERIPH_UART5_LSE          , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART5       , .ClkMuxId = RCC_CLK_MUX_UART5_LSE              },
#if defined(USART6)
  { .PeriphId = RCC_PERIPH_USART6_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_USART6      , .ClkMuxId = RCC_CLK_MUX_USART6_PCLK1           },
  { .PeriphId = RCC_PERIPH_USART6_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_USART6      , .ClkMuxId = RCC_CLK_MUX_USART6_SYSCLK          },
  { .PeriphId = RCC_PERIPH_USART6_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_USART6      , .ClkMuxId = RCC_CLK_MUX_USART6_HSI             },
  { .PeriphId = RCC_PERIPH_USART6_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART6      , .ClkMuxId = RCC_CLK_MUX_USART6_LSE             },
#endif /* USART6 */
  { .PeriphId = RCC_PERIPH_LPUART1_PCLK3      , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3          },
  { .PeriphId = RCC_PERIPH_LPUART1_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK         },
  { .PeriphId = RCC_PERIPH_LPUART1_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI            },
  { .PeriphId = RCC_PERIPH_LPUART1_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE            },
  { .PeriphId = RCC_PERIPH_LPUART1_MSIK       , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxId = RCC_CLK_MUX_LPUART1_MSIK           },
  { .PeriphId = RCC_PERIPH_FDCAN1_HSE         , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_FDCAN1      , .ClkMuxId = RCC_CLK_MUX_FDCAN1_HSE             },
  { .PeriphId = RCC_PERIPH_FDCAN1_PLL1Q       , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_FDCAN1      , .ClkMuxId = RCC_CLK_MUX_FDCAN1_PLL1Q           },
  { .PeriphId = RCC_PERIPH_FDCAN1_PLL2P       , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_FDCAN1      , .ClkMuxId = RCC_CLK_MUX_FDCAN1_PLL2P           },
#if defined(RCC_APB2ENR_USBEN)
  { .PeriphId = RCC_PERIPH_USB_HSI48          , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_USB         , .ClkMuxId = RCC_CLK_MUX_USB_HSI48              },
  { .PeriphId = RCC_PERIPH_USB_PLL2Q          , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USB         , .ClkMuxId = RCC_CLK_MUX_USB_PLL2Q              },
  { .PeriphId = RCC_PERIPH_USB_PLL1Q          , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_USB         , .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q              },
  { .PeriphId = RCC_PERIPH_USB_MSIK           , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_USB         , .ClkMuxId = RCC_CLK_MUX_USB_MSIK               },
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
  { .PeriphId = RCC_PERIPH_USB_HSI48          , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_OTG         , .ClkMuxId = RCC_CLK_MUX_USB_HSI48              },
  { .PeriphId = RCC_PERIPH_USB_PLL2Q          , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_OTG         , .ClkMuxId = RCC_CLK_MUX_USB_PLL2Q              },
  { .PeriphId = RCC_PERIPH_USB_PLL1Q          , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_OTG         , .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q              },
  { .PeriphId = RCC_PERIPH_USB_MSIK           , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_OTG         , .ClkMuxId = RCC_CLK_MUX_USB_MSIK               },
#endif /* OTG */
#if defined(RCC_AHB2ENR1_USBPHYCEN)
  { .PeriphId = RCC_PERIPH_USBPHYC            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_USBPHYC     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* USBPHYC */
#if defined(UCPD1)
  { .PeriphId = RCC_PERIPH_UCPD1              , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UCPD1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* UCPD1 */
  { .PeriphId = RCC_PERIPH_OCTOSPI1_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK         },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_MSIK      , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_MSIK           },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_PLL1Q     , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL1Q          },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_PLL2Q     , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL2Q          },
#if defined(OCTOSPI2)
  { .PeriphId = RCC_PERIPH_OCTOSPI2_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_OCTOSPI2    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK         },
  { .PeriphId = RCC_PERIPH_OCTOSPI2_MSIK      , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_OCTOSPI2    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_MSIK           },
  { .PeriphId = RCC_PERIPH_OCTOSPI2_PLL1Q     , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_OCTOSPI2    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL1Q          },
  { .PeriphId = RCC_PERIPH_OCTOSPI2_PLL2Q     , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_OCTOSPI2    , .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL2Q          },
#endif /* OCTOSPI2 */
#if defined(OCTOSPIM)
  { .PeriphId = RCC_PERIPH_OCTOSPIM           , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OCTOSPIM    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* OCTOSPIM */
#if defined(HSPI1)
  { .PeriphId = RCC_PERIPH_HSPI1              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_HSPI1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* HSPI1 */
#if defined(RCC_AHB2ENR2_FSMCEN)
  { .PeriphId = RCC_PERIPH_FMC                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_FMC         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* FMC */
  { .PeriphId = RCC_PERIPH_SDMMC1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SDMMC1      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(SDMMC2)
  { .PeriphId = RCC_PERIPH_SDMMC2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SDMMC2      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* SDMMC2 */

  /*------------------------------- Multimedia -------------------------------*/

  { .PeriphId = RCC_PERIPH_DCMI_PSSI          , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DCMI_PSSI   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_SAI1_PLL2P         , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P             },
  { .PeriphId = RCC_PERIPH_SAI1_PLL3P         , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P             },
  { .PeriphId = RCC_PERIPH_SAI1_PLL1P         , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK      , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1P             },
  { .PeriphId = RCC_PERIPH_SAI1_PIN           , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxId = RCC_CLK_MUX_SAI1_PIN               },
  { .PeriphId = RCC_PERIPH_SAI1_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxId = RCC_CLK_MUX_SAI1_HSI               },
#if defined(SAI2)
  { .PeriphId = RCC_PERIPH_SAI2_PLL2P         , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P             },
  { .PeriphId = RCC_PERIPH_SAI2_PLL3P         , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P             },
  { .PeriphId = RCC_PERIPH_SAI2_PLL1P         , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK      , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1P             },
  { .PeriphId = RCC_PERIPH_SAI2_PIN           , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxId = RCC_CLK_MUX_SAI2_PIN               },
  { .PeriphId = RCC_PERIPH_SAI2_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxId = RCC_CLK_MUX_SAI2_HSI               },
#endif /* SAI2 */
  { .PeriphId = RCC_PERIPH_MDF1_HCLK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxId = RCC_CLK_MUX_MDF1_HCLK              },
  { .PeriphId = RCC_PERIPH_MDF1_PLL1P         , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK      , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxId = RCC_CLK_MUX_MDF1_PLL1P             },
  { .PeriphId = RCC_PERIPH_MDF1_PLL3Q         , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxId = RCC_CLK_MUX_MDF1_PLL3Q             },
  { .PeriphId = RCC_PERIPH_MDF1_PIN           , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxId = RCC_CLK_MUX_MDF1_PIN               },
  { .PeriphId = RCC_PERIPH_MDF1_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxId = RCC_CLK_MUX_MDF1_MSIK              },
  { .PeriphId = RCC_PERIPH_ADF1_HCLK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxId = RCC_CLK_MUX_ADF1_HCLK              },
  { .PeriphId = RCC_PERIPH_ADF1_PLL1P         , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK      , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxId = RCC_CLK_MUX_ADF1_PLL1P             },
  { .PeriphId = RCC_PERIPH_ADF1_PLL3Q         , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxId = RCC_CLK_MUX_ADF1_PLL3Q             },
  { .PeriphId = RCC_PERIPH_ADF1_PIN           , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxId = RCC_CLK_MUX_ADF1_PIN               },
  { .PeriphId = RCC_PERIPH_ADF1_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxId = RCC_CLK_MUX_ADF1_MSIK              },
#if defined(DMA2D)
  { .PeriphId = RCC_PERIPH_DMA2D              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DMA2D       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* DMA2D */
#if defined(GPU2D)
  { .PeriphId = RCC_PERIPH_GPU2D              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPU2D       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* GPU2D */
#if defined(GFXMMU)
  { .PeriphId = RCC_PERIPH_GFXMMU             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GFXMMU      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* GFXMMU */
#if defined(GFXTIM)
  { .PeriphId = RCC_PERIPH_GFXTIM             , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_GFXTIM      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* GFXTIM */
#if defined(JPEG)
  { .PeriphId = RCC_PERIPH_JPEG               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_JPEG        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* JPEG */
#if defined(LTDC)
  { .PeriphId = RCC_PERIPH_LTDC               , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_LTDC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* LTDC */
#if defined(DSI)
  { .PeriphId = RCC_PERIPH_DSIHOST            , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_DSIHOST     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* DSIHOST */
  { .PeriphId = RCC_PERIPH_TSC                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_TSC         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  /*--------------------------------- Analog ---------------------------------*/

  { .PeriphId = RCC_PERIPH_ADC_HCLK           , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK           },
  { .PeriphId = RCC_PERIPH_ADC_SYSCLK         , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK         },
  { .PeriphId = RCC_PERIPH_ADC_PLL2R          , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R          },
  { .PeriphId = RCC_PERIPH_ADC_HSE            , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE            },
  { .PeriphId = RCC_PERIPH_ADC_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI            },
  { .PeriphId = RCC_PERIPH_ADC_MSIK           , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK           },
  { .PeriphId = RCC_PERIPH_ADC4_HCLK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ADC4        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK           },
  { .PeriphId = RCC_PERIPH_ADC4_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_ADC4        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK         },
  { .PeriphId = RCC_PERIPH_ADC4_PLL2R         , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_ADC4        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R          },
  { .PeriphId = RCC_PERIPH_ADC4_HSE           , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_ADC4        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE            },
  { .PeriphId = RCC_PERIPH_ADC4_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_ADC4        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI            },
  { .PeriphId = RCC_PERIPH_ADC4_MSIK          , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_ADC4        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK           },
  { .PeriphId = RCC_PERIPH_DAC_HCLK           , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK           },
  { .PeriphId = RCC_PERIPH_DAC_SYSCLK         , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK         },
  { .PeriphId = RCC_PERIPH_DAC_PLL2R          , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R          },
  { .PeriphId = RCC_PERIPH_DAC_HSE            , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE            },
  { .PeriphId = RCC_PERIPH_DAC_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI            },
  { .PeriphId = RCC_PERIPH_DAC_MSIK           , .ClkSrcId = RCC_CLK_SRC_MSIKCLK       , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK           },
  { .PeriphId = RCC_PERIPH_DAC_SAH_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE            },
  { .PeriphId = RCC_PERIPH_DAC_SAH_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSI            },
  { .PeriphId = RCC_PERIPH_COMP               , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_COMP        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_OPAMP              , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_OPAMP       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_VREF               , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_VREF        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  /*-------------------------------- Security --------------------------------*/

#if defined(AES)
  { .PeriphId = RCC_PERIPH_AES                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_AES         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* AES */
#if defined(SAES)
  { .PeriphId = RCC_PERIPH_SAES               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SAES        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* SAES */
  { .PeriphId = RCC_PERIPH_HASH               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_HASH        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(PKA)
  { .PeriphId = RCC_PERIPH_PKA                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_PKA         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* PKA */
#if defined(OTFDEC1)
  { .PeriphId = RCC_PERIPH_OTFDEC1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OTFDEC1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* OTFDEC1 */
#if defined(OTFDEC2)
  { .PeriphId = RCC_PERIPH_OTFDEC2            , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OTFDEC2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif /* OTFDEC2 */
  { .PeriphId = RCC_PERIPH_RNG_HSI48          , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_RNG         , .ClkMuxId = RCC_CLK_MUX_RNG_HSI48              },
  { .PeriphId = RCC_PERIPH_RNG_HSI48_DIV2     , .ClkSrcId = RCC_CLK_SRC_HSI48DIV2CLK  , .BlockId = RCC_BLOCK_RNG         , .ClkMuxId = RCC_CLK_MUX_RNG_HSI48_DIV2         },
  { .PeriphId = RCC_PERIPH_RNG_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI16CLK      , .BlockId = RCC_BLOCK_RNG         , .ClkMuxId = RCC_CLK_MUX_RNG_HSI                },

  /*------------------------------- Computing --------------------------------*/

  { .PeriphId = RCC_PERIPH_CORDIC             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_CORDIC      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_CRC                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_CRC         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_FMAC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_FMAC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
};

_Static_assert( (sizeof(rcc_ConfigStruct) / sizeof(rcc_PeriphConfigStruct_t)) == RCC_PERIPH_ID_CNT, "Rcc: rcc_ConfigStruct has incorrect size." );


/** \brief Configuration array of peripheral blocks, indexed by \ref rcc_BlockList_t */
const rcc_BlockConfigStruct_t           rcc_PeriphBlockConfig[] =
{
  /*------------------------------ System core -------------------------------*/

  { .BlockId = RCC_BLOCK_FLASH       , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_FLASHEN         , .LpCtrlMask = RCC_AHB1SMENR_FLASHSMEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_SYSCFG      , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SYSCFGEN        , .LpCtrlMask = RCC_APB3SMENR_SYSCFGSMEN      , .RstCtrlMask = RCC_APB3RSTR_SYSCFGRST       },
  { .BlockId = RCC_BLOCK_PWR         , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_PWREN           , .LpCtrlMask = RCC_AHB3SMENR_PWRSMEN         , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_SYSTICK     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_UNSUPPORTED_FUNCTION    , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_IWDG        , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_UNSUPPORTED_FUNCTION    , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_RTC         , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_RTCAPBEN        , .LpCtrlMask = RCC_APB3SMENR_RTCAPBSMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_CRS         , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_CRSEN          , .LpCtrlMask = RCC_APB1SMENR1_CRSSMEN        , .RstCtrlMask = RCC_APB1RSTR1_CRSRST         },
  { .BlockId = RCC_BLOCK_WWDG        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_WWDGEN         , .LpCtrlMask = RCC_APB1SMENR1_WWDGSMEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_RAMCFG      , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_RAMCFGEN        , .LpCtrlMask = RCC_AHB1SMENR_RAMCFGSMEN      , .RstCtrlMask = RCC_AHB1RSTR_RAMCFGRST       },
  { .BlockId = RCC_BLOCK_BKPSRAM     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_BKPSRAMEN       , .LpCtrlMask = RCC_AHB1SMENR_BKPSRAMSMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_SRAM1       , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_SRAM1EN         , .LpCtrlMask = RCC_AHB1SMENR_SRAM1SMEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_SRAM2       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SRAM2EN        , .LpCtrlMask = RCC_AHB2SMENR1_SRAM2SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#if defined(RCC_AHB2ENR1_SRAM3EN)
  { .BlockId = RCC_BLOCK_SRAM3       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SRAM3EN        , .LpCtrlMask = RCC_AHB2SMENR1_SRAM3SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#endif /* SRAM3 */
  { .BlockId = RCC_BLOCK_SRAM4       , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_SRAM4EN         , .LpCtrlMask = RCC_AHB3SMENR_SRAM4SMEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#if defined(RCC_AHB2ENR2_SRAM5EN)
  { .BlockId = RCC_BLOCK_SRAM5       , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_SRAM5EN        , .LpCtrlMask = RCC_AHB2SMENR2_SRAM5SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#endif /* SRAM5 */
#if defined(RCC_AHB2ENR2_SRAM6EN)
  { .BlockId = RCC_BLOCK_SRAM6       , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_SRAM6EN        , .LpCtrlMask = RCC_AHB2SMENR2_SRAM6SMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#endif /* SRAM6 */
  { .BlockId = RCC_BLOCK_DCACHE1     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DCACHE1EN       , .LpCtrlMask = RCC_AHB1SMENR_DCACHE1SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#if defined(DCACHE2)
  { .BlockId = RCC_BLOCK_DCACHE2     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DCACHE2EN       , .LpCtrlMask = RCC_AHB1SMENR_DCACHE2SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#endif /* DCACHE2 */
  { .BlockId = RCC_BLOCK_GTZC1       , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GTZC1EN         , .LpCtrlMask = RCC_AHB1SMENR_GTZC1SMEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_GTZC2       , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_GTZC2EN         , .LpCtrlMask = RCC_AHB3SMENR_GTZC2SMEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
  { .BlockId = RCC_BLOCK_GPDMA1      , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GPDMA1EN        , .LpCtrlMask = RCC_AHB1SMENR_GPDMA1SMEN      , .RstCtrlMask = RCC_AHB1RSTR_GPDMA1RST       },
  { .BlockId = RCC_BLOCK_LPDMA1      , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_LPDMA1EN        , .LpCtrlMask = RCC_AHB3SMENR_LPDMA1SMEN      , .RstCtrlMask = RCC_AHB3RSTR_LPDMA1RST       },
  { .BlockId = RCC_BLOCK_GPIOA       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOAEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOASMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOARST       },
  { .BlockId = RCC_BLOCK_GPIOB       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOBEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOBSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOBRST       },
  { .BlockId = RCC_BLOCK_GPIOC       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOCEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOCSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOCRST       },
  { .BlockId = RCC_BLOCK_GPIOD       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIODEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIODSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIODRST       },
  { .BlockId = RCC_BLOCK_GPIOE       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOEEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOESMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOERST       },
#if defined(GPIOF)
  { .BlockId = RCC_BLOCK_GPIOF       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOFEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOFSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOFRST       },
#endif /* GPIOF */
  { .BlockId = RCC_BLOCK_GPIOG       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOGEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOGSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOGRST       },
  { .BlockId = RCC_BLOCK_GPIOH       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOHEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOHSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOHRST       },
#if defined(GPIOI)
  { .BlockId = RCC_BLOCK_GPIOI       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOIEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOISMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOIRST       },
#endif /* GPIOI */
#if defined(GPIOJ)
  { .BlockId = RCC_BLOCK_GPIOJ       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_GPIOJEN        , .LpCtrlMask = RCC_AHB2SMENR1_GPIOJSMEN      , .RstCtrlMask = RCC_AHB2RSTR1_GPIOJRST       },
#endif /* GPIOJ */
  { .BlockId = RCC_BLOCK_LPGPIO1     , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_LPGPIO1EN       , .LpCtrlMask = RCC_AHB3SMENR_LPGPIO1SMEN     , .RstCtrlMask = RCC_AHB3RSTR_LPGPIO1RST      },

  /*--------------------------------- Timers ---------------------------------*/

  { .BlockId = RCC_BLOCK_TIM1        , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM1EN          , .LpCtrlMask = RCC_APB2SMENR_TIM1SMEN        , .RstCtrlMask = RCC_APB2RSTR_TIM1RST         },
  { .BlockId = RCC_BLOCK_TIM2        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM2EN         , .LpCtrlMask = RCC_APB1SMENR1_TIM2SMEN       , .RstCtrlMask = RCC_APB1RSTR1_TIM2RST        },
  { .BlockId = RCC_BLOCK_TIM3        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM3EN         , .LpCtrlMask = RCC_APB1SMENR1_TIM3SMEN       , .RstCtrlMask = RCC_APB1RSTR1_TIM3RST        },
  { .BlockId = RCC_BLOCK_TIM4        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM4EN         , .LpCtrlMask = RCC_APB1SMENR1_TIM4SMEN       , .RstCtrlMask = RCC_APB1RSTR1_TIM4RST        },
  { .BlockId = RCC_BLOCK_TIM5        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM5EN         , .LpCtrlMask = RCC_APB1SMENR1_TIM5SMEN       , .RstCtrlMask = RCC_APB1RSTR1_TIM5RST        },
  { .BlockId = RCC_BLOCK_TIM6        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM6EN         , .LpCtrlMask = RCC_APB1SMENR1_TIM6SMEN       , .RstCtrlMask = RCC_APB1RSTR1_TIM6RST        },
  { .BlockId = RCC_BLOCK_TIM7        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM7EN         , .LpCtrlMask = RCC_APB1SMENR1_TIM7SMEN       , .RstCtrlMask = RCC_APB1RSTR1_TIM7RST        },
  { .BlockId = RCC_BLOCK_TIM8        , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM8EN          , .LpCtrlMask = RCC_APB2SMENR_TIM8SMEN        , .RstCtrlMask = RCC_APB2RSTR_TIM8RST         },
  { .BlockId = RCC_BLOCK_TIM15       , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM15EN         , .LpCtrlMask = RCC_APB2SMENR_TIM15SMEN       , .RstCtrlMask = RCC_APB2RSTR_TIM15RST        },
  { .BlockId = RCC_BLOCK_TIM16       , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM16EN         , .LpCtrlMask = RCC_APB2SMENR_TIM16SMEN       , .RstCtrlMask = RCC_APB2RSTR_TIM16RST        },
  { .BlockId = RCC_BLOCK_TIM17       , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM17EN         , .LpCtrlMask = RCC_APB2SMENR_TIM17SMEN       , .RstCtrlMask = RCC_APB2RSTR_TIM17RST        },
  { .BlockId = RCC_BLOCK_LPTIM1      , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM1EN        , .LpCtrlMask = RCC_APB3SMENR_LPTIM1SMEN      , .RstCtrlMask = RCC_APB3RSTR_LPTIM1RST       },
  { .BlockId = RCC_BLOCK_LPTIM2      , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_LPTIM2EN       , .LpCtrlMask = RCC_APB1SMENR2_LPTIM2SMEN     , .RstCtrlMask = RCC_APB1RSTR2_LPTIM2RST      },
  { .BlockId = RCC_BLOCK_LPTIM3      , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM3EN        , .LpCtrlMask = RCC_APB3SMENR_LPTIM3SMEN      , .RstCtrlMask = RCC_APB3RSTR_LPTIM3RST       },
  { .BlockId = RCC_BLOCK_LPTIM4      , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPTIM4EN        , .LpCtrlMask = RCC_APB3SMENR_LPTIM4SMEN      , .RstCtrlMask = RCC_APB3RSTR_LPTIM4RST       },

  /*------------------------------ Connectivity ------------------------------*/

  { .BlockId = RCC_BLOCK_SPI1        , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI1EN          , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN        , .RstCtrlMask = RCC_APB2RSTR_SPI1RST         },
  { .BlockId = RCC_BLOCK_SPI2        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI2EN         , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN       , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST        },
  { .BlockId = RCC_BLOCK_SPI3        , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_SPI3EN          , .LpCtrlMask = RCC_APB3SMENR_SPI3SMEN        , .RstCtrlMask = RCC_APB3RSTR_SPI3RST         },
  { .BlockId = RCC_BLOCK_I2C1        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C1EN         , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN       , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST        },
  { .BlockId = RCC_BLOCK_I2C2        , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C2EN         , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN       , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST        },
  { .BlockId = RCC_BLOCK_I2C3        , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_I2C3EN          , .LpCtrlMask = RCC_APB3SMENR_I2C3SMEN        , .RstCtrlMask = RCC_APB3RSTR_I2C3RST         },
  { .BlockId = RCC_BLOCK_I2C4        , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_I2C4EN         , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN       , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST        },
#if defined(I2C5)
  { .BlockId = RCC_BLOCK_I2C5        , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_I2C5EN         , .LpCtrlMask = RCC_APB1SMENR2_I2C5SMEN       , .RstCtrlMask = RCC_APB1RSTR2_I2C5RST        },
#endif /* I2C5 */
#if defined(I2C6)
  { .BlockId = RCC_BLOCK_I2C6        , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_I2C6EN         , .LpCtrlMask = RCC_APB1SMENR2_I2C6SMEN       , .RstCtrlMask = RCC_APB1RSTR2_I2C6RST        },
#endif /* I2C6 */
  { .BlockId = RCC_BLOCK_USART1      , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USART1EN        , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN      , .RstCtrlMask = RCC_APB2RSTR_USART1RST       },
#if defined(USART2)
  { .BlockId = RCC_BLOCK_USART2      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART2EN       , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_USART2RST      },
#endif /* USART2 */
  { .BlockId = RCC_BLOCK_USART3      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART3EN       , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_USART3RST      },
  { .BlockId = RCC_BLOCK_UART4       , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART4EN        , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN      , .RstCtrlMask = RCC_APB1RSTR1_UART4RST       },
  { .BlockId = RCC_BLOCK_UART5       , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART5EN        , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN      , .RstCtrlMask = RCC_APB1RSTR1_UART5RST       },
#if defined(USART6)
  { .BlockId = RCC_BLOCK_USART6      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART6EN       , .LpCtrlMask = RCC_APB1SMENR1_USART6SMEN     , .RstCtrlMask = RCC_APB1RSTR1_USART6RST      },
#endif /* USART6 */
  { .BlockId = RCC_BLOCK_LPUART1     , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_LPUART1EN       , .LpCtrlMask = RCC_APB3SMENR_LPUART1SMEN     , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST      },
  { .BlockId = RCC_BLOCK_FDCAN1      , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_FDCAN1EN       , .LpCtrlMask = RCC_APB1SMENR2_FDCAN1SMEN     , .RstCtrlMask = RCC_APB1RSTR2_FDCAN1RST      },
#if defined(RCC_APB2ENR_USBEN)
  { .BlockId = RCC_BLOCK_USB         , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USBEN           , .LpCtrlMask = RCC_APB2SMENR_USBSMEN         , .RstCtrlMask = RCC_APB2RSTR_USBRST          },
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
  { .BlockId = RCC_BLOCK_OTG         , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OTGEN          , .LpCtrlMask = RCC_AHB2SMENR1_OTGSMEN        , .RstCtrlMask = RCC_AHB2RSTR1_OTGRST         },
#endif /* OTG */
#if defined(RCC_AHB2ENR1_USBPHYCEN)
  { .BlockId = RCC_BLOCK_USBPHYC     , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_USBPHYCEN      , .LpCtrlMask = RCC_AHB2SMENR1_USBPHYCSMEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION     },
#endif /* USBPHYC */
#if defined(UCPD1)
  { .BlockId = RCC_BLOCK_UCPD1       , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_UCPD1EN        , .LpCtrlMask = RCC_APB1SMENR2_UCPD1SMEN      , .RstCtrlMask = RCC_APB1RSTR2_UCPD1RST       },
#endif /* UCPD1 */
  { .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI1EN     , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI1SMEN   , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI1RST    },
#if defined(OCTOSPI2)
  { .BlockId = RCC_BLOCK_OCTOSPI2    , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_OCTOSPI2EN     , .LpCtrlMask = RCC_AHB2SMENR2_OCTOSPI2SMEN   , .RstCtrlMask = RCC_AHB2RSTR2_OCTOSPI2RST    },
#endif /* OCTOSPI2 */
#if defined(OCTOSPIM)
  { .BlockId = RCC_BLOCK_OCTOSPIM    , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OCTOSPIMEN     , .LpCtrlMask = RCC_AHB2SMENR1_OCTOSPIMSMEN   , .RstCtrlMask = RCC_AHB2RSTR1_OCTOSPIMRST    },
#endif /* OCTOSPIM */
#if defined(HSPI1)
  { .BlockId = RCC_BLOCK_HSPI1       , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_HSPI1EN        , .LpCtrlMask = RCC_AHB2SMENR2_HSPI1SMEN      , .RstCtrlMask = RCC_AHB2RSTR2_HSPI1RST       },
#endif /* HSPI1 */
#if defined(RCC_AHB2ENR2_FSMCEN)
  { .BlockId = RCC_BLOCK_FMC         , .ClkBusId = RCC_CLK_BUS_AHB2_2, .StateMask = RCC_AHB2ENR2_FSMCEN         , .LpCtrlMask = RCC_AHB2SMENR2_FSMCSMEN       , .RstCtrlMask = RCC_AHB2RSTR2_FSMCRST        },
#endif /* FMC */
  { .BlockId = RCC_BLOCK_SDMMC1      , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SDMMC1EN       , .LpCtrlMask = RCC_AHB2SMENR1_SDMMC1SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_SDMMC1RST      },
#if defined(SDMMC2)
  { .BlockId = RCC_BLOCK_SDMMC2      , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SDMMC2EN       , .LpCtrlMask = RCC_AHB2SMENR1_SDMMC2SMEN     , .RstCtrlMask = RCC_AHB2RSTR1_SDMMC2RST      },
#endif /* SDMMC2 */

  /*------------------------------- Multimedia -------------------------------*/

  { .BlockId = RCC_BLOCK_DCMI_PSSI   , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_DCMI_PSSIEN    , .LpCtrlMask = RCC_AHB2SMENR1_DCMI_PSSISMEN  , .RstCtrlMask = RCC_AHB2RSTR1_DCMI_PSSIRST   },
  { .BlockId = RCC_BLOCK_SAI1        , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN          , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN        , .RstCtrlMask = RCC_APB2RSTR_SAI1RST         },
#if defined(SAI2)
  { .BlockId = RCC_BLOCK_SAI2        , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI2EN          , .LpCtrlMask = RCC_APB2SMENR_SAI2SMEN        , .RstCtrlMask = RCC_APB2RSTR_SAI2RST         },
#endif /* SAI2 */
  { .BlockId = RCC_BLOCK_MDF1        , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_MDF1EN          , .LpCtrlMask = RCC_AHB1SMENR_MDF1SMEN        , .RstCtrlMask = RCC_AHB1RSTR_MDF1RST         },
  { .BlockId = RCC_BLOCK_ADF1        , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADF1EN          , .LpCtrlMask = RCC_AHB3SMENR_ADF1SMEN        , .RstCtrlMask = RCC_AHB3RSTR_ADF1RST         },
#if defined(DMA2D)
  { .BlockId = RCC_BLOCK_DMA2D       , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DMA2DEN         , .LpCtrlMask = RCC_AHB1SMENR_DMA2DSMEN       , .RstCtrlMask = RCC_AHB1RSTR_DMA2DRST        },
#endif /* DMA2D */
#if defined(GPU2D)
  { .BlockId = RCC_BLOCK_GPU2D       , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GPU2DEN         , .LpCtrlMask = RCC_AHB1SMENR_GPU2DSMEN       , .RstCtrlMask = RCC_AHB1RSTR_GPU2DRST        },
#endif /* GPU2D */
#if defined(GFXMMU)
  { .BlockId = RCC_BLOCK_GFXMMU      , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_GFXMMUEN        , .LpCtrlMask = RCC_AHB1SMENR_GFXMMUSMEN      , .RstCtrlMask = RCC_AHB1RSTR_GFXMMURST       },
#endif /* GFXMMU */
#if defined(GFXTIM)
  { .BlockId = RCC_BLOCK_GFXTIM      , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_GFXTIMEN        , .LpCtrlMask = RCC_APB2SMENR_GFXTIMSMEN      , .RstCtrlMask = RCC_APB2RSTR_GFXTIMRST       },
#endif /* GFXTIM */
#if defined(JPEG)
  { .BlockId = RCC_BLOCK_JPEG        , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_JPEGEN          , .LpCtrlMask = RCC_AHB1SMENR_JPEGSMEN        , .RstCtrlMask = RCC_AHB1RSTR_JPEGRST         },
#endif /* JPEG */
#if defined(LTDC)
  { .BlockId = RCC_BLOCK_LTDC        , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_LTDCEN          , .LpCtrlMask = RCC_APB2SMENR_LTDCSMEN        , .RstCtrlMask = RCC_APB2RSTR_LTDCRST         },
#endif /* LTDC */
#if defined(DSI)
  { .BlockId = RCC_BLOCK_DSIHOST     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_DSIHOSTEN       , .LpCtrlMask = RCC_APB2SMENR_DSIHOSTSMEN     , .RstCtrlMask = RCC_APB2RSTR_DSIHOSTRST      },
#endif /* DSIHOST */
  { .BlockId = RCC_BLOCK_TSC         , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_TSCEN           , .LpCtrlMask = RCC_AHB1SMENR_TSCSMEN         , .RstCtrlMask = RCC_AHB1RSTR_TSCRST          },

  /*--------------------------------- Analog ---------------------------------*/

  { .BlockId = RCC_BLOCK_ADC12       , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_ADC12EN        , .LpCtrlMask = RCC_AHB2SMENR1_ADC12SMEN      , .RstCtrlMask = RCC_AHB2RSTR1_ADC12RST       },
  { .BlockId = RCC_BLOCK_ADC4        , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_ADC4EN          , .LpCtrlMask = RCC_AHB3SMENR_ADC4SMEN        , .RstCtrlMask = RCC_AHB3RSTR_ADC4RST         },
  { .BlockId = RCC_BLOCK_DAC1        , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_DAC1EN          , .LpCtrlMask = RCC_AHB3SMENR_DAC1SMEN        , .RstCtrlMask = RCC_AHB3RSTR_DAC1RST         },
  { .BlockId = RCC_BLOCK_COMP        , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_COMPEN          , .LpCtrlMask = RCC_APB3SMENR_COMPSMEN        , .RstCtrlMask = RCC_APB3RSTR_COMPRST         },
  { .BlockId = RCC_BLOCK_OPAMP       , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_OPAMPEN         , .LpCtrlMask = RCC_APB3SMENR_OPAMPSMEN       , .RstCtrlMask = RCC_APB3RSTR_OPAMPRST        },
  { .BlockId = RCC_BLOCK_VREF        , .ClkBusId = RCC_CLK_BUS_APB3  , .StateMask = RCC_APB3ENR_VREFEN          , .LpCtrlMask = RCC_APB3SMENR_VREFSMEN        , .RstCtrlMask = RCC_APB3RSTR_VREFRST         },

  /*-------------------------------- Security --------------------------------*/

#if defined(AES)
  { .BlockId = RCC_BLOCK_AES         , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_AESEN          , .LpCtrlMask = RCC_AHB2SMENR1_AESSMEN        , .RstCtrlMask = RCC_AHB2RSTR1_AESRST         },
#endif /* AES */
#if defined(SAES)
  { .BlockId = RCC_BLOCK_SAES        , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_SAESEN         , .LpCtrlMask = RCC_AHB2SMENR1_SAESSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_SAESRST        },
#endif /* SAES */
  { .BlockId = RCC_BLOCK_HASH        , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_HASHEN         , .LpCtrlMask = RCC_AHB2SMENR1_HASHSMEN       , .RstCtrlMask = RCC_AHB2RSTR1_HASHRST        },
#if defined(PKA)
  { .BlockId = RCC_BLOCK_PKA         , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_PKAEN          , .LpCtrlMask = RCC_AHB2SMENR1_PKASMEN        , .RstCtrlMask = RCC_AHB2RSTR1_PKARST         },
#endif /* PKA */
#if defined(OTFDEC1)
  { .BlockId = RCC_BLOCK_OTFDEC1     , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OTFDEC1EN      , .LpCtrlMask = RCC_AHB2SMENR1_OTFDEC1SMEN    , .RstCtrlMask = RCC_AHB2RSTR1_OTFDEC1RST     },
#endif /* OTFDEC1 */
#if defined(OTFDEC2)
  { .BlockId = RCC_BLOCK_OTFDEC2     , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_OTFDEC2EN      , .LpCtrlMask = RCC_AHB2SMENR1_OTFDEC2SMEN    , .RstCtrlMask = RCC_AHB2RSTR1_OTFDEC2RST     },
#endif /* OTFDEC2 */
  { .BlockId = RCC_BLOCK_RNG         , .ClkBusId = RCC_CLK_BUS_AHB2_1, .StateMask = RCC_AHB2ENR1_RNGEN          , .LpCtrlMask = RCC_AHB2SMENR1_RNGSMEN        , .RstCtrlMask = RCC_AHB2RSTR1_RNGRST         },

  /*------------------------------- Computing --------------------------------*/

  { .BlockId = RCC_BLOCK_CORDIC      , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_CORDICEN        , .LpCtrlMask = RCC_AHB1SMENR_CORDICSMEN      , .RstCtrlMask = RCC_AHB1RSTR_CORDICRST       },
  { .BlockId = RCC_BLOCK_CRC         , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_CRCEN           , .LpCtrlMask = RCC_AHB1SMENR_CRCSMEN         , .RstCtrlMask = RCC_AHB1RSTR_CRCRST          },
  { .BlockId = RCC_BLOCK_FMAC        , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_FMACEN          , .LpCtrlMask = RCC_AHB1SMENR_FMACSMEN        , .RstCtrlMask = RCC_AHB1RSTR_FMACRST         },
};

_Static_assert( (sizeof(rcc_PeriphBlockConfig) / sizeof(rcc_BlockConfigStruct_t)) == RCC_BLOCK_LIST_CNT, "Rcc: rcc_PeriphBlockConfig has incorrect size." );

/* --------------------------- Reset source flags --------------------------- */

/** \brief CSR register flag masks, indexed by \ref rcc_ResetSrc_t */
static const uint32_t rcc_ResetSrcLut[] =
{
    RCC_CSR_PINRSTF , /**< \ref RCC_RESET_SRC_PIN  */
    RCC_CSR_BORRSTF , /**< \ref RCC_RESET_SRC_BOR  */
    RCC_CSR_SFTRSTF , /**< \ref RCC_RESET_SRC_SW   */
    RCC_CSR_IWDGRSTF, /**< \ref RCC_RESET_SRC_IWDG */
    RCC_CSR_WWDGRSTF, /**< \ref RCC_RESET_SRC_WWDG */
    RCC_CSR_LPWRRSTF, /**< \ref RCC_RESET_SRC_LPWR */
    RCC_CSR_OBLRSTF , /**< \ref RCC_RESET_SRC_OBL  */
};

_Static_assert( (sizeof(rcc_ResetSrcLut) / sizeof(uint32_t)) == RCC_RESET_SRC_CNT, "Rcc: rcc_ResetSrcLut has incorrect size." );

/* ------------------------------ Oscillators ------------------------------- */

/** \brief Control functions of oscillators, indexed by \ref rcc_OscId_t */
static const rcc_OscCtrl_t rcc_OscCtrlLut[ RCC_OSC_CNT ] =
{
    [RCC_OSC_HSI16] = { .SetActive = Rcc_ClkSrc_Set_Hsi16Active, .SetInactive = Rcc_ClkSrc_Set_Hsi16Inactive, .GetState = Rcc_ClkSrc_Get_Hsi16State,
                        .SetDiv    = RCC_NULL_PTR,               .GetDiv      = RCC_NULL_PTR                                                     },
    [RCC_OSC_HSI48] = { .SetActive = Rcc_ClkSrc_Set_Hsi48Active, .SetInactive = Rcc_ClkSrc_Set_Hsi48Inactive, .GetState = Rcc_ClkSrc_Get_Hsi48State,
                        .SetDiv    = RCC_NULL_PTR,               .GetDiv      = RCC_NULL_PTR                                                     },
    [RCC_OSC_MSIS]  = { .SetActive = Rcc_ClkSrc_Set_MsisActive,  .SetInactive = Rcc_ClkSrc_Set_MsisInactive,  .GetState = Rcc_ClkSrc_Get_MsisState,
                        .SetDiv    = Rcc_ClkSrc_Set_MsisDiv,     .GetDiv      = Rcc_ClkSrc_Get_MsisDiv                                           },
    [RCC_OSC_MSIK]  = { .SetActive = Rcc_ClkSrc_Set_MsikActive,  .SetInactive = Rcc_ClkSrc_Set_MsikInactive,  .GetState = Rcc_ClkSrc_Get_MsikState,
                        .SetDiv    = Rcc_ClkSrc_Set_MsikDiv,     .GetDiv      = Rcc_ClkSrc_Get_MsikDiv                                           },
    [RCC_OSC_LSI]   = { .SetActive = Rcc_ClkSrc_Set_LsiActive,   .SetInactive = Rcc_ClkSrc_Set_LsiInactive,   .GetState = Rcc_ClkSrc_Get_LsiState,
                        .SetDiv    = Rcc_ClkSrc_Set_LsiDiv,      .GetDiv      = Rcc_ClkSrc_Get_LsiDiv                                            },
    [RCC_OSC_LSE]   = { .SetActive = Rcc_ClkSrc_Set_LseActive,   .SetInactive = Rcc_ClkSrc_Set_LseInactive,   .GetState = Rcc_ClkSrc_Get_LseState,
                        .SetDiv    = RCC_NULL_PTR,               .GetDiv      = RCC_NULL_PTR                                                     },
};


/** \brief Internal oscillators started automatically for peripheral kernel clock, indexed by \ref rcc_ClkSrcId_t
 *         (\ref RCC_OSC_CNT - clock source is not an internal oscillator) */
static const rcc_OscId_t rcc_ClkSrcOscLut[ RCC_CLK_SRC_CNT ] =
{
    [RCC_CLK_SRC_SYSCLK]       = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL1RCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL1QCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL1PCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL2RCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL2QCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL2PCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL3RCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL3QCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_PLL3PCLK]     = RCC_OSC_CNT,
    [RCC_CLK_SRC_AHBCLK]       = RCC_OSC_CNT,
    [RCC_CLK_SRC_HCLKDIV8CLK]  = RCC_OSC_CNT,
    [RCC_CLK_SRC_APB1CLK]      = RCC_OSC_CNT,
    [RCC_CLK_SRC_APB2CLK]      = RCC_OSC_CNT,
    [RCC_CLK_SRC_APB3CLK]      = RCC_OSC_CNT,
    [RCC_CLK_SRC_APB1TIMCLK]   = RCC_OSC_CNT,
    [RCC_CLK_SRC_APB2TIMCLK]   = RCC_OSC_CNT,
    [RCC_CLK_SRC_HSI16CLK]     = RCC_OSC_HSI16,
    [RCC_CLK_SRC_MSISCLK]      = RCC_OSC_MSIS,
    [RCC_CLK_SRC_MSIKCLK]      = RCC_OSC_MSIK,
    [RCC_CLK_SRC_HSI48CLK]     = RCC_OSC_HSI48,
    [RCC_CLK_SRC_HSI48DIV2CLK] = RCC_OSC_HSI48,
    [RCC_CLK_SRC_HSECLK]       = RCC_OSC_CNT,
    [RCC_CLK_SRC_HSEDIV32CLK]  = RCC_OSC_CNT,
    [RCC_CLK_SRC_LSICLK]       = RCC_OSC_LSI,
    [RCC_CLK_SRC_LSECLK]       = RCC_OSC_CNT,
    [RCC_CLK_SRC_PINCLK]       = RCC_OSC_CNT,
};

/* ------------------------- Independent supplies --------------------------- */

/** \brief Peripheral blocks supplied by independent supply which has to be
 *         validated in PWR before use (VDDIO2 - PG[15:2], VDDA - analog
 *         peripherals, VDDUSB - USB). The supply must be present on the board. */
static const rcc_SupplyConfig_t rcc_SupplyLut[] =
{
    { .BlockId = RCC_BLOCK_GPIOG,  .EnableFunc = LL_PWR_EnableVddIO2, .IsEnabledFunc = LL_PWR_IsEnabledVddIO2 },
    { .BlockId = RCC_BLOCK_ADC12,  .EnableFunc = LL_PWR_EnableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
    { .BlockId = RCC_BLOCK_ADC4,   .EnableFunc = LL_PWR_EnableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
    { .BlockId = RCC_BLOCK_DAC1,   .EnableFunc = LL_PWR_EnableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
    { .BlockId = RCC_BLOCK_COMP,   .EnableFunc = LL_PWR_EnableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
    { .BlockId = RCC_BLOCK_OPAMP,  .EnableFunc = LL_PWR_EnableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
    { .BlockId = RCC_BLOCK_VREF,   .EnableFunc = LL_PWR_EnableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
#if defined(RCC_APB2ENR_USBEN)
    { .BlockId = RCC_BLOCK_USB,    .EnableFunc = LL_PWR_EnableVddUSB, .IsEnabledFunc = LL_PWR_IsEnabledVddUSB },
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
    { .BlockId = RCC_BLOCK_OTG,    .EnableFunc = LL_PWR_EnableVddUSB, .IsEnabledFunc = LL_PWR_IsEnabledVddUSB },
#endif /* OTG */
};

/** \brief Supplies validated by software, indexed by \ref rcc_PwrSupplyId_t */
static const rcc_PwrSupplyConfig_t rcc_PwrSupplyLut[ RCC_PWR_SUPPLY_CNT ] =
{
    [RCC_PWR_SUPPLY_VDDUSB] = { .EnableFunc = LL_PWR_EnableVddUSB, .DisableFunc = LL_PWR_DisableVddUSB, .IsEnabledFunc = LL_PWR_IsEnabledVddUSB },
    [RCC_PWR_SUPPLY_VDDIO2] = { .EnableFunc = LL_PWR_EnableVddIO2, .DisableFunc = LL_PWR_DisableVddIO2, .IsEnabledFunc = LL_PWR_IsEnabledVddIO2 },
    [RCC_PWR_SUPPLY_VDDA]   = { .EnableFunc = LL_PWR_EnableVddA,   .DisableFunc = LL_PWR_DisableVddA,   .IsEnabledFunc = LL_PWR_IsEnabledVddA   },
};

/* ----------------------- HSI48 trimming sources --------------------------- */

/** \brief Synchronization sources of the HSI48 automatic trimming, indexed by \ref rcc_Hsi48TrimSrc_t */
static const rcc_Hsi48TrimConfigStruct_t rcc_Hsi48TrimLut[ RCC_HSI48_TRIM_SRC_CNT ] =
{
    [RCC_HSI48_TRIM_SRC_USB_SOF] = { .LlSource = LL_CRS_SYNC_SOURCE_USB, .SyncFreq = RCC_HSI48_SYNC_USB_SOF_HZ },
    [RCC_HSI48_TRIM_SRC_LSE]     = { .LlSource = LL_CRS_SYNC_SOURCE_LSE, .SyncFreq = RCC_HSI48_SYNC_LSE_HZ     },
};

/* ----------------------- Flash latency and voltage ------------------------ */

/** \brief Maximal HCLK frequency for number of flash wait states in voltage ranges (RM0456) */
static const rcc_FlashLatencyConfig_t rcc_FlashLatencyLut[ RCC_PWR_VOLTAGE_SCALE_CNT ] =
{
    { .VoltageScale = LL_PWR_REGU_VOLTAGE_SCALE1, .ThresholdCnt = 5u, .MaxFreqHz = { 32000000u, 64000000u, 96000000u, 128000000u, 160000000u } },
    { .VoltageScale = LL_PWR_REGU_VOLTAGE_SCALE2, .ThresholdCnt = 5u, .MaxFreqHz = { 25000000u, 50000000u, 75000000u, 100000000u, 110000000u } },
    { .VoltageScale = LL_PWR_REGU_VOLTAGE_SCALE3, .ThresholdCnt = 5u, .MaxFreqHz = { 12500000u, 25000000u, 37500000u,  50000000u,  55000000u } },
    { .VoltageScale = LL_PWR_REGU_VOLTAGE_SCALE4, .ThresholdCnt = 4u, .MaxFreqHz = {  8000000u, 16000000u, 24000000u,  25000000u,         0u } },
};


/** \brief LL flash latency values, indexed by number of wait states */
static const uint32_t rcc_FlashLatencyValueLut[ RCC_FLASH_LATENCY_THR_CNT ] =
{
    LL_FLASH_LATENCY_0, LL_FLASH_LATENCY_1, LL_FLASH_LATENCY_2, LL_FLASH_LATENCY_3, LL_FLASH_LATENCY_4
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
 * Configuration sequence:
 *  1. FLASH, SYSCFG and PWR clocks, flash prefetch, instruction cache.
 *  2. System clock is moved to HSI16 (flash latency raised for it first) -
 *     voltage range and PLLs can be changed safely from any previous state.
 *  3. Voltage range (EPOD booster in range 1 / 2), flash latency for the
 *     expected processor clock.
 *  4. HSE (clock security system), PLL1 - PLL3, bus dividers.
 *  5. System clock source (PLL1 waits for EPOD booster), exact flash latency,
 *     SysTick, CMSIS SystemCoreClock, clock outputs.
 *
 * \param clockConfig [in]: Clock configuration. Must not be NULL.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Init( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState       = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       expectedSysClk = 0u;
    rcc_FreqHz_t       hsiFreq        = 0u;

    if( RCC_NULL_PTR != clockConfig )
    {
        retState = Rcc_Set_PeriphActive( RCC_PERIPH_FLASH );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_SYSCFG );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_PWR );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashPrefetchActive();
        }
        else
        {
            /* Error during initialization process */
        }

#if defined (PWR_UCPDR_UCPD_DBDIS)
        /* Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral */
        LL_PWR_DisableUCPDDeadBattery();
#endif

        LL_ICACHE_SetMode( LL_ICACHE_1WAY );
        LL_ICACHE_Enable();

        /*-------------- System clock temporarily from HSI16 -----------------*/
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_Hsi16Active();
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Get_Hsi16Clk( &hsiFreq );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashLatencyRaise( hsiFreq );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* A PLL cannot be deactivated by hardware while it drives SYSCLK
             * and voltage range can be lowered only at low frequency. HSI16 is
             * valid in every voltage range. */
            retState = Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_HSI );
        }
        else
        {
            /* Error during initialization process */
        }

        /*---------------- Voltage range and flash latency -------------------*/
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PwrRange( clockConfig );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Latency for the higher one of HSI16 and expected system clock (AHB divider ignored - upper bound) */
            if( hsiFreq > expectedSysClk )
            {
                expectedSysClk = hsiFreq;
            }
            else
            {
                /* Expected system clock is higher */
            }

            retState = Rcc_Set_FlashLatencyRaise( expectedSysClk );
        }
        else
        {
            /* Error during initialization process */
        }

        /*-------------------------- HSE and PLLs -----------------------------*/
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_HseActive( clockConfig->HSE_ClockType );
        }
        else
        {
            /* Error during initialization process */
        }

        if( ( RCC_REQUEST_OK    == retState                   ) &&
            ( RCC_HSE_TYPE_NONE != clockConfig->HSE_ClockType )    )
        {
            retState = Rcc_ClkSrc_Set_HseClk( clockConfig->HSE_Frequency_Hz );

            if( ( RCC_REQUEST_OK      == retState                ) &&
                ( RCC_FUNCTION_ACTIVE == clockConfig->CSS_Enable )    )
            {
                LL_RCC_HSE_EnableCSS();
            }
            else
            {
                /* Clock security system is not used */
            }
        }
        else
        {
            /* HSE is not used or error during initialization process */
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Config( pllId, &clockConfig->Pll_Config[ pllId ] );
            }
            else
            {
                /* Error during initialization process */
            }
        }

        /*-------------------------- Bus dividers -----------------------------*/
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_AHBDivider( clockConfig->AHB_Divider );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB1Divider( clockConfig->APB1_Divider );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB2Divider( clockConfig->APB2_Divider );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB3Divider( clockConfig->APB3_Divider );
        }
        else
        {
            /* Error during initialization process */
        }

        /*----------------------- System clock source -------------------------*/
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_SysClkOscActive( clockConfig->SystemClockSource );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Flash latency, SysTick and CMSIS SystemCoreClock depend on processor clock (HCLK) */
            retState = Rcc_Set_FlashLatencyExact( clockConfig->FlashLatency );
        }
        else
        {
            /* Error during initialization process */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_SysTickInterval( clockConfig->SysTickInterval );
        }
        else
        {
            /* Error during initialization process */
        }

        for( rcc_ClkOut_Id_t clkOutId = RCC_CLK_OUT_MCO1; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                /* MCO configuration (return states are not needed to be checked) */
                (void)Rcc_Set_ClkOutSource( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockSource );
                (void)Rcc_Set_ClkOutDivider( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockDivider );
            }
            else
            {
                /* Error during initialization process */
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
 * \brief Deinitializes module Rcc
 *
 * \param clockConfig [in]: Clock configuration (not used).
 */
void Rcc_Deinit( rcc_ConfigStruct_t * const clockConfig )
{
    (void) clockConfig;

    return;
}


/**
 * \brief Main task of module Rcc
 */
void Rcc_Task( void )
{
    return;
}


/**
 * \brief Clock configuration structure default value initialization
 *
 * Default configuration: system clock 160 MHz from PLL1 output R, PLL1 clocked
 * by MSIS 4 MHz (reset range, M = 1, N = 80, P = Q = R = 2), voltage range 1,
 * bus dividers 1, HSE, PLL2 and PLL3 not used, SysTick 1 ms.
 *
 * \param clockConfig [out]: Pointer to clock configuration structure. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_DefaultConfig( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clockConfig )
    {
        clockConfig->HSE_ClockType     = RCC_HSE_TYPE_NONE;
        clockConfig->HSE_Frequency_Hz  = RCC_DEFAULT_HSE_FREQ_HZ;
        clockConfig->SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
        clockConfig->CSS_Enable        = RCC_FUNCTION_INACTIVE;
        clockConfig->AHB_Divider       = RCC_AHB_DIVIDER_1;
        clockConfig->APB1_Divider      = RCC_APB1_DIVIDER_1;
        clockConfig->APB2_Divider      = RCC_APB2_DIVIDER_1;
        clockConfig->APB3_Divider      = RCC_APB3_DIVIDER_1;
        clockConfig->FlashLatency      = RCC_FLASH_LATENCY_0_WS;
        clockConfig->VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_1;
        clockConfig->SysTickInterval   = RCC_DEFAULT_SYSTICK_INTERVAL_MS;

        for( rcc_ClkOut_Id_t clkOutId = RCC_CLK_OUT_MCO1; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
        {
            clockConfig->McoConfig[ clkOutId ].ClockSource  = RCC_CLK_SOURCE_NONE;
            clockConfig->McoConfig[ clkOutId ].ClockDivider = RCC_DEFAULT_CLK_OUT_DIV;
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            clockConfig->Pll_Config[ pllId ].Pll_Source   = RCC_PLL_SRC_NONE;
            clockConfig->Pll_Config[ pllId ].M_Divider    = RCC_DEFAULT_PLL_M_DIV;
            clockConfig->Pll_Config[ pllId ].N_Multiplier = RCC_DEFAULT_PLL_N_MULT;
            clockConfig->Pll_Config[ pllId ].P_Divider    = RCC_DEFAULT_PLL_OUT_DIV;
            clockConfig->Pll_Config[ pllId ].Q_Divider    = RCC_DEFAULT_PLL_OUT_DIV;
            clockConfig->Pll_Config[ pllId ].R_Divider    = RCC_DEFAULT_PLL_OUT_DIV;
        }

        clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_MSIS;

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
 * If the kernel clock source of the peripheral ID is an internal oscillator
 * (HSI16, HSI48, MSIS, MSIK, LSI) which is not running, the oscillator is
 * started before the clock MUX is switched. External sources (HSE, LSE) and
 * PLL outputs are not started.
 *
 * Independent supply of the peripheral is validated in PWR after the clock is
 * enabled (VDDIO2 for port G, VDDA for ADC / DAC / COMP / OPAMP / VREFBUF,
 * VDDUSB for USB) - the supply must be present on the board.
 *
 * \param periphId [in]: ID of required peripheral to activate clock source, value from \ref rcc_PeriphId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when the oscillator can not be started
 *         or the clock MUX is set to other source, peripheral clock is not
 *         changed in that case).
 */
rcc_RequestState_t Rcc_Set_PeriphActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        retState = Rcc_Set_ClkSrcOscActive( rcc_ConfigStruct[ periphId ].ClkSrcId );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    if( RCC_REQUEST_OK == retState )
    {
        const rcc_ClkMuxId_t clkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;

        if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
        {
            /* Multiplexer set to other source is not changed - peripheral clock is not enabled */
            retState = Rcc_ClkMux_Set_ClkActive( clkMuxId );
        }
        else
        {
            /* Peripheral does not support clock multiplexing. */
        }
    }
    else
    {
        /* Oscillator could not be started or incorrect peripheral ID */
    }

    if( RCC_REQUEST_OK == retState )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;
        const uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            /* Activate peripheral clock */
            Rcc_Set_RegBit( stateRegId, stateMask );

            retState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegBit( stateRegId, stateMask );

                if( RCC_FLAG_CLEARED != regValue )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
            /* Peripheral without clock enable bit */
            retState = RCC_REQUEST_OK;
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_SupplyValid( blockId );
        }
        else
        {
            /* Peripheral clock was not enabled */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Peripheral clock disable request
 *
 * User can request de-activation of clock for required peripheral. If required
 * peripheral is correctly de-activated, and required peripheral ID is correct,
 * returned state is "OK". Otherwise returns error. The kernel clock multiplexer of
 * the peripheral is released (default source), so the peripheral can be activated
 * with another kernel clock later. RTC clock selection is kept (RTCSEL is write-once
 * until backup domain reset) and a multiplexer shared with another enabled
 * peripheral block (ADCDACSEL of ADC1 / ADC2, ADC4 and DAC1, LPTIM34SEL,
 * OCTOSPISEL) is kept.
 *
 * \param periphId [in]: ID of required peripheral to de-activate clock source, value from \ref rcc_PeriphId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PeriphInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;
        const rcc_ClkMuxId_t  clkMuxId   = rcc_ConfigStruct[ periphId ].ClkMuxId;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            Rcc_Reset_RegBit( stateRegId, stateMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegBit( stateRegId, stateMask );

                if( RCC_FLAG_CLEARED == regValue )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
            retState = RCC_REQUEST_OK;
        }

        /* Kernel clock multiplexer back to the default source */
        if( ( RCC_REQUEST_OK       == retState ) &&
            ( RCC_CLK_MUX_LIST_CNT  > clkMuxId ) &&
            ( RCC_BLOCK_RTC        != blockId  )    )
        {
            const rcc_FunctionState_t sharedState = Rcc_Get_ClkMuxShared( periphId );

            if( RCC_FUNCTION_INACTIVE == sharedState )
            {
                retState = Rcc_ClkMux_Set_ClkInactive( clkMuxId );
            }
            else
            {
                /* Multiplexer shared with another enabled peripheral block is kept */
            }
        }
        else
        {
            /* Clock disable failed, no multiplexer or RTC selection */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading state of peripheral clock activation
 *
 * \param periphId   [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 * \param funcState [out]: Pointer to store state of peripheral activation. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            const uint32_t regValue = Rcc_Get_RegBit( stateRegId, stateMask );

            if( RCC_FLAG_CLEARED == regValue )
            {
                *funcState = RCC_FUNCTION_INACTIVE;
            }
            else
            {
                *funcState = RCC_FUNCTION_ACTIVE;
            }
        }
        else
        {
            /* Peripheral without clock enable bit is always clocked */
            *funcState = RCC_FUNCTION_ACTIVE;
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
 * \brief Returns clock frequency for selected peripheral.
 *
 * \param periphId   [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 * \param periphClk [out]: Pointer to store frequency for selected peripheral in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for external clock input pin source).
 */
rcc_RequestState_t Rcc_Get_PeriphClk( rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != periphClk )    )
    {
        const rcc_ClkSrcId_t       periphClkSrcId = rcc_ConfigStruct[ periphId ].ClkSrcId;
        const rcc_ClkSrcCallback_t clkSrcCallback = rcc_PeriphClkSrcConfig[ periphClkSrcId ].ClkSrcCallback;

        if( RCC_NULL_PTR != clkSrcCallback )
        {
            returnState = clkSrcCallback( periphClk );
        }
        else
        {
            /* Frequency of the clock source is not known */
            *periphClk  = 0u;
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
 * \brief Returns clock source for selected peripheral.
 *
 * For peripherals that can use different clock sources through clock
 * multiplexer, any of enumeration in its range can be used. For example for
 * USART1 can be used any of \c RCC_PERIPH_USART1_PCLK2, \c RCC_PERIPH_USART1_SYSCLK,
 * \c RCC_PERIPH_USART1_HSI or \c RCC_PERIPH_USART1_LSE and correct enumeration
 * will be returned.
 *
 * \param periphId      [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 * \param periphClkSrc [out]: Pointer to store peripheral ID matching the actually selected clock source. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphClkSrc( rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc )
{
    rcc_RequestState_t returnState   = RCC_REQUEST_ERROR;
    rcc_ClkMuxId_t     clkMuxId      = RCC_CLK_MUX_LIST_CNT;
    rcc_PeriphId_t     foundPeriphId = RCC_PERIPH_ID_CNT;

    if( ( RCC_PERIPH_ID_CNT > periphId     ) &&
        ( RCC_NULL_PTR     != periphClkSrc )    )
    {
        const rcc_ClkMuxId_t  periphClkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;
        const rcc_BlockList_t blockId        = rcc_ConfigStruct[ periphId ].BlockId;

        if( RCC_CLK_MUX_LIST_CNT > periphClkMuxId )
        {
            returnState = Rcc_ClkMux_Get_ClkSrc( periphClkMuxId, &clkMuxId );

            if( RCC_REQUEST_OK == returnState )
            {
                /* Search peripheral entry of the same block with currently selected clock multiplexer input */
                for( rcc_PeriphId_t periphSearchId = (rcc_PeriphId_t)0u; RCC_PERIPH_ID_CNT > periphSearchId; periphSearchId ++ )
                {
                    if( ( blockId  == rcc_ConfigStruct[ periphSearchId ].BlockId  ) &&
                        ( clkMuxId == rcc_ConfigStruct[ periphSearchId ].ClkMuxId )    )
                    {
                        foundPeriphId = periphSearchId;
                        break;
                    }
                    else
                    {
                        /* Continue with next peripheral entry */
                    }
                }

                if( RCC_PERIPH_ID_CNT > foundPeriphId )
                {
                    *periphClkSrc = foundPeriphId;
                }
                else
                {
                    returnState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* Multiplexer selection is not known */
            }
        }
        else
        {
            *periphClkSrc = periphId;
            returnState   = RCC_REQUEST_OK;
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
 * \param periphId [in]: ID of required peripheral to activate reset, value from \ref rcc_PeriphId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for peripheral without reset control,
 *         no register is changed in that case).
 */
rcc_RequestState_t Rcc_Set_ResetActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        if( RCC_UNSUPPORTED_FUNCTION != resetMask )
        {
            Rcc_Set_RegBit( stateRegId, resetMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegBit( stateRegId, resetMask );

                if( RCC_FLAG_CLEARED != regValue )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
            /* Peripheral without reset control */
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
 * \brief Peripheral reset disable request
 *
 * \param periphId [in]: ID of required peripheral to deactivate reset, value from \ref rcc_PeriphId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error. Peripheral without reset control is never
 *         in reset - "OK" is returned without register access.
 */
rcc_RequestState_t Rcc_Set_ResetInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        if( RCC_UNSUPPORTED_FUNCTION != resetMask )
        {
            Rcc_Reset_RegBit( stateRegId, resetMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegBit( stateRegId, resetMask );

                if( RCC_FLAG_CLEARED == regValue )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
            /* Peripheral without reset control */
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
 * \brief Reading state of peripheral reset
 *
 * \param periphId   [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 * \param funcState [out]: Pointer to store reset state. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ResetState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;
        uint32_t              regValue   = RCC_FLAG_CLEARED;

        if( RCC_UNSUPPORTED_FUNCTION != resetMask )
        {
            regValue = Rcc_Get_RegBit( stateRegId, resetMask );
        }
        else
        {
            /* Peripheral without reset control is never in reset */
        }

        if( RCC_FLAG_CLEARED == regValue )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            *funcState = RCC_FUNCTION_ACTIVE;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------------- Power mode configuration -------------------------*/

/**
 * \brief Enable peripheral clock in Sleep and Stop modes
 *
 * \param periphId [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_SleepActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        if( RCC_UNSUPPORTED_FUNCTION != sleepMask )
        {
            Rcc_Set_RegBit( stateRegId, sleepMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegBit( stateRegId, sleepMask );

                if( RCC_FLAG_CLEARED != regValue )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
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
 * \brief Disable peripheral clock in Sleep and Stop modes
 *
 * \param periphId [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_SleepInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        if( RCC_UNSUPPORTED_FUNCTION != sleepMask )
        {
            Rcc_Reset_RegBit( stateRegId, sleepMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t regValue = Rcc_Get_RegBit( stateRegId, sleepMask );

                if( RCC_FLAG_CLEARED == regValue )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
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
 * \brief Reading state of peripheral clock in Sleep and Stop modes
 *
 * \param periphId   [in]: ID of required peripheral, value from \ref rcc_PeriphId_t
 * \param funcState [out]: Pointer to store state. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SleepState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        const uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        const rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;
        uint32_t              regValue   = RCC_FLAG_CLEARED;

        if( RCC_UNSUPPORTED_FUNCTION != sleepMask )
        {
            regValue = Rcc_Get_RegBit( stateRegId, sleepMask );
        }
        else
        {
            /* Peripheral without sleep mode control */
        }

        if( RCC_FLAG_CLEARED == regValue )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            *funcState = RCC_FUNCTION_ACTIVE;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------------- Clock buses configuration ------------------------*/

/**
 * \brief Function used for setting clock bus divider
 *
 * AHB1, AHB2 and AHB3 buses share AHB divider, APB1 groups share APB1 divider.
 *
 * \param clkBusId      [in]: ID of required clock bus, value from \ref rcc_ClkBusId_t
 * \param clkBusDivider [in]: Required clock bus divider (value of the bus divider type)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ClkBusDivider( rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_CLK_BUS_APB1_1 > clkBusId )
    {
        retState = Rcc_ClkBus_Set_AHBDivider( (rcc_AHB_Div_t)clkBusDivider );
    }
    else if( ( RCC_CLK_BUS_APB1_1 == clkBusId ) ||
             ( RCC_CLK_BUS_APB1_2 == clkBusId )    )
    {
        retState = Rcc_ClkBus_Set_APB1Divider( (rcc_APB1_Div_t)clkBusDivider );
    }
    else if( RCC_CLK_BUS_APB2 == clkBusId )
    {
        retState = Rcc_ClkBus_Set_APB2Divider( (rcc_APB2_Div_t)clkBusDivider );
    }
    else if( RCC_CLK_BUS_APB3 == clkBusId )
    {
        retState = Rcc_ClkBus_Set_APB3Divider( (rcc_APB3_Div_t)clkBusDivider );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Function used for getting clock bus divider
 *
 * \param clkBusId       [in]: ID of required clock bus, value from \ref rcc_ClkBusId_t
 * \param clkBusDivider [out]: Pointer to store current clock bus divider. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkBusDivider( rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_CLK_BUS_APB1_1 > clkBusId )
    {
        retState = Rcc_ClkBus_Get_AHBDivider( (rcc_AHB_Div_t*) clkBusDivider );
    }
    else if( ( RCC_CLK_BUS_APB1_1 == clkBusId ) ||
             ( RCC_CLK_BUS_APB1_2 == clkBusId )    )
    {
        retState = Rcc_ClkBus_Get_APB1Divider( (rcc_APB1_Div_t*) clkBusDivider );
    }
    else if( RCC_CLK_BUS_APB2 == clkBusId )
    {
        retState = Rcc_ClkBus_Get_APB2Divider( (rcc_APB2_Div_t*) clkBusDivider );
    }
    else if( RCC_CLK_BUS_APB3 == clkBusId )
    {
        retState = Rcc_ClkBus_Get_APB3Divider( (rcc_APB3_Div_t*) clkBusDivider );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Function used for getting clock bus frequency
 *
 * \param clkBusId    [in]: ID of required clock bus, value from \ref rcc_ClkBusId_t
 * \param clkBusFreq [out]: Pointer to store current clock bus frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkBusClk( rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_CLK_BUS_APB1_1 > clkBusId )
    {
        retState = Rcc_ClkBus_Get_AHBClk( clkBusFreq );
    }
    else if( ( RCC_CLK_BUS_APB1_1 == clkBusId ) ||
             ( RCC_CLK_BUS_APB1_2 == clkBusId )    )
    {
        retState = Rcc_ClkBus_Get_APB1Clk( clkBusFreq );
    }
    else if( RCC_CLK_BUS_APB2 == clkBusId )
    {
        retState = Rcc_ClkBus_Get_APB2Clk( clkBusFreq );
    }
    else if( RCC_CLK_BUS_APB3 == clkBusId )
    {
        retState = Rcc_ClkBus_Get_APB3Clk( clkBusFreq );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------ Power range and latency configuration -------------------*/

/**
 * \brief Function used for power (voltage) range configuration
 *
 * Voltage range limits the system clock: range 1 - 160 MHz, range 2 - 110 MHz,
 * range 3 - 55 MHz, range 4 - 25 MHz. Embedded power distribution booster
 * (EPOD) is enabled in range 1 and 2 (before the range is increased) and
 * disabled in range 3 and 4. PWR bus clock is enabled.
 *
 * \pre   Lower range is configured only while the system clock is within the
 *        limit of the new range (\ref Rcc_Init switches to HSI16 first).
 *
 * \param clockConfig [in]: Configuration structure (VoltageScaling). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PwrRange( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clockConfig )
    {
        const uint32_t voltageScale = (uint32_t)clockConfig->VoltageScaling;

        for( uint32_t lutIdx = 0u; RCC_PWR_VOLTAGE_SCALE_CNT > lutIdx; lutIdx++ )
        {
            if( voltageScale == rcc_FlashLatencyLut[ lutIdx ].VoltageScale )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next voltage range */
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_PWR );
        }
        else
        {
            /* Unsupported voltage range */
        }

        if( RCC_REQUEST_OK == retState )
        {
            if( ( RCC_PWR_VOLTAGE_SCALE_1 == clockConfig->VoltageScaling ) ||
                ( RCC_PWR_VOLTAGE_SCALE_2 == clockConfig->VoltageScaling )    )
            {
                /* EPOD booster must be enabled before range 1 / 2 is selected */
                LL_PWR_EnableEPODBooster();
            }
            else
            {
                /* Booster is not used in range 3 / 4 */
            }

            LL_PWR_SetRegulVoltageScaling( voltageScale );

            retState = RCC_REQUEST_ERROR;

            /* Requested range is written and regulator reached it (VOSRDY) */
            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t voltageScalingRegValue = LL_PWR_GetRegulVoltageScaling();
                const uint32_t voltageReadyFlag       = LL_PWR_IsActiveFlag_VOS();

                if( ( voltageScale     == voltageScalingRegValue ) &&
                    ( RCC_FLAG_CLEARED != voltageReadyFlag       )    )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Voltage scaling has not been reached yet, keep return state as error */
                    retState = RCC_REQUEST_ERROR;
                }
            }

            if( ( RCC_PWR_VOLTAGE_SCALE_3 == clockConfig->VoltageScaling ) ||
                ( RCC_PWR_VOLTAGE_SCALE_4 == clockConfig->VoltageScaling )    )
            {
                LL_PWR_DisableEPODBooster();
            }
            else
            {
                /* Booster stays enabled in range 1 / 2 */
            }
        }
        else
        {
            /* Unsupported voltage range or PWR clock not enabled */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Function used to flash wait states ("delay") configuration.
 *
 * Number of wait states is calculated from expected processor clock (system
 * clock of the configuration divided by AHB divider) and actual voltage range
 * (see \ref rcc_FlashLatency_t). Configured FlashLatency is used as minimum.
 *
 * \param clockConfig [in]: Configuration structure. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when the frequency exceeds the range).
 */
rcc_RequestState_t Rcc_Set_FlashLatency( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState       = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       expectedSysClk = 0u;
    rcc_FlashLatency_t latency        = RCC_FLASH_LATENCY_0_WS;

    if( RCC_NULL_PTR != clockConfig )
    {
        retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );

        if( RCC_REQUEST_OK == retState )
        {
            const rcc_FreqHz_t expectedHclk = __LL_RCC_CALC_HCLK_FREQ( expectedSysClk, clockConfig->AHB_Divider );

            retState = Rcc_Get_FlashLatencyFreq( expectedHclk, clockConfig->FlashLatency, &latency );
        }
        else
        {
            /* Expected system clock is not known */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashLatencyValue( latency );
        }
        else
        {
            /* Latency can not be calculated */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Function used to flash prefetch buffer activation.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchActive( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    Rcc_Set_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t prefetchState = Rcc_Get_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk );

        if( RCC_FLAG_CLEARED != prefetchState )
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
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchInactive( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    Rcc_Reset_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t prefetchState = Rcc_Get_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN_Msk );

        if( RCC_FLAG_CLEARED == prefetchState )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Prefetch buffer has not yet been de-activated, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return (retState);
}


/**
 * \brief Configures the interval between SysTick's in ms [0.001s]
 *
 * SysTick is configured by CMSIS SysTick_Config, which selects the processor
 * clock (HCLK) as SysTick clock source. Reload value is calculated from actual
 * HCLK frequency and must fit the 24-bit reload register.
 *
 * \param sysTickInterval [in]: Interval value between ticks in ms [0.001s], greater than 0
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Set_SysTickInterval( rcc_Time_ms_t sysTickInterval )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( 0u < sysTickInterval )
    {
        rcc_FreqHz_t             hclkFreq = 0u;
        const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hclkFreq );
        const uint64_t           ticksCnt = ( (uint64_t)hclkFreq * sysTickInterval ) / RCC_MS_IN_SECOND;

        if( ( RCC_REQUEST_OK        == clkState ) &&
            ( RCC_SYSTICK_TICKS_MIN <= ticksCnt ) &&
            ( RCC_SYSTICK_TICKS_MAX >= ticksCnt )    )
        {
            const uint32_t configState = SysTick_Config( (uint32_t)ticksCnt );

            if( 0u == configState )
            {
                retState = RCC_REQUEST_OK;
            }
            else
            {
                /* Reload value rejected by SysTick */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Clock not available or interval out of SysTick range */
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
 * \brief Returns the interval between SysTick's in ms [0.001s]
 *
 * \param sysTickInterval [out]: Pointer to store interval between ticks in ms [0.001s]. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SysTickInterval( rcc_Time_ms_t * const sysTickInterval )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != sysTickInterval )
    {
        rcc_FreqHz_t             hclkFreq = 0u;
        const rcc_RequestState_t clkState = Rcc_ClkBus_Get_AHBClk( &hclkFreq );

        if( ( RCC_REQUEST_OK == clkState ) &&
            ( 0u              < hclkFreq )    )
        {
            /* SysTick is clocked by processor clock (HCLK) */
            const uint64_t ticksCnt = (uint64_t)SysTick->LOAD + RCC_SYSTICK_RELOAD_OFFSET;

            *sysTickInterval = (rcc_Time_ms_t)( ( ticksCnt * RCC_MS_IN_SECOND ) / hclkFreq );

            retState = RCC_REQUEST_OK;
        }
        else
        {
            /* Clock is not available */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/**
 * \brief Configures Phase Locked Loop (source, dividers, multiplier, outputs) and activates it.
 *
 * \param pllId        [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param configStruct [in]: Phase Locked Loop configuration. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllConfig( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    return ( Rcc_Pll_Set_Config( pllId, configStruct ) );
}


/**
 * \brief Reads Phase Locked Loop internal (VCO) clock frequency.
 *
 * \param pllId   [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param pllClk [out]: Pointer to store VCO frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllInternalClk( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    return ( Rcc_Pll_Get_InternalClk( pllId, pllClk ) );
}


/**
 * \brief Activates Phase Locked Loop.
 *
 * \param pllId [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllActive( rcc_PllId_t pllId )
{
    return ( Rcc_Pll_Set_Active( pllId ) );
}


/**
 * \brief Deactivates Phase Locked Loop.
 *
 * \param pllId [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllInactive( rcc_PllId_t pllId )
{
    return ( Rcc_Pll_Set_Inactive( pllId ) );
}


/**
 * \brief Reads activation state of Phase Locked Loop.
 *
 * \param pllId     [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param retState [out]: Pointer to store activation state (PLL locked). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllState( rcc_PllId_t pllId, rcc_FunctionState_t * const retState )
{
    return ( Rcc_Pll_Get_State( pllId, retState ) );
}


/**
 * \brief Selects clock source of Phase Locked Loop.
 *
 * \param pllId     [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param clkSource [in]: PLL clock source, value from \ref rcc_PllClkSrc_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllsSource( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    return ( Rcc_Pll_Set_Source( pllId, clkSource ) );
}


/**
 * \brief Reads clock source of Phase Locked Loop.
 *
 * \param pllId      [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param clkSource [out]: Pointer to store PLL clock source. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllsSource( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource )
{
    return ( Rcc_Pll_Get_Source( pllId, clkSource ) );
}


/**
 * \brief Reads frequency of Phase Locked Loop output P.
 *
 * \param pllId   [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param pllClk [out]: Pointer to store frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    return ( Rcc_Pll_Get_Clk_OutP( pllId, pllClk ) );
}


/**
 * \brief Reads frequency of Phase Locked Loop output Q.
 *
 * \param pllId   [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param pllClk [out]: Pointer to store frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    return ( Rcc_Pll_Get_Clk_OutQ( pllId, pllClk ) );
}


/**
 * \brief Reads frequency of Phase Locked Loop output R.
 *
 * \param pllId   [in]: Phase Locked Loop identification, value from \ref rcc_PllId_t
 * \param pllClk [out]: Pointer to store frequency in Hz. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    return ( Rcc_Pll_Get_Clk_OutR( pllId, pllClk ) );
}

/*-------------------------- Oscillators configuration -----------------------*/

/**
 * \brief Activates oscillator.
 *
 * \param oscId [in]: Oscillator identification, value from \ref rcc_OscId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscActive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_OSC_CNT > oscId )
    {
        retState = rcc_OscCtrlLut[ oscId ].SetActive();
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Deactivates oscillator.
 *
 * \warning Oscillator used as system clock or PLL source must not be deactivated.
 *
 * \param oscId [in]: Oscillator identification, value from \ref rcc_OscId_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscInactive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_OSC_CNT > oscId )
    {
        retState = rcc_OscCtrlLut[ oscId ].SetInactive();
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads activation state of oscillator.
 *
 * \param oscId     [in]: Oscillator identification, value from \ref rcc_OscId_t
 * \param retState [out]: Pointer to store activation state (oscillator ready). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_OscState( rcc_OscId_t oscId, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t reqState = RCC_REQUEST_ERROR;

    if( RCC_OSC_CNT > oscId )
    {
        reqState = rcc_OscCtrlLut[ oscId ].GetState( retState );
    }
    else
    {
        reqState = RCC_REQUEST_ERROR;
    }

    return ( reqState );
}


/**
 * \brief Configures divider of oscillator output.
 *
 * Supported dividers are described by \ref rcc_OscDiv_t (MSIS / MSIK range,
 * LSI prescaler). Other oscillators accept divider 1 only.
 *
 * \warning MSIS divider changes the frequency of all clocks derived from MSIS
 *          (system clock, PLL input). Flash latency and CMSIS SystemCoreClock
 *          are updated when MSIS is the system clock, peripherals (SysTick,
 *          timers, communication) have to be reconfigured.
 *
 * \param oscId  [in]: Oscillator identification, value from \ref rcc_OscId_t
 * \param oscDiv [in]: Divider value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscDiv( rcc_OscId_t oscId, rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t retState  = RCC_REQUEST_ERROR;
    rcc_SystemClkSrc_t sysClkSrc = RCC_SYSTEM_CLOCK_SOURCE_MSIS;

    if( RCC_OSC_CNT <= oscId )
    {
        retState = RCC_REQUEST_ERROR;
    }
    else if( RCC_NULL_PTR == rcc_OscCtrlLut[ oscId ].SetDiv )
    {
        /* Oscillator without divider */
        if( RCC_OSC_DIV_NONE == oscDiv )
        {
            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        retState = Rcc_ClkBus_Get_SysClkSource( &sysClkSrc );

        if( ( RCC_REQUEST_OK               == retState  ) &&
            ( RCC_OSC_MSIS                 == oscId     ) &&
            ( RCC_SYSTEM_CLOCK_SOURCE_MSIS == sysClkSrc )    )
        {
            /* Flash latency for the maximal MSIS frequency until the new frequency is known */
            retState = Rcc_Set_FlashLatencyRaise( MSIRangeTable[ 0u ] );

            if( RCC_REQUEST_OK == retState )
            {
                retState = rcc_OscCtrlLut[ oscId ].SetDiv( oscDiv );
            }
            else
            {
                /* Flash latency could not be raised */
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Set_FlashLatencyExact( RCC_FLASH_LATENCY_0_WS );
            }
            else
            {
                /* Divider not applied */
            }
        }
        else if( RCC_REQUEST_OK == retState )
        {
            retState = rcc_OscCtrlLut[ oscId ].SetDiv( oscDiv );
        }
        else
        {
            /* System clock source is not known */
        }
    }

    return ( retState );
}


/**
 * \brief Reads divider of oscillator output.
 *
 * \param oscId   [in]: Oscillator identification, value from \ref rcc_OscId_t
 * \param oscDiv [out]: Pointer to store divider value (1 for oscillators without divider). Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_OscDiv( rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_OSC_CNT   > oscId  ) &&
        ( RCC_NULL_PTR != oscDiv )    )
    {
        if( RCC_NULL_PTR != rcc_OscCtrlLut[ oscId ].GetDiv )
        {
            retState = rcc_OscCtrlLut[ oscId ].GetDiv( oscDiv );
        }
        else
        {
            *oscDiv  = RCC_OSC_DIV_NONE;
            retState = RCC_REQUEST_OK;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*---------------------------- Power supply validity -------------------------*/

/**
 * \brief Validates a power supply (software confirms that the supply is present).
 *
 * The independent supply domain (VDDUSB - USB, VDDIO2 - PG[15:2], VDDA - analog
 * peripherals) is electrically and logically connected to the core after the
 * validation (PWR_SVMCR). The supply shall be validated only if it is present on
 * the supply pin. \ref Rcc_Set_PeriphActive validates the supply of the activated
 * peripheral automatically.
 *
 * \param supplyId [in]: Supply identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PwrSupplyActive( rcc_PwrSupplyId_t supplyId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PWR_SUPPLY_CNT > supplyId )
    {
        LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_PWR );
        rcc_PwrSupplyLut[ supplyId ].EnableFunc();

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t supplyState = rcc_PwrSupplyLut[ supplyId ].IsEnabledFunc();

            if( RCC_FLAG_CLEARED != supplyState )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Supply validity bit not set yet */
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
 * \brief Invalidates a power supply (isolates the supply domain).
 *
 * \param supplyId [in]: Supply identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PwrSupplyInactive( rcc_PwrSupplyId_t supplyId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PWR_SUPPLY_CNT > supplyId )
    {
        LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_PWR );
        rcc_PwrSupplyLut[ supplyId ].DisableFunc();

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t supplyState = rcc_PwrSupplyLut[ supplyId ].IsEnabledFunc();

            if( RCC_FLAG_CLEARED == supplyState )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Supply validity bit not cleared yet */
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
 * \brief Reads validity state of a power supply.
 *
 * \param supplyId  [in]: Supply identification
 * \param retState [out]: Validity state (active - supply is validated)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PwrSupplyState( rcc_PwrSupplyId_t supplyId, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t reqState = RCC_REQUEST_ERROR;

    if( ( RCC_PWR_SUPPLY_CNT > supplyId ) &&
        ( RCC_NULL_PTR      != retState )    )
    {
        LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_PWR );

        const uint32_t supplyState = rcc_PwrSupplyLut[ supplyId ].IsEnabledFunc();

        if( RCC_FLAG_CLEARED != supplyState )
        {
            *retState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *retState = RCC_FUNCTION_INACTIVE;
        }

        reqState = RCC_REQUEST_OK;
    }
    else
    {
        reqState = RCC_REQUEST_ERROR;
    }

    return ( reqState );
}

/*------------------------ HSI48 automatic trimming (CRS) --------------------*/

/**
 * \brief Activates automatic trimming of the HSI48 oscillator by the clock recovery system (CRS).
 *
 * The CRS counts the HSI48 periods between two synchronization events, compares the count
 * with the expected one (48 MHz) and trims the oscillator, so the clock keeps the accuracy
 * required by USB (+-0.25 %). The CRS clock and the HSI48 oscillator are activated, the
 * synchronization is configured for the target frequency 48 MHz (reload value, frequency error
 * limit, no synchronization divider, rising edge, middle of the trimming range as the start
 * value) and the automatic trimming with the frequency error counter is started.
 *
 * \note  \ref RCC_HSI48_TRIM_SRC_USB_SOF requires the USB controller to receive the start of
 *        frame (the device connected to a host). The trimming does not change the oscillator
 *        until the first synchronization event arrives.
 *
 * \param trimSource [in]: Synchronization source (USB start of frame, LSE)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_Hsi48TrimActive( rcc_Hsi48TrimSrc_t trimSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_HSI48_TRIM_SRC_CNT > trimSource )
    {
        const uint32_t llSource    = rcc_Hsi48TrimLut[ trimSource ].LlSource;
        const uint32_t reloadValue = ( RCC_HSI48_TARGET_HZ / rcc_Hsi48TrimLut[ trimSource ].SyncFreq ) - 1u;

        /* The clock recovery system and the trimmed oscillator have to run */
        retState = Rcc_Set_PeriphActive( RCC_PERIPH_CRS );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_OscActive( RCC_OSC_HSI48 );
        }
        else
        {
            /* CRS clock could not be activated */
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Configuration is written with the trimming stopped */
            LL_CRS_DisableAutoTrimming();
            LL_CRS_DisableFreqErrorCounter();

            LL_CRS_SetSyncDivider( LL_CRS_SYNC_DIV_1 );
            LL_CRS_SetSyncSignalSource( llSource );
            LL_CRS_SetSyncPolarity( LL_CRS_SYNC_POLARITY_RISING );
            LL_CRS_SetReloadCounter( reloadValue );
            LL_CRS_SetFreqErrorLimit( LL_CRS_ERRORLIMIT_DEFAULT );
            LL_CRS_SetHSI48SmoothTrimming( LL_CRS_HSI48CALIBRATION_DEFAULT );

            LL_CRS_EnableAutoTrimming();
            LL_CRS_EnableFreqErrorCounter();

            retState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t sourceValue  = LL_CRS_GetSyncSignalSource();
                const uint32_t reloadRead   = LL_CRS_GetReloadCounter();
                const uint32_t autoTrimOn   = LL_CRS_IsEnabledAutoTrimming();
                const uint32_t errCounterOn = LL_CRS_IsEnabledFreqErrorCounter();

                if( ( llSource         == sourceValue  ) &&
                    ( reloadValue      == reloadRead   ) &&
                    ( RCC_FLAG_CLEARED != autoTrimOn   ) &&
                    ( RCC_FLAG_CLEARED != errCounterOn )    )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Configuration not applied yet */
                }
            }
        }
        else
        {
            /* CRS clock or HSI48 oscillator could not be started */
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Deactivates automatic trimming of the HSI48 oscillator and the CRS clock.
 *
 * The oscillator keeps the last trim value.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_Hsi48TrimInactive( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    LL_CRS_DisableAutoTrimming();
    LL_CRS_DisableFreqErrorCounter();

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t autoTrimOn   = LL_CRS_IsEnabledAutoTrimming();
        const uint32_t errCounterOn = LL_CRS_IsEnabledFreqErrorCounter();

        if( ( RCC_FLAG_CLEARED == autoTrimOn   ) &&
            ( RCC_FLAG_CLEARED == errCounterOn )    )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Trimming not stopped yet */
        }
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Set_PeriphInactive( RCC_PERIPH_CRS );
    }
    else
    {
        /* Trimming could not be stopped - CRS clock is kept */
    }

    return ( retState );
}


/**
 * \brief Reads state of the automatic trimming of the HSI48 oscillator.
 *
 * \param retState [out]: Active if the CRS clock is enabled and both the automatic trimming and
 *                        the frequency error counter are enabled
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_Hsi48TrimState( rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t reqState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != retState )
    {
        rcc_FunctionState_t crsClockState = RCC_FUNCTION_INACTIVE;

        reqState = Rcc_Get_PeriphState( RCC_PERIPH_CRS, &crsClockState );

        *retState = RCC_FUNCTION_INACTIVE;

        if( ( RCC_REQUEST_OK      == reqState      ) &&
            ( RCC_FUNCTION_ACTIVE == crsClockState )    )
        {
            const uint32_t autoTrimOn   = LL_CRS_IsEnabledAutoTrimming();
            const uint32_t errCounterOn = LL_CRS_IsEnabledFreqErrorCounter();

            if( ( RCC_FLAG_CLEARED != autoTrimOn   ) &&
                ( RCC_FLAG_CLEARED != errCounterOn )    )
            {
                *retState = RCC_FUNCTION_ACTIVE;
            }
            else
            {
                /* Trimming not running */
            }
        }
        else
        {
            /* CRS is not clocked - trimming can not run */
        }
    }
    else
    {
        reqState = RCC_REQUEST_ERROR;
    }

    return ( reqState );
}

/*-------------------------- RTC clock configuration -------------------------*/

/**
 * \brief Selects Real Time Clock (RTC) clock source.
 *
 * \param clkSource [in]: RTC clock source, value from \ref rcc_Rtc_ClkSource_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_RtcClkSource( rcc_Rtc_ClkSource_t clkSource )
{
    return ( Rcc_Pll_Set_RtcClkSource( clkSource ) );
}


/**
 * \brief Reads Real Time Clock (RTC) clock source.
 *
 * \param clkSource [out]: Pointer to store RTC clock source. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_RtcClkSource( rcc_Rtc_ClkSource_t * const clkSource )
{
    return ( Rcc_Pll_Get_RtcClkSource( clkSource ) );
}

/*----------------------- Clock outputs configuration ------------------------*/

/**
 * \brief Function used to set clock output signal source.
 *
 * \param outId     [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkSource [in]: Clock output signal source, value from \ref rcc_ClkOut_Source_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ClkOutSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource )
{
    return Rcc_ClkOut_Set_ClockSource( outId, clkSource );
}


/**
 * \brief Function used to get clock output signal source.
 *
 * \param outId      [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkSource [out]: Pointer to store clock output source. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkOutSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource )
{
    return Rcc_ClkOut_Get_ClockSource( outId, clkSource );
}


/**
 * \brief Function used to set clock output divider.
 *
 * \param outId      [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkDivider [in]: Clock output divider, see \ref rcc_ClkOut_Div_t
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ClkOutDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider )
{
    return Rcc_ClkOut_Set_ClockDivider( outId, clkDivider );
}


/**
 * \brief Function used to get clock output divider.
 *
 * \param outId       [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkDivider [out]: Pointer to store clock output divider. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkOutDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider )
{
    return Rcc_ClkOut_Get_ClockDivider( outId, clkDivider );
}

/*--------------------------- Reset source flags -----------------------------*/

/**
 * \brief Returns state of required reset source flag.
 *
 * \note  Several flags can be active at once (e.g. NRST pin flag is set
 *        together with any internal reset source). Flags are kept until
 *        \ref Rcc_Set_ResetSourceClear is called or power-on reset occurs.
 *
 * \param resetSrc   [in]: Reset source identification, value from \ref rcc_ResetSrc_t
 * \param flagState [out]: Pointer to store reset source flag state. Must not be NULL.
 *
 * \return Function processing state. Returns \ref RCC_REQUEST_OK if request
 *         was processed without problems. Otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Get_ResetSource( rcc_ResetSrc_t resetSrc, rcc_FlagState_t * const flagState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_RESET_SRC_CNT > resetSrc  ) &&
        ( RCC_NULL_PTR     != flagState )    )
    {
        const uint32_t regValue = Rcc_Get_RegBit( RCC_REG_CSR, rcc_ResetSrcLut[ resetSrc ] );

        if( RCC_CSR_BITS_CLEARED != regValue )
        {
            *flagState = RCC_FLAG_ACTIVE;
        }
        else
        {
            *flagState = RCC_FLAG_INACTIVE;
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
 * \brief Clears all reset source flags.
 *
 * \note  Remove flag (RMVF) is set to clear the flags and cleared back
 *        afterwards so the following reset sources are latched again.
 *        Both writes are verified by read-back.
 *
 * \return Function processing state. Returns \ref RCC_REQUEST_OK if request
 *         was processed without problems. Otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Set_ResetSourceClear( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    /* Request removal of all reset source flags */
    Rcc_Set_RegVal( RCC_REG_CSR, RCC_CSR_RMVF, RCC_CSR_RMVF );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = Rcc_Get_RegBit( RCC_REG_CSR, RCC_CSR_RESET_SRC_MASK );

        if( RCC_CSR_BITS_CLEARED == regValue )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Flags have not been cleared yet */
            retState = RCC_REQUEST_ERROR;
        }
    }

    /* Release remove flag, so next reset sources are latched */
    Rcc_Set_RegVal( RCC_REG_CSR, RCC_CSR_RMVF, RCC_CSR_BITS_CLEARED );

    if( RCC_REQUEST_OK == retState )
    {
        retState = RCC_REQUEST_ERROR;

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            const uint32_t regValue = Rcc_Get_RegBit( RCC_REG_CSR, RCC_CSR_RMVF );

            if( RCC_CSR_BITS_CLEARED == regValue )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Remove flag has not been released yet */
                retState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        /* Flags were not cleared, error is returned */
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Starts internal oscillator used as peripheral kernel clock source.
 *
 * Clock sources HSI16, HSI48 (and HSI48 / 2), MSIS, MSIK and LSI are mapped
 * to internal oscillators, oscillator which is not running is activated. Other
 * clock sources (buses, PLL outputs, HSE, LSE) are not handled.
 *
 * \param clkSrcId [in]: Kernel clock source of the peripheral (valid)
 *
 * \return Returns "OK" if the clock source is not an internal oscillator or the
 *         oscillator is running. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_ClkSrcOscActive( rcc_ClkSrcId_t clkSrcId )
{
    rcc_RequestState_t  retState = RCC_REQUEST_OK;
    rcc_FunctionState_t oscState = RCC_FUNCTION_INACTIVE;
    const rcc_OscId_t   oscId    = rcc_ClkSrcOscLut[ clkSrcId ];

    if( RCC_OSC_CNT > oscId )
    {
        retState = Rcc_Get_OscState( oscId, &oscState );

        if( ( RCC_REQUEST_OK        == retState ) &&
            ( RCC_FUNCTION_INACTIVE == oscState )    )
        {
            retState = Rcc_Set_OscActive( oscId );
        }
        else
        {
            /* Oscillator is already running or state read failed */
        }
    }
    else
    {
        /* Clock source is not an internal oscillator */
    }

    return ( retState );
}


/**
 * \brief Starts oscillator of the system clock source (MSIS, HSI16) if it is not running.
 *
 * \param systemClkSource [in]: System clock source
 *
 * \return Returns "OK" if the source is running or is not an internal
 *         oscillator (HSE configured by \ref Rcc_Init, PLL), otherwise error.
 */
static rcc_RequestState_t Rcc_Set_SysClkOscActive( rcc_SystemClkSrc_t systemClkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_SYSTEM_CLOCK_SOURCE_MSIS == systemClkSource )
    {
        retState = Rcc_Set_ClkSrcOscActive( RCC_CLK_SRC_MSISCLK );
    }
    else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == systemClkSource )
    {
        retState = Rcc_Set_ClkSrcOscActive( RCC_CLK_SRC_HSI16CLK );
    }
    else if( RCC_SYSTEM_CLOCK_SOURCE_CNT > systemClkSource )
    {
        /* HSE or PLL - already configured */
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Validates independent supply of the peripheral block in PWR (if the block has one).
 *
 * PWR bus clock is enabled before the supply valid bit is written.
 *
 * \param blockId [in]: Peripheral block (valid)
 *
 * \return Returns "OK" if the block has no independent supply or the supply
 *         valid bit is set, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_SupplyValid( rcc_BlockList_t blockId )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( uint32_t lutIdx = 0u; RCC_SUPPLY_LIST_CNT > lutIdx; lutIdx++ )
    {
        if( blockId == rcc_SupplyLut[ lutIdx ].BlockId )
        {
            LL_AHB3_GRP1_EnableClock( LL_AHB3_GRP1_PERIPH_PWR );
            rcc_SupplyLut[ lutIdx ].EnableFunc();

            retState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t supplyValid = rcc_SupplyLut[ lutIdx ].IsEnabledFunc();

                if( RCC_FLAG_CLEARED != supplyValid )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Supply valid bit has not been written yet */
                }
            }
            break;
        }
        else
        {
            /* Continue with next block */
        }
    }

    return ( retState );
}


/**
 * \brief Checks if the kernel clock multiplexer of the peripheral is shared with another
 *        enabled peripheral block (e.g. ADCDACSEL of ADC1 / ADC2, ADC4 and DAC1).
 *
 * Records of one multiplexer field report the same selected record by
 * Rcc_ClkMux_Get_ClkSrc(), records of other blocks with the same selected record use the
 * same field.
 *
 * \param periphId [in]: ID of the peripheral (valid, with clock multiplexer)
 *
 * \return Returns RCC_FUNCTION_ACTIVE if another enabled peripheral block uses the same
 *         multiplexer field (or the selection can not be read). Otherwise returns
 *         RCC_FUNCTION_INACTIVE.
 */
static rcc_FunctionState_t Rcc_Get_ClkMuxShared( rcc_PeriphId_t periphId )
{
    rcc_FunctionState_t      sharedState = RCC_FUNCTION_ACTIVE;
    rcc_ClkMuxId_t           ownSelected = RCC_CLK_MUX_LIST_CNT;
    const rcc_BlockList_t    ownBlockId  = rcc_ConfigStruct[ periphId ].BlockId;
    const rcc_RequestState_t ownState    = Rcc_ClkMux_Get_ClkSrc( rcc_ConfigStruct[ periphId ].ClkMuxId, &ownSelected );

    if( RCC_REQUEST_OK == ownState )
    {
        sharedState = RCC_FUNCTION_INACTIVE;

        for( uint32_t rowIdx = 0u; RCC_PERIPH_ID_CNT > rowIdx; rowIdx ++ )
        {
            const rcc_PeriphConfigStruct_t * const row           = &rcc_ConfigStruct[ rowIdx ];
            rcc_ClkMuxId_t                         otherSelected = RCC_CLK_MUX_LIST_CNT;
            rcc_FunctionState_t                    otherState    = RCC_FUNCTION_INACTIVE;
            rcc_RequestState_t                     otherMuxState = RCC_REQUEST_ERROR;
            rcc_RequestState_t                     otherClkState = RCC_REQUEST_ERROR;

            if( ( ownBlockId          != row->BlockId  ) &&
                ( RCC_CLK_MUX_LIST_CNT > row->ClkMuxId )    )
            {
                otherMuxState = Rcc_ClkMux_Get_ClkSrc( row->ClkMuxId, &otherSelected );
                otherClkState = Rcc_Get_PeriphState( (rcc_PeriphId_t)rowIdx, &otherState );
            }
            else
            {
                /* Record of the own block or peripheral without multiplexer */
            }

            if( ( RCC_REQUEST_OK      == otherMuxState ) &&
                ( ownSelected         == otherSelected ) &&
                ( RCC_REQUEST_OK      == otherClkState ) &&
                ( RCC_FUNCTION_ACTIVE == otherState    )    )
            {
                sharedState = RCC_FUNCTION_ACTIVE;
                break;
            }
            else
            {
                /* Other field or disabled block */
            }
        }
    }
    else
    {
        /* Selection unknown - multiplexer is kept */
    }

    return ( sharedState );
}


/**
 * \brief Calculates number of flash wait states for processor clock in actual voltage range.
 *
 * \param hclkFreq   [in]: Processor clock (HCLK) frequency in Hz
 * \param minLatency [in]: Minimal number of wait states
 * \param latency   [out]: Pointer to store number of wait states
 *
 * \return Returns "OK" if the frequency is within the voltage range, otherwise error.
 */
static rcc_RequestState_t Rcc_Get_FlashLatencyFreq( rcc_FreqHz_t hclkFreq, rcc_FlashLatency_t minLatency, rcc_FlashLatency_t * const latency )
{
    rcc_RequestState_t retState     = RCC_REQUEST_ERROR;
    const uint32_t     voltageScale = LL_PWR_GetRegulVoltageScaling();

    for( uint32_t lutIdx = 0u; RCC_PWR_VOLTAGE_SCALE_CNT > lutIdx; lutIdx++ )
    {
        const rcc_FlashLatencyConfig_t * const scaleConfig = &rcc_FlashLatencyLut[ lutIdx ];

        if( voltageScale == scaleConfig->VoltageScale )
        {
            for( uint32_t waitStates = 0u; scaleConfig->ThresholdCnt > waitStates; waitStates++ )
            {
                if( hclkFreq <= scaleConfig->MaxFreqHz[ waitStates ] )
                {
                    *latency = (rcc_FlashLatency_t)rcc_FlashLatencyValueLut[ waitStates ];
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Continue with next number of wait states */
                }
            }
            break;
        }
        else
        {
            /* Continue with next voltage range */
        }
    }

    if( ( RCC_REQUEST_OK == retState   ) &&
        ( minLatency      > *latency   )    )
    {
        *latency = minLatency;
    }
    else
    {
        /* Calculated latency is used */
    }

    return ( retState );
}


/**
 * \brief Writes number of flash wait states (verified by read-back).
 *
 * \param latency [in]: Number of wait states (LL value)
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_FlashLatencyValue( rcc_FlashLatency_t latency )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    LL_FLASH_SetLatency( latency );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = LL_FLASH_GetLatency();

        if( (uint32_t)latency == regValue )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Latency has not yet been changed, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return ( retState );
}


/**
 * \brief Raises number of flash wait states for given processor clock (never lowers it).
 *
 * \param hclkFreq [in]: Processor clock frequency in Hz which will be used
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_FlashLatencyRaise( rcc_FreqHz_t hclkFreq )
{
    rcc_RequestState_t retState      = RCC_REQUEST_ERROR;
    rcc_FlashLatency_t actualLatency = (rcc_FlashLatency_t)LL_FLASH_GetLatency();
    rcc_FlashLatency_t latency       = RCC_FLASH_LATENCY_0_WS;

    retState = Rcc_Get_FlashLatencyFreq( hclkFreq, actualLatency, &latency );

    if( ( RCC_REQUEST_OK == retState      ) &&
        ( actualLatency  != latency       )    )
    {
        retState = Rcc_Set_FlashLatencyValue( latency );
    }
    else
    {
        /* Actual latency is sufficient or frequency out of voltage range */
    }

    return ( retState );
}


/**
 * \brief Sets number of flash wait states for actual processor clock.
 *
 * CMSIS SystemCoreClock is updated.
 *
 * \param minLatency [in]: Minimal number of wait states
 *
 * \return Returns "OK" if request was success, otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_FlashLatencyExact( rcc_FlashLatency_t minLatency )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hclkFreq = 0u;
    rcc_FlashLatency_t latency  = RCC_FLASH_LATENCY_0_WS;

    retState = Rcc_ClkBus_Get_AHBClk( &hclkFreq );

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Get_FlashLatencyFreq( hclkFreq, minLatency, &latency );
    }
    else
    {
        /* Processor clock is not available */
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Set_FlashLatencyValue( latency );
    }
    else
    {
        /* Latency can not be calculated */
    }

    if( RCC_REQUEST_OK == retState )
    {
        LL_SetSystemCoreClock( hclkFreq );
    }
    else
    {
        /* Error during configuration */
    }

    return ( retState );
}


/**
 * \brief Function used to wrap PLL1 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_1_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutR( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL1 clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_1_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutQ( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL1 clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_1_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutP( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL2 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_2_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutR( RCC_PLL_2, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL2 clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_2_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutQ( RCC_PLL_2, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL2 clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_2_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutP( RCC_PLL_2, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL3 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_3_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutR( RCC_PLL_3, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL3 clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_3_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutQ( RCC_PLL_3, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL3 clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_3_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutP( RCC_PLL_3, clkFreq ) );
}


/**
 * \brief Function used for system clock calculation from configuration setting
 *
 * This Function don't use register as reference for calculation of system
 * frequency. Instead of that, use configuration from configuration structure
 * (MSIS frequency is taken from actual MSIS range).
 *
 * \param clockConfig [in]: Configuration structure
 * \param sysClk     [out]: Pointer to store system clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const sysClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllSrcFreq = 0u;

    if( ( RCC_NULL_PTR != clockConfig ) &&
        ( RCC_NULL_PTR != sysClk      )    )
    {
        const rcc_PllConfigStruct_t * const pll1Config = &clockConfig->Pll_Config[ RCC_PLL_1 ];

        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource )
        {
            if( RCC_PLL_SRC_MSIS == pll1Config->Pll_Source )
            {
                retState = Rcc_ClkSrc_Get_MsisClk( &pllSrcFreq );
            }
            else if( RCC_PLL_SRC_HSI == pll1Config->Pll_Source )
            {
                retState = Rcc_ClkSrc_Get_Hsi16Clk( &pllSrcFreq );
            }
            else if( RCC_PLL_SRC_HSE == pll1Config->Pll_Source )
            {
                pllSrcFreq = clockConfig->HSE_Frequency_Hz;
                retState   = RCC_REQUEST_OK;
            }
            else
            {
                /* PLL1 without source can not drive system clock */
                retState = RCC_REQUEST_ERROR;
            }

            if( ( RCC_REQUEST_OK == retState              ) &&
                ( 0u             != pll1Config->M_Divider ) &&
                ( 0u             != pll1Config->R_Divider )    )
            {
                *sysClk = ( ( pllSrcFreq / pll1Config->M_Divider ) * pll1Config->N_Multiplier ) / pll1Config->R_Divider;
            }
            else
            {
                /* Reached error state */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_MSIS == clockConfig->SystemClockSource )
        {
            retState = Rcc_ClkSrc_Get_MsisClk( sysClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == clockConfig->SystemClockSource )
        {
            retState = Rcc_ClkSrc_Get_Hsi16Clk( sysClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == clockConfig->SystemClockSource )
        {
            *sysClk  = clockConfig->HSE_Frequency_Hz;
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

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
