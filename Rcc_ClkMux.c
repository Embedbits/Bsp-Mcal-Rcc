/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.c
 * \ingroup Rcc
 * \brief Rcc module ClkMux component functionality.
 *
 * The ClkMux component controls the peripheral kernel clock multiplexers
 * (clock source selection fields in the RCC BDCR, CCIPR and CCIPR2 registers).
 * All MCU specific knowledge is stored in the constant array rcc_ClkMuxConfig[];
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
 *                     multiplexer (RCC_CLK_MUX_<block>_DEFAULT - first record
 *                     of the multiplexer present on the selected MCU).
 *
 * Records belonging to one multiplexer form a group. They share the same
 * BlockId, ClkMuxRegId, ClkSrcMask and DefaultClkMuxId and differ only in
 * ClkMuxId and ClkSrcVal. Every record is wrapped in the same "#if defined()"
 * guard as the matching enumerator in rcc_ClkMuxId_t, so that the array always
 * matches the selected MCU. The tables are generated from the peripheral
 * specification of the STM32L4 family (peripheral, block and multiplexer
 * records are generated together).
 *
 * \par Rules for adding or changing records
 * - The records must be in exactly the same order as rcc_ClkMuxId_t, because
 *   the array is indexed directly by ClkMuxId. Rcc_ClkMux_Init() verifies
 *   that rcc_ClkMuxConfig[ i ].ClkMuxId == i and a _Static_assert verifies
 *   that the array has RCC_CLK_MUX_LIST_CNT records.
 * - The first record of the group must describe the reset value of the
 *   multiplexer field. Rcc_ClkMux_Set_ClkActive() changes the multiplexer
 *   only if the field currently holds ClkSrcVal of the default record (i.e.
 *   the peripheral was released first) and Rcc_ClkMux_Set_ClkInactive() writes
 *   this value back.
 *
 * \note RTC multiplexer (RTCSEL) can be written only once after backup domain
 *       reset. Rcc_ClkMux_Set_ClkInactive() can not return it to default
 *       value - error is returned.
 * \note USB and RNG use the same 48 MHz clock multiplexer (CLK48SEL). A change
 *       of the source for one of them changes the clock of the other one.
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

/* Default (reset) selection of every multiplexer - first record of the
 * multiplexer present on the selected MCU */
#define RCC_CLK_MUX_RTC_DEFAULT        ( RCC_CLK_MUX_RTC_NONE )
#define RCC_CLK_MUX_LPTIM1_DEFAULT     ( RCC_CLK_MUX_LPTIM1_PCLK1 )
#define RCC_CLK_MUX_LPTIM2_DEFAULT     ( RCC_CLK_MUX_LPTIM2_PCLK1 )
#define RCC_CLK_MUX_I2C1_DEFAULT       ( RCC_CLK_MUX_I2C1_PCLK1 )
#if defined(RCC_APB1ENR1_I2C2EN)
    #define RCC_CLK_MUX_I2C2_DEFAULT       ( RCC_CLK_MUX_I2C2_PCLK1 )
#endif
#define RCC_CLK_MUX_I2C3_DEFAULT       ( RCC_CLK_MUX_I2C3_PCLK1 )
#if defined(RCC_APB1ENR2_I2C4EN)
    #define RCC_CLK_MUX_I2C4_DEFAULT       ( RCC_CLK_MUX_I2C4_PCLK1 )
#endif
#define RCC_CLK_MUX_USART1_DEFAULT     ( RCC_CLK_MUX_USART1_PCLK2 )
#define RCC_CLK_MUX_USART2_DEFAULT     ( RCC_CLK_MUX_USART2_PCLK1 )
#if defined(RCC_APB1ENR1_USART3EN)
    #define RCC_CLK_MUX_USART3_DEFAULT     ( RCC_CLK_MUX_USART3_PCLK1 )
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    #define RCC_CLK_MUX_UART4_DEFAULT      ( RCC_CLK_MUX_UART4_PCLK1 )
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    #define RCC_CLK_MUX_UART5_DEFAULT      ( RCC_CLK_MUX_UART5_PCLK1 )
#endif
#define RCC_CLK_MUX_LPUART1_DEFAULT    ( RCC_CLK_MUX_LPUART1_PCLK1 )
#if defined(RCC_TYPES_USB_SUPPORT) && \
    !defined(RCC_CRRCR_HSI48ON)
    #define RCC_CLK_MUX_USB_DEFAULT        ( RCC_CLK_MUX_USB_NONE )
