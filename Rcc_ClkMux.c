/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.h
 * \ingroup Rcc
 * \brief Rcc module ClkMux component functionality.
 *
 * The ClkMux component controls the peripheral kernel clock multiplexers
 * (clock source selection fields in the RCC CCIPRx and BDCR registers). All
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
 * - ClkSrcVal       - Value of the multiplexer field selecting this clock
 *                     source (masked, not shifted - built from CMSIS bit
 *                     definitions of the field).
 * - DefaultClkMuxId - Record holding the default (reset) selection of this
 *                     multiplexer.
 *
 * Records belonging to one multiplexer form a group. They share the same
 * BlockId, ClkMuxRegId, ClkSrcMask and DefaultClkMuxId and differ only in
 * ClkMuxId and ClkSrcVal. Every group is wrapped in the same
 * "#if defined(<PERIPHERAL>)" guard as the matching enumerators in
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
 * - If one BlockId is served by more than one multiplexer (e.g. RCC_BLOCK_DAC
 *   by the ADC_DAC and DAC_SAH multiplexers), each multiplexer forms its own
 *   group with its own default record.
 * - Records without a peripheral block (e.g. SYSTICK) use RCC_BLOCK_LIST_CNT
 *   as BlockId.
 *
 * \par Relation to other arrays of the Rcc module
 * - rcc_ConfigStruct[] (Rcc.c), indexed by rcc_PeriphId_t, is the entry point
 *   of the module. Each record binds a peripheral / clock source pair to its
 *   ClkMuxId (RCC_CLK_MUX_LIST_CNT if the peripheral has no multiplexer),
 *   BlockId and ClkSrcId. The BlockId stored in rcc_ClkMuxConfig[] must be
 *   equal to the BlockId of the rcc_ConfigStruct[] records which reference
 *   the same ClkMuxId.
 * - rcc_PeriphBlockConfig[] (Rcc.c), indexed by rcc_BlockList_t, provides
 *   the bus (ClkBusId) and the enable / low power / reset bit masks of the
 *   peripheral block.
 * - rcc_ClkBusConfigStruct[] (Rcc.c), indexed by rcc_ClkBusId_t, provides the
 *   enable, sleep and reset registers of the bus.
 * - rcc_PeriphClkSrcConfig[] (Rcc.c), indexed by rcc_ClkSrcId_t, provides the
 *   callbacks returning the frequency of the selected clock source.
 *
 * Example - Rcc_Set_PeriphActive( periphId ):
 *   1. ClkMuxId is taken from rcc_ConfigStruct[ periphId ]. If it is valid,
 *      Rcc_ClkMux_Set_ClkActive( ClkMuxId ) selects the kernel clock using
 *      rcc_ClkMuxConfig[ ClkMuxId ].
 *   2. BlockId is taken from the same record and the peripheral clock is
 *      enabled using rcc_PeriphBlockConfig[ BlockId ] and
 *      rcc_ClkBusConfigStruct[ ClkBusId ].
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkMux.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
#include "Rcc.h"                            /* Module common header file      */
#include "Rcc_ClkSrc.h"                     /* Backup domain access           */
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
    volatile uint32_t const ClkSrcMask;       /**< Clock MUX bit mask.              */
    volatile uint32_t const ClkSrcVal;        /**< Clock MUX value.                 */
    rcc_ClkMuxId_t          DefaultClkMuxId;  /**< Peripheral default configuration */
}   rcc_ClkMuxConfigStruct_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static uint32_t           Rcc_ClkMux_Get_FieldVal ( rcc_ClkMuxId_t clkMuxId );
static rcc_RequestState_t Rcc_ClkMux_Check_Record ( rcc_ClkMuxId_t clkMuxId );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration array of MCU peripherals with available clock MUX. */
static const rcc_ClkMuxConfigStruct_t   rcc_ClkMuxConfig[ ] =
{
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 , .BlockId = RCC_BLOCK_SYSTICK     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SYSTICKSEL   , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8  },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI       , .BlockId = RCC_BLOCK_SYSTICK     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SYSTICKSEL   , .ClkSrcVal = RCC_CCIPR1_SYSTICKSEL_0                             , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8  },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE       , .BlockId = RCC_BLOCK_SYSTICK     , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SYSTICKSEL   , .ClkSrcVal = RCC_CCIPR1_SYSTICKSEL_1                             , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8  },
  { .ClkMuxId = RCC_CLK_MUX_RTC_NONE          , .BlockId = RCC_BLOCK_RTC         , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL         , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE           },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE           , .BlockId = RCC_BLOCK_RTC         , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL         , .ClkSrcVal = RCC_BDCR_RTCSEL_0                                   , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE           },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI           , .BlockId = RCC_BLOCK_RTC         , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL         , .ClkSrcVal = RCC_BDCR_RTCSEL_1                                   , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE           },
  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32     , .BlockId = RCC_BLOCK_RTC         , .ClkMuxRegId = RCC_REG_BDCR   , .ClkSrcMask = RCC_BDCR_RTCSEL         , .ClkSrcVal = ( RCC_BDCR_RTCSEL_1 | RCC_BDCR_RTCSEL_0 )           , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE           },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK       , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI        , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL    , .ClkSrcVal = RCC_CCIPR3_LPTIM1SEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI        , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL    , .ClkSrcVal = RCC_CCIPR3_LPTIM1SEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE        , .BlockId = RCC_BLOCK_LPTIM1      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM1SEL    , .ClkSrcVal = ( RCC_CCIPR3_LPTIM1SEL_1 | RCC_CCIPR3_LPTIM1SEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_MSIK        },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI        , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL    , .ClkSrcVal = RCC_CCIPR1_LPTIM2SEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_HSI        , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL    , .ClkSrcVal = RCC_CCIPR1_LPTIM2SEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE        , .BlockId = RCC_BLOCK_LPTIM2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_LPTIM2SEL    , .ClkSrcVal = ( RCC_CCIPR1_LPTIM2SEL_1 | RCC_CCIPR1_LPTIM2SEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK      , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL   , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSI       , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL   , .ClkSrcVal = RCC_CCIPR3_LPTIM34SEL_0                             , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_HSI       , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL   , .ClkSrcVal = RCC_CCIPR3_LPTIM34SEL_1                             , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK       },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM34_LSE       , .BlockId = RCC_BLOCK_LPTIM3      , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPTIM34SEL   , .ClkSrcVal = ( RCC_CCIPR3_LPTIM34SEL_1 | RCC_CCIPR3_LPTIM34SEL_0 ), .DefaultClkMuxId = RCC_CLK_MUX_LPTIM34_MSIK       },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PCLK2        , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI1SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PCLK2         },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_SYSCLK       , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI1SEL      , .ClkSrcVal = RCC_CCIPR1_SPI1SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PCLK2         },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_HSI          , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI1SEL      , .ClkSrcVal = RCC_CCIPR1_SPI1SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PCLK2         },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_MSIK         , .BlockId = RCC_BLOCK_SPI1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI1SEL      , .ClkSrcVal = ( RCC_CCIPR1_SPI1SEL_1 | RCC_CCIPR1_SPI1SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PCLK2         },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PCLK1        , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI2SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_SYSCLK       , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI2SEL      , .ClkSrcVal = RCC_CCIPR1_SPI2SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_HSI          , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI2SEL      , .ClkSrcVal = RCC_CCIPR1_SPI2SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_MSIK         , .BlockId = RCC_BLOCK_SPI2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_SPI2SEL      , .ClkSrcVal = ( RCC_CCIPR1_SPI2SEL_1 | RCC_CCIPR1_SPI2SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PCLK3        , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_SPI3SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_SYSCLK       , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_SPI3SEL      , .ClkSrcVal = RCC_CCIPR3_SPI3SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_HSI          , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_SPI3SEL      , .ClkSrcVal = RCC_CCIPR3_SPI3SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_MSIK         , .BlockId = RCC_BLOCK_SPI3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_SPI3SEL      , .ClkSrcVal = ( RCC_CCIPR3_SPI3SEL_1 | RCC_CCIPR3_SPI3SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C1SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK       , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C1SEL      , .ClkSrcVal = RCC_CCIPR1_I2C1SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI          , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C1SEL      , .ClkSrcVal = RCC_CCIPR1_I2C1SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_MSIK         , .BlockId = RCC_BLOCK_I2C1        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C1SEL      , .ClkSrcVal = ( RCC_CCIPR1_I2C1SEL_1 | RCC_CCIPR1_I2C1SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C2SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK       , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C2SEL      , .ClkSrcVal = RCC_CCIPR1_I2C2SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI          , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C2SEL      , .ClkSrcVal = RCC_CCIPR1_I2C2SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_MSIK         , .BlockId = RCC_BLOCK_I2C2        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C2SEL      , .ClkSrcVal = ( RCC_CCIPR1_I2C2SEL_1 | RCC_CCIPR1_I2C2SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_I2C3SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK       , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_I2C3SEL      , .ClkSrcVal = RCC_CCIPR3_I2C3SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI          , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_I2C3SEL      , .ClkSrcVal = RCC_CCIPR3_I2C3SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_MSIK         , .BlockId = RCC_BLOCK_I2C3        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_I2C3SEL      , .ClkSrcVal = ( RCC_CCIPR3_I2C3SEL_1 | RCC_CCIPR3_I2C3SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3         },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1        , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C4SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK       , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C4SEL      , .ClkSrcVal = RCC_CCIPR1_I2C4SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI          , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C4SEL      , .ClkSrcVal = RCC_CCIPR1_I2C4SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_MSIK         , .BlockId = RCC_BLOCK_I2C4        , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_I2C4SEL      , .ClkSrcVal = ( RCC_CCIPR1_I2C4SEL_1 | RCC_CCIPR1_I2C4SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK1         },
#if defined(I2C5)
  { .ClkMuxId = RCC_CLK_MUX_I2C5_PCLK1        , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C5SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_I2C5_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C5_SYSCLK       , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C5SEL      , .ClkSrcVal = RCC_CCIPR2_I2C5SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C5_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C5_HSI          , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C5SEL      , .ClkSrcVal = RCC_CCIPR2_I2C5SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C5_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C5_MSIK         , .BlockId = RCC_BLOCK_I2C5        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C5SEL      , .ClkSrcVal = ( RCC_CCIPR2_I2C5SEL_1 | RCC_CCIPR2_I2C5SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_I2C5_PCLK1         },
#endif /* I2C5 */
#if defined(I2C6)
  { .ClkMuxId = RCC_CLK_MUX_I2C6_PCLK1        , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C6SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_I2C6_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C6_SYSCLK       , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C6SEL      , .ClkSrcVal = RCC_CCIPR2_I2C6SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C6_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C6_HSI          , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C6SEL      , .ClkSrcVal = RCC_CCIPR2_I2C6SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_I2C6_PCLK1         },
  { .ClkMuxId = RCC_CLK_MUX_I2C6_MSIK         , .BlockId = RCC_BLOCK_I2C6        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_I2C6SEL      , .ClkSrcVal = ( RCC_CCIPR2_I2C6SEL_1 | RCC_CCIPR2_I2C6SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_I2C6_PCLK1         },
#endif /* I2C6 */
  { .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2      , .BlockId = RCC_BLOCK_USART1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART1SEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2       },
  { .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK     , .BlockId = RCC_BLOCK_USART1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART1SEL    , .ClkSrcVal = RCC_CCIPR1_USART1SEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2       },
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI        , .BlockId = RCC_BLOCK_USART1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART1SEL    , .ClkSrcVal = RCC_CCIPR1_USART1SEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2       },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE        , .BlockId = RCC_BLOCK_USART1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART1SEL    , .ClkSrcVal = ( RCC_CCIPR1_USART1SEL_1 | RCC_CCIPR1_USART1SEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2       },
#if defined(USART2)
  { .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1      , .BlockId = RCC_BLOCK_USART2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART2SEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK     , .BlockId = RCC_BLOCK_USART2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART2SEL    , .ClkSrcVal = RCC_CCIPR1_USART2SEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI        , .BlockId = RCC_BLOCK_USART2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART2SEL    , .ClkSrcVal = RCC_CCIPR1_USART2SEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE        , .BlockId = RCC_BLOCK_USART2      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART2SEL    , .ClkSrcVal = ( RCC_CCIPR1_USART2SEL_1 | RCC_CCIPR1_USART2SEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1       },
#endif /* USART2 */
  { .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1      , .BlockId = RCC_BLOCK_USART3      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART3SEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK     , .BlockId = RCC_BLOCK_USART3      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART3SEL    , .ClkSrcVal = RCC_CCIPR1_USART3SEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI        , .BlockId = RCC_BLOCK_USART3      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART3SEL    , .ClkSrcVal = RCC_CCIPR1_USART3SEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE        , .BlockId = RCC_BLOCK_USART3      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_USART3SEL    , .ClkSrcVal = ( RCC_CCIPR1_USART3SEL_1 | RCC_CCIPR1_USART3SEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1       , .BlockId = RCC_BLOCK_UART4       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART4SEL     , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART4_SYSCLK      , .BlockId = RCC_BLOCK_UART4       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART4SEL     , .ClkSrcVal = RCC_CCIPR1_UART4SEL_0                               , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI         , .BlockId = RCC_BLOCK_UART4       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART4SEL     , .ClkSrcVal = RCC_CCIPR1_UART4SEL_1                               , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE         , .BlockId = RCC_BLOCK_UART4       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART4SEL     , .ClkSrcVal = ( RCC_CCIPR1_UART4SEL_1 | RCC_CCIPR1_UART4SEL_0 )   , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1       , .BlockId = RCC_BLOCK_UART5       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART5SEL     , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART5_SYSCLK      , .BlockId = RCC_BLOCK_UART5       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART5SEL     , .ClkSrcVal = RCC_CCIPR1_UART5SEL_0                               , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI         , .BlockId = RCC_BLOCK_UART5       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART5SEL     , .ClkSrcVal = RCC_CCIPR1_UART5SEL_1                               , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE         , .BlockId = RCC_BLOCK_UART5       , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_UART5SEL     , .ClkSrcVal = ( RCC_CCIPR1_UART5SEL_1 | RCC_CCIPR1_UART5SEL_0 )   , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1        },
#if defined(USART6)
  { .ClkMuxId = RCC_CLK_MUX_USART6_PCLK1      , .BlockId = RCC_BLOCK_USART6      , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_USART6SEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART6_SYSCLK     , .BlockId = RCC_BLOCK_USART6      , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_USART6SEL    , .ClkSrcVal = RCC_CCIPR2_USART6SEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART6_HSI        , .BlockId = RCC_BLOCK_USART6      , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_USART6SEL    , .ClkSrcVal = RCC_CCIPR2_USART6SEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_USART6_LSE        , .BlockId = RCC_BLOCK_USART6      , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_USART6SEL    , .ClkSrcVal = ( RCC_CCIPR2_USART6SEL_1 | RCC_CCIPR2_USART6SEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1       },
#endif /* USART6 */
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL   , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK    , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL   , .ClkSrcVal = RCC_CCIPR3_LPUART1SEL_0                             , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI       , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL   , .ClkSrcVal = RCC_CCIPR3_LPUART1SEL_1                             , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE       , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL   , .ClkSrcVal = ( RCC_CCIPR3_LPUART1SEL_1 | RCC_CCIPR3_LPUART1SEL_0 ), .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_MSIK      , .BlockId = RCC_BLOCK_LPUART1     , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL   , .ClkSrcVal = RCC_CCIPR3_LPUART1SEL_2                             , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN1_HSE        , .BlockId = RCC_BLOCK_FDCAN1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_FDCANSEL     , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN1_HSE         },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN1_PLL1Q      , .BlockId = RCC_BLOCK_FDCAN1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_FDCANSEL     , .ClkSrcVal = RCC_CCIPR1_FDCANSEL_0                               , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN1_HSE         },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN1_PLL2P      , .BlockId = RCC_BLOCK_FDCAN1      , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_FDCANSEL     , .ClkSrcVal = RCC_CCIPR1_FDCANSEL_1                               , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN1_HSE         },
#if defined(RCC_APB2ENR_USBEN)
  { .ClkMuxId = RCC_CLK_MUX_USB_HSI48         , .BlockId = RCC_BLOCK_USB         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL2Q         , .BlockId = RCC_BLOCK_USB         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = RCC_CCIPR1_ICLKSEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q         , .BlockId = RCC_BLOCK_USB         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = RCC_CCIPR1_ICLKSEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_USB_MSIK          , .BlockId = RCC_BLOCK_USB         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = ( RCC_CCIPR1_ICLKSEL_1 | RCC_CCIPR1_ICLKSEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
  { .ClkMuxId = RCC_CLK_MUX_USB_HSI48         , .BlockId = RCC_BLOCK_OTG         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL2Q         , .BlockId = RCC_BLOCK_OTG         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = RCC_CCIPR1_ICLKSEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q         , .BlockId = RCC_BLOCK_OTG         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = RCC_CCIPR1_ICLKSEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_USB_MSIK          , .BlockId = RCC_BLOCK_OTG         , .ClkMuxRegId = RCC_REG_CCIPR1 , .ClkSrcMask = RCC_CCIPR1_ICLKSEL      , .ClkSrcVal = ( RCC_CCIPR1_ICLKSEL_1 | RCC_CCIPR1_ICLKSEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_USB_HSI48          },
#endif /* USB */
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK    , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL   , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_MSIK      , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL   , .ClkSrcVal = RCC_CCIPR2_OCTOSPISEL_0                             , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL1Q     , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL   , .ClkSrcVal = RCC_CCIPR2_OCTOSPISEL_1                             , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI_PLL2Q     , .BlockId = RCC_BLOCK_OCTOSPI1    , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_OCTOSPISEL   , .ClkSrcVal = ( RCC_CCIPR2_OCTOSPISEL_1 | RCC_CCIPR2_OCTOSPISEL_0 ), .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI_SYSCLK     },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P        , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI1SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P        , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI1SEL      , .ClkSrcVal = RCC_CCIPR2_SAI1SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1P        , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI1SEL      , .ClkSrcVal = RCC_CCIPR2_SAI1SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PIN          , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI1SEL      , .ClkSrcVal = ( RCC_CCIPR2_SAI1SEL_1 | RCC_CCIPR2_SAI1SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_HSI          , .BlockId = RCC_BLOCK_SAI1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI1SEL      , .ClkSrcVal = RCC_CCIPR2_SAI1SEL_2                                , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL2P         },
#if defined(SAI2)
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P        , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI2SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P        , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI2SEL      , .ClkSrcVal = RCC_CCIPR2_SAI2SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1P        , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI2SEL      , .ClkSrcVal = RCC_CCIPR2_SAI2SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PIN          , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI2SEL      , .ClkSrcVal = ( RCC_CCIPR2_SAI2SEL_1 | RCC_CCIPR2_SAI2SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL2P         },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_HSI          , .BlockId = RCC_BLOCK_SAI2        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_SAI2SEL      , .ClkSrcVal = RCC_CCIPR2_SAI2SEL_2                                , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL2P         },
#endif /* SAI2 */
  { .ClkMuxId = RCC_CLK_MUX_MDF1_HCLK         , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_MDF1SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_MDF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_PLL1P        , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_MDF1SEL      , .ClkSrcVal = RCC_CCIPR2_MDF1SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_MDF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_PLL3Q        , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_MDF1SEL      , .ClkSrcVal = RCC_CCIPR2_MDF1SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_MDF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_PIN          , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_MDF1SEL      , .ClkSrcVal = ( RCC_CCIPR2_MDF1SEL_1 | RCC_CCIPR2_MDF1SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_MDF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_MDF1_MSIK         , .BlockId = RCC_BLOCK_MDF1        , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_MDF1SEL      , .ClkSrcVal = RCC_CCIPR2_MDF1SEL_2                                , .DefaultClkMuxId = RCC_CLK_MUX_MDF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_HCLK         , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADF1SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_ADF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_PLL1P        , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADF1SEL      , .ClkSrcVal = RCC_CCIPR3_ADF1SEL_0                                , .DefaultClkMuxId = RCC_CLK_MUX_ADF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_PLL3Q        , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADF1SEL      , .ClkSrcVal = RCC_CCIPR3_ADF1SEL_1                                , .DefaultClkMuxId = RCC_CLK_MUX_ADF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_PIN          , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADF1SEL      , .ClkSrcVal = ( RCC_CCIPR3_ADF1SEL_1 | RCC_CCIPR3_ADF1SEL_0 )     , .DefaultClkMuxId = RCC_CLK_MUX_ADF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_ADF1_MSIK         , .BlockId = RCC_BLOCK_ADF1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADF1SEL      , .ClkSrcVal = RCC_CCIPR3_ADF1SEL_2                                , .DefaultClkMuxId = RCC_CLK_MUX_ADF1_HCLK          },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL    , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK       },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL    , .ClkSrcVal = RCC_CCIPR3_ADCDACSEL_0                              , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK       },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL    , .ClkSrcVal = RCC_CCIPR3_ADCDACSEL_1                              , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK       },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL    , .ClkSrcVal = ( RCC_CCIPR3_ADCDACSEL_1 | RCC_CCIPR3_ADCDACSEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK       },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI       , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL    , .ClkSrcVal = RCC_CCIPR3_ADCDACSEL_2                              , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK       },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_MSIK      , .BlockId = RCC_BLOCK_ADC12       , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_ADCDACSEL    , .ClkSrcVal = ( RCC_CCIPR3_ADCDACSEL_2 | RCC_CCIPR3_ADCDACSEL_0 ) , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK       },
  { .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_DAC1SEL      , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE        },
  { .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSI       , .BlockId = RCC_BLOCK_DAC1        , .ClkMuxRegId = RCC_REG_CCIPR3 , .ClkSrcMask = RCC_CCIPR3_DAC1SEL      , .ClkSrcVal = RCC_CCIPR3_DAC1SEL                                  , .DefaultClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE        },
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48         , .BlockId = RCC_BLOCK_RNG         , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_RNGSEL       , .ClkSrcVal = 0u                                                  , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48_DIV2    , .BlockId = RCC_BLOCK_RNG         , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_RNGSEL       , .ClkSrcVal = RCC_CCIPR2_RNGSEL_0                                 , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48          },
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI           , .BlockId = RCC_BLOCK_RNG         , .ClkMuxRegId = RCC_REG_CCIPR2 , .ClkSrcMask = RCC_CCIPR2_RNGSEL       , .ClkSrcVal = RCC_CCIPR2_RNGSEL_1                                 , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48          },
};

