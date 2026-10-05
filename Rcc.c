/**
 * \author Mr.Nobody
 * \file Rcc.c
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module common functionality
 *
 * \note  Exception of MCAL layering rule: PWR (voltage scaling, over-drive,
 *        backup domain access) and FLASH (latency, prefetch, caches) have no
 *        MCAL module. Their configuration is part of the clock configuration
 *        sequence, therefore RCC accesses their registers directly
 *        (\ref Rcc_Init, \ref Rcc_Set_PwrRange, \ref Rcc_Set_FlashLatency,
 *        \ref Rcc_Set_FlashPrefetchActive). New accesses shall be moved into a
 *        dedicated MCAL module once it exists.
 *
 * \note  STM32F4 family - kernel clocks of I2S, SAI, LTDC, DSI, CEC and SPDIFRX
 *        are not handled by the module (\ref Rcc_Get_PeriphClk returns error),
 *        48 MHz clock (CK48) and SDIO clock multiplexers are kept in reset
 *        configuration - their actual selection is used for frequency
 *        calculation only.
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
#define RCC_CSR_RESET_SRC_MASK                  ( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_PORRSTF | RCC_CSR_SFTRSTF | \
                                                  RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF )

/** Value of CSR register bits in cleared state */
#define RCC_CSR_BITS_CLEARED                    ( 0u )

/** Default HSE frequency used by \ref Rcc_Get_DefaultConfig (e.g. ST-LINK MCO of Nucleo boards) */
#define RCC_DEFAULT_HSE_FREQ_HZ                 ( 8000000u )

/** Default PLL input divider - HSI 16 MHz / 16 gives 1 MHz PLL input frequency */
#define RCC_DEFAULT_PLL_M_DIV                   ( 16u )

/** Default PLL multiplier - VCO 336 MHz (valid for all STM32F4 MCUs) */
#define RCC_DEFAULT_PLL_N_MULT                  ( 336u )

/** Default PLL output P divider - system clock 84 MHz (supported by all STM32F4 MCUs) */
#define RCC_DEFAULT_PLL_P_DIV                   ( 4u )

/** Default PLL output Q divider - 48 MHz clock (USB OTG FS, SDIO, RNG) */
#define RCC_DEFAULT_PLL_Q_DIV                   ( 7u )

/** PLL output divider value - output is not configured */
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

#if defined(PWR_CR_ODEN)
/** Maximal system clock in voltage scale 1 without over-drive mode */
#define RCC_PWR_OD_THRESHOLD_SCALE1_HZ          ( 168000000u )

/** Maximal system clock in voltage scale 2 without over-drive mode */
#define RCC_PWR_OD_THRESHOLD_SCALE2_HZ          ( 144000000u )
#endif /* PWR_CR_ODEN */

/* ============================== TYPEDEFS ================================== */

/** \brief Register banks - groups of enable / sleep / reset registers */
typedef enum
{
    RCC_REG_BANK_AHB1 = 0u, /**< AHB1 peripherals registers                   */
#if defined(RCC_AHB2_SUPPORT)
    RCC_REG_BANK_AHB2,      /**< AHB2 peripherals registers                   */
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    RCC_REG_BANK_AHB3,      /**< AHB3 peripherals registers                   */
#endif /* RCC_AHB3_SUPPORT */
    RCC_REG_BANK_APB1,      /**< APB1 peripherals registers                   */
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

static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t * const sysClk );
static rcc_RequestState_t Rcc_Set_ClkSrcOscActive( rcc_ClkSrcId_t clkSrcId );
static rcc_RequestState_t Rcc_Set_OverDrive( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t sysClk );
static rcc_RequestState_t Rcc_Wait_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal );
static uint32_t           Rcc_Get_FlashLatency( rcc_PwrVoltageScale_t voltageScale, rcc_FreqHz_t hclkFreq );

static rcc_RequestState_t Rcc_Pll_Get_Main_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_Main_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_Main_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_Pll48Clk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_SdioClk( rcc_FreqHz_t * const clkFreq );

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
  { .RegBankId = RCC_REG_BANK_AHB1, .EnableRegId = RCC_REG_AHB1ENR, .SleepRegId = RCC_REG_AHB1LPENR, .ResetRegId = RCC_REG_AHB1RSTR },
#if defined(RCC_AHB2_SUPPORT)
  { .RegBankId = RCC_REG_BANK_AHB2, .EnableRegId = RCC_REG_AHB2ENR, .SleepRegId = RCC_REG_AHB2LPENR, .ResetRegId = RCC_REG_AHB2RSTR },
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
  { .RegBankId = RCC_REG_BANK_AHB3, .EnableRegId = RCC_REG_AHB3ENR, .SleepRegId = RCC_REG_AHB3LPENR, .ResetRegId = RCC_REG_AHB3RSTR },
#endif /* RCC_AHB3_SUPPORT */
  { .RegBankId = RCC_REG_BANK_APB1, .EnableRegId = RCC_REG_APB1ENR, .SleepRegId = RCC_REG_APB1LPENR, .ResetRegId = RCC_REG_APB1RSTR },
  { .RegBankId = RCC_REG_BANK_APB2, .EnableRegId = RCC_REG_APB2ENR, .SleepRegId = RCC_REG_APB2LPENR, .ResetRegId = RCC_REG_APB2RSTR },
  { .RegBankId = RCC_REG_BANK_BDCR, .EnableRegId = RCC_REG_BDCR   , .SleepRegId = RCC_REG_CNT      , .ResetRegId = RCC_REG_CNT      },
};

_Static_assert( (sizeof(rcc_RegBankConfig) / sizeof(rcc_RegBankConfigStruct_t)) == RCC_REG_BANK_CNT, "Rcc: rcc_RegBankConfig has incorrect size." );


static const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK    , .ClkSrcCallback = Rcc_ClkBus_Get_SysClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLLPCLK   , .ClkSrcCallback = Rcc_Pll_Get_Main_PClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLLQCLK   , .ClkSrcCallback = Rcc_Pll_Get_Main_QClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLLRCLK   , .ClkSrcCallback = Rcc_Pll_Get_Main_RClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL48CLK  , .ClkSrcCallback = Rcc_Get_Pll48Clk          },
  { .PeriphClkSrcId = RCC_CLK_SRC_SDIOCLK   , .ClkSrcCallback = Rcc_Get_SdioClk           },
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK    , .ClkSrcCallback = Rcc_ClkBus_Get_AHBClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK   , .ClkSrcCallback = Rcc_ClkBus_Get_APB1Clk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK   , .ClkSrcCallback = Rcc_ClkBus_Get_APB2Clk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .ClkSrcCallback = Rcc_ClkBus_Get_APB1TimClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .ClkSrcCallback = Rcc_ClkBus_Get_APB2TimClk },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSICLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_HsiClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_HseClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSERTCCLK , .ClkSrcCallback = Rcc_ClkSrc_Get_HseRtcClk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_LsiClk     },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK    , .ClkSrcCallback = Rcc_ClkSrc_Get_LseClk     },
};

_Static_assert( (sizeof(rcc_PeriphClkSrcConfig) / sizeof(rcc_ClkSrcConfigStruct_t)) == RCC_CLK_SRC_CNT, "Rcc: rcc_PeriphClkSrcConfig has incorrect size." );


