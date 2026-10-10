/**
 * \author Mr.Nobody
 * \file Rcc.c
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module common functionality
 *
 * \note  Exception of MCAL layering rule: PWR (supply configuration, voltage
 *        scaling), SYSCFG (VOS0 overdrive), FLASH (latency, programming delay
 *        of STM32H7R / H7S), FMC (bank 1 reset state) and AXI interconnect
 *        (device errata) have no MCAL module. Their
 *        configuration is part of the clock configuration sequence, therefore
 *        RCC accesses their registers / LL functions directly (\ref Rcc_Init,
 *        \ref Rcc_Set_PwrRange, \ref Rcc_Set_FlashLatency). New accesses shall
 *        be moved into a dedicated MCAL module once it exists.
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
#include "Stm32_pwr.h"                      /* PWR RAL functionality          */
#include "Stm32_system.h"                   /* SYSCFG RAL functionality       */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define RCC_MAJOR_VERSION                       ( 1u )
/** Value of minor version of SW module */
#define RCC_MINOR_VERSION                       ( 0u )
/** Value of patch version of SW module */
#define RCC_PATCH_VERSION                       ( 0u )


/** Maximal wait time for configuration request confirmation */
#define RCC_TIMEOUT_RAW                         ( 0x84FCB )

#define RCC_UNSUPPORTED_FUNCTION                ( 0xFF )

/* Reset source flags of the CPU running the code (Cortex-M7 on dual-core devices) */
#if defined(RCC_RSR_SFT1RSTF)
/** Software reset flag */
#define RCC_RSR_SW_RESET_FLAG                   ( RCC_RSR_SFT1RSTF )
#else
/** Software reset flag */
#define RCC_RSR_SW_RESET_FLAG                   ( RCC_RSR_SFTRSTF )
#endif

#if defined(RCC_RSR_LPWR1RSTF)
/** Illegal Stop / Standby mode entry reset flag */
#define RCC_RSR_LPWR_RESET_FLAG                 ( RCC_RSR_LPWR1RSTF )
#else
/** Illegal Stop / Standby mode entry reset flag */
#define RCC_RSR_LPWR_RESET_FLAG                 ( RCC_RSR_LPWRRSTF )
#endif

#if defined(RCC_RSR_IWDG1RSTF)
/** Independent watchdog reset flag */
#define RCC_RSR_IWDG_RESET_FLAG                 ( RCC_RSR_IWDG1RSTF )
/** Window watchdog reset flag */
#define RCC_RSR_WWDG_RESET_FLAG                 ( RCC_RSR_WWDG1RSTF )
#else
/** Independent watchdog reset flag (STM32H7R / H7S) */
#define RCC_RSR_IWDG_RESET_FLAG                 ( RCC_RSR_IWDGRSTF )
/** Window watchdog reset flag (STM32H7R / H7S) */
#define RCC_RSR_WWDG_RESET_FLAG                 ( RCC_RSR_WWDGRSTF )
#endif

/** Mask of all reset source flags in RCC reset status register (RSR) */
#define RCC_RSR_RESET_SRC_MASK                  ( RCC_RSR_PINRSTF          | RCC_RSR_BORRSTF          | RCC_RSR_SW_RESET_FLAG | \
                                                  RCC_RSR_IWDG_RESET_FLAG  | RCC_RSR_WWDG_RESET_FLAG  | RCC_RSR_LPWR_RESET_FLAG )

#if defined(RCC_CR_HSECSSON)
/** Clock security system of HSE enable bit (STM32H7R / H7S) */
#define RCC_CR_HSE_CSS_ON                       ( RCC_CR_HSECSSON )
#else
/** Clock security system of HSE enable bit */
#define RCC_CR_HSE_CSS_ON                       ( RCC_CR_CSSHSEON )
#endif

#if defined(STM32H7RS)
/** HSI divider 1 (STM32H7R / H7S LL name) */
#define RCC_HSI_DIV_1                           ( LL_RCC_HSI_DIV_1 )
/** HSI divider 2 (STM32H7R / H7S LL name) */
#define RCC_HSI_DIV_2                           ( LL_RCC_HSI_DIV_2 )
/** HSI divider 4 (STM32H7R / H7S LL name) */
#define RCC_HSI_DIV_4                           ( LL_RCC_HSI_DIV_4 )
/** HSI divider 8 (STM32H7R / H7S LL name) */
#define RCC_HSI_DIV_8                           ( LL_RCC_HSI_DIV_8 )
#else
/** HSI divider 1 */
#define RCC_HSI_DIV_1                           ( LL_RCC_HSI_DIV1 )
/** HSI divider 2 */
#define RCC_HSI_DIV_2                           ( LL_RCC_HSI_DIV2 )
/** HSI divider 4 */
#define RCC_HSI_DIV_4                           ( LL_RCC_HSI_DIV4 )
/** HSI divider 8 */
#define RCC_HSI_DIV_8                           ( LL_RCC_HSI_DIV8 )
#endif

/** Value of RSR register bits in cleared state */
#define RCC_RSR_BITS_CLEARED                    ( 0u )

/** Default HSE frequency used by \ref Rcc_Get_DefaultConfig (ST-LINK MCO of Nucleo boards) */
#define RCC_DEFAULT_HSE_FREQ_HZ                 ( 8000000u )

/** Default PLL input divider - HSI 64 MHz / 4 gives 16 MHz reference frequency */
#define RCC_DEFAULT_PLL_M_DIV                   ( 4u )

/** Default PLL multiplier - VCO 400 MHz in wide VCO range of all lines */
#define RCC_DEFAULT_PLL_N_MULT                  ( 25u )

/** Default PLL output dividers (P, Q, R) */
#define RCC_DEFAULT_PLL_OUT_DIV                 ( 2u )

/** Default SysTick interval in ms */
#define RCC_DEFAULT_SYSTICK_INTERVAL_MS         ( 1u )

/** Default clock output divider (not divided) */
#define RCC_DEFAULT_CLK_OUT_DIV                 ( 1u )

#if defined(STM32H7RS)
/** Default PLL output dividers S, T - outputs not used (STM32H7R / H7S) */
#define RCC_DEFAULT_PLL_OUT_ST_DIV              ( 0u )

/** Default voltage scale - VOS low (reset value of STM32H7R / H7S) */
#define RCC_DEFAULT_VOLTAGE_SCALE               ( RCC_PWR_VOLTAGE_SCALE_1 )
#else
/** Default voltage scale - VOS3 (reset value) */
#define RCC_DEFAULT_VOLTAGE_SCALE               ( RCC_PWR_VOLTAGE_SCALE_3 )
#endif

/** System clock after reset - HSI 64 MHz (HSIDIV reset value 1), updated by \ref Rcc_Init */
#define RCC_SYSCLK_RESET_FREQ_HZ                ( 64000000u )

/** Count of milliseconds in one second */
#define RCC_MS_IN_SECOND                        ( 1000u )

/** Minimum SysTick ticks count per interval (reload register value 1) */
#define RCC_SYSTICK_TICKS_MIN                   ( 2u )

/** Maximum SysTick ticks count per interval (24-bit reload register + 1) */
#define RCC_SYSTICK_TICKS_MAX                   ( SysTick_LOAD_RELOAD_Msk + 1u )

/** SysTick reload register holds ticks count decremented by 1 */
#define RCC_SYSTICK_RELOAD_OFFSET               ( 1u )

#if defined(STM32H7RS)
/** Count of flash wait states in the latency table (0 - 7 WS) */
#define RCC_FLASH_WS_TABLE_SIZE                 ( 8u )

/** Flash latency used during the clock switch - 7 WS, valid for every clock
 *  frequency and voltage scale of STM32H7R / H7S (reset value is 3 WS) */
#define RCC_FLASH_LATENCY_SWITCH                ( RCC_FLASH_LATENCY_7_WS )

/** Flash latency and programming delay fields written together (STM32H7R / H7S) */
#define RCC_FLASH_ACR_TIMING_MASK               ( FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ )

/** Supply configuration register (STM32H7R / H7S) */
#define RCC_PWR_SUPPLY_REG                      ( PWR->CSR2 )

/** Supply configuration bits of the reset state - supply not configured yet */
#define RCC_PWR_SUPPLY_RESET_STATE              ( PWR_CSR2_SDEN | PWR_CSR2_LDOEN )

/** Supply source bits of PWR_CSR2 used to detect configured supply */
#define RCC_PWR_SUPPLY_SOURCE_MASK              ( PWR_CSR2_SDEN | PWR_CSR2_LDOEN | PWR_CSR2_BYPASS )

/** Active voltage scale register (STM32H7R / H7S) */
#define RCC_PWR_ACTVOS_REG                      ( PWR->SR1 )

/** Active voltage scale field - same position as VOS field of PWR_CSR4 */
#define RCC_PWR_ACTVOS_MASK                     ( PWR_SR1_ACTVOS )

_Static_assert( PWR_SR1_ACTVOS_Pos == PWR_CSR4_VOS_Pos, "Rcc: ACTVOS and VOS fields must have the same position." );
#else
/** Count of flash wait states in the latency table (0 - 6 WS) */
#define RCC_FLASH_WS_TABLE_SIZE                 ( 7u )

/** Flash latency used during the clock switch - reset value, valid for every
 *  clock frequency and voltage scale */
#define RCC_FLASH_LATENCY_SWITCH                ( FLASH_ACR_LATENCY_7WS )

/** Supply configuration register */
#define RCC_PWR_SUPPLY_REG                      ( PWR->CR3 )

/** Supply configuration bits of the reset state (SMPS devices) - supply not configured yet */
#define RCC_PWR_SUPPLY_RESET_STATE              ( PWR_CR3_SMPSEN | PWR_CR3_LDOEN )

/** Supply source bits of PWR_CR3 used to detect configured supply (SMPS devices) */
#define RCC_PWR_SUPPLY_SOURCE_MASK              ( PWR_CR3_SMPSEN | PWR_CR3_LDOEN | PWR_CR3_BYPASS )

/** Active voltage scale register */
#define RCC_PWR_ACTVOS_REG                      ( PWR->CSR1 )

/** Active voltage scale field */
#define RCC_PWR_ACTVOS_MASK                     ( PWR_CSR1_ACTVOS )
#endif

/** Active voltage scale mask used when any active voltage scale is accepted */
#define RCC_PWR_ACTVOS_ANY                      ( 0u )

/** Marker of supply configuration not available on the device */
#define RCC_PWR_SUPPLY_UNSUPPORTED              ( 0xFFFFFFFFu )

/** Marker of voltage scale not available on the device */
#define RCC_PWR_VOS_UNSUPPORTED                 ( 0xFFFFFFFFu )

/** Device revision ID (DBGMCU IDCODE REV_ID field) of the first revision without AXI SRAM errata (rev V) */
#define RCC_DEV_REV_ID_V                        ( 0x20000000u )

/** Address of AXI interconnect register AXI_TARG7_FN_MOD (AXI SRAM target issuing capability) */
#define RCC_AXI_TARG7_FN_MOD_ADDR               ( 0x51008108u )

/** AXI_TARG7_FN_MOD value - read issuing capability of the AXI SRAM target reduced to 1 */
#define RCC_AXI_TARG7_READ_ISS_1                ( 0x00000001u )

/** FMC bank 1 control register value with the bank disabled (speculative accesses blocked) */
#define RCC_FMC_BCR1_DISABLED                   ( 0x000030D2u )

/* ============================== TYPEDEFS ================================== */

/**
 * \brief Clock tree configuration structure
 */
typedef struct __attribute__((packed))
{
    rcc_BlockList_t BlockId;     /**< Peripheral block ID                    */
    rcc_ClkBusId_t  ClkBusId;    /**< Clock bus ID.                         */
    uint32_t const  StateMask;   /**< Peripheral activation state bit mask.  */
    uint32_t const  LpCtrlMask;  /**< Peripheral Low Power control bit mask. */
    uint32_t const  RstCtrlMask; /**< Peripheral Reset control bit mask.     */
}   rcc_BlockConfigStruct_t;


/**
 * \brief Clock tree configuration structure
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

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t  Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const sysClk );
static rcc_RequestState_t  Rcc_Get_ExpectedHclkFrequency  ( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const hclkFreq );
static rcc_RequestState_t  Rcc_Set_ClkSrcOscActive        ( rcc_ClkSrcId_t clkSrcId );
static rcc_FunctionState_t Rcc_Get_ClkMuxShared           ( rcc_PeriphId_t periphId );

static void                Rcc_Set_DeviceWorkarounds      ( void );
static rcc_RequestState_t  Rcc_Set_PwrSupply              ( rcc_PwrSupply_t pwrSupply );
static rcc_RequestState_t  Rcc_Get_PwrVoltageReady        ( uint32_t activeScaleMask, uint32_t activeScale );
static uint32_t            Rcc_Get_PwrVoltageLevelReady   ( void );
static rcc_RequestState_t  Rcc_Get_PwrVoltageScale        ( rcc_PwrVoltageScale_t * const voltageScale );
static rcc_RequestState_t  Rcc_Get_FlashLatencyRequired   ( rcc_FreqHz_t hclkFreq, uint32_t * const latency );
static rcc_RequestState_t  Rcc_Set_FlashLatencyValue      ( uint32_t latency );
static rcc_RequestState_t  Rcc_Set_FlashLatencySwitch     ( void );
static rcc_RequestState_t  Rcc_Set_FlashLatencyActual     ( void );
static rcc_RequestState_t  Rcc_Set_ClkVariables           ( void );
static rcc_RequestState_t  Rcc_Set_AHBDividerSafe         ( rcc_AHB_Div_t dividerId );

static rcc_RequestState_t  Rcc_Pll_Get_1_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_1_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_1_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_2_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_2_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_2_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_3_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_3_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_3_PClk( rcc_FreqHz_t * const clkFreq );
#if defined(STM32H7RS)
static rcc_RequestState_t  Rcc_Pll_Get_1_SClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_2_SClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_2_TClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t  Rcc_Pll_Get_3_SClk( rcc_FreqHz_t * const clkFreq );
#endif


/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* --------------------- Multipliers/Dividers arrays -------------------------*/

/* The most disgusting part in whole project. Definition of external variables
 * created by STM (CMSIS system file is not used)!!! Shame on you ST! */
uint32_t      SystemCoreClock        = RCC_SYSCLK_RESET_FREQ_HZ;
#if !defined(STM32H7RS)
uint32_t      SystemD2Clock          = RCC_SYSCLK_RESET_FREQ_HZ;
const uint8_t D1CorePrescTable[16u]  = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
#endif

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Configuration array of registers used by peripheral buses */
const rcc_ClkBusConfigStruct_t         rcc_ClkBusConfigStruct[] =
{
  { .ClkBusId = RCC_CLK_BUS_AHB1  , .EnableRegId = RCC_REG_AHB1ENR , .SleepRegId = RCC_REG_AHB1LPENR , .ResetRegId = RCC_REG_AHB1RSTR  },
  { .ClkBusId = RCC_CLK_BUS_AHB2  , .EnableRegId = RCC_REG_AHB2ENR , .SleepRegId = RCC_REG_AHB2LPENR , .ResetRegId = RCC_REG_AHB2RSTR  },
  { .ClkBusId = RCC_CLK_BUS_AHB3  , .EnableRegId = RCC_REG_AHB3ENR , .SleepRegId = RCC_REG_AHB3LPENR , .ResetRegId = RCC_REG_AHB3RSTR  },
  { .ClkBusId = RCC_CLK_BUS_AHB4  , .EnableRegId = RCC_REG_AHB4ENR , .SleepRegId = RCC_REG_AHB4LPENR , .ResetRegId = RCC_REG_AHB4RSTR  },
#if defined(STM32H7RS)
  { .ClkBusId = RCC_CLK_BUS_AHB5  , .EnableRegId = RCC_REG_AHB5ENR , .SleepRegId = RCC_REG_AHB5LPENR , .ResetRegId = RCC_REG_AHB5RSTR  },
#endif
  { .ClkBusId = RCC_CLK_BUS_APB1_1, .EnableRegId = RCC_REG_APB1LENR, .SleepRegId = RCC_REG_APB1LLPENR, .ResetRegId = RCC_REG_APB1LRSTR },
  { .ClkBusId = RCC_CLK_BUS_APB1_2, .EnableRegId = RCC_REG_APB1HENR, .SleepRegId = RCC_REG_APB1HLPENR, .ResetRegId = RCC_REG_APB1HRSTR },
  { .ClkBusId = RCC_CLK_BUS_APB2  , .EnableRegId = RCC_REG_APB2ENR , .SleepRegId = RCC_REG_APB2LPENR , .ResetRegId = RCC_REG_APB2RSTR  },
#if defined(STM32H7RS)
  { .ClkBusId = RCC_CLK_BUS_APB4  , .EnableRegId = RCC_REG_APB4ENR , .SleepRegId = RCC_REG_APB4LPENR , .ResetRegId = RCC_REG_APB4RSTR  },
  { .ClkBusId = RCC_CLK_BUS_APB5  , .EnableRegId = RCC_REG_APB5ENR , .SleepRegId = RCC_REG_APB5LPENR , .ResetRegId = RCC_REG_APB5RSTR  }
#else
  { .ClkBusId = RCC_CLK_BUS_APB3  , .EnableRegId = RCC_REG_APB3ENR , .SleepRegId = RCC_REG_APB3LPENR , .ResetRegId = RCC_REG_APB3RSTR  },
  { .ClkBusId = RCC_CLK_BUS_APB4  , .EnableRegId = RCC_REG_APB4ENR , .SleepRegId = RCC_REG_APB4LPENR , .ResetRegId = RCC_REG_APB4RSTR  }
#endif
};

_Static_assert( (sizeof(rcc_ClkBusConfigStruct) / sizeof(rcc_ClkBusConfigStruct_t)) == RCC_CLK_BUS_CNT, "Rcc: rcc_ClkBusConfigStruct has incorrect size." );


const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK      , .ClkSrcCallback = Rcc_ClkBus_Get_SysClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_CPUCLK      , .ClkSrcCallback = Rcc_ClkBus_Get_CpuClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1RCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_RClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1QCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_QClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1PCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_PClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2RCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_RClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2QCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_QClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2PCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_PClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3RCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_RClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3QCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_QClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3PCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_PClk          },
#if defined(STM32H7RS)
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1SCLK    , .ClkSrcCallback = Rcc_Pll_Get_1_SClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2SCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_SClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2TCLK    , .ClkSrcCallback = Rcc_Pll_Get_2_TClk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3SCLK    , .ClkSrcCallback = Rcc_Pll_Get_3_SClk          },
#endif
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK      , .ClkSrcCallback = Rcc_ClkBus_Get_AHBClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB1Clk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB2Clk      },
#if defined(STM32H7RS)
  { .PeriphClkSrcId = RCC_CLK_SRC_APB4CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB4Clk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB5CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB5Clk      },
#else
  { .PeriphClkSrcId = RCC_CLK_SRC_APB3CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB3Clk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB4CLK     , .ClkSrcCallback = Rcc_ClkBus_Get_APB4Clk      },
#endif
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1TIMCLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB1TimClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2TIMCLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB2TimClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI64CLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi64Clk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_CSI4CLK     , .ClkSrcCallback = Rcc_ClkSrc_Get_CsiClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_CSIDIV122CLK, .ClkSrcCallback = Rcc_ClkSrc_Get_CsiDiv122Clk },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48CLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi48Clk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_HseClk       },
#if defined(STM32H7RS)
  { .PeriphClkSrcId = RCC_CLK_SRC_HSEDIV2CLK  , .ClkSrcCallback = Rcc_ClkSrc_Get_HseDiv2Clk   },
#endif
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_LsiClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_LseClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PERCLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_PerClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_RTCHSECLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_RtcHseClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_PINCLK      , .ClkSrcCallback = Rcc_ClkSrc_Get_PinClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_TRACECLK    , .ClkSrcCallback = Rcc_ClkBus_Get_TraceClk     },
};

_Static_assert( (sizeof(rcc_PeriphClkSrcConfig) / sizeof(rcc_ClkSrcConfigStruct_t)) == RCC_CLK_SRC_CNT, "Rcc: rcc_PeriphClkSrcConfig has incorrect size." );


