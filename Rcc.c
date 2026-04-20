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
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define RCC_MAJOR_VERSION                       ( 1u )
/** Value of minor version of SW module */
#define RCC_MINOR_VERSION                       ( 0u )
/** Value of patch version of SW module */
#define RCC_PATCH_VERSION                       ( 0u )

/** Maximal wait time for configuration request confirmation */
#define RCC_TIMEOUT_RAW                         ( 0x84FCB )

/** Default value of HSI trimming */
#define RCC_HSI_DEFAULT_TRIM_VALUE              ( 64u )

/** Threshold frequency of SCLK in Hz, when "boost" mode shall be activated */
#define RCC_PWR_RANGE_THRESHOLD_HZ              ( 80000000u )

/** Threshold frequency of SCLK in Hz, when wait state shall be incremented */
#define RCC_FLASH_WAIT_STATE_THRESHOLD_HZ       ( 16000000u )

/** Identification of unsupported functionality */
#define RCC_UNSUPPORTED_FUNCTION                ( 0xFF )

/** HCLK frequency to set FLASH latency 1 in power scale 1 boost mode */
#define RCC_SCALE1_LATENCY1_BOOST_FREQ          (  34000000u )
/** HCLK frequency to set FLASH latency 2 in power scale 1 boost mode */
#define RCC_SCALE1_LATENCY2_BOOST_FREQ          (  68000000u )
/** HCLK frequency to set FLASH latency 3 in power scale 1 boost mode */
#define RCC_SCALE1_LATENCY3_BOOST_FREQ          ( 102000000u )
/** HCLK frequency to set FLASH latency 4 in power scale 1 boost mode */
#define RCC_SCALE1_LATENCY4_BOOST_FREQ          ( 136000000u )
/** HCLK frequency to set FLASH latency 5 in power scale 1 boost mode */
#define RCC_SCALE1_LATENCY5_BOOST_FREQ          ( 170000000u )

/** HCLK frequency to set FLASH latency 1 in power scale 1 normal mode */
#define RCC_SCALE1_LATENCY1_FREQ                (  30000000u )
/** HCLK frequency to set FLASH latency 2 in power scale 1 normal mode */
#define RCC_SCALE1_LATENCY2_FREQ                (  60000000u )
/** HCLK frequency to set FLASH latency 3 in power scale 1 normal mode */
#define RCC_SCALE1_LATENCY3_FREQ                (  90000000u )
/** HCLK frequency to set FLASH latency 4 in power scale 1 normal mode */
#define RCC_SCALE1_LATENCY4_FREQ                ( 120000000u )
/** HCLK frequency to set FLASH latency 5 in power scale 1 normal mode */
#define RCC_SCALE1_LATENCY5_FREQ                ( 150000000u )

/** HCLK frequency to set FLASH latency 1 in power scale 2 */
#define RCC_SCALE2_LATENCY1_FREQ                ( 12000000u )
/** HCLK frequency to set FLASH latency 2 in power scale 2 */
#define RCC_SCALE2_LATENCY2_FREQ                ( 24000000u )
/** HCLK frequency to set FLASH latency 3 in power scale 2 */
#define RCC_SCALE2_LATENCY3_FREQ                ( 26000000u )

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

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/* --------------------- Multipliers/Dividers arrays -------------------------*/

/* The most disgusting part in whole project. Definition of external variables
 * created by STM!!! Shame on you ST! */
uint32_t      SystemCoreClock    = 170000000U;
const uint8_t AHBPrescTable[16u] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8u]  = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Configuration array of registers used by peripheral buses */
const rcc_ClkBusConfigStruct_t         rcc_ClkBusConfigStruct[ RCC_CLK_BUS_CNT ] =
{
    { .ClkBusId = RCC_CLK_BUS_AHB1  , .EnableRegId = RCC_REG_AHB1ENR , .SleepRegId = RCC_REG_AHB1SMENR , .ResetRegId = RCC_REG_AHB1RSTR  },
    { .ClkBusId = RCC_CLK_BUS_AHB2  , .EnableRegId = RCC_REG_AHB2ENR , .SleepRegId = RCC_REG_AHB2SMENR , .ResetRegId = RCC_REG_AHB2RSTR  },
    { .ClkBusId = RCC_CLK_BUS_AHB3  , .EnableRegId = RCC_REG_AHB3ENR , .SleepRegId = RCC_REG_AHB3SMENR , .ResetRegId = RCC_REG_AHB3RSTR  },
    { .ClkBusId = RCC_CLK_BUS_APB1_1, .EnableRegId = RCC_REG_APB1ENR1, .SleepRegId = RCC_REG_APB1SMENR1, .ResetRegId = RCC_REG_APB1RSTR1 },
    { .ClkBusId = RCC_CLK_BUS_APB1_2, .EnableRegId = RCC_REG_APB1ENR2, .SleepRegId = RCC_REG_APB1SMENR2, .ResetRegId = RCC_REG_APB1RSTR2 },
    { .ClkBusId = RCC_CLK_BUS_APB2  , .EnableRegId = RCC_REG_APB2ENR , .SleepRegId = RCC_REG_APB2SMENR , .ResetRegId = RCC_REG_APB2RSTR  }
};


const rcc_ClkSrcConfigStruct_t rcc_PeriphClkSrcConfig[RCC_CLK_SRC_CNT] =
{
  { .PeriphClkSrcId = RCC_CLK_SRC_SYSCLK   , .ClkSrcCallback = Rcc_ClkBus_Get_SysClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1RCLK , .ClkSrcCallback = Rcc_Pll_Get_1_RClk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1QCLK , .ClkSrcCallback = Rcc_Pll_Get_1_QClk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_PLL1PCLK , .ClkSrcCallback = Rcc_Pll_Get_1_PClk      },
  { .PeriphClkSrcId = RCC_CLK_SRC_AHBCLK   , .ClkSrcCallback = Rcc_ClkBus_Get_AHBClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB1CLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB1Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_APB2CLK  , .ClkSrcCallback = Rcc_ClkBus_Get_APB2Clk  },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI16CLK , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi16Clk },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSI48CLK , .ClkSrcCallback = Rcc_ClkSrc_Get_Hsi48Clk },
  { .PeriphClkSrcId = RCC_CLK_SRC_HSECLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_HseClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSICLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_LsiClk   },
  { .PeriphClkSrcId = RCC_CLK_SRC_LSECLK   , .ClkSrcCallback = Rcc_ClkSrc_Get_LseClk   },
};