#elif defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CRRCR_HSI48ON)
    #define RCC_CLK_MUX_USB_DEFAULT        ( RCC_CLK_MUX_USB_HSI48 )
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
    #define RCC_CLK_MUX_SWPMI1_DEFAULT     ( RCC_CLK_MUX_SWPMI1_PCLK1 )
#endif
#if defined(RCC_CCIPR_DFSDM1SEL)
    #define RCC_CLK_MUX_DFSDM1_DEFAULT     ( RCC_CLK_MUX_DFSDM1_PCLK2 )
#elif defined(RCC_CCIPR2_DFSDM1SEL)
    #define RCC_CLK_MUX_DFSDM1_DEFAULT     ( RCC_CLK_MUX_DFSDM1_PCLK2 )
#endif
#if defined(RCC_CCIPR_ADCSEL)
    #define RCC_CLK_MUX_ADC_DEFAULT        ( RCC_CLK_MUX_ADC_HCLK )
#endif
#if !defined(RCC_CRRCR_HSI48ON)
    #define RCC_CLK_MUX_RNG_DEFAULT        ( RCC_CLK_MUX_RNG_NONE )
#elif defined(RCC_CRRCR_HSI48ON)
    #define RCC_CLK_MUX_RNG_DEFAULT        ( RCC_CLK_MUX_RNG_HSI48 )
#endif

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
  { .ClkMuxId = RCC_CLK_MUX_RTC_NONE      , .BlockId = RCC_BLOCK_RTC     , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_RTC_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSE       , .BlockId = RCC_BLOCK_RTC     , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = RCC_BDCR_RTCSEL_0       , .DefaultClkMuxId = RCC_CLK_MUX_RTC_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_RTC_LSI       , .BlockId = RCC_BLOCK_RTC     , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = RCC_BDCR_RTCSEL_1       , .DefaultClkMuxId = RCC_CLK_MUX_RTC_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_RTC_HSE_DIV32 , .BlockId = RCC_BLOCK_RTC     , .ClkMuxRegId = RCC_REG_BDCR  , .ClkSrcMask = RCC_BDCR_RTCSEL       , .ClkSrcVal = RCC_BDCR_RTCSEL         , .DefaultClkMuxId = RCC_CLK_MUX_RTC_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_PCLK1  , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSI    , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM1SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_HSI    , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM1SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM1_LSE    , .BlockId = RCC_BLOCK_LPTIM1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM1SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM1SEL     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_PCLK1  , .BlockId = RCC_BLOCK_LPTIM2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM2SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSI    , .BlockId = RCC_BLOCK_LPTIM2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM2SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM2SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_HSI    , .BlockId = RCC_BLOCK_LPTIM2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM2SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM2SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPTIM2_LSE    , .BlockId = RCC_BLOCK_LPTIM2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPTIM2SEL   , .ClkSrcVal = RCC_CCIPR_LPTIM2SEL     , .DefaultClkMuxId = RCC_CLK_MUX_LPTIM2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_PCLK1    , .BlockId = RCC_BLOCK_I2C1    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C1SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_SYSCLK   , .BlockId = RCC_BLOCK_I2C1    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C1SEL     , .ClkSrcVal = RCC_CCIPR_I2C1SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_I2C1_HSI      , .BlockId = RCC_BLOCK_I2C1    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C1SEL     , .ClkSrcVal = RCC_CCIPR_I2C1SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_I2C1_DEFAULT },
#if defined(RCC_APB1ENR1_I2C2EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_PCLK1    , .BlockId = RCC_BLOCK_I2C2    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C2SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_I2C2EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_SYSCLK   , .BlockId = RCC_BLOCK_I2C2    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C2SEL     , .ClkSrcVal = RCC_CCIPR_I2C2SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_I2C2EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C2_HSI      , .BlockId = RCC_BLOCK_I2C2    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C2SEL     , .ClkSrcVal = RCC_CCIPR_I2C2SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_I2C2_DEFAULT },
#endif
  { .ClkMuxId = RCC_CLK_MUX_I2C3_PCLK1    , .BlockId = RCC_BLOCK_I2C3    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C3SEL     , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_SYSCLK   , .BlockId = RCC_BLOCK_I2C3    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C3SEL     , .ClkSrcVal = RCC_CCIPR_I2C3SEL_0     , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_I2C3_HSI      , .BlockId = RCC_BLOCK_I2C3    , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_I2C3SEL     , .ClkSrcVal = RCC_CCIPR_I2C3SEL_1     , .DefaultClkMuxId = RCC_CLK_MUX_I2C3_DEFAULT },
