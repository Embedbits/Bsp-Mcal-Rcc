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
 * - ClkSrcVal       - Value selecting this clock source - LL_RCC_xxx_CLKSOURCE_yyy
 *                     constant. Depending on the multiplexer it is either the
 *                     raw field value (within ClkSrcMask) or the LL_CLKSOURCE()
 *                     encoded value (register offset, position, mask and field
 *                     value). Both forms are converted to the field value by
 *                     Rcc_ClkMux_Get_FieldVal().
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
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 , .BlockId = RCC_BLOCK_SYSTICK  , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_HCLKDIV8 , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSI       , .BlockId = RCC_BLOCK_SYSTICK  , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_LSI      , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },
  { .ClkMuxId = RCC_CLK_MUX_SYSTICK_LSE       , .BlockId = RCC_BLOCK_SYSTICK  , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SYSTICKSEL  , .ClkSrcVal = LL_RCC_SYSTICK_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_SYSTICK_HCLK_DIV8 },

  { .ClkMuxId = RCC_CLK_MUX_RTC_NONE          , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_NONE         , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32     , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_HSE_DIV      , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE           , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSE          , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI           , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_BDCR     , .ClkSrcMask = RCC_BDCR_RTCSEL        , .ClkSrcVal = LL_RCC_RTC_CLKSOURCE_LSI          , .DefaultClkMuxId = RCC_CLK_MUX_RTC_NONE          },


  { .ClkMuxId = RCC_CLK_MUX_LPCLK_HSI         , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CKERPSEL    , .ClkSrcVal = LL_RCC_CLKP_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPCLK_HSE         , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CKERPSEL    , .ClkSrcVal = LL_RCC_CLKP_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },
  { .ClkMuxId = RCC_CLK_MUX_LPCLK_CSI         , .BlockId = RCC_BLOCK_RTC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CKERPSEL    , .ClkSrcVal = LL_RCC_CLKP_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_LPCLK_HSI         },

    /*-------------------------------- Timers --------------------------------*/

#if defined(LPTIM1)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL2P      , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PLL3R      , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE        , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI        , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LPCLK      , .BlockId = RCC_BLOCK_LPTIM1   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM1SEL   , .ClkSrcVal = LL_RCC_LPTIM1_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK3      },
#endif /* LPTIM1 */
#if defined(LPTIM2)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL2P      , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PLL3R      , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE        , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI        , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LPCLK      , .BlockId = RCC_BLOCK_LPTIM2   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM2SEL   , .ClkSrcVal = LL_RCC_LPTIM2_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1      },
#endif /* LPTIM2 */
#if defined(LPTIM3)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_PLL2P      , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_PLL3R      , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_LSE        , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_LSI        , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM3_LPCLK      , .BlockId = RCC_BLOCK_LPTIM3   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM3SEL   , .ClkSrcVal = LL_RCC_LPTIM3_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM3_PCLK3      },
#endif /* LPTIM3 */
#if defined(LPTIM4)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_PLL2P      , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_PLL3R      , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_LSE        , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_LSI        , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM4_LPCLK      , .BlockId = RCC_BLOCK_LPTIM4   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM4SEL   , .ClkSrcVal = LL_RCC_LPTIM4_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM4_PCLK3      },
#endif /* LPTIM4 */
#if defined(LPTIM5)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_PLL2P      , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_PLL3R      , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_LSE        , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_LSI        , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM5_LPCLK      , .BlockId = RCC_BLOCK_LPTIM5   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM5SEL   , .ClkSrcVal = LL_RCC_LPTIM5_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM5_PCLK3      },
#endif /* LPTIM5 */
#if defined(LPTIM6)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_PCLK3     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_PLL2P      , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_PLL2P     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_PLL3R      , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_PLL3R     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_LSE        , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_LSI        , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM6_LPCLK      , .BlockId = RCC_BLOCK_LPTIM6   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_LPTIM6SEL   , .ClkSrcVal = LL_RCC_LPTIM6_CLKSOURCE_CLKP      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM6_PCLK3      },
#endif /* LPTIM6 */


    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PLL2P        , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PLL3P        , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SPI1_PIN          , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI1_LPCLK        , .BlockId = RCC_BLOCK_SPI1     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI1SEL     , .ClkSrcVal = LL_RCC_SPI1_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SPI1_PLL1Q        },
