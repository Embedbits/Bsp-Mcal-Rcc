/**
 * \author Mr.Nobody
 * \file Rcc.h
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module common functionality
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
#include "Gpio_Port.h"                      /* GPIO module port include       */
#include "Stm32_rcc.h"                      /* RCC RAL functionality          */
#include "Stm32_bus.h"                      /* CLK Buses RAL functionality    */
#include "Stm32_utils.h"                    /* MCU utilities RAL functionality*/
#include "Stm32_icache.h"                   /* Instruction cache functionality*/
#include "Stm32_dcache.h"                   /* Data cache functionality       */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define RCC_MAJOR_VERSION                       ( 1u )
/** Value of minor version of SW module */
#define RCC_MINOR_VERSION                       ( 0u )
/** Value of patch version of SW module */
#define RCC_PATCH_VERSION                       ( 0u )


/* Defines used for FLASH latency according to HCLK Frequency */
#define UTILS_SCALE1_LATENCY0_FREQ    (32000000U)      /**< HCLK frequency to set FLASH latency 0 in power scale 1 */
#define UTILS_SCALE1_LATENCY1_FREQ    (64000000U)      /**< HCLK frequency to set FLASH latency 1 in power scale 1 */
#define UTILS_SCALE1_LATENCY2_FREQ    (96000000U)      /**< HCLK frequency to set FLASH latency 2 in power scale 1 */
#define UTILS_SCALE1_LATENCY3_FREQ    (128000000U)     /**< HCLK frequency to set FLASH latency 3 in power scale 1 */
#define UTILS_SCALE1_LATENCY4_FREQ    (160000000U)     /**< HCLK frequency to set FLASH latency 4 in power scale 1 */
#define UTILS_SCALE2_LATENCY0_FREQ    (25000000U)      /**< HCLK frequency to set FLASH latency 0 in power scale 2 */
#define UTILS_SCALE2_LATENCY1_FREQ    (50000000U)      /**< HCLK frequency to set FLASH latency 1 in power scale 2 */
#define UTILS_SCALE2_LATENCY2_FREQ    (75000000U)      /**< HCLK frequency to set FLASH latency 2 in power scale 2 */
#define UTILS_SCALE2_LATENCY3_FREQ    (100000000U)     /**< HCLK frequency to set FLASH latency 3 in power scale 2 */
#define UTILS_SCALE3_LATENCY0_FREQ    (12500000U)      /**< HCLK frequency to set FLASH latency 0 in power scale 3 */
#define UTILS_SCALE3_LATENCY1_FREQ    (25000000U)      /**< HCLK frequency to set FLASH latency 1 in power scale 3 */
#define UTILS_SCALE3_LATENCY2_FREQ    (37500000U)      /**< HCLK frequency to set FLASH latency 2 in power scale 3 */
#define UTILS_SCALE3_LATENCY3_FREQ    (50000000U)      /**< HCLK frequency to set FLASH latency 3 in power scale 3 */
#define UTILS_SCALE4_LATENCY0_FREQ    (8000000U)       /**< HCLK frequency to set FLASH latency 0 in power scale 4 */
#define UTILS_SCALE4_LATENCY1_FREQ    (16000000U)      /**< HCLK frequency to set FLASH latency 1 in power scale 4 */
#define UTILS_SCALE4_LATENCY2_FREQ    (24000000U)      /**< HCLK frequency to set FLASH latency 2 in power scale 4 */

/** Maximal wait time for configuration request confirmation */
#define RCC_TIMEOUT_RAW                         ( 0x84FCB )

/** Default value of HSI trimming */
#define RCC_HSI_DEFAULT_TRIM_VALUE              ( 64u )

/** Threshold frequency of SCLK in Hz, when "boost" mode shall be activated */
#define RCC_PWR_RANGE_THRESHOLD_HZ              ( 80000000u )

/** Threshold frequency of SCLK in Hz, when wait state shall be incremented */
#define RCC_FLASH_WAIT_STATE_THRESHOLD_HZ       ( 16000000u )


#define RCC_UNSUPPORTED_FUNCTION                ( 0xFF )

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

static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t *sysClk );

static rcc_RequestState_t Rcc_Pll_Get_1_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_1_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_1_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_2_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_2_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_2_PClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_3_RClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_3_QClk( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Pll_Get_3_PClk( rcc_FreqHz_t * const clkFreq );

static rcc_RequestState_t Rcc_Get_MsisClk_S( rcc_FreqHz_t * const clkFreq );
static rcc_RequestState_t Rcc_Get_MsisClk_K( rcc_FreqHz_t * const clkFreq );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* --------------------- Multipliers/Dividers arrays -------------------------*/

/* The most disgusting part in whole project. Definition of external variables
 * created by STM!!! Shame on you ST! */
uint32_t      SystemCoreClock    = 160000000U;
const uint8_t AHBPrescTable[16u] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8u]  = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Configuration array of registers used by peripheral buses */
const rcc_ClkBusConfigStruct_t         rcc_ClkBusConfigStruct[ RCC_CLK_BUS_CNT ] =
{
  { .ClkBusId = RCC_CLK_BUS_AHB1  , .EnableRegId = RCC_REG_AHB1ENR , .SleepRegId = RCC_REG_AHB1LPENR , .ResetRegId = RCC_REG_AHB1RSTR  },
  { .ClkBusId = RCC_CLK_BUS_AHB2  , .EnableRegId = RCC_REG_AHB2ENR , .SleepRegId = RCC_REG_AHB2LPENR , .ResetRegId = RCC_REG_AHB2RSTR  },
  { .ClkBusId = RCC_CLK_BUS_AHB4  , .EnableRegId = RCC_REG_AHB4ENR , .SleepRegId = RCC_REG_AHB4LPENR , .ResetRegId = RCC_REG_AHB2RSTR  },
  { .ClkBusId = RCC_CLK_BUS_APB1_1, .EnableRegId = RCC_REG_APB1LENR, .SleepRegId = RCC_REG_APB1LLPENR, .ResetRegId = RCC_REG_APB1LRSTR },
  { .ClkBusId = RCC_CLK_BUS_APB1_2, .EnableRegId = RCC_REG_APB1HENR, .SleepRegId = RCC_REG_APB1HLPENR, .ResetRegId = RCC_REG_APB1HRSTR },
  { .ClkBusId = RCC_CLK_BUS_APB2  , .EnableRegId = RCC_REG_APB2ENR , .SleepRegId = RCC_REG_APB2LPENR , .ResetRegId = RCC_REG_APB2RSTR  },
  { .ClkBusId = RCC_CLK_BUS_APB3  , .EnableRegId = RCC_REG_APB3ENR , .SleepRegId = RCC_REG_APB3LPENR , .ResetRegId = RCC_REG_APB3RSTR  }
};


const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[RCC_CLK_SRC_CNT] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK   , .ClkSrcCallback = Rcc_ClkBus_Get_SysClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1RCLK , .ClkSrcCallback = Rcc_Pll_Get_1_RClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1QCLK , .ClkSrcCallback = Rcc_Pll_Get_1_QClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1PCLK , .ClkSrcCallback = Rcc_Pll_Get_1_PClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2RCLK , .ClkSrcCallback = Rcc_Pll_Get_2_RClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2QCLK , .ClkSrcCallback = Rcc_Pll_Get_2_QClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL2PCLK , .ClkSrcCallback = Rcc_Pll_Get_2_PClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3RCLK , .ClkSrcCallback = Rcc_Pll_Get_3_RClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3QCLK , .ClkSrcCallback = Rcc_Pll_Get_3_QClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL3PCLK , .ClkSrcCallback = Rcc_Pll_Get_3_PClk       },
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK   , .ClkSrcCallback = Rcc_ClkBus_Get_AHBClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB1Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB2Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB3CLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB3Clk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI64CLK , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi64Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_CSI4CLK  , .ClkSrcCallback = Rcc_Get_MsisClk_K        },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48CLK , .ClkSrcCallback = Rcc_Get_MsisClk_S        },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_HseClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_LsiClk    },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_LseClk    },
};