_Static_assert( (sizeof(rcc_ClkMuxConfig) / sizeof(rcc_ClkMuxConfigStruct_t)) == RCC_CLK_MUX_LIST_CNT, "Rcc_ClkMux: rcc_ClkMuxConfig has incorrect size." );

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
        const rcc_RequestState_t recordState = Rcc_ClkMux_Check_Record( clkMuxId );

        if( clkMuxId != rcc_ClkMuxConfig[ clkMuxId ].ClkMuxId )
        {
            /* Configuration error: Indexes do not match */
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else if( RCC_REQUEST_OK != recordState )
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
    uint32_t           regValue = 0u;

    if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
    {
        uint32_t    ctrlRegVal  = Rcc_ClkMux_Get_FieldVal( clkMuxId );
        uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
        rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

        rcc_ClkMuxId_t defaultClkMuxId   = rcc_ClkMuxConfig[ clkMuxId ].DefaultClkMuxId;
        uint32_t       defaultCtrlRegVal = Rcc_ClkMux_Get_FieldVal( defaultClkMuxId );

        /* Check if the clock multiplexer is set to default value (or already to required value). */
        uint32_t actualRegVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

        if( ( actualRegVal == defaultCtrlRegVal ) ||
            ( actualRegVal == ctrlRegVal        )    )
        {
            if( RCC_REG_BDCR == ctrlRegId )
            {
                /* Multiplexer in backup domain (RTCSEL) - result checked by read-back below */
                (void)Rcc_ClkSrc_Set_BkUpAccess();
            }
            else
            {
                /* Multiplexer outside of backup domain */
            }

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
    uint32_t           regValue = 0u;

    if( RCC_CLK_MUX_LIST_CNT > clkMuxId )
    {
        uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
        rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxId ].ClkMuxRegId;

        rcc_ClkMuxId_t defaultClkMuxId   = rcc_ClkMuxConfig[ clkMuxId ].DefaultClkMuxId;
        uint32_t       defaultCtrlRegVal = Rcc_ClkMux_Get_FieldVal( defaultClkMuxId );

        if( RCC_REG_BDCR == ctrlRegId )
        {
            /* Multiplexer in backup domain (RTCSEL) - result checked by read-back below */
            (void)Rcc_ClkSrc_Set_BkUpAccess();
        }
        else
        {
            /* Multiplexer outside of backup domain */
        }

        Rcc_Set_RegVal( ctrlRegId, ctrlRegMask, defaultCtrlRegVal );

        for( uint32_t iterationCnt = 0u; RCC_CLKMUX_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask);

            if( regValue == defaultCtrlRegVal )
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

        uint32_t    ctrlRegMask = rcc_ClkMuxConfig[ clkMuxIdIn ].ClkSrcMask;
        rcc_RegId_t ctrlRegId   = rcc_ClkMuxConfig[ clkMuxIdIn ].ClkMuxRegId;

        /* Actual multiplexer value is searched in consecutive entries of the same multiplexer */
        uint32_t actualRegVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

        *clkMuxId = RCC_CLK_MUX_LIST_CNT;

        for( rcc_ClkMuxId_t muxId = defaultClkMux; ( RCC_CLK_MUX_LIST_CNT > muxId                               ) &&
                                                   ( blockId       == rcc_ClkMuxConfig[ muxId ].BlockId         ) &&
                                                   ( defaultClkMux == rcc_ClkMuxConfig[ muxId ].DefaultClkMuxId );    muxId ++ )
        {
            const uint32_t fieldVal = Rcc_ClkMux_Get_FieldVal( muxId );

            if( fieldVal == actualRegVal )
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

/**
 * \brief Returns the multiplexer field value (masked, not shifted) selecting the clock source of the record.
 *
 * \param clkMuxId [in]: Clock multiplexer record identifier (must be valid)
 *
 * \return Field value to be written to / compared with the masked register value.
 */
static uint32_t Rcc_ClkMux_Get_FieldVal( rcc_ClkMuxId_t clkMuxId )
{
    const uint32_t fieldVal = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal & rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;

    return ( fieldVal );
}


/**
 * \brief Checks that the clock source value of the record belongs to the multiplexer field of the record.
 *
 * \param clkMuxId [in]: Clock multiplexer record identifier (must be valid)
 *
 * \return Returns "OK" if the value lies within ClkSrcMask, otherwise returns error.
 */
static rcc_RequestState_t Rcc_ClkMux_Check_Record( rcc_ClkMuxId_t clkMuxId )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    const uint32_t     clkSrcVal  = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal;
    const uint32_t     clkSrcMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;

    if( 0u == ( clkSrcVal & ~clkSrcMask ) )
    {
        /* Raw field value */
        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
