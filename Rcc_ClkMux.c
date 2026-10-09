/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.c
 * \ingroup Rcc
 * \brief Rcc module ClkMux component functionality.
 *
 * The ClkMux component controls the peripheral kernel clock multiplexers
 * (clock source selection fields in the RCC BDCR, CCIPR and CCIPR2 registers). All
 * MCU specific knowledge is stored in the constant array rcc_ClkMuxConfig[];
 * the functions only look up a record and read or write the register field
 * described by that record.
 *
 * \par Structure of rcc_ClkMuxConfig[]
 * One record (rcc_ClkMuxConfigStruct_t) describes one selectable clock source
 * of one clock multiplexer:
 * - ClkMuxId        - ID of this record (rcc_ClkMuxId_t, Rcc_ClkMux.h).
 * - BlockId         - Peripheral block clocked through this multiplexer
 *                     (rcc_BlockList_t, Rcc.h).
 * - ClkMuxRegId     - RCC register containing the multiplexer field.
 * - ClkSrcMask      - Bit mask of the multiplexer field in that register.
 * - ClkSrcVal       - Raw field value (within ClkSrcMask) selecting this
 *                     clock source.
 * - DefaultClkMuxId - Record holding the default (reset) selection of this
 *                     multiplexer.
 *
 * Records belonging to one multiplexer form a group. They share the same
 * BlockId, ClkMuxRegId, ClkSrcMask and DefaultClkMuxId and differ only in
 * ClkMuxId and ClkSrcVal. Every group is wrapped in the same
 * "#if defined(<RCC enable bit>)" guard as the matching enumerators in
 * rcc_ClkMuxId_t and rcc_BlockList_t, so that the array always matches the
 * selected MCU.
 *
 * \par Rules for adding or changing records
 * - The records must be in exactly the same order as rcc_ClkMuxId_t, because
 *   the array is indexed directly by ClkMuxId. Rcc_ClkMux_Init() verifies
 *   that rcc_ClkMuxConfig[ i ].ClkMuxId == i and a _Static_assert verifies
 *   that the array has RCC_CLK_MUX_LIST_CNT records.
 * - DefaultClkMuxId must point to the first record of the group for the given
 *   BlockId (e.g. all RCC_CLK_MUX_RTC_xxx records point to
 *   RCC_CLK_MUX_RTC_NONE). The first record of the group must therefore
 *   describe the reset value of the multiplexer field.
 *   Rcc_ClkMux_Set_ClkActive() changes the multiplexer only if the field
 *   currently holds ClkSrcVal of the default record (i.e. the peripheral was
 *   released first) and Rcc_ClkMux_Set_ClkInactive() writes this value back.
 *
 * \note RTC multiplexer (RTCSEL) can be written only once after backup domain
 *       reset. Rcc_ClkMux_Set_ClkInactive() can not return it to default
 *       value - error is returned.
 *
 * \par Relation to other arrays of the Rcc module
 * - rcc_ConfigStruct[] (Rcc.c), indexed by rcc_PeriphId_t, is the entry point
 *   of the module. Each record binds a peripheral / clock source pair to its
 *   ClkMuxId (RCC_CLK_MUX_LIST_CNT if the peripheral has no multiplexer),
 *   BlockId and ClkSrcId. The BlockId stored in rcc_ClkMuxConfig[] must be
 *   equal to the BlockId of the rcc_ConfigStruct[] records which reference
 *   the same ClkMuxId.
 * - rcc_PeriphBlockConfig[] (Rcc.c), indexed by rcc_BlockList_t, provides
 *   the register bank and the enable / low power / reset bit masks of the
 *   peripheral block.
 * - rcc_RegBankConfig[] (Rcc.c) provides the enable, sleep and reset
 *   registers of the register bank (bus or backup domain).
 * - rcc_PeriphClkSrcConfig[] (Rcc.c), indexed by rcc_ClkSrcId_t, provides the
 *   callbacks returning the frequency of the selected clock source.
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkMux.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
#include "Rcc.h"                            /* Module common header file      */
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
    rcc_BlockList_t         BlockId;          /**< Peripheral block ID              */
    rcc_RegId_t             ClkMuxRegId;      /**< Clock MUX register ID.           */
    uint32_t const          ClkSrcMask;       /**< Clock MUX bit mask.              */
    uint32_t const          ClkSrcVal;        /**< Clock MUX value.                 */
    rcc_ClkMuxId_t          DefaultClkMuxId;  /**< Peripheral default configuration */
}   rcc_ClkMuxConfigStruct_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration array of MCU peripherals with available clock MUX. */
static const rcc_ClkMuxConfigStruct_t   rcc_ClkMuxConfig[ ] =
{
    /*----------------------------- System core ------------------------------*/
  { .ClkMuxId = RCC_CLK_MUX_RTC_NONE         , .BlockId = RCC_BLOCK_RTC    , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE       },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE          , .BlockId = RCC_BLOCK_RTC    , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = RCC_BDCR_RTCSEL_0       , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE       },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI          , .BlockId = RCC_BLOCK_RTC    , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = RCC_BDCR_RTCSEL_1       , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE       },
  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32    , .BlockId = RCC_BLOCK_RTC    , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = RCC_BDCR_RTCSEL         , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE       },

    /*-------------------------------- Timers --------------------------------*/
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1     , .BlockId = RCC_BLOCK_LPTIM1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI       , .BlockId = RCC_BLOCK_LPTIM1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM1SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI       , .BlockId = RCC_BLOCK_LPTIM1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM1SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE       , .BlockId = RCC_BLOCK_LPTIM1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM1SEL     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1   },

    /*----------------------------- Connectivity -----------------------------*/