/** \brief Configuration array of MCU peripherals (indexed by \ref rcc_PeriphId_t). */
const rcc_PeriphConfigStruct_t          rcc_ConfigStruct[] =
{
  /*------------------------- System core (no clock enable) ----------------*/
  { .PeriphId = RCC_PERIPH_SYSTICK                , .ClkSrcId = RCC_CLK_SRC_CPUCLK        , .BlockId = RCC_BLOCK_SYSTICK       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  { .PeriphId = RCC_PERIPH_IWDG                   , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_IWDG          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  { .PeriphId = RCC_PERIPH_RTC_HSE_DIV            , .ClkSrcId = RCC_CLK_SRC_RTCHSECLK     , .BlockId = RCC_BLOCK_RTCAPB        , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV            },
  { .PeriphId = RCC_PERIPH_RTC_LSE                , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_RTCAPB        , .ClkMuxId = RCC_CLK_MUX_RTC_LSE                },
  { .PeriphId = RCC_PERIPH_RTC_LSI                , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_RTCAPB        , .ClkMuxId = RCC_CLK_MUX_RTC_LSI                },

  { .PeriphId = RCC_PERIPH_LPCLK_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_CKPER         , .ClkMuxId = RCC_CLK_MUX_CLKP_HSI               },
  { .PeriphId = RCC_PERIPH_LPCLK_CSI              , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_CKPER         , .ClkMuxId = RCC_CLK_MUX_CLKP_CSI               },
  { .PeriphId = RCC_PERIPH_LPCLK_HSE              , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_CKPER         , .ClkMuxId = RCC_CLK_MUX_CLKP_HSE               },

  { .PeriphId = RCC_PERIPH_TRACE                  , .ClkSrcId = RCC_CLK_SRC_TRACECLK      , .BlockId = RCC_BLOCK_TRACE         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  /*------------------------------ System core ------------------------------*/
#if defined(RCC_APB4ENR_SYSCFGEN)
  { .PeriphId = RCC_PERIPH_SYSCFG                 , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_SYSCFG        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB4ENR_SBSEN)
  { .PeriphId = RCC_PERIPH_SBS                    , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_SBS           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_FLASHEN)
  { .PeriphId = RCC_PERIPH_FLASH                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_FLASH         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_ARTEN)
  { .PeriphId = RCC_PERIPH_ART                    , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ART           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_HSEMEN) || \
    defined(RCC_AHB4ENR_HSEMEN)
  { .PeriphId = RCC_PERIPH_HSEM                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_HSEM          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_IOMNGREN)
  { .PeriphId = RCC_PERIPH_IOMNGR                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_IOMNGR        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_CRS                    , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_CRS           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_APB3ENR_WWDG1EN)
  { .PeriphId = RCC_PERIPH_WWDG1                  , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_WWDG1         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB1LENR_WWDG2EN)
  { .PeriphId = RCC_PERIPH_WWDG2                  , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_WWDG2         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB1ENR1_WWDGEN) || \
    defined(RCC_APB3ENR_WWDGEN)
#if defined(RCC_APB1ENR1_WWDGEN)
  { .PeriphId = RCC_PERIPH_WWDG                   , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_WWDG          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#elif defined(RCC_APB3ENR_WWDGEN)
  { .PeriphId = RCC_PERIPH_WWDG                   , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_WWDG          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#endif
#if defined(RCC_APB4ENR_DTSEN)
  { .PeriphId = RCC_PERIPH_DTS                    , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_DTS           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_BKPRAM                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_BKPRAM        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_AHB3ENR_AXISRAMEN)
  { .PeriphId = RCC_PERIPH_AXISRAM                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_AXISRAM       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_ITCMEN)
  { .PeriphId = RCC_PERIPH_ITCM                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ITCM          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_DTCM1EN)
  { .PeriphId = RCC_PERIPH_DTCM1                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DTCM1         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_DTCM2EN)
  { .PeriphId = RCC_PERIPH_DTCM2                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DTCM2         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_SRAM1EN)
  { .PeriphId = RCC_PERIPH_SRAM1                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM1         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_SRAM2EN)
  { .PeriphId = RCC_PERIPH_SRAM2                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM2         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_SRAM3EN)
  { .PeriphId = RCC_PERIPH_SRAM3                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRAM3         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_AHBSRAM1EN)
  { .PeriphId = RCC_PERIPH_AHBSRAM1               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_AHBSRAM1      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_AHBSRAM2EN)
  { .PeriphId = RCC_PERIPH_AHBSRAM2               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_AHBSRAM2      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_SRDSRAMEN)
  { .PeriphId = RCC_PERIPH_SRDSRAM                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SRDSRAM       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif

  /*------------------------------ DMA --------------------------------------*/
#if defined(RCC_AHB1ENR_DMA1EN)
  { .PeriphId = RCC_PERIPH_DMA1                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DMA1          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_DMA2EN)
  { .PeriphId = RCC_PERIPH_DMA2                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DMA2          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_BDMAEN)
  { .PeriphId = RCC_PERIPH_BDMA                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_BDMA          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_BDMA1EN)
  { .PeriphId = RCC_PERIPH_BDMA1                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_BDMA1         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_BDMA2EN)
  { .PeriphId = RCC_PERIPH_BDMA2                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_BDMA2         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_MDMAEN)
  { .PeriphId = RCC_PERIPH_MDMA                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_MDMA          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_GPDMA1EN)
  { .PeriphId = RCC_PERIPH_GPDMA1                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPDMA1        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB5ENR_HPDMA1EN)
  { .PeriphId = RCC_PERIPH_HPDMA1                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_HPDMA1        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_DMA2D                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DMA2D         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  /*------------------------------ GPIO -------------------------------------*/
  { .PeriphId = RCC_PERIPH_GPIOA                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOA         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOB                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOB         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOC                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOC         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOD                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOD         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOE                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOE         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOF                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOF         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOG                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOG         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_GPIOH                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOH         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_AHB4ENR_GPIOIEN)
  { .PeriphId = RCC_PERIPH_GPIOI                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOI         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_GPIOJEN)
  { .PeriphId = RCC_PERIPH_GPIOJ                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOJ         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_GPIOKEN)
  { .PeriphId = RCC_PERIPH_GPIOK                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOK         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_GPIOMEN)
  { .PeriphId = RCC_PERIPH_GPIOM                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOM         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_GPIONEN)
  { .PeriphId = RCC_PERIPH_GPION                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPION         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_GPIOOEN)
  { .PeriphId = RCC_PERIPH_GPIOO                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOO         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB4ENR_GPIOPEN)
  { .PeriphId = RCC_PERIPH_GPIOP                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPIOP         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif

  /*------------------------------ Timers -----------------------------------*/
  { .PeriphId = RCC_PERIPH_TIM1                   , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM1          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM2                   , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM2          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM3                   , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM3          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM4                   , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM4          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM5                   , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM5          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM6                   , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM6          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM7                   , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM7          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_APB2ENR_TIM8EN)
  { .PeriphId = RCC_PERIPH_TIM8                   , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM8          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB2ENR_TIM9EN)
  { .PeriphId = RCC_PERIPH_TIM9                   , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM9          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_TIM12                  , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM12         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM13                  , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM13         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM14                  , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM14         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM15                  , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM15         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM16                  , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM16         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
  { .PeriphId = RCC_PERIPH_TIM17                  , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_TIM17         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_APB1HENR_TIM23EN)
  { .PeriphId = RCC_PERIPH_TIM23                  , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM23         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB1HENR_TIM24EN)
  { .PeriphId = RCC_PERIPH_TIM24                  , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK    , .BlockId = RCC_BLOCK_TIM24         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB2ENR_HRTIMEN)
  { .PeriphId = RCC_PERIPH_HRTIM                  , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK    , .BlockId = RCC_BLOCK_HRTIM         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_LPTIM1_PCLK1           , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_LPTIM1        , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1           },
  { .PeriphId = RCC_PERIPH_LPTIM1_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM1        , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL2P           },
  { .PeriphId = RCC_PERIPH_LPTIM1_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM1        , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL3R           },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM1        , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE             },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM1        , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI             },
  { .PeriphId = RCC_PERIPH_LPTIM1_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM1        , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LPCLK           },
#if defined(LL_RCC_LPTIM2_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM2_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK4           },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM2_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_PCLK4          },
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM2_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL2P           },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM2_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_PLL2P          },
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM2_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL3R           },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM2_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_PLL3R          },
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM2_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE             },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM2_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_LSE            },
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM2_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI             },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM2_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_LSI            },
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM2_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LPCLK           },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM2_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM2        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_LPCLK          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM3_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PCLK4         },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM3_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_PCLK4          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM3_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PLL2P         },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM3_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_PLL2P          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM3_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PLL3R         },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM3_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_PLL3R          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM3_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LSE           },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM3_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_LSE            },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM3_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LSI           },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM3_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_LSI            },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM3_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LPCLK         },
#elif defined(LL_RCC_LPTIM23_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM3_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM3        , .ClkMuxId = RCC_CLK_MUX_LPTIM23_LPCLK          },
#endif
#if defined(RCC_APB4ENR_LPTIM4EN)
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM4_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PCLK4         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM4_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_PCLK4          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM4_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PLL2P         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM4_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_PLL2P          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM4_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PLL3R         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM4_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_PLL3R          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM4_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LSE           },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM4_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_LSE            },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM4_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LSI           },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM4_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_LSI            },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM4_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LPCLK         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM4_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM4        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_LPCLK          },
#endif
#endif
#if defined(RCC_APB4ENR_LPTIM5EN)
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM5_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PCLK4         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_PCLK4)
  { .PeriphId = RCC_PERIPH_LPTIM5_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_PCLK4          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM5_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PLL2P         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_LPTIM5_PLL2P           , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_PLL2P          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM5_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_PLL3R         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_LPTIM5_PLL3R           , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_PLL3R          },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM5_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LSE           },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_LPTIM5_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_LSE            },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM5_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LSI           },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_LPTIM5_LSI             , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_LSI            },
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM5_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM345_LPCLK         },
#elif defined(LL_RCC_LPTIM45_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_LPTIM5_LPCLK           , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_LPTIM5        , .ClkMuxId = RCC_CLK_MUX_LPTIM45_LPCLK          },
#endif
#endif

  /*------------------------------ Connectivity -----------------------------*/
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPI1_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL1Q           },
#elif defined(LL_RCC_SPI1_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPI1_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q             },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SPI1_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL2P           },
#elif defined(LL_RCC_SPI1_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SPI1_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI1_PLL2P             },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SPI1_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL3P           },
#elif defined(LL_RCC_SPI1_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SPI1_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI1_PLL3P             },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI1_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI123_PIN             },
#elif defined(LL_RCC_SPI1_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI1_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI1_PIN               },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SPI1_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI123_LPCLK           },
#elif defined(LL_RCC_SPI1_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SPI1_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SPI1          , .ClkMuxId = RCC_CLK_MUX_SPI1_LPCLK             },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPI2_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL1Q           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPI2_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI23_PLL1Q            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SPI2_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL2P           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SPI2_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI23_PLL2P            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SPI2_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL3P           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SPI2_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI23_PLL3P            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI2_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI123_PIN             },
#elif defined(LL_RCC_SPI23_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI2_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI23_PIN              },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SPI2_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI123_LPCLK           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SPI2_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SPI2          , .ClkMuxId = RCC_CLK_MUX_SPI23_LPCLK            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPI3_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL1Q           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPI3_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI23_PLL1Q            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SPI3_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL2P           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SPI3_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI23_PLL2P            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SPI3_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI123_PLL3P           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SPI3_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI23_PLL3P            },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI3_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI123_PIN             },
#elif defined(LL_RCC_SPI23_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI3_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI23_PIN              },
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SPI3_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI123_LPCLK           },
#elif defined(LL_RCC_SPI23_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SPI3_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SPI3          , .ClkMuxId = RCC_CLK_MUX_SPI23_LPCLK            },
#endif
  { .PeriphId = RCC_PERIPH_SPI4_PCLK2             , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_SPI4          , .ClkMuxId = RCC_CLK_MUX_SPI45_PCLK2            },
  { .PeriphId = RCC_PERIPH_SPI4_PLL2Q             , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_SPI4          , .ClkMuxId = RCC_CLK_MUX_SPI45_PLL2Q            },
  { .PeriphId = RCC_PERIPH_SPI4_PLL3Q             , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_SPI4          , .ClkMuxId = RCC_CLK_MUX_SPI45_PLL3Q            },
  { .PeriphId = RCC_PERIPH_SPI4_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_SPI4          , .ClkMuxId = RCC_CLK_MUX_SPI45_HSI              },
  { .PeriphId = RCC_PERIPH_SPI4_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_SPI4          , .ClkMuxId = RCC_CLK_MUX_SPI45_CSI              },
  { .PeriphId = RCC_PERIPH_SPI4_HSE               , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_SPI4          , .ClkMuxId = RCC_CLK_MUX_SPI45_HSE              },
  { .PeriphId = RCC_PERIPH_SPI5_PCLK2             , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_SPI5          , .ClkMuxId = RCC_CLK_MUX_SPI45_PCLK2            },
  { .PeriphId = RCC_PERIPH_SPI5_PLL2Q             , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_SPI5          , .ClkMuxId = RCC_CLK_MUX_SPI45_PLL2Q            },
  { .PeriphId = RCC_PERIPH_SPI5_PLL3Q             , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_SPI5          , .ClkMuxId = RCC_CLK_MUX_SPI45_PLL3Q            },
  { .PeriphId = RCC_PERIPH_SPI5_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_SPI5          , .ClkMuxId = RCC_CLK_MUX_SPI45_HSI              },
  { .PeriphId = RCC_PERIPH_SPI5_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_SPI5          , .ClkMuxId = RCC_CLK_MUX_SPI45_CSI              },
  { .PeriphId = RCC_PERIPH_SPI5_HSE               , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_SPI5          , .ClkMuxId = RCC_CLK_MUX_SPI45_HSE              },
  { .PeriphId = RCC_PERIPH_SPI6_PCLK4             , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_PCLK4             },
  { .PeriphId = RCC_PERIPH_SPI6_PLL2Q             , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_PLL2Q             },
  { .PeriphId = RCC_PERIPH_SPI6_PLL3Q             , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_PLL3Q             },
  { .PeriphId = RCC_PERIPH_SPI6_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_HSI               },
  { .PeriphId = RCC_PERIPH_SPI6_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_CSI               },
  { .PeriphId = RCC_PERIPH_SPI6_HSE               , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_HSE               },
#if defined(LL_RCC_SPI6_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SPI6_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SPI6          , .ClkMuxId = RCC_CLK_MUX_SPI6_PIN               },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PCLK1)
  { .PeriphId = RCC_PERIPH_I2C1_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C123_PCLK1           },
#elif defined(LL_RCC_I2C1_CLKSOURCE_PCLK1)
  { .PeriphId = RCC_PERIPH_I2C1_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1             },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_I2C1_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C123_PLL3R           },
#elif defined(LL_RCC_I2C1_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_I2C1_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C1_PLL3R             },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_I2C1_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C123_HSI             },
#elif defined(LL_RCC_I2C1_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_I2C1_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C1_HSI               },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_I2C1_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C123_CSI             },
#elif defined(LL_RCC_I2C1_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_I2C1_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C1          , .ClkMuxId = RCC_CLK_MUX_I2C1_CSI               },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PCLK1)
  { .PeriphId = RCC_PERIPH_I2C2_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C123_PCLK1           },
#elif defined(LL_RCC_I2C23_CLKSOURCE_PCLK1)
  { .PeriphId = RCC_PERIPH_I2C2_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C23_PCLK1            },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_I2C2_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C123_PLL3R           },
#elif defined(LL_RCC_I2C23_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_I2C2_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C23_PLL3R            },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_I2C2_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C123_HSI             },
#elif defined(LL_RCC_I2C23_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_I2C2_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C23_HSI              },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_I2C2_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C123_CSI             },
#elif defined(LL_RCC_I2C23_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_I2C2_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C2          , .ClkMuxId = RCC_CLK_MUX_I2C23_CSI              },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PCLK1)
  { .PeriphId = RCC_PERIPH_I2C3_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C123_PCLK1           },
#elif defined(LL_RCC_I2C23_CLKSOURCE_PCLK1)
  { .PeriphId = RCC_PERIPH_I2C3_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C23_PCLK1            },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_I2C3_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C123_PLL3R           },
#elif defined(LL_RCC_I2C23_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_I2C3_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C23_PLL3R            },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_I2C3_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C123_HSI             },
#elif defined(LL_RCC_I2C23_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_I2C3_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C23_HSI              },
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_I2C3_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C123_CSI             },
#elif defined(LL_RCC_I2C23_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_I2C3_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C3          , .ClkMuxId = RCC_CLK_MUX_I2C23_CSI              },
#endif
#if defined(RCC_APB4ENR_I2C4EN)
  { .PeriphId = RCC_PERIPH_I2C4_PCLK4             , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_I2C4          , .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK4             },
  { .PeriphId = RCC_PERIPH_I2C4_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C4          , .ClkMuxId = RCC_CLK_MUX_I2C4_PLL3R             },
  { .PeriphId = RCC_PERIPH_I2C4_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C4          , .ClkMuxId = RCC_CLK_MUX_I2C4_HSI               },
  { .PeriphId = RCC_PERIPH_I2C4_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C4          , .ClkMuxId = RCC_CLK_MUX_I2C4_CSI               },
#endif
#if defined(RCC_APB1LENR_I2C5EN)
  { .PeriphId = RCC_PERIPH_I2C5_PCLK1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_I2C5          , .ClkMuxId = RCC_CLK_MUX_I2C123_PCLK1           },
  { .PeriphId = RCC_PERIPH_I2C5_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_I2C5          , .ClkMuxId = RCC_CLK_MUX_I2C123_PLL3R           },
  { .PeriphId = RCC_PERIPH_I2C5_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_I2C5          , .ClkMuxId = RCC_CLK_MUX_I2C123_HSI             },
  { .PeriphId = RCC_PERIPH_I2C5_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_I2C5          , .ClkMuxId = RCC_CLK_MUX_I2C123_CSI             },
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_PCLK2)
  { .PeriphId = RCC_PERIPH_USART1_PCLK2           , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART16910_PCLK2       },
#elif defined(LL_RCC_USART1_CLKSOURCE_PCLK2)
  { .PeriphId = RCC_PERIPH_USART1_PCLK2           , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2           },
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_PLL2Q)
  { .PeriphId = RCC_PERIPH_USART1_PLL2Q           , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL2Q       },
#elif defined(LL_RCC_USART1_CLKSOURCE_PLL2Q)
  { .PeriphId = RCC_PERIPH_USART1_PLL2Q           , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART1_PLL2Q           },
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_PLL3Q)
  { .PeriphId = RCC_PERIPH_USART1_PLL3Q           , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL3Q       },
#elif defined(LL_RCC_USART1_CLKSOURCE_PLL3Q)
  { .PeriphId = RCC_PERIPH_USART1_PLL3Q           , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART1_PLL3Q           },
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_USART1_HSI             , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART16910_HSI         },
#elif defined(LL_RCC_USART1_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_USART1_HSI             , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART1_HSI             },
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_USART1_CSI             , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART16910_CSI         },
#elif defined(LL_RCC_USART1_CLKSOURCE_CSI)
  { .PeriphId = RCC_PERIPH_USART1_CSI             , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART1_CSI             },
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_USART1_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART16910_LSE         },
#elif defined(LL_RCC_USART1_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_USART1_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART1        , .ClkMuxId = RCC_CLK_MUX_USART1_LSE             },
#endif
  { .PeriphId = RCC_PERIPH_USART2_PCLK1           , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_USART2        , .ClkMuxId = RCC_CLK_MUX_USART234578_PCLK1      },
  { .PeriphId = RCC_PERIPH_USART2_PLL2Q           , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USART2        , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL2Q      },
  { .PeriphId = RCC_PERIPH_USART2_PLL3Q           , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USART2        , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL3Q      },
  { .PeriphId = RCC_PERIPH_USART2_HSI             , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_USART2        , .ClkMuxId = RCC_CLK_MUX_USART234578_HSI        },
  { .PeriphId = RCC_PERIPH_USART2_CSI             , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_USART2        , .ClkMuxId = RCC_CLK_MUX_USART234578_CSI        },
  { .PeriphId = RCC_PERIPH_USART2_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART2        , .ClkMuxId = RCC_CLK_MUX_USART234578_LSE        },
  { .PeriphId = RCC_PERIPH_USART3_PCLK1           , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_USART3        , .ClkMuxId = RCC_CLK_MUX_USART234578_PCLK1      },
  { .PeriphId = RCC_PERIPH_USART3_PLL2Q           , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USART3        , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL2Q      },
  { .PeriphId = RCC_PERIPH_USART3_PLL3Q           , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USART3        , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL3Q      },
  { .PeriphId = RCC_PERIPH_USART3_HSI             , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_USART3        , .ClkMuxId = RCC_CLK_MUX_USART234578_HSI        },
  { .PeriphId = RCC_PERIPH_USART3_CSI             , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_USART3        , .ClkMuxId = RCC_CLK_MUX_USART234578_CSI        },
  { .PeriphId = RCC_PERIPH_USART3_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART3        , .ClkMuxId = RCC_CLK_MUX_USART234578_LSE        },
  { .PeriphId = RCC_PERIPH_UART4_PCLK1            , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UART4         , .ClkMuxId = RCC_CLK_MUX_USART234578_PCLK1      },
  { .PeriphId = RCC_PERIPH_UART4_PLL2Q            , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_UART4         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL2Q      },
  { .PeriphId = RCC_PERIPH_UART4_PLL3Q            , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_UART4         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL3Q      },
  { .PeriphId = RCC_PERIPH_UART4_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_UART4         , .ClkMuxId = RCC_CLK_MUX_USART234578_HSI        },
  { .PeriphId = RCC_PERIPH_UART4_CSI              , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_UART4         , .ClkMuxId = RCC_CLK_MUX_USART234578_CSI        },
  { .PeriphId = RCC_PERIPH_UART4_LSE              , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART4         , .ClkMuxId = RCC_CLK_MUX_USART234578_LSE        },
  { .PeriphId = RCC_PERIPH_UART5_PCLK1            , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UART5         , .ClkMuxId = RCC_CLK_MUX_USART234578_PCLK1      },
  { .PeriphId = RCC_PERIPH_UART5_PLL2Q            , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_UART5         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL2Q      },
  { .PeriphId = RCC_PERIPH_UART5_PLL3Q            , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_UART5         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL3Q      },
  { .PeriphId = RCC_PERIPH_UART5_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_UART5         , .ClkMuxId = RCC_CLK_MUX_USART234578_HSI        },
  { .PeriphId = RCC_PERIPH_UART5_CSI              , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_UART5         , .ClkMuxId = RCC_CLK_MUX_USART234578_CSI        },
  { .PeriphId = RCC_PERIPH_UART5_LSE              , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART5         , .ClkMuxId = RCC_CLK_MUX_USART234578_LSE        },
#if defined(RCC_APB2ENR_USART6EN)
  { .PeriphId = RCC_PERIPH_USART6_PCLK2           , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_USART6        , .ClkMuxId = RCC_CLK_MUX_USART16910_PCLK2       },
  { .PeriphId = RCC_PERIPH_USART6_PLL2Q           , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USART6        , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL2Q       },
  { .PeriphId = RCC_PERIPH_USART6_PLL3Q           , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USART6        , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL3Q       },
  { .PeriphId = RCC_PERIPH_USART6_HSI             , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_USART6        , .ClkMuxId = RCC_CLK_MUX_USART16910_HSI         },
  { .PeriphId = RCC_PERIPH_USART6_CSI             , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_USART6        , .ClkMuxId = RCC_CLK_MUX_USART16910_CSI         },
  { .PeriphId = RCC_PERIPH_USART6_LSE             , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART6        , .ClkMuxId = RCC_CLK_MUX_USART16910_LSE         },
#endif
  { .PeriphId = RCC_PERIPH_UART7_PCLK1            , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UART7         , .ClkMuxId = RCC_CLK_MUX_USART234578_PCLK1      },
  { .PeriphId = RCC_PERIPH_UART7_PLL2Q            , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_UART7         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL2Q      },
  { .PeriphId = RCC_PERIPH_UART7_PLL3Q            , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_UART7         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL3Q      },
  { .PeriphId = RCC_PERIPH_UART7_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_UART7         , .ClkMuxId = RCC_CLK_MUX_USART234578_HSI        },
  { .PeriphId = RCC_PERIPH_UART7_CSI              , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_UART7         , .ClkMuxId = RCC_CLK_MUX_USART234578_CSI        },
  { .PeriphId = RCC_PERIPH_UART7_LSE              , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART7         , .ClkMuxId = RCC_CLK_MUX_USART234578_LSE        },
  { .PeriphId = RCC_PERIPH_UART8_PCLK1            , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UART8         , .ClkMuxId = RCC_CLK_MUX_USART234578_PCLK1      },
  { .PeriphId = RCC_PERIPH_UART8_PLL2Q            , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_UART8         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL2Q      },
  { .PeriphId = RCC_PERIPH_UART8_PLL3Q            , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_UART8         , .ClkMuxId = RCC_CLK_MUX_USART234578_PLL3Q      },
  { .PeriphId = RCC_PERIPH_UART8_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_UART8         , .ClkMuxId = RCC_CLK_MUX_USART234578_HSI        },
  { .PeriphId = RCC_PERIPH_UART8_CSI              , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_UART8         , .ClkMuxId = RCC_CLK_MUX_USART234578_CSI        },
  { .PeriphId = RCC_PERIPH_UART8_LSE              , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART8         , .ClkMuxId = RCC_CLK_MUX_USART234578_LSE        },
#if defined(RCC_APB2ENR_UART9EN)
  { .PeriphId = RCC_PERIPH_UART9_PCLK2            , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_UART9         , .ClkMuxId = RCC_CLK_MUX_USART16910_PCLK2       },
  { .PeriphId = RCC_PERIPH_UART9_PLL2Q            , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_UART9         , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL2Q       },
  { .PeriphId = RCC_PERIPH_UART9_PLL3Q            , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_UART9         , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL3Q       },
  { .PeriphId = RCC_PERIPH_UART9_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_UART9         , .ClkMuxId = RCC_CLK_MUX_USART16910_HSI         },
  { .PeriphId = RCC_PERIPH_UART9_CSI              , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_UART9         , .ClkMuxId = RCC_CLK_MUX_USART16910_CSI         },
  { .PeriphId = RCC_PERIPH_UART9_LSE              , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_UART9         , .ClkMuxId = RCC_CLK_MUX_USART16910_LSE         },
#endif
#if defined(RCC_APB2ENR_USART10EN)
  { .PeriphId = RCC_PERIPH_USART10_PCLK2          , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_USART10       , .ClkMuxId = RCC_CLK_MUX_USART16910_PCLK2       },
  { .PeriphId = RCC_PERIPH_USART10_PLL2Q          , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_USART10       , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL2Q       },
  { .PeriphId = RCC_PERIPH_USART10_PLL3Q          , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USART10       , .ClkMuxId = RCC_CLK_MUX_USART16910_PLL3Q       },
  { .PeriphId = RCC_PERIPH_USART10_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_USART10       , .ClkMuxId = RCC_CLK_MUX_USART16910_HSI         },
  { .PeriphId = RCC_PERIPH_USART10_CSI            , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_USART10       , .ClkMuxId = RCC_CLK_MUX_USART16910_CSI         },
  { .PeriphId = RCC_PERIPH_USART10_LSE            , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_USART10       , .ClkMuxId = RCC_CLK_MUX_USART16910_LSE         },
#endif
  { .PeriphId = RCC_PERIPH_LPUART1_PCLK4          , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_LPUART1       , .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK4          },
  { .PeriphId = RCC_PERIPH_LPUART1_PLL2Q          , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_LPUART1       , .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL2Q          },
  { .PeriphId = RCC_PERIPH_LPUART1_PLL3Q          , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_LPUART1       , .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL3Q          },
  { .PeriphId = RCC_PERIPH_LPUART1_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_LPUART1       , .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI            },
  { .PeriphId = RCC_PERIPH_LPUART1_CSI            , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_LPUART1       , .ClkMuxId = RCC_CLK_MUX_LPUART1_CSI            },
  { .PeriphId = RCC_PERIPH_LPUART1_LSE            , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_LPUART1       , .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE            },
  { .PeriphId = RCC_PERIPH_FDCAN_HSE              , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_FDCAN         , .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE              },
  { .PeriphId = RCC_PERIPH_FDCAN_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_FDCAN         , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL1Q            },
  { .PeriphId = RCC_PERIPH_FDCAN_PLL2Q            , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_FDCAN         , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL2Q            },
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL1Q           , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SDMMC1        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL1Q            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2R)
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL2R           , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_SDMMC1        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL2R            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2S)
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL2S           , .ClkSrcId = RCC_CLK_SRC_PLL2SCLK      , .BlockId = RCC_BLOCK_SDMMC1        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL2S            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2T)
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL2T           , .ClkSrcId = RCC_CLK_SRC_PLL2TCLK      , .BlockId = RCC_BLOCK_SDMMC1        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL2T            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL1Q           , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SDMMC2        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL1Q            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2R)
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL2R           , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_SDMMC2        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL2R            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2S)
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL2S           , .ClkSrcId = RCC_CLK_SRC_PLL2SCLK      , .BlockId = RCC_BLOCK_SDMMC2        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL2S            },
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2T)
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL2T           , .ClkSrcId = RCC_CLK_SRC_PLL2TCLK      , .BlockId = RCC_BLOCK_SDMMC2        , .ClkMuxId = RCC_CLK_MUX_SDMMC_PLL2T            },
#endif
  { .PeriphId = RCC_PERIPH_FMC_HCLK               , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_FMC           , .ClkMuxId = RCC_CLK_MUX_FMC_HCLK               },
  { .PeriphId = RCC_PERIPH_FMC_PLL1Q              , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_FMC           , .ClkMuxId = RCC_CLK_MUX_FMC_PLL1Q              },
  { .PeriphId = RCC_PERIPH_FMC_PLL2R              , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_FMC           , .ClkMuxId = RCC_CLK_MUX_FMC_PLL2R              },