/** \brief Configuration array of MCU peripherals. */
const rcc_PeriphConfigStruct_t          rcc_ConfigStruct[ RCC_PERIPH_LIST_CNT ] =
{
#if defined(SYSCFG)
    { .PeriphId = RCC_PERIPH_SYSCFG           , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SYSCFG   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* SYSCFG */


#if defined(PWR)
    { .PeriphId = RCC_PERIPH_PWR              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_PWR      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* PWR */


#if defined(RCC_APB1ENR1_RTCAPBEN)
    { .PeriphId = RCC_PERIPH_RTC_HSE_DIV32    , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32 },
    { .PeriphId = RCC_PERIPH_RTC_LSE          , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_RTC_LSE       },
    { .PeriphId = RCC_PERIPH_RTC_LSI          , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_RTC      , .ClkMuxId = RCC_CLK_MUX_RTC_LSI       },
#endif /* RCC_PERIPH_APB1ENR1_RTCAPBEN */
#if defined(WWDG)
    { .PeriphId = RCC_PERIPH_WWDG             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_WWDG     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* WWDG */


#if defined(DMA1)
    { .PeriphId = RCC_PERIPH_DMA1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DMA1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DMA1 */
#if defined(DMA2)
    { .PeriphId = RCC_PERIPH_DMA2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DMA2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DMA2 */
#if defined(DMAMUX1)
    { .PeriphId = RCC_PERIPH_DMAMUX1          , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DMAMUX1  , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DMAMUX1 */


#if defined(FLASH)
    { .PeriphId = RCC_PERIPH_FLASH            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_FLASH    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* FLASH */
#if defined(FMC_R_BASE)
    { .PeriphId = RCC_PERIPH_FMC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_FMC      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* FMC_R_BASE */
#if defined(QUADSPI)
    { .PeriphId = RCC_PERIPH_QUADSPI_SYSCLK   , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_QUADSPI  , .ClkMuxId = RCC_CLK_MUX_QUADSPI_SYSCLK},
    { .PeriphId = RCC_PERIPH_QUADSPI_PLLQ     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_QUADSPI  , .ClkMuxId = RCC_CLK_MUX_QUADSPI_PLLQ  },
    { .PeriphId = RCC_PERIPH_QUADSPI_HSI16    , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_QUADSPI  , .ClkMuxId = RCC_CLK_MUX_QUADSPI_HSI16 },
#endif /* QUADSPI */

#if defined(ADC12_COMMON)
    { .PeriphId = RCC_PERIPH_ADC12_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_ADC12    , .ClkMuxId = RCC_CLK_MUX_ADC12_SYSCLK  },
    { .PeriphId = RCC_PERIPH_ADC12_PLLP       , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_ADC12    , .ClkMuxId = RCC_CLK_MUX_ADC12_PLLP    },
#endif /* ADC12_COMMON */

#if defined(ADC345_COMMON)
    { .PeriphId = RCC_PERIPH_ADC345_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_ADC345   , .ClkMuxId = RCC_CLK_MUX_ADC345_SYSCLK },
    { .PeriphId = RCC_PERIPH_ADC345_PLLP      , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_ADC345   , .ClkMuxId = RCC_CLK_MUX_ADC345_PLLP   },
#endif /* ADC345_COMMON */


#if defined(DAC1)
    { .PeriphId = RCC_PERIPH_DAC1             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DAC1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DAC1 */
#if defined(DAC2)
    { .PeriphId = RCC_PERIPH_DAC2             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DAC2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DAC2 */
#if defined(DAC3)
    { .PeriphId = RCC_PERIPH_DAC3             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DAC3     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DAC3 */
#if defined(DAC4)
    { .PeriphId = RCC_PERIPH_DAC4             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_DAC4     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* DAC4 */


#if defined(CORDIC)
    { .PeriphId = RCC_PERIPH_CORDIC           , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_CORDIC   , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* CORDIC */
#if defined(FMAC)
    { .PeriphId = RCC_PERIPH_FMAC             , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_FMAC     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* FMAC */


#if defined(CRC)
    { .PeriphId = RCC_PERIPH_CRC              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_CRC      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* CRC */
#if defined(AES)
    { .PeriphId = RCC_PERIPH_AES              , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_AES      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* AES */
#if defined(RNG)
    { .PeriphId = RCC_PERIPH_RNG_PLLQ         , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_RNG      , .ClkMuxId = RCC_CLK_MUX_USB_RNG_PLLQ  },
    { .PeriphId = RCC_PERIPH_RNG_HSI48        , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_RNG      , .ClkMuxId = RCC_CLK_MUX_USB_RNG_HSI48 },
#endif /* RNG */


#if defined(GPIOA)
    { .PeriphId = RCC_PERIPH_GPIOA            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOA    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOA */
#if defined(GPIOB)
    { .PeriphId = RCC_PERIPH_GPIOB            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOB    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOB */
#if defined(GPIOC)
    { .PeriphId = RCC_PERIPH_GPIOC            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOC    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOC */
#if defined(GPIOD)
    { .PeriphId = RCC_PERIPH_GPIOD            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOD    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOD */
#if defined(GPIOE)
    { .PeriphId = RCC_PERIPH_GPIOE            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOE    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOE */
#if defined(GPIOF)
    { .PeriphId = RCC_PERIPH_GPIOF            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOF    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOF */
#if defined(GPIOG)
    { .PeriphId = RCC_PERIPH_GPIOG            , .ClkSrcId = RCC_CLK_SRC_AHBCLK  , .BlockId = RCC_BLOCK_GPIOG    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* GPIOG */


#if defined(TIM1)
    { .PeriphId = RCC_PERIPH_TIM1             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM1 */
#if defined(TIM2)
    { .PeriphId = RCC_PERIPH_TIM2             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM2     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM2 */
#if defined(TIM3)
    { .PeriphId = RCC_PERIPH_TIM3             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM3     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM3 */
#if defined(TIM4)
    { .PeriphId = RCC_PERIPH_TIM4             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM4     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM4 */
#if defined(TIM5)
    { .PeriphId = RCC_PERIPH_TIM5             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM5     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM5 */
#if defined(TIM6)
    { .PeriphId = RCC_PERIPH_TIM6             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM6     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM6 */
#if defined(TIM7)
    { .PeriphId = RCC_PERIPH_TIM7             , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_TIM7     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM7 */
#if defined(TIM8)
    { .PeriphId = RCC_PERIPH_TIM8             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM8     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM8 */
#if defined(TIM15)
    { .PeriphId = RCC_PERIPH_TIM15            , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM15    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM15 */
#if defined(TIM16)
    { .PeriphId = RCC_PERIPH_TIM16            , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM16    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM16 */
#if defined(TIM17)
    { .PeriphId = RCC_PERIPH_TIM17            , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM17    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM17 */
#if defined(TIM20)
    { .PeriphId = RCC_PERIPH_TIM20            , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_TIM20    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM17 */
#if defined(HRTIM1)
    { .PeriphId = RCC_PERIPH_HRTIM            , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_HRTIM    , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* TIM17 */
#if defined(LPTIM1)
    { .PeriphId = RCC_PERIPH_LPTIM1_APB1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_APB1   },
    { .PeriphId = RCC_PERIPH_LPTIM1_LSI       , .ClkSrcId = RCC_CLK_SRC_LSICLK  , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI    },
    { .PeriphId = RCC_PERIPH_LPTIM1_HSI16     , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI16  },
    { .PeriphId = RCC_PERIPH_LPTIM1_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE    },
#endif /* LPTIM1 */


#if defined(CRS)
    { .PeriphId = RCC_PERIPH_CRS              , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_CRS      , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* CRS */


#if defined(SPI1)
    { .PeriphId = RCC_PERIPH_SPI1             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* SPI1 */
#if defined(SPI2)
    { .PeriphId = RCC_PERIPH_SPI2_I2S2_SYSCLK , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_SPI2_I2S2, .ClkMuxId = RCC_CLK_MUX_I2S_SYSCLK    },
    { .PeriphId = RCC_PERIPH_I2S2_PLLQ        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SPI2_I2S2, .ClkMuxId = RCC_CLK_MUX_I2S_PLLQ      },
    { .PeriphId = RCC_PERIPH_I2S2_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_SPI2_I2S2, .ClkMuxId = RCC_CLK_MUX_I2S_HSI16     },
    { .PeriphId = RCC_PERIPH_I2S2_CKIN        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI2_I2S2, .ClkMuxId = RCC_CLK_MUX_I2S_CKIN      },
#endif /* SPI2 */
#if defined(SPI3)
    { .PeriphId = RCC_PERIPH_SPI3_I2S3_SYSCLK , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_SPI3_I2S3, .ClkMuxId = RCC_CLK_MUX_I2S_SYSCLK    },
    { .PeriphId = RCC_PERIPH_I2S3_PLLQ        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SPI3_I2S3, .ClkMuxId = RCC_CLK_MUX_I2S_PLLQ      },
    { .PeriphId = RCC_PERIPH_I2S3_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_SPI3_I2S3, .ClkMuxId = RCC_CLK_MUX_I2S_HSI16     },
    { .PeriphId = RCC_PERIPH_I2S3_CKIN        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SPI3_I2S3, .ClkMuxId = RCC_CLK_MUX_I2S_CKIN      },
#endif /* SPI3 */
#if defined(SPI4)
    { .PeriphId = RCC_PERIPH_SPI4             , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxId = RCC_CLK_MUX_LIST_CNT      },
#endif /* SPI4 */


#if defined(USART1)
    { .PeriphId = RCC_PERIPH_USART1_APB1      , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_APB2   },
    { .PeriphId = RCC_PERIPH_USART1_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK },
    { .PeriphId = RCC_PERIPH_USART1_HSI16     , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_HSI    },
    { .PeriphId = RCC_PERIPH_USART1_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART1   , .ClkMuxId = RCC_CLK_MUX_USART1_LSE    },
#endif /* USART1 */
#if defined(USART2)
    { .PeriphId = RCC_PERIPH_USART2_APB1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_APB1   },
    { .PeriphId = RCC_PERIPH_USART2_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK },
    { .PeriphId = RCC_PERIPH_USART2_HSI16     , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_HSI    },
    { .PeriphId = RCC_PERIPH_USART2_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART2   , .ClkMuxId = RCC_CLK_MUX_USART2_LSE    },
#endif /* USART2 */
#if defined(USART3)
    { .PeriphId = RCC_PERIPH_USART3_APB1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_APB1   },
    { .PeriphId = RCC_PERIPH_USART3_SYSCLK    , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK },
    { .PeriphId = RCC_PERIPH_USART3_HSI16     , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_HSI    },
    { .PeriphId = RCC_PERIPH_USART3_LSE       , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_USART3   , .ClkMuxId = RCC_CLK_MUX_USART3_LSE    },
#endif /* USART3 */
#if defined(UART4)
    { .PeriphId = RCC_PERIPH_UART4_APB1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_APB1    },
    { .PeriphId = RCC_PERIPH_UART4_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_USART4_SYSCLK },
    { .PeriphId = RCC_PERIPH_UART4_HSI16      , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_HSI     },
    { .PeriphId = RCC_PERIPH_UART4_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART4    , .ClkMuxId = RCC_CLK_MUX_UART4_LSE     },
#endif /* UART4 */
#if defined(UART5)
    { .PeriphId = RCC_PERIPH_UART5_APB1       , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_APB1    },
    { .PeriphId = RCC_PERIPH_UART5_SYSCLK     , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_USART5_SYSCLK },
    { .PeriphId = RCC_PERIPH_UART5_HSI16      , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_HSI16   },
    { .PeriphId = RCC_PERIPH_UART5_LSE        , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_UART5    , .ClkMuxId = RCC_CLK_MUX_UART5_LSE     },
#endif /* UART5 */
#if defined(LPUART1)
    { .PeriphId = RCC_PERIPH_LPUART1_APB1     , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_APB1  },
    { .PeriphId = RCC_PERIPH_LPUART1_SYSCLK   , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK},
    { .PeriphId = RCC_PERIPH_LPUART1_HSI16    , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI16 },
    { .PeriphId = RCC_PERIPH_LPUART1_LSE      , .ClkSrcId = RCC_CLK_SRC_LSECLK  , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE   },
#endif /* LPUART1 */


#if defined(I2C1)
    { .PeriphId = RCC_PERIPH_I2C1_APB1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_APB1     },
    { .PeriphId = RCC_PERIPH_I2C1_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK   },
    { .PeriphId = RCC_PERIPH_I2C1_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_I2C1     , .ClkMuxId = RCC_CLK_MUX_I2C1_HSI16    },
#endif /* I2C1 */
#if defined(I2C2)
    { .PeriphId = RCC_PERIPH_I2C2_APB1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_APB1     },
    { .PeriphId = RCC_PERIPH_I2C2_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK   },
    { .PeriphId = RCC_PERIPH_I2C2_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_I2C2     , .ClkMuxId = RCC_CLK_MUX_I2C2_HSI16    },
#endif /* I2C2 */
#if defined(I2C3)
    { .PeriphId = RCC_PERIPH_I2C3_APB1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_APB1     },
    { .PeriphId = RCC_PERIPH_I2C3_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK   },
    { .PeriphId = RCC_PERIPH_I2C3_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_I2C3     , .ClkMuxId = RCC_CLK_MUX_I2C3_HSI16    },
#endif /* I2C3 */
#if defined(I2C4)
    { .PeriphId = RCC_PERIPH_I2C4_APB1        , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_APB1     },
    { .PeriphId = RCC_PERIPH_I2C4_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_SYSCLK  , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK   },
    { .PeriphId = RCC_PERIPH_I2C4_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_I2C4     , .ClkMuxId = RCC_CLK_MUX_I2C4_HSI16    },
#endif /* I2C4 */


#if defined(USB)
    { .PeriphId = RCC_PERIPH_USB_PLLQ         , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_USB      , .ClkMuxId = RCC_CLK_MUX_USB_RNG_PLLQ  },
    { .PeriphId = RCC_PERIPH_USB_HSI48        , .ClkSrcId = RCC_CLK_SRC_HSI48CLK, .BlockId = RCC_BLOCK_USB      , .ClkMuxId = RCC_CLK_MUX_USB_RNG_HSI48 },
#endif /* USB */


#if defined(FDCAN1)
    { .PeriphId = RCC_PERIPH_FDCAN1_APB1      , .ClkSrcId = RCC_CLK_SRC_APB1CLK , .BlockId = RCC_BLOCK_FDCAN1   , .ClkMuxId = RCC_CLK_MUX_FDCAN_APB1    },
    { .PeriphId = RCC_PERIPH_FDCAN1_PLLQ      , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_FDCAN1   , .ClkMuxId = RCC_CLK_MUX_FDCAN_PLLQ    },
    { .PeriphId = RCC_PERIPH_FDCAN1_HSE       , .ClkSrcId = RCC_CLK_SRC_HSECLK  , .BlockId = RCC_BLOCK_FDCAN1   , .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE     },
#endif /* CAN1 */

#if defined(SAI1)
    { .PeriphId = RCC_PERIPH_SAI1_SYSCLK      , .ClkSrcId = RCC_CLK_SRC_APB2CLK , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK   },
    { .PeriphId = RCC_PERIPH_SAI1_PLLQ        , .ClkSrcId = RCC_CLK_SRC_PLL1QCLK, .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_PLLQ     },
    { .PeriphId = RCC_PERIPH_SAI1_HSI16       , .ClkSrcId = RCC_CLK_SRC_HSI16CLK, .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_HSI16    },
    { .PeriphId = RCC_PERIPH_SAI1_CKIN        , .ClkSrcId = RCC_CLK_SRC_CNT     , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN     },
#endif /* SAI1 */
};


/** \brief Configuration registers of RCC peripheral blocks.
 *
 * This array is created to reduce size of configuration.
 */
const rcc_BlockConfigStruct_t           rcc_PeriphBlockConfig[ RCC_BLOCK_LIST_CNT ] =
{
#if defined(SYSCFG)
    { .BlockId = RCC_BLOCK_SYSCFG   , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SYSCFGEN     , .LpCtrlMask = RCC_APB2SMENR_SYSCFGSMEN     , .RstCtrlMask = RCC_APB2RSTR_SYSCFGRST     },
#endif /* SYSCFG */


#if defined(PWR)
    { .BlockId = RCC_BLOCK_PWR      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_PWREN       , .LpCtrlMask = RCC_APB1SMENR1_PWRSMEN       , .RstCtrlMask = RCC_APB1RSTR1_PWRRST       },
#endif /* PWR */


#if defined(RCC_APB1ENR1_RTCAPBEN)
    { .BlockId = RCC_BLOCK_RTC      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_RTCAPBEN    , .LpCtrlMask = RCC_APB1SMENR1_RTCAPBSMEN    , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* RCC_BLOCK_APB1ENR1_RTCAPBEN */
#if defined(WWDG)
    { .BlockId = RCC_BLOCK_WWDG     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_WWDGEN      , .LpCtrlMask = RCC_APB1SMENR1_WWDGSMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* WWDG */


#if defined(DMA1)
    { .BlockId = RCC_BLOCK_DMA1     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DMA1EN       , .LpCtrlMask = RCC_AHB1SMENR_DMA1SMEN       , .RstCtrlMask = RCC_AHB1RSTR_DMA1RST       },
#endif /* DMA1 */
#if defined(DMA2)
    { .BlockId = RCC_BLOCK_DMA2     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DMA2EN       , .LpCtrlMask = RCC_AHB1SMENR_DMA2SMEN       , .RstCtrlMask = RCC_AHB1RSTR_DMA2RST       },
#endif /* DMA2 */
#if defined(DMAMUX1)
    { .BlockId = RCC_BLOCK_DMAMUX1  , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_DMAMUX1EN    , .LpCtrlMask = RCC_AHB1SMENR_DMAMUX1SMEN    , .RstCtrlMask = RCC_AHB1RSTR_DMAMUX1RST    },
#endif /* DMAMUX1 */


#if defined(FLASH)
    { .BlockId = RCC_BLOCK_FLASH    , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_FLASHEN      , .LpCtrlMask = RCC_AHB1SMENR_FLASHSMEN      , .RstCtrlMask = RCC_UNSUPPORTED_FUNCTION   },
#endif /* FLASH */
#if defined(FMC_R_BASE)
    { .BlockId = RCC_BLOCK_FMC      , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_FMCEN        , .LpCtrlMask = RCC_AHB3SMENR_FMCSMEN        , .RstCtrlMask = RCC_AHB3RSTR_FMCRST        },
#endif /* FMC_R_BASE */
#if defined(QUADSPI)
    { .BlockId = RCC_BLOCK_QUADSPI  , .ClkBusId = RCC_CLK_BUS_AHB3  , .StateMask = RCC_AHB3ENR_QSPIEN       , .LpCtrlMask = RCC_AHB3SMENR_QSPISMEN       , .RstCtrlMask = RCC_AHB3RSTR_QSPIRST       },
#endif /* QUADSPI */

#if defined(ADC12_COMMON)
    { .BlockId = RCC_BLOCK_ADC12    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_ADC12EN      , .LpCtrlMask = RCC_AHB2SMENR_ADC12SMEN      , .RstCtrlMask = RCC_AHB2RSTR_ADC12RST      },
#endif /* ADC12_COMMON */

#if defined(ADC345_COMMON)
    { .BlockId = RCC_BLOCK_ADC345   , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_ADC345EN     , .LpCtrlMask = RCC_AHB2SMENR_ADC345SMEN     , .RstCtrlMask = RCC_AHB2RSTR_ADC345RST     },
#endif /* ADC345_COMMON */


#if defined(DAC1)
    { .BlockId = RCC_BLOCK_DAC1     , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_DAC1EN       , .LpCtrlMask = RCC_AHB2SMENR_DAC1SMEN       , .RstCtrlMask = RCC_AHB2RSTR_DAC1RST       },
#endif /* DAC1 */
#if defined(DAC2)
    { .BlockId = RCC_BLOCK_DAC2     , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_DAC2EN       , .LpCtrlMask = RCC_AHB2SMENR_DAC2SMEN       , .RstCtrlMask = RCC_AHB2RSTR_DAC2RST       },
#endif /* DAC2 */
#if defined(DAC3)
    { .BlockId = RCC_BLOCK_DAC3     , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_DAC3EN       , .LpCtrlMask = RCC_AHB2SMENR_DAC3SMEN       , .RstCtrlMask = RCC_AHB2RSTR_DAC3RST       },
#endif /* DAC3 */
#if defined(DAC4)
    { .BlockId = RCC_BLOCK_DAC4     , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_DAC4EN       , .LpCtrlMask = RCC_AHB2SMENR_DAC4SMEN       , .RstCtrlMask = RCC_AHB2RSTR_DAC4RST       },
#endif /* DAC4 */


#if defined(CORDIC)
    { .BlockId = RCC_BLOCK_CORDIC   , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_CORDICEN     , .LpCtrlMask = RCC_AHB1SMENR_CORDICSMEN     , .RstCtrlMask = RCC_AHB1RSTR_CORDICRST     },
#endif /* CORDIC */
#if defined(FMAC)
    { .BlockId = RCC_BLOCK_FMAC     , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_FMACEN       , .LpCtrlMask = RCC_AHB1SMENR_FMACSMEN       , .RstCtrlMask = RCC_AHB1RSTR_FMACRST       },
#endif /* FMAC */


#if defined(CRC)
    { .BlockId = RCC_BLOCK_CRC      , .ClkBusId = RCC_CLK_BUS_AHB1  , .StateMask = RCC_AHB1ENR_CRCEN        , .LpCtrlMask = RCC_AHB1SMENR_CRCSMEN        , .RstCtrlMask = RCC_AHB1RSTR_CRCRST        },
#endif /* CRC */
#if defined(AES)
    { .BlockId = RCC_BLOCK_AES      , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_AESEN        , .LpCtrlMask = RCC_AHB2SMENR_AESSMEN        , .RstCtrlMask = RCC_AHB2RSTR_AESRST        },
#endif /* AES */
#if defined(RNG)
    { .BlockId = RCC_BLOCK_RNG      , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_RNGEN        , .LpCtrlMask = RCC_AHB2SMENR_RNGSMEN        , .RstCtrlMask = RCC_AHB2RSTR_RNGRST        },
#endif /* RNG */


#if defined(GPIOA)
    { .BlockId = RCC_BLOCK_GPIOA    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIOAEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIOASMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOARST      },
#endif /* GPIOA */
#if defined(GPIOB)
    { .BlockId = RCC_BLOCK_GPIOB    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIOBEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIOBSMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOBRST      },
#endif /* GPIOB */
#if defined(GPIOC)
    { .BlockId = RCC_BLOCK_GPIOC    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIOCEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIOCSMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOCRST      },
#endif /* GPIOC */
#if defined(GPIOD)
    { .BlockId = RCC_BLOCK_GPIOD    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIODEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIODSMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIODRST      },
#endif /* GPIOD */
#if defined(GPIOE)
    { .BlockId = RCC_BLOCK_GPIOE    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIOEEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIOESMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOERST      },
#endif /* GPIOE */
#if defined(GPIOF)
    { .BlockId = RCC_BLOCK_GPIOF    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIOFEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIOFSMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOFRST      },
#endif /* GPIOF */
#if defined(GPIOG)
    { .BlockId = RCC_BLOCK_GPIOG    , .ClkBusId = RCC_CLK_BUS_AHB2  , .StateMask = RCC_AHB2ENR_GPIOGEN      , .LpCtrlMask = RCC_AHB2SMENR_GPIOGSMEN      , .RstCtrlMask = RCC_AHB2RSTR_GPIOGRST      },
#endif /* GPIOG */




#if defined(TIM1)
    { .BlockId = RCC_BLOCK_TIM1     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM1EN       , .LpCtrlMask = RCC_APB2SMENR_TIM1SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM1RST        },
#endif /* TIM1 */
#if defined(TIM2)
    { .BlockId = RCC_BLOCK_TIM2     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM2EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM2RST       },
#endif /* TIM2 */
#if defined(TIM3)
    { .BlockId = RCC_BLOCK_TIM3     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM3EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM3RST       },
#endif /* TIM3 */
#if defined(TIM4)
    { .BlockId = RCC_BLOCK_TIM4     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM4EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM4SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM4RST       },
#endif /* TIM4 */
#if defined(TIM5)
    { .BlockId = RCC_BLOCK_TIM5     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM5EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM5SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM5RST       },
#endif /* TIM5 */
#if defined(TIM6)
    { .BlockId = RCC_BLOCK_TIM6     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM6EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM6SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM6RST       },
#endif /* TIM6 */
#if defined(TIM7)
    { .BlockId = RCC_BLOCK_TIM7     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_TIM7EN      , .LpCtrlMask = RCC_APB1SMENR1_TIM7SMEN     , .RstCtrlMask = RCC_APB1RSTR1_TIM7RST       },
#endif /* TIM7 */
#if defined(TIM8)
    { .BlockId = RCC_BLOCK_TIM8     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM8EN       , .LpCtrlMask = RCC_APB2SMENR_TIM8SMEN      , .RstCtrlMask = RCC_APB2RSTR_TIM8RST        },
#endif /* TIM8 */
#if defined(TIM15)
    { .BlockId = RCC_BLOCK_TIM15    , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM15EN      , .LpCtrlMask = RCC_APB2SMENR_TIM15SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM15RST       },
#endif /* TIM15 */
#if defined(TIM16)
    { .BlockId = RCC_BLOCK_TIM16    , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM16EN      , .LpCtrlMask = RCC_APB2SMENR_TIM16SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM16RST       },
#endif /* TIM16 */
#if defined(TIM17)
    { .BlockId = RCC_BLOCK_TIM17    , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM17EN      , .LpCtrlMask = RCC_APB2SMENR_TIM17SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM17RST       },
#endif /* TIM17 */
#if defined(TIM20)
    { .BlockId = RCC_BLOCK_TIM20    , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_TIM20EN      , .LpCtrlMask = RCC_APB2SMENR_TIM20SMEN     , .RstCtrlMask = RCC_APB2RSTR_TIM20RST       },
#endif /* TIM17 */
#if defined(HRTIM1)
    { .BlockId = RCC_BLOCK_HRTIM    , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_HRTIM1EN     , .LpCtrlMask = RCC_APB2SMENR_HRTIM1SMEN    , .RstCtrlMask = RCC_APB2RSTR_HRTIM1RST      },
#endif /* TIM17 */
#if defined(LPTIM1)
    { .BlockId = RCC_BLOCK_LPTIM1   , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_LPTIM1EN    , .LpCtrlMask = RCC_APB1SMENR1_LPTIM1SMEN   , .RstCtrlMask = RCC_APB1RSTR1_LPTIM1RST     },
#endif /* LPTIM1 */


#if defined(CRS)
    { .BlockId = RCC_BLOCK_CRS      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_CRSEN       , .LpCtrlMask = RCC_APB1SMENR1_CRSSMEN      , .RstCtrlMask = RCC_APB1RSTR1_CRSRST        },
#endif /* CRS */


#if defined(SPI1)
    { .BlockId = RCC_BLOCK_SPI1     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI1EN       , .LpCtrlMask = RCC_APB2SMENR_SPI1SMEN      , .RstCtrlMask = RCC_APB2RSTR_SPI1RST        },
#endif /* SPI1 */
#if defined(SPI2)
    { .BlockId = RCC_BLOCK_SPI2_I2S2, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI2EN      , .LpCtrlMask = RCC_APB1SMENR1_SPI2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_SPI2RST       },
#endif /* SPI2 */
#if defined(SPI3)
    { .BlockId = RCC_BLOCK_SPI3_I2S3, .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_SPI3EN      , .LpCtrlMask = RCC_APB1SMENR1_SPI3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_SPI3RST       },
#endif /* SPI3 */
#if defined(SPI4)
    { .BlockId = RCC_BLOCK_SPI4     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SPI4EN       , .LpCtrlMask = RCC_APB2SMENR_SPI4SMEN      , .RstCtrlMask = RCC_APB2RSTR_SPI4RST        },
#endif /* SPI4 */


#if defined(USART1)
    { .BlockId = RCC_BLOCK_USART1   , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_USART1EN     , .LpCtrlMask = RCC_APB2SMENR_USART1SMEN    , .RstCtrlMask = RCC_APB2RSTR_USART1RST      },
#endif /* USART1 */
#if defined(USART2)
    { .BlockId = RCC_BLOCK_USART2   , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART2EN    , .LpCtrlMask = RCC_APB1SMENR1_USART2SMEN   , .RstCtrlMask = RCC_APB1RSTR1_USART2RST     },
#endif /* USART2 */
#if defined(USART3)
    { .BlockId = RCC_BLOCK_USART3   , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USART3EN    , .LpCtrlMask = RCC_APB1SMENR1_USART3SMEN   , .RstCtrlMask = RCC_APB1RSTR1_USART3RST     },
#endif /* USART3 */
#if defined(UART4)
    { .BlockId = RCC_BLOCK_UART4    , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART4EN     , .LpCtrlMask = RCC_APB1SMENR1_UART4SMEN    , .RstCtrlMask = RCC_APB1RSTR1_UART4RST      },
#endif /* UART4 */
#if defined(UART5)
    { .BlockId = RCC_BLOCK_UART5    , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_UART5EN     , .LpCtrlMask = RCC_APB1SMENR1_UART5SMEN    , .RstCtrlMask = RCC_APB1RSTR1_UART5RST      },
#endif /* UART5 */
#if defined(LPUART1)
    { .BlockId = RCC_BLOCK_LPUART1  , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_LPUART1EN   , .LpCtrlMask = RCC_APB1SMENR2_LPUART1SMEN  , .RstCtrlMask = RCC_APB1RSTR2_LPUART1RST    },
#endif /* LPUART1 */


#if defined(I2C1)
    { .BlockId = RCC_BLOCK_I2C1     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C1EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C1SMEN     , .RstCtrlMask = RCC_APB1RSTR1_I2C1RST       },
#endif /* I2C1 */
#if defined(I2C2)
    { .BlockId = RCC_BLOCK_I2C2     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C2EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C2SMEN     , .RstCtrlMask = RCC_APB1RSTR1_I2C2RST       },
#endif /* I2C2 */
#if defined(I2C3)
    { .BlockId = RCC_BLOCK_I2C3     , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_I2C3EN      , .LpCtrlMask = RCC_APB1SMENR1_I2C3SMEN     , .RstCtrlMask = RCC_APB1RSTR1_I2C3RST       },
#endif /* I2C3 */
#if defined(I2C4)
    { .BlockId = RCC_BLOCK_I2C4     , .ClkBusId = RCC_CLK_BUS_APB1_2, .StateMask = RCC_APB1ENR2_I2C4EN      , .LpCtrlMask = RCC_APB1SMENR2_I2C4SMEN     , .RstCtrlMask = RCC_APB1RSTR2_I2C4RST       },
#endif /* I2C4 */


#if defined(USB)
    { .BlockId = RCC_BLOCK_USB      , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_USBEN       , .LpCtrlMask = RCC_APB1SMENR1_USBSMEN      , .RstCtrlMask = RCC_APB1RSTR1_USBRST        },
#endif /* USB */


#if defined(FDCAN1)
    { .BlockId = RCC_BLOCK_FDCAN1   , .ClkBusId = RCC_CLK_BUS_APB1_1, .StateMask = RCC_APB1ENR1_FDCANEN     , .LpCtrlMask = RCC_APB1SMENR1_FDCANSMEN    , .RstCtrlMask = RCC_APB1RSTR1_FDCANRST      },
#endif /* CAN1 */

#if defined(SAI1)
    { .BlockId = RCC_BLOCK_SAI1     , .ClkBusId = RCC_CLK_BUS_APB2  , .StateMask = RCC_APB2ENR_SAI1EN       , .LpCtrlMask = RCC_APB2SMENR_SAI1SMEN      , .RstCtrlMask = RCC_APB2RSTR_SAI1RST        },
#endif /* SAI1 */
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
 * and set up all the necessary resources for the module to work.
 *
 * \note In debug build the configuration arrays are checked. If incorrect
 *       configuration is detected, error will be raised.
 */
rcc_RequestState_t Rcc_Init( rcc_ConfigStruct_t * const clockConfig )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

#if defined(DEBUG)

    /*---------------------- Configuration arrays check ----------------------*/

    for( rcc_ClkMuxId_t clkMuxId = 0u; RCC_CLK_MUX_LIST_CNT > clkMuxId; clkMuxId++ )
    {
        if( clkMuxId != rcc_ClkMuxConfig[ clkMuxId ].ClkMuxId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* Array is configured correctly */
        }
    }

    for( rcc_PeriphId_t periphId = 0u; RCC_PERIPH_LIST_CNT > periphId; periphId++ )
    {
        if( periphId != rcc_ConfigStruct[ periphId ].PeriphId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* Array is configured correctly */
        }
    }

    for( rcc_ClkBusId_t clkBusId = 0u; RCC_CLK_BUS_CNT > clkBusId; clkBusId++ )
    {
        if( clkBusId != rcc_ClkBusConfigStruct[ clkBusId ].ClkBusId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* Array is configured correctly */
        }
    }

    for( rcc_ClkBusId_t periphClkSrcId = 0u; RCC_CLK_SRC_CNT > periphClkSrcId; periphClkSrcId++ )
    {
        if( periphClkSrcId != rcc_PeriphClkSrcConfig[ periphClkSrcId ].PeriphClkSrcId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* Array is configured correctly */
        }
    }

#endif

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

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_PwrRange( clockConfig->VoltageScaling );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_HseConfig( clockConfig->HSE_ClockType, clockConfig->HSE_Frequency_Hz );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_LseInactive();
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkSrc_Set_LseType( clockConfig->LSE_ClockType );
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

        rcc_FlashLatency_t flashLatency = RCC_FLASH_LATENCY_0_WS;
        rcc_FreqHz_t       hclkClk      = 0u;

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Get_ExpectedSysClkFrequency( clockConfig, &hclkClk );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Get_ExpectedFlashLatency( hclkClk, &flashLatency );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_FlashLatency( flashLatency );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_ClkBus_Set_SysClkSource( clockConfig->SystemClockSource );
        }

        if( RCC_REQUEST_OK == retState )
        {
            retState = Rcc_Set_SysTickInterval( clockConfig->SysTickInterval );
        }

        if( RCC_REQUEST_OK == retState )
        {
            for( rcc_ClkOut_Id_t clkOutId = 0u; RCC_CLK_OUT_CNT > clkOutId; clkOutId++ )
            {
                /* Clock outputs configuration (return states are not needed to be checked) */
                (void)Rcc_Set_ClkOutSource( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockSource );

                (void)Rcc_Set_ClkOutDivider( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockDivider );

                (void)Rcc_Set_ClkOutPin( clkOutId, clockConfig->McoConfig[ clkOutId ].ClockOutPin );
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
        clockConfig->HSE_Frequency_Hz  = 24000000u;
        clockConfig->SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;
        clockConfig->CSS_Enable        = RCC_FUNCTION_INACTIVE;
        clockConfig->AHB_Divider       = RCC_AHB_DIVIDER_1;
        clockConfig->APB1_Divider      = RCC_APB1_DIVIDER_1;
        clockConfig->APB2_Divider      = RCC_APB2_DIVIDER_1;
        clockConfig->APB3_Divider      = RCC_APB3_DIVIDER_NONE;
        clockConfig->VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_0;
        clockConfig->SysTickInterval   = 1u;

        clockConfig->McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLKOUT_SOURCE_MCO1_SYSCLK;
        clockConfig->McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 1u;
        clockConfig->McoConfig[ RCC_CLK_OUT_MCO1 ].ClockOutPin  = RCC_CLKOUT_PIN_MCO1_PA8;

        clockConfig->McoConfig[ RCC_CLK_OUT_LSCO ].ClockSource  = RCC_CLKOUT_SOURCE_NONE;
        clockConfig->McoConfig[ RCC_CLK_OUT_LSCO ].ClockDivider = 1u;

        clockConfig->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSI16;
        clockConfig->Pll_Config[ RCC_PLL_1 ].M_Divider    = 4u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].N_Multiplier = 85u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].P_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].Q_Divider    = 2u;
        clockConfig->Pll_Config[ RCC_PLL_1 ].R_Divider    = 2u;

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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_LIST_CNT > periphId )
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

    if( RCC_PERIPH_LIST_CNT > periphId )
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

    if( ( RCC_PERIPH_LIST_CNT > periphId  ) &&
        ( RCC_NULL_PTR       != funcState )    )
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    volatile uint32_t  regValue = 0u;

    if( RCC_PERIPH_LIST_CNT > periphId )
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

    if( RCC_PERIPH_LIST_CNT > periphId )
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

    if( ( RCC_PERIPH_LIST_CNT > periphId  ) &&
        ( RCC_NULL_PTR       != funcState )    )
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

    if( RCC_PERIPH_LIST_CNT > periphId )
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

    if( RCC_PERIPH_LIST_CNT > periphId )
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

    if( ( RCC_PERIPH_LIST_CNT > periphId  ) &&
        ( RCC_NULL_PTR       != funcState )    )
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Set_Config( pllId, configStruct );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Get_InternalClk( pllId, pllClk );

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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Set_Active( pllId );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Set_Active( pllId );

    return ( retState );
}


/**
 * \brief Reading status of Phase Locked Loop (PLL) block
 *
 * \param retState [out]: Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_PllState( rcc_PllId_t pllId, rcc_FunctionState_t * const pllRetState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Get_State( pllId, pllRetState );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Set_Source( pllId, clkSource );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Get_Source( pllId, clkSource );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Get_Clk_OutP( pllId, pllClk );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Get_Clk_OutQ( pllId, pllClk );

    return ( retState );
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
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    retState = Rcc_Pll_Get_Clk_OutR( pllId, pllClk );

    return ( retState );
}

/*------------------------ Clock sources configuration -----------------------*/

/**
 * \brief Activation of oscillator
 *
 * User can request activation of required oscillator. If required
 * oscillator is correctly activated, and required oscillator ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param oscId [in]: Oscillator ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscActive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_OSC_HSE == oscId )
    {
        retState = Rcc_ClkSrc_Set_HseActive();
    }
    else if( RCC_OSC_LSE == oscId )
    {
        retState = Rcc_ClkSrc_Set_LseActive();
    }
    else if( RCC_OSC_HSI16 == oscId )
    {
        retState = Rcc_ClkSrc_Set_Hsi16Active();
    }
    else if( RCC_OSC_HSI48 == oscId )
    {
        retState = Rcc_ClkSrc_Set_Hsi48Active();
    }
    else if( RCC_OSC_LSI == oscId )
    {
        retState = Rcc_ClkSrc_Set_LsiActive();
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief De-activation of oscillator
 *
 * User can request de-activation of required oscillator. If required
 * oscillator is correctly de-activated, and required oscillator ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param oscId [in]: Oscillator ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscInactive( rcc_OscId_t oscId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_OSC_HSE == oscId )
    {
        retState = Rcc_ClkSrc_Set_HseInactive();
    }
    else if( RCC_OSC_LSE == oscId )
    {
        retState = Rcc_ClkSrc_Set_LseInactive();
    }
    else if( RCC_OSC_HSI16 == oscId )
    {
        retState = Rcc_ClkSrc_Set_Hsi16Inactive();
    }
    else if( RCC_OSC_HSI48 == oscId )
    {
        retState = Rcc_ClkSrc_Set_Hsi48Inactive();
    }
    else if( RCC_OSC_LSI == oscId )
    {
        retState = Rcc_ClkSrc_Set_LsiInactive();
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Returns activation status of oscillator
 *
 * \param oscId        [in]: Oscillator ID
 * \param oscRetState [out]: Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_OscState( rcc_OscId_t oscId, rcc_FunctionState_t * const oscRetState )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != oscRetState )
    {
        if( RCC_OSC_HSE == oscId )
        {
            retState = Rcc_ClkSrc_Get_HseState( oscRetState );
        }
        else if( RCC_OSC_LSE == oscId )
        {
            retState = Rcc_ClkSrc_Get_LseState( oscRetState );
        }
        else if( RCC_OSC_HSI16 == oscId )
        {
            retState = Rcc_ClkSrc_Get_Hsi16State( oscRetState );
        }
        else if( RCC_OSC_HSI48 == oscId )
        {
            retState = Rcc_ClkSrc_Get_Hsi48State( oscRetState );
        }
        else if( RCC_OSC_LSI == oscId )
        {
            retState = Rcc_ClkSrc_Get_LsiState( oscRetState );
        }
        else
        {
            *oscRetState = RCC_FUNCTION_INACTIVE;
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
 * \brief Function used for setting oscillator divider
 *
 * User can set required oscillator divider. If required oscillator ID and
 * divider are correct, returned state is "OK". Otherwise returns error.
 *
 * \param oscId [in]: Oscillator ID
 * \param oscDiv [in]: Required oscillator divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_OscDiv( rcc_OscId_t oscId, rcc_OscDiv_t oscDiv )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    (void)oscId;
    (void)oscDiv;

    retState = RCC_REQUEST_ERROR;

    return ( retState );
}


/**
 * \brief Function used for getting oscillator divider
 *
 * User can get current oscillator divider. If required oscillator ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param oscId [in]: Oscillator ID
 * \param oscDiv [out]: Current oscillator divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_OscDiv( rcc_OscId_t oscId, rcc_OscDiv_t * const oscDiv )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    (void)oscId;

    if( RCC_NULL_PTR != oscDiv )
    {
        *oscDiv = 0u;
    }

    retState = RCC_REQUEST_ERROR;

    return ( retState );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

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
            retState = Rcc_ClkBus_Set_AHBDivider( clkBusDivider );
            break;

        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2:
            retState = Rcc_ClkBus_Set_APB1Divider( clkBusDivider );
            break;

        case RCC_CLK_BUS_APB2:
            retState = Rcc_ClkBus_Set_APB2Divider( clkBusDivider );
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
            retState = Rcc_ClkBus_Get_AHBDivider( (rcc_AHB_Div_t*) clkBusDivider );
            break;

        case RCC_CLK_BUS_APB1_1:
        case RCC_CLK_BUS_APB1_2:
            retState = Rcc_ClkBus_Get_APB1Divider( (rcc_APB1_Div_t*) clkBusDivider );
            break;

        case RCC_CLK_BUS_APB2:
            retState = Rcc_ClkBus_Get_APB2Divider( (rcc_APB2_Div_t*) clkBusDivider );
            break;

        default:
            retState = RCC_REQUEST_ERROR;
            break;
    }

    return ( retState );
}


/**
 * \brief Function used for getting clock frequency
 *
 * User can get current clock bus frequency. If required clock bus ID is correct,
 * returned state is "OK". Otherwise returns error.
 *
 * \param clkId   [in]: ID of required clock bus
 * \param clkBusFreq [out]: Current clock bus frequency in Hz
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ClkFreq( rcc_ClkSrcId_t clkId, rcc_FreqHz_t * const clkBusFreq )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_CLK_SRC_CNT >  clkId      ) &&
        ( RCC_NULL_PTR    != clkBusFreq )    )
    {
        if( RCC_NULL_PTR != rcc_PeriphClkSrcConfig[ clkId ].ClkSrcCallback )
        {
            retState = rcc_PeriphClkSrcConfig[ clkId ].ClkSrcCallback( clkBusFreq );
        }
        else
        {
            *clkBusFreq = 0u;

            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
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
 *  ================================================================
 * |  Wait states   | Vcore Range 1 | Vcore Range 1 | Vcore Range 2 |
 * |   (LATENCY)    |  boost mode   |  normal mode  |               |
 * |================|===============|===============|===============|
 * |      0 WS      |  =< 34 MHz    |  =< 30 MHz    |  =< 12 MHz    |
 * | (1 CPU cycle)  |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      1 WS      |  =< 68 MHz    |  =< 60 MHz    |  =< 24 MHz    |
 * | (2 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      2 WS      |  =< 102 MHz   |  =< 90 MHz    |  =< 26 MHz    |
 * | (3 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      3 WS      |  =< 136 MHz   |  =< 120 MHz   |      ---      |
 * | (4 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      4 WS      |  =< 170 MHz   |  =< 150 MHz   |      ---      |
 * | (5 CPU cycles) |               |               |               |
 *  ================================================================
 * \endcond
 *
* \param clockConfig [in]: Configuration structure
* \return State of request execution. Returns "OK" if request was success,
*         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_PwrRange( rcc_PwrVoltageScale_t voltageScale )
{
    rcc_RequestState_t retState       = RCC_REQUEST_ERROR;
    uint32_t           regValue       = 0u;
    uint32_t           reqRegVal      = 0u;
    uint32_t           vosRegVal      = 0u;
    uint32_t           boostRegVal    = 0u;
    uint32_t           reqBoostRegVal = 0u;

    if( RCC_PWR_VOLTAGE_SCALE_CNT > voltageScale )
    {
        if( RCC_PWR_VOLTAGE_SCALE_0 == voltageScale )
        {
            LL_PWR_EnableRange1BoostMode();

            reqBoostRegVal = 1u;

            reqRegVal = LL_PWR_REGU_VOLTAGE_SCALE1;
        }
        else if( RCC_PWR_VOLTAGE_SCALE_1 == voltageScale )
        {
            LL_PWR_DisableRange1BoostMode();

            reqBoostRegVal = 0u;

            reqRegVal = LL_PWR_REGU_VOLTAGE_SCALE1;
        }
        else
        {
            LL_PWR_DisableRange1BoostMode();

            reqBoostRegVal = 0u;

            reqRegVal = LL_PWR_REGU_VOLTAGE_SCALE2;
        }

        LL_PWR_SetRegulVoltageScaling( reqRegVal );

        for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = LL_PWR_GetRegulVoltageScaling();

            vosRegVal = LL_PWR_IsActiveFlag_VOS();

            boostRegVal = LL_PWR_IsEnabledRange1BoostMode();

            if( ( reqRegVal      == regValue    ) &&
                ( 0u             == vosRegVal   ) &&
                ( reqBoostRegVal == boostRegVal )    )
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
 * \brief Function used for calculating expected flash latency
 *
 * The flash latency is dependent on expected core clock (SYSCLK) and core power
 * range. This function calculates expected flash latency based on current
 * configuration of clock and power range. The voltage scaling has to be already
 * configured before executing this function, because it uses real data from
 * registers.
 *
 * \warning The calculation uses real data from registers. The voltage scaling
 *          has to be already configured before executing this function.
 *
 * \param hclkClk       [in]: HCLK clock frequency in Hz.
 * \param voltageScale [out]: Current power range configuration
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Get_ExpectedFlashLatency( rcc_FreqHz_t hclkClk, rcc_FlashLatency_t * const retLatency )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    if( RCC_NULL_PTR != retLatency )
    {
        if( ( 0u                             != hclkClk  ) &&
            ( RCC_SCALE1_LATENCY5_BOOST_FREQ >= hclkClk  )    )
        {
            uint32_t voltageScaling = LL_PWR_GetRegulVoltageScaling();
            uint32_t booststatus    = LL_PWR_IsEnabledRange1BoostMode();

            if( ( LL_PWR_REGU_VOLTAGE_SCALE1 == voltageScaling ) &&
                ( 0u                         == booststatus    )    )
            {
                if( RCC_SCALE1_LATENCY4_BOOST_FREQ < hclkClk )
                {
                    /* 136 < HCLK <= 170 => 4WS (5 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_4_WS;
                }
                else if( RCC_SCALE1_LATENCY3_BOOST_FREQ < hclkClk )
                {
                    /* 102 < HCLK <= 136 => 3WS (4 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_3_WS;
                }
                else if( RCC_SCALE1_LATENCY2_BOOST_FREQ < hclkClk )
                {
                    /* 68 < HCLK <= 102 => 2WS (3 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_2_WS;
                }
                else
                {
                    if( RCC_SCALE1_LATENCY1_BOOST_FREQ < hclkClk )
                    {
                        /* 34 < HCLK <= 68 => 1WS (2 CPU cycles) */
                        *retLatency = RCC_FLASH_LATENCY_1_WS;
                    }
                    else
                    {
                        /* else HCLKFrequency <= 10MHz default RCC_FLASH_LATENCY_0 0WS */
                        *retLatency = RCC_FLASH_LATENCY_0_WS;
                    }
                }
            }
            else if( LL_PWR_REGU_VOLTAGE_SCALE1 == voltageScaling )
            {
                if( RCC_SCALE1_LATENCY4_FREQ < hclkClk )
                {
                    /* 120 < HCLK <= 150 => 4WS (5 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_4_WS;
                }
                else if( RCC_SCALE1_LATENCY3_FREQ < hclkClk )
                {
                    /* 90 < HCLK <= 120 => 3WS (4 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_3_WS;
                }
                else if( RCC_SCALE1_LATENCY2_FREQ < hclkClk )
                {
                    /* 60 < HCLK <= 90 => 2WS (3 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_2_WS;
                }
                else
                {
                    if( RCC_SCALE1_LATENCY1_FREQ < hclkClk )
                    {
                    /* 30 < HCLK <= 60 => 1WS (2 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_1_WS;
                    }
                    else
                    {
                        /* else HCLKFrequency <= 10MHz default RCC_FLASH_LATENCY_0 0WS */
                        *retLatency = RCC_FLASH_LATENCY_0_WS;
                    }
                }
            }
            else if( LL_PWR_REGU_VOLTAGE_SCALE2 == voltageScaling )
            {
                if( RCC_SCALE2_LATENCY2_FREQ < hclkClk )
                {
                    /* 24 < HCLK <= 26 => 2WS (3 CPU cycles) */
                    *retLatency = RCC_FLASH_LATENCY_2_WS;
                }
                else
                {
                    if( RCC_SCALE2_LATENCY1_FREQ < hclkClk )
                    {
                        /* 12 < HCLK <= 24 => 1WS (2 CPU cycles) */
                        *retLatency = RCC_FLASH_LATENCY_1_WS;
                    }
                    else
                    {
                        /* hclkClk <= 8MHz */
                        *retLatency = RCC_FLASH_LATENCY_0_WS;
                    }
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
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Calculates and configures proper FLASH memory reading wait states.
 *
 * To access data from FLASH memory, system has to wait for specific CPU cycles
 * between requesting them and getting them. The flash wait state is dependent
 * on expected core frequency (SYSCLK) and core power range.
 *
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ================================================================
 * |  Wait states   | Vcore Range 1 | Vcore Range 1 | Vcore Range 2 |
 * |   (LATENCY)    |  boost mode   |  normal mode  |               |
 * |================|===============|===============|===============|
 * |      0 WS      |  =< 34 MHz    |  =< 30 MHz    |  =< 12 MHz    |
 * | (1 CPU cycle)  |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      1 WS      |  =< 68 MHz    |  =< 60 MHz    |  =< 24 MHz    |
 * | (2 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      2 WS      |  =< 102 MHz   |  =< 90 MHz    |  =< 26 MHz    |
 * | (3 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      3 WS      |  =< 136 MHz   |  =< 120 MHz   |      ---      |
 * | (4 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      4 WS      |  =< 170 MHz   |  =< 150 MHz   |      ---      |
 * | (5 CPU cycles) |               |               |               |
 *  ================================================================
 * \endcond
 *
 * \param hclkClk [in]: System clock frequency (HCLK frequency)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_FlashLatency( rcc_FlashLatency_t flashLatency )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    LL_FLASH_SetLatency( flashLatency );

    for( uint32_t iterationCnt = 0u; RCC_TIMEOUT_RAW > iterationCnt; iterationCnt++ )
    {
        uint32_t regVal = LL_FLASH_GetLatency();

        if( flashLatency == regVal )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }

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


/**
 * \brief Function used to set clock output pin.
 *
 * \param outId     [in]: Clock output identification
 * \param clkOutPin [in]: Clock output pin
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Set_ClkOutPin( rcc_ClkOut_Id_t outId, rcc_ClkOut_Pin_t clkOutPin )
{
    return Rcc_ClkOut_Set_ClockPin( outId, clkOutPin );
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
                 if( ( 0u != clockConfig->Pll_Config[ RCC_PLL_1 ].M_Divider ) &&
                     ( 0u != clockConfig->Pll_Config[ RCC_PLL_1 ].R_Divider )    )
                 {
                     rcc_FreqHz_t outFreq = pllSrcFreq / clockConfig->Pll_Config[ RCC_PLL_1 ].M_Divider;

                     outFreq = outFreq * clockConfig->Pll_Config[ RCC_PLL_1 ].N_Multiplier;

                     outFreq = outFreq / clockConfig->Pll_Config[ RCC_PLL_1 ].R_Divider;

                     *sysClk = outFreq;

                     retState = RCC_REQUEST_OK;
                 }
                 else
                 {
                     retState = RCC_REQUEST_ERROR;

                     *sysClk = 0u;
                 }
            }
            else
            {
                /* Reached error state */
            }
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSI == clockConfig->SystemClockSource )
        {
            *sysClk = HSI_VALUE;

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_SYSTEM_CLOCK_SOURCE_HSE == clockConfig->SystemClockSource )
        {
            *sysClk = clockConfig->HSE_Frequency_Hz;

            retState = RCC_REQUEST_OK;
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