#if defined(RCC_CCIPR_I2S23SEL)
  { .ClkMuxId = RCC_CLK_MUX_I2S23_SYSCLK     , .BlockId = RCC_BLOCK_I2S23  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2S23SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2S23_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2S23_PLLQ       , .BlockId = RCC_BLOCK_I2S23  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2S23SEL    , .ClkSrcVal = RCC_CCIPR_I2S23SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_I2S23_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2S23_I2S_CKIN   , .BlockId = RCC_BLOCK_I2S23  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2S23SEL    , .ClkSrcVal = RCC_CCIPR_I2S23SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_I2S23_SYSCLK   },
  { .ClkMuxId = RCC_CLK_MUX_I2S23_HSI        , .BlockId = RCC_BLOCK_I2S23  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2S23SEL    , .ClkSrcVal = RCC_CCIPR_I2S23SEL      , .DefaultClkMuxId = RCC_CLK_MUX_I2S23_SYSCLK   },
#endif /* RCC_CCIPR_I2S23SEL */

  { .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1       , .BlockId = RCC_BLOCK_I2C1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C1SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK      , .BlockId = RCC_BLOCK_I2C1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C1SEL     , .ClkSrcVal = RCC_CCIPR_I2C1SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI         , .BlockId = RCC_BLOCK_I2C1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C1SEL     , .ClkSrcVal = RCC_CCIPR_I2C1SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1     },

  { .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1       , .BlockId = RCC_BLOCK_I2C2   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C2SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK      , .BlockId = RCC_BLOCK_I2C2   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C2SEL     , .ClkSrcVal = RCC_CCIPR_I2C2SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI         , .BlockId = RCC_BLOCK_I2C2   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C2SEL     , .ClkSrcVal = RCC_CCIPR_I2C2SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1     },

#if defined(RCC_APB1ENR1_I2C3EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK1       , .BlockId = RCC_BLOCK_I2C3   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C3SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK      , .BlockId = RCC_BLOCK_I2C3   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C3SEL     , .ClkSrcVal = RCC_CCIPR_I2C3SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI         , .BlockId = RCC_BLOCK_I2C3   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C3SEL     , .ClkSrcVal = RCC_CCIPR_I2C3SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK1     },
#endif /* RCC_APB1ENR1_I2C3EN */

