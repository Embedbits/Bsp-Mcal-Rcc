/**
 * \author Mr.Nobody
 * \file Rcc.c
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module common functionality
 *
 * \note  Exception of MCAL layering rule: PWR (voltage scaling, range 1 boost
 *        mode, backup domain access) and FLASH (latency, prefetch, caches) have
 *        no MCAL module. Their configuration is part of the clock configuration
 *        sequence, therefore RCC accesses their registers directly
 *        (\ref Rcc_Init, \ref Rcc_Set_PwrRange, \ref Rcc_Set_FlashLatency,
 *        \ref Rcc_Set_FlashPrefetchActive). New accesses shall be moved into a
 *        dedicated MCAL module once it exists.
 *
 * \note  STM32G4 family - kernel clock of an external source (I2S_CKIN of
 *        I2S / SAI) is not known by the module (\ref Rcc_Get_PeriphClk returns
 *        error).
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
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define RCC_MAJOR_VERSION                       ( 1u )
/** Value of minor version of SW module */
#define RCC_MINOR_VERSION                       ( 0u )
/** Value of patch version of SW module */
#define RCC_PATCH_VERSION                       ( 0u )

/** Maximal wait time for configuration request confirmation */
#define RCC_TIMEOUT_RAW                         ( 0x84FCB )

/** Value of mask meaning "function is not supported by the peripheral block" */
#define RCC_UNSUPPORTED_FUNCTION                ( 0u )

/** Mask of all reset source flags in RCC clock control & status register (CSR) */
#define RCC_CSR_RESET_SRC_MASK                  ( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_SFTRSTF  | RCC_CSR_IWDGRSTF | \
                                                  RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF | RCC_CSR_OBLRSTF )

/** Value of CSR register bits in cleared state */
#define RCC_CSR_BITS_CLEARED                    ( 0u )

/** Default HSE frequency used by \ref Rcc_Get_DefaultConfig (24 MHz crystal of NUCLEO-G474RE) */
#define RCC_DEFAULT_HSE_FREQ_HZ                 ( 24000000u )

/** Default PLL input divider - HSI 16 MHz / 4 gives 4 MHz PLL input frequency */
#define RCC_DEFAULT_PLL_M_DIV                   ( 4u )

/** Default PLL multiplier - VCO 340 MHz */
#define RCC_DEFAULT_PLL_N_MULT                  ( 85u )

/** Default PLL output P divider - 56.67 MHz ADC kernel clock (ADC maximum 60 MHz) */
#define RCC_DEFAULT_PLL_P_DIV                   ( 6u )

/** Default PLL output R divider - system clock 170 MHz */
#define RCC_DEFAULT_PLL_R_DIV                   ( 2u )

/** PLL output divider value - output is not used */
#define RCC_DEFAULT_PLL_OUT_UNUSED              ( 0u )

/** Default SysTick interval in ms */
#define RCC_DEFAULT_SYSTICK_INTERVAL_MS         ( 1u )

/** Default clock output divider (not divided) */
#define RCC_DEFAULT_CLK_OUT_DIV                 ( 1u )

/** System clock after reset - HSI 16 MHz, updated by \ref Rcc_Init */
#define RCC_SYSCLK_RESET_FREQ_HZ                ( HSI_VALUE )

/** Count of milliseconds in one second */
#define RCC_MS_IN_SECOND                        ( 1000u )

/** Minimum SysTick ticks count per interval (reload register value 1) */
#define RCC_SYSTICK_TICKS_MIN                   ( 2u )

/** Maximum SysTick ticks count per interval (24-bit reload register + 1) */
#define RCC_SYSTICK_TICKS_MAX                   ( SysTick_LOAD_RELOAD_Msk + 1u )

/** SysTick reload register holds ticks count decremented by 1 */
#define RCC_SYSTICK_RELOAD_OFFSET               ( 1u )

/** Maximal system clock in voltage range 1 boost mode */
#define RCC_MAX_FREQ_SCALE_0_HZ                 ( 170000000u )

/** Maximal system clock in voltage range 1 normal mode */
#define RCC_MAX_FREQ_SCALE_1_HZ                 ( 150000000u )

/** Maximal system clock in voltage range 2 */
#define RCC_MAX_FREQ_SCALE_2_HZ                 ( 26000000u )

/** System clock above which the AHB prescaler transition state (divider 2 for
 *  at least 1 us) is used when switching to the PLL (RM0440 "Clock
 *  configuration - transition state") */
#define RCC_SYSCLK_TRANSITION_HZ                ( 80000000u )

/** Count of read iterations giving at least 1 us at HCLK 85 MHz (AHB / 2) */
#define RCC_TRANSITION_WAIT_RAW                 ( 100u )

/** Flash latency used while the voltage range and the clock tree are changed
 *  (safe for every frequency of every range) */
#define RCC_FLASH_LATENCY_TRANSITION            ( RCC_FLASH_LATENCY_4_WS )

/** PWR_CR1 VOS value of voltage range 1 */
#define RCC_PWR_VOS_RANGE_1                     ( PWR_CR1_VOS_0 )

/** PWR_CR1 VOS value of voltage range 2 */
#define RCC_PWR_VOS_RANGE_2                     ( PWR_CR1_VOS_1 )

/* ============================== TYPEDEFS ================================== */

/** \brief Register banks - groups of enable / sleep / reset registers */
typedef enum
{
    RCC_REG_BANK_AHB1 = 0u, /**< AHB1 peripherals registers                   */
    RCC_REG_BANK_AHB2,      /**< AHB2 peripherals registers                   */
    RCC_REG_BANK_AHB3,      /**< AHB3 peripherals registers                   */
    RCC_REG_BANK_APB1_1,    /**< APB1 peripherals registers 1                 */
    RCC_REG_BANK_APB1_2,    /**< APB1 peripherals registers 2                 */
    RCC_REG_BANK_APB2,      /**< APB2 peripherals registers                   */
    RCC_REG_BANK_BDCR,      /**< Backup domain control register (RTC enable)  */
    RCC_REG_BANK_CNT        /**< Count of register banks                      */
}   rcc_RegBankId_t;


/**
 * \brief Peripheral block configuration structure
 *
 * Mask value \ref RCC_UNSUPPORTED_FUNCTION marks function not available for
 * the peripheral block.
 */
typedef struct __attribute__((packed))
{
    rcc_BlockList_t BlockId;     /**< Peripheral block ID                    */
    rcc_RegBankId_t RegBankId;   /**< Register bank ID.                      */
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


/** \brief Register bank configuration structure */
typedef struct
{
    rcc_RegBankId_t RegBankId;   /**< Register bank ID                                       */
    rcc_RegId_t     EnableRegId; /**< Peripheral clock enabled register                      */
    rcc_RegId_t     SleepRegId;  /**< Peripheral enabled in sleep mode register (CNT - none) */
    rcc_RegId_t     ResetRegId;  /**< Reset request register (CNT - none)                    */
}   rcc_RegBankConfigStruct_t;


typedef rcc_RequestState_t (*rcc_ClkSrcCallback_t)( rcc_FreqHz_t * const clkFreq );


typedef struct
{
    rcc_ClkSrcId_t       PeriphClkSrcId; /**< Peripheral clock source ID */
    rcc_ClkSrcCallback_t ClkSrcCallback; /**< Callback function pointer */
}   rcc_ClkSrcConfigStruct_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t  Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const sysClk );
static rcc_RequestState_t  Rcc_Set_ClkSrcOscActive( rcc_ClkSrcId_t clkSrcId );
static rcc_RequestState_t  Rcc_Set_SysClkTransition( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t sysClk );
static rcc_RequestState_t  Rcc_Wait_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal );
static uint32_t            Rcc_Get_FlashLatency( rcc_PwrVoltageScale_t voltageScale, rcc_FreqHz_t hclkFreq );
static rcc_FreqHz_t        Rcc_Get_ScaleMaxFreq( rcc_PwrVoltageScale_t voltageScale );
static rcc_FunctionState_t Rcc_Get_ClkMuxShared( rcc_PeriphId_t periphId );

static rcc_RequestState_t Rcc_Pll_Get_Main_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_Main_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_Main_RClk( rcc_FreqHz_t * const clkFreq );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* --------------------- Multipliers/Dividers arrays -------------------------*/

/* The most disgusting part in whole project. Definition of external variables
 * created by STM!!! Shame on you ST! */
uint32_t      SystemCoreClock    = RCC_SYSCLK_RESET_FREQ_HZ;
const uint8_t AHBPrescTable[16u] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8u]  = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Configuration array of registers used by register banks */
static const rcc_RegBankConfigStruct_t  rcc_RegBankConfig[] =
{
  { .RegBankId = RCC_REG_BANK_AHB1  , .EnableRegId = RCC_REG_AHB1ENR , .SleepRegId = RCC_REG_AHB1SMENR , .ResetRegId = RCC_REG_AHB1RSTR  },
  { .RegBankId = RCC_REG_BANK_AHB2  , .EnableRegId = RCC_REG_AHB2ENR , .SleepRegId = RCC_REG_AHB2SMENR , .ResetRegId = RCC_REG_AHB2RSTR  },
  { .RegBankId = RCC_REG_BANK_AHB3  , .EnableRegId = RCC_REG_AHB3ENR , .SleepRegId = RCC_REG_AHB3SMENR , .ResetRegId = RCC_REG_AHB3RSTR  },
  { .RegBankId = RCC_REG_BANK_APB1_1, .EnableRegId = RCC_REG_APB1ENR1, .SleepRegId = RCC_REG_APB1SMENR1, .ResetRegId = RCC_REG_APB1RSTR1 },
  { .RegBankId = RCC_REG_BANK_APB1_2, .EnableRegId = RCC_REG_APB1ENR2, .SleepRegId = RCC_REG_APB1SMENR2, .ResetRegId = RCC_REG_APB1RSTR2 },
  { .RegBankId = RCC_REG_BANK_APB2  , .EnableRegId = RCC_REG_APB2ENR , .SleepRegId = RCC_REG_APB2SMENR , .ResetRegId = RCC_REG_APB2RSTR  },
  { .RegBankId = RCC_REG_BANK_BDCR  , .EnableRegId = RCC_REG_BDCR    , .SleepRegId = RCC_REG_CNT       , .ResetRegId = RCC_REG_CNT       },
};

