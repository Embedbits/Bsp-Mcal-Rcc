/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.h
 * \ingroup Rcc
 * \brief Rcc module ClkMux component functionality.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkMux.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_CLKMUX_TIMEOUT_RAW             ( 0x84FCB )

/* ============================== TYPEDEFS ================================== */

/**
 * \brief Clock tree configuration structure
 */
typedef struct __attribute__((packed))
{
    rcc_ClkMuxId_t          ClkMuxId;         /**< Peripheral ID                    */
    rcc_RegId_t             ClkMuxRegId;      /**< Clock MUX register ID.           */
    volatile uint32_t const ClkSrcMask;       /**< Clock MUX bit mask.              */
    volatile uint32_t const ClkSrcVal;        /**< Clock MUX value.                 */
    rcc_ClkMuxId_t          DefaultClkMuxId;  /**< Peripheral default configuration */
}   rcc_ClkMuxConfigStruct_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration array of MCU peripherals with available clock MUX. */
static const rcc_ClkMuxConfigStruct_t   rcc_ClkMuxConfig[ RCC_CLK_MUX_LIST_CNT ] =
{
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_HCLKDIV8 , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI       , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_LSI      , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE       , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },

  { .ClkMuxId = RCC_CLK_MUX_RTC_NONE          , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_NONE         , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32     , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_HSE_DIV      , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE           , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSE          , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI           , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSI          , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },


  { .ClkMuxId = RCC_CLK_MUX_LPCLK_HSI         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CKERPSEL    , .ClkSrcVal = LL_RCC_CLKP_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPCLK_HSE         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CKERPSEL    , .ClkSrcVal = LL_RCC_CLKP_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPCLK_CSI         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CKERPSEL    , .ClkSrcVal = LL_RCC_CLKP_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },

    /*-------------------------------- Timers --------------------------------*/

#if defined(LPTIM1)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL2P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LPCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
#endif /* LPTIM1 */
#if defined(LPTIM2)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL2P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LPCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
#endif /* LPTIM2 */
#if defined(LPTIM3)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_PLL2P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_LSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_LPCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
#endif /* LPTIM3 */
#if defined(LPTIM4)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_PLL2P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_LSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_LPCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
#endif /* LPTIM4 */
#if defined(LPTIM5)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_PLL2P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_LSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_LPCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
#endif /* LPTIM5 */
#if defined(LPTIM6)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_PLL2P      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_PLL3R      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_LSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_LPCLK      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
#endif /* LPTIM6 */


    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PIN          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_LPCLK        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