#if defined(LL_RCC_FMC_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_FMC_LPCLK              , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_FMC           , .ClkMuxId = RCC_CLK_MUX_FMC_LPCLK              },
#endif
#if defined(LL_RCC_FMC_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_FMC_HSI                , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_FMC           , .ClkMuxId = RCC_CLK_MUX_FMC_HSI                },
#endif
#if defined(RCC_AHB3ENR_QSPIEN)
  { .PeriphId = RCC_PERIPH_QSPI_HCLK              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_QSPI          , .ClkMuxId = RCC_CLK_MUX_QSPI_HCLK              },
  { .PeriphId = RCC_PERIPH_QSPI_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_QSPI          , .ClkMuxId = RCC_CLK_MUX_QSPI_PLL1Q             },
  { .PeriphId = RCC_PERIPH_QSPI_PLL2R             , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_QSPI          , .ClkMuxId = RCC_CLK_MUX_QSPI_PLL2R             },
  { .PeriphId = RCC_PERIPH_QSPI_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_QSPI          , .ClkMuxId = RCC_CLK_MUX_QSPI_LPCLK             },
#endif
#if defined(RCC_AHB3ENR_OSPI1EN)
  { .PeriphId = RCC_PERIPH_OSPI1_HCLK             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OSPI1         , .ClkMuxId = RCC_CLK_MUX_OSPI_HCLK              },
  { .PeriphId = RCC_PERIPH_OSPI1_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_OSPI1         , .ClkMuxId = RCC_CLK_MUX_OSPI_PLL1Q             },
  { .PeriphId = RCC_PERIPH_OSPI1_PLL2R            , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_OSPI1         , .ClkMuxId = RCC_CLK_MUX_OSPI_PLL2R             },
  { .PeriphId = RCC_PERIPH_OSPI1_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_OSPI1         , .ClkMuxId = RCC_CLK_MUX_OSPI_LPCLK             },
#endif
#if defined(RCC_AHB3ENR_OSPI2EN)
  { .PeriphId = RCC_PERIPH_OSPI2_HCLK             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OSPI2         , .ClkMuxId = RCC_CLK_MUX_OSPI_HCLK              },
  { .PeriphId = RCC_PERIPH_OSPI2_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_OSPI2         , .ClkMuxId = RCC_CLK_MUX_OSPI_PLL1Q             },
  { .PeriphId = RCC_PERIPH_OSPI2_PLL2R            , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_OSPI2         , .ClkMuxId = RCC_CLK_MUX_OSPI_PLL2R             },
  { .PeriphId = RCC_PERIPH_OSPI2_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_OSPI2         , .ClkMuxId = RCC_CLK_MUX_OSPI_LPCLK             },
#endif
#if defined(RCC_AHB3ENR_OTFDEC1EN)
  { .PeriphId = RCC_PERIPH_OTFDEC1                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OTFDEC1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_OTFDEC2EN)
  { .PeriphId = RCC_PERIPH_OTFDEC2                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OTFDEC2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB5ENR_XSPI1EN)
  { .PeriphId = RCC_PERIPH_XSPI1_HCLK             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_XSPI1         , .ClkMuxId = RCC_CLK_MUX_XSPI1_HCLK             },
  { .PeriphId = RCC_PERIPH_XSPI1_PLL2S            , .ClkSrcId = RCC_CLK_SRC_PLL2SCLK      , .BlockId = RCC_BLOCK_XSPI1         , .ClkMuxId = RCC_CLK_MUX_XSPI1_PLL2S            },
  { .PeriphId = RCC_PERIPH_XSPI1_PLL2T            , .ClkSrcId = RCC_CLK_SRC_PLL2TCLK      , .BlockId = RCC_BLOCK_XSPI1         , .ClkMuxId = RCC_CLK_MUX_XSPI1_PLL2T            },
#endif
#if defined(RCC_AHB5ENR_XSPI2EN)
  { .PeriphId = RCC_PERIPH_XSPI2_HCLK             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_XSPI2         , .ClkMuxId = RCC_CLK_MUX_XSPI2_HCLK             },
  { .PeriphId = RCC_PERIPH_XSPI2_PLL2S            , .ClkSrcId = RCC_CLK_SRC_PLL2SCLK      , .BlockId = RCC_BLOCK_XSPI2         , .ClkMuxId = RCC_CLK_MUX_XSPI2_PLL2S            },
  { .PeriphId = RCC_PERIPH_XSPI2_PLL2T            , .ClkSrcId = RCC_CLK_SRC_PLL2TCLK      , .BlockId = RCC_BLOCK_XSPI2         , .ClkMuxId = RCC_CLK_MUX_XSPI2_PLL2T            },
#endif
#if defined(RCC_AHB5ENR_XSPIMEN)
  { .PeriphId = RCC_PERIPH_XSPIM                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_XSPIM         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSEN)
  { .PeriphId = RCC_PERIPH_USB1OTGHS_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_USB1OTGHS     , .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q              },
  { .PeriphId = RCC_PERIPH_USB1OTGHS_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USB1OTGHS     , .ClkMuxId = RCC_CLK_MUX_USB_PLL3Q              },
  { .PeriphId = RCC_PERIPH_USB1OTGHS_HSI48        , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_USB1OTGHS     , .ClkMuxId = RCC_CLK_MUX_USB_HSI48              },
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSULPIEN)
  { .PeriphId = RCC_PERIPH_USB1OTGHSULPI          , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_USB1OTGHSULPI , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSEN)
  { .PeriphId = RCC_PERIPH_USB2OTGFS_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_USB2OTGFS     , .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q              },
  { .PeriphId = RCC_PERIPH_USB2OTGFS_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USB2OTGFS     , .ClkMuxId = RCC_CLK_MUX_USB_PLL3Q              },
  { .PeriphId = RCC_PERIPH_USB2OTGFS_HSI48        , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_USB2OTGFS     , .ClkMuxId = RCC_CLK_MUX_USB_HSI48              },
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSULPIEN)
  { .PeriphId = RCC_PERIPH_USB2OTGFSULPI          , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_USB2OTGFSULPI , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_OTGHSEN)
  { .PeriphId = RCC_PERIPH_OTGHS                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_OTGHS         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_OTGFSEN)
  { .PeriphId = RCC_PERIPH_OTGFS_HSI48            , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_OTGFS         , .ClkMuxId = RCC_CLK_MUX_OTGFS_HSI48            },
  { .PeriphId = RCC_PERIPH_OTGFS_PLL3Q            , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_OTGFS         , .ClkMuxId = RCC_CLK_MUX_OTGFS_PLL3Q            },
  { .PeriphId = RCC_PERIPH_OTGFS_HSE              , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_OTGFS         , .ClkMuxId = RCC_CLK_MUX_OTGFS_HSE              },
  { .PeriphId = RCC_PERIPH_OTGFS_CLK48            , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_OTGFS         , .ClkMuxId = RCC_CLK_MUX_OTGFS_CLK48            },
#endif
#if defined(RCC_AHB1ENR_USBPHYCEN)
  { .PeriphId = RCC_PERIPH_USBPHYC_HSE            , .ClkSrcId = RCC_CLK_SRC_HSECLK        , .BlockId = RCC_BLOCK_USBPHYC       , .ClkMuxId = RCC_CLK_MUX_USBPHYC_HSE            },
  { .PeriphId = RCC_PERIPH_USBPHYC_HSE_DIV2       , .ClkSrcId = RCC_CLK_SRC_HSEDIV2CLK    , .BlockId = RCC_BLOCK_USBPHYC       , .ClkMuxId = RCC_CLK_MUX_USBPHYC_HSE_DIV2       },
  { .PeriphId = RCC_PERIPH_USBPHYC_PLL3Q          , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK      , .BlockId = RCC_BLOCK_USBPHYC       , .ClkMuxId = RCC_CLK_MUX_USBPHYC_PLL3Q          },
#endif
#if defined(RCC_APB1ENR2_UCPD1EN)
  { .PeriphId = RCC_PERIPH_UCPD1                  , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_UCPD1         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_ETH1MACEN)
  { .PeriphId = RCC_PERIPH_ETH1MAC                , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ETH1MAC       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_ETH1TXEN)
  { .PeriphId = RCC_PERIPH_ETH1TX                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ETH1TX        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB1ENR_ETH1RXEN)
  { .PeriphId = RCC_PERIPH_ETH1RX                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ETH1RX        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_MDIOS                  , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_MDIOS         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_APB1HENR_SWPMIEN)
  { .PeriphId = RCC_PERIPH_SWPMI_PCLK1            , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_SWPMI         , .ClkMuxId = RCC_CLK_MUX_SWP_PCLK1              },
  { .PeriphId = RCC_PERIPH_SWPMI_HSI              , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_SWPMI         , .ClkMuxId = RCC_CLK_MUX_SWP_HSI                },
#endif
  { .PeriphId = RCC_PERIPH_CEC_LSE                , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_CEC           , .ClkMuxId = RCC_CLK_MUX_CEC_LSE                },
  { .PeriphId = RCC_PERIPH_CEC_LSI                , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_CEC           , .ClkMuxId = RCC_CLK_MUX_CEC_LSI                },
#if defined(LL_RCC_CEC_CLKSOURCE_CSI_DIV122)
  { .PeriphId = RCC_PERIPH_CEC_CSI_DIV122         , .ClkSrcId = RCC_CLK_SRC_CSIDIV122CLK  , .BlockId = RCC_BLOCK_CEC           , .ClkMuxId = RCC_CLK_MUX_CEC_CSI_DIV122         },
#elif defined(LL_RCC_CEC_CLKSOURCE_CSI_DIV_122)
  { .PeriphId = RCC_PERIPH_CEC_CSI_DIV122         , .ClkSrcId = RCC_CLK_SRC_CSIDIV122CLK  , .BlockId = RCC_BLOCK_CEC           , .ClkMuxId = RCC_CLK_MUX_CEC_CSI_DIV122         },
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPDIFRX_PLL1Q          , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIF_PLL1Q            },
#elif defined(LL_RCC_SPDIFRX_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SPDIFRX_PLL1Q          , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIFRX_PLL1Q          },
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_PLL2R)
  { .PeriphId = RCC_PERIPH_SPDIFRX_PLL2R          , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIF_PLL2R            },
#elif defined(LL_RCC_SPDIFRX_CLKSOURCE_PLL2R)
  { .PeriphId = RCC_PERIPH_SPDIFRX_PLL2R          , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIFRX_PLL2R          },
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_SPDIFRX_PLL3R          , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIF_PLL3R            },
#elif defined(LL_RCC_SPDIFRX_CLKSOURCE_PLL3R)
  { .PeriphId = RCC_PERIPH_SPDIFRX_PLL3R          , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIFRX_PLL3R          },
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_SPDIFRX_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIF_HSI              },
#elif defined(LL_RCC_SPDIFRX_CLKSOURCE_HSI)
  { .PeriphId = RCC_PERIPH_SPDIFRX_HSI            , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_SPDIFRX       , .ClkMuxId = RCC_CLK_MUX_SPDIFRX_HSI            },
#endif
#if defined(RCC_APB2ENR_DFSDM1EN)
  { .PeriphId = RCC_PERIPH_DFSDM1_PCLK2           , .ClkSrcId = RCC_CLK_SRC_APB2CLK       , .BlockId = RCC_BLOCK_DFSDM1        , .ClkMuxId = RCC_CLK_MUX_DFSDM1_PCLK2           },
  { .PeriphId = RCC_PERIPH_DFSDM1_SYSCLK          , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_DFSDM1        , .ClkMuxId = RCC_CLK_MUX_DFSDM1_SYSCLK          },
#endif
#if defined(RCC_APB4ENR_DFSDM2EN)
  { .PeriphId = RCC_PERIPH_DFSDM2_PCLK4           , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_DFSDM2        , .ClkMuxId = RCC_CLK_MUX_DFSDM2_PCLK4           },
  { .PeriphId = RCC_PERIPH_DFSDM2_SYSCLK          , .ClkSrcId = RCC_CLK_SRC_SYSCLK        , .BlockId = RCC_BLOCK_DFSDM2        , .ClkMuxId = RCC_CLK_MUX_DFSDM2_SYSCLK          },
#endif

  /*------------------------------ Multimedia -------------------------------*/
  { .PeriphId = RCC_PERIPH_SAI1_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI1          , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q             },
  { .PeriphId = RCC_PERIPH_SAI1_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI1          , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P             },
  { .PeriphId = RCC_PERIPH_SAI1_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI1          , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P             },
  { .PeriphId = RCC_PERIPH_SAI1_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI1          , .ClkMuxId = RCC_CLK_MUX_SAI1_PIN               },
  { .PeriphId = RCC_PERIPH_SAI1_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI1          , .ClkMuxId = RCC_CLK_MUX_SAI1_LPCLK             },
#if defined(RCC_APB2ENR_SAI2EN)
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SAI2_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI23_PLL1Q            },
#elif defined(LL_RCC_SAI2_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SAI2_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q             },
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SAI2_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI23_PLL2P            },
#elif defined(LL_RCC_SAI2_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SAI2_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P             },
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SAI2_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI23_PLL3P            },
#elif defined(LL_RCC_SAI2_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SAI2_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P             },
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SAI2_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI23_PIN              },
#elif defined(LL_RCC_SAI2_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SAI2_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2_PIN               },
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SAI2_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI23_LPCLK            },
#elif defined(LL_RCC_SAI2_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SAI2_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2_LPCLK             },
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SAI2A_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2A_PLL1Q            },
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SAI2A_PLL2P            , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2A_PLL2P            },
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SAI2A_PLL3P            , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2A_PLL3P            },
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SAI2A_PIN              , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2A_PIN              },
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SAI2A_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2A_LPCLK            },
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_SPDIF)
  { .PeriphId = RCC_PERIPH_SAI2A_SPDIF            , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2A_SPDIF            },
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_SAI2B_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2B_PLL1Q            },
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL2P)
  { .PeriphId = RCC_PERIPH_SAI2B_PLL2P            , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2B_PLL2P            },
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL3P)
  { .PeriphId = RCC_PERIPH_SAI2B_PLL3P            , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2B_PLL3P            },
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_I2S_CKIN)
  { .PeriphId = RCC_PERIPH_SAI2B_PIN              , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2B_PIN              },
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_CLKP)
  { .PeriphId = RCC_PERIPH_SAI2B_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2B_LPCLK            },
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_SPDIF)
  { .PeriphId = RCC_PERIPH_SAI2B_SPDIF            , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2B_SPDIF            },
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_SPDIFRX)
  { .PeriphId = RCC_PERIPH_SAI2_SPDIF             , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI2          , .ClkMuxId = RCC_CLK_MUX_SAI2_SPDIF             },
#endif
#endif
#if defined(RCC_APB2ENR_SAI3EN)
  { .PeriphId = RCC_PERIPH_SAI3_PLL1Q             , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI3          , .ClkMuxId = RCC_CLK_MUX_SAI23_PLL1Q            },
  { .PeriphId = RCC_PERIPH_SAI3_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI3          , .ClkMuxId = RCC_CLK_MUX_SAI23_PLL2P            },
  { .PeriphId = RCC_PERIPH_SAI3_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI3          , .ClkMuxId = RCC_CLK_MUX_SAI23_PLL3P            },
  { .PeriphId = RCC_PERIPH_SAI3_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI3          , .ClkMuxId = RCC_CLK_MUX_SAI23_PIN              },
  { .PeriphId = RCC_PERIPH_SAI3_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI3          , .ClkMuxId = RCC_CLK_MUX_SAI23_LPCLK            },
#endif
#if defined(RCC_APB4ENR_SAI4EN)
  { .PeriphId = RCC_PERIPH_SAI4A_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4A_PLL1Q            },
  { .PeriphId = RCC_PERIPH_SAI4A_PLL2P            , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4A_PLL2P            },
  { .PeriphId = RCC_PERIPH_SAI4A_PLL3P            , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4A_PLL3P            },
  { .PeriphId = RCC_PERIPH_SAI4A_PIN              , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4A_PIN              },
  { .PeriphId = RCC_PERIPH_SAI4A_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4A_LPCLK            },
#if defined(LL_RCC_SAI4A_CLKSOURCE_SPDIF)
  { .PeriphId = RCC_PERIPH_SAI4A_SPDIF            , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4A_SPDIF            },
#endif
  { .PeriphId = RCC_PERIPH_SAI4B_PLL1Q            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4B_PLL1Q            },
  { .PeriphId = RCC_PERIPH_SAI4B_PLL2P            , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4B_PLL2P            },
  { .PeriphId = RCC_PERIPH_SAI4B_PLL3P            , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4B_PLL3P            },
  { .PeriphId = RCC_PERIPH_SAI4B_PIN              , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4B_PIN              },
  { .PeriphId = RCC_PERIPH_SAI4B_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4B_LPCLK            },
#if defined(LL_RCC_SAI4B_CLKSOURCE_SPDIF)
  { .PeriphId = RCC_PERIPH_SAI4B_SPDIF            , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_SAI4          , .ClkMuxId = RCC_CLK_MUX_SAI4B_SPDIF            },
#endif
#endif
#if defined(RCC_AHB1ENR_ADF1EN)
  { .PeriphId = RCC_PERIPH_ADF1_HCLK              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ADF1          , .ClkMuxId = RCC_CLK_MUX_ADF1_HCLK              },
  { .PeriphId = RCC_PERIPH_ADF1_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_ADF1          , .ClkMuxId = RCC_CLK_MUX_ADF1_PLL2P             },
  { .PeriphId = RCC_PERIPH_ADF1_PLL3P             , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK      , .BlockId = RCC_BLOCK_ADF1          , .ClkMuxId = RCC_CLK_MUX_ADF1_PLL3P             },
  { .PeriphId = RCC_PERIPH_ADF1_PIN               , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_ADF1          , .ClkMuxId = RCC_CLK_MUX_ADF1_PIN               },
  { .PeriphId = RCC_PERIPH_ADF1_CSI               , .ClkSrcId = RCC_CLK_SRC_CSI4CLK       , .BlockId = RCC_BLOCK_ADF1          , .ClkMuxId = RCC_CLK_MUX_ADF1_CSI               },
  { .PeriphId = RCC_PERIPH_ADF1_HSI               , .ClkSrcId = RCC_CLK_SRC_HSI64CLK      , .BlockId = RCC_BLOCK_ADF1          , .ClkMuxId = RCC_CLK_MUX_ADF1_HSI               },
#endif
#if defined(RCC_AHB2ENR_DCMIEN)
  { .PeriphId = RCC_PERIPH_DCMI                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DCMI          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_DCMI_PSSIEN)
  { .PeriphId = RCC_PERIPH_DCMI_PSSI              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_DCMI_PSSI     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB5ENR_DCMIPPEN)
  { .PeriphId = RCC_PERIPH_DCMIPP                 , .ClkSrcId = RCC_CLK_SRC_APB5CLK       , .BlockId = RCC_BLOCK_DCMIPP        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_PSSIEN)
  { .PeriphId = RCC_PERIPH_PSSI_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_PSSI          , .ClkMuxId = RCC_CLK_MUX_PSSI_PLL3R             },
  { .PeriphId = RCC_PERIPH_PSSI_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_PSSI          , .ClkMuxId = RCC_CLK_MUX_PSSI_LPCLK             },
#endif
#if defined(RCC_APB3ENR_LTDCEN) || \
    defined(RCC_APB5ENR_LTDCEN)