/** \brief Configuration array of MCU peripherals. */
static const rcc_PeriphConfigStruct_t   rcc_ConfigStruct[] =
{
  { .PeriphId = RCC_PERIPH_FLASH             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_FLASH      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_SYSCFG            , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SYSCFG     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_PWR               , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_PWR        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_SYSTICK           , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SYSTICK    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_IWDG              , .ClkSrcId = RCC_CLK_SRC_LSICLK    , .BlockId = RCC_BLOCK_IWDG       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },

  { .PeriphId = RCC_PERIPH_RTC_HSE_DIV       , .ClkSrcId = RCC_CLK_SRC_HSERTCCLK , .BlockId = RCC_BLOCK_RTC        , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV    },
  { .PeriphId = RCC_PERIPH_RTC_LSE           , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_RTC        , .ClkMuxId = RCC_CLK_MUX_RTC_LSE        },
  { .PeriphId = RCC_PERIPH_RTC_LSI           , .ClkSrcId = RCC_CLK_SRC_LSICLK    , .BlockId = RCC_BLOCK_RTC        , .ClkMuxId = RCC_CLK_MUX_RTC_LSI        },

#if defined(RCC_APB1ENR_RTCAPBEN)
  { .PeriphId = RCC_PERIPH_RTCAPB            , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_RTCAPB     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_RTCAPBEN */

#if defined(RCC_AHB1LPENR_SRAM1LPEN)
  { .PeriphId = RCC_PERIPH_SRAM1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SRAM1      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1LPENR_SRAM1LPEN */
#if defined(RCC_AHB1LPENR_SRAM2LPEN)
  { .PeriphId = RCC_PERIPH_SRAM2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SRAM2      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1LPENR_SRAM2LPEN */
#if defined(RCC_AHB1LPENR_SRAM3LPEN)
  { .PeriphId = RCC_PERIPH_SRAM3             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_SRAM3      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1LPENR_SRAM3LPEN */

#if defined(RCC_AHB1ENR_BKPSRAMEN)
  { .PeriphId = RCC_PERIPH_BKPSRAM           , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_BKPSRAM    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_BKPSRAMEN */
#if defined(RCC_AHB1ENR_CCMDATARAMEN)
  { .PeriphId = RCC_PERIPH_CCMDATARAM        , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_CCMDATARAM , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_CCMDATARAMEN */

  { .PeriphId = RCC_PERIPH_DMA1              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DMA1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_DMA2              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DMA2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_AHB1ENR_DMA2DEN)
  { .PeriphId = RCC_PERIPH_DMA2D             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DMA2D      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_DMA2DEN */

  { .PeriphId = RCC_PERIPH_WWDG              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_WWDG       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },

#if defined(RCC_APB2ENR_EXTITEN)
  { .PeriphId = RCC_PERIPH_EXTIT             , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_EXTIT      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_EXTITEN */

  { .PeriphId = RCC_PERIPH_GPIOA             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOA      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_GPIOB             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOB      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_GPIOC             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOC      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_AHB1ENR_GPIODEN)
  { .PeriphId = RCC_PERIPH_GPIOD             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOD      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIODEN */
#if defined(RCC_AHB1ENR_GPIOEEN)
  { .PeriphId = RCC_PERIPH_GPIOE             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOE      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIOEEN */
#if defined(RCC_AHB1ENR_GPIOFEN)
  { .PeriphId = RCC_PERIPH_GPIOF             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOF      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIOFEN */
#if defined(RCC_AHB1ENR_GPIOGEN)
  { .PeriphId = RCC_PERIPH_GPIOG             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOG      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIOGEN */
  { .PeriphId = RCC_PERIPH_GPIOH             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOH      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_AHB1ENR_GPIOIEN)
  { .PeriphId = RCC_PERIPH_GPIOI             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOI      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIOIEN */
#if defined(RCC_AHB1ENR_GPIOJEN)
  { .PeriphId = RCC_PERIPH_GPIOJ             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOJ      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIOJEN */
#if defined(RCC_AHB1ENR_GPIOKEN)
  { .PeriphId = RCC_PERIPH_GPIOK             , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_GPIOK      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_GPIOKEN */

  /*--------------------------------- Timers ---------------------------------*/

  { .PeriphId = RCC_PERIPH_TIM1              , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB1ENR_TIM2EN)
  { .PeriphId = RCC_PERIPH_TIM2              , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM2EN */
#if defined(RCC_APB1ENR_TIM3EN)
  { .PeriphId = RCC_PERIPH_TIM3              , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM3       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM3EN */
#if defined(RCC_APB1ENR_TIM4EN)
  { .PeriphId = RCC_PERIPH_TIM4              , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM4       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM4EN */
  { .PeriphId = RCC_PERIPH_TIM5              , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM5       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB1ENR_TIM6EN)
  { .PeriphId = RCC_PERIPH_TIM6              , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM6       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM6EN */
#if defined(RCC_APB1ENR_TIM7EN)
  { .PeriphId = RCC_PERIPH_TIM7              , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM7       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM7EN */
#if defined(RCC_APB2ENR_TIM8EN)
  { .PeriphId = RCC_PERIPH_TIM8              , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM8       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_TIM8EN */
  { .PeriphId = RCC_PERIPH_TIM9              , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM9       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB2ENR_TIM10EN)
  { .PeriphId = RCC_PERIPH_TIM10             , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM10      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_TIM10EN */
  { .PeriphId = RCC_PERIPH_TIM11             , .ClkSrcId = RCC_CLK_SRC_APB2TIMCLK, .BlockId = RCC_BLOCK_TIM11      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB1ENR_TIM12EN)
  { .PeriphId = RCC_PERIPH_TIM12             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM12      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM12EN */
#if defined(RCC_APB1ENR_TIM13EN)
  { .PeriphId = RCC_PERIPH_TIM13             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM13      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM13EN */
#if defined(RCC_APB1ENR_TIM14EN)
  { .PeriphId = RCC_PERIPH_TIM14             , .ClkSrcId = RCC_CLK_SRC_APB1TIMCLK, .BlockId = RCC_BLOCK_TIM14      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_TIM14EN */

#if defined(RCC_APB1ENR_LPTIM1EN)
  { .PeriphId = RCC_PERIPH_LPTIM1_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_LPTIM1     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1   },
  { .PeriphId = RCC_PERIPH_LPTIM1_HSI        , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_LPTIM1     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI     },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK    , .BlockId = RCC_BLOCK_LPTIM1     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI     },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK    , .BlockId = RCC_BLOCK_LPTIM1     , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE     },
#endif /* RCC_APB1ENR_LPTIM1EN */

  /*------------------------------ Connectivity ------------------------------*/

  { .PeriphId = RCC_PERIPH_SPI1              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SPI1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB1ENR_SPI2EN)
  { .PeriphId = RCC_PERIPH_SPI2              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_SPI2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_SPI2EN */
#if defined(RCC_APB1ENR_SPI3EN)
  { .PeriphId = RCC_PERIPH_SPI3              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_SPI3       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
  { .PeriphId = RCC_PERIPH_SPI4              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SPI4       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_APB2ENR_SPI5EN)
  { .PeriphId = RCC_PERIPH_SPI5              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SPI5       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_SPI5EN */
#if defined(RCC_APB2ENR_SPI6EN)
  { .PeriphId = RCC_PERIPH_SPI6              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_SPI6       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_SPI6EN */

  { .PeriphId = RCC_PERIPH_I2C1              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_I2C2              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB1ENR_I2C3EN)
  { .PeriphId = RCC_PERIPH_I2C3              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_I2C3       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_I2C3EN */

#if defined(RCC_APB1ENR_FMPI2C1EN)
  { .PeriphId = RCC_PERIPH_FMPI2C1_PCLK1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_FMPI2C1    , .ClkMuxId = RCC_CLK_MUX_FMPI2C1_PCLK1  },
  { .PeriphId = RCC_PERIPH_FMPI2C1_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK    , .BlockId = RCC_BLOCK_FMPI2C1    , .ClkMuxId = RCC_CLK_MUX_FMPI2C1_SYSCLK },
  { .PeriphId = RCC_PERIPH_FMPI2C1_HSI       , .ClkSrcId = RCC_CLK_SRC_HSICLK    , .BlockId = RCC_BLOCK_FMPI2C1    , .ClkMuxId = RCC_CLK_MUX_FMPI2C1_HSI    },
#endif /* RCC_APB1ENR_FMPI2C1EN */

  { .PeriphId = RCC_PERIPH_USART1            , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_USART1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_USART2            , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_USART2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB1ENR_USART3EN)
  { .PeriphId = RCC_PERIPH_USART3            , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_USART3     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_USART3EN */
#if defined(RCC_APB1ENR_UART4EN)
  { .PeriphId = RCC_PERIPH_UART4             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_UART4      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_UART4EN */
#if defined(RCC_APB1ENR_UART5EN)
  { .PeriphId = RCC_PERIPH_UART5             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_UART5      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_UART5EN */
#if defined(RCC_APB2ENR_USART6EN)
  { .PeriphId = RCC_PERIPH_USART6            , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_USART6     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_USART6EN */
#if defined(RCC_APB1ENR_UART7EN)
  { .PeriphId = RCC_PERIPH_UART7             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_UART7      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_UART7EN */
#if defined(RCC_APB1ENR_UART8EN)
  { .PeriphId = RCC_PERIPH_UART8             , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_UART8      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_UART8EN */
#if defined(RCC_APB2ENR_UART9EN)
  { .PeriphId = RCC_PERIPH_UART9             , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_UART9      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_UART9EN */
#if defined(RCC_APB2ENR_UART10EN)
  { .PeriphId = RCC_PERIPH_UART10            , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_UART10     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_UART10EN */

#if defined(RCC_APB1ENR_CAN1EN)
  { .PeriphId = RCC_PERIPH_CAN1              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_CAN1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_CAN1EN */
#if defined(RCC_APB1ENR_CAN2EN)
  { .PeriphId = RCC_PERIPH_CAN2              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_CAN2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_CAN2EN */
#if defined(RCC_APB1ENR_CAN3EN)
  { .PeriphId = RCC_PERIPH_CAN3              , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_CAN3       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_CAN3EN */

#if defined(RCC_APB2ENR_SDIOEN)
  { .PeriphId = RCC_PERIPH_SDIO              , .ClkSrcId = RCC_CLK_SRC_SDIOCLK   , .BlockId = RCC_BLOCK_SDIO       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_SDIOEN */

#if defined(RCC_AHB3ENR_FSMCEN)
  { .PeriphId = RCC_PERIPH_FSMC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_FSMC       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB3ENR_FSMCEN */
#if defined(RCC_AHB3ENR_FMCEN)
  { .PeriphId = RCC_PERIPH_FMC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_FMC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
  { .PeriphId = RCC_PERIPH_QSPI              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_QSPI       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB3ENR_QSPIEN */

#if defined(RCC_AHB2ENR_OTGFSEN)
  { .PeriphId = RCC_PERIPH_USB_OTG_FS        , .ClkSrcId = RCC_CLK_SRC_PLL48CLK  , .BlockId = RCC_BLOCK_USB_OTG_FS , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB2ENR_OTGFSEN */
#if defined(RCC_AHB1ENR_OTGHSEN)
  { .PeriphId = RCC_PERIPH_USB_OTG_HS        , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_USB_OTG_HS , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_USB_OTG_HS_ULPI   , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_USB_OTG_HS_ULPI, .ClkMuxId = RCC_CLK_MUX_LIST_CNT   },
#endif /* RCC_AHB1ENR_OTGHSEN */

#if defined(RCC_AHB1ENR_ETHMACEN)
  { .PeriphId = RCC_PERIPH_ETH               , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_ETH        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_ETH_TX            , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_ETH_TX     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_ETH_RX            , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_ETH_RX     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
  { .PeriphId = RCC_PERIPH_ETH_PTP           , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_ETH_PTP    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_ETHMACEN */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(RCC_AHB2ENR_DCMIEN)
  { .PeriphId = RCC_PERIPH_DCMI              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_DCMI       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB2ENR_DCMIEN */
#if defined(RCC_APB2ENR_LTDCEN)
  { .PeriphId = RCC_PERIPH_LTDC              , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_LTDC       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_LTDCEN */
#if defined(RCC_APB2ENR_DSIEN)
  { .PeriphId = RCC_PERIPH_DSI               , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_DSI        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_DSIEN */
#if defined(RCC_APB2ENR_SAI1EN)
  { .PeriphId = RCC_PERIPH_SAI1              , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_SAI1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_SAI1EN */
#if defined(RCC_APB2ENR_SAI2EN)
  { .PeriphId = RCC_PERIPH_SAI2              , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_SAI2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_SAI2EN */
#if defined(RCC_APB1ENR_CECEN)
  { .PeriphId = RCC_PERIPH_CEC               , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_CEC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_CECEN */
#if defined(RCC_APB1ENR_SPDIFRXEN)
  { .PeriphId = RCC_PERIPH_SPDIFRX           , .ClkSrcId = RCC_CLK_SRC_CNT       , .BlockId = RCC_BLOCK_SPDIFRX    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_SPDIFRXEN */
#if defined(RCC_APB2ENR_DFSDM1EN)
  { .PeriphId = RCC_PERIPH_DFSDM1            , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_DFSDM1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_DFSDM1EN */
#if defined(RCC_APB2ENR_DFSDM2EN)
  { .PeriphId = RCC_PERIPH_DFSDM2            , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_DFSDM2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_DFSDM2EN */

  /*--------------------------------- Analog ---------------------------------*/

  { .PeriphId = RCC_PERIPH_ADC1              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_ADC1       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#if defined(RCC_APB2ENR_ADC2EN)
  { .PeriphId = RCC_PERIPH_ADC2              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_ADC2       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_ADC2EN */
#if defined(RCC_APB2ENR_ADC3EN)
  { .PeriphId = RCC_PERIPH_ADC3              , .ClkSrcId = RCC_CLK_SRC_APB2CLK   , .BlockId = RCC_BLOCK_ADC3       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB2ENR_ADC3EN */
#if defined(RCC_APB1ENR_DACEN)
  { .PeriphId = RCC_PERIPH_DAC               , .ClkSrcId = RCC_CLK_SRC_APB1CLK   , .BlockId = RCC_BLOCK_DAC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_APB1ENR_DACEN */

  /*-------------------------------- Security --------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
  { .PeriphId = RCC_PERIPH_AES               , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_AES        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB2ENR_AESEN */
#if defined(RCC_AHB2ENR_CRYPEN)
  { .PeriphId = RCC_PERIPH_CRYP              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_CRYP       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB2ENR_CRYPEN */
#if defined(RCC_AHB2ENR_HASHEN)
  { .PeriphId = RCC_PERIPH_HASH              , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_HASH       , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB2ENR_HASHEN */
#if defined(RCC_AHB1ENR_RNGEN) || defined(RCC_AHB2ENR_RNGEN)
  { .PeriphId = RCC_PERIPH_RNG               , .ClkSrcId = RCC_CLK_SRC_PLL48CLK  , .BlockId = RCC_BLOCK_RNG        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
#endif /* RCC_AHB1ENR_RNGEN OR RCC_AHB2ENR_RNGEN */

  /*-------------------------------- Computing -------------------------------*/

  { .PeriphId = RCC_PERIPH_CRC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK    , .BlockId = RCC_BLOCK_CRC        , .ClkMuxId = RCC_CLK_MUX_LIST_CNT       },
};

_Static_assert( (sizeof(rcc_ConfigStruct) / sizeof(rcc_PeriphConfigStruct_t)) == RCC_PERIPH_ID_CNT, "Rcc: rcc_ConfigStruct has incorrect size." );


/** \brief Configuration registers of RCC peripheral blocks.
 *
 * This array is created to reduce size of configuration.
 */
static const rcc_BlockConfigStruct_t    rcc_PeriphBlockConfig[] =
{
  { .BlockId = RCC_BLOCK_FLASH      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_UNSUPPORTED_FUNCTION   , .LpCtrlMask = RCC_AHB1LPENR_FLITFLPEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_SYSCFG     , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SYSCFGEN       , .LpCtrlMask = RCC_APB2LPENR_SYSCFGLPEN      , .RstCtrlMask = RCC_APB2RSTR_SYSCFGRST     },
  { .BlockId = RCC_BLOCK_PWR        , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_PWREN          , .LpCtrlMask = RCC_APB1LPENR_PWRLPEN         , .RstCtrlMask = RCC_APB1RSTR_PWRRST        },
  { .BlockId = RCC_BLOCK_SYSTICK    , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_UNSUPPORTED_FUNCTION   , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_IWDG       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_UNSUPPORTED_FUNCTION   , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_RTC        , .RegBankId = RCC_REG_BANK_BDCR, .StateMask = RCC_BDCR_RTCEN             , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },

#if defined(RCC_APB1ENR_RTCAPBEN)
  { .BlockId = RCC_BLOCK_RTCAPB     , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_RTCAPBEN       , .LpCtrlMask = RCC_APB1LPENR_RTCAPBLPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_APB1ENR_RTCAPBEN */

#if defined(RCC_AHB1LPENR_SRAM1LPEN)
  { .BlockId = RCC_BLOCK_SRAM1      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_UNSUPPORTED_FUNCTION   , .LpCtrlMask = RCC_AHB1LPENR_SRAM1LPEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1LPENR_SRAM1LPEN */
#if defined(RCC_AHB1LPENR_SRAM2LPEN)
  { .BlockId = RCC_BLOCK_SRAM2      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_UNSUPPORTED_FUNCTION   , .LpCtrlMask = RCC_AHB1LPENR_SRAM2LPEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1LPENR_SRAM2LPEN */
#if defined(RCC_AHB1LPENR_SRAM3LPEN)
  { .BlockId = RCC_BLOCK_SRAM3      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_UNSUPPORTED_FUNCTION   , .LpCtrlMask = RCC_AHB1LPENR_SRAM3LPEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1LPENR_SRAM3LPEN */

#if defined(RCC_AHB1ENR_BKPSRAMEN)
  { .BlockId = RCC_BLOCK_BKPSRAM    , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_BKPSRAMEN      , .LpCtrlMask = RCC_AHB1LPENR_BKPSRAMLPEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1ENR_BKPSRAMEN */
#if defined(RCC_AHB1ENR_CCMDATARAMEN)
  { .BlockId = RCC_BLOCK_CCMDATARAM , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_CCMDATARAMEN   , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1ENR_CCMDATARAMEN */

  { .BlockId = RCC_BLOCK_DMA1       , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_DMA1EN         , .LpCtrlMask = RCC_AHB1LPENR_DMA1LPEN        , .RstCtrlMask = RCC_AHB1RSTR_DMA1RST       },
  { .BlockId = RCC_BLOCK_DMA2       , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_DMA2EN         , .LpCtrlMask = RCC_AHB1LPENR_DMA2LPEN        , .RstCtrlMask = RCC_AHB1RSTR_DMA2RST       },
#if defined(RCC_AHB1ENR_DMA2DEN)
  { .BlockId = RCC_BLOCK_DMA2D      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_DMA2DEN        , .LpCtrlMask = RCC_AHB1LPENR_DMA2DLPEN       , .RstCtrlMask = RCC_AHB1RSTR_DMA2DRST      },
#endif /* RCC_AHB1ENR_DMA2DEN */

  { .BlockId = RCC_BLOCK_WWDG       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_WWDGEN         , .LpCtrlMask = RCC_APB1LPENR_WWDGLPEN        , .RstCtrlMask = RCC_APB1RSTR_WWDGRST       },

#if defined(RCC_APB2ENR_EXTITEN)
  { .BlockId = RCC_BLOCK_EXTIT      , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_EXTITEN        , .LpCtrlMask = RCC_APB2LPENR_EXTITLPEN       , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_APB2ENR_EXTITEN */

  { .BlockId = RCC_BLOCK_GPIOA      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOAEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOALPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOARST      },
  { .BlockId = RCC_BLOCK_GPIOB      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOBEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOBLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOBRST      },
  { .BlockId = RCC_BLOCK_GPIOC      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOCEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOCLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOCRST      },
#if defined(RCC_AHB1ENR_GPIODEN)
  { .BlockId = RCC_BLOCK_GPIOD      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIODEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIODLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIODRST      },
#endif /* RCC_AHB1ENR_GPIODEN */
#if defined(RCC_AHB1ENR_GPIOEEN)
  { .BlockId = RCC_BLOCK_GPIOE      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOEEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOELPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOERST      },
#endif /* RCC_AHB1ENR_GPIOEEN */
#if defined(RCC_AHB1ENR_GPIOFEN)
  { .BlockId = RCC_BLOCK_GPIOF      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOFEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOFLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOFRST      },
#endif /* RCC_AHB1ENR_GPIOFEN */
#if defined(RCC_AHB1ENR_GPIOGEN)
  { .BlockId = RCC_BLOCK_GPIOG      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOGEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOGLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOGRST      },
#endif /* RCC_AHB1ENR_GPIOGEN */
  { .BlockId = RCC_BLOCK_GPIOH      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOHEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOHLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOHRST      },
#if defined(RCC_AHB1ENR_GPIOIEN)
  { .BlockId = RCC_BLOCK_GPIOI      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOIEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOILPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOIRST      },
#endif /* RCC_AHB1ENR_GPIOIEN */
#if defined(RCC_AHB1ENR_GPIOJEN)
  { .BlockId = RCC_BLOCK_GPIOJ      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOJEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOJLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOJRST      },
#endif /* RCC_AHB1ENR_GPIOJEN */
#if defined(RCC_AHB1ENR_GPIOKEN)
  { .BlockId = RCC_BLOCK_GPIOK      , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_GPIOKEN        , .LpCtrlMask = RCC_AHB1LPENR_GPIOKLPEN       , .RstCtrlMask = RCC_AHB1RSTR_GPIOKRST      },
#endif /* RCC_AHB1ENR_GPIOKEN */

  /*--------------------------------- Timers ---------------------------------*/

  { .BlockId = RCC_BLOCK_TIM1       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_TIM1EN         , .LpCtrlMask = RCC_APB2LPENR_TIM1LPEN        , .RstCtrlMask = RCC_APB2RSTR_TIM1RST       },
#if defined(RCC_APB1ENR_TIM2EN)
  { .BlockId = RCC_BLOCK_TIM2       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM2EN         , .LpCtrlMask = RCC_APB1LPENR_TIM2LPEN        , .RstCtrlMask = RCC_APB1RSTR_TIM2RST       },
#endif /* RCC_APB1ENR_TIM2EN */
#if defined(RCC_APB1ENR_TIM3EN)
  { .BlockId = RCC_BLOCK_TIM3       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM3EN         , .LpCtrlMask = RCC_APB1LPENR_TIM3LPEN        , .RstCtrlMask = RCC_APB1RSTR_TIM3RST       },
#endif /* RCC_APB1ENR_TIM3EN */
#if defined(RCC_APB1ENR_TIM4EN)
  { .BlockId = RCC_BLOCK_TIM4       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM4EN         , .LpCtrlMask = RCC_APB1LPENR_TIM4LPEN        , .RstCtrlMask = RCC_APB1RSTR_TIM4RST       },
#endif /* RCC_APB1ENR_TIM4EN */
  { .BlockId = RCC_BLOCK_TIM5       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM5EN         , .LpCtrlMask = RCC_APB1LPENR_TIM5LPEN        , .RstCtrlMask = RCC_APB1RSTR_TIM5RST       },
#if defined(RCC_APB1ENR_TIM6EN)
  { .BlockId = RCC_BLOCK_TIM6       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM6EN         , .LpCtrlMask = RCC_APB1LPENR_TIM6LPEN        , .RstCtrlMask = RCC_APB1RSTR_TIM6RST       },
#endif /* RCC_APB1ENR_TIM6EN */
#if defined(RCC_APB1ENR_TIM7EN)
  { .BlockId = RCC_BLOCK_TIM7       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM7EN         , .LpCtrlMask = RCC_APB1LPENR_TIM7LPEN        , .RstCtrlMask = RCC_APB1RSTR_TIM7RST       },
#endif /* RCC_APB1ENR_TIM7EN */
#if defined(RCC_APB2ENR_TIM8EN)
  { .BlockId = RCC_BLOCK_TIM8       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_TIM8EN         , .LpCtrlMask = RCC_APB2LPENR_TIM8LPEN        , .RstCtrlMask = RCC_APB2RSTR_TIM8RST       },
#endif /* RCC_APB2ENR_TIM8EN */
  { .BlockId = RCC_BLOCK_TIM9       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_TIM9EN         , .LpCtrlMask = RCC_APB2LPENR_TIM9LPEN        , .RstCtrlMask = RCC_APB2RSTR_TIM9RST       },
#if defined(RCC_APB2ENR_TIM10EN)
  { .BlockId = RCC_BLOCK_TIM10      , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_TIM10EN        , .LpCtrlMask = RCC_APB2LPENR_TIM10LPEN       , .RstCtrlMask = RCC_APB2RSTR_TIM10RST      },
#endif /* RCC_APB2ENR_TIM10EN */
  { .BlockId = RCC_BLOCK_TIM11      , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_TIM11EN        , .LpCtrlMask = RCC_APB2LPENR_TIM11LPEN       , .RstCtrlMask = RCC_APB2RSTR_TIM11RST      },
#if defined(RCC_APB1ENR_TIM12EN)
  { .BlockId = RCC_BLOCK_TIM12      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM12EN        , .LpCtrlMask = RCC_APB1LPENR_TIM12LPEN       , .RstCtrlMask = RCC_APB1RSTR_TIM12RST      },
#endif /* RCC_APB1ENR_TIM12EN */
#if defined(RCC_APB1ENR_TIM13EN)
  { .BlockId = RCC_BLOCK_TIM13      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM13EN        , .LpCtrlMask = RCC_APB1LPENR_TIM13LPEN       , .RstCtrlMask = RCC_APB1RSTR_TIM13RST      },
#endif /* RCC_APB1ENR_TIM13EN */
#if defined(RCC_APB1ENR_TIM14EN)
  { .BlockId = RCC_BLOCK_TIM14      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_TIM14EN        , .LpCtrlMask = RCC_APB1LPENR_TIM14LPEN       , .RstCtrlMask = RCC_APB1RSTR_TIM14RST      },
#endif /* RCC_APB1ENR_TIM14EN */

#if defined(RCC_APB1ENR_LPTIM1EN)
  { .BlockId = RCC_BLOCK_LPTIM1     , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_LPTIM1EN       , .LpCtrlMask = RCC_APB1LPENR_LPTIM1LPEN      , .RstCtrlMask = RCC_APB1RSTR_LPTIM1RST     },
#endif /* RCC_APB1ENR_LPTIM1EN */

  /*------------------------------ Connectivity ------------------------------*/

  { .BlockId = RCC_BLOCK_SPI1       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SPI1EN         , .LpCtrlMask = RCC_APB2LPENR_SPI1LPEN        , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       },
#if defined(RCC_APB1ENR_SPI2EN)
  { .BlockId = RCC_BLOCK_SPI2       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_SPI2EN         , .LpCtrlMask = RCC_APB1LPENR_SPI2LPEN        , .RstCtrlMask = RCC_APB1RSTR_SPI2RST       },
#endif /* RCC_APB1ENR_SPI2EN */
#if defined(RCC_APB1ENR_SPI3EN)
  { .BlockId = RCC_BLOCK_SPI3       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_SPI3EN         , .LpCtrlMask = RCC_APB1LPENR_SPI3LPEN        , .RstCtrlMask = RCC_APB1RSTR_SPI3RST       },
#endif /* RCC_APB1ENR_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
  { .BlockId = RCC_BLOCK_SPI4       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SPI4EN         , .LpCtrlMask = RCC_APB2LPENR_SPI4LPEN        , .RstCtrlMask = RCC_APB2RSTR_SPI4RST       },
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_APB2ENR_SPI5EN)
  { .BlockId = RCC_BLOCK_SPI5       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SPI5EN         , .LpCtrlMask = RCC_APB2LPENR_SPI5LPEN        , .RstCtrlMask = RCC_APB2RSTR_SPI5RST       },
#endif /* RCC_APB2ENR_SPI5EN */
#if defined(RCC_APB2ENR_SPI6EN)
  { .BlockId = RCC_BLOCK_SPI6       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SPI6EN         , .LpCtrlMask = RCC_APB2LPENR_SPI6LPEN        , .RstCtrlMask = RCC_APB2RSTR_SPI6RST       },
#endif /* RCC_APB2ENR_SPI6EN */

  { .BlockId = RCC_BLOCK_I2C1       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_I2C1EN         , .LpCtrlMask = RCC_APB1LPENR_I2C1LPEN        , .RstCtrlMask = RCC_APB1RSTR_I2C1RST       },
  { .BlockId = RCC_BLOCK_I2C2       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_I2C2EN         , .LpCtrlMask = RCC_APB1LPENR_I2C2LPEN        , .RstCtrlMask = RCC_APB1RSTR_I2C2RST       },
#if defined(RCC_APB1ENR_I2C3EN)
  { .BlockId = RCC_BLOCK_I2C3       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_I2C3EN         , .LpCtrlMask = RCC_APB1LPENR_I2C3LPEN        , .RstCtrlMask = RCC_APB1RSTR_I2C3RST       },
#endif /* RCC_APB1ENR_I2C3EN */
#if defined(RCC_APB1ENR_FMPI2C1EN)
#if defined(RCC_APB1LPENR_FMPI2C1LPEN)
  { .BlockId = RCC_BLOCK_FMPI2C1    , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_FMPI2C1EN      , .LpCtrlMask = RCC_APB1LPENR_FMPI2C1LPEN     , .RstCtrlMask = RCC_APB1RSTR_FMPI2C1RST    },
#else
  { .BlockId = RCC_BLOCK_FMPI2C1    , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_FMPI2C1EN      , .LpCtrlMask = RCC_UNSUPPORTED_FUNCTION      , .RstCtrlMask = RCC_APB1RSTR_FMPI2C1RST    },
#endif /* RCC_APB1LPENR_FMPI2C1LPEN */
#endif /* RCC_APB1ENR_FMPI2C1EN */

  { .BlockId = RCC_BLOCK_USART1     , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_USART1EN       , .LpCtrlMask = RCC_APB2LPENR_USART1LPEN      , .RstCtrlMask = RCC_APB2RSTR_USART1RST     },
  { .BlockId = RCC_BLOCK_USART2     , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_USART2EN       , .LpCtrlMask = RCC_APB1LPENR_USART2LPEN      , .RstCtrlMask = RCC_APB1RSTR_USART2RST     },
#if defined(RCC_APB1ENR_USART3EN)
  { .BlockId = RCC_BLOCK_USART3     , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_USART3EN       , .LpCtrlMask = RCC_APB1LPENR_USART3LPEN      , .RstCtrlMask = RCC_APB1RSTR_USART3RST     },
#endif /* RCC_APB1ENR_USART3EN */
#if defined(RCC_APB1ENR_UART4EN)
  { .BlockId = RCC_BLOCK_UART4      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_UART4EN        , .LpCtrlMask = RCC_APB1LPENR_UART4LPEN       , .RstCtrlMask = RCC_APB1RSTR_UART4RST      },
#endif /* RCC_APB1ENR_UART4EN */
#if defined(RCC_APB1ENR_UART5EN)
  { .BlockId = RCC_BLOCK_UART5      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_UART5EN        , .LpCtrlMask = RCC_APB1LPENR_UART5LPEN       , .RstCtrlMask = RCC_APB1RSTR_UART5RST      },
#endif /* RCC_APB1ENR_UART5EN */
#if defined(RCC_APB2ENR_USART6EN)
  { .BlockId = RCC_BLOCK_USART6     , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_USART6EN       , .LpCtrlMask = RCC_APB2LPENR_USART6LPEN      , .RstCtrlMask = RCC_APB2RSTR_USART6RST     },
#endif /* RCC_APB2ENR_USART6EN */
#if defined(RCC_APB1ENR_UART7EN)
  { .BlockId = RCC_BLOCK_UART7      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_UART7EN        , .LpCtrlMask = RCC_APB1LPENR_UART7LPEN       , .RstCtrlMask = RCC_APB1RSTR_UART7RST      },
#endif /* RCC_APB1ENR_UART7EN */
#if defined(RCC_APB1ENR_UART8EN)
  { .BlockId = RCC_BLOCK_UART8      , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_UART8EN        , .LpCtrlMask = RCC_APB1LPENR_UART8LPEN       , .RstCtrlMask = RCC_APB1RSTR_UART8RST      },
#endif /* RCC_APB1ENR_UART8EN */
#if defined(RCC_APB2ENR_UART9EN)
  { .BlockId = RCC_BLOCK_UART9      , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_UART9EN        , .LpCtrlMask = RCC_APB2LPENR_UART9LPEN       , .RstCtrlMask = RCC_APB2RSTR_UART9RST      },
#endif /* RCC_APB2ENR_UART9EN */
#if defined(RCC_APB2ENR_UART10EN)
  { .BlockId = RCC_BLOCK_UART10     , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_UART10EN       , .LpCtrlMask = RCC_APB2LPENR_UART10LPEN      , .RstCtrlMask = RCC_APB2RSTR_UART10RST     },
#endif /* RCC_APB2ENR_UART10EN */

#if defined(RCC_APB1ENR_CAN1EN)
  { .BlockId = RCC_BLOCK_CAN1       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_CAN1EN         , .LpCtrlMask = RCC_APB1LPENR_CAN1LPEN        , .RstCtrlMask = RCC_APB1RSTR_CAN1RST       },
#endif /* RCC_APB1ENR_CAN1EN */
#if defined(RCC_APB1ENR_CAN2EN)
  { .BlockId = RCC_BLOCK_CAN2       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_CAN2EN         , .LpCtrlMask = RCC_APB1LPENR_CAN2LPEN        , .RstCtrlMask = RCC_APB1RSTR_CAN2RST       },
#endif /* RCC_APB1ENR_CAN2EN */
#if defined(RCC_APB1ENR_CAN3EN)
  { .BlockId = RCC_BLOCK_CAN3       , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_CAN3EN         , .LpCtrlMask = RCC_APB1LPENR_CAN3LPEN        , .RstCtrlMask = RCC_APB1RSTR_CAN3RST       },
#endif /* RCC_APB1ENR_CAN3EN */

#if defined(RCC_APB2ENR_SDIOEN)
  { .BlockId = RCC_BLOCK_SDIO       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SDIOEN         , .LpCtrlMask = RCC_APB2LPENR_SDIOLPEN        , .RstCtrlMask = RCC_APB2RSTR_SDIORST       },
#endif /* RCC_APB2ENR_SDIOEN */

#if defined(RCC_AHB3ENR_FSMCEN)
  { .BlockId = RCC_BLOCK_FSMC       , .RegBankId = RCC_REG_BANK_AHB3, .StateMask = RCC_AHB3ENR_FSMCEN         , .LpCtrlMask = RCC_AHB3LPENR_FSMCLPEN        , .RstCtrlMask = RCC_AHB3RSTR_FSMCRST       },
#endif /* RCC_AHB3ENR_FSMCEN */
#if defined(RCC_AHB3ENR_FMCEN)
  { .BlockId = RCC_BLOCK_FMC        , .RegBankId = RCC_REG_BANK_AHB3, .StateMask = RCC_AHB3ENR_FMCEN          , .LpCtrlMask = RCC_AHB3LPENR_FMCLPEN         , .RstCtrlMask = RCC_AHB3RSTR_FMCRST        },
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
  { .BlockId = RCC_BLOCK_QSPI       , .RegBankId = RCC_REG_BANK_AHB3, .StateMask = RCC_AHB3ENR_QSPIEN         , .LpCtrlMask = RCC_AHB3LPENR_QSPILPEN        , .RstCtrlMask = RCC_AHB3RSTR_QSPIRST       },
#endif /* RCC_AHB3ENR_QSPIEN */

#if defined(RCC_AHB2ENR_OTGFSEN)
  { .BlockId = RCC_BLOCK_USB_OTG_FS , .RegBankId = RCC_REG_BANK_AHB2, .StateMask = RCC_AHB2ENR_OTGFSEN        , .LpCtrlMask = RCC_AHB2LPENR_OTGFSLPEN       , .RstCtrlMask = RCC_AHB2RSTR_OTGFSRST      },
#endif /* RCC_AHB2ENR_OTGFSEN */
#if defined(RCC_AHB1ENR_OTGHSEN)
  { .BlockId = RCC_BLOCK_USB_OTG_HS , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_OTGHSEN        , .LpCtrlMask = RCC_AHB1LPENR_OTGHSLPEN       , .RstCtrlMask = RCC_AHB1RSTR_OTGHRST       },
  { .BlockId = RCC_BLOCK_USB_OTG_HS_ULPI, .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_OTGHSULPIEN, .LpCtrlMask = RCC_AHB1LPENR_OTGHSULPILPEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1ENR_OTGHSEN */

#if defined(RCC_AHB1ENR_ETHMACEN)
  { .BlockId = RCC_BLOCK_ETH        , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_ETHMACEN       , .LpCtrlMask = RCC_AHB1LPENR_ETHMACLPEN      , .RstCtrlMask = RCC_AHB1RSTR_ETHMACRST     },
  { .BlockId = RCC_BLOCK_ETH_TX     , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_ETHMACTXEN     , .LpCtrlMask = RCC_AHB1LPENR_ETHMACTXLPEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_ETH_RX     , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_ETHMACRXEN     , .LpCtrlMask = RCC_AHB1LPENR_ETHMACRXLPEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_ETH_PTP    , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_ETHMACPTPEN    , .LpCtrlMask = RCC_AHB1LPENR_ETHMACPTPLPEN   , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_AHB1ENR_ETHMACEN */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(RCC_AHB2ENR_DCMIEN)
  { .BlockId = RCC_BLOCK_DCMI       , .RegBankId = RCC_REG_BANK_AHB2, .StateMask = RCC_AHB2ENR_DCMIEN         , .LpCtrlMask = RCC_AHB2LPENR_DCMILPEN        , .RstCtrlMask = RCC_AHB2RSTR_DCMIRST       },
#endif /* RCC_AHB2ENR_DCMIEN */
#if defined(RCC_APB2ENR_LTDCEN)
  { .BlockId = RCC_BLOCK_LTDC       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_LTDCEN         , .LpCtrlMask = RCC_APB2LPENR_LTDCLPEN        , .RstCtrlMask = RCC_APB2RSTR_LTDCRST       },
#endif /* RCC_APB2ENR_LTDCEN */
#if defined(RCC_APB2ENR_DSIEN)
  { .BlockId = RCC_BLOCK_DSI        , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_DSIEN          , .LpCtrlMask = RCC_APB2LPENR_DSILPEN         , .RstCtrlMask = RCC_APB2RSTR_DSIRST        },
#endif /* RCC_APB2ENR_DSIEN */
#if defined(RCC_APB2ENR_SAI1EN)
  { .BlockId = RCC_BLOCK_SAI1       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SAI1EN         , .LpCtrlMask = RCC_APB2LPENR_SAI1LPEN        , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       },
#endif /* RCC_APB2ENR_SAI1EN */
#if defined(RCC_APB2ENR_SAI2EN)
  { .BlockId = RCC_BLOCK_SAI2       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_SAI2EN         , .LpCtrlMask = RCC_APB2LPENR_SAI2LPEN        , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       },
#endif /* RCC_APB2ENR_SAI2EN */
#if defined(RCC_APB1ENR_CECEN)
  { .BlockId = RCC_BLOCK_CEC        , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_CECEN          , .LpCtrlMask = RCC_APB1LPENR_CECLPEN         , .RstCtrlMask = RCC_APB1RSTR_CECRST        },
#endif /* RCC_APB1ENR_CECEN */
#if defined(RCC_APB1ENR_SPDIFRXEN)
  { .BlockId = RCC_BLOCK_SPDIFRX    , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_SPDIFRXEN      , .LpCtrlMask = RCC_APB1LPENR_SPDIFRXLPEN     , .RstCtrlMask = RCC_APB1RSTR_SPDIFRXRST    },
#endif /* RCC_APB1ENR_SPDIFRXEN */
#if defined(RCC_APB2ENR_DFSDM1EN)
  { .BlockId = RCC_BLOCK_DFSDM1     , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_DFSDM1EN       , .LpCtrlMask = RCC_APB2LPENR_DFSDM1LPEN      , .RstCtrlMask = RCC_APB2RSTR_DFSDM1RST     },
#endif /* RCC_APB2ENR_DFSDM1EN */
#if defined(RCC_APB2ENR_DFSDM2EN)
  { .BlockId = RCC_BLOCK_DFSDM2     , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_DFSDM2EN       , .LpCtrlMask = RCC_APB2LPENR_DFSDM2LPEN      , .RstCtrlMask = RCC_APB2RSTR_DFSDM2RST     },
#endif /* RCC_APB2ENR_DFSDM2EN */

  /*--------------------------------- Analog ---------------------------------*/

  { .BlockId = RCC_BLOCK_ADC1       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_ADC1EN         , .LpCtrlMask = RCC_APB2LPENR_ADC1LPEN        , .RstCtrlMask = RCC_APB2RSTR_ADCRST        },
#if defined(RCC_APB2ENR_ADC2EN)
  { .BlockId = RCC_BLOCK_ADC2       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_ADC2EN         , .LpCtrlMask = RCC_APB2LPENR_ADC2LPEN        , .RstCtrlMask = RCC_APB2RSTR_ADCRST        },
#endif /* RCC_APB2ENR_ADC2EN */
#if defined(RCC_APB2ENR_ADC3EN)
  { .BlockId = RCC_BLOCK_ADC3       , .RegBankId = RCC_REG_BANK_APB2, .StateMask = RCC_APB2ENR_ADC3EN         , .LpCtrlMask = RCC_APB2LPENR_ADC3LPEN        , .RstCtrlMask = RCC_APB2RSTR_ADCRST        },
#endif /* RCC_APB2ENR_ADC3EN */
#if defined(RCC_APB1ENR_DACEN)
  { .BlockId = RCC_BLOCK_DAC        , .RegBankId = RCC_REG_BANK_APB1, .StateMask = RCC_APB1ENR_DACEN          , .LpCtrlMask = RCC_APB1LPENR_DACLPEN         , .RstCtrlMask = RCC_APB1RSTR_DACRST        },
#endif /* RCC_APB1ENR_DACEN */

  /*-------------------------------- Security --------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
  { .BlockId = RCC_BLOCK_AES        , .RegBankId = RCC_REG_BANK_AHB2, .StateMask = RCC_AHB2ENR_AESEN          , .LpCtrlMask = RCC_AHB2LPENR_AESLPEN         , .RstCtrlMask = RCC_AHB2RSTR_AESRST        },
#endif /* RCC_AHB2ENR_AESEN */
#if defined(RCC_AHB2ENR_CRYPEN)
  { .BlockId = RCC_BLOCK_CRYP       , .RegBankId = RCC_REG_BANK_AHB2, .StateMask = RCC_AHB2ENR_CRYPEN         , .LpCtrlMask = RCC_AHB2LPENR_CRYPLPEN        , .RstCtrlMask = RCC_AHB2RSTR_CRYPRST       },
#endif /* RCC_AHB2ENR_CRYPEN */
#if defined(RCC_AHB2ENR_HASHEN)
  { .BlockId = RCC_BLOCK_HASH       , .RegBankId = RCC_REG_BANK_AHB2, .StateMask = RCC_AHB2ENR_HASHEN         , .LpCtrlMask = RCC_AHB2LPENR_HASHLPEN        , .RstCtrlMask = RCC_AHB2RSTR_HASHRST       },
#endif /* RCC_AHB2ENR_HASHEN */
#if defined(RCC_AHB1ENR_RNGEN)
  { .BlockId = RCC_BLOCK_RNG        , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_RNGEN          , .LpCtrlMask = RCC_AHB1LPENR_RNGLPEN         , .RstCtrlMask = RCC_AHB1RSTR_RNGRST        },
#elif defined(RCC_AHB2ENR_RNGEN)
  { .BlockId = RCC_BLOCK_RNG        , .RegBankId = RCC_REG_BANK_AHB2, .StateMask = RCC_AHB2ENR_RNGEN          , .LpCtrlMask = RCC_AHB2LPENR_RNGLPEN         , .RstCtrlMask = RCC_AHB2RSTR_RNGRST        },
#endif /* RCC_AHB1ENR_RNGEN */

  /*-------------------------------- Computing -------------------------------*/

  { .BlockId = RCC_BLOCK_CRC        , .RegBankId = RCC_REG_BANK_AHB1, .StateMask = RCC_AHB1ENR_CRCEN          , .LpCtrlMask = RCC_AHB1LPENR_CRCLPEN         , .RstCtrlMask = RCC_AHB1RSTR_CRCRST        },
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
};

_Static_assert( (sizeof(rcc_ResetSrcLut) / sizeof(uint32_t)) == RCC_RESET_SRC_CNT, "Rcc: rcc_ResetSrcLut has incorrect size." );

/* ------------------------- Flash latency thresholds ----------------------- */

#if defined(RCC_MAX_FREQUENCY_SCALE1)
/** \brief HCLK frequencies above which next wait state is required - voltage scale 1 (VDD 2.7 - 3.6 V) */
static const rcc_FreqHz_t rcc_FlashLatencyScale1[] =
{
    FLASH_SCALE1_LATENCY1_FREQ,
    FLASH_SCALE1_LATENCY2_FREQ,
#if defined(FLASH_SCALE1_LATENCY3_FREQ)
    FLASH_SCALE1_LATENCY3_FREQ,
#endif /* FLASH_SCALE1_LATENCY3_FREQ */
#if defined(FLASH_SCALE1_LATENCY4_FREQ)
    FLASH_SCALE1_LATENCY4_FREQ,
#endif /* FLASH_SCALE1_LATENCY4_FREQ */
#if defined(FLASH_SCALE1_LATENCY5_FREQ)
    FLASH_SCALE1_LATENCY5_FREQ,
#endif /* FLASH_SCALE1_LATENCY5_FREQ */
};
#endif /* RCC_MAX_FREQUENCY_SCALE1 */

/** \brief HCLK frequencies above which next wait state is required - voltage scale 2 (VDD 2.7 - 3.6 V) */
static const rcc_FreqHz_t rcc_FlashLatencyScale2[] =
{
    FLASH_SCALE2_LATENCY1_FREQ,
    FLASH_SCALE2_LATENCY2_FREQ,
#if defined(FLASH_SCALE2_LATENCY3_FREQ)
    FLASH_SCALE2_LATENCY3_FREQ,
#endif /* FLASH_SCALE2_LATENCY3_FREQ */
#if defined(FLASH_SCALE2_LATENCY4_FREQ)
    FLASH_SCALE2_LATENCY4_FREQ,
#endif /* FLASH_SCALE2_LATENCY4_FREQ */
#if defined(FLASH_SCALE2_LATENCY5_FREQ)
    FLASH_SCALE2_LATENCY5_FREQ,
#endif /* FLASH_SCALE2_LATENCY5_FREQ */
};

#if defined(LL_PWR_REGU_VOLTAGE_SCALE3)
/** \brief HCLK frequencies above which next wait state is required - voltage scale 3 (VDD 2.7 - 3.6 V) */
static const rcc_FreqHz_t rcc_FlashLatencyScale3[] =
{
    FLASH_SCALE3_LATENCY1_FREQ,
    FLASH_SCALE3_LATENCY2_FREQ,
#if defined(FLASH_SCALE3_LATENCY3_FREQ)
    FLASH_SCALE3_LATENCY3_FREQ,
#endif /* FLASH_SCALE3_LATENCY3_FREQ */
};
#endif /* LL_PWR_REGU_VOLTAGE_SCALE3 */

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
 *  2. System clock is switched to HSI and all PLLs are de-activated (voltage
 *     scaling can be changed only while main PLL is off).
 *  3. Voltage scaling, HSE oscillator and CSS are configured.
 *  4. PLLs are configured and activated, voltage scaling ready is awaited and
 *     over-drive mode is activated if required.
 *  5. Flash latency, bus dividers and system clock source are configured.
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
            /* Flash instruction and data caches (ART accelerator) */
            Rcc_Set_RegBit( RCC_REG_FLASH_ACR, FLASH_ACR_ICEN | FLASH_ACR_DCEN );

            /* A PLL cannot be deactivated by hardware while it drives SYSCLK
             * (e.g. after a previous Rcc_Init() call). Move SYSCLK to HSI,
             * which is always available, before any PLL is reconfigured. */
            retState = Rcc_ClkSrc_Set_HsiActive();
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_HSI );
        }

        for( rcc_PllId_t pllId = RCC_PLL_MAIN; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                /* Shared PLL settings and voltage scaling require inactive PLLs */
                retState = Rcc_Pll_Set_Inactive( pllId );
            }
        }

#if defined(PWR_CR_ODEN)
        if( RCC_REQUEST_OK == retState )
        {
            /* Over-drive mode is activated again below if required */
            Rcc_Reset_RegBit( RCC_REG_PWR_CR, PWR_CR_ODSWEN );
            Rcc_Reset_RegBit( RCC_REG_PWR_CR, PWR_CR_ODEN );

            retState = Rcc_Wait_RegVal( RCC_REG_PWR_CSR, PWR_CSR_ODRDY | PWR_CSR_ODSWRDY, 0u );
        }
#endif /* PWR_CR_ODEN */

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
                Rcc_Reset_RegBit( RCC_REG_CR, RCC_CR_CSSON );
            }
        }

        for( rcc_PllId_t pllId = RCC_PLL_MAIN; RCC_PLL_CNT > pllId; pllId++ )
        {
            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Config( pllId, &clockConfig->Pll_Config[ pllId ] );
            }
        }

        if( ( RCC_REQUEST_OK   == retState                                         ) &&
            ( RCC_PLL_SRC_NONE != clockConfig->Pll_Config[ RCC_PLL_MAIN ].Pll_Source )    )
        {
            /* Voltage scaling is applied when main PLL is active */
            retState = Rcc_Wait_RegVal( RCC_REG_PWR_CSR, PWR_CSR_VOSRDY, PWR_CSR_VOSRDY );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_OverDrive( clockConfig, expectedSysClk );
        }

        if( RCC_REQUEST_OK == retState )
        {
            /* Flash latency for the expected clock (HSI used meanwhile requires the lowest latency) */
            retState = Rcc_Set_FlashLatency( clockConfig );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_AHBDivider( clockConfig->AHB_Divider );
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
            retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
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
                /* MCO configuration (return states are not needed to be checked) */
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
 * Default configuration is valid for all MCUs of STM32F4 family:
 * - main PLL clocked by HSI (16 MHz / 16 * 336), SYSCLK 84 MHz (P = 4),
 *   48 MHz clock (Q = 7)
 * - AHB 84 MHz, APB1 42 MHz, APB2 84 MHz
 * - HSE, PLLI2S, PLLSAI and clock outputs are not used
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
        clockConfig->APB1_Divider      = RCC_APB1_DIVIDER_2;
        clockConfig->APB2_Divider      = RCC_APB2_DIVIDER_1;
        clockConfig->FlashLatency      = RCC_FLASH_LATENCY_0_WS;
#if defined(RCC_MAX_FREQUENCY_SCALE1)
        clockConfig->VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_1;
#else
        clockConfig->VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_2;
#endif /* RCC_MAX_FREQUENCY_SCALE1 */
        clockConfig->SysTickInterval   = RCC_DEFAULT_SYSTICK_INTERVAL_MS;

        for( rcc_ClkOut_Id_t clkOutId = RCC_CLK_OUT_MCO1; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
        {
            clockConfig->McoConfig[ clkOutId ].ClockSource  = RCC_CLK_SOURCE_NONE;
            clockConfig->McoConfig[ clkOutId ].ClockDivider = RCC_DEFAULT_CLK_OUT_DIV;
        }

        for( rcc_PllId_t pllId = RCC_PLL_MAIN; RCC_PLL_CNT > pllId; pllId++ )
        {
            /* PLLI2S and PLLSAI are not used by default */
            clockConfig->Pll_Config[ pllId ].Pll_Source   = RCC_PLL_SRC_NONE;
            clockConfig->Pll_Config[ pllId ].M_Divider    = RCC_DEFAULT_PLL_M_DIV;
            clockConfig->Pll_Config[ pllId ].N_Multiplier = RCC_DEFAULT_PLL_N_MULT;
            clockConfig->Pll_Config[ pllId ].P_Divider    = RCC_DEFAULT_PLL_OUT_UNUSED;
            clockConfig->Pll_Config[ pllId ].Q_Divider    = RCC_DEFAULT_PLL_OUT_UNUSED;
            clockConfig->Pll_Config[ pllId ].R_Divider    = RCC_DEFAULT_PLL_OUT_UNUSED;
        }

        clockConfig->Pll_Config[ RCC_PLL_MAIN ].Pll_Source = RCC_PLL_SRC_HSI;
        clockConfig->Pll_Config[ RCC_PLL_MAIN ].P_Divider  = RCC_DEFAULT_PLL_P_DIV;
        clockConfig->Pll_Config[ RCC_PLL_MAIN ].Q_Divider  = RCC_DEFAULT_PLL_Q_DIV;

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
 * (HSI, LSI) which is not running, the oscillator is started before the clock
 * MUX is switched. For divided HSE clock of RTC the RTC prescaler is
 * configured if needed. External sources (HSE, LSE) and PLL outputs are not
 * started.
 *
 * \warning Some peripherals have connected clock source. (e.g. USB OTG FS, SDIO
 *          and RNG use the common 48 MHz clock)
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
 * The kernel clock multiplexer of the peripheral (FMPI2C1, LPTIM1) is set back to
 * its default value, so the peripheral can be activated again with another clock
 * source. RTC clock selection (RTCSEL) is kept - it can be changed only by backup
 * domain reset.
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

        if( ( RCC_REQUEST_OK       == retState ) &&
            ( RCC_CLK_MUX_LIST_CNT  > clkMuxId ) &&
            ( RCC_BLOCK_RTC        != blockId  )    )
        {
            /* Kernel clock multiplexer is released for the next activation */
            retState = Rcc_ClkMux_Set_ClkInactive( clkMuxId );
        }
        else
        {
            /* Clock still enabled, no multiplexer or write-once RTC clock selection */
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
 *         otherwise return error (also for peripherals with kernel clock not
 *         handled by the module, e.g. SAI, LTDC, external clocks).
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
 * LPTIM1 any of \c RCC_PERIPH_LPTIM1_PCLK1, \c RCC_PERIPH_LPTIM1_HSI,
 * \c RCC_PERIPH_LPTIM1_LSI or \c RCC_PERIPH_LPTIM1_LSE can be used and
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
 * \note ADC1, ADC2 and ADC3 share one reset control.
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
 * \note All AHB buses share one divider (HCLK).
 *
 * \note STM32F405 / 407 / 415 / 417 device errata "Slowing down APB clock during a DMA
 *       transfer": a divider increase (slower bus clock) is refused while a DMA stream is
 *       enabled - a running DMA transfer would be blocked until reset.
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

    if( RCC_CLK_BUS_APB1 == clkBusId )
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
    else if( RCC_CLK_BUS_APB1 == clkBusId )
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

    if( RCC_CLK_BUS_APB1 == clkBusId )
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
 * \brief Function used for power range (voltage scaling) configuration
 *
 * Voltage scaling has to be configured according to expected system clock
 * frequency (maximal frequency of the scale is given by the MCU).
 *
 * \note Voltage scaling can be changed only while main PLL is inactive. The
 *       new value is applied by hardware after main PLL activation.
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if main PLL is active and the scale differs).
 */
rcc_RequestState_t Rcc_Set_PwrRange( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clockConfig )
    {
        rcc_FunctionState_t pllState = RCC_FUNCTION_INACTIVE;

        /* PWR registers are accessible with enabled interface clock only */
        retState = Rcc_Set_PeriphActive( RCC_PERIPH_PWR );

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Pll_Get_State( RCC_PLL_MAIN, &pllState );
        }

        if( RCC_REQUEST_OK != retState )
        {
            /* PWR interface or PLL state is not available */
        }
        else if( (uint32_t)clockConfig->VoltageScaling == Rcc_Get_RegVal( RCC_REG_PWR_CR, PWR_CR_VOS ) )
        {
            /* Required scale is already configured */
        }
        else if( RCC_FUNCTION_INACTIVE == pllState )
        {
            Rcc_Set_RegVal( RCC_REG_PWR_CR, PWR_CR_VOS, (uint32_t)clockConfig->VoltageScaling );

            retState = Rcc_Wait_RegVal( RCC_REG_PWR_CR, PWR_CR_VOS, (uint32_t)clockConfig->VoltageScaling );
        }
        else
        {
            /* Voltage scaling can not be changed while main PLL is active */
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
 * \brief Function used to flash wait states ("delay") configuration.
 *
 * The flash wait state is dependent on expected processor clock (HCLK) and
 * voltage scaling. Required number of wait states is calculated for supply
 * voltage 2.7 - 3.6 V, configured \ref rcc_ConfigStruct_t::FlashLatency is
 * used as minimal number of wait states.
 *
 * \note Voltage scaling has to be configured first (\ref Rcc_Set_PwrRange).
 *
 * \param clockConfig [in]: Configuration structure
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also if expected system clock exceeds maximal
 *         frequency of the voltage scale).
 */
rcc_RequestState_t Rcc_Set_FlashLatency( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState       = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       expectedSysClk = 0u;
    rcc_FreqHz_t       maxSysClk      = 0u;

    if( RCC_NULL_PTR != clockConfig )
    {
        retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &expectedSysClk );

        switch( clockConfig->VoltageScaling )
        {
#if defined(RCC_MAX_FREQUENCY_SCALE1)
            case RCC_PWR_VOLTAGE_SCALE_1: maxSysClk = RCC_MAX_FREQUENCY_SCALE1; break;
#endif /* RCC_MAX_FREQUENCY_SCALE1 */
            case RCC_PWR_VOLTAGE_SCALE_2: maxSysClk = RCC_MAX_FREQUENCY_SCALE2; break;
#if defined(LL_PWR_REGU_VOLTAGE_SCALE3)
            case RCC_PWR_VOLTAGE_SCALE_3: maxSysClk = RCC_MAX_FREQUENCY_SCALE3; break;
#endif /* LL_PWR_REGU_VOLTAGE_SCALE3 */
            default:                      maxSysClk = 0u;                       break;
        }

        if( ( RCC_REQUEST_OK == retState       ) &&
            ( 0u              < expectedSysClk ) &&
            ( maxSysClk      >= expectedSysClk )    )
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
 * \note PLL source multiplexer is common for all PLLs.
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
        case RCC_OSC_HSI: retState = Rcc_ClkSrc_Set_HsiActive(); break;
        case RCC_OSC_LSI: retState = Rcc_ClkSrc_Set_LsiActive(); break;
        case RCC_OSC_LSE: retState = Rcc_ClkSrc_Set_LseActive(); break;
        default:          retState = RCC_REQUEST_ERROR;          break;
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
        case RCC_OSC_HSI: retState = Rcc_ClkSrc_Set_HsiInactive(); break;
        case RCC_OSC_LSI: retState = Rcc_ClkSrc_Set_LsiInactive(); break;
        case RCC_OSC_LSE: retState = Rcc_ClkSrc_Set_LseInactive(); break;
        default:          retState = RCC_REQUEST_ERROR;            break;
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
        case RCC_OSC_HSI: reqState = Rcc_ClkSrc_Get_HsiState( retState ); break;
        case RCC_OSC_LSI: reqState = Rcc_ClkSrc_Get_LsiState( retState ); break;
        case RCC_OSC_LSE: reqState = Rcc_ClkSrc_Get_LseState( retState ); break;
        default:          reqState = RCC_REQUEST_ERROR;                   break;
    }

    return ( reqState );
}


/**
 * \brief Configures divider of oscillator output.
 *
 * Oscillators of STM32F4 family have no output divider - divider 1 is accepted only.
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
 * \param clkDivider [in]: Clock output divider (1 - 5)
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
 * Clock sources HSI and LSI are mapped to internal oscillators, oscillator
 * which is not running is activated. For divided HSE clock of RTC the RTC
 * prescaler is configured (if not configured yet). Other clock sources (buses,
 * PLL outputs, HSE, LSE) are not handled.
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
 * \brief Activates over-drive mode if required by expected system clock.
 *
 * Over-drive mode (where available) is required for system clock above 168 MHz
 * in voltage scale 1 and above 144 MHz in voltage scale 2. It can be activated
 * only while the main PLL is active and system clock is HSI or HSE.
 *
 * \param clockConfig [in]: Configuration structure
 * \param sysClk      [in]: Expected system clock frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success or
 *         over-drive is not required, otherwise return error.
 */
static rcc_RequestState_t Rcc_Set_OverDrive( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t sysClk )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

#if defined(PWR_CR_ODEN)
    rcc_FunctionState_t odRequired = RCC_FUNCTION_INACTIVE;

    if( ( RCC_PWR_VOLTAGE_SCALE_1        == clockConfig->VoltageScaling ) &&
        ( RCC_PWR_OD_THRESHOLD_SCALE1_HZ <  sysClk                      )    )
    {
        odRequired = RCC_FUNCTION_ACTIVE;
    }
    else if( ( RCC_PWR_VOLTAGE_SCALE_2        == clockConfig->VoltageScaling ) &&
             ( RCC_PWR_OD_THRESHOLD_SCALE2_HZ <  sysClk                      )    )
    {
        odRequired = RCC_FUNCTION_ACTIVE;
    }
    else
    {
        /* Over-drive mode is not required */
    }

    if( RCC_FUNCTION_ACTIVE == odRequired )
    {
        Rcc_Set_RegBit( RCC_REG_PWR_CR, PWR_CR_ODEN );

        retState = Rcc_Wait_RegVal( RCC_REG_PWR_CSR, PWR_CSR_ODRDY, PWR_CSR_ODRDY );

        if( RCC_REQUEST_OK == retState )
        {
            Rcc_Set_RegBit( RCC_REG_PWR_CR, PWR_CR_ODSWEN );

            retState = Rcc_Wait_RegVal( RCC_REG_PWR_CSR, PWR_CSR_ODSWRDY, PWR_CSR_ODSWRDY );
        }
        else
        {
            /* Over-drive mode is not ready */
        }
    }
    else
    {
        /* Over-drive mode is not required */
    }
#else
    (void) clockConfig;
    (void) sysClk;
#endif /* PWR_CR_ODEN */

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
 * Thresholds of the MCU for supply voltage 2.7 - 3.6 V are used (device header
 * definitions FLASH_SCALEx_LATENCYy_FREQ).
 *
 * \param voltageScale [in]: Voltage scaling
 * \param hclkFreq     [in]: Processor clock (HCLK) frequency in Hz
 *
 * \return Number of wait states
 */
static uint32_t Rcc_Get_FlashLatency( rcc_PwrVoltageScale_t voltageScale, rcc_FreqHz_t hclkFreq )
{
    const rcc_FreqHz_t * thresholds = rcc_FlashLatencyScale2;
    uint32_t             thrCnt     = sizeof( rcc_FlashLatencyScale2 ) / sizeof( rcc_FreqHz_t );
    uint32_t             latency    = 0u;

#if defined(RCC_MAX_FREQUENCY_SCALE1)
    if( RCC_PWR_VOLTAGE_SCALE_1 == voltageScale )
    {
        thresholds = rcc_FlashLatencyScale1;
        thrCnt     = sizeof( rcc_FlashLatencyScale1 ) / sizeof( rcc_FreqHz_t );
    }
#endif /* RCC_MAX_FREQUENCY_SCALE1 */
#if defined(LL_PWR_REGU_VOLTAGE_SCALE3)
    if( RCC_PWR_VOLTAGE_SCALE_3 == voltageScale )
    {
        thresholds = rcc_FlashLatencyScale3;
        thrCnt     = sizeof( rcc_FlashLatencyScale3 ) / sizeof( rcc_FreqHz_t );
    }
#endif /* LL_PWR_REGU_VOLTAGE_SCALE3 */

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
 * \brief Function used to wrap main PLL clock output P frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_Main_PClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutP( RCC_PLL_MAIN, clkFreq ) );
}


/**
 * \brief Function used to wrap main PLL clock output Q frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Pll_Get_Main_QClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutQ( RCC_PLL_MAIN, clkFreq ) );
}


/**
 * \brief Function used to wrap main PLL clock output R frequency
 *
 * \param clkFreq [out]: Pointer to PLL clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error (also if the output is not available).
 */
static rcc_RequestState_t Rcc_Pll_Get_Main_RClk( rcc_FreqHz_t * const clkFreq )
{
    return ( Rcc_Pll_Get_Clk_OutR( RCC_PLL_MAIN, clkFreq ) );
}


/**
 * \brief Returns frequency of 48 MHz clock (PLL48CLK) used by USB OTG FS, SDIO and RNG
 *
 * The clock is main PLL output Q, or (CK48 multiplexer on selected MCUs)
 * PLLSAI output P / PLLI2S output Q.
 *
 * \param clkFreq [out]: Pointer to clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_Pll48Clk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           ck48Sel  = 0u;

#if defined(RCC_DCKCFGR2_CK48MSEL)
    ck48Sel = Rcc_Get_RegBit( RCC_REG_DCKCFGR2, RCC_DCKCFGR2_CK48MSEL );
#elif defined(RCC_DCKCFGR_CK48MSEL)
    ck48Sel = Rcc_Get_RegBit( RCC_REG_DCKCFGR, RCC_DCKCFGR_CK48MSEL );
#endif /* RCC_DCKCFGR2_CK48MSEL */

    if( 0u == ck48Sel )
    {
        retState = Rcc_Pll_Get_Clk_OutQ( RCC_PLL_MAIN, clkFreq );
    }
    else
    {
#if defined(RCC_PLLSAICFGR_PLLSAIP)
        retState = Rcc_Pll_Get_Clk_OutP( RCC_PLL_SAI, clkFreq );
#elif defined(RCC_PLLI2SCFGR_PLLI2SQ)
        retState = Rcc_Pll_Get_Clk_OutQ( RCC_PLL_I2S, clkFreq );
#else
        retState = RCC_REQUEST_ERROR;
#endif /* RCC_PLLSAICFGR_PLLSAIP */
    }

    return ( retState );
}


/**
 * \brief Returns frequency of SDIO kernel clock
 *
 * The clock is 48 MHz clock (PLL48CLK) or (SDIO multiplexer on selected MCUs)
 * system clock.
 *
 * \param clkFreq [out]: Pointer to clock frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
static rcc_RequestState_t Rcc_Get_SdioClk( rcc_FreqHz_t * const clkFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           sdioSel  = 0u;

#if defined(RCC_DCKCFGR2_SDIOSEL)
    sdioSel = Rcc_Get_RegBit( RCC_REG_DCKCFGR2, RCC_DCKCFGR2_SDIOSEL );
#elif defined(RCC_DCKCFGR_SDIOSEL)
    sdioSel = Rcc_Get_RegBit( RCC_REG_DCKCFGR, RCC_DCKCFGR_SDIOSEL );
#endif /* RCC_DCKCFGR2_SDIOSEL */

    if( 0u == sdioSel )
    {
        retState = Rcc_Get_Pll48Clk( clkFreq );
    }
    else
    {
        retState = Rcc_ClkBus_Get_SysClk( clkFreq );
    }

    return ( retState );
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
        const rcc_PllConfigStruct_t * const pllConfig = &clockConfig->Pll_Config[ RCC_PLL_MAIN ];

        if( RCC_SYSTEM_CLOCK_SOURCE_HSI == clockConfig->SystemClockSource )
        {
            retState = Rcc_ClkSrc_Get_HsiClk( sysClk );
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == clockConfig->SystemClockSource )
        {
            *sysClk  = clockConfig->HSE_Frequency_Hz;
            retState = RCC_REQUEST_OK;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_CNT > clockConfig->SystemClockSource )
        {
            /* PLL output P or R */
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
                ( 0u             != pllConfig->M_Divider  )    )
            {
                const uint64_t vcoFreq = ( (uint64_t)pllSrcFreq * pllConfig->N_Multiplier ) / pllConfig->M_Divider;
                const uint32_t outDiv  = ( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource ) ? pllConfig->P_Divider :
                                                                                                             pllConfig->R_Divider;

                if( 0u != outDiv )
                {
                    *sysClk = (rcc_FreqHz_t)( vcoFreq / outDiv );
                }
                else
                {
                    /* Output divider is not configured */
                    retState = RCC_REQUEST_ERROR;
                }
            }
            else
            {
                /* Reached error state */
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
