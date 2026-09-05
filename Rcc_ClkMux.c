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
  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32 , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL     , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_HSE_DIV32 },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE       , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL     , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSE       },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI       , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL     , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSI       },

  /*--------------------------------- Timers ---------------------------------*/

#if defined(LPTIM1)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_APB1   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PCLK1  },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSI    },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI16  , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_HSI    },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSE    },
#endif /* LPTIM1 */

  /*------------------------------ Connectivity ------------------------------*/

#if defined(SPI2) || defined(SPI3)
  { .ClkMuxId = RCC_CLK_MUX_I2S_SYSCLK    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2S23SEL  , .ClkSrcVal = LL_RCC_I2S_CLKSOURCE_SYSCLK    },
  { .ClkMuxId = RCC_CLK_MUX_I2S_PLLQ      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2S23SEL  , .ClkSrcVal = LL_RCC_I2S_CLKSOURCE_PLL       },
  { .ClkMuxId = RCC_CLK_MUX_I2S_HSI16     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2S23SEL  , .ClkSrcVal = LL_RCC_I2S_CLKSOURCE_HSI       },
  { .ClkMuxId = RCC_CLK_MUX_I2S_CKIN      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2S23SEL  , .ClkSrcVal = LL_RCC_I2S_CLKSOURCE_PIN       },
#endif /* SPI2 */

#if defined(I2C1)
  { .ClkMuxId = RCC_CLK_MUX_I2C1_APB1     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C1SEL   , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C1SEL   , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI16    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C1SEL   , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_HSI      },
#endif /* I2C1 */
#if defined(I2C2)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_APB1     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C2SEL   , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C2SEL   , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI16    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C2SEL   , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_HSI      },
#endif /* I2C2 */
#if defined(I2C3)
  { .ClkMuxId = RCC_CLK_MUX_I2C3_APB1     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C3SEL   , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C3SEL   , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI16    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_I2C3SEL   , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_HSI      },
#endif /* I2C3 */
#if defined(I2C4)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_APB1     , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C4SEL  , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK   , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C4SEL  , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI16    , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C4SEL  , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_HSI      },
#endif /* I2C4 */

#if defined(USART1)
  { .ClkMuxId = RCC_CLK_MUX_USART1_APB2   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART1SEL , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PCLK2  },
  { .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART1SEL , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_SYSCLK },
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART1SEL , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_HSI    },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART1SEL , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_LSE    },
#endif /* USART1 */
#if defined(USART2)
  { .ClkMuxId = RCC_CLK_MUX_USART2_APB1   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART2SEL , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PCLK1  },
  { .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART2SEL , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_SYSCLK },
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART2SEL , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_HSI    },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART2SEL , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_LSE    },
#endif /* USART2 */
#if defined(USART3)
  { .ClkMuxId = RCC_CLK_MUX_USART3_APB1   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART3SEL , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PCLK1  },
  { .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART3SEL , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_SYSCLK },
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART3SEL , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_HSI    },
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_USART3SEL , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_LSE    },
#endif /* USART3 */

#if defined(UART4)
  { .ClkMuxId = RCC_CLK_MUX_UART4_APB1    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART4SEL  , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART4_SYSCLK , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART4SEL  , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_SYSCLK  },
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART4SEL  , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_HSI     },
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART4SEL  , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_LSE     },
#endif /* UART4 */
#if defined(UART5)
  { .ClkMuxId = RCC_CLK_MUX_UART5_APB1    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART5SEL  , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART5_SYSCLK , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART5SEL  , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_SYSCLK  },
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI16   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART5SEL  , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_HSI     },
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_UART5SEL  , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_LSE     },
#endif /* USART5 */

#if defined(LPUART1)
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_APB1  , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPUART1SEL, .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PCLK1 },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK, .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPUART1SEL, .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_SYSCLK},
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI16 , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPUART1SEL, .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_HSI   },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_LPUART1SEL, .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_LSE   },
#endif /* LPUART1 */

#if defined(FDCAN1)
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_APB1    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_FDCANSEL  , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLLQ    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_FDCANSEL  , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL     },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_FDCANSEL  , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_HSE     },
#endif /* FDCAN1 */

#if defined(QUADSPI)
  { .ClkMuxId = RCC_CLK_MUX_QUADSPI_SYSCLK, .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_QSPISEL  , .ClkSrcVal = LL_RCC_QUADSPI_CLKSOURCE_SYSCLK},
  { .ClkMuxId = RCC_CLK_MUX_QUADSPI_PLLQ  , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_QSPISEL  , .ClkSrcVal = LL_RCC_QUADSPI_CLKSOURCE_PLL   },
  { .ClkMuxId = RCC_CLK_MUX_QUADSPI_HSI16 , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_QSPISEL  , .ClkSrcVal = LL_RCC_QUADSPI_CLKSOURCE_HSI   },
#endif /* OCTOSPI1 || OCTOSPI2 */

  /*------------------------------- Multimedia -------------------------------*/

#if defined(SAI1)
  { .ClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_SAI1SEL   , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLLQ     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_SAI1SEL   , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL      },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_HSI16    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_SAI1SEL   , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_HSI      },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_SAI1SEL   , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PIN      },
#endif /* SAI1 */

  /*--------------------------------- Analog ---------------------------------*/

#if defined(ADC12_COMMON)
  { .ClkMuxId = RCC_CLK_MUX_ADC12_SYSCLK  , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_ADC12SEL  , .ClkSrcVal = LL_RCC_ADC12_CLKSOURCE_SYSCLK  },
  { .ClkMuxId = RCC_CLK_MUX_ADC12_PLLP    , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_ADC12SEL  , .ClkSrcVal = LL_RCC_ADC12_CLKSOURCE_PLL     },
#endif /* ADC1 */

#if defined(ADC345_COMMON)
  { .ClkMuxId = RCC_CLK_MUX_ADC345_SYSCLK , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_ADC345SEL , .ClkSrcVal = LL_RCC_ADC345_CLKSOURCE_SYSCLK },
  { .ClkMuxId = RCC_CLK_MUX_ADC345_PLLP   , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_ADC345SEL , .ClkSrcVal = LL_RCC_ADC345_CLKSOURCE_PLL    },
#endif /* ADC1 */

#if defined(RNG) || defined(USB)
  { .ClkMuxId = RCC_CLK_MUX_USB_RNG_PLLQ  , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_CLK48SEL  , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_PLL       },
  { .ClkMuxId = RCC_CLK_MUX_USB_RNG_HSI48 , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR_CLK48SEL  , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_HSI48     },
#endif /* USB_OTG_HS */
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