#if defined(RCC_APB3ENR_LTDCEN)
  { .PeriphId = RCC_PERIPH_LTDC                   , .ClkSrcId = RCC_CLK_SRC_APB3CLK       , .BlockId = RCC_BLOCK_LTDC          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#elif defined(RCC_APB5ENR_LTDCEN)
  { .PeriphId = RCC_PERIPH_LTDC                   , .ClkSrcId = RCC_CLK_SRC_APB5CLK       , .BlockId = RCC_BLOCK_LTDC          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#endif
#if defined(RCC_APB3ENR_DSIEN)
  { .PeriphId = RCC_PERIPH_DSI_PHY                , .ClkSrcId = RCC_CLK_SRC_PINCLK        , .BlockId = RCC_BLOCK_DSI           , .ClkMuxId = RCC_CLK_MUX_DSI_PHY                },
  { .PeriphId = RCC_PERIPH_DSI_PLL2Q              , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK      , .BlockId = RCC_BLOCK_DSI           , .ClkMuxId = RCC_CLK_MUX_DSI_PLL2Q              },
#endif
#if defined(RCC_AHB3ENR_JPGDECEN)
  { .PeriphId = RCC_PERIPH_JPGDEC                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_JPGDEC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB5ENR_JPEGEN)
  { .PeriphId = RCC_PERIPH_JPEG                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_JPEG          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_GFXMMUEN) || \
    defined(RCC_AHB5ENR_GFXMMUEN)
  { .PeriphId = RCC_PERIPH_GFXMMU                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GFXMMU        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB5ENR_GFXTIMEN)
  { .PeriphId = RCC_PERIPH_GFXTIM                 , .ClkSrcId = RCC_CLK_SRC_APB5CLK       , .BlockId = RCC_BLOCK_GFXTIM        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB5ENR_GPU2DEN)
  { .PeriphId = RCC_PERIPH_GPU2D                  , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_GPU2D         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif

  /*------------------------------ Analog -----------------------------------*/
  { .PeriphId = RCC_PERIPH_ADC12_PLL2P            , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_ADC12         , .ClkMuxId = RCC_CLK_MUX_ADC_PLL2P              },
  { .PeriphId = RCC_PERIPH_ADC12_PLL3R            , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_ADC12         , .ClkMuxId = RCC_CLK_MUX_ADC_PLL3R              },
  { .PeriphId = RCC_PERIPH_ADC12_LPCLK            , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_ADC12         , .ClkMuxId = RCC_CLK_MUX_ADC_LPCLK              },
  { .PeriphId = RCC_PERIPH_ADC12_HCLK             , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ADC12         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_AHB4ENR_ADC3EN)
  { .PeriphId = RCC_PERIPH_ADC3_PLL2P             , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK      , .BlockId = RCC_BLOCK_ADC3          , .ClkMuxId = RCC_CLK_MUX_ADC_PLL2P              },
  { .PeriphId = RCC_PERIPH_ADC3_PLL3R             , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK      , .BlockId = RCC_BLOCK_ADC3          , .ClkMuxId = RCC_CLK_MUX_ADC_PLL3R              },
  { .PeriphId = RCC_PERIPH_ADC3_LPCLK             , .ClkSrcId = RCC_CLK_SRC_PERCLK        , .BlockId = RCC_BLOCK_ADC3          , .ClkMuxId = RCC_CLK_MUX_ADC_LPCLK              },
  { .PeriphId = RCC_PERIPH_ADC3_HCLK              , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_ADC3          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB1LENR_DAC12EN)
  { .PeriphId = RCC_PERIPH_DAC12                  , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_DAC12         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB4ENR_DAC2EN)
  { .PeriphId = RCC_PERIPH_DAC2                   , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_DAC2          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB4ENR_COMP12EN)
  { .PeriphId = RCC_PERIPH_COMP12                 , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_COMP12        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_APB1HENR_OPAMPEN)
  { .PeriphId = RCC_PERIPH_OPAMP                  , .ClkSrcId = RCC_CLK_SRC_APB1CLK       , .BlockId = RCC_BLOCK_OPAMP         , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
  { .PeriphId = RCC_PERIPH_VREF                   , .ClkSrcId = RCC_CLK_SRC_APB4CLK       , .BlockId = RCC_BLOCK_VREF          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },

  /*------------------------------ Security ---------------------------------*/
#if defined(RCC_AHB2ENR_CRYPEN) || \
    defined(RCC_AHB3ENR_CRYPEN)
  { .PeriphId = RCC_PERIPH_CRYP                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_CRYP          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_HASHEN) || \
    defined(RCC_AHB3ENR_HASHEN)
  { .PeriphId = RCC_PERIPH_HASH                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_HASH          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_HSI48)
  { .PeriphId = RCC_PERIPH_RNG_HSI48              , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_RNG           , .ClkMuxId = RCC_CLK_MUX_RNG_HSI48              },
#else
  { .PeriphId = RCC_PERIPH_RNG_HSI48              , .ClkSrcId = RCC_CLK_SRC_HSI48CLK      , .BlockId = RCC_BLOCK_RNG           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_PLL1Q)
  { .PeriphId = RCC_PERIPH_RNG_PLL1Q              , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK      , .BlockId = RCC_BLOCK_RNG           , .ClkMuxId = RCC_CLK_MUX_RNG_PLL1Q              },
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_LSE)
  { .PeriphId = RCC_PERIPH_RNG_LSE                , .ClkSrcId = RCC_CLK_SRC_LSECLK        , .BlockId = RCC_BLOCK_RNG           , .ClkMuxId = RCC_CLK_MUX_RNG_LSE                },
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_LSI)
  { .PeriphId = RCC_PERIPH_RNG_LSI                , .ClkSrcId = RCC_CLK_SRC_LSICLK        , .BlockId = RCC_BLOCK_RNG           , .ClkMuxId = RCC_CLK_MUX_RNG_LSI                },
#endif
#if defined(RCC_AHB3ENR_PKAEN)
  { .PeriphId = RCC_PERIPH_PKA                    , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_PKA           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB3ENR_SAESEN)
  { .PeriphId = RCC_PERIPH_SAES                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_SAES          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif

  /*------------------------------ Computing --------------------------------*/
  { .PeriphId = RCC_PERIPH_CRC                    , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_CRC           , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#if defined(RCC_AHB2ENR_CORDICEN)
  { .PeriphId = RCC_PERIPH_CORDIC                 , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_CORDIC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
#if defined(RCC_AHB2ENR_FMACEN)
  { .PeriphId = RCC_PERIPH_FMAC                   , .ClkSrcId = RCC_CLK_SRC_AHBCLK        , .BlockId = RCC_BLOCK_FMAC          , .ClkMuxId = RCC_CLK_MUX_LIST_CNT               },
#endif
};

_Static_assert( (sizeof(rcc_ConfigStruct) / sizeof(rcc_PeriphConfigStruct_t)) == RCC_PERIPH_ID_CNT, "Rcc: rcc_ConfigStruct has incorrect size." );


/** \brief Configuration registers of RCC peripheral blocks (indexed by \ref rcc_BlockList_t).
 *
 * This array is created to reduce size of configuration.
 */
const rcc_BlockConfigStruct_t           rcc_PeriphBlockConfig[] =
{
  /*------------------------- System core (no clock enable) ----------------*/
  { .BlockId = RCC_BLOCK_SYSTICK       , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_UNSUPPORTED_FUNCTION      , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
  { .BlockId = RCC_BLOCK_IWDG          , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_UNSUPPORTED_FUNCTION      , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
  { .BlockId = RCC_BLOCK_CKPER         , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_UNSUPPORTED_FUNCTION      , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
  { .BlockId = RCC_BLOCK_TRACE         , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_UNSUPPORTED_FUNCTION      , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
  /*------------------------------ System core ------------------------------*/
#if defined(RCC_APB4ENR_SYSCFGEN)
  { .BlockId = RCC_BLOCK_SYSCFG        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_SYSCFGEN          , .LpCtrlMask = RCC_APB4LPENR_SYSCFGLPEN          , .RstCtrlMask = RCC_APB4RSTR_SYSCFGRST         },
#endif
#if defined(RCC_APB4ENR_SBSEN)
  { .BlockId = RCC_BLOCK_SBS           , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_SBSEN             , .LpCtrlMask = RCC_APB4LPENR_SBSLPEN             , .RstCtrlMask = RCC_APB4RSTR_SBSRST            },
#endif
#if defined(RCC_AHB3ENR_FLASHEN)
  { .BlockId = RCC_BLOCK_FLASH         , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_FLASHEN           , .LpCtrlMask = RCC_AHB3LPENR_FLASHLPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB1ENR_ARTEN)
  { .BlockId = RCC_BLOCK_ART           , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ARTEN             , .LpCtrlMask = RCC_AHB1LPENR_ARTLPEN             , .RstCtrlMask = RCC_AHB1RSTR_ARTRST            },
#endif
#if defined(RCC_AHB2ENR_HSEMEN)
  { .BlockId = RCC_BLOCK_HSEM          , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_HSEMEN            , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION          , .RstCtrlMask = RCC_AHB2RSTR_HSEMRST           },
#elif defined(RCC_AHB4ENR_HSEMEN)
  { .BlockId = RCC_BLOCK_HSEM          , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_HSEMEN            , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION          , .RstCtrlMask = RCC_AHB4RSTR_HSEMRST           },
#endif
#if defined(RCC_AHB3ENR_IOMNGREN)
  { .BlockId = RCC_BLOCK_IOMNGR        , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_IOMNGREN          , .LpCtrlMask = RCC_AHB3LPENR_IOMNGRLPEN          , .RstCtrlMask = RCC_AHB3RSTR_IOMNGRRST         },
#endif
#if defined(RCC_APB1HENR_CRSEN)
  { .BlockId = RCC_BLOCK_CRS           , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_CRSEN            , .LpCtrlMask = RCC_APB1HLPENR_CRSLPEN            , .RstCtrlMask = RCC_APB1HRSTR_CRSRST           },
#elif defined(RCC_APB1ENR2_CRSEN)
  { .BlockId = RCC_BLOCK_CRS           , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1ENR2_CRSEN            , .LpCtrlMask = RCC_APB1LPENR2_CRSLPEN            , .RstCtrlMask = RCC_APB1RSTR2_CRSRST           },
#endif
  { .BlockId = RCC_BLOCK_RTCAPB        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_RTCAPBEN          , .LpCtrlMask = RCC_APB4LPENR_RTCAPBLPEN          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#if defined(RCC_APB3ENR_WWDG1EN)
  { .BlockId = RCC_BLOCK_WWDG1         , .ClkBusId = RCC_CLK_BUS_APB3    , .StateMask = RCC_APB3ENR_WWDG1EN           , .LpCtrlMask = RCC_APB3LPENR_WWDG1LPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_APB1LENR_WWDG2EN)
  { .BlockId = RCC_BLOCK_WWDG2         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_WWDG2EN          , .LpCtrlMask = RCC_APB1LLPENR_WWDG2LPEN          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_APB1ENR1_WWDGEN)
  { .BlockId = RCC_BLOCK_WWDG          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_WWDGEN           , .LpCtrlMask = RCC_APB1LPENR1_WWDGLPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#elif defined(RCC_APB3ENR_WWDGEN)
  { .BlockId = RCC_BLOCK_WWDG          , .ClkBusId = RCC_CLK_BUS_APB3    , .StateMask = RCC_APB3ENR_WWDGEN            , .LpCtrlMask = RCC_APB3LPENR_WWDGLPEN            , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_APB4ENR_DTSEN)
  { .BlockId = RCC_BLOCK_DTS           , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_DTSEN             , .LpCtrlMask = RCC_APB4LPENR_DTSLPEN             , .RstCtrlMask = RCC_APB4RSTR_DTSRST            },
#endif
  { .BlockId = RCC_BLOCK_BKPRAM        , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_BKPRAMEN          , .LpCtrlMask = RCC_AHB4LPENR_BKPRAMLPEN          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#if defined(RCC_AHB3ENR_AXISRAMEN)
  { .BlockId = RCC_BLOCK_AXISRAM       , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_AXISRAMEN         , .LpCtrlMask = RCC_AHB3LPENR_AXISRAMLPEN         , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB3ENR_ITCMEN)
  { .BlockId = RCC_BLOCK_ITCM          , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_ITCMEN            , .LpCtrlMask = RCC_AHB3LPENR_ITCMLPEN            , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB3ENR_DTCM1EN)
  { .BlockId = RCC_BLOCK_DTCM1         , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_DTCM1EN           , .LpCtrlMask = RCC_AHB3LPENR_DTCM1LPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB3ENR_DTCM2EN)
  { .BlockId = RCC_BLOCK_DTCM2         , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_DTCM2EN           , .LpCtrlMask = RCC_AHB3LPENR_DTCM2LPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB2ENR_SRAM1EN)
  { .BlockId = RCC_BLOCK_SRAM1         , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_SRAM1EN           , .LpCtrlMask = RCC_AHB2LPENR_SRAM1LPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB2ENR_SRAM2EN)
  { .BlockId = RCC_BLOCK_SRAM2         , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_SRAM2EN           , .LpCtrlMask = RCC_AHB2LPENR_SRAM2LPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB2ENR_SRAM3EN)
  { .BlockId = RCC_BLOCK_SRAM3         , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_SRAM3EN           , .LpCtrlMask = RCC_AHB2LPENR_SRAM3LPEN           , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB2ENR_AHBSRAM1EN)
  { .BlockId = RCC_BLOCK_AHBSRAM1      , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_AHBSRAM1EN        , .LpCtrlMask = RCC_AHB2LPENR_AHBSRAM1LPEN        , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB2ENR_AHBSRAM2EN)
  { .BlockId = RCC_BLOCK_AHBSRAM2      , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_AHBSRAM2EN        , .LpCtrlMask = RCC_AHB2LPENR_AHBSRAM2LPEN        , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB4ENR_SRDSRAMEN)
  { .BlockId = RCC_BLOCK_SRDSRAM       , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_SRDSRAMEN         , .LpCtrlMask = RCC_AHB4LPENR_SRDSRAMLPEN         , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif

  /*------------------------------ DMA --------------------------------------*/
#if defined(RCC_AHB1ENR_DMA1EN)
  { .BlockId = RCC_BLOCK_DMA1          , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_DMA1EN            , .LpCtrlMask = RCC_AHB1LPENR_DMA1LPEN            , .RstCtrlMask = RCC_AHB1RSTR_DMA1RST           },
#endif
#if defined(RCC_AHB1ENR_DMA2EN)
  { .BlockId = RCC_BLOCK_DMA2          , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_DMA2EN            , .LpCtrlMask = RCC_AHB1LPENR_DMA2LPEN            , .RstCtrlMask = RCC_AHB1RSTR_DMA2RST           },
#endif
#if defined(RCC_AHB4ENR_BDMAEN)
  { .BlockId = RCC_BLOCK_BDMA          , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_BDMAEN            , .LpCtrlMask = RCC_AHB4LPENR_BDMALPEN            , .RstCtrlMask = RCC_AHB4RSTR_BDMARST           },
#endif
#if defined(RCC_AHB2ENR_BDMA1EN)
  { .BlockId = RCC_BLOCK_BDMA1         , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_BDMA1EN           , .LpCtrlMask = RCC_AHB2LPENR_BDMA1LPEN           , .RstCtrlMask = RCC_AHB2RSTR_BDMA1RST          },
#endif
#if defined(RCC_AHB4ENR_BDMA2EN)
  { .BlockId = RCC_BLOCK_BDMA2         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_BDMA2EN           , .LpCtrlMask = RCC_AHB4LPENR_BDMA2LPEN           , .RstCtrlMask = RCC_AHB4RSTR_BDMA2RST          },
#endif
#if defined(RCC_AHB3ENR_MDMAEN)
  { .BlockId = RCC_BLOCK_MDMA          , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_MDMAEN            , .LpCtrlMask = RCC_AHB3LPENR_MDMALPEN            , .RstCtrlMask = RCC_AHB3RSTR_MDMARST           },
#endif
#if defined(RCC_AHB1ENR_GPDMA1EN)
  { .BlockId = RCC_BLOCK_GPDMA1        , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_GPDMA1EN          , .LpCtrlMask = RCC_AHB1LPENR_GPDMA1LPEN          , .RstCtrlMask = RCC_AHB1RSTR_GPDMA1RST         },
#endif
#if defined(RCC_AHB5ENR_HPDMA1EN)
  { .BlockId = RCC_BLOCK_HPDMA1        , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_HPDMA1EN          , .LpCtrlMask = RCC_AHB5LPENR_HPDMA1LPEN          , .RstCtrlMask = RCC_AHB5RSTR_HPDMA1RST         },
#endif
#if defined(RCC_AHB3ENR_DMA2DEN)
  { .BlockId = RCC_BLOCK_DMA2D         , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_DMA2DEN           , .LpCtrlMask = RCC_AHB3LPENR_DMA2DLPEN           , .RstCtrlMask = RCC_AHB3RSTR_DMA2DRST          },
#elif defined(RCC_AHB5ENR_DMA2DEN)
  { .BlockId = RCC_BLOCK_DMA2D         , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_DMA2DEN           , .LpCtrlMask = RCC_AHB5LPENR_DMA2DLPEN           , .RstCtrlMask = RCC_AHB5RSTR_DMA2DRST          },
#endif

  /*------------------------------ GPIO -------------------------------------*/
  { .BlockId = RCC_BLOCK_GPIOA         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOAEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOALPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOARST          },
  { .BlockId = RCC_BLOCK_GPIOB         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOBEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOBLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOBRST          },
  { .BlockId = RCC_BLOCK_GPIOC         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOCEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOCLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOCRST          },
  { .BlockId = RCC_BLOCK_GPIOD         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIODEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIODLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIODRST          },
  { .BlockId = RCC_BLOCK_GPIOE         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOEEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOELPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOERST          },
  { .BlockId = RCC_BLOCK_GPIOF         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOFEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOFLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOFRST          },
  { .BlockId = RCC_BLOCK_GPIOG         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOGEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOGLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOGRST          },
  { .BlockId = RCC_BLOCK_GPIOH         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOHEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOHLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOHRST          },
#if defined(RCC_AHB4ENR_GPIOIEN)
  { .BlockId = RCC_BLOCK_GPIOI         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOIEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOILPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOIRST          },
#endif
#if defined(RCC_AHB4ENR_GPIOJEN)
  { .BlockId = RCC_BLOCK_GPIOJ         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOJEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOJLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOJRST          },
#endif
#if defined(RCC_AHB4ENR_GPIOKEN)
  { .BlockId = RCC_BLOCK_GPIOK         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOKEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOKLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOKRST          },
#endif
#if defined(RCC_AHB4ENR_GPIOMEN)
  { .BlockId = RCC_BLOCK_GPIOM         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOMEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOMLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOMRST          },
#endif
#if defined(RCC_AHB4ENR_GPIONEN)
  { .BlockId = RCC_BLOCK_GPION         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIONEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIONLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIONRST          },
#endif
#if defined(RCC_AHB4ENR_GPIOOEN)
  { .BlockId = RCC_BLOCK_GPIOO         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOOEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOOLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOORST          },
#endif
#if defined(RCC_AHB4ENR_GPIOPEN)
  { .BlockId = RCC_BLOCK_GPIOP         , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_GPIOPEN           , .LpCtrlMask = RCC_AHB4LPENR_GPIOPLPEN           , .RstCtrlMask = RCC_AHB4RSTR_GPIOPRST          },
#endif

  /*------------------------------ Timers -----------------------------------*/
  { .BlockId = RCC_BLOCK_TIM1          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_TIM1EN            , .LpCtrlMask = RCC_APB2LPENR_TIM1LPEN            , .RstCtrlMask = RCC_APB2RSTR_TIM1RST           },
#if defined(RCC_APB1LENR_TIM2EN)
  { .BlockId = RCC_BLOCK_TIM2          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM2EN           , .LpCtrlMask = RCC_APB1LLPENR_TIM2LPEN           , .RstCtrlMask = RCC_APB1LRSTR_TIM2RST          },
#elif defined(RCC_APB1ENR1_TIM2EN)
  { .BlockId = RCC_BLOCK_TIM2          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM2EN           , .LpCtrlMask = RCC_APB1LPENR1_TIM2LPEN           , .RstCtrlMask = RCC_APB1RSTR1_TIM2RST          },
#endif
#if defined(RCC_APB1LENR_TIM3EN)
  { .BlockId = RCC_BLOCK_TIM3          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM3EN           , .LpCtrlMask = RCC_APB1LLPENR_TIM3LPEN           , .RstCtrlMask = RCC_APB1LRSTR_TIM3RST          },
#elif defined(RCC_APB1ENR1_TIM3EN)
  { .BlockId = RCC_BLOCK_TIM3          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM3EN           , .LpCtrlMask = RCC_APB1LPENR1_TIM3LPEN           , .RstCtrlMask = RCC_APB1RSTR1_TIM3RST          },
#endif
#if defined(RCC_APB1LENR_TIM4EN)
  { .BlockId = RCC_BLOCK_TIM4          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM4EN           , .LpCtrlMask = RCC_APB1LLPENR_TIM4LPEN           , .RstCtrlMask = RCC_APB1LRSTR_TIM4RST          },
#elif defined(RCC_APB1ENR1_TIM4EN)
  { .BlockId = RCC_BLOCK_TIM4          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM4EN           , .LpCtrlMask = RCC_APB1LPENR1_TIM4LPEN           , .RstCtrlMask = RCC_APB1RSTR1_TIM4RST          },
#endif
#if defined(RCC_APB1LENR_TIM5EN)
  { .BlockId = RCC_BLOCK_TIM5          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM5EN           , .LpCtrlMask = RCC_APB1LLPENR_TIM5LPEN           , .RstCtrlMask = RCC_APB1LRSTR_TIM5RST          },
#elif defined(RCC_APB1ENR1_TIM5EN)
  { .BlockId = RCC_BLOCK_TIM5          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM5EN           , .LpCtrlMask = RCC_APB1LPENR1_TIM5LPEN           , .RstCtrlMask = RCC_APB1RSTR1_TIM5RST          },
#endif
#if defined(RCC_APB1LENR_TIM6EN)
  { .BlockId = RCC_BLOCK_TIM6          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM6EN           , .LpCtrlMask = RCC_APB1LLPENR_TIM6LPEN           , .RstCtrlMask = RCC_APB1LRSTR_TIM6RST          },
#elif defined(RCC_APB1ENR1_TIM6EN)
  { .BlockId = RCC_BLOCK_TIM6          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM6EN           , .LpCtrlMask = RCC_APB1LPENR1_TIM6LPEN           , .RstCtrlMask = RCC_APB1RSTR1_TIM6RST          },
#endif
#if defined(RCC_APB1LENR_TIM7EN)
  { .BlockId = RCC_BLOCK_TIM7          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM7EN           , .LpCtrlMask = RCC_APB1LLPENR_TIM7LPEN           , .RstCtrlMask = RCC_APB1LRSTR_TIM7RST          },
#elif defined(RCC_APB1ENR1_TIM7EN)
  { .BlockId = RCC_BLOCK_TIM7          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM7EN           , .LpCtrlMask = RCC_APB1LPENR1_TIM7LPEN           , .RstCtrlMask = RCC_APB1RSTR1_TIM7RST          },
#endif
#if defined(RCC_APB2ENR_TIM8EN)
  { .BlockId = RCC_BLOCK_TIM8          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_TIM8EN            , .LpCtrlMask = RCC_APB2LPENR_TIM8LPEN            , .RstCtrlMask = RCC_APB2RSTR_TIM8RST           },
#endif
#if defined(RCC_APB2ENR_TIM9EN)
  { .BlockId = RCC_BLOCK_TIM9          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_TIM9EN            , .LpCtrlMask = RCC_APB2LPENR_TIM9LPEN            , .RstCtrlMask = RCC_APB2RSTR_TIM9RST           },
#endif
#if defined(RCC_APB1LENR_TIM12EN)
  { .BlockId = RCC_BLOCK_TIM12         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM12EN          , .LpCtrlMask = RCC_APB1LLPENR_TIM12LPEN          , .RstCtrlMask = RCC_APB1LRSTR_TIM12RST         },
#elif defined(RCC_APB1ENR1_TIM12EN)
  { .BlockId = RCC_BLOCK_TIM12         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM12EN          , .LpCtrlMask = RCC_APB1LPENR1_TIM12LPEN          , .RstCtrlMask = RCC_APB1RSTR1_TIM12RST         },
#endif
#if defined(RCC_APB1LENR_TIM13EN)
  { .BlockId = RCC_BLOCK_TIM13         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM13EN          , .LpCtrlMask = RCC_APB1LLPENR_TIM13LPEN          , .RstCtrlMask = RCC_APB1LRSTR_TIM13RST         },
#elif defined(RCC_APB1ENR1_TIM13EN)
  { .BlockId = RCC_BLOCK_TIM13         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM13EN          , .LpCtrlMask = RCC_APB1LPENR1_TIM13LPEN          , .RstCtrlMask = RCC_APB1RSTR1_TIM13RST         },
#endif
#if defined(RCC_APB1LENR_TIM14EN)
  { .BlockId = RCC_BLOCK_TIM14         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_TIM14EN          , .LpCtrlMask = RCC_APB1LLPENR_TIM14LPEN          , .RstCtrlMask = RCC_APB1LRSTR_TIM14RST         },
#elif defined(RCC_APB1ENR1_TIM14EN)
  { .BlockId = RCC_BLOCK_TIM14         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_TIM14EN          , .LpCtrlMask = RCC_APB1LPENR1_TIM14LPEN          , .RstCtrlMask = RCC_APB1RSTR1_TIM14RST         },
#endif
  { .BlockId = RCC_BLOCK_TIM15         , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_TIM15EN           , .LpCtrlMask = RCC_APB2LPENR_TIM15LPEN           , .RstCtrlMask = RCC_APB2RSTR_TIM15RST          },
  { .BlockId = RCC_BLOCK_TIM16         , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_TIM16EN           , .LpCtrlMask = RCC_APB2LPENR_TIM16LPEN           , .RstCtrlMask = RCC_APB2RSTR_TIM16RST          },
  { .BlockId = RCC_BLOCK_TIM17         , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_TIM17EN           , .LpCtrlMask = RCC_APB2LPENR_TIM17LPEN           , .RstCtrlMask = RCC_APB2RSTR_TIM17RST          },
#if defined(RCC_APB1HENR_TIM23EN)
  { .BlockId = RCC_BLOCK_TIM23         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_TIM23EN          , .LpCtrlMask = RCC_APB1HLPENR_TIM23LPEN          , .RstCtrlMask = RCC_APB1HRSTR_TIM23RST         },
#endif
#if defined(RCC_APB1HENR_TIM24EN)
  { .BlockId = RCC_BLOCK_TIM24         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_TIM24EN          , .LpCtrlMask = RCC_APB1HLPENR_TIM24LPEN          , .RstCtrlMask = RCC_APB1HRSTR_TIM24RST         },
#endif
#if defined(RCC_APB2ENR_HRTIMEN)
  { .BlockId = RCC_BLOCK_HRTIM         , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_HRTIMEN           , .LpCtrlMask = RCC_APB2LPENR_HRTIMLPEN           , .RstCtrlMask = RCC_APB2RSTR_HRTIMRST          },
#endif
#if defined(RCC_APB1LENR_LPTIM1EN)
  { .BlockId = RCC_BLOCK_LPTIM1        , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_LPTIM1EN         , .LpCtrlMask = RCC_APB1LLPENR_LPTIM1LPEN         , .RstCtrlMask = RCC_APB1LRSTR_LPTIM1RST        },
#elif defined(RCC_APB1ENR1_LPTIM1EN)
  { .BlockId = RCC_BLOCK_LPTIM1        , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_LPTIM1EN         , .LpCtrlMask = RCC_APB1LPENR1_LPTIM1LPEN         , .RstCtrlMask = RCC_APB1RSTR1_LPTIM1RST        },
#endif
  { .BlockId = RCC_BLOCK_LPTIM2        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_LPTIM2EN          , .LpCtrlMask = RCC_APB4LPENR_LPTIM2LPEN          , .RstCtrlMask = RCC_APB4RSTR_LPTIM2RST         },
  { .BlockId = RCC_BLOCK_LPTIM3        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_LPTIM3EN          , .LpCtrlMask = RCC_APB4LPENR_LPTIM3LPEN          , .RstCtrlMask = RCC_APB4RSTR_LPTIM3RST         },
#if defined(RCC_APB4ENR_LPTIM4EN)
  { .BlockId = RCC_BLOCK_LPTIM4        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_LPTIM4EN          , .LpCtrlMask = RCC_APB4LPENR_LPTIM4LPEN          , .RstCtrlMask = RCC_APB4RSTR_LPTIM4RST         },
#endif
#if defined(RCC_APB4ENR_LPTIM5EN)
  { .BlockId = RCC_BLOCK_LPTIM5        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_LPTIM5EN          , .LpCtrlMask = RCC_APB4LPENR_LPTIM5LPEN          , .RstCtrlMask = RCC_APB4RSTR_LPTIM5RST         },
#endif

  /*------------------------------ Connectivity -----------------------------*/
  { .BlockId = RCC_BLOCK_SPI1          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_SPI1EN            , .LpCtrlMask = RCC_APB2LPENR_SPI1LPEN            , .RstCtrlMask = RCC_APB2RSTR_SPI1RST           },
#if defined(RCC_APB1LENR_SPI2EN)
  { .BlockId = RCC_BLOCK_SPI2          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_SPI2EN           , .LpCtrlMask = RCC_APB1LLPENR_SPI2LPEN           , .RstCtrlMask = RCC_APB1LRSTR_SPI2RST          },
#elif defined(RCC_APB1ENR1_SPI2EN)
  { .BlockId = RCC_BLOCK_SPI2          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_SPI2EN           , .LpCtrlMask = RCC_APB1LPENR1_SPI2LPEN           , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST          },
#endif
#if defined(RCC_APB1LENR_SPI3EN)
  { .BlockId = RCC_BLOCK_SPI3          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_SPI3EN           , .LpCtrlMask = RCC_APB1LLPENR_SPI3LPEN           , .RstCtrlMask = RCC_APB1LRSTR_SPI3RST          },
#elif defined(RCC_APB1ENR1_SPI3EN)
  { .BlockId = RCC_BLOCK_SPI3          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_SPI3EN           , .LpCtrlMask = RCC_APB1LPENR1_SPI3LPEN           , .RstCtrlMask = RCC_APB1RSTR1_SPI3RST          },
#endif
  { .BlockId = RCC_BLOCK_SPI4          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_SPI4EN            , .LpCtrlMask = RCC_APB2LPENR_SPI4LPEN            , .RstCtrlMask = RCC_APB2RSTR_SPI4RST           },
  { .BlockId = RCC_BLOCK_SPI5          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_SPI5EN            , .LpCtrlMask = RCC_APB2LPENR_SPI5LPEN            , .RstCtrlMask = RCC_APB2RSTR_SPI5RST           },
  { .BlockId = RCC_BLOCK_SPI6          , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_SPI6EN            , .LpCtrlMask = RCC_APB4LPENR_SPI6LPEN            , .RstCtrlMask = RCC_APB4RSTR_SPI6RST           },
#if defined(RCC_APB1LENR_I2C1EN)
  { .BlockId = RCC_BLOCK_I2C1          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_I2C1EN           , .LpCtrlMask = RCC_APB1LLPENR_I2C1LPEN           , .RstCtrlMask = RCC_APB1LRSTR_I2C1RST          },
#elif defined(RCC_APB1ENR1_I2C1_I3C1EN)
  { .BlockId = RCC_BLOCK_I2C1          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_I2C1_I3C1EN      , .LpCtrlMask = RCC_APB1LPENR1_I2C1_I3C1LPEN      , .RstCtrlMask = RCC_APB1RSTR1_I2C1_I3C1RST     },
#endif
#if defined(RCC_APB1LENR_I2C2EN)
  { .BlockId = RCC_BLOCK_I2C2          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_I2C2EN           , .LpCtrlMask = RCC_APB1LLPENR_I2C2LPEN           , .RstCtrlMask = RCC_APB1LRSTR_I2C2RST          },
#elif defined(RCC_APB1ENR1_I2C2EN)
  { .BlockId = RCC_BLOCK_I2C2          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_I2C2EN           , .LpCtrlMask = RCC_APB1LPENR1_I2C2LPEN           , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST          },
#endif
#if defined(RCC_APB1LENR_I2C3EN)
  { .BlockId = RCC_BLOCK_I2C3          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_I2C3EN           , .LpCtrlMask = RCC_APB1LLPENR_I2C3LPEN           , .RstCtrlMask = RCC_APB1LRSTR_I2C3RST          },
#elif defined(RCC_APB1ENR1_I2C3EN)
  { .BlockId = RCC_BLOCK_I2C3          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_I2C3EN           , .LpCtrlMask = RCC_APB1LPENR1_I2C3LPEN           , .RstCtrlMask = RCC_APB1RSTR1_I2C3RST          },
#endif
#if defined(RCC_APB4ENR_I2C4EN)
  { .BlockId = RCC_BLOCK_I2C4          , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_I2C4EN            , .LpCtrlMask = RCC_APB4LPENR_I2C4LPEN            , .RstCtrlMask = RCC_APB4RSTR_I2C4RST           },
#endif
#if defined(RCC_APB1LENR_I2C5EN)
  { .BlockId = RCC_BLOCK_I2C5          , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_I2C5EN           , .LpCtrlMask = RCC_APB1LLPENR_I2C5LPEN           , .RstCtrlMask = RCC_APB1LRSTR_I2C5RST          },
#endif
  { .BlockId = RCC_BLOCK_USART1        , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_USART1EN          , .LpCtrlMask = RCC_APB2LPENR_USART1LPEN          , .RstCtrlMask = RCC_APB2RSTR_USART1RST         },
#if defined(RCC_APB1LENR_USART2EN)
  { .BlockId = RCC_BLOCK_USART2        , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_USART2EN         , .LpCtrlMask = RCC_APB1LLPENR_USART2LPEN         , .RstCtrlMask = RCC_APB1LRSTR_USART2RST        },
#elif defined(RCC_APB1ENR1_USART2EN)
  { .BlockId = RCC_BLOCK_USART2        , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_USART2EN         , .LpCtrlMask = RCC_APB1LPENR1_USART2LPEN         , .RstCtrlMask = RCC_APB1RSTR1_USART2RST        },
#endif
#if defined(RCC_APB1LENR_USART3EN)
  { .BlockId = RCC_BLOCK_USART3        , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_USART3EN         , .LpCtrlMask = RCC_APB1LLPENR_USART3LPEN         , .RstCtrlMask = RCC_APB1LRSTR_USART3RST        },
#elif defined(RCC_APB1ENR1_USART3EN)
  { .BlockId = RCC_BLOCK_USART3        , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_USART3EN         , .LpCtrlMask = RCC_APB1LPENR1_USART3LPEN         , .RstCtrlMask = RCC_APB1RSTR1_USART3RST        },
#endif
#if defined(RCC_APB1LENR_UART4EN)
  { .BlockId = RCC_BLOCK_UART4         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_UART4EN          , .LpCtrlMask = RCC_APB1LLPENR_UART4LPEN          , .RstCtrlMask = RCC_APB1LRSTR_UART4RST         },
#elif defined(RCC_APB1ENR1_UART4EN)
  { .BlockId = RCC_BLOCK_UART4         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_UART4EN          , .LpCtrlMask = RCC_APB1LPENR1_UART4LPEN          , .RstCtrlMask = RCC_APB1RSTR1_UART4RST         },
#endif
#if defined(RCC_APB1LENR_UART5EN)
  { .BlockId = RCC_BLOCK_UART5         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_UART5EN          , .LpCtrlMask = RCC_APB1LLPENR_UART5LPEN          , .RstCtrlMask = RCC_APB1LRSTR_UART5RST         },
#elif defined(RCC_APB1ENR1_UART5EN)
  { .BlockId = RCC_BLOCK_UART5         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_UART5EN          , .LpCtrlMask = RCC_APB1LPENR1_UART5LPEN          , .RstCtrlMask = RCC_APB1RSTR1_UART5RST         },
#endif
#if defined(RCC_APB2ENR_USART6EN)
  { .BlockId = RCC_BLOCK_USART6        , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_USART6EN          , .LpCtrlMask = RCC_APB2LPENR_USART6LPEN          , .RstCtrlMask = RCC_APB2RSTR_USART6RST         },
#endif
#if defined(RCC_APB1LENR_UART7EN)
  { .BlockId = RCC_BLOCK_UART7         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_UART7EN          , .LpCtrlMask = RCC_APB1LLPENR_UART7LPEN          , .RstCtrlMask = RCC_APB1LRSTR_UART7RST         },
#elif defined(RCC_APB1ENR1_UART7EN)
  { .BlockId = RCC_BLOCK_UART7         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_UART7EN          , .LpCtrlMask = RCC_APB1LPENR1_UART7LPEN          , .RstCtrlMask = RCC_APB1RSTR1_UART7RST         },
#endif
#if defined(RCC_APB1LENR_UART8EN)
  { .BlockId = RCC_BLOCK_UART8         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_UART8EN          , .LpCtrlMask = RCC_APB1LLPENR_UART8LPEN          , .RstCtrlMask = RCC_APB1LRSTR_UART8RST         },
#elif defined(RCC_APB1ENR1_UART8EN)
  { .BlockId = RCC_BLOCK_UART8         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_UART8EN          , .LpCtrlMask = RCC_APB1LPENR1_UART8LPEN          , .RstCtrlMask = RCC_APB1RSTR1_UART8RST         },
#endif
#if defined(RCC_APB2ENR_UART9EN)
  { .BlockId = RCC_BLOCK_UART9         , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_UART9EN           , .LpCtrlMask = RCC_APB2LPENR_UART9LPEN           , .RstCtrlMask = RCC_APB2RSTR_UART9RST          },
#endif
#if defined(RCC_APB2ENR_USART10EN)
  { .BlockId = RCC_BLOCK_USART10       , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_USART10EN         , .LpCtrlMask = RCC_APB2LPENR_USART10LPEN         , .RstCtrlMask = RCC_APB2RSTR_USART10RST        },
#endif
  { .BlockId = RCC_BLOCK_LPUART1       , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_LPUART1EN         , .LpCtrlMask = RCC_APB4LPENR_LPUART1LPEN         , .RstCtrlMask = RCC_APB4RSTR_LPUART1RST        },
#if defined(RCC_APB1HENR_FDCANEN)
  { .BlockId = RCC_BLOCK_FDCAN         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_FDCANEN          , .LpCtrlMask = RCC_APB1HLPENR_FDCANLPEN          , .RstCtrlMask = RCC_APB1HRSTR_FDCANRST         },
#elif defined(RCC_APB1ENR2_FDCANEN)
  { .BlockId = RCC_BLOCK_FDCAN         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1ENR2_FDCANEN          , .LpCtrlMask = RCC_APB1LPENR2_FDCANLPEN          , .RstCtrlMask = RCC_APB1RSTR2_FDCANRST         },
#endif
#if defined(RCC_AHB3ENR_SDMMC1EN)
  { .BlockId = RCC_BLOCK_SDMMC1        , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_SDMMC1EN          , .LpCtrlMask = RCC_AHB3LPENR_SDMMC1LPEN          , .RstCtrlMask = RCC_AHB3RSTR_SDMMC1RST         },
#elif defined(RCC_AHB5ENR_SDMMC1EN)
  { .BlockId = RCC_BLOCK_SDMMC1        , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_SDMMC1EN          , .LpCtrlMask = RCC_AHB5LPENR_SDMMC1LPEN          , .RstCtrlMask = RCC_AHB5RSTR_SDMMC1RST         },
#endif
  { .BlockId = RCC_BLOCK_SDMMC2        , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_SDMMC2EN          , .LpCtrlMask = RCC_AHB2LPENR_SDMMC2LPEN          , .RstCtrlMask = RCC_AHB2RSTR_SDMMC2RST         },
#if defined(RCC_AHB3ENR_FMCEN)
  { .BlockId = RCC_BLOCK_FMC           , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_FMCEN             , .LpCtrlMask = RCC_AHB3LPENR_FMCLPEN             , .RstCtrlMask = RCC_AHB3RSTR_FMCRST            },
#elif defined(RCC_AHB5ENR_FMCEN)
  { .BlockId = RCC_BLOCK_FMC           , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_FMCEN             , .LpCtrlMask = RCC_AHB5LPENR_FMCLPEN             , .RstCtrlMask = RCC_AHB5RSTR_FMCRST            },
#endif
#if defined(RCC_AHB3ENR_QSPIEN)
  { .BlockId = RCC_BLOCK_QSPI          , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_QSPIEN            , .LpCtrlMask = RCC_AHB3LPENR_QSPILPEN            , .RstCtrlMask = RCC_AHB3RSTR_QSPIRST           },
#endif
#if defined(RCC_AHB3ENR_OSPI1EN)
  { .BlockId = RCC_BLOCK_OSPI1         , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_OSPI1EN           , .LpCtrlMask = RCC_AHB3LPENR_OSPI1LPEN           , .RstCtrlMask = RCC_AHB3RSTR_OSPI1RST          },
#endif
#if defined(RCC_AHB3ENR_OSPI2EN)
  { .BlockId = RCC_BLOCK_OSPI2         , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_OSPI2EN           , .LpCtrlMask = RCC_AHB3LPENR_OSPI2LPEN           , .RstCtrlMask = RCC_AHB3RSTR_OSPI2RST          },
#endif
#if defined(RCC_AHB3ENR_OTFDEC1EN)
  { .BlockId = RCC_BLOCK_OTFDEC1       , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_OTFDEC1EN         , .LpCtrlMask = RCC_AHB3LPENR_OTFDEC1LPEN         , .RstCtrlMask = RCC_AHB3RSTR_OTFDEC1RST        },
#endif
#if defined(RCC_AHB3ENR_OTFDEC2EN)
  { .BlockId = RCC_BLOCK_OTFDEC2       , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_OTFDEC2EN         , .LpCtrlMask = RCC_AHB3LPENR_OTFDEC2LPEN         , .RstCtrlMask = RCC_AHB3RSTR_OTFDEC2RST        },
#endif
#if defined(RCC_AHB5ENR_XSPI1EN)
  { .BlockId = RCC_BLOCK_XSPI1         , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_XSPI1EN           , .LpCtrlMask = RCC_AHB5LPENR_XSPI1LPEN           , .RstCtrlMask = RCC_AHB5RSTR_XSPI1RST          },
#endif
#if defined(RCC_AHB5ENR_XSPI2EN)
  { .BlockId = RCC_BLOCK_XSPI2         , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_XSPI2EN           , .LpCtrlMask = RCC_AHB5LPENR_XSPI2LPEN           , .RstCtrlMask = RCC_AHB5RSTR_XSPI2RST          },
#endif
#if defined(RCC_AHB5ENR_XSPIMEN)
  { .BlockId = RCC_BLOCK_XSPIM         , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_XSPIMEN           , .LpCtrlMask = RCC_AHB5LPENR_XSPIMLPEN           , .RstCtrlMask = RCC_AHB5RSTR_XSPIMRST          },
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSEN)
  { .BlockId = RCC_BLOCK_USB1OTGHS     , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_USB1OTGHSEN       , .LpCtrlMask = RCC_AHB1LPENR_USB1OTGHSLPEN       , .RstCtrlMask = RCC_AHB1RSTR_USB1OTGHSRST      },
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSULPIEN)
  { .BlockId = RCC_BLOCK_USB1OTGHSULPI , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_USB1OTGHSULPIEN   , .LpCtrlMask = RCC_AHB1LPENR_USB1OTGHSULPILPEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSEN)
  { .BlockId = RCC_BLOCK_USB2OTGFS     , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_USB2OTGFSEN       , .LpCtrlMask = RCC_AHB1LPENR_USB2OTGFSLPEN       , .RstCtrlMask = RCC_AHB1RSTR_USB2OTGFSRST      },
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSULPIEN)
  { .BlockId = RCC_BLOCK_USB2OTGFSULPI , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_USB2OTGFSULPIEN   , .LpCtrlMask = RCC_AHB1LPENR_USB2OTGFSULPILPEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB1ENR_OTGHSEN)
  { .BlockId = RCC_BLOCK_OTGHS         , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_OTGHSEN           , .LpCtrlMask = RCC_AHB1LPENR_OTGHSLPEN           , .RstCtrlMask = RCC_AHB1RSTR_OTGHSRST          },
#endif
#if defined(RCC_AHB1ENR_OTGFSEN)
  { .BlockId = RCC_BLOCK_OTGFS         , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_OTGFSEN           , .LpCtrlMask = RCC_AHB1LPENR_OTGFSLPEN           , .RstCtrlMask = RCC_AHB1RSTR_OTGFSRST          },
#endif
#if defined(RCC_AHB1ENR_USBPHYCEN)
  { .BlockId = RCC_BLOCK_USBPHYC       , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_USBPHYCEN         , .LpCtrlMask = RCC_AHB1LPENR_USBPHYCLPEN         , .RstCtrlMask = RCC_AHB1RSTR_USBPHYCRST        },
#endif
#if defined(RCC_APB1ENR2_UCPD1EN)
  { .BlockId = RCC_BLOCK_UCPD1         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1ENR2_UCPD1EN          , .LpCtrlMask = RCC_APB1LPENR2_UCPD1LPEN          , .RstCtrlMask = RCC_APB1RSTR2_UCPD1RST         },
#endif
#if defined(RCC_AHB1ENR_ETH1MACEN)
#if defined(RCC_AHB1RSTR_ETH1MACRST)
  { .BlockId = RCC_BLOCK_ETH1MAC       , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ETH1MACEN         , .LpCtrlMask = RCC_AHB1LPENR_ETH1MACLPEN         , .RstCtrlMask = RCC_AHB1RSTR_ETH1MACRST        },
#else
  { .BlockId = RCC_BLOCK_ETH1MAC       , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ETH1MACEN         , .LpCtrlMask = RCC_AHB1LPENR_ETH1MACLPEN         , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#endif
#if defined(RCC_AHB1ENR_ETH1TXEN)
  { .BlockId = RCC_BLOCK_ETH1TX        , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ETH1TXEN          , .LpCtrlMask = RCC_AHB1LPENR_ETH1TXLPEN          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_AHB1ENR_ETH1RXEN)
  { .BlockId = RCC_BLOCK_ETH1RX        , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ETH1RXEN          , .LpCtrlMask = RCC_AHB1LPENR_ETH1RXLPEN          , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION       },
#endif
#if defined(RCC_APB1HENR_MDIOSEN)
  { .BlockId = RCC_BLOCK_MDIOS         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_MDIOSEN          , .LpCtrlMask = RCC_APB1HLPENR_MDIOSLPEN          , .RstCtrlMask = RCC_APB1HRSTR_MDIOSRST         },
#elif defined(RCC_APB1ENR2_MDIOSEN)
  { .BlockId = RCC_BLOCK_MDIOS         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1ENR2_MDIOSEN          , .LpCtrlMask = RCC_APB1LPENR2_MDIOSLPEN          , .RstCtrlMask = RCC_APB1RSTR2_MDIOSRST         },
#endif
#if defined(RCC_APB1HENR_SWPMIEN)
  { .BlockId = RCC_BLOCK_SWPMI         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_SWPMIEN          , .LpCtrlMask = RCC_APB1HLPENR_SWPMILPEN          , .RstCtrlMask = RCC_APB1HRSTR_SWPMIRST         },
#endif
#if defined(RCC_APB1LENR_CECEN)
  { .BlockId = RCC_BLOCK_CEC           , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_CECEN            , .LpCtrlMask = RCC_APB1LLPENR_CECLPEN            , .RstCtrlMask = RCC_APB1LRSTR_CECRST           },
#elif defined(RCC_APB1ENR1_CECEN)
  { .BlockId = RCC_BLOCK_CEC           , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_CECEN            , .LpCtrlMask = RCC_APB1LPENR1_CECLPEN            , .RstCtrlMask = RCC_APB1RSTR1_CECRST           },
#endif
#if defined(RCC_APB1LENR_SPDIFRXEN)
  { .BlockId = RCC_BLOCK_SPDIFRX       , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_SPDIFRXEN        , .LpCtrlMask = RCC_APB1LLPENR_SPDIFRXLPEN        , .RstCtrlMask = RCC_APB1LRSTR_SPDIFRXRST       },
#elif defined(RCC_APB1ENR1_SPDIFRXEN)
  { .BlockId = RCC_BLOCK_SPDIFRX       , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1ENR1_SPDIFRXEN        , .LpCtrlMask = RCC_APB1LPENR1_SPDIFRXLPEN        , .RstCtrlMask = RCC_APB1RSTR1_SPDIFRXRST       },
#endif
#if defined(RCC_APB2ENR_DFSDM1EN)
  { .BlockId = RCC_BLOCK_DFSDM1        , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_DFSDM1EN          , .LpCtrlMask = RCC_APB2LPENR_DFSDM1LPEN          , .RstCtrlMask = RCC_APB2RSTR_DFSDM1RST         },
#endif
#if defined(RCC_APB4ENR_DFSDM2EN)
  { .BlockId = RCC_BLOCK_DFSDM2        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_DFSDM2EN          , .LpCtrlMask = RCC_APB4LPENR_DFSDM2LPEN          , .RstCtrlMask = RCC_APB4RSTR_DFSDM2RST         },
#endif

  /*------------------------------ Multimedia -------------------------------*/
  { .BlockId = RCC_BLOCK_SAI1          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_SAI1EN            , .LpCtrlMask = RCC_APB2LPENR_SAI1LPEN            , .RstCtrlMask = RCC_APB2RSTR_SAI1RST           },
#if defined(RCC_APB2ENR_SAI2EN)
  { .BlockId = RCC_BLOCK_SAI2          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_SAI2EN            , .LpCtrlMask = RCC_APB2LPENR_SAI2LPEN            , .RstCtrlMask = RCC_APB2RSTR_SAI2RST           },
#endif
#if defined(RCC_APB2ENR_SAI3EN)
  { .BlockId = RCC_BLOCK_SAI3          , .ClkBusId = RCC_CLK_BUS_APB2    , .StateMask = RCC_APB2ENR_SAI3EN            , .LpCtrlMask = RCC_APB2LPENR_SAI3LPEN            , .RstCtrlMask = RCC_APB2RSTR_SAI3RST           },
#endif
#if defined(RCC_APB4ENR_SAI4EN)
  { .BlockId = RCC_BLOCK_SAI4          , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_SAI4EN            , .LpCtrlMask = RCC_APB4LPENR_SAI4LPEN            , .RstCtrlMask = RCC_APB4RSTR_SAI4RST           },
#endif
#if defined(RCC_AHB1ENR_ADF1EN)
  { .BlockId = RCC_BLOCK_ADF1          , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ADF1EN            , .LpCtrlMask = RCC_AHB1LPENR_ADF1LPEN            , .RstCtrlMask = RCC_AHB1RSTR_ADF1RST           },
#endif
#if defined(RCC_AHB2ENR_DCMIEN)
  { .BlockId = RCC_BLOCK_DCMI          , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_DCMIEN            , .LpCtrlMask = RCC_AHB2LPENR_DCMILPEN            , .RstCtrlMask = RCC_AHB2RSTR_DCMIRST           },
#endif
#if defined(RCC_AHB2ENR_DCMI_PSSIEN)
  { .BlockId = RCC_BLOCK_DCMI_PSSI     , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_DCMI_PSSIEN       , .LpCtrlMask = RCC_AHB2LPENR_DCMI_PSSILPEN       , .RstCtrlMask = RCC_AHB2RSTR_DCMI_PSSIRST      },
#endif
#if defined(RCC_APB5ENR_DCMIPPEN)
  { .BlockId = RCC_BLOCK_DCMIPP        , .ClkBusId = RCC_CLK_BUS_APB5    , .StateMask = RCC_APB5ENR_DCMIPPEN          , .LpCtrlMask = RCC_APB5LPENR_DCMIPPLPEN          , .RstCtrlMask = RCC_APB5RSTR_DCMIPPRST         },
#endif
#if defined(RCC_AHB2ENR_PSSIEN)
  { .BlockId = RCC_BLOCK_PSSI          , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_PSSIEN            , .LpCtrlMask = RCC_AHB2LPENR_PSSILPEN            , .RstCtrlMask = RCC_AHB2RSTR_PSSIRST           },
#endif
#if defined(RCC_APB3ENR_LTDCEN)
  { .BlockId = RCC_BLOCK_LTDC          , .ClkBusId = RCC_CLK_BUS_APB3    , .StateMask = RCC_APB3ENR_LTDCEN            , .LpCtrlMask = RCC_APB3LPENR_LTDCLPEN            , .RstCtrlMask = RCC_APB3RSTR_LTDCRST           },
#elif defined(RCC_APB5ENR_LTDCEN)
  { .BlockId = RCC_BLOCK_LTDC          , .ClkBusId = RCC_CLK_BUS_APB5    , .StateMask = RCC_APB5ENR_LTDCEN            , .LpCtrlMask = RCC_APB5LPENR_LTDCLPEN            , .RstCtrlMask = RCC_APB5RSTR_LTDCRST           },
#endif
#if defined(RCC_APB3ENR_DSIEN)
  { .BlockId = RCC_BLOCK_DSI           , .ClkBusId = RCC_CLK_BUS_APB3    , .StateMask = RCC_APB3ENR_DSIEN             , .LpCtrlMask = RCC_APB3LPENR_DSILPEN             , .RstCtrlMask = RCC_APB3RSTR_DSIRST            },
#endif
#if defined(RCC_AHB3ENR_JPGDECEN)
  { .BlockId = RCC_BLOCK_JPGDEC        , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_JPGDECEN          , .LpCtrlMask = RCC_AHB3LPENR_JPGDECLPEN          , .RstCtrlMask = RCC_AHB3RSTR_JPGDECRST         },
#endif
#if defined(RCC_AHB5ENR_JPEGEN)
  { .BlockId = RCC_BLOCK_JPEG          , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_JPEGEN            , .LpCtrlMask = RCC_AHB5LPENR_JPEGLPEN            , .RstCtrlMask = RCC_AHB5RSTR_JPEGRST           },
#endif
#if defined(RCC_AHB3ENR_GFXMMUEN)
  { .BlockId = RCC_BLOCK_GFXMMU        , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_GFXMMUEN          , .LpCtrlMask = RCC_AHB3LPENR_GFXMMULPEN          , .RstCtrlMask = RCC_AHB3RSTR_GFXMMURST         },
#elif defined(RCC_AHB5ENR_GFXMMUEN)
  { .BlockId = RCC_BLOCK_GFXMMU        , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_GFXMMUEN          , .LpCtrlMask = RCC_AHB5LPENR_GFXMMULPEN          , .RstCtrlMask = RCC_AHB5RSTR_GFXMMURST         },
#endif
#if defined(RCC_APB5ENR_GFXTIMEN)
  { .BlockId = RCC_BLOCK_GFXTIM        , .ClkBusId = RCC_CLK_BUS_APB5    , .StateMask = RCC_APB5ENR_GFXTIMEN          , .LpCtrlMask = RCC_APB5LPENR_GFXTIMLPEN          , .RstCtrlMask = RCC_APB5RSTR_GFXTIMRST         },
#endif
#if defined(RCC_AHB5ENR_GPU2DEN)
  { .BlockId = RCC_BLOCK_GPU2D         , .ClkBusId = RCC_CLK_BUS_AHB5    , .StateMask = RCC_AHB5ENR_GPU2DEN           , .LpCtrlMask = RCC_AHB5LPENR_GPU2DLPEN           , .RstCtrlMask = RCC_AHB5RSTR_GPU2DRST          },
#endif

  /*------------------------------ Analog -----------------------------------*/
  { .BlockId = RCC_BLOCK_ADC12         , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_ADC12EN           , .LpCtrlMask = RCC_AHB1LPENR_ADC12LPEN           , .RstCtrlMask = RCC_AHB1RSTR_ADC12RST          },
#if defined(RCC_AHB4ENR_ADC3EN)
  { .BlockId = RCC_BLOCK_ADC3          , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_ADC3EN            , .LpCtrlMask = RCC_AHB4LPENR_ADC3LPEN            , .RstCtrlMask = RCC_AHB4RSTR_ADC3RST           },
#endif
#if defined(RCC_APB1LENR_DAC12EN)
  { .BlockId = RCC_BLOCK_DAC12         , .ClkBusId = RCC_CLK_BUS_APB1_1  , .StateMask = RCC_APB1LENR_DAC12EN          , .LpCtrlMask = RCC_APB1LLPENR_DAC12LPEN          , .RstCtrlMask = RCC_APB1LRSTR_DAC12RST         },
#endif
#if defined(RCC_APB4ENR_DAC2EN)
  { .BlockId = RCC_BLOCK_DAC2          , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_DAC2EN            , .LpCtrlMask = RCC_APB4LPENR_DAC2LPEN            , .RstCtrlMask = RCC_APB4RSTR_DAC2RST           },
#endif
#if defined(RCC_APB4ENR_COMP12EN)
  { .BlockId = RCC_BLOCK_COMP12        , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_COMP12EN          , .LpCtrlMask = RCC_APB4LPENR_COMP12LPEN          , .RstCtrlMask = RCC_APB4RSTR_COMP12RST         },
#endif
#if defined(RCC_APB1HENR_OPAMPEN)
  { .BlockId = RCC_BLOCK_OPAMP         , .ClkBusId = RCC_CLK_BUS_APB1_2  , .StateMask = RCC_APB1HENR_OPAMPEN          , .LpCtrlMask = RCC_APB1HLPENR_OPAMPLPEN          , .RstCtrlMask = RCC_APB1HRSTR_OPAMPRST         },
#endif
  { .BlockId = RCC_BLOCK_VREF          , .ClkBusId = RCC_CLK_BUS_APB4    , .StateMask = RCC_APB4ENR_VREFEN            , .LpCtrlMask = RCC_APB4LPENR_VREFLPEN            , .RstCtrlMask = RCC_APB4RSTR_VREFRST           },

  /*------------------------------ Security ---------------------------------*/
#if defined(RCC_AHB2ENR_CRYPEN)
  { .BlockId = RCC_BLOCK_CRYP          , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_CRYPEN            , .LpCtrlMask = RCC_AHB2LPENR_CRYPLPEN            , .RstCtrlMask = RCC_AHB2RSTR_CRYPRST           },
#elif defined(RCC_AHB3ENR_CRYPEN)
  { .BlockId = RCC_BLOCK_CRYP          , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_CRYPEN            , .LpCtrlMask = RCC_AHB3LPENR_CRYPLPEN            , .RstCtrlMask = RCC_AHB3RSTR_CRYPRST           },
#endif
#if defined(RCC_AHB2ENR_HASHEN)
  { .BlockId = RCC_BLOCK_HASH          , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_HASHEN            , .LpCtrlMask = RCC_AHB2LPENR_HASHLPEN            , .RstCtrlMask = RCC_AHB2RSTR_HASHRST           },
#elif defined(RCC_AHB3ENR_HASHEN)
  { .BlockId = RCC_BLOCK_HASH          , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_HASHEN            , .LpCtrlMask = RCC_AHB3LPENR_HASHLPEN            , .RstCtrlMask = RCC_AHB3RSTR_HASHRST           },
#endif
#if defined(RCC_AHB2ENR_RNGEN)
  { .BlockId = RCC_BLOCK_RNG           , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_RNGEN             , .LpCtrlMask = RCC_AHB2LPENR_RNGLPEN             , .RstCtrlMask = RCC_AHB2RSTR_RNGRST            },
#elif defined(RCC_AHB3ENR_RNGEN)
  { .BlockId = RCC_BLOCK_RNG           , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_RNGEN             , .LpCtrlMask = RCC_AHB3LPENR_RNGLPEN             , .RstCtrlMask = RCC_AHB3RSTR_RNGRST            },
#endif
#if defined(RCC_AHB3ENR_PKAEN)
  { .BlockId = RCC_BLOCK_PKA           , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_PKAEN             , .LpCtrlMask = RCC_AHB3LPENR_PKALPEN             , .RstCtrlMask = RCC_AHB3RSTR_PKARST            },
#endif
#if defined(RCC_AHB3ENR_SAESEN)
  { .BlockId = RCC_BLOCK_SAES          , .ClkBusId = RCC_CLK_BUS_AHB3    , .StateMask = RCC_AHB3ENR_SAESEN            , .LpCtrlMask = RCC_AHB3LPENR_SAESLPEN            , .RstCtrlMask = RCC_AHB3RSTR_SAESRST           },
#endif

  /*------------------------------ Computing --------------------------------*/
#if defined(RCC_AHB1ENR_CRCEN)
  { .BlockId = RCC_BLOCK_CRC           , .ClkBusId = RCC_CLK_BUS_AHB1    , .StateMask = RCC_AHB1ENR_CRCEN             , .LpCtrlMask = RCC_AHB1LPENR_CRCLPEN             , .RstCtrlMask = RCC_AHB1RSTR_CRCRST            },
#elif defined(RCC_AHB4ENR_CRCEN)
  { .BlockId = RCC_BLOCK_CRC           , .ClkBusId = RCC_CLK_BUS_AHB4    , .StateMask = RCC_AHB4ENR_CRCEN             , .LpCtrlMask = RCC_AHB4LPENR_CRCLPEN             , .RstCtrlMask = RCC_AHB4RSTR_CRCRST            },
#endif
#if defined(RCC_AHB2ENR_CORDICEN)
  { .BlockId = RCC_BLOCK_CORDIC        , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_CORDICEN          , .LpCtrlMask = RCC_AHB2LPENR_CORDICLPEN          , .RstCtrlMask = RCC_AHB2RSTR_CORDICRST         },
#endif
#if defined(RCC_AHB2ENR_FMACEN)
  { .BlockId = RCC_BLOCK_FMAC          , .ClkBusId = RCC_CLK_BUS_AHB2    , .StateMask = RCC_AHB2ENR_FMACEN            , .LpCtrlMask = RCC_AHB2LPENR_FMACLPEN            , .RstCtrlMask = RCC_AHB2RSTR_FMACRST           },
#endif
};

_Static_assert( (sizeof(rcc_PeriphBlockConfig) / sizeof(rcc_BlockConfigStruct_t)) == RCC_BLOCK_LIST_CNT, "Rcc: rcc_PeriphBlockConfig has incorrect size." );

/* --------------------------- Reset source flags --------------------------- */

/** \brief RSR register flag masks, indexed by \ref rcc_ResetSrc_t */
static const uint32_t rcc_ResetSrcLut[] =
{
    RCC_RSR_PINRSTF        , /**< \ref RCC_RESET_SRC_PIN  */
    RCC_RSR_BORRSTF        , /**< \ref RCC_RESET_SRC_BOR  */
    RCC_RSR_SW_RESET_FLAG  , /**< \ref RCC_RESET_SRC_SW   */
    RCC_RSR_IWDG_RESET_FLAG, /**< \ref RCC_RESET_SRC_IWDG */
    RCC_RSR_WWDG_RESET_FLAG, /**< \ref RCC_RESET_SRC_WWDG */
    RCC_RSR_LPWR_RESET_FLAG, /**< \ref RCC_RESET_SRC_LPWR */
};

_Static_assert( (sizeof(rcc_ResetSrcLut) / sizeof(uint32_t)) == RCC_RESET_SRC_CNT, "Rcc: rcc_ResetSrcLut has incorrect size." );

/* ------------------------ Power supply and voltage ------------------------ */

/** \brief PWR_CR3 (PWR_CSR2 on STM32H7R / H7S) supply configuration values,
 *  indexed by \ref rcc_PwrSupply_t */
static const uint32_t rcc_PwrSupplyLut[] =
{
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_DEFAULT (not written) */
    LL_PWR_LDO_SUPPLY                       , /**< \ref RCC_PWR_SUPPLY_LDO                   */
#if defined(STM32H7RS)
    LL_PWR_DIRECT_SMPS_SUPPLY               , /**< \ref RCC_PWR_SUPPLY_DIRECT_SMPS           */
    LL_PWR_SMPS_1V8_SUPPLIES_LDO            , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_LDO          */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_LDO          */
    LL_PWR_SMPS_1V8_SUPPLIES_EXT_AND_LDO    , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_EXT_LDO      */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_EXT_LDO      */
    LL_PWR_SMPS_1V8_SUPPLIES_EXT            , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_EXT          */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_EXT          */
#elif defined(SMPS)
    LL_PWR_DIRECT_SMPS_SUPPLY               , /**< \ref RCC_PWR_SUPPLY_DIRECT_SMPS           */
    LL_PWR_SMPS_1V8_SUPPLIES_LDO            , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_LDO          */
    LL_PWR_SMPS_2V5_SUPPLIES_LDO            , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_LDO          */
    LL_PWR_SMPS_1V8_SUPPLIES_EXT_AND_LDO    , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_EXT_LDO      */
    LL_PWR_SMPS_2V5_SUPPLIES_EXT_AND_LDO    , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_EXT_LDO      */
    LL_PWR_SMPS_1V8_SUPPLIES_EXT            , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_EXT          */
    LL_PWR_SMPS_2V5_SUPPLIES_EXT            , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_EXT          */
#else
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_DIRECT_SMPS           */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_LDO          */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_LDO          */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_EXT_LDO      */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_EXT_LDO      */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_1V8_EXT          */
    RCC_PWR_SUPPLY_UNSUPPORTED              , /**< \ref RCC_PWR_SUPPLY_SMPS_2V5_EXT          */
#endif
    LL_PWR_EXTERNAL_SOURCE_SUPPLY           , /**< \ref RCC_PWR_SUPPLY_EXTERNAL_SOURCE       */
};

_Static_assert( (sizeof(rcc_PwrSupplyLut) / sizeof(uint32_t)) == RCC_PWR_SUPPLY_CNT, "Rcc: rcc_PwrSupplyLut has incorrect size." );


/** \brief PWR VOS field values, indexed by \ref rcc_PwrVoltageScale_t. VOS0 of
 *  STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 is VOS1 with SYSCFG
 *  overdrive (ODEN). STM32H7R / H7S: VOS high (scale 0) and VOS low (scale 1). */
static const uint32_t rcc_PwrVoltageScaleLut[] =
{
#if defined(STM32H7RS)
    LL_PWR_REGU_VOLTAGE_SCALE0, /**< \ref RCC_PWR_VOLTAGE_SCALE_0 (VOS high) */
    LL_PWR_REGU_VOLTAGE_SCALE1, /**< \ref RCC_PWR_VOLTAGE_SCALE_1 (VOS low)  */
    RCC_PWR_VOS_UNSUPPORTED   , /**< \ref RCC_PWR_VOLTAGE_SCALE_2            */
    RCC_PWR_VOS_UNSUPPORTED   , /**< \ref RCC_PWR_VOLTAGE_SCALE_3            */
#else
    LL_PWR_REGU_VOLTAGE_SCALE0, /**< \ref RCC_PWR_VOLTAGE_SCALE_0 */
    LL_PWR_REGU_VOLTAGE_SCALE1, /**< \ref RCC_PWR_VOLTAGE_SCALE_1 */
    LL_PWR_REGU_VOLTAGE_SCALE2, /**< \ref RCC_PWR_VOLTAGE_SCALE_2 */
    LL_PWR_REGU_VOLTAGE_SCALE3, /**< \ref RCC_PWR_VOLTAGE_SCALE_3 */
#endif
};

_Static_assert( (sizeof(rcc_PwrVoltageScaleLut) / sizeof(uint32_t)) == RCC_PWR_VOLTAGE_SCALE_CNT, "Rcc: rcc_PwrVoltageScaleLut has incorrect size." );

/* --------------------------- Flash wait states ---------------------------- */

/**
 * \brief Maximal AXI clock (HCLK) frequency in Hz for every number of flash wait
 *        states (column = wait states), rows indexed by \ref rcc_PwrVoltageScale_t.
 *        Value 0 - wait states not used in the voltage scale.
 *
 * Every limit is the lower one of the reference manual table and of the ST LL
 * driver (LL_SetFlashLatency).
 *
 * \note ST LL driver sets 2 wait states up to 240 MHz in VOS0 / VOS1 of
 *       STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757, the reference
 *       manual RM0433 requires 3 wait states above 210 MHz and 4 wait states above
 *       225 MHz - the LL driver is not used.
 * \note STM32H7R / H7S (RM0477): VOS high 40 MHz per wait state (max HCLK 300 MHz),
 *       VOS low 36 MHz per wait state (max HCLK 200 MHz).
 */
static const rcc_FreqHz_t rcc_FlashLatencyMaxHclk[ RCC_PWR_VOLTAGE_SCALE_CNT ][ RCC_FLASH_WS_TABLE_SIZE ] =
{
#if (STM32H7_DEV_ID == 0x450UL)
    /* RM0433 - STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 */
    { 70000000u, 140000000u, 210000000u, 225000000u, 240000000u,         0u,         0u }, /* VOS0 */
    { 70000000u, 140000000u, 210000000u, 225000000u,         0u,         0u,         0u }, /* VOS1 */
    { 55000000u, 110000000u, 165000000u, 220000000u,         0u,         0u,         0u }, /* VOS2 */
    { 45000000u,  90000000u, 135000000u, 180000000u, 224000000u,         0u,         0u }, /* VOS3 */
#elif (STM32H7_DEV_ID == 0x483UL)
    /* RM0468 - STM32H723 / H725 / H730 / H733 / H735 */
    { 70000000u, 140000000u, 210000000u, 275000000u,         0u,         0u,         0u }, /* VOS0 */
    { 67000000u, 133000000u, 200000000u,         0u,         0u,         0u,         0u }, /* VOS1 */
    { 50000000u, 100000000u, 150000000u,         0u,         0u,         0u,         0u }, /* VOS2 */
    { 35000000u,  70000000u,  85000000u,         0u,         0u,         0u,         0u }, /* VOS3 */
#elif (STM32H7_DEV_ID == 0x480UL)
    /* RM0455 - STM32H7A3 / H7B0 / H7B3 */
    { 42000000u,  84000000u, 126000000u, 168000000u, 210000000u, 252000000u, 280000000u }, /* VOS0 */
    { 38000000u,  76000000u, 114000000u, 152000000u, 190000000u, 225000000u,         0u }, /* VOS1 */
    { 34000000u,  68000000u, 102000000u, 136000000u, 160000000u,         0u,         0u }, /* VOS2 */
    { 22000000u,  44000000u,  66000000u,  88000000u,         0u,         0u,         0u }, /* VOS3 */
#elif defined(STM32H7RS)
    /* RM0477 - STM32H7R3 / H7R7 / H7S3 / H7S7 */
    { 40000000u,  80000000u, 120000000u, 160000000u, 200000000u, 240000000u, 280000000u, 320000000u }, /* VOS high */
    { 36000000u,  72000000u, 108000000u, 144000000u, 180000000u, 216000000u,         0u,         0u }, /* VOS low  */
    {        0u,          0u,         0u,         0u,         0u,         0u,         0u,         0u }, /* -        */
    {        0u,          0u,         0u,         0u,         0u,         0u,         0u,         0u }, /* -        */
#else
#error "Rcc: flash wait states are not defined for the selected device line."
#endif
};

#if defined(STM32H7RS)
/** \brief Flash programming delay (FLASH_ACR WRHIGHFREQ) required by the wait
 *         states 0 - 7 (RM0477 - the delay follows the AXI clock frequency, 2 wait
 *         states per delay step). */
static const uint32_t rcc_FlashWrHighFreqLut[ RCC_FLASH_WS_TABLE_SIZE ] =
{
    ( 0u << FLASH_ACR_WRHIGHFREQ_Pos ), ( 0u << FLASH_ACR_WRHIGHFREQ_Pos ),
    ( 1u << FLASH_ACR_WRHIGHFREQ_Pos ), ( 1u << FLASH_ACR_WRHIGHFREQ_Pos ),
    ( 2u << FLASH_ACR_WRHIGHFREQ_Pos ), ( 2u << FLASH_ACR_WRHIGHFREQ_Pos ),
    ( 3u << FLASH_ACR_WRHIGHFREQ_Pos ), ( 3u << FLASH_ACR_WRHIGHFREQ_Pos ),
};
#endif

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
 *  1. device reset state workarounds (AXI SRAM errata of revision Y, FMC bank 1),
 *  2. maximal flash latency, system clock switched to HSI, all PLLs stopped,
 *  3. supply configuration (write-once) and voltage scaling,
 *  4. HSE (and its clock security system), PLLs configured,
 *  5. system / bus prescalers, system clock source,
 *  6. flash latency for the AXI clock, SystemCoreClock (CPU clock), SystemD2Clock
 *     (AXI clock), SysTick and clock outputs.
 *
 * \warning PLL clocked peripherals lose their kernel clock during the
 *          configuration. The supply configuration must match the board.
 *
 * \param clockConfig [in]: Clock configuration.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Init( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    if( RCC_NULL_PTR != clockConfig )
    {
        Rcc_Set_DeviceWorkarounds();

        /* Flash latency valid for every clock during the reconfiguration */
        retState = Rcc_Set_FlashLatencySwitch();

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_Hsi64Active();
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* A PLL cannot be deactivated by hardware while it drives SYSCLK
             * (e.g. after a previous Rcc_Init() call). Move SYSCLK to HSI before
             * the voltage scaling and the PLLs are reconfigured below. */
            retState = Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_HSI );
        }
        else
        {
            /* Previous step failed */
        }

        /* PLLs share one clock source and can be clocked by HSE - all PLLs are
         * stopped before the oscillators and the PLL source are reconfigured */
        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Inactive( pllId );
            }
            else
            {
                /* Previous step failed */
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PwrSupply( clockConfig->PowerSupply );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PwrRange( clockConfig );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            const uint32_t cssState = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSE_CSS_ON );
            const uint32_t hseReady = Rcc_Get_RegBit( RCC_REG_CR, RCC_CR_HSERDY );

            if( ( 0u != cssState ) &&
                ( 0u != hseReady )    )
            {
                /* HSE supervised by clock security system is kept running - stopped
                 * HSE would be detected as failure (CSS is disabled by reset only) */
            }
            else
            {
                retState = Rcc_ClkSrc_Set_HseActive( clockConfig->HSE_ClockType );
            }
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_HseClk( clockConfig->HSE_Frequency_Hz );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            if( RCC_FUNCTION_ACTIVE == clockConfig->CSS_Enable )
            {
                if( RCC_HSE_TYPE_NONE != clockConfig->HSE_ClockType )
                {
                    LL_RCC_HSE_EnableCSS();
                }
                else
                {
                    /* Clock security system needs HSE */
                    retState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* Clock security system is not requested (can be disabled by reset only) */
            }
        }
        else
        {
            /* Previous step failed */
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Config( pllId, &clockConfig->Pll_Config[ pllId ] );
            }
            else
            {
                /* Previous step failed */
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SYSDivider( clockConfig->SYS_Divider );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_AHBDivider( clockConfig->AHB_Divider );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB1Divider( clockConfig->APB1_Divider );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB2Divider( clockConfig->APB2_Divider );
        }
        else
        {
            /* Previous step failed */
        }

#if defined(STM32H7RS)
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB5Divider( clockConfig->APB5_Divider );
        }
        else
        {
            /* Previous step failed */
        }
#else
        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB3Divider( clockConfig->APB3_Divider );
        }
        else
        {
            /* Previous step failed */
        }
#endif

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB4Divider( clockConfig->APB4_Divider );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Flash latency required by the AXI clock (HCLK) in the actual voltage scale */
            retState = Rcc_Set_FlashLatencyActual();
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_ClkVariables();
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_SysTickInterval( clockConfig->SysTickInterval );
        }
        else
        {
            /* Previous step failed */
        }

        for( rcc_ClkOut_Id_t clkOutId = 0u; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                /* MCO configuration (return states are not needed to be checked) */
                (void)Rcc_Set_ClkOutSource( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockSource );

                (void)Rcc_Set_ClkOutDivider( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockDivider );
            }
            else
            {
                /* Previous step failed */
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
 * This function shall call every necessary sub-module deinitialization function
 * and free all the resources allocated by the module. In case of failure, the
 * function shall handle it by itself and shall not be transferred to AppMain
 * layer.
 *
 * \param clockConfig [in]: Clock configuration.
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
 * Default configuration is valid for every STM32H7 board: system clock from HSI
 * (64 MHz), all PLLs stopped, all prescalers 1, voltage scale 3 (VOS low -
 * scale 1 on STM32H7R / H7S) and supply configuration of the reset (not
 * written).
 *
 * \param clockConfig [out]: Pointer to clock configuration structure.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_DefaultConfig( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clockConfig )
    {
        clockConfig->PowerSupply       = RCC_PWR_SUPPLY_DEFAULT;
        clockConfig->HSE_ClockType     = RCC_HSE_TYPE_NONE;
        clockConfig->HSE_Frequency_Hz  = RCC_DEFAULT_HSE_FREQ_HZ;
        clockConfig->SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;
        clockConfig->CSS_Enable        = RCC_FUNCTION_INACTIVE;
        clockConfig->SYS_Divider       = RCC_SYS_DIVIDER_1;
        clockConfig->AHB_Divider       = RCC_AHB_DIVIDER_1;
        clockConfig->APB1_Divider      = RCC_APB1_DIVIDER_1;
        clockConfig->APB2_Divider      = RCC_APB2_DIVIDER_1;
#if defined(STM32H7RS)
        clockConfig->APB5_Divider      = RCC_APB5_DIVIDER_1;
#else
        clockConfig->APB3_Divider      = RCC_APB3_DIVIDER_1;
#endif
        clockConfig->APB4_Divider      = RCC_APB4_DIVIDER_1;
        clockConfig->FlashLatency      = RCC_FLASH_LATENCY_7_WS;
        clockConfig->VoltageScaling    = RCC_DEFAULT_VOLTAGE_SCALE;
        clockConfig->SysTickInterval   = RCC_DEFAULT_SYSTICK_INTERVAL_MS;

        for( rcc_ClkOut_Id_t clkOutId = 0u; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
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
#if defined(STM32H7RS)
            clockConfig->Pll_Config[ pllId ].S_Divider    = RCC_DEFAULT_PLL_OUT_ST_DIV;
            clockConfig->Pll_Config[ pllId ].T_Divider    = RCC_DEFAULT_PLL_OUT_ST_DIV;
#endif
        }

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
 * (HSI64, HSI48, CSI, LSI) which is not running, the oscillator is started
 * before the clock MUX is switched. External sources (HSE, LSE), PLL outputs and
 * the peripheral clock (CKPER) are not started.
 *
 * \warning Some peripherals have common clock multiplexer (e.g. SPI1 / SPI2 /
 *          SPI3, USART2 / USART3 / UART4 / UART5 / UART7 / UART8).
 *
 * \param periphId (in): ID of required peripheral to activate clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when the oscillator can not be started
 *         or the clock MUX is set to other source, peripheral clock is not
 *         changed in that case).
 */
rcc_RequestState_t Rcc_Set_PeriphActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

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
        rcc_ClkMuxId_t clkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;

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
        retState = RCC_REQUEST_ERROR;

        rcc_BlockList_t blockId  = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId = rcc_PeriphBlockConfig[ blockId ].ClkBusId;

        if( RCC_CLK_BUS_CNT > clkBusId )
        {
            rcc_RegId_t stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;
            uint32_t    stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;

            if( RCC_UNSUPPORTED_FUNCTION != stateMask )
            {
                /* Activate peripheral by setting "1" to corresponding register */
                Rcc_Set_RegBit( stateRegId, stateMask );

                /* Activate peripheral clock */
                for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
                {
                    regValue = Rcc_Get_RegBit( stateRegId, stateMask );

                    if( 0u != ( regValue & stateMask ) )
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
            /* No action required */
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
 * with another kernel clock later. RTC / peripheral clock (CKPER) selection is kept
 * (RTCSEL is write-once until backup domain reset, CKPER is shared) and a multiplexer
 * shared with another enabled peripheral block (e.g. SPI123SEL of SPI1 / SPI2 / SPI3)
 * is kept.
 *
 * \param periphId (in): ID of required peripheral to de-activate clock source
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PeriphInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            Rcc_Reset_RegBit( stateRegId, stateMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = Rcc_Get_RegBit( stateRegId, stateMask );

                if( 0u == regValue )
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
        const rcc_ClkMuxId_t clkMuxId  = rcc_ConfigStruct[ periphId ].ClkMuxId;
        rcc_FunctionState_t  muxShared = RCC_FUNCTION_ACTIVE;

        if( ( RCC_REQUEST_OK        == retState ) &&
            ( RCC_CLK_MUX_LIST_CNT   > clkMuxId ) &&
            ( RCC_BLOCK_RTCAPB      != blockId  ) &&
            ( RCC_BLOCK_CKPER       != blockId  )    )
        {
            muxShared = Rcc_Get_ClkMuxShared( periphId );
        }
        else
        {
            /* Clock disable failed, no multiplexer or RTC / CKPER selection - multiplexer is kept */
        }

        if( RCC_FUNCTION_INACTIVE == muxShared )
        {
            retState = Rcc_ClkMux_Set_ClkInactive( clkMuxId );
        }
        else
        {
            /* Multiplexer kept or shared with another enabled peripheral block */
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
 * Peripherals clocks can activated or deactivated. This function can be used
 * by user to read this activation status.
 *
 * \param periphId   (in): ID of required peripheral
 * \param funcState (out): State of peripheral activation
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            regValue = Rcc_Get_RegBit( stateRegId, stateMask );

            if( 0u == regValue )
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
            *funcState = RCC_FUNCTION_ACTIVE;

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
 * \brief Returns clock frequency for selected peripheral.
 *
 * \param periphId   [in]: ID of required peripheral
 * \param periphClk [out]: Value of frequency for selected peripheral in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for external clock input - frequency
 *         is unknown).
 */
rcc_RequestState_t Rcc_Get_PeriphClk( rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != periphClk )    )
    {
        const rcc_ClkSrcId_t periphClkSrcId = rcc_ConfigStruct[ periphId ].ClkSrcId;

        if( ( RCC_CLK_SRC_CNT > periphClkSrcId                                          ) &&
            ( RCC_NULL_PTR   != rcc_PeriphClkSrcConfig[ periphClkSrcId ].ClkSrcCallback )    )
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
 * \brief Returns clock source for selected peripheral.
 *
 * For peripherals that can use different clock sources through clock
 * multiplexer, any of enumeration in its range can be used. For example for
 * USART1 can be used any of \c RCC_PERIPH_USART1_PCLK2, \c RCC_PERIPH_USART1_PLL2Q,
 * \c RCC_PERIPH_USART1_HSI, \c RCC_PERIPH_USART1_LSE or \c RCC_PERIPH_USART1_CSI
 * can be used and correct enumeration will be returned.
 *
 * \param periphId      [in]: ID of required peripheral
 * \param periphClkSrc [out]: Pointer to store peripheral ID matching the actually selected clock source. Must not be NULL.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphClkSrc( rcc_PeriphId_t periphId, rcc_PeriphId_t * const periphClkSrc )
{
    rcc_RequestState_t returnState     = RCC_REQUEST_ERROR;
    rcc_ClkMuxId_t     clkMuxId        = RCC_CLK_MUX_LIST_CNT;
    rcc_PeriphId_t     foundPeriphId   = RCC_PERIPH_ID_CNT;

    if( ( RCC_PERIPH_ID_CNT > periphId     ) &&
        ( RCC_NULL_PTR     != periphClkSrc )    )
    {
        const rcc_ClkMuxId_t  periphClkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;
        const rcc_BlockList_t blockId        = rcc_ConfigStruct[ periphId ].BlockId;

        if ( RCC_CLK_MUX_LIST_CNT > periphClkMuxId )
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
                        /* Entry of other block or other multiplexer input */
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
                /* Previous step failed */
            }
        }
        else
        {
            *periphClkSrc = periphId;

            returnState = RCC_REQUEST_OK;
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
 *         otherwise return error (also for peripheral without reset control,
 *         no register is changed in that case).
 */
rcc_RequestState_t Rcc_Set_ResetActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( ( RCC_PERIPH_ID_CNT        >  periphId                                                                   ) &&
        ( RCC_UNSUPPORTED_FUNCTION != rcc_PeriphBlockConfig[ rcc_ConfigStruct[ periphId ].BlockId ].RstCtrlMask )    )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        Rcc_Set_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( stateRegId, resetMask );

            if( 0u != regValue )
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
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Peripheral reset disable request
 *
 * User can request deactivation of reset for required peripheral. If required
 * peripheral is correctly switched from reset state, and required peripheral ID
 * is correct, returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to deactivate reset
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error. Peripheral without reset control is never
 *         in reset - "OK" is returned without register access.
 */
rcc_RequestState_t Rcc_Set_ResetInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( ( RCC_PERIPH_ID_CNT        >  periphId                                                                   ) &&
        ( RCC_UNSUPPORTED_FUNCTION == rcc_PeriphBlockConfig[ rcc_ConfigStruct[ periphId ].BlockId ].RstCtrlMask )    )
    {
        /* Peripheral without reset control */
        retState = RCC_REQUEST_OK;
    }
    else if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        Rcc_Reset_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( stateRegId, resetMask );

            if( 0u == regValue )
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
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading state of peripheral reset state
 *
 * Peripherals can be reset. This function can be used by user to read this
 * reset status.
 *
 * \param periphId   (in): ID of required peripheral
 * \param funcState (out): State of peripheral reset
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ResetState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            regValue = Rcc_Get_RegBit( stateRegId, stateMask );
        }
        else
        {
            /* Peripheral without reset control is never in reset */
            regValue = 0u;
        }

        if( 0u == regValue )
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        if( RCC_UNSUPPORTED_FUNCTION != sleepMask )
        {
            Rcc_Set_RegBit( stateRegId, sleepMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = Rcc_Get_RegBit( stateRegId, sleepMask );

                if( 0u != regValue )
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
    rcc_RequestState_t retState      = RCC_REQUEST_ERROR;
    volatile uint32_t  registerValue = 0u;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        if( RCC_UNSUPPORTED_FUNCTION != sleepMask )
        {
            Rcc_Reset_RegBit( stateRegId, sleepMask );

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                registerValue = Rcc_Get_RegBit( stateRegId, sleepMask );

                if( 0u == registerValue )
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
 * \brief Reading state of activation peripheral in sleep mode
 *
 * Peripherals can be configured to be active in sleep mode. This function can
 * be used by user to read this activation status.
 *
 * \param periphId   (in): ID of required peripheral
 * \param funcState (out): State of peripheral activation in sleep mode
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SleepState( rcc_PeriphId_t periphId, rcc_FunctionState_t * const funcState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;
        uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            regValue = Rcc_Get_RegBit( stateRegId, stateMask );

            if( 0u == regValue )
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
            *funcState = RCC_FUNCTION_ACTIVE;

            retState = RCC_REQUEST_OK;
        }
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
 * User can set required clock bus divider. If required clock bus ID and
 * divider are correct, returned state is "OK". Otherwise returns error.
 * AHB1 - AHB4 share the AHB prescaler (HPRE), APB1 groups share the APB1
 * prescaler. Change of the AHB prescaler updates the flash latency and
 * SystemD2Clock.
 *
 * \param clkBusId      [in]: ID of required clock bus
 * \param clkBusDivider [in]: Required clock bus divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ClkBusDivider( rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t clkBusDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( clkBusId )
    {
        case RCC_CLK_BUS_AHB1:
        case RCC_CLK_BUS_AHB2:
        case RCC_CLK_BUS_AHB3:
#if defined(STM32H7RS)
        case RCC_CLK_BUS_AHB5:
#endif
        case RCC_CLK_BUS_AHB4:   retState = Rcc_Set_AHBDividerSafe( (rcc_AHB_Div_t)clkBusDivider );          break;
        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2: retState = Rcc_ClkBus_Set_APB1Divider( (rcc_APB1_Div_t)clkBusDivider );     break;
        case RCC_CLK_BUS_APB2:   retState = Rcc_ClkBus_Set_APB2Divider( (rcc_APB2_Div_t)clkBusDivider );     break;
#if defined(STM32H7RS)
        case RCC_CLK_BUS_APB5:   retState = Rcc_ClkBus_Set_APB5Divider( (rcc_APB5_Div_t)clkBusDivider );     break;
#else
        case RCC_CLK_BUS_APB3:   retState = Rcc_ClkBus_Set_APB3Divider( (rcc_APB3_Div_t)clkBusDivider );     break;
#endif
        case RCC_CLK_BUS_APB4:   retState = Rcc_ClkBus_Set_APB4Divider( (rcc_APB4_Div_t)clkBusDivider );     break;
        default:                 retState = RCC_REQUEST_ERROR;                                               break;
    }

    return ( retState );
}


/**
 * \brief Function used for getting clock bus divider
 *
 * User can get current clock bus divider. If required clock bus ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param clkBusId      [in]: ID of required clock bus
 * \param clkBusDivider [out]: Current clock bus divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkBusDivider( rcc_ClkBusId_t clkBusId, rcc_ClkBusDiv_t * const clkBusDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( clkBusId )
    {
        case RCC_CLK_BUS_AHB1:
        case RCC_CLK_BUS_AHB2:
        case RCC_CLK_BUS_AHB3:
#if defined(STM32H7RS)
        case RCC_CLK_BUS_AHB5:
#endif
        case RCC_CLK_BUS_AHB4:   retState = Rcc_ClkBus_Get_AHBDivider( (rcc_AHB_Div_t*) clkBusDivider );     break;
        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2: retState = Rcc_ClkBus_Get_APB1Divider( (rcc_APB1_Div_t*) clkBusDivider );   break;
        case RCC_CLK_BUS_APB2:   retState = Rcc_ClkBus_Get_APB2Divider( (rcc_APB2_Div_t*) clkBusDivider );   break;
#if defined(STM32H7RS)
        case RCC_CLK_BUS_APB5:   retState = Rcc_ClkBus_Get_APB5Divider( (rcc_APB5_Div_t*) clkBusDivider );   break;
#else
        case RCC_CLK_BUS_APB3:   retState = Rcc_ClkBus_Get_APB3Divider( (rcc_APB3_Div_t*) clkBusDivider );   break;
#endif
        case RCC_CLK_BUS_APB4:   retState = Rcc_ClkBus_Get_APB4Divider( (rcc_APB4_Div_t*) clkBusDivider );   break;
        default:                 retState = RCC_REQUEST_ERROR;                                               break;
    }

    return ( retState );
}


/**
 * \brief Function used for getting clock bus frequency
 *
 * User can get current clock bus frequency. If required clock bus ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param clkBusId   [in]: ID of required clock bus
 * \param clkBusFreq [out]: Current clock bus frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkBusClk( rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( clkBusId )
    {
        case RCC_CLK_BUS_AHB1:
        case RCC_CLK_BUS_AHB2:
        case RCC_CLK_BUS_AHB3:
#if defined(STM32H7RS)
        case RCC_CLK_BUS_AHB5:
#endif
        case RCC_CLK_BUS_AHB4:   retState = Rcc_ClkBus_Get_AHBClk( clkBusFreq );    break;
        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2: retState = Rcc_ClkBus_Get_APB1Clk( clkBusFreq );   break;
        case RCC_CLK_BUS_APB2:   retState = Rcc_ClkBus_Get_APB2Clk( clkBusFreq );   break;
#if defined(STM32H7RS)
        case RCC_CLK_BUS_APB5:   retState = Rcc_ClkBus_Get_APB5Clk( clkBusFreq );   break;
#else
        case RCC_CLK_BUS_APB3:   retState = Rcc_ClkBus_Get_APB3Clk( clkBusFreq );   break;
#endif
        case RCC_CLK_BUS_APB4:   retState = Rcc_ClkBus_Get_APB4Clk( clkBusFreq );   break;
        default:                 retState = RCC_REQUEST_ERROR;                      break;
    }

    return ( retState );
}

/*------------------ Power range and latency configuration -------------------*/

/**
 * \brief Function used for voltage scaling (VOS) configuration
 *
 * Voltage scale is written only if it differs from the active voltage scale,
 * then the function waits until the regulator reaches it (ACTVOSRDY). VOS0 of
 * STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 is reached through
 * VOS1 by the SYSCFG overdrive (ODEN) and needs the LDO supply.
 *
 * \warning Voltage scale must allow the actual clock frequencies (lower voltage
 *          scale has to be set after the clock frequency is decreased). Voltage
 *          scale other than VOS3 needs the supply configuration written after
 *          power-on reset (\ref rcc_ConfigStruct_t PowerSupply).
 * \note    STM32H7R / H7S: voltage scale 0 (VOS high) and 1 (VOS low) only, the
 *          other scales return error.
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PwrRange( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t    retState     = RCC_REQUEST_ERROR;
    rcc_PwrVoltageScale_t activeScale  = RCC_PWR_VOLTAGE_SCALE_CNT;

    if( ( RCC_NULL_PTR              != clockConfig                 ) &&
        ( RCC_PWR_VOLTAGE_SCALE_CNT  > clockConfig->VoltageScaling )    )
    {
        const rcc_PwrVoltageScale_t requestedScale = clockConfig->VoltageScaling;

        retState = Rcc_Get_PwrVoltageScale( &activeScale );

        if( RCC_PWR_VOS_UNSUPPORTED == rcc_PwrVoltageScaleLut[ requestedScale ] )
        {
            /* Voltage scale is not available on the device */
            retState = RCC_REQUEST_ERROR;
        }
        else if( ( RCC_REQUEST_OK == retState    ) &&
                 ( requestedScale == activeScale )    )
        {
            /* Requested voltage scale is already active - nothing is written */
        }
        else
        {
#if defined(SYSCFG_PWRCR_ODEN)
            if( ( RCC_PWR_VOLTAGE_SCALE_0 == requestedScale                         ) &&
                ( 0u                      == READ_BIT( PWR->CR3, PWR_CR3_LDOEN )    )    )
            {
                /* Voltage scale 0 is possible with LDO regulator only */
                retState = RCC_REQUEST_ERROR;
            }
            else
            {
                /* SYSCFG clock is needed for the overdrive control */
                retState = Rcc_Set_PeriphActive( RCC_PERIPH_SYSCFG );
            }

            if( ( RCC_REQUEST_OK == retState                                  ) &&
                ( 0u             != READ_BIT( SYSCFG->PWRCR, SYSCFG_PWRCR_ODEN ) )    )
            {
                /* Overdrive is disabled before VOS1 changes to other scale */
                CLEAR_BIT( SYSCFG->PWRCR, SYSCFG_PWRCR_ODEN );

                retState = Rcc_Get_PwrVoltageReady( RCC_PWR_ACTVOS_ANY, RCC_PWR_ACTVOS_ANY );
            }
            else
            {
                /* Overdrive is not active or error occurred */
            }
#else
            retState = RCC_REQUEST_OK;
#endif

            if( RCC_REQUEST_OK == retState )
            {
                LL_PWR_SetRegulVoltageScaling( rcc_PwrVoltageScaleLut[ requestedScale ] );

                /* Ready flag of the previous scale can be still set - applied scale is checked */
                retState = Rcc_Get_PwrVoltageReady( RCC_PWR_ACTVOS_MASK, rcc_PwrVoltageScaleLut[ requestedScale ] );
            }
            else
            {
                /* Previous step failed */
            }

#if defined(SYSCFG_PWRCR_ODEN)
            if( ( RCC_REQUEST_OK          == retState       ) &&
                ( RCC_PWR_VOLTAGE_SCALE_0 == requestedScale )    )
            {
                SET_BIT( SYSCFG->PWRCR, SYSCFG_PWRCR_ODEN );

                retState = Rcc_Get_PwrVoltageReady( RCC_PWR_ACTVOS_ANY, RCC_PWR_ACTVOS_ANY );
            }
            else
            {
                /* Overdrive is not requested or error occurred */
            }
#endif
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
 * Number of wait states is calculated for the AXI clock (HCLK) expected from
 * the configuration structure and for the active voltage scale. The flash
 * programming delay (WRHIGHFREQ) keeps its reset value (maximal delay), on
 * STM32H7R / H7S it is set together with the wait states.
 *
 * \warning Number of wait states must not be decreased before the clock
 *          frequency is decreased (\ref Rcc_Init handles the order).
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for frequency out of the voltage scale range).
 */
rcc_RequestState_t Rcc_Set_FlashLatency( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       hclkFreq = 0u;
    uint32_t           latency  = RCC_FLASH_LATENCY_SWITCH;

    retState = Rcc_Get_ExpectedHclkFrequency( clockConfig, &hclkFreq );

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Get_FlashLatencyRequired( hclkFreq, &latency );
    }
    else
    {
        /* Previous step failed */
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Set_FlashLatencyValue( latency );
    }
    else
    {
        /* Previous step failed */
    }

    return ( retState );
}


/**
 * \brief Function used to flash prefetch buffer activation.
 *
 * \note  STM32H7 flash interface has no prefetch buffer - request returns error.
 *
 * \return Returns always \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchActive( void )
{
    return ( RCC_REQUEST_ERROR );
}


/**
 * \brief Function used to flash prefetch buffer de-activation.
 *
 * \note  STM32H7 flash interface has no prefetch buffer - request returns error.
 *
 * \return Returns always \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchInactive( void )
{
    return ( RCC_REQUEST_ERROR );
}


/**
 * \brief Configures the interval between SysTick's in ms [0.001s]
 *
 * SysTick is configured by CMSIS SysTick_Config, which selects the processor
 * clock (CPU clock) as SysTick clock source. Reload value is calculated from
 * actual CPU clock frequency and must fit the 24-bit reload register.
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
        rcc_FreqHz_t             cpuFreq  = 0u;
        const rcc_RequestState_t clkState = Rcc_ClkBus_Get_CpuClk( &cpuFreq );
        const uint64_t           ticksCnt = ( (uint64_t)cpuFreq * sysTickInterval ) / RCC_MS_IN_SECOND;

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
        rcc_FreqHz_t             cpuFreq  = 0u;
        const rcc_RequestState_t clkState = Rcc_ClkBus_Get_CpuClk( &cpuFreq );

        if( ( RCC_REQUEST_OK == clkState ) &&
            ( 0u              < cpuFreq  )    )
        {
            /* SysTick is clocked by processor clock (CPU clock) */
            const uint64_t ticksCnt = (uint64_t)SysTick->LOAD + RCC_SYSTICK_RELOAD_OFFSET;

            *sysTickInterval = (rcc_Time_ms_t)( ( ticksCnt * RCC_MS_IN_SECOND ) / cpuFreq );

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
 * \param pllId        [in]: Phase Locked Loop identification
 * \param configStruct [in]: Phase Locked Loop configuration
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
 * \param pllId   [in]: Phase Locked Loop identification
 * \param pllClk [out]: Internal clock frequency in Hz
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
 * \param pllId [in]: Phase Locked Loop identification
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
 * \param pllId [in]: Phase Locked Loop identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllInactive( rcc_PllId_t pllId )
{
    return ( Rcc_Pll_Set_Inactive( pllId ) );
}


/**
 * \brief Reads Phase Locked Loop activation state.
 *
 * \param pllId     [in]: Phase Locked Loop identification
 * \param retState [out]: Activation state
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllState( rcc_PllId_t pllId, rcc_FunctionState_t * const retState )
{
    return ( Rcc_Pll_Get_State( pllId, retState ) );
}


/**
 * \brief Selects Phase Locked Loop clock source.
 *
 * \note  The clock source is common for all PLLs - it can be changed only while
 *        all PLLs are stopped.
 *
 * \param pllId     [in]: Phase Locked Loop identification
 * \param clkSource [in]: Clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PllsSource( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    return ( Rcc_Pll_Set_Source( pllId, clkSource ) );
}


/**
 * \brief Reads Phase Locked Loop clock source.
 *
 * \param pllId      [in]: Phase Locked Loop identification
 * \param clkSource [out]: Clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllsSource( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource )
{
    return ( Rcc_Pll_Get_Source( pllId, clkSource ) );
}


/**
 * \brief Reads Phase Locked Loop P output clock frequency.
 *
 * \param pllId   [in]: Phase Locked Loop identification
 * \param pllClk [out]: P output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    return ( Rcc_Pll_Get_Clk_OutP( pllId, pllClk ) );
}


/**
 * \brief Reads Phase Locked Loop Q output clock frequency.
 *
 * \param pllId   [in]: Phase Locked Loop identification
 * \param pllClk [out]: Q output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    return ( Rcc_Pll_Get_Clk_OutQ( pllId, pllClk ) );
}


/**
 * \brief Reads Phase Locked Loop R output clock frequency.
 *
 * \param pllId   [in]: Phase Locked Loop identification
 * \param pllClk [out]: R output frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllClk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t *pllClk )
{
    return ( Rcc_Pll_Get_Clk_OutR( pllId, pllClk ) );
}

/*----------------------- Internal oscillators configuration -----------------*/

/**
 * \brief Activates internal oscillator.
 *
 * \param oscId [in]: Internal oscillator identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscActive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( oscId )
    {
        case RCC_OSC_HSI64: retState = Rcc_ClkSrc_Set_Hsi64Active(); break;
        case RCC_OSC_HSI48: retState = Rcc_ClkSrc_Set_Hsi48Active(); break;
        case RCC_OSC_CSI:   retState = Rcc_ClkSrc_Set_CsiActive();   break;
        case RCC_OSC_LSI:   retState = Rcc_ClkSrc_Set_LsiActive();   break;
        default:            retState = RCC_REQUEST_ERROR;            break;
    }

    return ( retState );
}


/**
 * \brief Deactivates internal oscillator.
 *
 * \warning Oscillator used as system clock or PLL source must not be deactivated.
 *
 * \param oscId [in]: Internal oscillator identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscInactive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( oscId )
    {
        case RCC_OSC_HSI64: retState = Rcc_ClkSrc_Set_Hsi64Inactive(); break;
        case RCC_OSC_HSI48: retState = Rcc_ClkSrc_Set_Hsi48Inactive(); break;
        case RCC_OSC_CSI:   retState = Rcc_ClkSrc_Set_CsiInactive();   break;
        case RCC_OSC_LSI:   retState = Rcc_ClkSrc_Set_LsiInactive();   break;
        default:            retState = RCC_REQUEST_ERROR;              break;
    }

    return ( retState );
}


/**
 * \brief Reads activation state of internal oscillator.
 *
 * \param oscId     [in]: Internal oscillator identification
 * \param retState [out]: Activation state (oscillator ready)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_OscState( rcc_OscId_t oscId, rcc_FunctionState_t * const retState )
{
    rcc_RequestState_t reqState = RCC_REQUEST_ERROR;

    switch( oscId )
    {
        case RCC_OSC_HSI64: reqState = Rcc_ClkSrc_Get_Hsi64State( retState ); break;
        case RCC_OSC_HSI48: reqState = Rcc_ClkSrc_Get_Hsi48State( retState ); break;
        case RCC_OSC_CSI:   reqState = Rcc_ClkSrc_Get_CsiState( retState );   break;
        case RCC_OSC_LSI:   reqState = Rcc_ClkSrc_Get_LsiState( retState );   break;
        default:            reqState = RCC_REQUEST_ERROR;                     break;
    }

    return ( reqState );
}


/**
 * \brief Configures divider of internal oscillator output.
 *
 * Only HSI64 has an output divider (1, 2, 4 or 8). Other oscillators accept divider 1 only.
 *
 * \warning HSI64 divider changes the frequency of all clocks derived from HSI (system clock,
 *          PLL input, peripheral kernel clocks). Flash latency, SystemCoreClock and
 *          SystemD2Clock are updated, peripherals (SysTick, timers, communication) have
 *          to be reconfigured.
 *
 * \param oscId  [in]: Internal oscillator identification
 * \param oscDiv [in]: Divider value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscDiv( rcc_OscId_t oscId, rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_OSC_HSI64 == oscId )
    {
        uint32_t llDivider = RCC_HSI_DIV_1;

        retState = RCC_REQUEST_OK;

        switch( oscDiv )
        {
            case 1u: llDivider = RCC_HSI_DIV_1;            break;
            case 2u: llDivider = RCC_HSI_DIV_2;            break;
            case 4u: llDivider = RCC_HSI_DIV_4;            break;
            case 8u: llDivider = RCC_HSI_DIV_8;            break;
            default: retState  = RCC_REQUEST_ERROR;        break;
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Frequency can increase - latency valid for every clock during the change */
            retState = Rcc_Set_FlashLatencySwitch();
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            LL_RCC_HSI_SetDivider( llDivider );

            retState = RCC_REQUEST_ERROR;

            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                const uint32_t actualDivider = LL_RCC_HSI_GetDivider();
                const uint32_t dividerReady  = LL_RCC_HSI_IsDividerReady();

                if( ( llDivider == actualDivider ) &&
                    ( 0u        != dividerReady  )    )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Divider not applied yet */
                }
            }
        }
        else
        {
            /* Unsupported divider value or flash latency not set */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashLatencyActual();
        }
        else
        {
            /* Previous step failed */
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_ClkVariables();
        }
        else
        {
            /* Previous step failed */
        }
    }
    else if( ( RCC_OSC_CNT > oscId  ) &&
             ( 1u          == oscDiv )    )
    {
        /* Oscillator without divider */
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads divider of internal oscillator output.
 *
 * \param oscId   [in]: Internal oscillator identification
 * \param oscDiv [out]: Divider value (1 for oscillators without divider)
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
        if( RCC_OSC_HSI64 == oscId )
        {
            *oscDiv = (rcc_OscDiv_t)( 1u << ( LL_RCC_HSI_GetDivider() >> RCC_CR_HSIDIV_Pos ) );
        }
        else
        {
            *oscDiv = 1u;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*-------------------------- RTC clock configuration -------------------------*/

/**
 * \brief Selects Real Time Clock (RTC) clock source.
 *
 * \param clkSource [in]: RTC clock source
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
 * \param clkSource [out]: RTC clock source
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
 * \param outId     [in]: Clock output identification
 * \param clkSource [in]: Clock output signal source
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
 * \param outId      [in]: Clock output identification
 * \param clkSource [out]: Pointer to MCO clock source
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
 * \param outId      [in]: Clock output identification
 * \param clkDivider [in]: Clock output divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ClkOutDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider )
{
    return Rcc_ClkOut_Set_ClockDivider( outId, clkDivider );
}


/**
 * \brief Function used to get MCO clock divider.
 *
 * \param outId      [in] : Clock output identification
 * \param clkDivider [out]: Pointer to MCO clock divider
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
 *        Dual-core devices report the reset sources of the Cortex-M7 core.
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
    uint32_t           regValue = RCC_RSR_BITS_CLEARED;

    if( ( RCC_RESET_SRC_CNT > resetSrc  ) &&
        ( RCC_NULL_PTR     != flagState )    )
    {
        regValue = Rcc_Get_RegBit( RCC_REG_RSR, rcc_ResetSrcLut[ resetSrc ] );

        if( RCC_RSR_BITS_CLEARED != regValue )
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
    uint32_t           regValue = RCC_RSR_BITS_CLEARED;

    /* Request removal of all reset source flags */
    Rcc_Set_RegVal( RCC_REG_RSR, RCC_RSR_RMVF, RCC_RSR_RMVF );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        regValue = Rcc_Get_RegBit( RCC_REG_RSR, RCC_RSR_RESET_SRC_MASK );

        if( RCC_RSR_BITS_CLEARED == regValue )
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
    Rcc_Set_RegVal( RCC_REG_RSR, RCC_RSR_RMVF, RCC_RSR_BITS_CLEARED );

    if( RCC_REQUEST_OK == retState )
    {
        retState = RCC_REQUEST_ERROR;

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( RCC_REG_RSR, RCC_RSR_RMVF );

            if( RCC_RSR_BITS_CLEARED == regValue )
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
 * Clock sources HSI64, HSI48, CSI (also CSI / 122) and LSI are mapped to
 * internal oscillators, oscillator which is not running is activated. Other
 * clock sources (buses, PLL outputs, HSE, LSE, CKPER, external input) are not
 * handled.
 *
 * \param clkSrcId [in]: Kernel clock source of the peripheral
 *
 * \return Returns "OK" if the clock source is not an internal oscillator or the
 *         oscillator is running. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_ClkSrcOscActive( rcc_ClkSrcId_t clkSrcId )
{
    rcc_RequestState_t  retState = RCC_REQUEST_OK;
    rcc_OscId_t         oscId    = RCC_OSC_CNT;
    rcc_FunctionState_t oscState = RCC_FUNCTION_INACTIVE;

    switch( clkSrcId )
    {
        case RCC_CLK_SRC_HSI64CLK:     oscId = RCC_OSC_HSI64; break;
        case RCC_CLK_SRC_HSI48CLK:     oscId = RCC_OSC_HSI48; break;
        case RCC_CLK_SRC_CSI4CLK:      oscId = RCC_OSC_CSI;   break;
        case RCC_CLK_SRC_CSIDIV122CLK: oscId = RCC_OSC_CSI;   break;
        case RCC_CLK_SRC_LSICLK:       oscId = RCC_OSC_LSI;   break;
        default:                       oscId = RCC_OSC_CNT;   break;
    }

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
 * \brief Checks if the kernel clock multiplexer of the peripheral is shared with another
 *        enabled peripheral block (e.g. SPI123SEL of SPI1 / SPI2 / SPI3).
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
    const rcc_RequestState_t ownMuxState = Rcc_ClkMux_Get_ClkSrc( rcc_ConfigStruct[ periphId ].ClkMuxId, &ownSelected );

    if( RCC_REQUEST_OK == ownMuxState )
    {
        sharedState = RCC_FUNCTION_INACTIVE;

        for( uint32_t rowIdx = 0u; RCC_PERIPH_ID_CNT > rowIdx; rowIdx ++ )
        {
            const rcc_PeriphConfigStruct_t * const row              = &rcc_ConfigStruct[ rowIdx ];
            rcc_ClkMuxId_t                         otherSelected    = RCC_CLK_MUX_LIST_CNT;
            rcc_FunctionState_t                    otherState       = RCC_FUNCTION_INACTIVE;
            rcc_RequestState_t                     otherMuxState    = RCC_REQUEST_ERROR;
            rcc_RequestState_t                     otherPeriphState = RCC_REQUEST_ERROR;

            if( ( ownBlockId           != row->BlockId  ) &&
                ( RCC_CLK_MUX_LIST_CNT  > row->ClkMuxId )    )
            {
                otherMuxState = Rcc_ClkMux_Get_ClkSrc( row->ClkMuxId, &otherSelected );
            }
            else
            {
                /* Record of the own block or without multiplexer */
            }

            if( ( RCC_REQUEST_OK == otherMuxState ) &&
                ( ownSelected    == otherSelected )    )
            {
                otherPeriphState = Rcc_Get_PeriphState( (rcc_PeriphId_t)rowIdx, &otherState );
            }
            else
            {
                /* Other multiplexer field or selection not readable */
            }

            if( ( RCC_REQUEST_OK      == otherPeriphState ) &&
                ( RCC_FUNCTION_ACTIVE == otherState       )    )
            {
                sharedState = RCC_FUNCTION_ACTIVE;
                break;
            }
            else
            {
                /* Record of the own block, other field or disabled block */
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
 * \brief Applies workarounds of the device reset state (done by the CMSIS
 *        SystemInit of ST, which is not used).
 *
 * - STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 revision Y: read
 *   issuing capability of the AXI SRAM target is reduced to 1 (device errata
 *   "Reading from AXI SRAM may lead to data read corruption").
 * - FMC bank 1 is enabled after reset, CPU speculative accesses to it block the
 *   FMC. The bank is disabled if the FMC clock is not enabled (FMC not used yet).
 *   Not done on STM32H7R / H7S (FMC on AHB5, the workaround is not part of the
 *   ST system initialization of these lines).
 */
static void Rcc_Set_DeviceWorkarounds( void )
{
#if (STM32H7_DEV_ID == 0x450UL)
    if( RCC_DEV_REV_ID_V > ( DBGMCU->IDCODE & DBGMCU_IDCODE_REV_ID ) )
    {
        *( (volatile uint32_t *) RCC_AXI_TARG7_FN_MOD_ADDR ) = RCC_AXI_TARG7_READ_ISS_1;
    }
    else
    {
        /* Revision without AXI SRAM errata */
    }
#endif

#if defined(FMC_Bank1_R) && \
    defined(RCC_AHB3ENR_FMCEN)
    const uint32_t fmcClkState = Rcc_Get_RegBit( RCC_REG_AHB3ENR, RCC_AHB3ENR_FMCEN );

    if( 0u == fmcClkState )
    {
        Rcc_Set_RegBit( RCC_REG_AHB3ENR, RCC_AHB3ENR_FMCEN );

        FMC_Bank1_R->BTCR[ 0u ] = RCC_FMC_BCR1_DISABLED;

        Rcc_Reset_RegBit( RCC_REG_AHB3ENR, RCC_AHB3ENR_FMCEN );
    }
    else
    {
        /* FMC is used by application - configuration is kept */
    }
#endif
}


/**
 * \brief Writes the supply configuration (PWR_CR3, PWR_CSR2 on STM32H7R / H7S).
 *
 * The configuration can be written once after power-on reset. Locked
 * configuration is accepted only if it matches the requested one.
 *
 * \param pwrSupply [in]: Supply configuration
 *
 * \return Returns "OK" if the configuration is not requested (\ref RCC_PWR_SUPPLY_DEFAULT)
 *         or it is active. Otherwise returns error (also for SMPS configuration on
 *         devices without SMPS).
 */
static rcc_RequestState_t Rcc_Set_PwrSupply( rcc_PwrSupply_t pwrSupply )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PWR_SUPPLY_DEFAULT == pwrSupply )
    {
        /* Supply configuration of the reset is kept */
        retState = RCC_REQUEST_OK;
    }
    else if( ( RCC_PWR_SUPPLY_CNT          > pwrSupply                       ) &&
             ( RCC_PWR_SUPPLY_UNSUPPORTED != rcc_PwrSupplyLut[ pwrSupply ] )    )
    {
        const uint32_t supplyValue = rcc_PwrSupplyLut[ pwrSupply ];
#if defined(PWR_CR3_SCUEN)
        const uint32_t supplyLocked = ( 0u == READ_BIT( PWR->CR3, PWR_CR3_SCUEN ) ) ? 1u : 0u;
#else
        const uint32_t supplyLocked = ( RCC_PWR_SUPPLY_RESET_STATE != READ_BIT( RCC_PWR_SUPPLY_REG, RCC_PWR_SUPPLY_SOURCE_MASK ) ) ? 1u : 0u;
#endif

        if( 0u != supplyLocked )
        {
            /* Supply configuration can not be changed - it must match */
            const uint32_t actualSupply = LL_PWR_GetSupply();

            if( supplyValue == actualSupply )
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
            LL_PWR_ConfigSupply( supplyValue );

            retState = Rcc_Get_PwrVoltageReady( RCC_PWR_ACTVOS_ANY, RCC_PWR_ACTVOS_ANY );

#if defined(SMPS)
            if( ( RCC_REQUEST_OK                   == retState  ) &&
                ( RCC_PWR_SUPPLY_SMPS_1V8_EXT_LDO  <= pwrSupply ) &&
                ( RCC_PWR_SUPPLY_SMPS_2V5_EXT      >= pwrSupply )    )
            {
                /* SMPS supplies external circuits - wait for the external supply ready */
                retState = RCC_REQUEST_ERROR;

                for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
                {
                    const uint32_t extSupplyReady = LL_PWR_IsActiveFlag_SMPSEXT();

                    if( 0u != extSupplyReady )
                    {
                        retState = RCC_REQUEST_OK;
                        break;
                    }
                    else
                    {
                        /* External supply is not ready yet */
                    }
                }
            }
            else
            {
                /* SMPS does not supply external circuits or error occurred */
            }
#elif defined(PWR_CSR2_SDEXTRDY)
            if( ( RCC_REQUEST_OK                   == retState  ) &&
                ( RCC_PWR_SUPPLY_SMPS_1V8_EXT_LDO  <= pwrSupply ) &&
                ( RCC_PWR_SUPPLY_SMPS_2V5_EXT      >= pwrSupply )    )
            {
                /* SMPS supplies external circuits - wait for the external supply ready */
                retState = RCC_REQUEST_ERROR;

                for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
                {
                    if( 0u != READ_BIT( PWR->CSR2, PWR_CSR2_SDEXTRDY ) )
                    {
                        retState = RCC_REQUEST_OK;
                        break;
                    }
                    else
                    {
                        /* External supply is not ready yet */
                    }
                }
            }
            else
            {
                /* SMPS does not supply external circuits or error occurred */
            }
#endif
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Waits until the voltage levels are ready for the active voltage scale
 *        and supply configuration (ACTVOSRDY) and the required voltage scale is
 *        applied (ACTVOS).
 *
 * \param activeScaleMask [in]: Mask of ACTVOS field, \ref RCC_PWR_ACTVOS_ANY - any scale
 * \param activeScale     [in]: Required ACTVOS field value (VOS field value)
 *
 * \return Returns "OK" if the voltage levels are ready. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Get_PwrVoltageReady( uint32_t activeScaleMask, uint32_t activeScale )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t levelReady = Rcc_Get_PwrVoltageLevelReady();

        if( ( ( activeScale & activeScaleMask ) == READ_BIT( RCC_PWR_ACTVOS_REG, activeScaleMask ) ) &&
            ( 0u                                != levelReady                                      )    )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Voltage levels are not ready yet */
        }
    }

    return ( retState );
}


/**
 * \brief Reads the voltage level ready flag of the active voltage scale and
 *        supply configuration (ACTVOSRDY - PWR_CSR1 on STM32H72x / H74x / H7A3,
 *        PWR_SR1 on STM32H7R / H7S).
 *
 * \return Flag state - 0 if the voltage level is not ready.
 */
static uint32_t Rcc_Get_PwrVoltageLevelReady( void )
{
#if defined(STM32H7RS)
    return ( LL_PWR_IsActiveFlag_ACTVOSRDY() );
#else
    return ( LL_PWR_IsActiveFlag_ACTVOS() );
#endif
}


/**
 * \brief Reads the active voltage scale (PWR_CSR1 ACTVOS, SYSCFG overdrive -
 *        PWR_SR1 ACTVOS on STM32H7R / H7S).
 *
 * \param voltageScale [out]: Active voltage scale
 *
 * \return Returns "OK" if the voltage scale is known. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Get_PwrVoltageScale( rcc_PwrVoltageScale_t * const voltageScale )
{
    rcc_RequestState_t retState    = RCC_REQUEST_ERROR;
    const uint32_t     activeScale = READ_BIT( RCC_PWR_ACTVOS_REG, RCC_PWR_ACTVOS_MASK );

    for( rcc_PwrVoltageScale_t scaleIdx = RCC_PWR_VOLTAGE_SCALE_0; RCC_PWR_VOLTAGE_SCALE_CNT > scaleIdx; scaleIdx ++ )
    {
        if( rcc_PwrVoltageScaleLut[ scaleIdx ] == activeScale )
        {
            *voltageScale = scaleIdx;
            retState      = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Other voltage scale */
        }
    }

#if defined(SYSCFG_PWRCR_ODEN)
    /* VOS0 and VOS1 have the same VOS field value - VOS0 is VOS1 with overdrive */
    if( ( RCC_REQUEST_OK          == retState                                   ) &&
        ( RCC_PWR_VOLTAGE_SCALE_0 == *voltageScale                              ) &&
        ( 0u                      == READ_BIT( SYSCFG->PWRCR, SYSCFG_PWRCR_ODEN ) )    )
    {
        *voltageScale = RCC_PWR_VOLTAGE_SCALE_1;
    }
    else
    {
        /* Voltage scale other than VOS0 / VOS1 or overdrive active */
    }
#endif

    return ( retState );
}


/**
 * \brief Calculates flash wait states required by the AXI clock (HCLK) in the
 *        active voltage scale.
 *
 * \param hclkFreq [in]: AXI clock frequency in Hz
 * \param latency [out]: FLASH_ACR LATENCY field value
 *
 * \return Returns "OK" if the frequency is allowed in the voltage scale.
 *         Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Get_FlashLatencyRequired( rcc_FreqHz_t hclkFreq, uint32_t * const latency )
{
    rcc_PwrVoltageScale_t voltageScale = RCC_PWR_VOLTAGE_SCALE_CNT;
    rcc_RequestState_t    retState     = Rcc_Get_PwrVoltageScale( &voltageScale );

    if( ( RCC_REQUEST_OK == retState ) &&
        ( 0u              < hclkFreq )    )
    {
        retState = RCC_REQUEST_ERROR;

        for( uint32_t waitStates = 0u; RCC_FLASH_WS_TABLE_SIZE > waitStates; waitStates ++ )
        {
            const rcc_FreqHz_t maxFreq = rcc_FlashLatencyMaxHclk[ voltageScale ][ waitStates ];

            if( hclkFreq <= maxFreq )
            {
                *latency = ( waitStates << FLASH_ACR_LATENCY_Pos );
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Frequency needs more wait states (0 - not allowed in the voltage scale) */
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
 * \brief Writes flash wait states and waits until they are applied.
 *
 * STM32H7R / H7S: the flash programming delay (WRHIGHFREQ) of the wait states
 * is written together with the wait states.
 *
 * \param latency [in]: FLASH_ACR LATENCY field value
 *
 * \return Returns "OK" if the latency is applied. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_FlashLatencyValue( uint32_t latency )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

#if defined(STM32H7RS)
    const uint32_t waitStates = ( latency & FLASH_ACR_LATENCY ) >> FLASH_ACR_LATENCY_Pos;
    const uint32_t timingVal  = ( RCC_FLASH_WS_TABLE_SIZE > waitStates ) ?
                                ( latency | rcc_FlashWrHighFreqLut[ waitStates ] ) :
                                ( latency | FLASH_ACR_WRHIGHFREQ );

    Rcc_Set_RegVal( RCC_REG_FLASH_ACR, RCC_FLASH_ACR_TIMING_MASK, timingVal );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t actualTiming = Rcc_Get_RegVal( RCC_REG_FLASH_ACR, RCC_FLASH_ACR_TIMING_MASK );

        if( timingVal == actualTiming )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Latency has not been applied yet */
        }
    }
#else
    LL_FLASH_SetLatency( latency );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t actualLatency = LL_FLASH_GetLatency();

        if( latency == actualLatency )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Latency has not been applied yet */
        }
    }
#endif

    return ( retState );
}


/**
 * \brief Increases flash wait states to the value valid for every clock frequency
 *        and voltage scale (used during the clock switch). Higher latency is kept.
 *
 * \return Returns "OK" if the latency is applied. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_FlashLatencySwitch( void )
{
    rcc_RequestState_t retState      = RCC_REQUEST_OK;
    const uint32_t     actualLatency = LL_FLASH_GetLatency();

    if( RCC_FLASH_LATENCY_SWITCH > actualLatency )
    {
        retState = Rcc_Set_FlashLatencyValue( RCC_FLASH_LATENCY_SWITCH );
    }
    else
    {
        /* Actual latency is valid for every clock */
    }

    return ( retState );
}


/**
 * \brief Sets flash wait states required by the actual AXI clock (HCLK) in the
 *        active voltage scale.
 *
 * \return Returns "OK" if the latency is applied. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_FlashLatencyActual( void )
{
    rcc_FreqHz_t       hclkFreq = 0u;
    uint32_t           latency  = RCC_FLASH_LATENCY_SWITCH;
    rcc_RequestState_t retState = Rcc_ClkBus_Get_AHBClk( &hclkFreq );

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Get_FlashLatencyRequired( hclkFreq, &latency );
    }
    else
    {
        /* Previous step failed */
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Set_FlashLatencyValue( latency );
    }
    else
    {
        /* Previous step failed */
    }

    return ( retState );
}


/**
 * \brief Updates CMSIS clock variables - SystemCoreClock (CPU clock) and
 *        SystemD2Clock (AXI clock, HCLK - not defined on STM32H7R / H7S).
 *
 * \return Returns "OK" if the clocks are known. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_ClkVariables( void )
{
    rcc_FreqHz_t       cpuFreq  = 0u;
    rcc_FreqHz_t       hclkFreq = 0u;
    rcc_RequestState_t retState = Rcc_ClkBus_Get_CpuClk( &cpuFreq );

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_ClkBus_Get_AHBClk( &hclkFreq );
    }
    else
    {
        /* Previous step failed */
    }

    if( RCC_REQUEST_OK == retState )
    {
        SystemCoreClock = cpuFreq;
#if defined(STM32H7RS)
        (void) hclkFreq;
#else
        SystemD2Clock   = hclkFreq;
#endif
    }
    else
    {
        /* Clocks are not known - variables are kept */
    }

    return ( retState );
}