#if defined(RCC_APB1ENR2_I2C4EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1       , .BlockId = RCC_BLOCK_I2C4   , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_I2C4SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK      , .BlockId = RCC_BLOCK_I2C4   , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_I2C4SEL    , .ClkSrcVal = RCC_CCIPR2_I2C4SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI         , .BlockId = RCC_BLOCK_I2C4   , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_I2C4SEL    , .ClkSrcVal = RCC_CCIPR2_I2C4SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1     },
#endif /* RCC_APB1ENR2_I2C4EN */

  { .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2     , .BlockId = RCC_BLOCK_USART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2   },
  { .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK    , .BlockId = RCC_BLOCK_USART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = RCC_CCIPR_USART1SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2   },
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI       , .BlockId = RCC_BLOCK_USART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = RCC_CCIPR_USART1SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2   },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE       , .BlockId = RCC_BLOCK_USART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = RCC_CCIPR_USART1SEL     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2   },

  { .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1     , .BlockId = RCC_BLOCK_USART2 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK    , .BlockId = RCC_BLOCK_USART2 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = RCC_CCIPR_USART2SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI       , .BlockId = RCC_BLOCK_USART2 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = RCC_CCIPR_USART2SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE       , .BlockId = RCC_BLOCK_USART2 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = RCC_CCIPR_USART2SEL     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1   },

#if defined(RCC_APB1ENR1_USART3EN)
  { .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1     , .BlockId = RCC_BLOCK_USART3 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK    , .BlockId = RCC_BLOCK_USART3 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = RCC_CCIPR_USART3SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI       , .BlockId = RCC_BLOCK_USART3 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = RCC_CCIPR_USART3SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1   },
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE       , .BlockId = RCC_BLOCK_USART3 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = RCC_CCIPR_USART3SEL     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1   },
#endif /* RCC_APB1ENR1_USART3EN */

  { .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1      , .BlockId = RCC_BLOCK_UART4  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_UART4_SYSCLK     , .BlockId = RCC_BLOCK_UART4  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = RCC_CCIPR_UART4SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI        , .BlockId = RCC_BLOCK_UART4  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = RCC_CCIPR_UART4SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE        , .BlockId = RCC_BLOCK_UART4  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = RCC_CCIPR_UART4SEL      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1    },

#if defined(RCC_APB1ENR1_UART5EN)
  { .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1      , .BlockId = RCC_BLOCK_UART5  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_UART5_SYSCLK     , .BlockId = RCC_BLOCK_UART5  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = RCC_CCIPR_UART5SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI        , .BlockId = RCC_BLOCK_UART5  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = RCC_CCIPR_UART5SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1    },
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE        , .BlockId = RCC_BLOCK_UART5  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = RCC_CCIPR_UART5SEL      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1    },
#endif /* RCC_APB1ENR1_UART5EN */

  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1    , .BlockId = RCC_BLOCK_LPUART1, .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1  },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK   , .BlockId = RCC_BLOCK_LPUART1, .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = RCC_CCIPR_LPUART1SEL_0  , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1  },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI      , .BlockId = RCC_BLOCK_LPUART1, .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = RCC_CCIPR_LPUART1SEL_1  , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1  },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE      , .BlockId = RCC_BLOCK_LPUART1, .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = RCC_CCIPR_LPUART1SEL    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1  },

  { .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE        , .BlockId = RCC_BLOCK_FDCAN  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_FDCANSEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE      },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLLQ       , .BlockId = RCC_BLOCK_FDCAN  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_FDCANSEL    , .ClkSrcVal = RCC_CCIPR_FDCANSEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE      },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PCLK1      , .BlockId = RCC_BLOCK_FDCAN  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_FDCANSEL    , .ClkSrcVal = RCC_CCIPR_FDCANSEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE      },

  { .ClkMuxId = RCC_CLK_MUX_CLK48_HSI48      , .BlockId = RCC_BLOCK_RNG    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_CLK48_HSI48    },
  { .ClkMuxId = RCC_CLK_MUX_CLK48_PLLQ       , .BlockId = RCC_BLOCK_RNG    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_CLK48_HSI48    },