_Static_assert( (sizeof(rcc_RegBankConfig) / sizeof(rcc_RegBankConfigStruct_t)) == RCC_REG_BANK_CNT, "Rcc: rcc_RegBankConfig has incorrect size." );


static const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK    , .ClkSrcCallback = Rcc_ClkBus_Get_SysClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLLPCLK   , .ClkSrcCallback = Rcc_Pll_Get_Main_PClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLLQCLK   , .ClkSrcCallback = Rcc_Pll_Get_Main_QClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLLRCLK   , .ClkSrcCallback = Rcc_Pll_Get_Main_RClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK    , .ClkSrcCallback = Rcc_ClkBus_Get_AHBClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK   , .ClkSrcCallback = Rcc_ClkBus_Get_APB1Clk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK   , .ClkSrcCallback = Rcc_ClkBus_Get_APB2Clk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .ClkSrcCallback = Rcc_ClkBus_Get_APB1TimClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .ClkSrcCallback = Rcc_ClkBus_Get_APB2TimClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSICLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_HsiClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48CLK  , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi48Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_HseClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSERTCCLK , .ClkSrcCallback = Rcc_ClkSrc_Get_HseRtcClk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_LsiClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_LseClk     },
};

_Static_assert( (sizeof(rcc_PeriphClkSrcConfig) / sizeof(rcc_ClkSrcConfigStruct_t)) == RCC_CLK_SRC_CNT, "Rcc: rcc_PeriphClkSrcConfig has incorrect size." );