/**
 * \brief Changes AHB prescaler at runtime. The flash latency is increased before
 *        the change and set for the new AXI clock afterwards, SystemD2Clock is updated.
 *
 * \param dividerId [in]: AHB prescaler value
 *
 * \return Returns "OK" if the prescaler is applied. Otherwise returns error.
 */
static rcc_RequestState_t Rcc_Set_AHBDividerSafe( rcc_AHB_Div_t dividerId )
{
    rcc_RequestState_t retState = Rcc_Set_FlashLatencySwitch();

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_ClkBus_Set_AHBDivider( dividerId );
    }
    else
    {
        /* Previous step failed */
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Set_FlashLatencyActual();
    }
    else
    {
        /* Previous step failed */
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Set_ClkVariables();
    }
    else
    {
        /* Previous step failed */
    }

    return ( retState );
}


/**
 * \brief Function used to wrap PLL1 clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
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
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_3_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutP( RCC_PLL_3, clkFreq ) );
}

#if defined(STM32H7RS)

/**
 * \brief Function used to wrap PLL1 clock output S frequency (STM32H7R / H7S)
 *
 * \param clkFreq [out]: Pointer to PLL1 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_1_SClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutS( RCC_PLL_1, clkFreq ) );
}

/**
 * \brief Function used to wrap PLL2 clock output S frequency (STM32H7R / H7S)
 *
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_2_SClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutS( RCC_PLL_2, clkFreq ) );
}

/**
 * \brief Function used to wrap PLL2 clock output T frequency (STM32H7R / H7S)
 *
 * \param clkFreq [out]: Pointer to PLL2 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_2_TClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutT( RCC_PLL_2, clkFreq ) );
}

/**
 * \brief Function used to wrap PLL3 clock output S frequency (STM32H7R / H7S)
 *
 * \param clkFreq [out]: Pointer to PLL3 clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_3_SClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutS( RCC_PLL_3, clkFreq ) );
}
#endif


/**
 * \brief Function used for system clock calculation from configuration setting
 *
 * This Function don't use register as reference for calculation of system
 * frequency. Instead of that, use configuration from configuration structure
 * (HSI / CSI frequency is read from the actual oscillator configuration).
 *
 * \param clockConfig [in]: Configuration structure
 * \param sysClk     [out]: Pointer to system clock frequency in Hz
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
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource )
        {
            const rcc_PllConfigStruct_t * const pllConfig = &clockConfig->Pll_Config[ RCC_PLL_1 ];

            if( RCC_PLL_SRC_HSI == pllConfig->Pll_Source )
            {
                retState = Rcc_ClkSrc_Get_Hsi64Clk( &pllSrcFreq );
            }
            else if( RCC_PLL_SRC_CSI == pllConfig->Pll_Source )
            {
                retState = Rcc_ClkSrc_Get_CsiClk( &pllSrcFreq );
            }
            else if( RCC_PLL_SRC_HSE == pllConfig->Pll_Source )
            {
                pllSrcFreq = clockConfig->HSE_Frequency_Hz;
                retState   = RCC_REQUEST_OK;
            }
            else
            {
                /* PLL is not used */
                retState = RCC_REQUEST_ERROR;
            }

            if( ( RCC_REQUEST_OK == retState              ) &&
                ( 0u             != pllConfig->M_Divider  ) &&
                ( 0u             != pllConfig->P_Divider  )    )
            {
                const uint64_t vcoFreq = ( (uint64_t)pllSrcFreq * pllConfig->N_Multiplier ) / pllConfig->M_Divider;

                *sysClk = (rcc_FreqHz_t)( vcoFreq / pllConfig->P_Divider );
            }
            else
            {
                /* Reached error state or PLL output disabled */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == clockConfig->SystemClockSource )
        {
            retState = Rcc_ClkSrc_Get_Hsi64Clk( sysClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_CSI == clockConfig->SystemClockSource )
        {
            retState = Rcc_ClkSrc_Get_CsiClk( sysClk );
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


/**
 * \brief Function used for AXI clock (HCLK) calculation from configuration setting
 *
 * \param clockConfig [in]: Configuration structure
 * \param hclkFreq   [out]: Pointer to AXI clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_ExpectedHclkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const hclkFreq )
{
    rcc_FreqHz_t       sysClk   = 0u;
    rcc_RequestState_t retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &sysClk );

    if( RCC_REQUEST_OK == retState )
    {
        const rcc_FreqHz_t cpuClk = Rcc_ClkBus_Calc_CpuClk( sysClk, clockConfig->SYS_Divider );

        *hclkFreq = Rcc_ClkBus_Calc_AHBClk( cpuClk, clockConfig->AHB_Divider );
    }
    else
    {
        /* System clock is not known */
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