#if defined(RCC_APB1ENR2_I2C4EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_PCLK1    , .BlockId = RCC_BLOCK_I2C4    , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_I2C4SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_DEFAULT },
#endif
#if defined(RCC_APB1ENR2_I2C4EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_SYSCLK   , .BlockId = RCC_BLOCK_I2C4    , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_I2C4SEL    , .ClkSrcVal = RCC_CCIPR2_I2C4SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_DEFAULT },
#endif
#if defined(RCC_APB1ENR2_I2C4EN)
  { .ClkMuxId = RCC_CLK_MUX_I2C4_HSI      , .BlockId = RCC_BLOCK_I2C4    , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_I2C4SEL    , .ClkSrcVal = RCC_CCIPR2_I2C4SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_I2C4_DEFAULT },
#endif
  { .ClkMuxId = RCC_CLK_MUX_USART1_PCLK2  , .BlockId = RCC_BLOCK_USART1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART1_SYSCLK , .BlockId = RCC_BLOCK_USART1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = RCC_CCIPR_USART1SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_USART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART1_HSI    , .BlockId = RCC_BLOCK_USART1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = RCC_CCIPR_USART1SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_USART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART1_LSE    , .BlockId = RCC_BLOCK_USART1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART1SEL   , .ClkSrcVal = RCC_CCIPR_USART1SEL     , .DefaultClkMuxId = RCC_CLK_MUX_USART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART2_PCLK1  , .BlockId = RCC_BLOCK_USART2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USART2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART2_SYSCLK , .BlockId = RCC_BLOCK_USART2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = RCC_CCIPR_USART2SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_USART2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART2_HSI    , .BlockId = RCC_BLOCK_USART2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = RCC_CCIPR_USART2SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_USART2_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_USART2_LSE    , .BlockId = RCC_BLOCK_USART2  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART2SEL   , .ClkSrcVal = RCC_CCIPR_USART2SEL     , .DefaultClkMuxId = RCC_CLK_MUX_USART2_DEFAULT },
#if defined(RCC_APB1ENR1_USART3EN)
  { .ClkMuxId = RCC_CLK_MUX_USART3_PCLK1  , .BlockId = RCC_BLOCK_USART3  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USART3_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_USART3EN)
  { .ClkMuxId = RCC_CLK_MUX_USART3_SYSCLK , .BlockId = RCC_BLOCK_USART3  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = RCC_CCIPR_USART3SEL_0   , .DefaultClkMuxId = RCC_CLK_MUX_USART3_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_USART3EN)
  { .ClkMuxId = RCC_CLK_MUX_USART3_HSI    , .BlockId = RCC_BLOCK_USART3  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = RCC_CCIPR_USART3SEL_1   , .DefaultClkMuxId = RCC_CLK_MUX_USART3_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_USART3EN)
  { .ClkMuxId = RCC_CLK_MUX_USART3_LSE    , .BlockId = RCC_BLOCK_USART3  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_USART3SEL   , .ClkSrcVal = RCC_CCIPR_USART3SEL     , .DefaultClkMuxId = RCC_CLK_MUX_USART3_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART4EN)
  { .ClkMuxId = RCC_CLK_MUX_UART4_PCLK1   , .BlockId = RCC_BLOCK_UART4   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART4EN)
  { .ClkMuxId = RCC_CLK_MUX_UART4_SYSCLK  , .BlockId = RCC_BLOCK_UART4   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = RCC_CCIPR_UART4SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_UART4_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART4EN)
  { .ClkMuxId = RCC_CLK_MUX_UART4_HSI     , .BlockId = RCC_BLOCK_UART4   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = RCC_CCIPR_UART4SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_UART4_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART4EN)
  { .ClkMuxId = RCC_CLK_MUX_UART4_LSE     , .BlockId = RCC_BLOCK_UART4   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART4SEL    , .ClkSrcVal = RCC_CCIPR_UART4SEL      , .DefaultClkMuxId = RCC_CLK_MUX_UART4_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART5EN)
  { .ClkMuxId = RCC_CLK_MUX_UART5_PCLK1   , .BlockId = RCC_BLOCK_UART5   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART5EN)
  { .ClkMuxId = RCC_CLK_MUX_UART5_SYSCLK  , .BlockId = RCC_BLOCK_UART5   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = RCC_CCIPR_UART5SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_UART5_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART5EN)
  { .ClkMuxId = RCC_CLK_MUX_UART5_HSI     , .BlockId = RCC_BLOCK_UART5   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = RCC_CCIPR_UART5SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_UART5_DEFAULT },