/** \brief Configuration array of MCU peripherals. */
const rcc_PeriphConfigStruct_t          rcc_ConfigStruct[ RCC_PERIPH_ID_CNT ] =
{
  { .PeriphId = RCC_PERIPH_FLASH             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_FLASH    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
  { .PeriphId = RCC_PERIPH_SBS               , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_SBS      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

  { .PeriphId = RCC_PERIPH_SYSTICK_HCLK_DIV8 , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_LIST_CNT , .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },
  { .PeriphId = RCC_PERIPH_SYSTICK_LSI       , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LIST_CNT , .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI       },
  { .PeriphId = RCC_PERIPH_SYSTICK_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LIST_CNT , .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE       },

  { .PeriphId = RCC_PERIPH_RTC_HSE_DIV32     , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32     },
  { .PeriphId = RCC_PERIPH_RTC_LSE           , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_RTC_LSE           },
  { .PeriphId = RCC_PERIPH_RTC_LSI           , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_RTC_LSI           },

  { .PeriphId = RCC_PERIPH_CRS               , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_CRS      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

  { .PeriphId = RCC_PERIPH_LPCLK_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },
  { .PeriphId = RCC_PERIPH_LPCLK_HSE         , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_LPCLK_HSE         },
  { .PeriphId = RCC_PERIPH_LPCLK_CSI         , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_LPCLK_CSI         },

#if defined(DCACHE1)
  { .PeriphId = RCC_PERIPH_DCACHE1           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DCACHE1  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* DCACHE1 */

  { .PeriphId = RCC_PERIPH_RAMCFG            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_RAMCFG   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },

#if defined(RAMCFG_BKPRAM)
  { .PeriphId = RCC_PERIPH_BKPSRAM           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_BKPSRAM  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_BKPRAM */

#if defined(RAMCFG_SRAM1)
  { .PeriphId = RCC_PERIPH_SRAM1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_SRAM1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM1 */
#if defined(RAMCFG_SRAM2)
  { .PeriphId = RCC_PERIPH_SRAM2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_SRAM2    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM2 */
#if defined(RAMCFG_SRAM3)
  { .PeriphId = RCC_PERIPH_SRAM3             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_SRAM3    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* RAMCFG_SRAM3 */

#if defined(GPDMA1)
  { .PeriphId = RCC_PERIPH_GPDMA1            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPDMA1   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPDMA1 */
#if defined(GPDMA2)
  { .PeriphId = RCC_PERIPH_GPDMA2            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPDMA2   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPDMA2 */

#if defined(WWDG)
  { .PeriphId = RCC_PERIPH_WWDG              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_WWDG     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* WWDG */

#if defined(GPIOA)
  { .PeriphId = RCC_PERIPH_GPIOA             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOA    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOA */
#if defined(GPIOB)
  { .PeriphId = RCC_PERIPH_GPIOB             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOB    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOB */
#if defined(GPIOC)
  { .PeriphId = RCC_PERIPH_GPIOC             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOC    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOC */
#if defined(GPIOD)
  { .PeriphId = RCC_PERIPH_GPIOD             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOD    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOD */
#if defined(GPIOE)
  { .PeriphId = RCC_PERIPH_GPIOE             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOE    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOE */
#if defined(GPIOF)
  { .PeriphId = RCC_PERIPH_GPIOF             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOF    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOF */
#if defined(GPIOG)
  { .PeriphId = RCC_PERIPH_GPIOG             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOG    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */
#if defined(GPIOH)
  { .PeriphId = RCC_PERIPH_GPIOH             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOH    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */
#if defined(GPIOI)
  { .PeriphId = RCC_PERIPH_GPIOI             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOI    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GPIOG */

  /*--------------------------------- Timers ---------------------------------*/

#if defined(TIM1)
  { .PeriphId = RCC_PERIPH_TIM1              , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM1 */
#if defined(TIM2)
  { .PeriphId = RCC_PERIPH_TIM2              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM2 */
#if defined(TIM3)
  { .PeriphId = RCC_PERIPH_TIM3              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM3     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM3 */
#if defined(TIM4)
  { .PeriphId = RCC_PERIPH_TIM4              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM4     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM4 */
#if defined(TIM5)
  { .PeriphId = RCC_PERIPH_TIM5              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM5     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM5 */
#if defined(TIM6)
  { .PeriphId = RCC_PERIPH_TIM6              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM6     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM6 */
#if defined(TIM7)
  { .PeriphId = RCC_PERIPH_TIM7              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM7     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM7 */
#if defined(TIM8)
  { .PeriphId = RCC_PERIPH_TIM8              , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM8     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM8 */
#if defined(TIM12)
  { .PeriphId = RCC_PERIPH_TIM12             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM12    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM12 */
#if defined(TIM13)
  { .PeriphId = RCC_PERIPH_TIM13             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM13    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM13 */
#if defined(TIM14)
  { .PeriphId = RCC_PERIPH_TIM14             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM14    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM14 */
#if defined(TIM15)
  { .PeriphId = RCC_PERIPH_TIM15             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM15    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM15 */
#if defined(TIM16)
  { .PeriphId = RCC_PERIPH_TIM16             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM16    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM16 */
#if defined(TIM17)
  { .PeriphId = RCC_PERIPH_TIM17             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM17    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* TIM17 */


#if defined(LPTIM1)
  { .PeriphId = RCC_PERIPH_LPTIM1_PCLK3      , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .PeriphId = RCC_PERIPH_LPTIM1_PLL2P      , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL2P      },
  { .PeriphId = RCC_PERIPH_LPTIM1_PLL3R      , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL3R      },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE        },
  { .PeriphId = RCC_PERIPH_LPTIM1_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM1_LPCLK      , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LPCLK      },
#endif /* LPTIM1 */
#if defined(LPTIM2)
  { .PeriphId = RCC_PERIPH_LPTIM2_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .PeriphId = RCC_PERIPH_LPTIM2_PLL2P      , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL2P      },
  { .PeriphId = RCC_PERIPH_LPTIM2_PLL3R      , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL3R      },
  { .PeriphId = RCC_PERIPH_LPTIM2_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE        },
  { .PeriphId = RCC_PERIPH_LPTIM2_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM2_LPCLK      , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxId = RCC_CLK_MUX_LPTIM2_LPCLK      },
#endif /* LPTIM2 */
#if defined(LPTIM3)
  { .PeriphId = RCC_PERIPH_LPTIM3_PCLK3      , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .PeriphId = RCC_PERIPH_LPTIM3_PLL2P      , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxId = RCC_CLK_MUX_LPTIM3_PLL2P      },
  { .PeriphId = RCC_PERIPH_LPTIM3_PLL3R      , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxId = RCC_CLK_MUX_LPTIM3_PLL3R      },
  { .PeriphId = RCC_PERIPH_LPTIM3_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxId = RCC_CLK_MUX_LPTIM3_LSE        },
  { .PeriphId = RCC_PERIPH_LPTIM3_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxId = RCC_CLK_MUX_LPTIM3_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM3_LPCLK      , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxId = RCC_CLK_MUX_LPTIM3_LPCLK      },
#endif /* LPTIM3 */
#if defined(LPTIM4)
  { .PeriphId = RCC_PERIPH_LPTIM4_PCLK3      , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .PeriphId = RCC_PERIPH_LPTIM4_PLL2P      , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxId = RCC_CLK_MUX_LPTIM4_PLL2P      },
  { .PeriphId = RCC_PERIPH_LPTIM4_PLL3R      , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxId = RCC_CLK_MUX_LPTIM4_PLL3R      },
  { .PeriphId = RCC_PERIPH_LPTIM4_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxId = RCC_CLK_MUX_LPTIM4_LSE        },
  { .PeriphId = RCC_PERIPH_LPTIM4_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxId = RCC_CLK_MUX_LPTIM4_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM4_LPCLK      , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxId = RCC_CLK_MUX_LPTIM4_LPCLK      },
#endif /* LPTIM4 */
#if defined(LPTIM5)
  { .PeriphId = RCC_PERIPH_LPTIM5_PCLK3      , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .PeriphId = RCC_PERIPH_LPTIM5_PLL2P      , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxId = RCC_CLK_MUX_LPTIM5_PLL2P      },
  { .PeriphId = RCC_PERIPH_LPTIM5_PLL3R      , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxId = RCC_CLK_MUX_LPTIM5_PLL3R      },
  { .PeriphId = RCC_PERIPH_LPTIM5_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxId = RCC_CLK_MUX_LPTIM5_LSE        },
  { .PeriphId = RCC_PERIPH_LPTIM5_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxId = RCC_CLK_MUX_LPTIM5_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM5_LPCLK      , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxId = RCC_CLK_MUX_LPTIM5_LPCLK      },
#endif /* LPTIM5 */
#if defined(LPTIM6)
  { .PeriphId = RCC_PERIPH_LPTIM6_PCLK3      , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .PeriphId = RCC_PERIPH_LPTIM6_PLL2P      , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxId = RCC_CLK_MUX_LPTIM6_PLL2P      },
  { .PeriphId = RCC_PERIPH_LPTIM6_PLL3R      , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxId = RCC_CLK_MUX_LPTIM6_PLL3R      },
  { .PeriphId = RCC_PERIPH_LPTIM6_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxId = RCC_CLK_MUX_LPTIM6_LSE        },
  { .PeriphId = RCC_PERIPH_LPTIM6_LSI        , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxId = RCC_CLK_MUX_LPTIM6_LSI        },
  { .PeriphId = RCC_PERIPH_LPTIM6_LPCLK      , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxId = RCC_CLK_MUX_LPTIM6_LPCLK      },
#endif /* LPTIM6 */

  /*------------------------------ Connectivity ------------------------------*/

#if defined(SPI1)
  { .PeriphId = RCC_PERIPH_SPI1_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SPI1     , .ClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .PeriphId = RCC_PERIPH_SPI1_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .BlockId = RCC_BLOCK_SPI1     , .ClkMuxId = RCC_CLK_MUX_SPI1_PLL2P        },
  { .PeriphId = RCC_PERIPH_SPI1_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .BlockId = RCC_BLOCK_SPI1     , .ClkMuxId = RCC_CLK_MUX_SPI1_PLL3P        },
  { .PeriphId = RCC_PERIPH_SPI1_PIN          , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxId = RCC_CLK_MUX_SPI1_PIN          },
  { .PeriphId = RCC_PERIPH_SPI1_LPCLK        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxId = RCC_CLK_MUX_SPI1_LPCLK        },
#endif /* SPI1 */
#if defined(SPI2)
  { .PeriphId = RCC_PERIPH_SPI2_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SPI2     , .ClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .PeriphId = RCC_PERIPH_SPI2_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .BlockId = RCC_BLOCK_SPI2     , .ClkMuxId = RCC_CLK_MUX_SPI2_PLL2P        },
  { .PeriphId = RCC_PERIPH_SPI2_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .BlockId = RCC_BLOCK_SPI2     , .ClkMuxId = RCC_CLK_MUX_SPI2_PLL3P        },
  { .PeriphId = RCC_PERIPH_SPI2_PIN          , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxId = RCC_CLK_MUX_SPI2_PIN          },
  { .PeriphId = RCC_PERIPH_SPI2_LPCLK        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxId = RCC_CLK_MUX_SPI2_LPCLK        },
#endif /* SPI2 */
#if defined(SPI3)
  { .PeriphId = RCC_PERIPH_SPI3_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SPI3     , .ClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .PeriphId = RCC_PERIPH_SPI3_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .BlockId = RCC_BLOCK_SPI3     , .ClkMuxId = RCC_CLK_MUX_SPI3_PLL2P        },
  { .PeriphId = RCC_PERIPH_SPI3_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .BlockId = RCC_BLOCK_SPI3     , .ClkMuxId = RCC_CLK_MUX_SPI3_PLL3P        },
  { .PeriphId = RCC_PERIPH_SPI3_PIN          , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxId = RCC_CLK_MUX_SPI3_PIN          },
  { .PeriphId = RCC_PERIPH_SPI3_LPCLK        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxId = RCC_CLK_MUX_SPI3_LPCLK        },
#endif /* SPI3 */
#if defined(SPI4)
  { .PeriphId = RCC_PERIPH_SPI4_PCLK2        , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .PeriphId = RCC_PERIPH_SPI4_PLL2Q        , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_SPI4_PLL2Q        },
  { .PeriphId = RCC_PERIPH_SPI4_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_SPI4_PLL3Q        },
  { .PeriphId = RCC_PERIPH_SPI4_HSI64        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_SPI4_HSI64        },
  { .PeriphId = RCC_PERIPH_SPI4_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_SPI4_CSI          },
  { .PeriphId = RCC_PERIPH_SPI4_HSE          , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_SPI4_HSE          },
#endif /* SPI4 */
#if defined(SPI5)
  { .PeriphId = RCC_PERIPH_SPI5_PCLK3        , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .PeriphId = RCC_PERIPH_SPI5_PLL2Q        , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_SPI5     , .ClkMuxId = RCC_CLK_MUX_SPI5_PLL2Q        },
  { .PeriphId = RCC_PERIPH_SPI5_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_SPI5     , .ClkMuxId = RCC_CLK_MUX_SPI5_PLL3Q        },
  { .PeriphId = RCC_PERIPH_SPI5_HSI64        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_SPI5     , .ClkMuxId = RCC_CLK_MUX_SPI5_HSI64        },
  { .PeriphId = RCC_PERIPH_SPI5_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxId = RCC_CLK_MUX_SPI5_CSI          },
  { .PeriphId = RCC_PERIPH_SPI5_HSE          , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxId = RCC_CLK_MUX_SPI5_HSE          },
#endif /* SPI5 */
#if defined(SPI6)
  { .PeriphId = RCC_PERIPH_SPI6_PCLK2        , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .PeriphId = RCC_PERIPH_SPI6_PLL2Q        , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_SPI6     , .ClkMuxId = RCC_CLK_MUX_SPI6_PLL2Q        },
  { .PeriphId = RCC_PERIPH_SPI6_PLL3Q        , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_SPI6     , .ClkMuxId = RCC_CLK_MUX_SPI6_PLL3Q        },
  { .PeriphId = RCC_PERIPH_SPI6_HSI64        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_SPI6     , .ClkMuxId = RCC_CLK_MUX_SPI6_HSI64        },
  { .PeriphId = RCC_PERIPH_SPI6_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxId = RCC_CLK_MUX_SPI6_CSI          },
  { .PeriphId = RCC_PERIPH_SPI6_HSE          , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxId = RCC_CLK_MUX_SPI6_HSE          },
#endif /* SPI6 */


#if defined(I2C1)
  { .PeriphId = RCC_PERIPH_I2C1_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C1_PLL3R        , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_PLL3R        },
  { .PeriphId = RCC_PERIPH_I2C1_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_HSI64        },
  { .PeriphId = RCC_PERIPH_I2C1_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_CSI          },
#endif /* I2C1 */
#if defined(I2C2)
  { .PeriphId = RCC_PERIPH_I2C2_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
  { .PeriphId = RCC_PERIPH_I2C2_PLL3R        , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_PLL3R        },
  { .PeriphId = RCC_PERIPH_I2C2_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_HSI64        },
  { .PeriphId = RCC_PERIPH_I2C2_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_CSI          },
#endif /* I2C2 */
#if defined(I2C3)
  { .PeriphId = RCC_PERIPH_I2C3_PCLK3        , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
  { .PeriphId = RCC_PERIPH_I2C3_PLL3R        , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_PLL3R        },
  { .PeriphId = RCC_PERIPH_I2C3_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_HSI64        },
  { .PeriphId = RCC_PERIPH_I2C3_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_CSI          },
#endif /* I2C3 */
#if defined(I2C4)
  { .PeriphId = RCC_PERIPH_I2C4_PCLK3        , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
  { .PeriphId = RCC_PERIPH_I2C4_PLL3R        , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_PLL3R        },
  { .PeriphId = RCC_PERIPH_I2C4_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_HSI64        },
  { .PeriphId = RCC_PERIPH_I2C4_CSI          , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_CSI          },
#endif /* I2C4 */

#if defined(I3C1)
  { .PeriphId = RCC_PERIPH_I3C1_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I3C1     , .ClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
  { .PeriphId = RCC_PERIPH_I3C1_PLL3R        , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_I3C1     , .ClkMuxId = RCC_CLK_MUX_I3C1_PLL3R        },
  { .PeriphId = RCC_PERIPH_I3C1_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_I3C1     , .ClkMuxId = RCC_CLK_MUX_I3C1_HSI64        },
#endif /* I3C1 */
#if defined(I3C2)
  { .PeriphId = RCC_PERIPH_I3C2_PCLK1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I3C2     , .ClkMuxId = RCC_CLK_MUX_I3C2_PCLK1        },
  { .PeriphId = RCC_PERIPH_I3C2_PLL3R        , .ClkSrcId = RCC_CLK_SRC_PLL3RCLK, .BlockId = RCC_BLOCK_I3C2     , .ClkMuxId = RCC_CLK_MUX_I3C2_PLL3R        },
  { .PeriphId = RCC_PERIPH_I3C2_HSI          , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_I3C2     , .ClkMuxId = RCC_CLK_MUX_I3C2_HSI64        },
#endif /* I3C2 */


#if defined(USART1)
  { .PeriphId = RCC_PERIPH_USART1_PCLK2      , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .PeriphId = RCC_PERIPH_USART1_PLL2Q      , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_PLL2Q      },
  { .PeriphId = RCC_PERIPH_USART1_PLL3Q      , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_PLL3Q      },
  { .PeriphId = RCC_PERIPH_USART1_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_HSI        },
  { .PeriphId = RCC_PERIPH_USART1_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_LSE        },
  { .PeriphId = RCC_PERIPH_USART1_CSI        , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_CSI        },
#endif /* USART1 */
#if defined(USART2)
  { .PeriphId = RCC_PERIPH_USART2_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .PeriphId = RCC_PERIPH_USART2_PLL2Q      , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_PLL2Q      },
  { .PeriphId = RCC_PERIPH_USART2_PLL3Q      , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_PLL3Q      },
  { .PeriphId = RCC_PERIPH_USART2_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_HSI        },
  { .PeriphId = RCC_PERIPH_USART2_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_LSE        },
  { .PeriphId = RCC_PERIPH_USART2_CSI        , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_CSI        },
#endif /* USART2 */
#if defined(USART3)
  { .PeriphId = RCC_PERIPH_USART3_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .PeriphId = RCC_PERIPH_USART3_PLL2Q      , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_PLL2Q      },
  { .PeriphId = RCC_PERIPH_USART3_PLL3Q      , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_PLL3Q      },
  { .PeriphId = RCC_PERIPH_USART3_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_HSI        },
  { .PeriphId = RCC_PERIPH_USART3_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_LSE        },
  { .PeriphId = RCC_PERIPH_USART3_CSI        , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_CSI        },
#endif /* USART3 */
#if defined(USART6)
  { .PeriphId = RCC_PERIPH_USART6_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART6   , .ClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .PeriphId = RCC_PERIPH_USART6_PLL2Q      , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_USART6   , .ClkMuxId = RCC_CLK_MUX_USART6_PLL2Q      },
  { .PeriphId = RCC_PERIPH_USART6_PLL3Q      , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_USART6   , .ClkMuxId = RCC_CLK_MUX_USART6_PLL3Q      },
  { .PeriphId = RCC_PERIPH_USART6_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_USART6   , .ClkMuxId = RCC_CLK_MUX_USART6_HSI        },
  { .PeriphId = RCC_PERIPH_USART6_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART6   , .ClkMuxId = RCC_CLK_MUX_USART6_LSE        },
  { .PeriphId = RCC_PERIPH_USART6_CSI        , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_USART6   , .ClkMuxId = RCC_CLK_MUX_USART6_CSI        },
#endif /* USART6 */
#if defined(USART10)
  { .PeriphId = RCC_PERIPH_USART10_PCLK1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART10  , .ClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .PeriphId = RCC_PERIPH_USART10_PLL2Q     , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_USART10  , .ClkMuxId = RCC_CLK_MUX_USART10_PLL2Q     },
  { .PeriphId = RCC_PERIPH_USART10_PLL3Q     , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_USART10  , .ClkMuxId = RCC_CLK_MUX_USART10_PLL3Q     },
  { .PeriphId = RCC_PERIPH_USART10_HSI       , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_USART10  , .ClkMuxId = RCC_CLK_MUX_USART10_HSI       },
  { .PeriphId = RCC_PERIPH_USART10_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART10  , .ClkMuxId = RCC_CLK_MUX_USART10_LSE       },
  { .PeriphId = RCC_PERIPH_USART10_CSI       , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_USART10  , .ClkMuxId = RCC_CLK_MUX_USART10_CSI       },
#endif /* USART10 */
#if defined(USART11)
  { .PeriphId = RCC_PERIPH_USART11_PCLK1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART11  , .ClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .PeriphId = RCC_PERIPH_USART11_PLL2Q     , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_USART11  , .ClkMuxId = RCC_CLK_MUX_USART11_PLL2Q     },
  { .PeriphId = RCC_PERIPH_USART11_PLL3Q     , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_USART11  , .ClkMuxId = RCC_CLK_MUX_USART11_PLL3Q     },
  { .PeriphId = RCC_PERIPH_USART11_HSI       , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_USART11  , .ClkMuxId = RCC_CLK_MUX_USART11_HSI       },
  { .PeriphId = RCC_PERIPH_USART11_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART11  , .ClkMuxId = RCC_CLK_MUX_USART11_LSE       },
  { .PeriphId = RCC_PERIPH_USART11_CSI       , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_USART11  , .ClkMuxId = RCC_CLK_MUX_USART11_CSI       },
#endif /* USART11 */

#if defined(UART4)
  { .PeriphId = RCC_PERIPH_UART4_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .PeriphId = RCC_PERIPH_UART4_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_PLL2Q       },
  { .PeriphId = RCC_PERIPH_UART4_PLL3Q       , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_PLL3Q       },
  { .PeriphId = RCC_PERIPH_UART4_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_HSI         },
  { .PeriphId = RCC_PERIPH_UART4_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_LSE         },
  { .PeriphId = RCC_PERIPH_UART4_CSI         , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_CSI         },
#endif /* UART4 */
#if defined(UART5)
  { .PeriphId = RCC_PERIPH_UART5_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .PeriphId = RCC_PERIPH_UART5_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_PLL2Q       },
  { .PeriphId = RCC_PERIPH_UART5_PLL3Q       , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_PLL3Q       },
  { .PeriphId = RCC_PERIPH_UART5_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_HSI         },
  { .PeriphId = RCC_PERIPH_UART5_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_LSE         },
  { .PeriphId = RCC_PERIPH_UART5_CSI         , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_CSI         },
#endif /* USART5 */
#if defined(UART7)
  { .PeriphId = RCC_PERIPH_UART7_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART7    , .ClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .PeriphId = RCC_PERIPH_UART7_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_UART7    , .ClkMuxId = RCC_CLK_MUX_UART7_PLL2Q       },
  { .PeriphId = RCC_PERIPH_UART7_PLL3Q       , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_UART7    , .ClkMuxId = RCC_CLK_MUX_UART7_PLL3Q       },
  { .PeriphId = RCC_PERIPH_UART7_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_UART7    , .ClkMuxId = RCC_CLK_MUX_UART7_HSI         },
  { .PeriphId = RCC_PERIPH_UART7_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART7    , .ClkMuxId = RCC_CLK_MUX_UART7_LSE         },
  { .PeriphId = RCC_PERIPH_UART7_CSI         , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_UART7    , .ClkMuxId = RCC_CLK_MUX_UART7_CSI         },
#endif /* UART7 */
#if defined(UART8)
  { .PeriphId = RCC_PERIPH_UART8_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART8    , .ClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .PeriphId = RCC_PERIPH_UART8_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_UART8    , .ClkMuxId = RCC_CLK_MUX_UART8_PLL2Q       },
  { .PeriphId = RCC_PERIPH_UART8_PLL3Q       , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_UART8    , .ClkMuxId = RCC_CLK_MUX_UART8_PLL3Q       },
  { .PeriphId = RCC_PERIPH_UART8_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_UART8    , .ClkMuxId = RCC_CLK_MUX_UART8_HSI         },
  { .PeriphId = RCC_PERIPH_UART8_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART8    , .ClkMuxId = RCC_CLK_MUX_UART8_LSE         },
  { .PeriphId = RCC_PERIPH_UART8_CSI         , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_UART8    , .ClkMuxId = RCC_CLK_MUX_UART8_CSI         },
#endif /* UART8 */
#if defined(UART9)
  { .PeriphId = RCC_PERIPH_UART9_PCLK1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART9    , .ClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .PeriphId = RCC_PERIPH_UART9_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_UART9    , .ClkMuxId = RCC_CLK_MUX_UART9_PLL2Q       },
  { .PeriphId = RCC_PERIPH_UART9_PLL3Q       , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_UART9    , .ClkMuxId = RCC_CLK_MUX_UART9_PLL3Q       },
  { .PeriphId = RCC_PERIPH_UART9_HSI         , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_UART9    , .ClkMuxId = RCC_CLK_MUX_UART9_HSI         },
  { .PeriphId = RCC_PERIPH_UART9_LSE         , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART9    , .ClkMuxId = RCC_CLK_MUX_UART9_LSE         },
  { .PeriphId = RCC_PERIPH_UART9_CSI         , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_UART9    , .ClkMuxId = RCC_CLK_MUX_UART9_CSI         },
#endif /* UART9 */
#if defined(UART12)
  { .PeriphId = RCC_PERIPH_UART12_PCLK1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART12   , .ClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .PeriphId = RCC_PERIPH_UART12_PLL2Q      , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_UART12   , .ClkMuxId = RCC_CLK_MUX_UART12_PLL2Q      },
  { .PeriphId = RCC_PERIPH_UART12_PLL3Q      , .ClkSrcId = RCC_CLK_SRC_PLL3QCLK, .BlockId = RCC_BLOCK_UART12   , .ClkMuxId = RCC_CLK_MUX_UART12_PLL3Q      },
  { .PeriphId = RCC_PERIPH_UART12_HSI        , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_UART12   , .ClkMuxId = RCC_CLK_MUX_UART12_HSI        },
  { .PeriphId = RCC_PERIPH_UART12_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART12   , .ClkMuxId = RCC_CLK_MUX_UART12_LSE        },
  { .PeriphId = RCC_PERIPH_UART12_CSI        , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_UART12   , .ClkMuxId = RCC_CLK_MUX_UART12_CSI        },
#endif /* UART12 */

#if defined(LPUART1)
  { .PeriphId = RCC_PERIPH_LPUART1_PCLK3     , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .PeriphId = RCC_PERIPH_LPUART1_PLL2Q     , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL2Q     },
  { .PeriphId = RCC_PERIPH_LPUART1_PLL3Q     , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL3Q     },
  { .PeriphId = RCC_PERIPH_LPUART1_HSI       , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI       },
  { .PeriphId = RCC_PERIPH_LPUART1_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE       },
  { .PeriphId = RCC_PERIPH_LPUART1_CSI       , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_CSI       },
#endif /* LPUART1 */

#if defined(FDCAN1)
  { .PeriphId = RCC_PERIPH_FDCAN_PLL1Q       , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_FDCAN    , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL1Q       },
  { .PeriphId = RCC_PERIPH_FDCAN_PLL2Q       , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .BlockId = RCC_BLOCK_FDCAN    , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL2Q       },
  { .PeriphId = RCC_PERIPH_FDCAN_HSE         , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_FDCAN    , .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
#endif /* FDCAN1 */

#if defined(SDMMC1)
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL1Q      , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SDMMC1   , .ClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      },
  { .PeriphId = RCC_PERIPH_SDMMC1_PLL2R      , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .BlockId = RCC_BLOCK_SDMMC1   , .ClkMuxId = RCC_CLK_MUX_SDMMC1_PLL2R      },
#endif /* SDMMC1 */
#if defined(SDMMC2)
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL1Q      , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SDMMC2   , .ClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      },
  { .PeriphId = RCC_PERIPH_SDMMC2_PLL2R      , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .BlockId = RCC_BLOCK_SDMMC2   , .ClkMuxId = RCC_CLK_MUX_SDMMC2_PLL2R      },
#endif /* SDMMC2 */

#if defined(FMC_BANK1)
  { .PeriphId = RCC_PERIPH_FMC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_FMC      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* FMC_BANK1 */

#if defined(OCTOSPI1)
  { .PeriphId = RCC_PERIPH_OCTOSPI1_HCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_PLL1Q    , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_PLL1Q    },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_PLL2R    , .ClkSrcId = RCC_CLK_SRC_PLL2QCLK, .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_PLL2R    },
  { .PeriphId = RCC_PERIPH_OCTOSPI1_LPCLK    , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_LPCLK    },
#endif /* OCTOSPI1 */

#if defined(USB_DRD_FS)
  { .PeriphId = RCC_PERIPH_USB_PLL1Q         , .ClkSrcId = RCC_CLK_SRC_PLL1PCLK, .BlockId = RCC_BLOCK_USB      , .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q         },
  { .PeriphId = RCC_PERIPH_USB_PLL3Q         , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_USB      , .ClkMuxId = RCC_CLK_MUX_USB_PLL3Q         },
  { .PeriphId = RCC_PERIPH_USB_HSI48         , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_USB      , .ClkMuxId = RCC_CLK_MUX_USB_HSI48         },
#endif /* USB_OTG_HS */

#if defined(UCPD1)
  { .PeriphId = RCC_PERIPH_UCPD1             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UCPD1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* UCPD1 */

#if defined(ETH)
  { .PeriphId = RCC_PERIPH_ETH               , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_ETH      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
  { .PeriphId = RCC_PERIPH_ETH_TX            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_ETH_TX   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
  { .PeriphId = RCC_PERIPH_ETH_RX            , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_ETH_RX   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* ETH */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(PSSI) || defined(DCMI)
  { .PeriphId = RCC_PERIPH_DCMI_PSSI         , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DCMI_PSSI, .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* PSSI OR DCMI */

#if defined(SAI1)
  { .PeriphId = RCC_PERIPH_SAI1_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P        },
  { .PeriphId = RCC_PERIPH_SAI1_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P        },
  { .PeriphId = RCC_PERIPH_SAI1_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .PeriphId = RCC_PERIPH_SAI1_CKIN         , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN         },
  { .PeriphId = RCC_PERIPH_SAI1_LPCLK        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_LPCLK        },
#endif /* SAI1 */
#if defined(SAI2)
  { .PeriphId = RCC_PERIPH_SAI2_PLL2P        , .ClkSrcId = RCC_CLK_SRC_PLL2PCLK, .BlockId = RCC_BLOCK_SAI2     , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P        },
  { .PeriphId = RCC_PERIPH_SAI2_PLL3P        , .ClkSrcId = RCC_CLK_SRC_PLL3PCLK, .BlockId = RCC_BLOCK_SAI2     , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P        },
  { .PeriphId = RCC_PERIPH_SAI2_PLL1Q        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SAI2     , .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .PeriphId = RCC_PERIPH_SAI2_CKIN         , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxId = RCC_CLK_MUX_SAI2_CKIN         },
  { .PeriphId = RCC_PERIPH_SAI2_LPCLK        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxId = RCC_CLK_MUX_SAI2_LPCLK        },
#endif /* SAI2 */

#if defined(CEC)
  { .PeriphId = RCC_PERIPH_CEC_LSE           , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_CEC      , .ClkMuxId = RCC_CLK_MUX_CEC_LSE           },
  { .PeriphId = RCC_PERIPH_CEC_CSI_DIV       , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_CEC      , .ClkMuxId = RCC_CLK_MUX_CEC_CSI_DIV       },
  { .PeriphId = RCC_PERIPH_CEC_LSI           , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_CEC      , .ClkMuxId = RCC_CLK_MUX_CEC_LSI           },
#endif /* CEC */

  /*--------------------------------- Analog ---------------------------------*/

#if defined(VREFBUF)
  { .PeriphId = RCC_PERIPH_VREF              , .ClkSrcId = RCC_CLK_SRC_APB3CLK , .BlockId = RCC_BLOCK_VREF     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* VREFBUF */

#if defined(ADC1) || defined(ADC2)
  { .PeriphId = RCC_PERIPH_ADC_HCLK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_ADC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .PeriphId = RCC_PERIPH_ADC_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_ADC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    },
  { .PeriphId = RCC_PERIPH_ADC_PLL2R         , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK, .BlockId = RCC_BLOCK_ADC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     },
  { .PeriphId = RCC_PERIPH_ADC_HSE           , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_ADC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       },
  { .PeriphId = RCC_PERIPH_ADC_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_ADC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI       },
  { .PeriphId = RCC_PERIPH_ADC_CSI           , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_ADC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_CSI       },
#endif /* ADC1 */

#if defined(DAC1)
  { .PeriphId = RCC_PERIPH_DAC_HCLK          , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .PeriphId = RCC_PERIPH_DAC_SYSCLK        , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    },
  { .PeriphId = RCC_PERIPH_DAC_PLL2R         , .ClkSrcId = RCC_CLK_SRC_PLL2RCLK, .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     },
  { .PeriphId = RCC_PERIPH_DAC_HSE           , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       },
  { .PeriphId = RCC_PERIPH_DAC_HSI           , .ClkSrcId = RCC_CLK_SRC_HSI64CLK, .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI       },
  { .PeriphId = RCC_PERIPH_DAC_CSI           , .ClkSrcId = RCC_CLK_SRC_CSI4CLK , .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_ADC_DAC_CSI       },

  { .PeriphId = RCC_PERIPH_DAC_SAH_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       },
  { .PeriphId = RCC_PERIPH_DAC_SAH_LSI       , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_DAC      , .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSI       },
#endif /* DAC1 */

  /*-------------------------------- Security --------------------------------*/

#if defined(AES)
  { .PeriphId = RCC_PERIPH_AES               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_AES      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* AES */

#if defined(GTZC_TZSC1)
  { .PeriphId = RCC_PERIPH_GTZC1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GTZC1    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* GTZC1 */

#if defined(HASH)
  { .PeriphId = RCC_PERIPH_HASH              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_HASH     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* HASH */

#if defined(OTFDEC1)
  { .PeriphId = RCC_PERIPH_OTFDEC1           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_OTFDEC1  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* OTFDEC1 */

#if defined(PKA)
  { .PeriphId = RCC_PERIPH_PKA               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_PKA      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* PKA */

#if defined(RNG)
  { .PeriphId = RCC_PERIPH_RNG_HSI48         , .ClkSrcId = RCC_CLK_SRC_HSI48CLK, .BlockId = RCC_BLOCK_RNG      , .ClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .PeriphId = RCC_PERIPH_RNG_PLL1Q         , .ClkSrcId = RCC_CLK_SRC_HSI48CLK, .BlockId = RCC_BLOCK_RNG      , .ClkMuxId = RCC_CLK_MUX_RNG_PLL1Q         },
  { .PeriphId = RCC_PERIPH_RNG_LSE           , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_RNG      , .ClkMuxId = RCC_CLK_MUX_RNG_LSE           },
  { .PeriphId = RCC_PERIPH_RNG_LSI           , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_RNG      , .ClkMuxId = RCC_CLK_MUX_RNG_LSI           },
#endif /* RNG */

#if defined(SAES)
  { .PeriphId = RCC_PERIPH_SAES              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_SAES     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* SAES */

  /*-------------------------------- Computing -------------------------------*/

#if defined(CORDIC)
  { .PeriphId = RCC_PERIPH_CORDIC            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_CORDIC   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* CORDIC */

#if defined(CRC)
  { .PeriphId = RCC_PERIPH_CRC               , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_CRC      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* CRC */

#if defined(FMAC)
  { .PeriphId = RCC_PERIPH_FMAC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_FMAC     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT          },
#endif /* FMAC */
};


/** \brief Configuration registers of RCC peripheral blocks.
 *
 * This array is created to reduce size of configuration.
 */
const rcc_BlockConfigStruct_t           rcc_PeriphBlockConfig[ RCC_BLOCK_LIST_CNT ] =
{
  { .BlockId = RCC_BLOCK_FLASH      , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_FLITFEN      , .LpCtrlMask = RCC_AHB1LPENR_FLITFLPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_SBS        , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_SBSEN        , .LpCtrlMask = RCC_APB3LPENR_SBSLPEN        , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_RTC        , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_RTCAPBEN     , .LpCtrlMask = RCC_APB3LPENR_RTCAPBLPEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
  { .BlockId = RCC_BLOCK_CRS        , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_CRSEN       , .LpCtrlMask = RCC_APB1LLPENR_CRSLPEN       , .RstCtrlMask = RCC_APB1LRSTR_CECRST       },

#if defined(DTS)
  { .BlockId = RCC_BLOCK_DTS        , .ClkBusId = RCC_CLK_BUS_APB1_2    , .StateMask = RCC_APB1HENR_DTSEN       , .LpCtrlMask = RCC_APB1HLPENR_DTSLPEN       , .RstCtrlMask = RCC_APB1HRSTR_DTSRST       },
#endif /* DTS */

#if defined(DCACHE1)
  { .BlockId = RCC_BLOCK_DCACHE1    , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_DCACHE1EN    , .LpCtrlMask = RCC_AHB1LPENR_DCACHE1LPEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* DCACHE1 */

  { .BlockId = RCC_BLOCK_RAMCFG     , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_RAMCFGEN     , .LpCtrlMask = RCC_AHB1LPENR_RAMCFGLPEN     , .RstCtrlMask = RCC_AHB1RSTR_RAMCFGRST     },

#if defined(RAMCFG_BKPRAM)
  { .BlockId = RCC_BLOCK_BKPSRAM    , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_BKPRAMEN     , .LpCtrlMask = RCC_AHB1LPENR_BKPRAMLPEN     , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RAMCFG_BKPRAM */

#if defined(RAMCFG_SRAM1)
  { .BlockId = RCC_BLOCK_SRAM1      , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_SRAM1EN      , .LpCtrlMask = RCC_AHB1LPENR_SRAM1LPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RAMCFG_SRAM1 */
#if defined(RAMCFG_SRAM2)
  { .BlockId = RCC_BLOCK_SRAM2      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_SRAM2EN      , .LpCtrlMask = RCC_AHB2LPENR_SRAM2LPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RAMCFG_SRAM2 */
#if defined(RAMCFG_SRAM3)
  { .BlockId = RCC_BLOCK_SRAM3      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_SRAM3EN      , .LpCtrlMask = RCC_AHB2LPENR_SRAM3LPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RAMCFG_SRAM3 */

#if defined(GPDMA1)
  { .BlockId = RCC_BLOCK_GPDMA1     , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_GPDMA1EN     , .LpCtrlMask = RCC_AHB1LPENR_GPDMA1LPEN     , .RstCtrlMask = RCC_AHB1RSTR_GPDMA1RST     },
#endif /* GPDMA1 */
#if defined(GPDMA2)
  { .BlockId = RCC_BLOCK_GPDMA2     , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_GPDMA2EN     , .LpCtrlMask = RCC_AHB1LPENR_GPDMA2LPEN     , .RstCtrlMask = RCC_AHB1RSTR_GPDMA2RST     },
#endif /* GPDMA2 */

#if defined(WWDG)
  { .BlockId = RCC_BLOCK_WWDG       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_WWDGEN      , .LpCtrlMask = RCC_APB1LLPENR_WWDGLPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* WWDG */

#if defined(GPIOA)
  { .BlockId = RCC_BLOCK_GPIOA      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOAEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOALPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOARST      },
#endif /* GPIOA */
#if defined(GPIOB)
  { .BlockId = RCC_BLOCK_GPIOB      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOBEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOBLPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOBRST      },
#endif /* GPIOB */
#if defined(GPIOC)
  { .BlockId = RCC_BLOCK_GPIOC      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOCEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOCLPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOCRST      },
#endif /* GPIOC */
#if defined(GPIOD)
  { .BlockId = RCC_BLOCK_GPIOD      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIODEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIODLPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIODRST      },
#endif /* GPIOD */
#if defined(GPIOE)
  { .BlockId = RCC_BLOCK_GPIOE      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOEEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOELPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOERST      },
#endif /* GPIOE */
#if defined(GPIOF)
  { .BlockId = RCC_BLOCK_GPIOF      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOFEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOFLPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOFRST      },
#endif /* GPIOF */
#if defined(GPIOG)
  { .BlockId = RCC_BLOCK_GPIOG      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOGEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOGLPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOGRST      },
#endif /* GPIOG */
#if defined(GPIOH)
  { .BlockId = RCC_BLOCK_GPIOH      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOHEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOHLPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOHRST      },
#endif /* GPIOG */
#if defined(GPIOI)
  { .BlockId = RCC_BLOCK_GPIOI      , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_GPIOIEN      , .LpCtrlMask = RCC_AHB2LPENR_GPIOILPEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOIRST      },
#endif /* GPIOG */

  /*--------------------------------- Timers ---------------------------------*/

#if defined(TIM1)
  { .BlockId = RCC_BLOCK_TIM1       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_TIM1EN       , .LpCtrlMask = RCC_APB2LPENR_TIM1LPEN       , .RstCtrlMask = RCC_APB2RSTR_TIM1RST       },
#endif /* TIM1 */
#if defined(TIM2)
  { .BlockId = RCC_BLOCK_TIM2       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_TIM2EN      , .LpCtrlMask = RCC_APB1LLPENR_TIM2LPEN      , .RstCtrlMask = RCC_APB1LRSTR_TIM2RST      },
#endif /* TIM2 */
#if defined(TIM3)
  { .BlockId = RCC_BLOCK_TIM3       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_TIM3EN      , .LpCtrlMask = RCC_APB1LLPENR_TIM3LPEN      , .RstCtrlMask = RCC_APB1LRSTR_TIM3RST      },
#endif /* TIM3 */
#if defined(TIM4)
  { .BlockId = RCC_BLOCK_TIM4       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_TIM4EN      , .LpCtrlMask = RCC_APB1LLPENR_TIM4LPEN      , .RstCtrlMask = RCC_APB1LRSTR_TIM4RST      },
#endif /* TIM4 */
#if defined(TIM5)
  { .BlockId = RCC_BLOCK_TIM5       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_TIM5EN      , .LpCtrlMask = RCC_APB1LLPENR_TIM5LPEN      , .RstCtrlMask = RCC_APB1LRSTR_TIM5RST      },
#endif /* TIM5 */
#if defined(TIM6)
  { .BlockId = RCC_BLOCK_TIM6       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_TIM6EN      , .LpCtrlMask = RCC_APB1LLPENR_TIM6LPEN      , .RstCtrlMask = RCC_APB1LRSTR_TIM6RST      },
#endif /* TIM6 */
#if defined(TIM7)
  { .BlockId = RCC_BLOCK_TIM7       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_TIM7EN      , .LpCtrlMask = RCC_APB1LLPENR_TIM7LPEN      , .RstCtrlMask = RCC_APB1LRSTR_TIM7RST      },
#endif /* TIM7 */
#if defined(TIM8)
  { .BlockId = RCC_BLOCK_TIM8       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_TIM8EN       , .LpCtrlMask = RCC_APB2LPENR_TIM8LPEN       , .RstCtrlMask = RCC_APB2RSTR_TIM8RST       },
#endif /* TIM8 */
#if defined(TIM15)
  { .BlockId = RCC_BLOCK_TIM15      , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_TIM15EN      , .LpCtrlMask = RCC_APB2LPENR_TIM15LPEN      , .RstCtrlMask = RCC_APB2RSTR_TIM15RST      },
#endif /* TIM15 */
#if defined(TIM16)
  { .BlockId = RCC_BLOCK_TIM16      , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_TIM16EN      , .LpCtrlMask = RCC_APB2LPENR_TIM16LPEN      , .RstCtrlMask = RCC_APB2RSTR_TIM16RST      },
#endif /* TIM16 */
#if defined(TIM17)
  { .BlockId = RCC_BLOCK_TIM17      , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_TIM17EN      , .LpCtrlMask = RCC_APB2LPENR_TIM17LPEN      , .RstCtrlMask = RCC_APB2RSTR_TIM17RST      },
#endif /* TIM17 */


#if defined(LPTIM1)
  { .BlockId = RCC_BLOCK_LPTIM1     , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_LPTIM1EN     , .LpCtrlMask = RCC_APB3LPENR_LPTIM1LPEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM1RST     },
#endif /* LPTIM1 */
#if defined(LPTIM2)
  { .BlockId = RCC_BLOCK_LPTIM2     , .ClkBusId = RCC_CLK_BUS_APB1_2    , .StateMask = RCC_APB1HENR_LPTIM2EN    , .LpCtrlMask = RCC_APB1HLPENR_LPTIM2LPEN    , .RstCtrlMask = RCC_APB1HRSTR_LPTIM2RST    },
#endif /* LPTIM2 */
#if defined(LPTIM3)
  { .BlockId = RCC_BLOCK_LPTIM3     , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_LPTIM3EN     , .LpCtrlMask = RCC_APB3LPENR_LPTIM3LPEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM3RST     },
#endif /* LPTIM3 */
#if defined(LPTIM4)
  { .BlockId = RCC_BLOCK_LPTIM4     , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_LPTIM4EN     , .LpCtrlMask = RCC_APB3LPENR_LPTIM4LPEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM4RST     },
#endif /* LPTIM4 */
#if defined(LPTIM5)
  { .BlockId = RCC_BLOCK_LPTIM5     , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_LPTIM5EN     , .LpCtrlMask = RCC_APB3LPENR_LPTIM5LPEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM5RST     },
#endif /* LPTIM5 */
#if defined(LPTIM6)
  { .BlockId = RCC_BLOCK_LPTIM6     , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_LPTIM6EN     , .LpCtrlMask = RCC_APB3LPENR_LPTIM6LPEN     , .RstCtrlMask = RCC_APB3RSTR_LPTIM6RST     },
#endif /* LPTIM6 */

  /*------------------------------ Connectivity ------------------------------*/

#if defined(SPI1)
  { .BlockId = RCC_BLOCK_SPI1       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_SPI1EN       , .LpCtrlMask = RCC_APB2LPENR_SPI1LPEN       , .RstCtrlMask = RCC_APB2RSTR_SPI1RST       },
#endif /* SPI1 */
#if defined(SPI2)
  { .BlockId = RCC_BLOCK_SPI2       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_SPI2EN      , .LpCtrlMask = RCC_APB1LLPENR_SPI2LPEN      , .RstCtrlMask = RCC_APB1LRSTR_SPI2RST      },
#endif /* SPI2 */
#if defined(SPI3)
  { .BlockId = RCC_BLOCK_SPI3       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_SPI3EN      , .LpCtrlMask = RCC_APB1LLPENR_SPI3LPEN      , .RstCtrlMask = RCC_APB1LRSTR_SPI3RST      },
#endif /* SPI3 */
#if defined(SPI4)
  { .BlockId = RCC_BLOCK_SPI4       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_SPI4EN       , .LpCtrlMask = RCC_APB2LPENR_SPI4LPEN       , .RstCtrlMask = RCC_APB2RSTR_SPI4RST       },
#endif /* SPI4 */
#if defined(SPI5)
  { .BlockId = RCC_BLOCK_SPI5       , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_SPI5EN       , .LpCtrlMask = RCC_APB3LPENR_SPI5LPEN       , .RstCtrlMask = RCC_APB3RSTR_SPI5RST       },
#endif /* SPI5 */
#if defined(SPI6)
  { .BlockId = RCC_BLOCK_SPI6       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_SPI6EN       , .LpCtrlMask = RCC_APB2LPENR_SPI6LPEN       , .RstCtrlMask = RCC_APB2RSTR_SPI6RST       },
#endif /* SPI6 */


#if defined(I2C1)
  { .BlockId = RCC_BLOCK_I2C1       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_I2C1EN      , .LpCtrlMask = RCC_APB1LLPENR_I2C1LPEN      , .RstCtrlMask = RCC_APB1LRSTR_I2C1RST      },
#endif /* I2C1 */
#if defined(I2C2)
  { .BlockId = RCC_BLOCK_I2C2       , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_I2C2EN      , .LpCtrlMask = RCC_APB1LLPENR_I2C2LPEN      , .RstCtrlMask = RCC_APB1LRSTR_I2C2RST      },
#endif /* I2C2 */
#if defined(I2C3)
  { .BlockId = RCC_BLOCK_I2C3       , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_I2C3EN       , .LpCtrlMask = RCC_APB3LPENR_I2C3LPEN       , .RstCtrlMask = RCC_APB3RSTR_I2C3RST       },
#endif /* I2C3 */
#if defined(I2C4)
  { .BlockId = RCC_BLOCK_I2C4       , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_I2C4EN       , .LpCtrlMask = RCC_APB3LPENR_I2C4LPEN       , .RstCtrlMask = RCC_APB3RSTR_I2C4RST       },
#endif /* I2C4 */


#if defined(USART1)
  { .BlockId = RCC_BLOCK_USART1     , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_USART1EN     , .LpCtrlMask = RCC_APB2LPENR_USART1LPEN     , .RstCtrlMask = RCC_APB2RSTR_USART1RST     },
#endif /* USART1 */
#if defined(USART2)
  { .BlockId = RCC_BLOCK_USART2     , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_USART2EN    , .LpCtrlMask = RCC_APB1LLPENR_USART2LPEN    , .RstCtrlMask = RCC_APB1LRSTR_USART2RST    },
#endif /* USART2 */
#if defined(USART3)
  { .BlockId = RCC_BLOCK_USART3     , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_USART3EN    , .LpCtrlMask = RCC_APB1LLPENR_USART3LPEN    , .RstCtrlMask = RCC_APB1LRSTR_USART3RST    },
#endif /* USART3 */
#if defined(USART6)
  { .BlockId = RCC_BLOCK_USART6     , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_USART6EN    , .LpCtrlMask = RCC_APB1LLPENR_USART6LPEN    , .RstCtrlMask = RCC_APB1LRSTR_USART6RST    },
#endif /* USART6 */
#if defined(USART10)
  { .BlockId = RCC_BLOCK_USART10    , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_USART10EN   , .LpCtrlMask = RCC_APB1LLPENR_USART10LPEN   , .RstCtrlMask = RCC_APB1LRSTR_USART10RST   },
#endif /* USART10 */
#if defined(USART11)
  { .BlockId = RCC_BLOCK_USART11    , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_USART11EN   , .LpCtrlMask = RCC_APB1LLPENR_USART11LPEN   , .RstCtrlMask = RCC_APB1LRSTR_USART11RST   },
#endif /* USART11 */

#if defined(UART4)
  { .BlockId = RCC_BLOCK_UART4      , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_UART4EN     , .LpCtrlMask = RCC_APB1LLPENR_UART4LPEN     , .RstCtrlMask = RCC_APB1LRSTR_UART4RST     },
#endif /* UART4 */
#if defined(UART5)
  { .BlockId = RCC_BLOCK_UART5      , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_UART5EN     , .LpCtrlMask = RCC_APB1LLPENR_UART5LPEN     , .RstCtrlMask = RCC_APB1LRSTR_UART5RST     },
#endif /* USART5 */
#if defined(UART7)
  { .BlockId = RCC_BLOCK_UART7      , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_UART7EN     , .LpCtrlMask = RCC_APB1LLPENR_UART7LPEN     , .RstCtrlMask = RCC_APB1LRSTR_UART7RST     },
#endif /* UART7 */
#if defined(UART8)
  { .BlockId = RCC_BLOCK_UART8      , .ClkBusId = RCC_CLK_BUS_APB1_1    , .StateMask = RCC_APB1LENR_UART8EN     , .LpCtrlMask = RCC_APB1LLPENR_UART8LPEN     , .RstCtrlMask = RCC_APB1LRSTR_UART8RST     },
#endif /* UART8 */
#if defined(UART9)
  { .BlockId = RCC_BLOCK_UART9      , .ClkBusId = RCC_CLK_BUS_APB1_2    , .StateMask = RCC_APB1HENR_UART9EN     , .LpCtrlMask = RCC_APB1HLPENR_UART9LPEN     , .RstCtrlMask = RCC_APB1HRSTR_UART9RST     },
#endif /* UART9 */
#if defined(UART12)
  { .BlockId = RCC_BLOCK_UART12     , .ClkBusId = RCC_CLK_BUS_APB1_2    , .StateMask = RCC_APB1HENR_UART12EN    , .LpCtrlMask = RCC_APB1HLPENR_UART12LPEN    , .RstCtrlMask = RCC_APB1HRSTR_UART12RST    },
#endif /* UART12 */

#if defined(LPUART1)
  { .BlockId = RCC_BLOCK_LPUART1    , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_LPUART1EN    , .LpCtrlMask = RCC_APB3LPENR_LPUART1LPEN    , .RstCtrlMask = RCC_APB3RSTR_LPUART1RST    },
#endif /* LPUART1 */

#if defined(FDCAN1)
  { .BlockId = RCC_BLOCK_FDCAN      , .ClkBusId = RCC_CLK_BUS_APB1_2    , .StateMask = RCC_APB1HENR_FDCANEN     , .LpCtrlMask = RCC_APB1HLPENR_FDCANLPEN     , .RstCtrlMask = RCC_APB1HRSTR_FDCANRST     },
#endif /* FDCAN1 */

#if defined(SDMMC1)
  { .BlockId = RCC_BLOCK_SDMMC1     , .ClkBusId = RCC_CLK_BUS_AHB4      , .StateMask = RCC_AHB4ENR_SDMMC1EN     , .LpCtrlMask = RCC_AHB4LPENR_SDMMC1LPEN     , .RstCtrlMask = RCC_AHB4RSTR_SDMMC1RST     },
#endif /* SDMMC1 */
#if defined(SDMMC2)
  { .BlockId = RCC_BLOCK_SDMMC2     , .ClkBusId = RCC_CLK_BUS_AHB4      , .StateMask = RCC_AHB4ENR_SDMMC2EN     , .LpCtrlMask = RCC_AHB4LPENR_SDMMC2LPEN     , .RstCtrlMask = RCC_AHB4RSTR_SDMMC2RST     },
#endif /* SDMMC2 */

#if defined(FMC_BANK1)
  { .BlockId = RCC_BLOCK_FMC        , .ClkBusId = RCC_CLK_BUS_AHB4      , .StateMask = RCC_AHB4ENR_FMCEN        , .LpCtrlMask = RCC_AHB4LPENR_FMCLPEN        , .RstCtrlMask = RCC_AHB4RSTR_FMCRST        },
#endif /* FMC_BANK1 */

#if defined(OCTOSPI1)
  { .BlockId = RCC_BLOCK_OCTOSPI1   , .ClkBusId = RCC_CLK_BUS_AHB4      , .StateMask = RCC_AHB4ENR_OCTOSPI1EN   , .LpCtrlMask = RCC_AHB4LPENR_OCTOSPI1LPEN   , .RstCtrlMask = RCC_AHB4RSTR_OCTOSPI1RST   },
#endif /* OCTOSPI1 */

#if defined(USB_DRD_FS)
  { .BlockId = RCC_BLOCK_USB        , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_USBEN        , .LpCtrlMask = RCC_APB2LPENR_USBLPEN        , .RstCtrlMask = RCC_APB2RSTR_USBRST        },
#endif /* USB_OTG_HS */

#if defined(UCPD1)
  { .BlockId = RCC_BLOCK_UCPD1      , .ClkBusId = RCC_CLK_BUS_APB1_2    , .StateMask = RCC_APB1HENR_UCPD1EN     , .LpCtrlMask = RCC_APB1HLPENR_UCPD1LPEN     , .RstCtrlMask = RCC_APB1HRSTR_UCPD1RST     },
#endif /* UCPD1 */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(PSSI) || defined(DCMI)
  { .BlockId = RCC_BLOCK_DCMI_PSSI  , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_DCMI_PSSIEN  , .LpCtrlMask = RCC_AHB2LPENR_DCMI_PSSILPEN  , .RstCtrlMask = RCC_AHB2RSTR_DCMI_PSSIRST  },
#endif /* PSSI OR DCMI */

#if defined(SAI1)
  { .BlockId = RCC_BLOCK_SAI1       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2LPENR_SAI1LPEN       , .RstCtrlMask = RCC_APB2RSTR_SAI1RST       },
#endif /* SAI1 */
#if defined(SAI2)
  { .BlockId = RCC_BLOCK_SAI2       , .ClkBusId = RCC_CLK_BUS_APB2      , .StateMask = RCC_APB2ENR_SAI2EN       , .LpCtrlMask = RCC_APB2LPENR_SAI2LPEN       , .RstCtrlMask = RCC_APB2RSTR_SAI2RST       },
#endif /* SAI2 */

  /*--------------------------------- Analog ---------------------------------*/

#if defined(VREFBUF)
  { .BlockId = RCC_BLOCK_VREF       , .ClkBusId = RCC_CLK_BUS_APB3      , .StateMask = RCC_APB3ENR_VREFEN       , .LpCtrlMask = RCC_APB3LPENR_VREFLPEN       , .RstCtrlMask = RCC_APB3RSTR_VREFRST       },
#endif /* VREFBUF */

#if defined(ADC1) || defined(ADC2)
  { .BlockId = RCC_BLOCK_ADC        , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_ADCEN        , .LpCtrlMask = RCC_AHB2LPENR_ADCLPEN        , .RstCtrlMask = RCC_AHB2RSTR_ADCRST        },
#endif /* ADC1 */

#if defined(DAC1)
  { .BlockId = RCC_BLOCK_DAC        , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_DAC1EN       , .LpCtrlMask = RCC_AHB2LPENR_DAC1LPEN       , .RstCtrlMask = RCC_AHB2RSTR_DAC1RST       },
#endif /* DAC1 */

  /*-------------------------------- Security --------------------------------*/

#if defined(AES)
  { .BlockId = RCC_BLOCK_AES        , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_AESEN        , .LpCtrlMask = RCC_AHB2LPENR_AESLPEN        , .RstCtrlMask = RCC_AHB2RSTR_AESRST        },
#endif /* AES */

#if defined(GTZC_TZSC1)
  { .BlockId = RCC_BLOCK_GTZC1      , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_TZSC1EN      , .LpCtrlMask = RCC_AHB1LPENR_TZSC1LPEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* GTZC1 */

#if defined(HASH)
  { .BlockId = RCC_BLOCK_HASH       , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_HASHEN       , .LpCtrlMask = RCC_AHB2LPENR_HASHLPEN       , .RstCtrlMask = RCC_AHB2RSTR_HASHRST       },
#endif /* HASH */

#if defined(OTFDEC1)
  { .BlockId = RCC_BLOCK_OTFDEC1    , .ClkBusId = RCC_CLK_BUS_AHB4      , .StateMask = RCC_AHB4ENR_OTFDEC1EN    , .LpCtrlMask = RCC_AHB4LPENR_OTFDEC1LPEN    , .RstCtrlMask = RCC_AHB4RSTR_OTFDEC1RST    },
#endif /* OTFDEC1 */

#if defined(PKA)
  { .BlockId = RCC_BLOCK_PKA        , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_PKAEN        , .LpCtrlMask = RCC_AHB2LPENR_PKALPEN        , .RstCtrlMask = RCC_AHB2RSTR_PKARST        },
#endif /* PKA */

#if defined(RNG)
  { .BlockId = RCC_BLOCK_RNG        , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_RNGEN        , .LpCtrlMask = RCC_AHB2LPENR_RNGLPEN        , .RstCtrlMask = RCC_AHB2RSTR_RNGRST        },
#endif /* RNG */

#if defined(SAES)
  { .BlockId = RCC_BLOCK_SAES       , .ClkBusId = RCC_CLK_BUS_AHB2      , .StateMask = RCC_AHB2ENR_SAESEN       , .LpCtrlMask = RCC_AHB2LPENR_SAESLPEN       , .RstCtrlMask = RCC_AHB2RSTR_SAESRST       },
#endif /* SAES */

  /*-------------------------------- Computing -------------------------------*/

#if defined(CORDIC)
  { .BlockId = RCC_BLOCK_CORDIC     , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_CORDICEN     , .LpCtrlMask = RCC_AHB1LPENR_CORDICLPEN     , .RstCtrlMask = RCC_AHB1RSTR_CORDICRST     },
#endif /* CORDIC */

#if defined(CRC)
  { .BlockId = RCC_BLOCK_CRC        , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_CRCEN        , .LpCtrlMask = RCC_AHB1LPENR_CRCLPEN        , .RstCtrlMask = RCC_AHB1RSTR_CRCRST        },
#endif /* CRC */

#if defined(FMAC)
  { .BlockId = RCC_BLOCK_FMAC       , .ClkBusId = RCC_CLK_BUS_AHB1      , .StateMask = RCC_AHB1ENR_FMACEN       , .LpCtrlMask = RCC_AHB1LPENR_FMACLPEN       , .RstCtrlMask = RCC_AHB1RSTR_FMACRST       },
#endif /* FMAC */
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
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_FLASH );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PeriphActive( RCC_PERIPH_SBS );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashPrefetchActive();
        }

        /* Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral */
        LL_PWR_DisableUCPDDeadBattery();

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

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_HseClk( clockConfig->HSE_Frequency_Hz );
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
            retState = Rcc_ClkBus_Set_APB3Divider( clockConfig->APB3_Divider );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
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

            Rcc_ClkBus_Get_SysClk( &sysClk );

            LL_SetFlashLatency( sysClk );

            Rcc_Set_SysTickInterval( clockConfig->SysTickInterval );

            LL_SetSystemCoreClock( sysClk );
        }

        for( rcc_ClkOut_Id_t clkOutId = 0u; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
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
        clockConfig->SysTickInterval   = 1u;

        clockConfig->McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLK_SOURCE_NONE;
        clockConfig->McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 1u;

        clockConfig->McoConfig[ RCC_CLK_OUT_MCO2 ].ClockSource  = RCC_CLK_SOURCE_NONE;
        clockConfig->McoConfig[ RCC_CLK_OUT_MCO2 ].ClockDivider = 1u;

        clockConfig->McoConfig[ RCC_CLK_OUT_LSCO ].ClockSource  = RCC_CLK_SOURCE_NONE;
        clockConfig->McoConfig[ RCC_CLK_OUT_LSCO ].ClockDivider = 1u;


        clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSI;
        clockConfig->Pll_Config[ RCC_PLL_1 ].M_Divider    = 1u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].N_Multiplier = 20u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].P_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].Q_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].R_Divider    = 2u;

        clockConfig->Pll_Config[ RCC_PLL_2 ].Pll_Source   = RCC_PLL_SRC_HSI;
        clockConfig->Pll_Config[ RCC_PLL_2 ].M_Divider    = 1u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].N_Multiplier = 20u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].P_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].Q_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_2 ].R_Divider    = 2u;

        clockConfig->Pll_Config[ RCC_PLL_3 ].Pll_Source   = RCC_PLL_SRC_HSI;
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
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PeriphActive( rcc_PeriphId_t periphId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_ClkMuxId_t clkMuxId = rcc_ConfigStruct[ periphId ].ClkMuxId;

        if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
        {
            Rcc_ClkMux_Set_ClkActive( clkMuxId );
        }
        else
        {
            /* Peripheral does not support clock multiplexing. */
        }

        rcc_BlockList_t blockId  = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId = rcc_PeriphBlockConfig[ blockId ].ClkBusId;

        if( RCC_CLK_BUS_CNT > clkBusId )
        {
            rcc_RegId_t stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;
            uint32_t    stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;

            /* Activate peripheral by setting "1" to corresponding register */
            Rcc_Set_RegBit( stateRegId, stateMask );

            /* Activate peripheral clock */
            for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = Rcc_Get_RegBit( stateRegId, stateMask);

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
 * returned state is "OK". Otherwise returns error.
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

        Rcc_Reset_RegBit( stateRegId, stateMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( stateRegId, stateMask);

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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != funcState )    )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        stateMask  = rcc_PeriphBlockConfig[ blockId ].StateMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].EnableRegId;

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
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PeriphClk( rcc_PeriphId_t periphId, rcc_FreqHz_t * const periphClk )
{
    rcc_RequestState_t returnState = RCC_REQUEST_ERROR;

    if( ( RCC_PERIPH_ID_CNT > periphId  ) &&
        ( RCC_NULL_PTR     != periphClk )    )
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        Rcc_Set_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( stateRegId, resetMask);

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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_ID_CNT > periphId )
    {
        rcc_BlockList_t blockId    = rcc_ConfigStruct[ periphId ].BlockId;
        rcc_ClkBusId_t  clkBusId   = rcc_PeriphBlockConfig[ blockId ].ClkBusId;
        uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].RstCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].ResetRegId;

        Rcc_Reset_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( stateRegId, resetMask);

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
 * \param periphId   (in): ID of required peripheral to be inactive in low power mode
 * \param funcState (out): State of peripheral activation
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ResetState( rcc_PeriphId_t periphId, rcc_FunctionState_t *funcState )
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

        regValue = Rcc_Get_RegBit( stateRegId, stateMask);

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
        uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        Rcc_Set_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( stateRegId, resetMask);

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
        uint32_t        resetMask  = rcc_PeriphBlockConfig[ blockId ].LpCtrlMask;
        rcc_RegId_t     stateRegId = rcc_ClkBusConfigStruct[ clkBusId ].SleepRegId;

        Rcc_Reset_RegBit( stateRegId, resetMask );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            registerValue = Rcc_Get_RegBit( stateRegId, resetMask);

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
 * \param periphId   (in): ID of required peripheral to be inactive in low power mode
 * \param funcState (out): State of peripheral activation
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_SleepState( rcc_PeriphId_t periphId, rcc_FunctionState_t *funcState )
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
        case RCC_CLK_BUS_AHB4:
            retState = Rcc_ClkBus_Set_AHBDivider( clkBusDivider );
            break;

        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2:
            retState = Rcc_ClkBus_Set_APB1Divider( clkBusDivider );
            break;

        case RCC_CLK_BUS_APB2:
            retState = Rcc_ClkBus_Set_APB2Divider( clkBusDivider );
            break;

        case RCC_CLK_BUS_APB3:
            retState = Rcc_ClkBus_Set_APB3Divider( clkBusDivider );
            break;

        default:
            retState = RCC_REQUEST_ERROR;
            break;
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
        case RCC_CLK_BUS_AHB4:
            retState = Rcc_ClkBus_Get_AHBDivider( (rcc_AHB_Div_t*) clkBusDivider );
            break;

        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2:
            retState = Rcc_ClkBus_Get_APB1Divider( (rcc_APB1_Div_t*) clkBusDivider );
            break;

        case RCC_CLK_BUS_APB2:
            retState = Rcc_ClkBus_Get_APB2Divider( (rcc_APB2_Div_t*) clkBusDivider );
            break;

        case RCC_CLK_BUS_APB3:
            retState = Rcc_ClkBus_Get_APB3Divider( (rcc_APB3_Div_t*) clkBusDivider );
            break;

        default:
            retState = RCC_REQUEST_ERROR;
            break;
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
rcc_RequestState_t Rcc_Get_ClkBusFreq( rcc_ClkBusId_t clkBusId, rcc_FreqHz_t * const clkBusFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    switch( clkBusId )
    {
        case RCC_CLK_BUS_AHB1:
        case RCC_CLK_BUS_AHB2:
        case RCC_CLK_BUS_AHB4:
            retState = Rcc_ClkBus_Get_AHBClk( clkBusFreq );
            break;

        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2:
            retState = Rcc_ClkBus_Get_APB1Clk( clkBusFreq );
            break;

        case RCC_CLK_BUS_APB2:
            retState = Rcc_ClkBus_Get_APB2Clk( clkBusFreq );
            break;

        case RCC_CLK_BUS_APB3:
            retState = Rcc_ClkBus_Get_APB3Clk( clkBusFreq );
            break;

        default:
            retState = RCC_REQUEST_ERROR;
            break;
    }

    return ( retState );
}

/*------------------ Power range and latency configuration -------------------*/

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

        retState = Rcc_ClkBus_Get_SysClk( &systemFreq );

        SysTick_Config( systemFreq / ( 8u * ticksCnt ) );

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

        retState = Rcc_ClkBus_Get_SysClk( &systemFreq );

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

/* =========================== LOCAL FUNCTIONS ============================== */

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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = 0u;

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkFreq )
    {
        *clkFreq = 0u;

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
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
static rcc_RequestState_t Rcc_Get_ExpectedSysClkFrequency( rcc_ConfigStruct_t * const clockConfig, rcc_FreqHz_t *sysClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllSrcFreq = 0u;

    if( ( RCC_NULL_PTR != clockConfig ) &&
        ( RCC_NULL_PTR != sysClk      )    )
    {
        if( RCC_SYSTEM_CLOCK_SOURCE_PLL == clockConfig->SystemClockSource )
        {
            if( RCC_PLL_SRC_HSI == clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source )
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
                 *sysClk = __LL_RCC_CALC_PLL1CLK_P_FREQ( pllSrcFreq,
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