#endif /* SPI1 */
#if defined(SPI2)
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PLL2P        , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PLL3P        , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SPI2_PIN          , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI2_LPCLK        , .BlockId = RCC_BLOCK_SPI2     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI2SEL     , .ClkSrcVal = LL_RCC_SPI2_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SPI2_PLL1Q        },
#endif /* SPI2 */
#if defined(SPI3)
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PLL2P        , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PLL3P        , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SPI3_PIN          , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SPI3_LPCLK        , .BlockId = RCC_BLOCK_SPI3     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI3SEL     , .ClkSrcVal = LL_RCC_SPI3_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SPI3_PLL1Q        },
#endif /* SPI3 */
#if defined(SPI4)
  { .ClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_PCLK2       , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_PLL2Q        , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_PLL2Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SPI4_PLL3Q        , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_PLL3Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SPI4_HSI64        , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_CSI          , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI4_HSE          , .BlockId = RCC_BLOCK_SPI4     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI4SEL     , .ClkSrcVal = LL_RCC_SPI4_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_SPI4_PCLK2        },
#endif /* SPI4 */
#if defined(SPI5)
  { .ClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_PLL2Q        , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_PLL2Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SPI5_PLL3Q        , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_PLL3Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SPI5_HSI64        , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_CSI          , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_SPI5_HSE          , .BlockId = RCC_BLOCK_SPI5     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI5SEL     , .ClkSrcVal = LL_RCC_SPI5_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_SPI5_PCLK3        },
#endif /* SPI5 */
#if defined(SPI6)
  { .ClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_PCLK2       , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_PLL2Q        , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_PLL2Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SPI6_PLL3Q        , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_PLL3Q       , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SPI6_HSI64        , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_CSI          , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
  { .ClkMuxId = RCC_CLK_MUX_SPI6_HSE          , .BlockId = RCC_BLOCK_SPI6     , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_SPI6SEL     , .ClkSrcVal = LL_RCC_SPI6_CLKSOURCE_HSE         , .DefaultClkMuxId = RCC_CLK_MUX_SPI6_PCLK2        },
#endif /* SPI6 */


#if defined(I2C1)
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PLL3R        , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
#else
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PLL2R        , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI64        , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_CSI          , .BlockId = RCC_BLOCK_I2C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C1SEL     , .ClkSrcVal = LL_RCC_I2C1_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_PCLK1        },
#endif /* I2C1 */
#if defined(I2C2)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PLL3R        , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
#else
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PLL2R        , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI64        , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
  { .ClkMuxId = RCC_CLK_MUX_I2C2_CSI          , .BlockId = RCC_BLOCK_I2C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C2SEL     , .ClkSrcVal = LL_RCC_I2C2_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_PCLK1        },
#endif /* I2C2 */
#if defined(I2C3)
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PLL3R        , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
#else
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PLL2R        , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI64        , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_CSI          , .BlockId = RCC_BLOCK_I2C3     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C3SEL     , .ClkSrcVal = LL_RCC_I2C3_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_PCLK3        },
#endif /* I2C3 */
#if defined(I2C4)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PLL3R        , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI64        , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
  { .ClkMuxId = RCC_CLK_MUX_I2C4_CSI          , .BlockId = RCC_BLOCK_I2C4     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I2C4SEL     , .ClkSrcVal = LL_RCC_I2C4_CLKSOURCE_CSI         , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_PCLK3        },
#endif /* I2C4 */