#if defined(RCC_AHB3ENR_QSPIEN)
  { .ClkMuxId = RCC_CLK_MUX_QSPI_SYSCLK      , .BlockId = RCC_BLOCK_QSPI   , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_QSPISEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_QSPI_SYSCLK    },
  { .ClkMuxId = RCC_CLK_MUX_QSPI_HSI         , .BlockId = RCC_BLOCK_QSPI   , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_QSPISEL    , .ClkSrcVal = RCC_CCIPR2_QSPISEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_QSPI_SYSCLK    },
  { .ClkMuxId = RCC_CLK_MUX_QSPI_PLLQ        , .BlockId = RCC_BLOCK_QSPI   , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_QSPISEL    , .ClkSrcVal = RCC_CCIPR2_QSPISEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_QSPI_SYSCLK    },
#endif /* RCC_AHB3ENR_QSPIEN */

    /*------------------------------ Multimedia ------------------------------*/
#if defined(RCC_APB2ENR_SAI1EN)
  { .ClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK      , .BlockId = RCC_BLOCK_SAI1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_SAI1SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK    },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLLQ        , .BlockId = RCC_BLOCK_SAI1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_SAI1SEL     , .ClkSrcVal = RCC_CCIPR_SAI1SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK    },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_I2S_CKIN    , .BlockId = RCC_BLOCK_SAI1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_SAI1SEL     , .ClkSrcVal = RCC_CCIPR_SAI1SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK    },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_HSI         , .BlockId = RCC_BLOCK_SAI1   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_SAI1SEL     , .ClkSrcVal = RCC_CCIPR_SAI1SEL       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_SYSCLK    },
#endif /* RCC_APB2ENR_SAI1EN */

    /*-------------------------------- Analog --------------------------------*/
  { .ClkMuxId = RCC_CLK_MUX_ADC12_HCLK       , .BlockId = RCC_BLOCK_ADC12  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADC12SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_ADC12_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_ADC12_PLLP       , .BlockId = RCC_BLOCK_ADC12  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADC12SEL    , .ClkSrcVal = RCC_CCIPR_ADC12SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_ADC12_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_ADC12_SYSCLK     , .BlockId = RCC_BLOCK_ADC12  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADC12SEL    , .ClkSrcVal = RCC_CCIPR_ADC12SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_ADC12_HCLK     },

#if defined(RCC_AHB2ENR_ADC345EN)
  { .ClkMuxId = RCC_CLK_MUX_ADC345_HCLK      , .BlockId = RCC_BLOCK_ADC345 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADC345SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_ADC345_HCLK    },
  { .ClkMuxId = RCC_CLK_MUX_ADC345_PLLP      , .BlockId = RCC_BLOCK_ADC345 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADC345SEL   , .ClkSrcVal = RCC_CCIPR_ADC345SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_ADC345_HCLK    },
  { .ClkMuxId = RCC_CLK_MUX_ADC345_SYSCLK    , .BlockId = RCC_BLOCK_ADC345 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADC345SEL   , .ClkSrcVal = RCC_CCIPR_ADC345SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_ADC345_HCLK    },
#endif /* RCC_AHB2ENR_ADC345EN */
};