#endif /* SPI1 */
#if defined(SPI2)
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PIN          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_LPCLK        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
#endif /* SPI2 */
#if defined(SPI3)
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PIN          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_LPCLK        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
#endif /* SPI3 */
#if defined(SPI4)
  { .ClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_PCLK2       , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_PLL2Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_PLL2Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_PLL3Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_PLL3Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_CSI          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_HSE          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
#endif /* SPI4 */
#if defined(SPI5)
  { .ClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_PLL2Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_PLL2Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_PLL3Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_PLL3Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_CSI          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_HSE          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
#endif /* SPI5 */
#if defined(SPI6)
  { .ClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_PCLK2       , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_PLL2Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_PLL2Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_PLL3Q        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_PLL3Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_CSI          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_HSE          , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
#endif /* SPI6 */


#if defined(I2C1)
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PLL3R        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_CSI          , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
#endif /* I2C1 */
#if defined(I2C2)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PLL3R        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_CSI          , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
#endif /* I2C2 */
#if defined(I2C3)
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PLL3R        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_CSI          , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
#endif /* I2C3 */
#if defined(I2C4)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PLL3R        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_CSI          , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
#endif /* I2C4 */


#if defined(I3C1)
  { .ClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I3C1_PLL3R        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I3C1_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
#endif /* I3C1 */

#if defined(I3C2)
  { .ClkMuxId = RCC_CLK_MUX_I3C2_PCLK1        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I3C2_PLL3R        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I3C2_HSI64        , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK1        },
#endif /* I3C2 */


#if defined(USART1)
  { .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PCLK2     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_PLL2Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_PLL3Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_CSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
#endif /* USART1 */
#if defined(USART2)
  { .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_PLL2Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_PLL3Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_CSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
#endif /* USART2 */
#if defined(USART3)
  { .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_PLL2Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_PLL3Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_CSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
#endif /* USART3 */
#if defined(USART6)
  { .ClkMuxId = RCC_CLK_MUX_USART6_PCLK1      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_PLL2Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_PLL3Q      , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_HSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_LSE        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_CSI        , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
#endif /* USART6 */
#if defined(USART10)
  { .ClkMuxId = RCC_CLK_MUX_USART10_PCLK1     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_PCLK1    , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_PLL2Q     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_PLL3Q     , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_PLL3Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_HSI       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_HSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_LSE       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_CSI       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_CSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
#endif /* USART10 */
#if defined(USART11)
  { .ClkMuxId = RCC_CLK_MUX_USART11_PCLK1     , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_PCLK1    , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_PLL2Q     , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_PLL3Q     , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_PLL3Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_HSI       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_HSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_LSE       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_CSI       , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_CSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
#endif /* USART11 */


#if defined(UART4)
  { .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_PLL3Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_CSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
#endif /* UART4 */
#if defined(UART5)
  { .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_PLL3Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_CSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
#endif /* USART5 */
#if defined(UART7)
  { .ClkMuxId = RCC_CLK_MUX_UART7_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_PLL3Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_HSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_CSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
#endif /* USART5 */
#if defined(UART8)
  { .ClkMuxId = RCC_CLK_MUX_UART8_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_PLL3Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_HSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_CSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
#endif /* USART5 */
#if defined(UART9)
  { .ClkMuxId = RCC_CLK_MUX_UART9_PCLK1       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_PLL3Q       , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_HSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_LSE         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_CSI         , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
#endif /* USART5 */
#if defined(UART12)
  { .ClkMuxId = RCC_CLK_MUX_UART12_PCLK1      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_PLL2Q      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_PLL3Q      , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_HSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_LSE        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_CSI        , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
#endif /* USART5 */


#if defined(LPUART1)
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PCLK3    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL2Q     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL3Q     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_HSI      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_CSI       , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_CSI      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
#endif /* LPUART1 */


#if defined(FDCAN1)
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL1Q       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL1Q      , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL2Q       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_HSE        , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
#endif /* FDCAN1 */


#if defined(SDMMC1)
  { .ClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC1SEL   , .ClkSrcVal = LL_RCC_SDMMC1_CLKSOURCE_PLL1Q     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      },
  { .ClkMuxId = RCC_CLK_MUX_SDMMC1_PLL2R      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC1SEL   , .ClkSrcVal = LL_RCC_SDMMC1_CLKSOURCE_PLL2R     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      },
#endif /* SDMMC1 */
#if defined(SDMMC2)
  { .ClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC2SEL   , .ClkSrcVal = LL_RCC_SDMMC2_CLKSOURCE_PLL1Q     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      },
  { .ClkMuxId = RCC_CLK_MUX_SDMMC2_PLL2R      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC2SEL   , .ClkSrcVal = LL_RCC_SDMMC2_CLKSOURCE_PLL2R     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      },
#endif /* SDMMC2 */


#if defined(OCTOSPI1)
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_HCLK        , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_PLL1Q    , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_PLL2R    , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_LPCLK    , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
#endif /* OCTOSPI1 || OCTOSPI2 */

#if defined(USB_DRD_FS)
  { .ClkMuxId = RCC_CLK_MUX_USB_NONE          , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_NONE         , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q         , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_PLL1Q        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL3Q         , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_PLL3Q        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_USB_HSI48         , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_HSI48        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
#endif /* USB_OTG_HS */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(SAI1)
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_LPCLK        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
#endif /* SAI1 */
#if defined(SAI2)
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_CKIN         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_LPCLK        , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
#endif /* SAI2 */


#if defined(CEC)
  { .ClkMuxId = RCC_CLK_MUX_CEC_LSE           , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CECSEL      , .ClkSrcVal = LL_RCC_CEC_CLKSOURCE_LSE          , .DefaultClkMuxId = RCC_CLK_MUX_CEC_LSE           },
  { .ClkMuxId = RCC_CLK_MUX_CEC_CSI_DIV       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CECSEL      , .ClkSrcVal = LL_RCC_CEC_CLKSOURCE_CSI_DIV122   , .DefaultClkMuxId = RCC_CLK_MUX_CEC_LSE           },
  { .ClkMuxId = RCC_CLK_MUX_CEC_LSI           , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CECSEL      , .ClkSrcVal = LL_RCC_CEC_CLKSOURCE_LSI          , .DefaultClkMuxId = RCC_CLK_MUX_CEC_LSE           },
#endif /* CEC */

    /*-------------------------------- Analog --------------------------------*/

#if defined(ADC1) || defined(ADC2) || defined(DAC1)
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HCLK      , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_SYSCLK    , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_PLL2R     , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HSE       , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_CSI       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
#endif /* ADC1 || ADC2 || DAC1 */

#if defined(DAC1)
  { .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_DACSEL      , .ClkSrcVal = LL_RCC_DAC_LP_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       },
  { .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSI       , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_DACSEL      , .ClkSrcVal = LL_RCC_DAC_LP_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       },
#endif /* DAC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(RNG)
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_HSI48        , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .ClkMuxId = RCC_CLK_MUX_RNG_PLL1Q         , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_PLL1Q        , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .ClkMuxId = RCC_CLK_MUX_RNG_LSE           , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_LSE          , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .ClkMuxId = RCC_CLK_MUX_RNG_LSI           , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_LSI          , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
#endif /* RNG */
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes peripheral clock multiplexer module.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkMux_Init( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_ClkMuxId_t clkMuxId = 0u; RCC_CLK_MUX_LIST_CNT > clkMuxId; clkMuxId++ )
    {
        if( clkMuxId != rcc_ClkMuxConfig[ clkMuxId ].ClkMuxId )
        {
            /* Configuration error: Indexes do not match */
            retState = RCC_REQUEST_ERROR;
        }
        else
        {
            /* Indexes match, proceed to next entry */
        }
    }

    return ( retState );
}


/**
 * \brief De-initializes peripheral clock multiplexer module.
 */
void Rcc_ClkMux_Deinit( void )
{
    return;
}


/**
 * \brief Main task of peripheral clock multiplexer module.
 */
void Rcc_ClkMux_Task( void )
{
    return;
}


/**
 * \brief Sets the peripheral clock multiplexer to required clock source.
 *
 * \warning To change the clock source of a peripheral, first the peripheral
 * must be disabled by setting the clock multiplexer to its default value.
 *
 * \param clkMuxId [in]: Peripheral clock source identifier
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkMux_Set_ClkActive( rcc_ClkMuxId_t clkMuxId  )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
    {
        uint32_t    ctrlRegVal  = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal;
        uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
        rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

        rcc_ClkMuxId_t defaultClkMuxId   = rcc_ClkMuxConfig[ clkMuxId ].DefaultClkMuxId;
        uint32_t       defaultCtrlRegVal = rcc_ClkMuxConfig[ defaultClkMuxId ].ClkSrcVal;

        /* Check if the clock multiplexer is set to default value. */
        uint32_t actualRegVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

        if( actualRegVal == defaultCtrlRegVal )
        {
            Rcc_Set_RegVal( ctrlRegId, ctrlRegMask, ctrlRegVal );

            for( uint32_t iterationCnt = 0u; RCC_CLKMUX_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                regValue = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask);

                if( regValue == ctrlRegVal )
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
            /* To configure the clock multiplexer, first set it to default
             * value by disabling peripheral. */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        retState = RCC_REQUEST_OK;
    }

    return ( retState );
}


/**
 * \brief Sets the peripheral clock multiplexer to default clock source.
 *
 * \param clkMuxId [in]: Peripheral clock source identifier
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkMux_Set_ClkInactive( rcc_ClkMuxId_t clkMuxId  )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
    {
        uint32_t    ctrlRegVal  = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal;
        uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
        rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

        rcc_ClkMuxId_t defaultClkMuxId   = rcc_ClkMuxConfig[ clkMuxId ].DefaultClkMuxId;
        uint32_t       defaultCtrlRegVal = rcc_ClkMuxConfig[ defaultClkMuxId ].ClkSrcVal;

        Rcc_Set_RegVal( ctrlRegId, ctrlRegMask, defaultCtrlRegVal );

        for( uint32_t iterationCnt = 0u; RCC_CLKMUX_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask);

            if( regValue == ctrlRegVal )
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

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