#endif
#if defined(RCC_APB1ENR1_UART5EN)
  { .ClkMuxId = RCC_CLK_MUX_UART5_LSE     , .BlockId = RCC_BLOCK_UART5   , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_UART5SEL    , .ClkSrcVal = RCC_CCIPR_UART5SEL      , .DefaultClkMuxId = RCC_CLK_MUX_UART5_DEFAULT },
#endif
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_PCLK1 , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_SYSCLK, .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = RCC_CCIPR_LPUART1SEL_0  , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_HSI   , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = RCC_CCIPR_LPUART1SEL_1  , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_LPUART1_LSE   , .BlockId = RCC_BLOCK_LPUART1 , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_LPUART1SEL  , .ClkSrcVal = RCC_CCIPR_LPUART1SEL    , .DefaultClkMuxId = RCC_CLK_MUX_LPUART1_DEFAULT },
#if defined(RCC_TYPES_USB_SUPPORT) && \
    !defined(RCC_CRRCR_HSI48ON)
  { .ClkMuxId = RCC_CLK_MUX_USB_NONE      , .BlockId = RCC_BLOCK_USB     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USB_DEFAULT },
#endif
#if defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CRRCR_HSI48ON)
  { .ClkMuxId = RCC_CLK_MUX_USB_HSI48     , .BlockId = RCC_BLOCK_USB     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_USB_DEFAULT },
#endif
#if defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CR_PLLSAI1ON)
  { .ClkMuxId = RCC_CLK_MUX_USB_PLLSAI1Q  , .BlockId = RCC_BLOCK_USB     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_USB_DEFAULT },
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
  { .ClkMuxId = RCC_CLK_MUX_USB_PLLQ      , .BlockId = RCC_BLOCK_USB     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_USB_DEFAULT },
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
  { .ClkMuxId = RCC_CLK_MUX_USB_MSI       , .BlockId = RCC_BLOCK_USB     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL      , .DefaultClkMuxId = RCC_CLK_MUX_USB_DEFAULT },
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
  { .ClkMuxId = RCC_CLK_MUX_SWPMI1_PCLK1  , .BlockId = RCC_BLOCK_SWPMI1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_SWPMI1SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_SWPMI1_DEFAULT },
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
  { .ClkMuxId = RCC_CLK_MUX_SWPMI1_HSI    , .BlockId = RCC_BLOCK_SWPMI1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_SWPMI1SEL   , .ClkSrcVal = RCC_CCIPR_SWPMI1SEL     , .DefaultClkMuxId = RCC_CLK_MUX_SWPMI1_DEFAULT },
#endif
#if defined(RCC_CCIPR_DFSDM1SEL)
  { .ClkMuxId = RCC_CLK_MUX_DFSDM1_PCLK2  , .BlockId = RCC_BLOCK_DFSDM1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_DFSDM1SEL   , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_DFSDM1_DEFAULT },
#endif
#if defined(RCC_CCIPR_DFSDM1SEL)
  { .ClkMuxId = RCC_CLK_MUX_DFSDM1_SYSCLK , .BlockId = RCC_BLOCK_DFSDM1  , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_DFSDM1SEL   , .ClkSrcVal = RCC_CCIPR_DFSDM1SEL     , .DefaultClkMuxId = RCC_CLK_MUX_DFSDM1_DEFAULT },
#endif
#if defined(RCC_CCIPR2_DFSDM1SEL)
  { .ClkMuxId = RCC_CLK_MUX_DFSDM1_PCLK2  , .BlockId = RCC_BLOCK_DFSDM1  , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_DFSDM1SEL  , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_DFSDM1_DEFAULT },
#endif
#if defined(RCC_CCIPR2_DFSDM1SEL)
  { .ClkMuxId = RCC_CLK_MUX_DFSDM1_SYSCLK , .BlockId = RCC_BLOCK_DFSDM1  , .ClkMuxRegId = RCC_REG_CCIPR2, .ClkSrcMask = RCC_CCIPR2_DFSDM1SEL  , .ClkSrcVal = RCC_CCIPR2_DFSDM1SEL    , .DefaultClkMuxId = RCC_CLK_MUX_DFSDM1_DEFAULT },