_Static_assert( (sizeof(rcc_ClkMuxConfig) / sizeof(rcc_ClkMuxConfigStruct_t)) == RCC_CLK_MUX_LIST_CNT, "Rcc_ClkMux: rcc_ClkMuxConfig has incorrect size." );

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes peripheral clock multiplexer module.
 *
 * During initialization process, module checks correctness of clock
 * multiplexer configuration array.
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_ClkMux_Init( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_ClkMuxId_t clkMuxId = (rcc_ClkMuxId_t)0u; RCC_CLK_MUX_LIST_CNT > clkMuxId; clkMuxId++ )
    {
        if( clkMuxId != rcc_ClkMuxConfig[ clkMuxId ].ClkMuxId )
        {
            /* Configuration error: Indexes do not match */
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else if( 0u != ( rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal & ~rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask ) )
        {
            /* Configuration error: Clock source value does not belong to the multiplexer field */
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else
        {
            /* Record is valid, proceed to next entry */
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
 * Selection of the already selected clock source is accepted.
 *
 * \param clkMuxId [in]: Peripheral clock source identifier
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkMux_Set_ClkActive( rcc_ClkMuxId_t clkMuxId  )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
    {
        const uint32_t    ctrlRegVal  = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal;
        const uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
        const rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

        const rcc_ClkMuxId_t defaultClkMuxId   = rcc_ClkMuxConfig[ clkMuxId ].DefaultClkMuxId;
        const uint32_t       defaultCtrlRegVal = rcc_ClkMuxConfig[ defaultClkMuxId ].ClkSrcVal;

        /* Check if the clock multiplexer is set to default value (or already to required value). */
        const uint32_t actualRegVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

        if( actualRegVal == ctrlRegVal )
        {
            /* Required clock source is already selected */
            retState = RCC_REQUEST_OK;
        }
        else if( actualRegVal == defaultCtrlRegVal )
        {
            Rcc_Set_RegVal( ctrlRegId, ctrlRegMask, ctrlRegVal );

            for( uint32_t iterationCnt = 0u; RCC_CLKMUX_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                if( ctrlRegVal == Rcc_Get_RegVal( ctrlRegId, ctrlRegMask ) )
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
        retState = RCC_REQUEST_ERROR;
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

    if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
    {
        const uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
        const rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

        const rcc_ClkMuxId_t defaultClkMuxId   = rcc_ClkMuxConfig[ clkMuxId ].DefaultClkMuxId;
        const uint32_t       defaultCtrlRegVal = rcc_ClkMuxConfig[ defaultClkMuxId ].ClkSrcVal;

        Rcc_Set_RegVal( ctrlRegId, ctrlRegMask, defaultCtrlRegVal );

        for( uint32_t iterationCnt = 0u; RCC_CLKMUX_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            if( defaultCtrlRegVal == Rcc_Get_RegVal( ctrlRegId, ctrlRegMask ) )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Write-once field (RTCSEL) keeps the value */
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
 * \brief Returns the peripheral clock multiplexer for required clock source.
 *
 * \param clkMuxIdIn [in]: Any clock multiplexer ID of the peripheral (identifies the multiplexer), value from \ref rcc_ClkMuxId_t.
 * \param clkMuxId  [out]: Peripheral clock source identifier
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkMux_Get_ClkSrc( rcc_ClkMuxId_t clkMuxIdIn, rcc_ClkMuxId_t * const clkMuxId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR        != clkMuxId   ) &&
        ( RCC_CLK_MUX_LIST_CNT > clkMuxIdIn )    )
    {
        const rcc_ClkMuxId_t  defaultClkMux = rcc_ClkMuxConfig[ clkMuxIdIn ].DefaultClkMuxId;
        const rcc_BlockList_t blockId       = rcc_ClkMuxConfig[ clkMuxIdIn ].BlockId;

        const uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxIdIn ].ClkSrcMask;
        const rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxIdIn ].ClkMuxRegId;

        /* Actual multiplexer value is searched in consecutive entries of the same multiplexer */
        const uint32_t actualRegVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

        *clkMuxId = RCC_CLK_MUX_LIST_CNT;

        for( rcc_ClkMuxId_t muxId = defaultClkMux; ( RCC_CLK_MUX_LIST_CNT > muxId                               ) &&
                                                   ( blockId       == rcc_ClkMuxConfig[ muxId ].BlockId         ) &&
                                                   ( defaultClkMux == rcc_ClkMuxConfig[ muxId ].DefaultClkMuxId );    muxId ++ )
        {
            if( rcc_ClkMuxConfig[ muxId ].ClkSrcVal == actualRegVal )
            {
                *clkMuxId = rcc_ClkMuxConfig[ muxId ].ClkMuxId;
                retState  = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next entry of the block */
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

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