#if defined(I3C1)
  { .ClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        , .BlockId = RCC_BLOCK_I3C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_PCLK1       , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_I3C1_PLL3R        , .BlockId = RCC_BLOCK_I3C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
#else
  { .ClkMuxId = RCC_CLK_MUX_I3C1_PLL2R        , .BlockId = RCC_BLOCK_I3C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I3C1_HSI64        , .BlockId = RCC_BLOCK_I3C1     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C1SEL     , .ClkSrcVal = LL_RCC_I3C1_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I3C1_PCLK1        },
#endif /* I3C1 */

#if defined(I3C2)
  { .ClkMuxId = RCC_CLK_MUX_I3C2_PCLK3        , .BlockId = RCC_BLOCK_I3C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_PCLK3       , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK3        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_I3C2_PLL3R        , .BlockId = RCC_BLOCK_I3C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_PLL3R       , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK3        },
#else
  { .ClkMuxId = RCC_CLK_MUX_I3C2_PLL2R        , .BlockId = RCC_BLOCK_I3C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK3        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I3C2_HSI64        , .BlockId = RCC_BLOCK_I3C2     , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_I3C2SEL     , .ClkSrcVal = LL_RCC_I3C2_CLKSOURCE_HSI         , .DefaultClkMuxId = RCC_CLK_MUX_I3C2_PCLK3        },
#endif /* I3C2 */


#if defined(USART1)
  { .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2      , .BlockId = RCC_BLOCK_USART1   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PCLK2     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_PLL2Q      , .BlockId = RCC_BLOCK_USART1   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USART1_PLL3Q      , .BlockId = RCC_BLOCK_USART1   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI        , .BlockId = RCC_BLOCK_USART1   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE        , .BlockId = RCC_BLOCK_USART1   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
  { .ClkMuxId = RCC_CLK_MUX_USART1_CSI        , .BlockId = RCC_BLOCK_USART1   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART1SEL   , .ClkSrcVal = LL_RCC_USART1_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART1_PCLK2      },
#endif /* USART1 */
#if defined(USART2)
  { .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1      , .BlockId = RCC_BLOCK_USART2   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_PLL2Q      , .BlockId = RCC_BLOCK_USART2   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USART2_PLL3Q      , .BlockId = RCC_BLOCK_USART2   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI        , .BlockId = RCC_BLOCK_USART2   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE        , .BlockId = RCC_BLOCK_USART2   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART2_CSI        , .BlockId = RCC_BLOCK_USART2   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART2SEL   , .ClkSrcVal = LL_RCC_USART2_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART2_PCLK1      },
#endif /* USART2 */
#if defined(USART3)
  { .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1      , .BlockId = RCC_BLOCK_USART3   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_PLL2Q      , .BlockId = RCC_BLOCK_USART3   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USART3_PLL3Q      , .BlockId = RCC_BLOCK_USART3   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI        , .BlockId = RCC_BLOCK_USART3   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE        , .BlockId = RCC_BLOCK_USART3   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART3_CSI        , .BlockId = RCC_BLOCK_USART3   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART3SEL   , .ClkSrcVal = LL_RCC_USART3_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART3_PCLK1      },
#endif /* USART3 */
#if defined(USART6)
  { .ClkMuxId = RCC_CLK_MUX_USART6_PCLK1      , .BlockId = RCC_BLOCK_USART6   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_PLL2Q      , .BlockId = RCC_BLOCK_USART6   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USART6_PLL3Q      , .BlockId = RCC_BLOCK_USART6   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART6_HSI        , .BlockId = RCC_BLOCK_USART6   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_LSE        , .BlockId = RCC_BLOCK_USART6   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_USART6_CSI        , .BlockId = RCC_BLOCK_USART6   , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART6SEL   , .ClkSrcVal = LL_RCC_USART6_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_USART6_PCLK1      },
#endif /* USART6 */
#if defined(USART10)
  { .ClkMuxId = RCC_CLK_MUX_USART10_PCLK1     , .BlockId = RCC_BLOCK_USART10  , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_PCLK1    , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_PLL2Q     , .BlockId = RCC_BLOCK_USART10  , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USART10_PLL3Q     , .BlockId = RCC_BLOCK_USART10  , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_PLL3Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART10_HSI       , .BlockId = RCC_BLOCK_USART10  , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_HSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_LSE       , .BlockId = RCC_BLOCK_USART10  , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART10_CSI       , .BlockId = RCC_BLOCK_USART10  , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_USART10SEL  , .ClkSrcVal = LL_RCC_USART10_CLKSOURCE_CSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART10_PCLK1     },
#endif /* USART10 */
#if defined(USART11)
  { .ClkMuxId = RCC_CLK_MUX_USART11_PCLK1     , .BlockId = RCC_BLOCK_USART11  , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_PCLK1    , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_PLL2Q     , .BlockId = RCC_BLOCK_USART11  , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USART11_PLL3Q     , .BlockId = RCC_BLOCK_USART11  , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_PLL3Q    , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART11_HSI       , .BlockId = RCC_BLOCK_USART11  , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_HSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_LSE       , .BlockId = RCC_BLOCK_USART11  , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
  { .ClkMuxId = RCC_CLK_MUX_USART11_CSI       , .BlockId = RCC_BLOCK_USART11  , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_USART11SEL  , .ClkSrcVal = LL_RCC_USART11_CLKSOURCE_CSI      , .DefaultClkMuxId = RCC_CLK_MUX_USART11_PCLK1     },
#endif /* USART11 */


#if defined(UART4)
  { .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1       , .BlockId = RCC_BLOCK_UART4    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_PLL2Q       , .BlockId = RCC_BLOCK_UART4    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_UART4_PLL3Q       , .BlockId = RCC_BLOCK_UART4    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
#endif
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI         , .BlockId = RCC_BLOCK_UART4    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE         , .BlockId = RCC_BLOCK_UART4    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART4_CSI         , .BlockId = RCC_BLOCK_UART4    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART4SEL    , .ClkSrcVal = LL_RCC_UART4_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART4_PCLK1       },
#endif /* UART4 */
#if defined(UART5)
  { .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1       , .BlockId = RCC_BLOCK_UART5    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_PLL2Q       , .BlockId = RCC_BLOCK_UART5    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_UART5_PLL3Q       , .BlockId = RCC_BLOCK_UART5    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
#endif
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI         , .BlockId = RCC_BLOCK_UART5    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE         , .BlockId = RCC_BLOCK_UART5    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART5_CSI         , .BlockId = RCC_BLOCK_UART5    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART5SEL    , .ClkSrcVal = LL_RCC_UART5_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART5_PCLK1       },
#endif /* USART5 */
#if defined(UART7)
  { .ClkMuxId = RCC_CLK_MUX_UART7_PCLK1       , .BlockId = RCC_BLOCK_UART7    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_PLL2Q       , .BlockId = RCC_BLOCK_UART7    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_UART7_PLL3Q       , .BlockId = RCC_BLOCK_UART7    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
#endif
  { .ClkMuxId = RCC_CLK_MUX_UART7_HSI         , .BlockId = RCC_BLOCK_UART7    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_LSE         , .BlockId = RCC_BLOCK_UART7    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART7_CSI         , .BlockId = RCC_BLOCK_UART7    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART7SEL    , .ClkSrcVal = LL_RCC_UART7_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART7_PCLK1       },
#endif /* USART5 */
#if defined(UART8)
  { .ClkMuxId = RCC_CLK_MUX_UART8_PCLK1       , .BlockId = RCC_BLOCK_UART8    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_PLL2Q       , .BlockId = RCC_BLOCK_UART8    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_UART8_PLL3Q       , .BlockId = RCC_BLOCK_UART8    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
#endif
  { .ClkMuxId = RCC_CLK_MUX_UART8_HSI         , .BlockId = RCC_BLOCK_UART8    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_LSE         , .BlockId = RCC_BLOCK_UART8    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART8_CSI         , .BlockId = RCC_BLOCK_UART8    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART8SEL    , .ClkSrcVal = LL_RCC_UART8_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART8_PCLK1       },
#endif /* USART5 */
#if defined(UART9)
  { .ClkMuxId = RCC_CLK_MUX_UART9_PCLK1       , .BlockId = RCC_BLOCK_UART9    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_PCLK1      , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_PLL2Q       , .BlockId = RCC_BLOCK_UART9    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_UART9_PLL3Q       , .BlockId = RCC_BLOCK_UART9    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_PLL3Q      , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
#endif
  { .ClkMuxId = RCC_CLK_MUX_UART9_HSI         , .BlockId = RCC_BLOCK_UART9    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_HSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_LSE         , .BlockId = RCC_BLOCK_UART9    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_LSE        , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
  { .ClkMuxId = RCC_CLK_MUX_UART9_CSI         , .BlockId = RCC_BLOCK_UART9    , .ClkMuxRegId = RCC_REG_CCIPR1   , .ClkSrcMask = RCC_CCIPR1_UART9SEL    , .ClkSrcVal = LL_RCC_UART9_CLKSOURCE_CSI        , .DefaultClkMuxId = RCC_CLK_MUX_UART9_PCLK1       },
#endif /* USART5 */
#if defined(UART12)
  { .ClkMuxId = RCC_CLK_MUX_UART12_PCLK1      , .BlockId = RCC_BLOCK_UART12   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_PCLK1     , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_PLL2Q      , .BlockId = RCC_BLOCK_UART12   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_PLL2Q     , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_UART12_PLL3Q      , .BlockId = RCC_BLOCK_UART12   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_PLL3Q     , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
#endif
  { .ClkMuxId = RCC_CLK_MUX_UART12_HSI        , .BlockId = RCC_BLOCK_UART12   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_LSE        , .BlockId = RCC_BLOCK_UART12   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
  { .ClkMuxId = RCC_CLK_MUX_UART12_CSI        , .BlockId = RCC_BLOCK_UART12   , .ClkMuxRegId = RCC_REG_CCIPR2   , .ClkSrcMask = RCC_CCIPR2_UART12SEL   , .ClkSrcVal = LL_RCC_UART12_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_UART12_PCLK1      },
#endif /* USART5 */


#if defined(LPUART1)
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PCLK3    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL2Q     , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PLL3Q     , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_PLL2Q    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI       , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_HSI      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE       , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_LSE      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_CSI       , .BlockId = RCC_BLOCK_LPUART1  , .ClkMuxRegId = RCC_REG_CCIPR3   , .ClkSrcMask = RCC_CCIPR3_LPUART1SEL  , .ClkSrcVal = LL_RCC_LPUART1_CLKSOURCE_CSI      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_PCLK3     },
#endif /* LPUART1 */


#if defined(FDCAN1)
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL1Q       , .BlockId = RCC_BLOCK_FDCAN    , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL1Q      , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_PLL2Q       , .BlockId = RCC_BLOCK_FDCAN    , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_PLL2Q      , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
  { .ClkMuxId = RCC_CLK_MUX_FDCAN_HSE         , .BlockId = RCC_BLOCK_FDCAN    , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_FDCANSEL    , .ClkSrcVal = LL_RCC_FDCAN_CLKSOURCE_HSE        , .DefaultClkMuxId = RCC_CLK_MUX_FDCAN_HSE         },
#endif /* FDCAN1 */


#if defined(SDMMC1)
  { .ClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      , .BlockId = RCC_BLOCK_SDMMC1   , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC1SEL   , .ClkSrcVal = LL_RCC_SDMMC1_CLKSOURCE_PLL1Q     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      },
  { .ClkMuxId = RCC_CLK_MUX_SDMMC1_PLL2R      , .BlockId = RCC_BLOCK_SDMMC1   , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC1SEL   , .ClkSrcVal = LL_RCC_SDMMC1_CLKSOURCE_PLL2R     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC1_PLL1Q      },
#endif /* SDMMC1 */
#if defined(SDMMC2)
  { .ClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      , .BlockId = RCC_BLOCK_SDMMC2   , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC2SEL   , .ClkSrcVal = LL_RCC_SDMMC2_CLKSOURCE_PLL1Q     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      },
  { .ClkMuxId = RCC_CLK_MUX_SDMMC2_PLL2R      , .BlockId = RCC_BLOCK_SDMMC2   , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_SDMMC2SEL   , .ClkSrcVal = LL_RCC_SDMMC2_CLKSOURCE_PLL2R     , .DefaultClkMuxId = RCC_CLK_MUX_SDMMC2_PLL1Q      },
#endif /* SDMMC2 */


#if defined(OCTOSPI1)
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     , .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_HCLK        , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_PLL1Q    , .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_PLL2R    , .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_PLL2R       , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
  { .ClkMuxId = RCC_CLK_MUX_OCTOSPI1_LPCLK    , .BlockId = RCC_BLOCK_OCTOSPI1 , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_OCTOSPISEL  , .ClkSrcVal = LL_RCC_OSPI_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_OCTOSPI1_HCLK     },
#endif /* OCTOSPI1 || OCTOSPI2 */

#if defined(USB_DRD_FS)
  { .ClkMuxId = RCC_CLK_MUX_USB_NONE          , .BlockId = RCC_BLOCK_USB      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_NONE         , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL1Q         , .BlockId = RCC_BLOCK_USB      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_PLL1Q        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL3Q         , .BlockId = RCC_BLOCK_USB      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_PLL3Q        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
#else
  { .ClkMuxId = RCC_CLK_MUX_USB_PLL2Q         , .BlockId = RCC_BLOCK_USB      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_PLL2Q        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USB_HSI48         , .BlockId = RCC_BLOCK_USB      , .ClkMuxRegId = RCC_REG_CCIPR4   , .ClkSrcMask = RCC_CCIPR4_USBSEL      , .ClkSrcVal = LL_RCC_USB_CLKSOURCE_HSI48        , .DefaultClkMuxId = RCC_CLK_MUX_USB_NONE          },
#endif /* USB_OTG_HS */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(SAI1)
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL2P        , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL3P        , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_CKIN         , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI1_LPCLK        , .BlockId = RCC_BLOCK_SAI1     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI1SEL     , .ClkSrcVal = LL_RCC_SAI1_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SAI1_PLL1Q        },
#endif /* SAI1 */
#if defined(SAI2)
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL2P        , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL2P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
#if defined(RCC_CR_PLL3ON)
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL3P        , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL3P       , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
#endif
  { .ClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PLL1Q       , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_CKIN         , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_PIN         , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
  { .ClkMuxId = RCC_CLK_MUX_SAI2_LPCLK        , .BlockId = RCC_BLOCK_SAI2     , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_SAI2SEL     , .ClkSrcVal = LL_RCC_SAI2_CLKSOURCE_CLKP        , .DefaultClkMuxId = RCC_CLK_MUX_SAI2_PLL1Q        },
#endif /* SAI2 */


#if defined(CEC)
  { .ClkMuxId = RCC_CLK_MUX_CEC_LSE           , .BlockId = RCC_BLOCK_CEC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CECSEL      , .ClkSrcVal = LL_RCC_CEC_CLKSOURCE_LSE          , .DefaultClkMuxId = RCC_CLK_MUX_CEC_LSE           },
  { .ClkMuxId = RCC_CLK_MUX_CEC_CSI_DIV       , .BlockId = RCC_BLOCK_CEC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CECSEL      , .ClkSrcVal = LL_RCC_CEC_CLKSOURCE_CSI_DIV122   , .DefaultClkMuxId = RCC_CLK_MUX_CEC_LSE           },
  { .ClkMuxId = RCC_CLK_MUX_CEC_LSI           , .BlockId = RCC_BLOCK_CEC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_CECSEL      , .ClkSrcVal = LL_RCC_CEC_CLKSOURCE_LSI          , .DefaultClkMuxId = RCC_CLK_MUX_CEC_LSE           },
#endif /* CEC */

    /*-------------------------------- Analog --------------------------------*/

#if defined(ADC1) || defined(ADC2) || defined(DAC1)
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      , .BlockId = RCC_BLOCK_ADC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HCLK      , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_SYSCLK    , .BlockId = RCC_BLOCK_ADC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_SYSCLK    , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_PLL2R     , .BlockId = RCC_BLOCK_ADC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_PLL2R     , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSE       , .BlockId = RCC_BLOCK_ADC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HSE       , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_HSI       , .BlockId = RCC_BLOCK_ADC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_HSI       , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
  { .ClkMuxId = RCC_CLK_MUX_ADC_DAC_CSI       , .BlockId = RCC_BLOCK_ADC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_ADCDACSEL   , .ClkSrcVal = LL_RCC_ADCDAC_CLKSOURCE_CSI       , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DAC_HCLK      },
#endif /* ADC1 || ADC2 || DAC1 */

#if defined(DAC1)
  { .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       , .BlockId = RCC_BLOCK_DAC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_DACSEL      , .ClkSrcVal = LL_RCC_DAC_LP_CLKSOURCE_LSE       , .DefaultClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       },
  { .ClkMuxId = RCC_CLK_MUX_DAC_SAH_LSI       , .BlockId = RCC_BLOCK_DAC      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_DACSEL      , .ClkSrcVal = LL_RCC_DAC_LP_CLKSOURCE_LSI       , .DefaultClkMuxId = RCC_CLK_MUX_DAC_SAH_LSE       },
#endif /* DAC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(RNG)
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48         , .BlockId = RCC_BLOCK_RNG      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_HSI48        , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .ClkMuxId = RCC_CLK_MUX_RNG_PLL1Q         , .BlockId = RCC_BLOCK_RNG      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_PLL1Q        , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .ClkMuxId = RCC_CLK_MUX_RNG_LSE           , .BlockId = RCC_BLOCK_RNG      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_LSE          , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
  { .ClkMuxId = RCC_CLK_MUX_RNG_LSI           , .BlockId = RCC_BLOCK_RNG      , .ClkMuxRegId = RCC_REG_CCIPR5   , .ClkSrcMask = RCC_CCIPR5_RNGSEL      , .ClkSrcVal = LL_RCC_RNG_CLKSOURCE_LSI          , .DefaultClkMuxId = RCC_CLK_MUX_RNG_HSI48         },
#endif /* RNG */
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
        if( clkMuxId != rcc_ClkMuxConfig[ clkMuxId ].ClkMuxId )
        {
            /* Configuration error: Indexes do not match */
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else if( RCC_REQUEST_OK != Rcc_ClkMux_Check_Record( clkMuxId ) )
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
            if( Rcc_ClkMux_Get_FieldVal( muxId ) == actualRegVal )
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
 * ClkSrcVal holds either the raw field value (always within ClkSrcMask) or the
 * LL_CLKSOURCE() encoded value (bits outside of ClkSrcMask - register offset,
 * position and mask), which is converted by LL_CLKSOURCE_CONFIG().
 *
 * \param clkMuxId [in]: Clock multiplexer record identifier (must be valid)
 *
 * \return Field value to be written to / compared with the masked register value.
 */
static uint32_t Rcc_ClkMux_Get_FieldVal( rcc_ClkMuxId_t clkMuxId )
{
    const uint32_t clkSrcVal  = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal;
    const uint32_t clkSrcMask = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;
    uint32_t       fieldVal   = clkSrcVal;

    if( 0u != ( clkSrcVal & ~clkSrcMask ) )
    {
        /* LL_CLKSOURCE() encoded value */
        fieldVal = LL_CLKSOURCE_CONFIG( clkSrcVal );
    }
    else
    {
        /* Raw field value */
    }

    return ( fieldVal );
}


/**
 * \brief Checks that the clock source value of the record belongs to the multiplexer field of the record.
 *
 * \param clkMuxId [in]: Clock multiplexer record identifier (must be valid)
 *
 * \return Returns "OK" if the raw value lies within ClkSrcMask or the encoded value
 *         describes the same field (mask), otherwise returns error.
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
    else if( clkSrcMask == LL_CLKSOURCE_MASK( clkSrcVal ) )
    {
        /* LL_CLKSOURCE() encoded value of the same field */
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