/** \brief Configuration array of MCU peripherals. */
static const rcc_PeriphConfigStruct_t   rcc_ConfigStruct[] =
{
  /*------------------------------ System core -------------------------------*/

  { .PeriphId = RCC_PERIPH_FLASH            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_FLASH   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_SYSCFG           , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SYSCFG  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_PWR              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_PWR     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_SYSTICK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SYSTICK , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_IWDG             , .ClkSrcId = RCC_CLK_SRC_LSICLK    , .BlockId = RCC_BLOCK_IWDG    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_RTC_HSE_DIV32    , .ClkSrcId = RCC_CLK_SRC_HSERTCCLK , .BlockId = RCC_BLOCK_RTC     , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32   },
  { .PeriphId = RCC_PERIPH_RTC_LSE          , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_RTC     , .ClkMuxId = RCC_CLK_MUX_RTC_LSE         },
  { .PeriphId = RCC_PERIPH_RTC_LSI          , .ClkSrcId = RCC_CLK_SRC_LSICLK    , .BlockId = RCC_BLOCK_RTC     , .ClkMuxId = RCC_CLK_MUX_RTC_LSI         },
  { .PeriphId = RCC_PERIPH_RTCAPB           , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_RTCAPB  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_SRAM1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SRAM1   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#if defined(RCC_AHB2SMENR_SRAM2SMEN)
  { .PeriphId = RCC_PERIPH_SRAM2            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SRAM2   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_AHB2SMENR_SRAM2SMEN */
  { .PeriphId = RCC_PERIPH_CCMSRAM          , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_CCMSRAM , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_DMA1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DMA1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_DMA2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DMA2    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_DMAMUX1          , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DMAMUX1 , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_WWDG             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_WWDG    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_CRS              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_CRS     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOA            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOA   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOB            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOB   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOC            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOC   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOD            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOD   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOE            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOE   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOF            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOF   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_GPIOG            , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOG   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },

  /*--------------------------------- Timers ---------------------------------*/

  { .PeriphId = RCC_PERIPH_TIM1             , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM2             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM2    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM3             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM3    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM4             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM4    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#if defined(RCC_APB1ENR1_TIM5EN)
  { .PeriphId = RCC_PERIPH_TIM5             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM5    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_APB1ENR1_TIM5EN */
  { .PeriphId = RCC_PERIPH_TIM6             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM6    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM7             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM7    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM8             , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM8    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM15            , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM15   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM16            , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM16   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_TIM17            , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM17   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#if defined(RCC_APB2ENR_TIM20EN)
  { .PeriphId = RCC_PERIPH_TIM20            , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM20   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_APB2ENR_TIM20EN */
#if defined(RCC_APB2ENR_HRTIM1EN)
  { .PeriphId = RCC_PERIPH_HRTIM1           , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_HRTIM1  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_APB2ENR_HRTIM1EN */
  { .PeriphId = RCC_PERIPH_LPTIM1_PCLK1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1    },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSI       , .ClkSrcId = RCC_CLK_SRC_LSICLK    , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI      },
  { .PeriphId = RCC_PERIPH_LPTIM1_HSI       , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI      },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE      },

  /*------------------------------ Connectivity ------------------------------*/

  { .PeriphId = RCC_PERIPH_SPI1             , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SPI1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_SPI2             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_SPI2    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#if defined(RCC_APB1ENR1_SPI3EN)
  { .PeriphId = RCC_PERIPH_SPI3             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_SPI3    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_APB1ENR1_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
  { .PeriphId = RCC_PERIPH_SPI4             , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SPI4    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_CCIPR_I2S23SEL)
  { .PeriphId = RCC_PERIPH_I2S23_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_I2S23   , .ClkMuxId = RCC_CLK_MUX_I2S23_SYSCLK    },
  { .PeriphId = RCC_PERIPH_I2S23_PLLQ       , .ClkSrcId = RCC_CLK_SRC_PLLQCLK   , .BlockId = RCC_BLOCK_I2S23   , .ClkMuxId = RCC_CLK_MUX_I2S23_PLLQ      },
  { .PeriphId = RCC_PERIPH_I2S23_I2S_CKIN   , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_I2S23   , .ClkMuxId = RCC_CLK_MUX_I2S23_I2S_CKIN  },
  { .PeriphId = RCC_PERIPH_I2S23_HSI        , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_I2S23   , .ClkMuxId = RCC_CLK_MUX_I2S23_HSI       },
#endif /* RCC_CCIPR_I2S23SEL */
  { .PeriphId = RCC_PERIPH_I2C1_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C1    , .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1      },
  { .PeriphId = RCC_PERIPH_I2C1_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_I2C1    , .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK     },
  { .PeriphId = RCC_PERIPH_I2C1_HSI         , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_I2C1    , .ClkMuxId = RCC_CLK_MUX_I2C1_HSI        },
  { .PeriphId = RCC_PERIPH_I2C2_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C2    , .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1      },
  { .PeriphId = RCC_PERIPH_I2C2_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_I2C2    , .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK     },
  { .PeriphId = RCC_PERIPH_I2C2_HSI         , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_I2C2    , .ClkMuxId = RCC_CLK_MUX_I2C2_HSI        },
#if defined(RCC_APB1ENR1_I2C3EN)
  { .PeriphId = RCC_PERIPH_I2C3_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C3    , .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK1      },
  { .PeriphId = RCC_PERIPH_I2C3_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_I2C3    , .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK     },
  { .PeriphId = RCC_PERIPH_I2C3_HSI         , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_I2C3    , .ClkMuxId = RCC_CLK_MUX_I2C3_HSI        },
#endif /* RCC_APB1ENR1_I2C3EN */
#if defined(RCC_APB1ENR2_I2C4EN)
  { .PeriphId = RCC_PERIPH_I2C4_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C4    , .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1      },
  { .PeriphId = RCC_PERIPH_I2C4_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_I2C4    , .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK     },
  { .PeriphId = RCC_PERIPH_I2C4_HSI         , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_I2C4    , .ClkMuxId = RCC_CLK_MUX_I2C4_HSI        },
#endif /* RCC_APB1ENR2_I2C4EN */
  { .PeriphId = RCC_PERIPH_USART1_PCLK2     , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_USART1  , .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2    },
  { .PeriphId = RCC_PERIPH_USART1_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_USART1  , .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK   },
  { .PeriphId = RCC_PERIPH_USART1_HSI       , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_USART1  , .ClkMuxId = RCC_CLK_MUX_USART1_HSI      },
  { .PeriphId = RCC_PERIPH_USART1_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_USART1  , .ClkMuxId = RCC_CLK_MUX_USART1_LSE      },
  { .PeriphId = RCC_PERIPH_USART2_PCLK1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_USART2  , .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1    },
  { .PeriphId = RCC_PERIPH_USART2_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_USART2  , .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK   },
  { .PeriphId = RCC_PERIPH_USART2_HSI       , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_USART2  , .ClkMuxId = RCC_CLK_MUX_USART2_HSI      },
  { .PeriphId = RCC_PERIPH_USART2_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_USART2  , .ClkMuxId = RCC_CLK_MUX_USART2_LSE      },
#if defined(RCC_APB1ENR1_USART3EN)
  { .PeriphId = RCC_PERIPH_USART3_PCLK1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_USART3  , .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1    },
  { .PeriphId = RCC_PERIPH_USART3_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_USART3  , .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK   },
  { .PeriphId = RCC_PERIPH_USART3_HSI       , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_USART3  , .ClkMuxId = RCC_CLK_MUX_USART3_HSI      },
  { .PeriphId = RCC_PERIPH_USART3_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_USART3  , .ClkMuxId = RCC_CLK_MUX_USART3_LSE      },
#endif /* RCC_APB1ENR1_USART3EN */
  { .PeriphId = RCC_PERIPH_UART4_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_UART4   , .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1     },
  { .PeriphId = RCC_PERIPH_UART4_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_UART4   , .ClkMuxId = RCC_CLK_MUX_UART4_SYSCLK    },
  { .PeriphId = RCC_PERIPH_UART4_HSI        , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_UART4   , .ClkMuxId = RCC_CLK_MUX_UART4_HSI       },
  { .PeriphId = RCC_PERIPH_UART4_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_UART4   , .ClkMuxId = RCC_CLK_MUX_UART4_LSE       },
#if defined(RCC_APB1ENR1_UART5EN)
  { .PeriphId = RCC_PERIPH_UART5_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_UART5   , .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1     },
  { .PeriphId = RCC_PERIPH_UART5_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_UART5   , .ClkMuxId = RCC_CLK_MUX_UART5_SYSCLK    },
  { .PeriphId = RCC_PERIPH_UART5_HSI        , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_UART5   , .ClkMuxId = RCC_CLK_MUX_UART5_HSI       },
  { .PeriphId = RCC_PERIPH_UART5_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_UART5   , .ClkMuxId = RCC_CLK_MUX_UART5_LSE       },
#endif /* RCC_APB1ENR1_UART5EN */
  { .PeriphId = RCC_PERIPH_LPUART1_PCLK1    , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1   },
  { .PeriphId = RCC_PERIPH_LPUART1_SYSCLK   , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK  },
  { .PeriphId = RCC_PERIPH_LPUART1_HSI      , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI     },
  { .PeriphId = RCC_PERIPH_LPUART1_LSE      , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE     },
  { .PeriphId = RCC_PERIPH_FDCAN_HSE        , .ClkSrcId = RCC_CLK_SRC_HSECLK    , .BlockId = RCC_BLOCK_FDCAN   , .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE       },
  { .PeriphId = RCC_PERIPH_FDCAN_PLLQ       , .ClkSrcId = RCC_CLK_SRC_PLLQCLK   , .BlockId = RCC_BLOCK_FDCAN   , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLLQ      },
  { .PeriphId = RCC_PERIPH_FDCAN_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_FDCAN   , .ClkMuxId = RCC_CLK_MUX_FDCAN_PCLK1     },
#if defined(RCC_APB1ENR1_USBEN)
  { .PeriphId = RCC_PERIPH_USB_HSI48        , .ClkSrcId = RCC_CLK_SRC_HSI48CLK  , .BlockId = RCC_BLOCK_USB     , .ClkMuxId = RCC_CLK_MUX_CLK48_HSI48     },
  { .PeriphId = RCC_PERIPH_USB_PLLQ         , .ClkSrcId = RCC_CLK_SRC_PLLQCLK   , .BlockId = RCC_BLOCK_USB     , .ClkMuxId = RCC_CLK_MUX_CLK48_PLLQ      },
#endif /* RCC_APB1ENR1_USBEN */
#if defined(RCC_APB1ENR2_UCPD1EN)
  { .PeriphId = RCC_PERIPH_UCPD1            , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_UCPD1   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_APB1ENR2_UCPD1EN */
#if defined(RCC_AHB3ENR_FMCEN)
  { .PeriphId = RCC_PERIPH_FMC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_FMC     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
  { .PeriphId = RCC_PERIPH_QSPI_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_QSPI    , .ClkMuxId = RCC_CLK_MUX_QSPI_SYSCLK     },
  { .PeriphId = RCC_PERIPH_QSPI_HSI         , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_QSPI    , .ClkMuxId = RCC_CLK_MUX_QSPI_HSI        },
  { .PeriphId = RCC_PERIPH_QSPI_PLLQ        , .ClkSrcId = RCC_CLK_SRC_PLLQCLK   , .BlockId = RCC_BLOCK_QSPI    , .ClkMuxId = RCC_CLK_MUX_QSPI_PLLQ       },
#endif /* RCC_AHB3ENR_QSPIEN */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(RCC_APB2ENR_SAI1EN)
  { .PeriphId = RCC_PERIPH_SAI1_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_SAI1    , .ClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK     },
  { .PeriphId = RCC_PERIPH_SAI1_PLLQ        , .ClkSrcId = RCC_CLK_SRC_PLLQCLK   , .BlockId = RCC_BLOCK_SAI1    , .ClkMuxId = RCC_CLK_MUX_SAI1_PLLQ       },
  { .PeriphId = RCC_PERIPH_SAI1_I2S_CKIN    , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_SAI1    , .ClkMuxId = RCC_CLK_MUX_SAI1_I2S_CKIN   },
  { .PeriphId = RCC_PERIPH_SAI1_HSI         , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_SAI1    , .ClkMuxId = RCC_CLK_MUX_SAI1_HSI        },
#endif /* RCC_APB2ENR_SAI1EN */

  /*--------------------------------- Analog ---------------------------------*/

  { .PeriphId = RCC_PERIPH_ADC12_HCLK       , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_ADC12   , .ClkMuxId = RCC_CLK_MUX_ADC12_HCLK      },
  { .PeriphId = RCC_PERIPH_ADC12_PLLP       , .ClkSrcId = RCC_CLK_SRC_PLLPCLK   , .BlockId = RCC_BLOCK_ADC12   , .ClkMuxId = RCC_CLK_MUX_ADC12_PLLP      },
  { .PeriphId = RCC_PERIPH_ADC12_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_ADC12   , .ClkMuxId = RCC_CLK_MUX_ADC12_SYSCLK    },
#if defined(RCC_AHB2ENR_ADC345EN)
  { .PeriphId = RCC_PERIPH_ADC345_HCLK      , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_ADC345  , .ClkMuxId = RCC_CLK_MUX_ADC345_HCLK     },
  { .PeriphId = RCC_PERIPH_ADC345_PLLP      , .ClkSrcId = RCC_CLK_SRC_PLLPCLK   , .BlockId = RCC_BLOCK_ADC345  , .ClkMuxId = RCC_CLK_MUX_ADC345_PLLP     },
  { .PeriphId = RCC_PERIPH_ADC345_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_ADC345  , .ClkMuxId = RCC_CLK_MUX_ADC345_SYSCLK   },
#endif /* RCC_AHB2ENR_ADC345EN */
  { .PeriphId = RCC_PERIPH_DAC1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DAC1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#if defined(RCC_AHB2ENR_DAC2EN)
  { .PeriphId = RCC_PERIPH_DAC2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DAC2    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_AHB2ENR_DAC2EN */
  { .PeriphId = RCC_PERIPH_DAC3             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DAC3    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#if defined(RCC_AHB2ENR_DAC4EN)
  { .PeriphId = RCC_PERIPH_DAC4             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DAC4    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_AHB2ENR_DAC4EN */

  /*-------------------------------- Security --------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
  { .PeriphId = RCC_PERIPH_AES              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_AES     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
#endif /* RCC_AHB2ENR_AESEN */
  { .PeriphId = RCC_PERIPH_RNG_HSI48        , .ClkSrcId = RCC_CLK_SRC_HSI48CLK  , .BlockId = RCC_BLOCK_RNG     , .ClkMuxId = RCC_CLK_MUX_CLK48_HSI48     },
  { .PeriphId = RCC_PERIPH_RNG_PLLQ         , .ClkSrcId = RCC_CLK_SRC_PLLQCLK   , .BlockId = RCC_BLOCK_RNG     , .ClkMuxId = RCC_CLK_MUX_CLK48_PLLQ      },

  /*------------------------------- Computing --------------------------------*/

  { .PeriphId = RCC_PERIPH_CORDIC           , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_CORDIC  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_CRC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_CRC     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
  { .PeriphId = RCC_PERIPH_FMAC             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_FMAC    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT        },
};

_Static_assert( (sizeof(rcc_ConfigStruct) / sizeof(rcc_PeriphConfigStruct_t)) == RCC_PERIPH_ID_CNT, "Rcc: rcc_ConfigStruct has incorrect size." );


/** \brief Configuration registers of RCC peripheral blocks.
 *
 * This array is created to reduce size of configuration.
 */
static const rcc_BlockConfigStruct_t    rcc_PeriphBlockConfig[] =
{
  /*------------------------------ System core -------------------------------*/

  { .BlockId = RCC_BLOCK_FLASH   , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_FLASHEN       , .LpCtrlMask = RCC_AHB1SMENR_FLASHSMEN     , .RstCtrlMask = RCC_AHB1RSTR_FLASHRST      },
  { .BlockId = RCC_BLOCK_SYSCFG  , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_SYSCFGEN      , .LpCtrlMask = RCC_APB2SMENR_SYSCFGSMEN    , .RstCtrlMask = RCC_APB2RSTR_SYSCFGRST     },
  { .BlockId = RCC_BLOCK_PWR     , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_PWREN        , .LpCtrlMask = RCC_APB1SMENR1_PWRSMEN      , .RstCtrlMask = RCC_APB1RSTR1_PWRRST       },
  { .BlockId = RCC_BLOCK_SYSTICK , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_UNSUPPORTED_FUNCTION  , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_IWDG    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_UNSUPPORTED_FUNCTION  , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_RTC     , .RegBankId = RCC_REG_BANK_BDCR   , .StateMask = RCC_BDCR_RTCEN            , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_RTCAPB  , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_RTCAPBEN     , .LpCtrlMask = RCC_APB1SMENR1_RTCAPBSMEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_SRAM1   , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_UNSUPPORTED_FUNCTION  , .LpCtrlMask = RCC_AHB1SMENR_SRAM1SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#if defined(RCC_AHB2SMENR_SRAM2SMEN)
  { .BlockId = RCC_BLOCK_SRAM2   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_UNSUPPORTED_FUNCTION  , .LpCtrlMask = RCC_AHB2SMENR_SRAM2SMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB2SMENR_SRAM2SMEN */
  { .BlockId = RCC_BLOCK_CCMSRAM , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_UNSUPPORTED_FUNCTION  , .LpCtrlMask = RCC_AHB2SMENR_CCMSRAMSMEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_DMA1    , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_DMA1EN        , .LpCtrlMask = RCC_AHB1SMENR_DMA1SMEN      , .RstCtrlMask = RCC_AHB1RSTR_DMA1RST       },
  { .BlockId = RCC_BLOCK_DMA2    , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_DMA2EN        , .LpCtrlMask = RCC_AHB1SMENR_DMA2SMEN      , .RstCtrlMask = RCC_AHB1RSTR_DMA2RST       },
  { .BlockId = RCC_BLOCK_DMAMUX1 , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_DMAMUX1EN     , .LpCtrlMask = RCC_AHB1SMENR_DMAMUX1SMEN   , .RstCtrlMask = RCC_AHB1RSTR_DMAMUX1RST    },
  { .BlockId = RCC_BLOCK_WWDG    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_WWDGEN       , .LpCtrlMask = RCC_APB1SMENR1_WWDGSMEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_CRS     , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_CRSEN        , .LpCtrlMask = RCC_APB1SMENR1_CRSSMEN      , .RstCtrlMask = RCC_APB1RSTR1_CRSRST       },
  { .BlockId = RCC_BLOCK_GPIOA   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIOAEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIOASMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIOARST      },
  { .BlockId = RCC_BLOCK_GPIOB   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIOBEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIOBSMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIOBRST      },
  { .BlockId = RCC_BLOCK_GPIOC   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIOCEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIOCSMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIOCRST      },
  { .BlockId = RCC_BLOCK_GPIOD   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIODEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIODSMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIODRST      },
  { .BlockId = RCC_BLOCK_GPIOE   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIOEEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIOESMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIOERST      },
  { .BlockId = RCC_BLOCK_GPIOF   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIOFEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIOFSMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIOFRST      },
  { .BlockId = RCC_BLOCK_GPIOG   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_GPIOGEN       , .LpCtrlMask = RCC_AHB2SMENR_GPIOGSMEN     , .RstCtrlMask = RCC_AHB2RSTR_GPIOGRST      },

  /*--------------------------------- Timers ---------------------------------*/

  { .BlockId = RCC_BLOCK_TIM1    , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_TIM1EN        , .LpCtrlMask = RCC_APB2SMENR_TIM1SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM1RST       },
  { .BlockId = RCC_BLOCK_TIM2    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_TIM2EN       , .LpCtrlMask = RCC_APB1SMENR1_TIM2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM2RST      },
  { .BlockId = RCC_BLOCK_TIM3    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_TIM3EN       , .LpCtrlMask = RCC_APB1SMENR1_TIM3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM3RST      },
  { .BlockId = RCC_BLOCK_TIM4    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_TIM4EN       , .LpCtrlMask = RCC_APB1SMENR1_TIM4SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM4RST      },
#if defined(RCC_APB1ENR1_TIM5EN)
  { .BlockId = RCC_BLOCK_TIM5    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_TIM5EN       , .LpCtrlMask = RCC_APB1SMENR1_TIM5SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM5RST      },
#endif /* RCC_APB1ENR1_TIM5EN */
  { .BlockId = RCC_BLOCK_TIM6    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_TIM6EN       , .LpCtrlMask = RCC_APB1SMENR1_TIM6SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM6RST      },
  { .BlockId = RCC_BLOCK_TIM7    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_TIM7EN       , .LpCtrlMask = RCC_APB1SMENR1_TIM7SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM7RST      },
  { .BlockId = RCC_BLOCK_TIM8    , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_TIM8EN        , .LpCtrlMask = RCC_APB2SMENR_TIM8SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM8RST       },
  { .BlockId = RCC_BLOCK_TIM15   , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_TIM15EN       , .LpCtrlMask = RCC_APB2SMENR_TIM15SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM15RST      },
  { .BlockId = RCC_BLOCK_TIM16   , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_TIM16EN       , .LpCtrlMask = RCC_APB2SMENR_TIM16SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM16RST      },
  { .BlockId = RCC_BLOCK_TIM17   , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_TIM17EN       , .LpCtrlMask = RCC_APB2SMENR_TIM17SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM17RST      },
#if defined(RCC_APB2ENR_TIM20EN)
  { .BlockId = RCC_BLOCK_TIM20   , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_TIM20EN       , .LpCtrlMask = RCC_APB2SMENR_TIM20SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM20RST      },
#endif /* RCC_APB2ENR_TIM20EN */
#if defined(RCC_APB2ENR_HRTIM1EN)
  { .BlockId = RCC_BLOCK_HRTIM1  , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_HRTIM1EN      , .LpCtrlMask = RCC_APB2SMENR_HRTIM1SMEN    , .RstCtrlMask = RCC_APB2RSTR_HRTIM1RST     },
#endif /* RCC_APB2ENR_HRTIM1EN */
  { .BlockId = RCC_BLOCK_LPTIM1  , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_LPTIM1EN     , .LpCtrlMask = RCC_APB1SMENR1_LPTIM1SMEN   , .RstCtrlMask = RCC_APB1RSTR1_LPTIM1RST    },

  /*------------------------------ Connectivity ------------------------------*/

  { .BlockId = RCC_BLOCK_SPI1    , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_SPI1EN        , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN      , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       },
  { .BlockId = RCC_BLOCK_SPI2    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_SPI2EN       , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST      },
#if defined(RCC_APB1ENR1_SPI3EN)
  { .BlockId = RCC_BLOCK_SPI3    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_SPI3EN       , .LpCtrlMask = RCC_APB1SMENR1_SPI3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_SPI3RST      },
#endif /* RCC_APB1ENR1_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
  { .BlockId = RCC_BLOCK_SPI4    , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_SPI4EN        , .LpCtrlMask = RCC_APB2SMENR_SPI4SMEN      , .RstCtrlMask = RCC_APB2RSTR_SPI4RST       },
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_CCIPR_I2S23SEL)
  { .BlockId = RCC_BLOCK_I2S23   , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_UNSUPPORTED_FUNCTION  , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_CCIPR_I2S23SEL */
  { .BlockId = RCC_BLOCK_I2C1    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_I2C1EN       , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN     , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST      },
  { .BlockId = RCC_BLOCK_I2C2    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_I2C2EN       , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST      },
#if defined(RCC_APB1ENR1_I2C3EN)
  { .BlockId = RCC_BLOCK_I2C3    , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_I2C3EN       , .LpCtrlMask = RCC_APB1SMENR1_I2C3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_I2C3RST      },
#endif /* RCC_APB1ENR1_I2C3EN */
#if defined(RCC_APB1ENR2_I2C4EN)
  { .BlockId = RCC_BLOCK_I2C4    , .RegBankId = RCC_REG_BANK_APB1_2 , .StateMask = RCC_APB1ENR2_I2C4EN       , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN     , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST      },
#endif /* RCC_APB1ENR2_I2C4EN */
  { .BlockId = RCC_BLOCK_USART1  , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_USART1EN      , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN    , .RstCtrlMask = RCC_APB2RSTR_USART1RST     },
  { .BlockId = RCC_BLOCK_USART2  , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_USART2EN     , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN   , .RstCtrlMask = RCC_APB1RSTR1_USART2RST    },
#if defined(RCC_APB1ENR1_USART3EN)
  { .BlockId = RCC_BLOCK_USART3  , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_USART3EN     , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN   , .RstCtrlMask = RCC_APB1RSTR1_USART3RST    },
#endif /* RCC_APB1ENR1_USART3EN */
  { .BlockId = RCC_BLOCK_UART4   , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_UART4EN      , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN    , .RstCtrlMask = RCC_APB1RSTR1_UART4RST     },
#if defined(RCC_APB1ENR1_UART5EN)
  { .BlockId = RCC_BLOCK_UART5   , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_UART5EN      , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN    , .RstCtrlMask = RCC_APB1RSTR1_UART5RST     },
#endif /* RCC_APB1ENR1_UART5EN */
  { .BlockId = RCC_BLOCK_LPUART1 , .RegBankId = RCC_REG_BANK_APB1_2 , .StateMask = RCC_APB1ENR2_LPUART1EN    , .LpCtrlMask = RCC_APB1SMENR2_LPUART1SMEN  , .RstCtrlMask = RCC_APB1RSTR2_LPUART1RST   },
  { .BlockId = RCC_BLOCK_FDCAN   , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_FDCANEN      , .LpCtrlMask = RCC_APB1SMENR1_FDCANSMEN    , .RstCtrlMask = RCC_APB1RSTR1_FDCANRST     },
#if defined(RCC_APB1ENR1_USBEN)
  { .BlockId = RCC_BLOCK_USB     , .RegBankId = RCC_REG_BANK_APB1_1 , .StateMask = RCC_APB1ENR1_USBEN        , .LpCtrlMask = RCC_APB1SMENR1_USBSMEN      , .RstCtrlMask = RCC_APB1RSTR1_USBRST       },
#endif /* RCC_APB1ENR1_USBEN */
#if defined(RCC_APB1ENR2_UCPD1EN)
  { .BlockId = RCC_BLOCK_UCPD1   , .RegBankId = RCC_REG_BANK_APB1_2 , .StateMask = RCC_APB1ENR2_UCPD1EN      , .LpCtrlMask = RCC_APB1SMENR2_UCPD1SMEN    , .RstCtrlMask = RCC_APB1RSTR2_UCPD1RST     },
#endif /* RCC_APB1ENR2_UCPD1EN */
#if defined(RCC_AHB3ENR_FMCEN)
  { .BlockId = RCC_BLOCK_FMC     , .RegBankId = RCC_REG_BANK_AHB3   , .StateMask = RCC_AHB3ENR_FMCEN         , .LpCtrlMask = RCC_AHB3SMENR_FMCSMEN       , .RstCtrlMask = RCC_AHB3RSTR_FMCRST        },
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
  { .BlockId = RCC_BLOCK_QSPI    , .RegBankId = RCC_REG_BANK_AHB3   , .StateMask = RCC_AHB3ENR_QSPIEN        , .LpCtrlMask = RCC_AHB3SMENR_QSPISMEN      , .RstCtrlMask = RCC_AHB3RSTR_QSPIRST       },
#endif /* RCC_AHB3ENR_QSPIEN */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(RCC_APB2ENR_SAI1EN)
  { .BlockId = RCC_BLOCK_SAI1    , .RegBankId = RCC_REG_BANK_APB2   , .StateMask = RCC_APB2ENR_SAI1EN        , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN      , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       },
#endif /* RCC_APB2ENR_SAI1EN */

  /*--------------------------------- Analog ---------------------------------*/

  { .BlockId = RCC_BLOCK_ADC12   , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_ADC12EN       , .LpCtrlMask = RCC_AHB2SMENR_ADC12SMEN     , .RstCtrlMask = RCC_AHB2RSTR_ADC12RST      },
#if defined(RCC_AHB2ENR_ADC345EN)
  { .BlockId = RCC_BLOCK_ADC345  , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_ADC345EN      , .LpCtrlMask = RCC_AHB2SMENR_ADC345SMEN    , .RstCtrlMask = RCC_AHB2RSTR_ADC345RST     },
#endif /* RCC_AHB2ENR_ADC345EN */
  { .BlockId = RCC_BLOCK_DAC1    , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_DAC1EN        , .LpCtrlMask = RCC_AHB2SMENR_DAC1SMEN      , .RstCtrlMask = RCC_AHB2RSTR_DAC1RST       },
#if defined(RCC_AHB2ENR_DAC2EN)
  { .BlockId = RCC_BLOCK_DAC2    , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_DAC2EN        , .LpCtrlMask = RCC_AHB2SMENR_DAC2SMEN      , .RstCtrlMask = RCC_AHB2RSTR_DAC2RST       },
#endif /* RCC_AHB2ENR_DAC2EN */
  { .BlockId = RCC_BLOCK_DAC3    , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_DAC3EN        , .LpCtrlMask = RCC_AHB2SMENR_DAC3SMEN      , .RstCtrlMask = RCC_AHB2RSTR_DAC3RST       },
#if defined(RCC_AHB2ENR_DAC4EN)
  { .BlockId = RCC_BLOCK_DAC4    , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_DAC4EN        , .LpCtrlMask = RCC_AHB2SMENR_DAC4SMEN      , .RstCtrlMask = RCC_AHB2RSTR_DAC4RST       },
#endif /* RCC_AHB2ENR_DAC4EN */

  /*-------------------------------- Security --------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
  { .BlockId = RCC_BLOCK_AES     , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_AESEN         , .LpCtrlMask = RCC_AHB2SMENR_AESSMEN       , .RstCtrlMask = RCC_AHB2RSTR_AESRST        },
#endif /* RCC_AHB2ENR_AESEN */
  { .BlockId = RCC_BLOCK_RNG     , .RegBankId = RCC_REG_BANK_AHB2   , .StateMask = RCC_AHB2ENR_RNGEN         , .LpCtrlMask = RCC_AHB2SMENR_RNGSMEN       , .RstCtrlMask = RCC_AHB2RSTR_RNGRST        },

  /*------------------------------- Computing --------------------------------*/

  { .BlockId = RCC_BLOCK_CORDIC  , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_CORDICEN      , .LpCtrlMask = RCC_AHB1SMENR_CORDICSMEN    , .RstCtrlMask = RCC_AHB1RSTR_CORDICRST     },
  { .BlockId = RCC_BLOCK_CRC     , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_CRCEN         , .LpCtrlMask = RCC_AHB1SMENR_CRCSMEN       , .RstCtrlMask = RCC_AHB1RSTR_CRCRST        },
  { .BlockId = RCC_BLOCK_FMAC    , .RegBankId = RCC_REG_BANK_AHB1   , .StateMask = RCC_AHB1ENR_FMACEN        , .LpCtrlMask = RCC_AHB1SMENR_FMACSMEN      , .RstCtrlMask = RCC_AHB1RSTR_FMACRST       },
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

/* ------------------------- Flash latency thresholds ----------------------- */

/** \brief HCLK frequencies above which next wait state is required - range 1 boost mode */
static const rcc_FreqHz_t rcc_FlashLatencyScale0[] =
{
     34000000u,
     68000000u,
    102000000u,
    136000000u,
};

/** \brief HCLK frequencies above which next wait state is required - range 1 normal mode */
static const rcc_FreqHz_t rcc_FlashLatencyScale1[] =
{
     30000000u,
     60000000u,
     90000000u,
    120000000u,
};

/** \brief HCLK frequencies above which next wait state is required - range 2 */
static const rcc_FreqHz_t rcc_FlashLatencyScale2[] =
{
     12000000u,
     24000000u,
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
 *
 * Configuration sequence:
 *  1. PWR and SYSCFG interface clocks, flash prefetch and caches are activated.
 *  2. System clock is switched to HSI, flash latency is set to the value safe
 *     for every frequency and the PLL is de-activated.
 *  3. Voltage range (and range 1 boost mode), HSE oscillator and CSS are
 *     configured.
 *  4. PLL is configured and activated, bus dividers are configured.
 *  5. System clock source is switched (AHB prescaler transition state above
 *     80 MHz), flash latency is reduced to the required value.
 *  6. SysTick, CMSIS SystemCoreClock and clock outputs are configured.
 *
 * \param clockConfig [in]: Clock configuration.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Init( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState       = RCC_REQUEST_OK;
    rcc_FreqHz_t       expectedSysClk = 0u;

    if( RCC_NULL_PTR != clockConfig )
    {
        /* Consistency of configuration arrays */
        retState = Rcc_Pll_Init();

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkMux_Init();
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_PWR );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_SYSCFG );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashPrefetchActive();
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Flash instruction and data caches */
            Rcc_Set_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_ICEN | FLASH_ACR_DCEN );

            /* The PLL cannot be deactivated by hardware while it drives SYSCLK
             * (e.g. after a previous Rcc_Init() call). Move SYSCLK to HSI,
             * which is always available, before the PLL is reconfigured. */
            retState = Rcc_ClkSrc_Set_HsiActive();
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_HSI );
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Latency safe for every frequency while range and clocks change */
            Rcc_Set_RegVal( RCC_REG_FLASH_ACR, FLASH_ACR_LATENCY, (uint32_t)RCC_FLASH_LATENCY_TRANSITION );

            retState = Rcc_Wait_RegVal( RCC_REG_FLASH_ACR, FLASH_ACR_LATENCY, (uint32_t)RCC_FLASH_LATENCY_TRANSITION );
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Inactive( pllId );
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PwrRange( clockConfig );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_HseActive( clockConfig->HSE_ClockType );
        }

        if( ( RCC_REQUEST_OK    == retState                   ) &&
            ( RCC_HSE_TYPE_NONE != clockConfig->HSE_ClockType )    )
        {
            retState = Rcc_ClkSrc_Set_HseClk( clockConfig->HSE_Frequency_Hz );
        }

        if( RCC_REQUEST_OK == retState )
        {
            if( ( RCC_FUNCTION_ACTIVE == clockConfig->CSS_Enable    ) &&
                ( RCC_HSE_TYPE_NONE   != clockConfig->HSE_ClockType )    )
            {
                Rcc_Set_RegBit( RCC_REG_CR, RCC_CR_CSSON );
            }
            else
            {
                /* CSS can be disabled only by reset or HSE de-activation */
            }
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Config( pllId, &clockConfig->Pll_Config[ pllId ] );
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );
        }

        if( ( RCC_REQUEST_OK == retState                                             ) &&
            ( Rcc_Get_ScaleMaxFreq( clockConfig->VoltageScaling ) < expectedSysClk )    )
        {
            /* Required system clock exceeds the voltage range */
            retState = RCC_REQUEST_ERROR;
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB1Divider( clockConfig->APB1_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_APB2Divider( clockConfig->APB2_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* AHB divider and system clock source (with transition state) */
            retState = Rcc_Set_SysClkTransition( clockConfig, expectedSysClk );
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Flash latency of the new clock (lowered from transition value) */
            retState = Rcc_Set_FlashLatency( clockConfig );
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* SysTick and CMSIS SystemCoreClock depend on processor clock (HCLK) */
            rcc_FreqHz_t hclkFreq = 0u;

            retState = Rcc_ClkBus_Get_AHBClk( &hclkFreq );

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Set_SysTickInterval( clockConfig->SysTickInterval );
            }
            else
            {
                /* Processor clock is not available */
            }

            if( RCC_REQUEST_OK == retState )
            {
                SystemCoreClock = hclkFreq;
            }
            else
            {
                /* Error during initialization process */
            }
        }

        for( rcc_ClkOut_Id_t clkOutId = RCC_CLK_OUT_MCO1; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                /* Clock output configuration (return states are not needed to be checked) */
                (void)Rcc_Set_ClkOutSource( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockSource );

                (void)Rcc_Set_ClkOutDivider( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockDivider );
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

    return;
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
 * Default configuration is valid for all MCUs of STM32G4 family:
 * - PLL clocked by HSI16 (16 MHz / 4 * 85 = VCO 340 MHz), SYSCLK 170 MHz
 *   (R = 2), ADC kernel clock 56.67 MHz (P = 6), output Q not used
 * - voltage range 1 boost mode, AHB / APB1 / APB2 170 MHz
 * - HSE and clock outputs are not used
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
        clockConfig->HSE_ClockType     = RCC_HSE_TYPE_NONE;
        clockConfig->HSE_Frequency_Hz  = RCC_DEFAULT_HSE_FREQ_HZ;
        clockConfig->SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
        clockConfig->CSS_Enable        = RCC_FUNCTION_INACTIVE;
        clockConfig->AHB_Divider       = RCC_AHB_DIVIDER_1;
        clockConfig->APB1_Divider      = RCC_APB1_DIVIDER_1;
        clockConfig->APB2_Divider      = RCC_APB2_DIVIDER_1;
        clockConfig->FlashLatency      = RCC_FLASH_LATENCY_0_WS;
        clockConfig->VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_0;
        clockConfig->SysTickInterval   = RCC_DEFAULT_SYSTICK_INTERVAL_MS;

        for( rcc_ClkOut_Id_t clkOutId = RCC_CLK_OUT_MCO1; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
        {
            clockConfig->McoConfig[ clkOutId ].ClockSource  = RCC_CLK_SOURCE_NONE;
            clockConfig->McoConfig[ clkOutId ].ClockDivider = RCC_DEFAULT_CLK_OUT_DIV;
        }

        for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
        {
            clockConfig->Pll_Config[ pllId ].Pll_Source   = RCC_PLL_SRC_HSI;
            clockConfig->Pll_Config[ pllId ].M_Divider    = RCC_DEFAULT_PLL_M_DIV;
            clockConfig->Pll_Config[ pllId ].N_Multiplier = RCC_DEFAULT_PLL_N_MULT;
            clockConfig->Pll_Config[ pllId ].P_Divider    = RCC_DEFAULT_PLL_P_DIV;
            clockConfig->Pll_Config[ pllId ].Q_Divider    = RCC_DEFAULT_PLL_OUT_UNUSED;
            clockConfig->Pll_Config[ pllId ].R_Divider    = RCC_DEFAULT_PLL_R_DIV;
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
 * (HSI16, HSI48, LSI) which is not running, the oscillator is started before
 * the clock MUX is switched. External sources (HSE, LSE) and PLL outputs are
 * not started.
 *
 * \warning Some peripherals have connected clock source. (e.g. RNG and USB use
 *          the common 48 MHz clock)
 *
 * \param periphId (in): ID of required peripheral to activate clock source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also when the oscillator can not be started,
 *         peripheral clock is not changed in that case).
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
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     stateRegId = rcc_RegBankConfig[ regBankId ].EnableRegId;
        const uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            /* Activate peripheral by setting "1" to corresponding register */
            Rcc_Set_RegBit( stateRegId, stateMask );

            retState = Rcc_Wait_RegVal( stateRegId, stateMask, stateMask );
        }
        else
        {
            /* Peripheral has no clock enable - always clocked */
        }
    }
    else
    {
        /* Clock multiplexer could not be configured */
    }

    return ( retState );
}


/**
 * \brief Peripheral clock disable request
 *
 * User can request de-activation of clock for required peripheral. If required
 * peripheral is correctly de-activated, and required peripheral ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * The kernel clock multiplexer of the peripheral is set back to its default
 * value, so the peripheral can be activated again with another clock source.
 * RTC clock selection (RTCSEL) is kept - it can be changed only by backup
 * domain reset. A multiplexer shared with another enabled peripheral block
 * (CLK48SEL of RNG and USB) is kept as well.
 *
 * \param periphId (in): ID of required peripheral to de-activate clock source
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PeriphInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     stateRegId = rcc_RegBankConfig[ regBankId ].EnableRegId;
        const uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;

        if( RCC_UNSUPPORTED_FUNCTION != stateMask )
        {
            Rcc_Reset_RegBit( stateRegId, stateMask );

            retState = Rcc_Wait_RegVal( stateRegId, stateMask, 0u );
        }
        else
        {
            retState = RCC_REQUEST_OK;
        }

        const rcc_ClkMuxId_t clkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;

        if( ( RCC_REQUEST_OK        == retState                         ) &&
            ( RCC_CLK_MUX_LIST_CNT   > clkMuxId                         ) &&
            ( RCC_BLOCK_RTC         != blockId                          ) &&
            ( RCC_FUNCTION_INACTIVE == Rcc_Get_ClkMuxShared( periphId ) )    )
        {
            /* Kernel clock multiplexer is released for the next activation */
            retState = Rcc_ClkMux_Set_ClkInactive( clkMuxId );
        }
        else
        {
            /* Clock still enabled, no multiplexer, write-once RTC clock selection
               or multiplexer shared with another enabled peripheral block */
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

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     stateRegId = rcc_RegBankConfig[ regBankId ].EnableRegId;
        const uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != stateMask                              ) &&
            ( 0u                       == Rcc_Get_RegBit( stateRegId, stateMask ) )    )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            /* Peripheral is active or has no clock enable (always clocked) */
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
 * \param periphId   [in]: ID of required peripheral
 * \param periphClk [out]: Value of frequency for selected peripheral in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for external kernel clock I2S_CKIN or
 *         disabled PLL output).
 */
rcc_RequestState_t Rcc_Get_PeriphClk( rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != periphClk )    )
    {
        const rcc_ClkSrcId_t periphClkSrcId = rcc_ConfigStruct[ periphId ].ClkSrcId;

        if( RCC_CLK_SRC_CNT > periphClkSrcId )
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
 * USART1 any of \c RCC_PERIPH_USART1_PCLK2, \c RCC_PERIPH_USART1_SYSCLK,
 * \c RCC_PERIPH_USART1_HSI or \c RCC_PERIPH_USART1_LSE can be used and
 * correct enumeration will be returned.
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
                for( rcc_PeriphId_t periphSearchId = (rcc_PeriphId_t)0u; ( RCC_PERIPH_ID_CNT > periphSearchId ) && ( RCC_PERIPH_ID_CNT == foundPeriphId ); periphSearchId ++ )
                {
                    if( ( blockId  == rcc_ConfigStruct[ periphSearchId ].BlockId  ) &&
                        ( clkMuxId == rcc_ConfigStruct[ periphSearchId ].ClkMuxId )    )
                    {
                        foundPeriphId = periphSearchId;
                    }
                }

                if( RCC_PERIPH_ID_CNT > foundPeriphId )
                {
                    *periphClkSrc = foundPeriphId;
                }
                else
                {
                    /* Selected source has no peripheral ID (e.g. RTC without clock) */
                    returnState = RCC_REQUEST_ERROR;
                }
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
 * \note ADC1 / ADC2 and ADC3 / ADC4 / ADC5 share one reset control per group.
 *
 * \param periphId (in): ID of required peripheral to activate reset
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the peripheral has no reset control).
 */
rcc_RequestState_t Rcc_Set_ResetActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     resetRegId = rcc_RegBankConfig[ regBankId ].ResetRegId;
        const uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != resetMask  ) &&
            ( RCC_REG_CNT              >  resetRegId )    )
        {
            Rcc_Set_RegBit( resetRegId, resetMask );

            retState = Rcc_Wait_RegVal( resetRegId, resetMask, resetMask );
        }
        else
        {
            /* Peripheral has no reset control */
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
 * User can request de-activation of reset for required peripheral. If required
 * peripheral is correctly switched from reset state, and required peripheral ID
 * is correct, returned state is "OK". Otherwise returns error.
 *
 * \param periphId (in): ID of required peripheral to deactivate reset
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ResetInactive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     resetRegId = rcc_RegBankConfig[ regBankId ].ResetRegId;
        const uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != resetMask  ) &&
            ( RCC_REG_CNT              >  resetRegId )    )
        {
            Rcc_Reset_RegBit( resetRegId, resetMask );

            retState = Rcc_Wait_RegVal( resetRegId, resetMask, 0u );
        }
        else
        {
            /* Peripheral has no reset control - never in reset */
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

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     resetRegId = rcc_RegBankConfig[ regBankId ].ResetRegId;
        const uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != resetMask                              ) &&
            ( 0u                       != Rcc_Get_RegBit( resetRegId, resetMask ) )    )
        {
            *funcState = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            /* Peripheral is not in reset (or has no reset control) */
            *funcState = RCC_FUNCTION_INACTIVE;
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

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     sleepRegId = rcc_RegBankConfig[ regBankId ].SleepRegId;
        const uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != sleepMask  ) &&
            ( RCC_REG_CNT              >  sleepRegId )    )
        {
            Rcc_Set_RegBit( sleepRegId, sleepMask );

            retState = Rcc_Wait_RegVal( sleepRegId, sleepMask, sleepMask );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     sleepRegId = rcc_RegBankConfig[ regBankId ].SleepRegId;
        const uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != sleepMask  ) &&
            ( RCC_REG_CNT              >  sleepRegId )    )
        {
            Rcc_Reset_RegBit( sleepRegId, sleepMask );

            retState = Rcc_Wait_RegVal( sleepRegId, sleepMask, 0u );
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

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        const rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        const rcc_RegBankId_t regBankId  = rcc_PeriphBlockConfig[ blockId ].RegBankId;
        const rcc_RegId_t     sleepRegId = rcc_RegBankConfig[ regBankId ].SleepRegId;
        const uint32_t        sleepMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;

        if( ( RCC_UNSUPPORTED_FUNCTION != sleepMask                              ) &&
            ( 0u                       == Rcc_Get_RegBit( sleepRegId, sleepMask ) )    )
        {
            *funcState = RCC_FUNCTION_INACTIVE;
        }
        else
        {
            /* Active in sleep mode or no sleep mode control */
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
 * User can set required clock bus divider. If required clock bus ID and
 * divider are correct, returned state is "OK". Otherwise returns error.
 *
 * \note All AHB buses share one divider (HCLK), both APB1 groups share APB1
 *       divider (PCLK1).
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

    if( ( RCC_CLK_BUS_APB1_1 == clkBusId ) ||
        ( RCC_CLK_BUS_APB1_2 == clkBusId )    )
    {
        retState = Rcc_ClkBus_Set_APB1Divider( (rcc_APB1_Div_t)clkBusDivider );
    }
    else if( RCC_CLK_BUS_APB2 == clkBusId )
    {
        retState = Rcc_ClkBus_Set_APB2Divider( (rcc_APB2_Div_t)clkBusDivider );
    }
    else if( RCC_CLK_BUS_CNT > clkBusId )
    {
        /* AHB buses */
        retState = Rcc_ClkBus_Set_AHBDivider( (rcc_AHB_Div_t)clkBusDivider );
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

    if( RCC_NULL_PTR == clkBusDivider )
    {
        retState = RCC_REQUEST_ERROR;
    }
    else if( ( RCC_CLK_BUS_APB1_1 == clkBusId ) ||
             ( RCC_CLK_BUS_APB1_2 == clkBusId )    )
    {
        rcc_APB1_Div_t divider = RCC_APB1_DIVIDER_1;

        retState       = Rcc_ClkBus_Get_APB1Divider( &divider );
        *clkBusDivider = (rcc_ClkBusDiv_t)divider;
    }
    else if( RCC_CLK_BUS_APB2 == clkBusId )
    {
        rcc_APB2_Div_t divider = RCC_APB2_DIVIDER_1;

        retState       = Rcc_ClkBus_Get_APB2Divider( &divider );
        *clkBusDivider = (rcc_ClkBusDiv_t)divider;
    }
    else if( RCC_CLK_BUS_CNT > clkBusId )
    {
        /* AHB buses */
        rcc_AHB_Div_t divider = RCC_AHB_DIVIDER_1;

        retState       = Rcc_ClkBus_Get_AHBDivider( &divider );
        *clkBusDivider = (rcc_ClkBusDiv_t)divider;
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

    if( ( RCC_CLK_BUS_APB1_1 == clkBusId ) ||
        ( RCC_CLK_BUS_APB1_2 == clkBusId )    )
    {
        retState = Rcc_ClkBus_Get_APB1Clk( clkBusFreq );
    }
    else if( RCC_CLK_BUS_APB2 == clkBusId )
    {
        retState = Rcc_ClkBus_Get_APB2Clk( clkBusFreq );
    }
    else if( RCC_CLK_BUS_CNT > clkBusId )
    {
        /* AHB buses */
        retState = Rcc_ClkBus_Get_AHBClk( clkBusFreq );
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*------------------ Power range and latency configuration -------------------*/

/**
 * \brief Function used for power range (dynamic voltage scaling) configuration
 *
 * Voltage range has to be configured according to expected system clock
 * frequency (range 1 boost mode - 170 MHz, range 1 normal mode - 150 MHz,
 * range 2 - 26 MHz).
 *
 * \note The range can be changed only while the actual system clock does not
 *       exceed the maximal frequency of the required range. Flash latency has
 *       to be configured for the actual clock in the required range by user.
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if the actual system clock is too high).
 */
rcc_RequestState_t Rcc_Set_PwrRange( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clockConfig )
    {
        rcc_FreqHz_t   sysClk  = 0u;
        const uint32_t vosVal  = ( RCC_PWR_VOLTAGE_SCALE_2 == clockConfig->VoltageScaling ) ? RCC_PWR_VOS_RANGE_2 :
                                                                                               RCC_PWR_VOS_RANGE_1;
        const uint32_t r1Mode  = ( RCC_PWR_VOLTAGE_SCALE_1 == clockConfig->VoltageScaling ) ? PWR_CR5_R1MODE : 0u;

        /* PWR registers are accessible with enabled interface clock only */
        retState = Rcc_Set_PeriphActive( RCC_PERIPH_PWR );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Get_SysClk( &sysClk );
        }

        if( RCC_REQUEST_OK != retState )
        {
            /* PWR interface or system clock is not available */
        }
        else if( ( RCC_PWR_VOLTAGE_SCALE_2                              < clockConfig->VoltageScaling ) ||
                 ( Rcc_Get_ScaleMaxFreq( clockConfig->VoltageScaling ) < sysClk                      )    )
        {
            /* Unknown range or actual system clock exceeds the required range */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            if( RCC_PWR_VOS_RANGE_1 == vosVal )
            {
                /* Range 1 first, boost mode (R1MODE) is applied in range 1 */
                Rcc_Set_RegVal( RCC_REG_PWR_CR1, PWR_CR1_VOS, vosVal );

                retState = Rcc_Wait_RegVal( RCC_REG_PWR_SR2, PWR_SR2_VOSF, 0u );

                if( RCC_REQUEST_OK == retState )
                {
                    Rcc_Set_RegVal( RCC_REG_PWR_CR5, PWR_CR5_R1MODE, r1Mode );

                    retState = Rcc_Wait_RegVal( RCC_REG_PWR_CR5, PWR_CR5_R1MODE, r1Mode );
                }
            }
            else
            {
                /* Range 2 - boost mode is not used (normal mode selected first) */
                Rcc_Set_RegVal( RCC_REG_PWR_CR5, PWR_CR5_R1MODE, PWR_CR5_R1MODE );

                Rcc_Set_RegVal( RCC_REG_PWR_CR1, PWR_CR1_VOS, vosVal );

                retState = Rcc_Wait_RegVal( RCC_REG_PWR_SR2, PWR_SR2_VOSF, 0u );
            }

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Wait_RegVal( RCC_REG_PWR_CR1, PWR_CR1_VOS, vosVal );
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
 * \brief Function used to flash wait states ("delay") configuration.
 *
 * The flash wait state is dependent on expected processor clock (HCLK) and
 * voltage range. Required number of wait states is calculated, configured
 * \ref rcc_ConfigStruct_t::FlashLatency is used as minimal number of wait
 * states.
 *
 * \note Voltage range has to be configured first (\ref Rcc_Set_PwrRange).
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if expected system clock exceeds maximal
 *         frequency of the voltage range).
 */
rcc_RequestState_t Rcc_Set_FlashLatency( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState       = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       expectedSysClk = 0u;

    if( RCC_NULL_PTR != clockConfig )
    {
        retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );

        if( ( RCC_REQUEST_OK                                      == retState       ) &&
            ( 0u                                                   < expectedSysClk ) &&
            ( Rcc_Get_ScaleMaxFreq( clockConfig->VoltageScaling ) >= expectedSysClk )    )
        {
            const uint32_t ahbPresc = ( (uint32_t)clockConfig->AHB_Divider & RCC_CFGR_HPRE ) >> RCC_CFGR_HPRE_Pos;
            const uint32_t hclkFreq = expectedSysClk >> AHBPrescTable[ ahbPresc ];
            uint32_t       latency  = Rcc_Get_FlashLatency( clockConfig->VoltageScaling, hclkFreq ) << FLASH_ACR_LATENCY_Pos;

            if( (uint32_t)clockConfig->FlashLatency > latency )
            {
                /* Higher number of wait states required by user */
                latency = (uint32_t)clockConfig->FlashLatency;
            }
            else
            {
                /* Calculated latency is used */
            }

            Rcc_Set_RegVal( RCC_REG_FLASH_ACR, FLASH_ACR_LATENCY, latency );

            retState = Rcc_Wait_RegVal( RCC_REG_FLASH_ACR, FLASH_ACR_LATENCY, latency );
        }
        else
        {
            /* Expected frequency is not available or out of range */
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
 * \brief Function used to flash prefetch buffer activation.
 *
 * Flash prefetch buffer is used to increase performance of FLASH memory access.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchActive( void )
{
    Rcc_Set_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN );

    return ( Rcc_Wait_RegVal( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN, FLASH_ACR_PRFTEN ) );
}


/**
 * \brief Function used to flash prefetch buffer de-activation.
 *
 * Flash prefetch buffer is used to increase performance of FLASH memory access.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashPrefetchInactive( void )
{
    Rcc_Reset_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN );

    return ( Rcc_Wait_RegVal( RCC_REG_FLASH_ACR, FLASH_ACR_PRFTEN, 0u ) );
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

/*----------------------------- Oscillators configuration --------------------*/

/**
 * \brief Activates oscillator.
 *
 * \param oscId [in]: Oscillator identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscActive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( oscId )
    {
        case RCC_OSC_HSI:   retState = Rcc_ClkSrc_Set_HsiActive();   break;
        case RCC_OSC_HSI48: retState = Rcc_ClkSrc_Set_Hsi48Active(); break;
        case RCC_OSC_LSI:   retState = Rcc_ClkSrc_Set_LsiActive();   break;
        case RCC_OSC_LSE:   retState = Rcc_ClkSrc_Set_LseActive();   break;
        default:            retState = RCC_REQUEST_ERROR;            break;
    }

    return ( retState );
}


/**
 * \brief Deactivates oscillator.
 *
 * \warning Oscillator used as system clock or PLL source must not be deactivated.
 *
 * \param oscId [in]: Oscillator identification
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscInactive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( oscId )
    {
        case RCC_OSC_HSI:   retState = Rcc_ClkSrc_Set_HsiInactive();   break;
        case RCC_OSC_HSI48: retState = Rcc_ClkSrc_Set_Hsi48Inactive(); break;
        case RCC_OSC_LSI:   retState = Rcc_ClkSrc_Set_LsiInactive();   break;
        case RCC_OSC_LSE:   retState = Rcc_ClkSrc_Set_LseInactive();   break;
        default:            retState = RCC_REQUEST_ERROR;              break;
    }

    return ( retState );
}


/**
 * \brief Reads activation state of oscillator.
 *
 * \param oscId     [in]: Oscillator identification
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
        case RCC_OSC_HSI:   reqState = Rcc_ClkSrc_Get_HsiState( retState );   break;
        case RCC_OSC_HSI48: reqState = Rcc_ClkSrc_Get_Hsi48State( retState ); break;
        case RCC_OSC_LSI:   reqState = Rcc_ClkSrc_Get_LsiState( retState );   break;
        case RCC_OSC_LSE:   reqState = Rcc_ClkSrc_Get_LseState( retState );   break;
        default:            reqState = RCC_REQUEST_ERROR;                     break;
    }

    return ( reqState );
}


/**
 * \brief Configures divider of oscillator output.
 *
 * Oscillators of STM32G4 family have no output divider - divider 1 is accepted only.
 *
 * \param oscId  [in]: Oscillator identification
 * \param oscDiv [in]: Divider value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscDiv( rcc_OscId_t oscId, rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_OSC_CNT > oscId  ) &&
        ( 1u         == oscDiv )    )
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
 * \brief Reads divider of oscillator output.
 *
 * \param oscId   [in]: Oscillator identification
 * \param oscDiv [out]: Divider value (always 1)
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
        *oscDiv = 1u;

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
 * \warning RTC clock source can be selected only once after backup domain reset.
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
 * \param clkSource [out]: Pointer to clock output source
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
 * \param clkDivider [in]: Clock output divider (MCO 1, 2, 4, 8, 16; LSCO 1)
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
 * \param outId      [in] : Clock output identification
 * \param clkDivider [out]: Pointer to clock output divider
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
 *        afterwards (write of 0 has no effect when the bit is cleared by
 *        hardware). The cleared flags are verified by read-back.
 *
 * \return Function processing state. Returns \ref RCC_REQUEST_OK if request
 *         was processed without problems. Otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_Set_ResetSourceClear( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    /* Request removal of all reset source flags */
    Rcc_Set_RegBit( RCC_REG_CSR, RCC_CSR_RMVF );

    retState = Rcc_Wait_RegVal( RCC_REG_CSR, RCC_CSR_RESET_SRC_MASK, RCC_CSR_BITS_CLEARED );

    /* Release remove flag, so next reset sources are latched */
    Rcc_Reset_RegBit( RCC_REG_CSR, RCC_CSR_RMVF );

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_Wait_RegVal( RCC_REG_CSR, RCC_CSR_RMVF, RCC_CSR_BITS_CLEARED );
    }
    else
    {
        /* Flags were not cleared, error is returned */
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Starts oscillator used as peripheral kernel clock source.
 *
 * Clock sources HSI16, HSI48 and LSI are mapped to internal oscillators,
 * oscillator which is not running is activated. Divided HSE clock of RTC
 * requires configured HSE frequency. Other clock sources (buses, PLL outputs,
 * HSE, LSE) are not handled.
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
        case RCC_CLK_SRC_HSICLK:    oscId = RCC_OSC_HSI;                             break;
        case RCC_CLK_SRC_HSI48CLK:  oscId = RCC_OSC_HSI48;                           break;
        case RCC_CLK_SRC_LSICLK:    oscId = RCC_OSC_LSI;                             break;
        case RCC_CLK_SRC_HSERTCCLK: retState = Rcc_ClkSrc_Set_HseRtcActive();        break;
        default:                    oscId = RCC_OSC_CNT;                             break;
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
 * \brief Configures AHB divider and switches the system clock source.
 *
 * When the PLL is selected and the expected system clock is higher than
 * 80 MHz, the AHB prescaler transition state is used (RM0440): HCLK is divided
 * by 2 during the switch, the configured AHB divider is applied after at
 * least 1 us.
 *
 * \param clockConfig [in]: Configuration structure
 * \param sysClk      [in]: Expected system clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
static rcc_RequestState_t Rcc_Set_SysClkTransition( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t sysClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    if( ( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource ) &&
        ( RCC_SYSCLK_TRANSITION_HZ     < sysClk                         ) &&
        ( RCC_AHB_DIVIDER_1           == clockConfig->AHB_Divider       )    )
    {
        retState = Rcc_ClkBus_Set_AHBDivider( RCC_AHB_DIVIDER_2 );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
        }

        /* Transition state lasts at least 1 us */
        for( volatile uint32_t iterationCnt = 0u; RCC_TRANSITION_WAIT_RAW > iterationCnt; iterationCnt ++ )
        {
            /* Waiting */
        }
    }
    else
    {
        retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
    }

    if( RCC_REQUEST_OK == retState )
    {
        retState = Rcc_ClkBus_Set_AHBDivider( clockConfig->AHB_Divider );
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
static rcc_RequestState_t Rcc_Wait_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
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


/**
 * \brief Calculates number of flash wait states for processor clock (HCLK).
 *
 * \param voltageScale [in]: Voltage range
 * \param hclkFreq     [in]: Processor clock (HCLK) frequency in Hz
 *
 * \return Number of wait states
 */
static uint32_t Rcc_Get_FlashLatency( rcc_PwrVoltageScale_t voltageScale, rcc_FreqHz_t hclkFreq )
{
    const rcc_FreqHz_t * thresholds = rcc_FlashLatencyScale0;
    uint32_t             thrCnt     = sizeof( rcc_FlashLatencyScale0 ) / sizeof( rcc_FreqHz_t );
    uint32_t             latency    = 0u;

    if( RCC_PWR_VOLTAGE_SCALE_1 == voltageScale )
    {
        thresholds = rcc_FlashLatencyScale1;
        thrCnt     = sizeof( rcc_FlashLatencyScale1 ) / sizeof( rcc_FreqHz_t );
    }
    else if( RCC_PWR_VOLTAGE_SCALE_2 == voltageScale )
    {
        thresholds = rcc_FlashLatencyScale2;
        thrCnt     = sizeof( rcc_FlashLatencyScale2 ) / sizeof( rcc_FreqHz_t );
    }
    else
    {
        /* Range 1 boost mode thresholds */
    }

    /* Thresholds are ascending - every exceeded threshold adds one wait state */
    for( uint32_t thrIdx = 0u; thrCnt > thrIdx; thrIdx++ )
    {
        if( hclkFreq > thresholds[ thrIdx ] )
        {
            latency++;
        }
        else
        {
            break;
        }
    }

    return ( latency );
}


/**
 * \brief Returns maximal system clock frequency of the voltage range.
 *
 * \param voltageScale [in]: Voltage range
 *
 * \return Maximal system clock in Hz (0 for unknown range)
 */
static rcc_FreqHz_t Rcc_Get_ScaleMaxFreq( rcc_PwrVoltageScale_t voltageScale )
{
    rcc_FreqHz_t maxFreq = 0u;

    switch( voltageScale )
    {
        case RCC_PWR_VOLTAGE_SCALE_0: maxFreq = RCC_MAX_FREQ_SCALE_0_HZ; break;
        case RCC_PWR_VOLTAGE_SCALE_1: maxFreq = RCC_MAX_FREQ_SCALE_1_HZ; break;
        case RCC_PWR_VOLTAGE_SCALE_2: maxFreq = RCC_MAX_FREQ_SCALE_2_HZ; break;
        default:                      maxFreq = 0u;                      break;
    }

    return ( maxFreq );
}


/**
 * \brief Checks if the kernel clock multiplexer of the peripheral is shared with another
 *        enabled peripheral block (e.g. CLK48SEL of RNG and USB).
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
    rcc_FunctionState_t   sharedState = RCC_FUNCTION_ACTIVE;
    rcc_ClkMuxId_t        ownSelected = RCC_CLK_MUX_LIST_CNT;
    const rcc_BlockList_t ownBlockId  = rcc_ConfigStruct[ periphId ].BlockId;

    if( RCC_REQUEST_OK == Rcc_ClkMux_Get_ClkSrc( rcc_ConfigStruct[ periphId ].ClkMuxId, &ownSelected ) )
    {
        sharedState = RCC_FUNCTION_INACTIVE;

        for( uint32_t rowIdx = 0u; RCC_PERIPH_ID_CNT > rowIdx; rowIdx ++ )
        {
            const rcc_PeriphConfigStruct_t * const row           = &rcc_ConfigStruct[ rowIdx ];
            rcc_ClkMuxId_t                         otherSelected = RCC_CLK_MUX_LIST_CNT;
            rcc_FunctionState_t                    otherState    = RCC_FUNCTION_INACTIVE;

            if( ( ownBlockId           != row->BlockId                                                 ) &&
                ( RCC_CLK_MUX_LIST_CNT  > row->ClkMuxId                                                ) &&
                ( RCC_REQUEST_OK       == Rcc_ClkMux_Get_ClkSrc( row->ClkMuxId, &otherSelected )       ) &&
                ( ownSelected          == otherSelected                                                ) &&
                ( RCC_REQUEST_OK       == Rcc_Get_PeriphState( (rcc_PeriphId_t)rowIdx, &otherState )   ) &&
                ( RCC_FUNCTION_ACTIVE  == otherState                                                   )    )
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
 * \brief Function used to wrap PLL clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_Main_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutP( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_Main_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutQ( RCC_PLL_1, clkFreq ) );
}


/**
 * \brief Function used to wrap PLL clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_Main_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutR( RCC_PLL_1, clkFreq ) );
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
static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const sysClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllSrcFreq = 0u;

    if( ( RCC_NULL_PTR != clockConfig ) &&
        ( RCC_NULL_PTR != sysClk      )    )
    {
        const rcc_PllConfigStruct_t * const pllConfig = &clockConfig->Pll_Config[ RCC_PLL_1 ];

        if( RCC_SYSTEM_CLOCK_SOURCE_HSI == clockConfig->SystemClockSource )
        {
            retState = Rcc_ClkSrc_Get_HsiClk( sysClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == clockConfig->SystemClockSource )
        {
            *sysClk  = clockConfig->HSE_Frequency_Hz;
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource )
        {
            /* PLL output R */
            if( RCC_PLL_SRC_HSI == pllConfig->Pll_Source )
            {
                retState = Rcc_ClkSrc_Get_HsiClk( &pllSrcFreq );
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
                ( 0u             != pllConfig->R_Divider  )    )
            {
                const uint64_t vcoFreq = ( (uint64_t)pllSrcFreq * pllConfig->N_Multiplier ) / pllConfig->M_Divider;

                *sysClk = (rcc_FreqHz_t)( vcoFreq / pllConfig->R_Divider );
            }
            else
            {
                /* Divider is not configured or PLL is not used */
                retState = RCC_REQUEST_ERROR;
            }
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