#endif
#if defined(RCC_CCIPR_ADCSEL)
  { .ClkMuxId = RCC_CLK_MUX_ADC_HCLK      , .BlockId = RCC_BLOCK_ADC     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADCSEL      , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DEFAULT },
#endif
#if defined(RCC_CCIPR_ADCSEL) && \
    defined(RCC_CR_PLLSAI1ON)
  { .ClkMuxId = RCC_CLK_MUX_ADC_PLLSAI1R  , .BlockId = RCC_BLOCK_ADC     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADCSEL      , .ClkSrcVal = RCC_CCIPR_ADCSEL_0      , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DEFAULT },
#endif
#if defined(RCC_CCIPR_ADCSEL) && \
    defined(RCC_TYPES_ADC_PLLSAI2R_SUPPORT)
  { .ClkMuxId = RCC_CLK_MUX_ADC_PLLSAI2R  , .BlockId = RCC_BLOCK_ADC     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADCSEL      , .ClkSrcVal = RCC_CCIPR_ADCSEL_1      , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DEFAULT },
#endif
#if defined(RCC_CCIPR_ADCSEL)
  { .ClkMuxId = RCC_CLK_MUX_ADC_SYSCLK    , .BlockId = RCC_BLOCK_ADC     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_ADCSEL      , .ClkSrcVal = RCC_CCIPR_ADCSEL        , .DefaultClkMuxId = RCC_CLK_MUX_ADC_DEFAULT },
#endif
#if !defined(RCC_CRRCR_HSI48ON)
  { .ClkMuxId = RCC_CLK_MUX_RNG_NONE      , .BlockId = RCC_BLOCK_RNG     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_RNG_DEFAULT },
#endif
#if defined(RCC_CRRCR_HSI48ON)
  { .ClkMuxId = RCC_CLK_MUX_RNG_HSI48     , .BlockId = RCC_BLOCK_RNG     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = 0u                      , .DefaultClkMuxId = RCC_CLK_MUX_RNG_DEFAULT },
#endif
#if defined(RCC_CR_PLLSAI1ON)
  { .ClkMuxId = RCC_CLK_MUX_RNG_PLLSAI1Q  , .BlockId = RCC_BLOCK_RNG     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL_0    , .DefaultClkMuxId = RCC_CLK_MUX_RNG_DEFAULT },
#endif
  { .ClkMuxId = RCC_CLK_MUX_RNG_PLLQ      , .BlockId = RCC_BLOCK_RNG     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL_1    , .DefaultClkMuxId = RCC_CLK_MUX_RNG_DEFAULT },
  { .ClkMuxId = RCC_CLK_MUX_RNG_MSI       , .BlockId = RCC_BLOCK_RNG     , .ClkMuxRegId = RCC_REG_CCIPR , .ClkSrcMask = RCC_CCIPR_CLK48SEL    , .ClkSrcVal = RCC_CCIPR_CLK48SEL      , .DefaultClkMuxId = RCC_CLK_MUX_RNG_DEFAULT },
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
        const uint32_t foreignBits = rcc_ClkMuxConfig[ clkMuxId ].ClkSrcVal & ~rcc_ClkMuxConfig[ clkMuxId ].ClkSrcMask;

        if( clkMuxId != rcc_ClkMuxConfig[ clkMuxId ].ClkMuxId )
        {
            /* Configuration error: Indexes do not match */
            retState = RCC_REQUEST_ERROR;
            break;
        }
        else if( 0u != foreignBits )
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
                const uint32_t regVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

                if( ctrlRegVal == regVal )
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
            const uint32_t regVal = Rcc_Get_RegVal( ctrlRegId, ctrlRegMask );

            if( defaultCtrlRegVal == regVal )
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

        for( rcc_ClkMuxId_t muxId = defaultClkMux; RCC_CLK_MUX_LIST_CNT > muxId; muxId ++ )
        {
            if( ( blockId       != rcc_ClkMuxConfig[ muxId ].BlockId         ) ||
                ( defaultClkMux != rcc_ClkMuxConfig[ muxId ].DefaultClkMuxId )    )
            {
                /* End of the multiplexer group */
                break;
            }
            else if( rcc_ClkMuxConfig[ muxId ].ClkSrcVal == actualRegVal )
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
