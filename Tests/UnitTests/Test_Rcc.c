/**
 * \author Mr.Nobody
 * \file Test_Rcc.c
 * \ingroup Rcc
 * \brief Unit tests of Reset and Clock Control (RCC) module.
 *
 * All components of the module (Rcc, Rcc_Reg, Rcc_ClkSrc, Rcc_Pll, Rcc_ClkBus,
 * Rcc_ClkMux, Rcc_ClkOut) are compiled unchanged with real LL drivers. RCC, PWR,
 * FLASH and SysTick registers are emulated by RegMem (host memory at MCU
 * addresses), GPIO module (clock output pins) is mocked by CMock.
 *
 * \note Emulated registers are plain memory. Ready flags (HSIRDY, PLLRDY, ...)
 *       and system clock switch status (SWS) are not updated by writes. Tests
 *       either preset the state the module waits for, or start HW model
 *       (\ref Ut_Rcc_HwModel) emulating the HW reaction in background thread.
 *       Tests without preset / model check the timeout branches.
 * \note Registers start at 0 after RegMem_Reset(), not at HW reset values
 *       (eg. MSION / MSIRDY are not set, MSISRANGE is 0 - reserved value of the
 *       MSI range after standby, so the MSI system clock (SWS 00) has no known
 *       frequency until the MSI range is preset).
 * \note STM32L4 and STM32L4+ (PWR_CR5_R1MODE) differ in the default
 *       configuration (80 / 120 MHz), flash latency thresholds and range 1
 *       boost mode - expected values are selected by the TEST_RCC_* constants.
 * \note Every test runs as separate process (CTest), static state of the
 *       module (HSE frequency, SystemCoreClock) does not depend on other tests.
 */

/* ============================= INCLUDES =================================== */
#include <stdbool.h>                        /* Atomic builtins arguments      */
#include <string.h>                         /* memset                         */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Rcc_ClkSrc.h"                     /* HSE frequency (Rcc_ClkSrc)     */
#include "Rcc_ClkBus.h"                     /* Bus clock component            */
#include "Rcc_ClkMux.h"                     /* Clock multiplexer component    */
#include "Rcc_ClkOut.h"                     /* Clock output component         */
#include "Rcc_Pll.h"                        /* PLL component                  */
#include "Rcc_Reg.h"                        /* Register access component      */
#include "MockGpio_Port.h"                  /* GPIO module mock               */
/* ============================= TYPEDEFS =================================== */

/** Register bits controlled by RCC for one peripheral */
typedef struct
{
    rcc_PeriphId_t      PeriphId;           /**< Peripheral identification      */
    volatile uint32_t * EnableReg;          /**< Clock enable register          */
    uint32_t            EnableMask;         /**< Clock enable bit               */
    volatile uint32_t * ResetReg;           /**< Reset register                 */
    uint32_t            ResetMask;          /**< Reset bit                      */
    volatile uint32_t * SleepReg;           /**< Clock enable in sleep register */
    uint32_t            SleepMask;          /**< Clock enable in sleep bit      */
}   utRcc_PeriphRegs_t;


/** Expected flash latency of one system clock frequency */
typedef struct
{
    rcc_PwrVoltageScale_t Scale;            /**< Voltage range                  */
    rcc_FreqHz_t          SysClk;           /**< System clock (HCLK) in Hz      */
    uint32_t              Latency;          /**< Expected FLASH_ACR LATENCY     */
}   utRcc_LatencyCase_t;

/* ======================= FORWARD DECLARATIONS ============================= */

static void Ut_Rcc_HwModel              ( void );
static void Ut_Rcc_HwModel_Update       ( volatile uint32_t * const reg, uint32_t mask, uint32_t value );
static void Ut_Rcc_Start_HwModel        ( void );
static void Ut_Rcc_Get_HsiOnlyConfig    ( rcc_ConfigStruct_t * const config );
static void Ut_Rcc_Get_HsePllConfig     ( rcc_ConfigStruct_t * const config );
static void Ut_Rcc_Clear_PeriphRegs     ( void );
static void Ut_Rcc_Check_OtherRegsClear ( volatile uint32_t * const * regList, uint32_t regCnt, volatile uint32_t * const usedReg );
static void Ut_Rcc_Check_Latency        ( const utRcc_LatencyCase_t * const caseList, uint32_t caseCnt );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSE frequency of tests - 8 MHz (ST-LINK MCO / crystal of Nucleo boards) */
#define TEST_RCC_HSE_FREQ_HZ                ( 8000000u )

/** HSE frequency of external clock signal tests (oscillator bypassed) */
#define TEST_RCC_HSE_BYPASS_FREQ_HZ         ( 24000000u )

/** HSI16 frequency */
#define TEST_RCC_HSI_FREQ_HZ                ( 16000000u )

/** MSI frequency of the highest range (MSIRANGE 1011) */
#define TEST_RCC_MSI_48MHZ_HZ               ( 48000000u )

/** MSI frequency of the reset range (MSIRANGE 0110) */
#define TEST_RCC_MSI_4MHZ_HZ                ( 4000000u )

/** System clock of HSE PLL configuration (HSE 8 MHz / 1 * 20 / 2) */
#define TEST_RCC_HSE_PLL_SYSCLK_HZ          ( 80000000u )

/** Main PLL output Q of HSE PLL configuration (VCO 160 MHz / 4) */
#define TEST_RCC_HSE_PLL_Q_HZ               ( 40000000u )

#if defined(PWR_CR5_R1MODE)
/** PLL N of default configuration (HSI 16 MHz / 1 * 15 / 2 = 120 MHz) */
#define TEST_RCC_DEFAULT_PLLN               ( 15u )

/** System clock of default configuration (range 1 boost mode) */
#define TEST_RCC_DEFAULT_SYSCLK_HZ          ( 120000000u )

/** PLL Q field of default configuration (divider 6 - 40 MHz) */
#define TEST_RCC_DEFAULT_PLLQ_FIELD         ( TEST_RCC_PLLQR_DIV6 )

/** Flash latency of default configuration (120 MHz) */
#define TEST_RCC_DEFAULT_LATENCY            ( FLASH_ACR_LATENCY_5WS )

/** PLL N of lower system clock (HSI 16 MHz / 1 * 10 / 2 = 80 MHz, normal mode) */
#define TEST_RCC_LOWER_PLLN                 ( 10u )

/** Lower system clock (range 1 normal mode) */
#define TEST_RCC_LOWER_SYSCLK_HZ            ( 80000000u )

/** Flash latency of 80 MHz in range 1 */
#define TEST_RCC_LATENCY_80MHZ              ( FLASH_ACR_LATENCY_3WS )

/** Flash latency of HSI 16 MHz in range 2 */
#define TEST_RCC_LATENCY_HSI_RANGE2         ( FLASH_ACR_LATENCY_1WS )
#else
/** PLL N of default configuration (HSI 16 MHz / 1 * 10 / 2 = 80 MHz) */
#define TEST_RCC_DEFAULT_PLLN               ( 10u )

/** System clock of default configuration */
#define TEST_RCC_DEFAULT_SYSCLK_HZ          ( 80000000u )

/** PLL Q field of default configuration (divider 4 - 40 MHz) */
#define TEST_RCC_DEFAULT_PLLQ_FIELD         ( TEST_RCC_PLLQR_DIV4 )

/** Flash latency of default configuration (80 MHz) */
#define TEST_RCC_DEFAULT_LATENCY            ( FLASH_ACR_LATENCY_4WS )

/** PLL N of lower system clock (HSI 16 MHz / 1 * 8 / 2 = 64 MHz) */
#define TEST_RCC_LOWER_PLLN                 ( 8u )

/** Lower system clock */
#define TEST_RCC_LOWER_SYSCLK_HZ            ( 64000000u )

/** Flash latency of 80 MHz in range 1 */
#define TEST_RCC_LATENCY_80MHZ              ( FLASH_ACR_LATENCY_4WS )

/** Flash latency of HSI 16 MHz in range 2 */
#define TEST_RCC_LATENCY_HSI_RANGE2         ( FLASH_ACR_LATENCY_2WS )
#endif /* PWR_CR5_R1MODE */

/** PLL VCO frequency of default configuration */
#define TEST_RCC_DEFAULT_VCO_HZ             ( 2u * TEST_RCC_DEFAULT_SYSCLK_HZ )

/** Flash latency used by Rcc_Init() while the clock tree is changed (highest of the family) */
#define TEST_RCC_TRANSITION_LATENCY         ( TEST_RCC_DEFAULT_LATENCY )

/** Count of milliseconds in one second */
#define TEST_RCC_MS_IN_SECOND               ( 1000u )

/** SysTick control register value after SysTick_Config() (HCLK, interrupt, enabled) */
#define TEST_RCC_SYSTICK_CTRL_ACTIVE        ( SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk )

/** Longest SysTick interval at HSI 16 MHz (24-bit reload register) */
#define TEST_RCC_SYSTICK_MAX_MS_HSI         ( ( SysTick_LOAD_RELOAD_Msk + 1u ) / ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) )

/** Field value of PLL Q / R divider 2 (field = divider / 2 - 1) */
#define TEST_RCC_PLLQR_DIV2                 ( 0u )

/** Field value of PLL Q / R divider 4 */
#define TEST_RCC_PLLQR_DIV4                 ( 1u )

/** Field value of PLL Q / R divider 6 */
#define TEST_RCC_PLLQR_DIV6                 ( 2u )

/** Field value of PLL Q / R divider 8 */
#define TEST_RCC_PLLQR_DIV8                 ( 3u )

#if defined(RCC_PLLCFGR_PLLPEN)
/** P divider used by tests (7 - available with P bit and PDIV) */
#define TEST_RCC_PLLP_DIV                   ( 7u )
#else
/** P divider used by tests (output P not available - STM32L41x / L42x) */
#define TEST_RCC_PLLP_DIV                   ( 0u )
#endif /* RCC_PLLCFGR_PLLPEN */

#if defined(RCC_PLLCFGR_PLLPDIV)
/** PLLCFGR bits of output P divider 7 (PDIV field) */
#define TEST_RCC_PLLP_DIV7_BITS             ( ( 7u << RCC_PLLCFGR_PLLPDIV_Pos ) | RCC_PLLCFGR_PLLPEN )
#elif defined(RCC_PLLCFGR_PLLPEN)
/** PLLCFGR bits of output P divider 7 (P bit cleared) */
#define TEST_RCC_PLLP_DIV7_BITS             ( RCC_PLLCFGR_PLLPEN )
#else
/** PLLCFGR bits of output P divider 7 (output not available) */
#define TEST_RCC_PLLP_DIV7_BITS             ( 0u )
#endif /* RCC_PLLCFGR_PLLPDIV */

#if defined(RCC_PLLCFGR_PLLPEN)
/** All PLL output enable bits */
#define TEST_RCC_PLL_OUTPUTS_EN             ( RCC_PLLCFGR_PLLPEN | RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN )
#else
/** All PLL output enable bits (no output P) */
#define TEST_RCC_PLL_OUTPUTS_EN             ( RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN )
#endif /* RCC_PLLCFGR_PLLPEN */

#if defined(RCC_PLLM_DIV_1_16_SUPPORT)
/** Lowest invalid M divider of main PLL */
#define TEST_RCC_PLLM_INVALID               ( 17u )
#else
/** Lowest invalid M divider of main PLL */
#define TEST_RCC_PLLM_INVALID               ( 9u )
#endif /* RCC_PLLM_DIV_1_16_SUPPORT */

#if defined(RCC_CR_PLLSAI2ON)
/** Ready flags of PLLSAI2 */
#define TEST_RCC_CR_PLLSAI2_RDY             ( RCC_CR_PLLSAI2RDY )
#else
/** Ready flags of PLLSAI2 (not available) */
#define TEST_RCC_CR_PLLSAI2_RDY             ( 0u )
#endif /* RCC_CR_PLLSAI2ON */

#if defined(RCC_CR_PLLSAI1ON)
/** Ready flags of PLLSAI1 */
#define TEST_RCC_CR_PLLSAI1_RDY             ( RCC_CR_PLLSAI1RDY )
#else
/** Ready flags of PLLSAI1 (not available) */
#define TEST_RCC_CR_PLLSAI1_RDY             ( 0u )
#endif /* RCC_CR_PLLSAI1ON */

/** Ready flags of RCC_CR emulated by HW model */
#define TEST_RCC_CR_RDY_MASK                ( RCC_CR_HSIRDY | RCC_CR_MSIRDY | RCC_CR_HSERDY | RCC_CR_PLLRDY | \
                                              TEST_RCC_CR_PLLSAI1_RDY | TEST_RCC_CR_PLLSAI2_RDY )

/** All reset source flags of RCC_CSR */
#define TEST_RCC_CSR_RESET_FLAGS            ( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_SFTRSTF  | RCC_CSR_IWDGRSTF | \
                                              RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF | RCC_CSR_OBLRSTF  | RCC_CSR_FWRSTF )

/** PWR_CR1 VOS value of voltage range 1 */
#define TEST_RCC_VOS_RANGE1                 ( PWR_CR1_VOS_0 )

/** PWR_CR1 VOS value of voltage range 2 */
#define TEST_RCC_VOS_RANGE2                 ( PWR_CR1_VOS_1 )

#if defined(RCC_APB1ENR1_USBFSEN)
/** Clock enable register of USB */
#define TEST_RCC_USB_ENR                    ( RCC->APB1ENR1 )
/** Clock enable bit of USB */
#define TEST_RCC_USB_EN                     ( RCC_APB1ENR1_USBFSEN )
#elif defined(RCC_AHB2ENR_OTGFSEN)
/** Clock enable register of USB OTG */
#define TEST_RCC_USB_ENR                    ( RCC->AHB2ENR )
/** Clock enable bit of USB OTG */
#define TEST_RCC_USB_EN                     ( RCC_AHB2ENR_OTGFSEN )
#endif /* RCC_APB1ENR1_USBFSEN */

#if defined(RCC_APB2ENR_SDMMC1EN)
/** Clock enable register of SDMMC1 */
#define TEST_RCC_SDMMC1_ENR                 ( RCC->APB2ENR )
/** Clock enable bit of SDMMC1 */
#define TEST_RCC_SDMMC1_EN                  ( RCC_APB2ENR_SDMMC1EN )
#elif defined(RCC_AHB2ENR_SDMMC1EN)
/** Clock enable register of SDMMC1 */
#define TEST_RCC_SDMMC1_ENR                 ( RCC->AHB2ENR )
/** Clock enable bit of SDMMC1 */
#define TEST_RCC_SDMMC1_EN                  ( RCC_AHB2ENR_SDMMC1EN )
#endif /* RCC_APB2ENR_SDMMC1EN */

/* ============================== MACROS ==================================== */

/** PLLCFGR register value (m, n - divider values, qField / rField - field values, src - PLLSRC) */
#define TEST_RCC_PLLCFGR( m, n, qField, rField, src )                                         \
                                            ( ( (uint32_t)( ( m ) - 1u ) << RCC_PLLCFGR_PLLM_Pos ) | \
                                              ( (uint32_t)( n )          << RCC_PLLCFGR_PLLN_Pos ) | \
                                              ( (uint32_t)( qField )     << RCC_PLLCFGR_PLLQ_Pos ) | \
                                              ( (uint32_t)( rField )     << RCC_PLLCFGR_PLLR_Pos ) | \
                                              ( src ) )

/** Count of items of an array */
#define TEST_RCC_ARRAY_CNT( array )         ( sizeof( array ) / sizeof( ( array )[ 0u ] ) )

/* ========================== LOCAL VARIABLES =============================== */

/** HW model emulates HSE which does not start (crystal missing) */
static volatile bool     utRcc_HseFails = false;

/** AHB prescaler (HPRE) seen by HW model when the system clock switched to PLL */
static volatile uint32_t utRcc_HpreAtPllSwitch = 0xFFFFFFFFu;

/** Peripherals of every register bank (enable, reset and sleep bits) */
static const utRcc_PeriphRegs_t utRcc_PeriphRegs[] =
{
    { RCC_PERIPH_DMA2,          &RCC->AHB1ENR,  RCC_AHB1ENR_DMA2EN,      &RCC->AHB1RSTR,  RCC_AHB1RSTR_DMA2RST,      &RCC->AHB1SMENR,  RCC_AHB1SMENR_DMA2SMEN      },
    { RCC_PERIPH_CRC,           &RCC->AHB1ENR,  RCC_AHB1ENR_CRCEN,       &RCC->AHB1RSTR,  RCC_AHB1RSTR_CRCRST,       &RCC->AHB1SMENR,  RCC_AHB1SMENR_CRCSMEN       },
    { RCC_PERIPH_GPIOA,         &RCC->AHB2ENR,  RCC_AHB2ENR_GPIOAEN,     &RCC->AHB2RSTR,  RCC_AHB2RSTR_GPIOARST,     &RCC->AHB2SMENR,  RCC_AHB2SMENR_GPIOASMEN     },
    { RCC_PERIPH_ADC_HCLK,      &RCC->AHB2ENR,  RCC_AHB2ENR_ADCEN,       &RCC->AHB2RSTR,  RCC_AHB2RSTR_ADCRST,       &RCC->AHB2SMENR,  RCC_AHB2SMENR_ADCSMEN       },
#if defined(RCC_AHB3ENR_QSPIEN)
    { RCC_PERIPH_QSPI,          &RCC->AHB3ENR,  RCC_AHB3ENR_QSPIEN,      &RCC->AHB3RSTR,  RCC_AHB3RSTR_QSPIRST,      &RCC->AHB3SMENR,  RCC_AHB3SMENR_QSPISMEN      },
#endif /* RCC_AHB3ENR_QSPIEN */
#if defined(RCC_AHB3ENR_OSPI1EN)
    { RCC_PERIPH_OSPI1,         &RCC->AHB3ENR,  RCC_AHB3ENR_OSPI1EN,     &RCC->AHB3RSTR,  RCC_AHB3RSTR_OSPI1RST,     &RCC->AHB3SMENR,  RCC_AHB3SMENR_OSPI1SMEN     },
#endif /* RCC_AHB3ENR_OSPI1EN */
    { RCC_PERIPH_TIM2,          &RCC->APB1ENR1, RCC_APB1ENR1_TIM2EN,     &RCC->APB1RSTR1, RCC_APB1RSTR1_TIM2RST,     &RCC->APB1SMENR1, RCC_APB1SMENR1_TIM2SMEN     },
    { RCC_PERIPH_USART2_PCLK1,  &RCC->APB1ENR1, RCC_APB1ENR1_USART2EN,   &RCC->APB1RSTR1, RCC_APB1RSTR1_USART2RST,   &RCC->APB1SMENR1, RCC_APB1SMENR1_USART2SMEN   },
    { RCC_PERIPH_PWR,           &RCC->APB1ENR1, RCC_APB1ENR1_PWREN,      &RCC->APB1RSTR1, RCC_APB1RSTR1_PWRRST,      &RCC->APB1SMENR1, RCC_APB1SMENR1_PWRSMEN      },
    { RCC_PERIPH_LPUART1_PCLK1, &RCC->APB1ENR2, RCC_APB1ENR2_LPUART1EN,  &RCC->APB1RSTR2, RCC_APB1RSTR2_LPUART1RST,  &RCC->APB1SMENR2, RCC_APB1SMENR2_LPUART1SMEN  },
    { RCC_PERIPH_LPTIM2_PCLK1,  &RCC->APB1ENR2, RCC_APB1ENR2_LPTIM2EN,   &RCC->APB1RSTR2, RCC_APB1RSTR2_LPTIM2RST,   &RCC->APB1SMENR2, RCC_APB1SMENR2_LPTIM2SMEN   },
    { RCC_PERIPH_TIM1,          &RCC->APB2ENR,  RCC_APB2ENR_TIM1EN,      &RCC->APB2RSTR,  RCC_APB2RSTR_TIM1RST,      &RCC->APB2SMENR,  RCC_APB2SMENR_TIM1SMEN      },
    { RCC_PERIPH_USART1_PCLK2,  &RCC->APB2ENR,  RCC_APB2ENR_USART1EN,    &RCC->APB2RSTR,  RCC_APB2RSTR_USART1RST,    &RCC->APB2SMENR,  RCC_APB2SMENR_USART1SMEN    },
    { RCC_PERIPH_SYSCFG,        &RCC->APB2ENR,  RCC_APB2ENR_SYSCFGEN,    &RCC->APB2RSTR,  RCC_APB2RSTR_SYSCFGRST,    &RCC->APB2SMENR,  RCC_APB2SMENR_SYSCFGSMEN    },
};

/** Clock enable registers of all register banks */
static volatile uint32_t * const utRcc_EnableRegs[] =
{
    &RCC->AHB1ENR, &RCC->AHB2ENR, &RCC->AHB3ENR, &RCC->APB1ENR1, &RCC->APB1ENR2, &RCC->APB2ENR, &RCC->BDCR
};

/** Reset registers of all register banks */
static volatile uint32_t * const utRcc_ResetRegs[] =
{
    &RCC->AHB1RSTR, &RCC->AHB2RSTR, &RCC->AHB3RSTR, &RCC->APB1RSTR1, &RCC->APB1RSTR2, &RCC->APB2RSTR
};

/** Clock enable in sleep mode registers of all register banks */
static volatile uint32_t * const utRcc_SleepRegs[] =
{
    &RCC->AHB1SMENR, &RCC->AHB2SMENR, &RCC->AHB3SMENR, &RCC->APB1SMENR1, &RCC->APB1SMENR2, &RCC->APB2SMENR
};

/** Expected configuration of MCO pin PA8 (alternate function 0) */
static const gpio_Config_t utRcc_McoPin =
{
    .PortId         = GPIO_PORT_A,
    .PinId          = GPIO_PIN_ID_8,
    .PinMode        = GPIO_PIN_MODE_ALTERNATE,
    .PinPull        = GPIO_PIN_PULL_NONE,
    .PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH,
    .PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL,
    .PinAltFunction = GPIO_ALT_FUNC_0,
    .PinActiveLevel = GPIO_PIN_LEVEL_HIGH
};

/** Expected configuration of LSCO pin PA2 (analog mode) */
static const gpio_Config_t utRcc_LscoPin =
{
    .PortId         = GPIO_PORT_A,
    .PinId          = GPIO_PIN_ID_2,
    .PinMode        = GPIO_PIN_MODE_ANALOG,
    .PinPull        = GPIO_PIN_PULL_NONE,
    .PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH,
    .PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL,
    .PinAltFunction = GPIO_ALT_FUNC_0,
    .PinActiveLevel = GPIO_PIN_LEVEL_HIGH
};

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    utRcc_HseFails        = false;
    utRcc_HpreAtPllSwitch = 0xFFFFFFFFu;

    /* Stops HW model of previous test and clears all registers */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );
}


void tearDown( void )
{
    /* Mocks are verified by generated runner */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelInactive() );
}

/* ========================== MODULE VERSION ================================ */

/**
 * \brief   Rcc_Get_ModuleVersion() returns version of the module.
 *
 * \details Reads the module version structure.
 *
 * \par Expected results
 * - Version is 1.0.0 (Major 1, Minor 0, Patch 0).
 */
void Ut_Rcc_Get_ModuleVersion_ReturnsVersion( void )
{
    rcc_ModuleVersion_t version = Rcc_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Minor );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Patch );
}


/**
 * \brief   Rcc_Deinit() and Rcc_Task() do not touch the hardware.
 *
 * \details Calls both functions on cleared registers.
 *
 * \par Expected results
 * - RCC_CR, RCC_CFGR and RCC_PLLCFGR stay 0.
 */
void Ut_Rcc_Deinit_Task_NoRegisterAccess( void )
{
    Rcc_Deinit( NULL );
    Rcc_Task();

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
}

/* ========================= DEFAULT CONFIGURATION ========================== */

/**
 * \brief   Rcc_Get_DefaultConfig() fills configuration of the highest clock from HSI PLL.
 *
 * \details Reads default configuration into structure filled by pattern.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSE not used (8 MHz preset), MSI range 4 MHz, system clock
 *   from PLL.
 * - Main PLL: HSI, M 1, N 10 (STM32L4, 80 MHz) / 15 (STM32L4+, 120 MHz),
 *   P not used, Q 4 / 6 (40 MHz), R 2. Other PLLs not used.
 * - AHB / 1, APB1 / 1, APB2 / 1, SysTick 1 ms, CSS off, voltage range 1.
 * - Clock outputs not used (source NONE, divider 1).
 */
void Ut_Rcc_Get_DefaultConfig_FillsHsiPllMaxClock( void )
{
    rcc_ConfigStruct_t config;

    (void)memset( &config, 0xA5, sizeof( config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_HSE_TYPE_NONE,           config.HSE_ClockType );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_FREQ_HZ, config.HSE_Frequency_Hz );
    TEST_ASSERT_EQUAL( RCC_MSI_RANGE_4MHZ,          config.MSI_Range );
    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_PLL, config.SystemClockSource );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE,       config.CSS_Enable );
    TEST_ASSERT_EQUAL( RCC_AHB_DIVIDER_1,           config.AHB_Divider );
    TEST_ASSERT_EQUAL( RCC_APB1_DIVIDER_1,          config.APB1_Divider );
    TEST_ASSERT_EQUAL( RCC_APB2_DIVIDER_1,          config.APB2_Divider );
    TEST_ASSERT_EQUAL_UINT32( 1u,                   config.SysTickInterval );
    TEST_ASSERT_EQUAL( RCC_FLASH_LATENCY_0_WS,      config.FlashLatency );
    TEST_ASSERT_EQUAL( RCC_PWR_VOLTAGE_SCALE_1,     config.VoltageScaling );

    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI,                config.Pll_Config[ RCC_PLL_1 ].Pll_Source );
    TEST_ASSERT_EQUAL_UINT32( 1u,                      config.Pll_Config[ RCC_PLL_1 ].M_Divider );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_PLLN,   config.Pll_Config[ RCC_PLL_1 ].N_Multiplier );
    TEST_ASSERT_EQUAL_UINT32( 0u,                      config.Pll_Config[ RCC_PLL_1 ].P_Divider );
    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_DEFAULT_PLLQ_FIELD + 1u ) * 2u, config.Pll_Config[ RCC_PLL_1 ].Q_Divider );
    TEST_ASSERT_EQUAL_UINT32( 2u,                      config.Pll_Config[ RCC_PLL_1 ].R_Divider );

    for( uint32_t pllIdx = 1u; (uint32_t)RCC_PLL_CNT > pllIdx; pllIdx++ )
    {
        TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE, config.Pll_Config[ pllIdx ].Pll_Source );
    }

    for( uint32_t outId = 0u; (uint32_t)RCC_CLK_OUT_CNT > outId; outId++ )
    {
        TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, config.McoConfig[ outId ].ClockSource );
        TEST_ASSERT_EQUAL_UINT32( 1u,           config.McoConfig[ outId ].ClockDivider );
    }
}


/**
 * \brief   Rcc_Get_DefaultConfig() rejects NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR is returned.
 */
void Ut_Rcc_Get_DefaultConfig_NullPtr_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_DefaultConfig( NULL ) );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Rcc_Init() rejects NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, no register is written.
 */
void Ut_Rcc_Init_NullConfig_ReturnsErrorWithoutWrite( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Rcc_Init() with default configuration sets the highest clock from HSI PLL.
 *
 * \details HW model emulates ready flags and system clock switch. STM32L4+:
 *          range 1 boost mode (R1MODE) preset to its reset value (normal mode).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, system clock source (SWS) is PLL.
 * - PLLCFGR: HSI source, M 1, N 10 / 15, Q 4 / 6 (output Q enabled), R 2
 *   (output R enabled), output P disabled. HSI and PLL on, MSI, HSE and CSS off.
 * - Flash: 4 (80 MHz) / 5 (120 MHz) wait states, prefetch and caches enabled.
 * - PWR and SYSCFG interface clocks enabled, voltage range 1 (VOS 01), STM32L4+
 *   in boost mode (R1MODE 0).
 * - AHB / 1, APB1 / 1, APB2 / 1, SysTick reload HCLK / 1000 - 1, SystemCoreClock
 *   80 / 120 MHz.
 */
void Ut_Rcc_Init_DefaultConfig_ConfiguresMaxClock( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

#if defined(PWR_CR5_R1MODE)
    PWR->CR5 = PWR_CR5_R1MODE;              /* HW reset value - normal mode */
#endif /* PWR_CR5_R1MODE */

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 1u, TEST_RCC_DEFAULT_PLLN, TEST_RCC_DEFAULT_PLLQ_FIELD, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                             RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_PLLON,
                             RCC->CR & ( RCC_CR_HSION | RCC_CR_MSION | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_DEFAULT_LATENCY | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN, FLASH->ACR );

    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN,   RCC->APB1ENR1 );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1,  PWR->CR1 & PWR_CR1_VOS );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( 0u,                   PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1,  RCC->CFGR & RCC_CFGR_HPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE1_DIV1, RCC->CFGR & RCC_CFGR_PPRE1 );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE2_DIV1, RCC->CFGR & RCC_CFGR_PPRE2 );

    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_DEFAULT_SYSCLK_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SYSTICK_CTRL_ACTIVE, SysTick->CTRL );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() divides HCLK by 2 while the system clock is switched to PLL above 80 MHz.
 *
 * \details Default configuration (AHB / 1). HW model records the AHB prescaler
 *          at the moment the system clock switch status changes to PLL.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE is /1 after Rcc_Init().
 * - STM32L4+ (120 MHz): HPRE was /2 during the switch (transition state of
 *   RM0432). STM32L4 (80 MHz): HPRE was /1 during the switch.
 */
void Ut_Rcc_Init_PllAbove80MHz_AhbTransitionStateUsed( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV2, utRcc_HpreAtPllSwitch );
#else
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1, utRcc_HpreAtPllSwitch );
#endif /* PWR_CR5_R1MODE */
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1, RCC->CFGR & RCC_CFGR_HPRE );
}


/**
 * \brief   Rcc_Init() switches to PLL of 80 MHz or less without transition state.
 *
 * \details PLL HSI / 1 * 10 / 2 = 80 MHz (STM32L4+) or * 8 / 2 = 64 MHz (STM32L4),
 *          AHB / 1.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE was /1 during the switch, flash latency 3 wait states.
 * - STM32L4+: range 1 normal mode (R1MODE 1).
 */
void Ut_Rcc_Init_PllUpTo80MHz_NoTransitionState( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_LOWER_PLLN;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1, utRcc_HpreAtPllSwitch );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_3WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_LOWER_SYSCLK_HZ, SystemCoreClock );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE, PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */
}


/**
 * \brief   Rcc_Init() configures 80 MHz from HSE crystal PLL with CSS.
 *
 * \details HSE crystal 8 MHz, PLL M 1, N 20, P not used, Q 4, R 2, AHB / 1,
 *          APB1 / 2, APB2 / 1, CSS enabled. HW model emulates ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is PLL, HSE on (not bypassed), CSS on, PLL on.
 * - PLLCFGR: HSE source, M 1, N 20, Q 4, R 2, outputs Q and R enabled.
 * - Flash latency 4 (STM32L4) / 3 (STM32L4+) wait states, STM32L4+ in range 1
 *   normal mode (R1MODE 1). SystemCoreClock 80 MHz.
 * - Clocks: AHB 80 MHz, APB1 40 MHz, APB2 80 MHz, APB1 timers 80 MHz, APB2
 *   timers 80 MHz, RNG (PLL Q) 40 MHz, ADC (SYSCLK) 80 MHz.
 */
void Ut_Rcc_Init_HseCrystalPll_Configures80MHz( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       clkFreq = 0u;

    Ut_Rcc_Get_HsePllConfig( &config );
    config.CSS_Enable = RCC_FUNCTION_ACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON,
                             RCC->CR & ( RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_CSSON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 1u, 20u, TEST_RCC_PLLQR_DIV4, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSE ) |
                             RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_LATENCY_80MHZ, FLASH->ACR & FLASH_ACR_LATENCY );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE, PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ / 2u, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );

    /* Timers of APB with prescaler > 1 are clocked by 2 x PCLK */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLLQ, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_Q_HZ, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC_SYSCLK, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );
}


/**
 * \brief   Rcc_Init() with HSE external clock signal as system clock.
 *
 * \details HSE bypass 24 MHz used directly as system clock, PLL not used.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is HSE, HSE on and bypassed, PLL off, CSS off (not requested).
 * - Flash latency 1 wait state, SystemCoreClock and AHB clock 24 MHz.
 */
void Ut_Rcc_Init_HseBypassSysClk_BypassesOscillator( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       ahbClk = 0u;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.HSE_ClockType     = RCC_HSE_TYPE_SIG_IN;
    config.HSE_Frequency_Hz  = TEST_RCC_HSE_BYPASS_FREQ_HZ;
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_HSE, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSEON | RCC_CR_HSEBYP,
                             RCC->CR & ( RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_CSSON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_1WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_BYPASS_FREQ_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_BYPASS_FREQ_HZ, ahbClk );
}


/**
 * \brief   Rcc_Init() with HSI system clock does not need PLL lock.
 *
 * \details No HW model - HSI ready flag and SWS of HSI are preset. PLL and HSE
 *          are not used, so the module only waits for flags which are already
 *          in the expected state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SW / SWS is HSI, PLL, MSI and HSE off.
 * - SysTick reload 16000 - 1, SystemCoreClock 16 MHz, flash latency 0 wait states.
 */
void Ut_Rcc_Init_HsiSysClk_NoPllRequired( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI, RCC->CFGR & ( RCC_CFGR_SW | RCC_CFGR_SWS ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION, RCC->CR & ( RCC_CR_HSION | RCC_CR_MSION | RCC_CR_HSEON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, SystemCoreClock );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_0WS, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Init() in voltage range 2 sets range 2 (and normal mode of STM32L4+).
 *
 * \details HSI system clock (16 MHz), voltage range 2 (max. 26 MHz). HSI ready
 *          flag and SWS of HSI preset, STM32L4+ boost mode preset (R1MODE 0).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOS = range 2, STM32L4+: R1MODE set (boost not used).
 * - Flash latency 2 (STM32L4, 12 - 18 MHz) / 1 (STM32L4+, 8 - 16 MHz) wait states.
 */
void Ut_Rcc_Init_Range2_SetsVosRange2AndLatency( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE2, PWR->CR1 & PWR_CR1_VOS );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE, PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_LATENCY_HSI_RANGE2, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Init() with MSI system clock configures the MSI range.
 *
 * \details MSI range 48 MHz, system clock MSI, PLL not used. HW model emulates
 *          ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is MSI, MSION, MSIRGSEL and MSIRANGE 1011 (48 MHz) set.
 * - Flash latency 2 wait states, SystemCoreClock 48 MHz.
 */
void Ut_Rcc_Init_MsiSysClk_ConfiguresMsiRange( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_MSI;
    config.MSI_Range         = RCC_MSI_RANGE_48MHZ;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_MSI, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSION | RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE_11,
                             RCC->CR & ( RCC_CR_MSION | RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_2WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_MSI_48MHZ_HZ, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() with PLL clocked by MSI.
 *
 * \details MSI range 4 MHz, PLL MSI / 1 * 40 / 2 = 80 MHz, outputs P and Q not used.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is PLL, MSI on with range 0110 (4 MHz).
 * - PLLCFGR: MSI source, M 1, N 40, R 2, only output R enabled.
 * - SystemCoreClock 80 MHz.
 */
void Ut_Rcc_Init_MsiPll_Configures80MHz( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_MSI;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = 40u;
    config.Pll_Config[ RCC_PLL_1 ].Q_Divider    = 0u;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSION | RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE_6,
                             RCC->CR & ( RCC_CR_MSION | RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 1u, 40u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_MSI ) | RCC_PLLCFGR_PLLREN,
                             RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_UINT32( 80000000u, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() stops when HSI does not get ready.
 *
 * \details No HW model, HSI ready flag stays 0 (timeout of HSI activation).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock is not switched, PLL is not activated.
 * - SystemCoreClock is not changed.
 */
void Ut_Rcc_Init_HsiNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;
    const uint32_t     coreClkBefore = SystemCoreClock;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR & RCC_CFGR_SW );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
    TEST_ASSERT_EQUAL_UINT32( coreClkBefore, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() stops when HSE crystal does not start.
 *
 * \details HSE PLL configuration, HW model does not set HSE ready flag.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock stays HSI, PLL is not activated.
 * - SystemCoreClock is not changed.
 */
void Ut_Rcc_Init_HseNotReady_ReturnsErrorOnHsi( void )
{
    rcc_ConfigStruct_t config;
    const uint32_t     coreClkBefore = SystemCoreClock;

    Ut_Rcc_Get_HsePllConfig( &config );

    utRcc_HseFails = true;
    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_HSI, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
    TEST_ASSERT_EQUAL_UINT32( coreClkBefore, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() rejects system clock above limit of voltage range without any change.
 *
 * \details Default configuration with range 2 (max. 26 MHz), PLL N + 1 above the
 *          maximum of range 1 (88 / 128 MHz) and invalid voltage range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 * - No register is written (RCC_CR, RCC_CFGR, RCC_APB1ENR1, FLASH_ACR, PWR_CR1).
 */
void Ut_Rcc_Init_SysClkOverRangeLimit_ReturnsErrorWithoutChange( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    config.VoltageScaling                       = RCC_PWR_VOLTAGE_SCALE_1;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_DEFAULT_PLLN + 1u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_DEFAULT_PLLN;
    config.VoltageScaling                       = (rcc_PwrVoltageScale_t)0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->CR1 );
}


/**
 * \brief   Rcc_Init() reconfigures PLL which drives the system clock.
 *
 * \details Default configuration is initialized first, then PLL is changed to
 *          N 10 (STM32L4+, 80 MHz) / N 8 (STM32L4, 64 MHz) by second Rcc_Init().
 *
 * \par Expected results
 * - Both calls return RCC_REQUEST_OK, SWS is PLL.
 * - PLLCFGR holds the new configuration, flash latency 3 wait states,
 *   SystemCoreClock 80 / 64 MHz. STM32L4+ in range 1 normal mode.
 */
void Ut_Rcc_Init_CalledTwice_ReconfiguresRunningPll( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_LOWER_PLLN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 1u, TEST_RCC_LOWER_PLLN, TEST_RCC_DEFAULT_PLLQ_FIELD, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                             RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_3WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_LOWER_SYSCLK_HZ, SystemCoreClock );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE, PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */
}


/**
 * \brief   Rcc_Init() configures clock outputs and their pins.
 *
 * \details HSI system clock configuration, MCO source HSI divided by 4, LSCO
 *          source LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Gpio_Init() called for PA8 (alternate function 0, very high
 *   speed, push-pull, no pull) and PA2 (analog).
 * - MCOSEL selects HSI, MCOPRE = divider 4, LSCOEN and LSCOSEL (LSE) set.
 */
void Ut_Rcc_Init_ClockOutputs_ConfiguresMcoLscoAndPins( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLK_SOURCE_MCO1_HSI;
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 4u;
    config.McoConfig[ RCC_CLK_OUT_LSCO ].ClockSource  = RCC_CLK_SOURCE_LSCO_LSE;

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_McoPin,  GPIO_REQUEST_OK );
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_LscoPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1SOURCE_HSI, RCC->CFGR & RCC_CFGR_MCOSEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_MCOPRE_DIV4,  RCC->CFGR & RCC_CFGR_MCOPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL, RCC->BDCR & ( RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL ) );
}


/**
 * \brief   Rcc_Init() configures PLLSAI1 together with the main PLL.
 *
 * \details Default configuration with PLLSAI1 clocked by HSI (shared source),
 *          M 1, N 12 (VCO 192 MHz), Q 4 (48 MHz), outputs P and R not used.
 *          MCUs without PLLSAI1: test ignored.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLSAI1 on, PLLSAI1CFGR: N 12, Q 4, only output Q enabled.
 * - Main PLL keeps its configuration (shared source HSI, M 1).
 * - RNG clocked by PLLSAI1 Q has 48 MHz.
 */
void Ut_Rcc_Init_PllSai1_ConfiguredWithMainPll( void )
{
#if defined(RCC_CR_PLLSAI1ON)
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       clkFreq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_2 ].Pll_Source   = RCC_PLL_SRC_HSI;
    config.Pll_Config[ RCC_PLL_2 ].M_Divider    = 1u;
    config.Pll_Config[ RCC_PLL_2 ].N_Multiplier = 12u;
    config.Pll_Config[ RCC_PLL_2 ].Q_Divider    = 4u;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_PLLON | RCC_CR_PLLSAI1ON, RCC->CR & ( RCC_CR_PLLON | RCC_CR_PLLSAI1ON ) );
    TEST_ASSERT_EQUAL_HEX32( ( 12u << RCC_PLLSAI1CFGR_PLLSAI1N_Pos ) | ( TEST_RCC_PLLQR_DIV4 << RCC_PLLSAI1CFGR_PLLSAI1Q_Pos ) |
                             RCC_PLLSAI1CFGR_PLLSAI1QEN, RCC->PLLSAI1CFGR );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 1u, TEST_RCC_DEFAULT_PLLN, TEST_RCC_DEFAULT_PLLQ_FIELD, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                             RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLLSAI1Q, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, clkFreq );
#else
    TEST_IGNORE_MESSAGE( "MCU without PLLSAI1" );
#endif /* RCC_CR_PLLSAI1ON */
}

/* ========================= PERIPHERAL CLOCKS ============================== */

/**
 * \brief   Rcc_Set_PeriphActive() sets enable bit of the peripheral.
 *
 * \details Activates and deactivates peripherals of every register bank (AHB1,
 *          AHB2, AHB3, APB1 1 / 2, APB2) one by one and reads their state.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, only own enable bit is set in all enable
 *   registers, state is ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, enable register is 0, state is INACTIVE.
 */
void Ut_Rcc_Set_PeriphActive_EveryRegBank_SetsOwnEnableBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs ) > idx; idx++ )
    {
        const utRcc_PeriphRegs_t * const periph = &utRcc_PeriphRegs[ idx ];

        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( periph->EnableMask, *periph->EnableReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_EnableRegs, TEST_RCC_ARRAY_CNT( utRcc_EnableRegs ), periph->EnableReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *periph->EnableReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Rcc_Set_PeriphInactive() clears only own enable bit.
 *
 * \details GPIOA and GPIOB clocks are preset, GPIOA is deactivated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, AHB2ENR holds GPIOB enable bit only.
 */
void Ut_Rcc_Set_PeriphInactive_KeepsOtherEnableBits( void )
{
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_GPIOBEN, RCC->AHB2ENR );
}


/**
 * \brief   Peripherals without clock enable are always clocked.
 *
 * \details SysTick has no clock enable bit (clocked by HCLK).
 *
 * \par Expected results
 * - Activation and deactivation return RCC_REQUEST_OK without any register write.
 * - State is always ACTIVE.
 */
void Ut_Rcc_Set_PeriphActive_NoClockEnable_AlwaysActive( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_SYSTICK ) );
    Ut_Rcc_Check_OtherRegsClear( utRcc_EnableRegs, TEST_RCC_ARRAY_CNT( utRcc_EnableRegs ), NULL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_SYSTICK ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_SYSTICK, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Activation of LSI clocked peripheral starts LSI oscillator.
 *
 * \details IWDG is clocked by LSI. LSI is off, its ready flag is preset (HW
 *          reaction on LSION).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, LSION is set in RCC_CSR.
 */
void Ut_Rcc_Set_PeriphActive_LsiClockedPeriph_StartsLsi( void )
{
    RCC->CSR = RCC_CSR_LSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_IWDG ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CSR_LSION, RCC->CSR & RCC_CSR_LSION );
}


/**
 * \brief   Activation of LSI clocked peripheral fails if LSI does not start.
 *
 * \details IWDG is activated, LSI ready flag stays 0.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR is returned.
 */
void Ut_Rcc_Set_PeriphActive_LsiNotReady_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_IWDG ) );
}


/**
 * \brief   Activation of RNG with internal oscillator as 48 MHz clock starts the oscillator.
 *
 * \details MCUs with HSI48: RNG with HSI48 (HSI48 ready flag preset). MCUs
 *          without HSI48: RNG with MSI (MSI ready flag preset, MSI range after
 *          standby 4 MHz preset).
 *
 * \par Expected results
 * - HSI48: RCC_REQUEST_OK, HSI48ON set, CLK48SEL = HSI48 (00), RNG clock
 *   enabled, Rcc_Get_PeriphClk() returns 48 MHz.
 * - MSI: RCC_REQUEST_OK, MSION set, CLK48SEL = MSI (11), RNG clock enabled,
 *   Rcc_Get_PeriphClk() returns 4 MHz.
 */
void Ut_Rcc_Set_PeriphActive_Clk48Oscillator_StartsOscillator( void )
{
    rcc_FreqHz_t freq = 0u;

#if defined(RCC_CRRCR_HSI48ON)
    RCC->CRRCR = RCC_CRRCR_HSI48RDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CRRCR_HSI48ON, RCC->CRRCR & RCC_CRRCR_HSI48ON );
    TEST_ASSERT_EQUAL_HEX32( 0u,                RCC->CCIPR & RCC_CCIPR_CLK48SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_HSI48, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );
#else
    RCC->CR  = RCC_CR_MSIRDY;
    RCC->CSR = RCC_CSR_MSISRANGE_4;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_MSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSION,       RCC->CR & RCC_CR_MSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL, RCC->CCIPR & RCC_CCIPR_CLK48SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_RNGEN,  RCC->AHB2ENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_MSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_MSI_4MHZ_HZ, freq );
#endif /* RCC_CRRCR_HSI48ON */
}


/**
 * \brief   Peripheral activation selects kernel clock source in CCIPR.
 *
 * \details USART1 with HSI kernel clock (HSI ready preset), then activation
 *          with SYSCLK while HSI is selected, deactivation and activation with
 *          SYSCLK.
 *
 * \par Expected results
 * - HSI: RCC_REQUEST_OK, HSION set, USART1SEL = HSI, USART1 clock enabled.
 * - SYSCLK while HSI selected: RCC_REQUEST_ERROR, USART1SEL unchanged.
 * - Deactivation: clock disabled, USART1SEL = PCLK2 (default).
 * - SYSCLK: RCC_REQUEST_OK, USART1SEL = SYSCLK.
 */
void Ut_Rcc_Set_PeriphActive_KernelClockMux_SelectsAndReleasesSource( void )
{
    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION,            RCC->CR & RCC_CR_HSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_USART1SEL_1,   RCC->CCIPR & RCC_CCIPR_USART1SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2ENR_USART1EN,    RCC->APB2ENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_USART1SEL_1,   RCC->CCIPR & RCC_CCIPR_USART1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2ENR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR & RCC_CCIPR_USART1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_USART1SEL_0,   RCC->CCIPR & RCC_CCIPR_USART1SEL );
}


/**
 * \brief   Kernel clock multiplexers of CCIPR and CCIPR2 are written by activation.
 *
 * \details HSI ready preset. LPTIM1 with LSE, I2C1 with HSI, ADC with SYSCLK
 *          (MCUs with ADCSEL), I2C4 with SYSCLK (CCIPR2, where available) and
 *          DFSDM1 with SYSCLK (CCIPR or CCIPR2, where available).
 *
 * \par Expected results
 * - LPTIM1SEL = LSE (11), I2C1SEL = HSI (10), ADCSEL = SYSCLK (11),
 *   I2C4SEL = SYSCLK (01), DFSDM1SEL = SYSCLK (1), clocks of the peripherals enabled.
 */
void Ut_Rcc_Set_PeriphActive_CcipRegisters_SelectSources( void )
{
    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_LPTIM1_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_LPTIM1SEL,   RCC->CCIPR & RCC_CCIPR_LPTIM1SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_I2C1SEL_1,   RCC->CCIPR & RCC_CCIPR_I2C1SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_LPTIM1EN | RCC_APB1ENR1_I2C1EN, RCC->APB1ENR1 );

#if defined(RCC_CCIPR_ADCSEL)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_ADCSEL,      RCC->CCIPR & RCC_CCIPR_ADCSEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_ADCEN,     RCC->AHB2ENR );
#endif /* RCC_CCIPR_ADCSEL */

#if defined(RCC_APB1ENR2_I2C4EN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C4_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR2_I2C4SEL_0,  RCC->CCIPR2 & RCC_CCIPR2_I2C4SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR2_I2C4EN,   RCC->APB1ENR2 );
#endif /* RCC_APB1ENR2_I2C4EN */

#if defined(RCC_CCIPR_DFSDM1SEL)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_DFSDM1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_DFSDM1SEL,   RCC->CCIPR & RCC_CCIPR_DFSDM1SEL );
#elif defined(RCC_CCIPR2_DFSDM1SEL)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_DFSDM1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR2_DFSDM1SEL,  RCC->CCIPR2 & RCC_CCIPR2_DFSDM1SEL );
#endif /* RCC_CCIPR_DFSDM1SEL */
}


/**
 * \brief   48 MHz clock multiplexer shared by RNG and USB is kept while the other block is enabled.
 *
 * \details RNG and USB activated with PLL Q as 48 MHz clock, RNG deactivated,
 *          then USB deactivated. MCUs without USB: test ignored.
 *
 * \par Expected results
 * - Activation: CLK48SEL = PLLQ (10), RNG and USB clocks enabled, VDDUSB
 *   validated (PWR_CR2 USV).
 * - RNG deactivation: RNG clock disabled, CLK48SEL kept (USB still enabled).
 * - USB deactivation: USB clock disabled, CLK48SEL = 00 (default).
 */
void Ut_Rcc_Set_PeriphInactive_SharedClk48Mux_KeptWhileUsbEnabled( void )
{
#if defined(RCC_TYPES_USB_SUPPORT)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_PLLQ ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USB_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL_1, RCC->CCIPR & RCC_CCIPR_CLK48SEL );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_USB_EN, TEST_RCC_USB_ENR & TEST_RCC_USB_EN );
#if defined(PWR_CR2_USV)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR2_USV,     PWR->CR2 & PWR_CR2_USV );
#endif /* PWR_CR2_USV */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RNG_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB2ENR & RCC_AHB2ENR_RNGEN );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL_1, RCC->CCIPR & RCC_CCIPR_CLK48SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USB_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, TEST_RCC_USB_ENR & TEST_RCC_USB_EN );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR & RCC_CCIPR_CLK48SEL );
#else
    TEST_IGNORE_MESSAGE( "MCU without USB" );
#endif /* RCC_TYPES_USB_SUPPORT */
}


/**
 * \brief   48 MHz clock multiplexer is kept while SDMMC1 is enabled and released by SDMMC1.
 *
 * \details RNG activated with PLL Q as 48 MHz clock, SDMMC1 activated, RNG
 *          deactivated, then SDMMC1 deactivated. MCUs without SDMMC1: test
 *          ignored.
 *
 * \par Expected results
 * - RNG deactivation: CLK48SEL kept (SDMMC1 clocked by the 48 MHz clock).
 * - SDMMC1 deactivation: SDMMC1 clock disabled, CLK48SEL = 00 (released).
 * - RNG can be activated with MSI afterwards (MSI ready preset).
 */
void Ut_Rcc_Set_PeriphInactive_SharedClk48Mux_KeptWhileSdmmcEnabled( void )
{
#if defined(RCC_TYPES_SDMMC1_SUPPORT)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_PLLQ ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_SDMMC1 ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SDMMC1_EN, TEST_RCC_SDMMC1_ENR & TEST_RCC_SDMMC1_EN );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RNG_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL_1, RCC->CCIPR & RCC_CCIPR_CLK48SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_SDMMC1 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, TEST_RCC_SDMMC1_ENR & TEST_RCC_SDMMC1_EN );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR & RCC_CCIPR_CLK48SEL );

    RCC->CR = RCC_CR_MSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_MSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL, RCC->CCIPR & RCC_CCIPR_CLK48SEL );
#else
    TEST_IGNORE_MESSAGE( "MCU without SDMMC1" );
#endif /* RCC_TYPES_SDMMC1_SUPPORT */
}


/**
 * \brief   Activation of port G validates VDDIO2 supply.
 *
 * \details PG[15:2] are supplied by VDDIO2 (PWR_CR2 IOSV). MCUs without port G:
 *          test ignored.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, IOSV set, PWR interface clock enabled, GPIOG clock enabled.
 */
void Ut_Rcc_Set_PeriphActive_GpioG_ValidatesVddio2( void )
{
#if defined(RCC_AHB2ENR_GPIOGEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_GPIOG ) );

    TEST_ASSERT_EQUAL_HEX32( PWR_CR2_IOSV,        PWR->CR2 & PWR_CR2_IOSV );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN,  RCC->APB1ENR1 & RCC_APB1ENR1_PWREN );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_GPIOGEN, RCC->AHB2ENR );
#else
    TEST_IGNORE_MESSAGE( "MCU without port G" );
#endif /* RCC_AHB2ENR_GPIOGEN */
}


/**
 * \brief   Other peripherals do not touch the independent supplies.
 *
 * \details GPIOA activated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_CR2 is not written, PWR interface clock not enabled.
 */
void Ut_Rcc_Set_PeriphActive_OtherBlock_SupplyNotChanged( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->CR2 );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR1 );
}


/**
 * \brief   RTC activation with LSI selects RTC clock in backup domain.
 *
 * \details LSI ready flag is preset, backup domain is write protected (DBP 0).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCSEL = LSI and RTCEN set in RCC_BDCR.
 * - Backup domain write protection released (PWR interface clock, DBP).
 */
void Ut_Rcc_Set_PeriphActive_RtcLsi_SelectsRtcClock( void )
{
    RCC->CSR = RCC_CSR_LSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1 | RCC_BDCR_RTCEN, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN, RCC->APB1ENR1 & RCC_APB1ENR1_PWREN );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR1_DBP, PWR->CR1 & PWR_CR1_DBP );
}


/**
 * \brief   RTC activation fails if other RTC clock is already selected.
 *
 * \details RTCSEL preset to LSE (write-once field), RTC with LSI is activated.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RTCSEL stays LSE, RTCEN is not set.
 */
void Ut_Rcc_Set_PeriphActive_RtcOtherSourceSelected_ReturnsError( void )
{
    RCC->CSR  = RCC_CSR_LSIRDY;
    RCC->BDCR = RCC_BDCR_RTCSEL_0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_0, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );
}


/**
 * \brief   RTC activation with HSE divided by 32.
 *
 * \details HSE frequency 8 MHz is configured.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCSEL = HSE / 32, RTCEN set.
 * - Rcc_Get_PeriphClk() of the RTC returns 250 kHz.
 */
void Ut_Rcc_Set_PeriphActive_RtcHseDiv32_SelectsHseClock( void )
{
    rcc_FreqHz_t rtcClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_FREQ_HZ ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_HSE_DIV32 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV32, &rtcClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_FREQ_HZ / 32u, rtcClk );
}


/**
 * \brief   RTC activation with HSE fails when HSE frequency is not known.
 *
 * \details HSE frequency is not configured (0).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, BDCR is not written.
 */
void Ut_Rcc_Set_PeriphActive_RtcHseDiv32NoHse_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_HSE_DIV32 ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
}


/**
 * \brief   Peripheral clock functions reject invalid arguments.
 *
 * \details Calls clock / reset / sleep functions with RCC_PERIPH_ID_CNT and
 *          state reading functions with NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, no enable / reset / sleep register is written.
 */
void Ut_Rcc_PeriphFunctions_InvalidArgs_ReturnError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;
    rcc_PeriphId_t      srcId = RCC_PERIPH_ID_CNT;
    rcc_FreqHz_t        freq  = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive  ( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphInactive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive   ( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetInactive ( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SleepActive   ( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SleepInactive ( RCC_PERIPH_ID_CNT ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphState ( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphState ( RCC_PERIPH_GPIOA,  NULL   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetState  ( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetState  ( RCC_PERIPH_GPIOA,  NULL   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SleepState  ( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SleepState  ( RCC_PERIPH_GPIOA,  NULL   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk   ( RCC_PERIPH_ID_CNT, &freq  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk   ( RCC_PERIPH_GPIOA,  NULL   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ID_CNT, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClkSrc( RCC_PERIPH_GPIOA,  NULL   ) );

    Ut_Rcc_Check_OtherRegsClear( utRcc_EnableRegs, TEST_RCC_ARRAY_CNT( utRcc_EnableRegs ), NULL );
    Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs,  TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ),  NULL );
    Ut_Rcc_Check_OtherRegsClear( utRcc_SleepRegs,  TEST_RCC_ARRAY_CNT( utRcc_SleepRegs ),  NULL );
}

/* ====================== PERIPHERAL CLOCK FREQUENCY ======================== */

/**
 * \brief   Rcc_Get_PeriphClk() returns clock of the bus of the peripheral.
 *
 * \details System clock HSI 16 MHz, AHB / 2, APB1 / 2, APB2 / 4 preset in CFGR.
 *
 * \par Expected results
 * - GPIOA (AHB2) 8 MHz, USART2 (APB1) 4 MHz, USART1 (APB2) 2 MHz.
 * - TIM2 (APB1 timer) 8 MHz, TIM1 (APB2 timer) 4 MHz (2 x PCLK).
 */
void Ut_Rcc_Get_PeriphClk_BusClockedPeriphs_ReturnBusFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CFGR = RCC_CFGR_SWS_HSI | RCC_CFGR_HPRE_DIV2 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV4;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_GPIOA, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 8000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART2_PCLK1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 4000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_PCLK2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 2000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 8000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 4000000u, freq );
}


/**
 * \brief   Timers of APB without prescaler are clocked by PCLK.
 *
 * \details System clock HSI 16 MHz, AHB / 1, APB1 / 1, APB2 / 1.
 *
 * \par Expected results
 * - TIM2 and TIM1 clock 16 MHz.
 */
void Ut_Rcc_Get_PeriphClk_ApbNotDivided_TimerClockEqualsPclk( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
}


/**
 * \brief   PLL output clocked peripherals return frequency of the output.
 *
 * \details PLLCFGR preset: HSI, M 1, N 10 / 15, Q 4, R 2, outputs Q and R enabled.
 *          System clock is PLL.
 *
 * \par Expected results
 * - RNG and USB (where available) with PLL Q: VCO / 4.
 * - ADC with SYSCLK: VCO / 2.
 */
void Ut_Rcc_Get_PeriphClk_PllOutputs_ReturnPllFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, TEST_RCC_DEFAULT_PLLN, TEST_RCC_PLLQR_DIV4, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                   RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN;
    RCC->CFGR    = RCC_CFGR_SWS_PLL;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 4u, freq );
#if defined(RCC_TYPES_USB_SUPPORT)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USB_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 4u, freq );
#endif /* RCC_TYPES_USB_SUPPORT */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC_SYSCLK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 2u, freq );
}


/**
 * \brief   Peripheral clocked by disabled PLL output has no clock.
 *
 * \details PLLCFGR preset with output R enabled, Q disabled.
 *
 * \par Expected results
 * - RNG PLLQ: RCC_REQUEST_ERROR, frequency 0.
 */
void Ut_Rcc_Get_PeriphClk_PllOutputDisabled_ReturnsError( void )
{
    rcc_FreqHz_t freq = 1u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, TEST_RCC_DEFAULT_PLLN, TEST_RCC_PLLQR_DIV4, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                   RCC_PLLCFGR_PLLREN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
}


/**
 * \brief   Oscillator clocked peripherals return oscillator frequency.
 *
 * \par Expected results
 * - IWDG and RTC with LSI clock 32 kHz, RTC with LSE clock 32.768 kHz, USART1
 *   with HSI 16 MHz, LPUART1 with LSE 32.768 kHz, LPTIM1 with LSI 32 kHz.
 */
void Ut_Rcc_Get_PeriphClk_OscillatorClockedPeriphs_ReturnOscFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_IWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_LSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_LSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_HSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPUART1_LSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPTIM1_LSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
}


/**
 * \brief   SDMMC kernel clock is the 48 MHz clock or (STM32L4+) main PLL output P.
 *
 * \details PLLCFGR preset: HSI, M 1, N 12 (VCO 192 MHz), PDIV 4 (where
 *          available), Q 4, outputs P and Q enabled. CLK48SEL = PLLQ. STM32L4+:
 *          SDMMCSEL set afterwards. MCUs without SDMMC1: test ignored.
 *
 * \par Expected results
 * - CLK48SEL PLLQ: SDMMC1 clock 48 MHz.
 * - SDMMCSEL (STM32L4+): SDMMC1 clock VCO / 4 from output P (48 MHz).
 */
void Ut_Rcc_Get_PeriphClk_Sdmmc_Clk48OrPllP( void )
{
#if defined(RCC_TYPES_SDMMC1_SUPPORT)
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, 12u, TEST_RCC_PLLQR_DIV4, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                   RCC_PLLCFGR_PLLQEN;
    RCC->CCIPR   = RCC_CCIPR_CLK48SEL_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SDMMC1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );

#if defined(RCC_CCIPR2_SDMMCSEL)
    RCC->PLLCFGR |= ( 4u << RCC_PLLCFGR_PLLPDIV_Pos ) | RCC_PLLCFGR_PLLPEN;
    RCC->CCIPR2   = RCC_CCIPR2_SDMMCSEL;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SDMMC1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );

    RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLPEN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_SDMMC1, &freq ) );
#endif /* RCC_CCIPR2_SDMMCSEL */
#else
    TEST_IGNORE_MESSAGE( "MCU without SDMMC1" );
#endif /* RCC_TYPES_SDMMC1_SUPPORT */
}


/**
 * \brief   Rcc_Get_PeriphClk() rejects peripheral without known kernel clock.
 *
 * \details RTC with HSE while HSE frequency is not configured (0). SAI1 (kernel
 *          clock not handled, where available).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, frequency 0 in both cases.
 */
void Ut_Rcc_Get_PeriphClk_NoKernelClock_ReturnsErrorAndZero( void )
{
    rcc_FreqHz_t freq = 1u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV32, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );

#if defined(RCC_APB2ENR_SAI1EN)
    freq = 1u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_SAI1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
#endif /* RCC_APB2ENR_SAI1EN */
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() of peripheral without clock multiplexer.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, the same peripheral ID is returned.
 */
void Ut_Rcc_Get_PeriphClkSrc_NoClockMux_ReturnsSameId( void )
{
    rcc_PeriphId_t srcId = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_GPIOA, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_GPIOA, srcId );
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() returns ID matching selected RTC clock.
 *
 * \details RTCSEL preset to LSE, query by ID of RTC with LSI.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_PERIPH_RTC_LSE is returned.
 */
void Ut_Rcc_Get_PeriphClkSrc_RtcMux_ReturnsSelectedSource( void )
{
    rcc_PeriphId_t srcId = RCC_PERIPH_ID_CNT;

    RCC->BDCR = RCC_BDCR_RTCSEL_0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_RTC_LSI, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_RTC_LSE, srcId );
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() fails if RTC has no clock selected.
 *
 * \details RTCSEL is 0 (no clock).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR is returned.
 */
void Ut_Rcc_Get_PeriphClkSrc_RtcNoClock_ReturnsError( void )
{
    rcc_PeriphId_t srcId = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClkSrc( RCC_PERIPH_RTC_LSE, &srcId ) );
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() returns ID matching selected kernel clock of CCIPR.
 *
 * \details USART2SEL preset to SYSCLK, ADCSEL to SYSCLK (where available),
 *          CLK48SEL to PLL Q.
 *
 * \par Expected results
 * - USART2 (queried by PCLK1 ID): RCC_PERIPH_USART2_SYSCLK.
 * - ADC (queried by HCLK ID): RCC_PERIPH_ADC_SYSCLK.
 * - RNG (queried by MSI ID): RCC_PERIPH_RNG_PLLQ, USB (where available):
 *   RCC_PERIPH_USB_PLLQ - own ID of the block sharing the multiplexer.
 */
void Ut_Rcc_Get_PeriphClkSrc_KernelMux_ReturnsSelectedSource( void )
{
    rcc_PeriphId_t srcId = RCC_PERIPH_ID_CNT;

    RCC->CCIPR = RCC_CCIPR_USART2SEL_0 | RCC_CCIPR_CLK48SEL_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART2_PCLK1, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART2_SYSCLK, srcId );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_RNG_MSI, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_RNG_PLLQ, srcId );
#if defined(RCC_TYPES_USB_SUPPORT)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USB_MSI, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USB_PLLQ, srcId );
#endif /* RCC_TYPES_USB_SUPPORT */
#if defined(RCC_CCIPR_ADCSEL)
    RCC->CCIPR |= RCC_CCIPR_ADCSEL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ADC_HCLK, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_ADC_SYSCLK, srcId );
#endif /* RCC_CCIPR_ADCSEL */
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() of 48 MHz clock without selected source.
 *
 * \details CLK48SEL 00 - HSI48 (MCUs with HSI48) or no clock (STM32L47x / L48x).
 *
 * \par Expected results
 * - HSI48: RCC_REQUEST_OK, RCC_PERIPH_RNG_HSI48.
 * - No clock: RCC_REQUEST_ERROR (no peripheral ID of the selection).
 */
void Ut_Rcc_Get_PeriphClkSrc_Clk48Default_HsiOrNoClock( void )
{
    rcc_PeriphId_t srcId = RCC_PERIPH_ID_CNT;

#if defined(RCC_CRRCR_HSI48ON)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_RNG_PLLQ, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_RNG_HSI48, srcId );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClkSrc( RCC_PERIPH_RNG_PLLQ, &srcId ) );
#endif /* RCC_CRRCR_HSI48ON */
}

/* ============================ PERIPHERAL RESET ============================ */

/**
 * \brief   Rcc_Set_ResetActive() / Rcc_Set_ResetInactive() control own reset bit.
 *
 * \details Resets peripherals of every register bank one by one.
 *
 * \par Expected results
 * - Reset active: RCC_REQUEST_OK, only own reset bit set in all reset
 *   registers, reset state ACTIVE.
 * - Reset inactive: RCC_REQUEST_OK, reset register 0, reset state INACTIVE.
 */
void Ut_Rcc_Set_ResetActive_EveryRegBank_SetsOwnResetBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs ) > idx; idx++ )
    {
        const utRcc_PeriphRegs_t * const periph = &utRcc_PeriphRegs[ idx ];

        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( periph->ResetMask, *periph->ResetReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs, TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ), periph->ResetReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *periph->ResetReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Reset of peripheral without reset control is rejected.
 *
 * \details WWDG (no reset bit on STM32L4), RTC APB interface (where available)
 *          and RTC (backup domain, no reset register) are reset.
 *
 * \par Expected results
 * - Reset active: RCC_REQUEST_ERROR, no reset register written.
 * - Reset inactive: RCC_REQUEST_OK (never in reset), reset state INACTIVE.
 */
void Ut_Rcc_Set_ResetActive_NoResetControl_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

#if defined(RCC_APB1ENR1_RTCAPBEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_RTCAPB ) );
#endif /* RCC_APB1ENR1_RTCAPBEN */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_WWDG ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_RTC_LSE ) );
    Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs, TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ), NULL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_WWDG ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_RTC_LSE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_RTC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/* ========================== SLEEP MODE CLOCKS ============================= */

/**
 * \brief   Rcc_Set_SleepActive() / Rcc_Set_SleepInactive() control own sleep bit.
 *
 * \details Enables peripherals of every register bank in sleep mode one by one.
 *
 * \par Expected results
 * - Sleep active: RCC_REQUEST_OK, only own bit set in all sleep registers,
 *   sleep state ACTIVE.
 * - Sleep inactive: RCC_REQUEST_OK, sleep register 0, sleep state INACTIVE.
 */
void Ut_Rcc_Set_SleepActive_EveryRegBank_SetsOwnSleepBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs ) > idx; idx++ )
    {
        const utRcc_PeriphRegs_t * const periph = &utRcc_PeriphRegs[ idx ];

        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( periph->SleepMask, *periph->SleepReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_SleepRegs, TEST_RCC_ARRAY_CNT( utRcc_SleepRegs ), periph->SleepReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *periph->SleepReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Sleep-only blocks (SRAM) are controlled in sleep register.
 *
 * \details SRAM1 (AHB1SMENR) and SRAM2 (AHB2SMENR) have sleep mode clock
 *          control only (where available). Other sleep bits of the registers
 *          are preset.
 *
 * \par Expected results
 * - Sleep inactive clears SRAM1SMEN / SRAM2SMEN, sleep active sets them,
 *   other bits kept.
 * - Clock enable state of the blocks is always ACTIVE (no enable bit).
 */
void Ut_Rcc_Set_SleepInactive_SramBlocks_ClearSleepBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

#if defined(RCC_AHB1SMENR_SRAM1SMEN)
    RCC->AHB1SMENR = RCC_AHB1SMENR_SRAM1SMEN | RCC_AHB1SMENR_DMA1SMEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_SRAM1 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1SMENR_DMA1SMEN, RCC->AHB1SMENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_SRAM1 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1SMENR_SRAM1SMEN | RCC_AHB1SMENR_DMA1SMEN, RCC->AHB1SMENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_SRAM1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
#endif /* RCC_AHB1SMENR_SRAM1SMEN */

#if defined(RCC_AHB2SMENR_SRAM2SMEN)
    RCC->AHB2SMENR = RCC_AHB2SMENR_SRAM2SMEN | RCC_AHB2SMENR_GPIOASMEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_SRAM2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2SMENR_GPIOASMEN, RCC->AHB2SMENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_SRAM2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2SMENR_SRAM2SMEN | RCC_AHB2SMENR_GPIOASMEN, RCC->AHB2SMENR );

    state = RCC_FUNCTION_INACTIVE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_SRAM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
#endif /* RCC_AHB2SMENR_SRAM2SMEN */

    (void)state;
}


/**
 * \brief   Peripheral without sleep control is always active in sleep mode.
 *
 * \details SysTick has no sleep mode clock control.
 *
 * \par Expected results
 * - Sleep active / inactive return RCC_REQUEST_OK without write, state ACTIVE.
 */
void Ut_Rcc_Set_SleepInactive_NoSleepControl_AlwaysActive( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_SYSTICK ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_SYSTICK ) );
    Ut_Rcc_Check_OtherRegsClear( utRcc_SleepRegs, TEST_RCC_ARRAY_CNT( utRcc_SleepRegs ), NULL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_SYSTICK, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}

/* ============================== CLOCK BUSES =============================== */

/**
 * \brief   Rcc_Set_ClkBusDivider() writes divider of every bus.
 *
 * \details Sets AHB3 / 8 (shared AHB divider), APB1 group 2 / 4 (shared APB1
 *          divider) and APB2 / 16.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE = /8, PPRE1 = /4, PPRE2 = /16.
 * - Rcc_Get_ClkBusDivider() returns the set dividers, AHB1 equals AHB3,
 *   APB1 group 1 equals group 2.
 */
void Ut_Rcc_Set_ClkBusDivider_AllBuses_WritesCfgr( void )
{
    rcc_ClkBusDiv_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB3,   RCC_AHB_DIVIDER_8   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_2, RCC_APB1_DIVIDER_4  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB2_DIVIDER_16 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV8 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV16, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB_DIVIDER_8, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1_DIVIDER_4, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2_DIVIDER_16, divider );
}


/**
 * \brief   Rcc_Set_ClkBusDivider() rejects invalid bus and divider.
 *
 * \details Divider of other bus (APB2 value for APB1), divider out of the
 *          field for AHB and invalid bus ID.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, CFGR is not written.
 */
void Ut_Rcc_Set_ClkBusDivider_InvalidArgs_ReturnsErrorWithoutWrite( void )
{
    rcc_ClkBusDiv_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB2_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT,    RCC_AHB_DIVIDER_2  ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_CNT,    &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_1, NULL     ) );
}


/**
 * \brief   Rcc_Get_ClkBusClk() calculates bus clocks from PLL system clock.
 *
 * \details Registers preset: PLL HSI / 1 * N / 2 (default system clock) is
 *          system clock, AHB / 1, APB1 / 2, APB2 / 1.
 *
 * \par Expected results
 * - AHB1 / AHB2 / AHB3 default system clock, APB1 groups half of it, APB2 equal.
 * - Invalid bus or NULL pointer returns RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkBusClk_PllSysClk_ReturnsBusFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, TEST_RCC_DEFAULT_PLLN, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) | RCC_PLLCFGR_PLLREN;
    RCC->CFGR    = RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2;

    for( rcc_ClkBusId_t busId = RCC_CLK_BUS_AHB1; RCC_CLK_BUS_AHB3 >= busId; busId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( busId, &freq ) );
        TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );
    }
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_CNT,    &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, NULL  ) );
}

/* ========================= POWER RANGE AND FLASH ========================== */

/**
 * \brief   Rcc_Set_PwrRange() writes voltage range 2 (and normal mode of STM32L4+).
 *
 * \details System clock HSI 16 MHz (below 26 MHz), VOS preset to range 1,
 *          range 2 is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOS = range 2, STM32L4+: R1MODE set, PWR interface clock
 *   enabled.
 */
void Ut_Rcc_Set_PwrRange_Range2_WritesVos( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    PWR->CR1  = TEST_RCC_VOS_RANGE1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE2, PWR->CR1 & PWR_CR1_VOS );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE,      PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN,  RCC->APB1ENR1 );
}


/**
 * \brief   Rcc_Set_PwrRange() sets range 1, STM32L4+ selects boost or normal mode.
 *
 * \details System clock HSI 16 MHz, VOS preset to range 2 (R1MODE set).
 *          Range 1 with default configuration (STM32L4+ 120 MHz) is requested,
 *          then range 1 with lower system clock (80 MHz).
 *
 * \par Expected results
 * - Default: RCC_REQUEST_OK, VOS = range 1, STM32L4+: R1MODE cleared (boost).
 * - Lower clock: RCC_REQUEST_OK, VOS = range 1, STM32L4+: R1MODE set (normal).
 */
void Ut_Rcc_Set_PwrRange_Range1_SelectsBoostOrNormal( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    PWR->CR1  = TEST_RCC_VOS_RANGE2;
#if defined(PWR_CR5_R1MODE)
    PWR->CR5  = PWR_CR5_R1MODE;
#endif /* PWR_CR5_R1MODE */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1, PWR->CR1 & PWR_CR1_VOS );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( 0u,                  PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */

    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_LOWER_PLLN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1, PWR->CR1 & PWR_CR1_VOS );
#if defined(PWR_CR5_R1MODE)
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE,      PWR->CR5 & PWR_CR5_R1MODE );
#endif /* PWR_CR5_R1MODE */
}


/**
 * \brief   Rcc_Set_PwrRange() rejects range whose maximal clock is below the actual system clock.
 *
 * \details Default PLL system clock (80 / 120 MHz) is running (registers preset),
 *          VOS range 1. Range 2 is requested, STM32L4+: range 1 with expected
 *          80 MHz (normal mode) while 120 MHz is running. Default range 1 again.
 *          NULL configuration and unknown range.
 *
 * \par Expected results
 * - Range 2 / normal mode: RCC_REQUEST_ERROR, VOS not changed.
 * - Default range 1: RCC_REQUEST_OK.
 * - NULL / unknown range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PwrRange_SysClkAboveRange_ChangeRejected( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, TEST_RCC_DEFAULT_PLLN, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) | RCC_PLLCFGR_PLLREN;
    RCC->CFGR    = RCC_CFGR_SWS_PLL;
    PWR->CR1     = TEST_RCC_VOS_RANGE1;

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1, PWR->CR1 & PWR_CR1_VOS );

#if defined(PWR_CR5_R1MODE)
    config.VoltageScaling                       = RCC_PWR_VOLTAGE_SCALE_1;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_LOWER_PLLN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->CR5 & PWR_CR5_R1MODE );
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = TEST_RCC_DEFAULT_PLLN;
#endif /* PWR_CR5_R1MODE */

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );

    config.VoltageScaling = (rcc_PwrVoltageScale_t)PWR_CR1_VOS;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
}


/**
 * \brief   Rcc_Set_PwrRange() fails when the voltage scaling does not finish.
 *
 * \details PWR_SR2 VOSF preset (regulator not ready), HSI system clock.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR is returned.
 */
void Ut_Rcc_Set_PwrRange_VoltageNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    PWR->SR2  = PWR_SR2_VOSF;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() calculates wait states from HCLK in range 1.
 *
 * \details HSE system clock with frequencies around thresholds of range 1
 *          (STM32L4: 16, 32, 48, 64, 80 MHz, STM32L4+: 20, 40, 60, 80, 100,
 *          120 MHz), AHB / 1.
 *
 * \par Expected results
 * - Every exceeded threshold adds one wait state.
 * - Frequency above the maximum of range 1: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_FlashLatency_Range1Thresholds_SetsWaitStates( void )
{
    static const utRcc_LatencyCase_t latencyLut[] =
    {
#if defined(PWR_CR5_R1MODE)
        { RCC_PWR_VOLTAGE_SCALE_1,  20000000u, FLASH_ACR_LATENCY_0WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  20000001u, FLASH_ACR_LATENCY_1WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  40000001u, FLASH_ACR_LATENCY_2WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  60000001u, FLASH_ACR_LATENCY_3WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  80000001u, FLASH_ACR_LATENCY_4WS },
        { RCC_PWR_VOLTAGE_SCALE_1, 100000001u, FLASH_ACR_LATENCY_5WS },
        { RCC_PWR_VOLTAGE_SCALE_1, 120000000u, FLASH_ACR_LATENCY_5WS },
#else
        { RCC_PWR_VOLTAGE_SCALE_1,  16000000u, FLASH_ACR_LATENCY_0WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  16000001u, FLASH_ACR_LATENCY_1WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  32000001u, FLASH_ACR_LATENCY_2WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  48000001u, FLASH_ACR_LATENCY_3WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  64000001u, FLASH_ACR_LATENCY_4WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  80000000u, FLASH_ACR_LATENCY_4WS },
#endif /* PWR_CR5_R1MODE */
    };
    rcc_ConfigStruct_t config;

    Ut_Rcc_Check_Latency( latencyLut, TEST_RCC_ARRAY_CNT( latencyLut ) );

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = latencyLut[ TEST_RCC_ARRAY_CNT( latencyLut ) - 1u ].SysClk + 1u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() thresholds of range 2.
 *
 * \details HSE system clock around thresholds of range 2 (STM32L4: 6, 12, 18,
 *          26 MHz, STM32L4+: 8, 16, 26 MHz).
 *
 * \par Expected results
 * - Every exceeded threshold adds one wait state.
 * - Frequency above 26 MHz: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_FlashLatency_Range2Thresholds_SetsWaitStates( void )
{
    static const utRcc_LatencyCase_t latencyLut[] =
    {
#if defined(PWR_CR5_R1MODE)
        { RCC_PWR_VOLTAGE_SCALE_2,   8000000u, FLASH_ACR_LATENCY_0WS },
        { RCC_PWR_VOLTAGE_SCALE_2,   8000001u, FLASH_ACR_LATENCY_1WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  16000001u, FLASH_ACR_LATENCY_2WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  26000000u, FLASH_ACR_LATENCY_2WS },
#else
        { RCC_PWR_VOLTAGE_SCALE_2,   6000000u, FLASH_ACR_LATENCY_0WS },
        { RCC_PWR_VOLTAGE_SCALE_2,   6000001u, FLASH_ACR_LATENCY_1WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  12000001u, FLASH_ACR_LATENCY_2WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  18000001u, FLASH_ACR_LATENCY_3WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  26000000u, FLASH_ACR_LATENCY_3WS },
#endif /* PWR_CR5_R1MODE */
    };
    rcc_ConfigStruct_t config;

    Ut_Rcc_Check_Latency( latencyLut, TEST_RCC_ARRAY_CNT( latencyLut ) );

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_2;
    config.HSE_Frequency_Hz  = 26000001u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() uses HCLK (AHB divider) and minimal latency of the user.
 *
 * \details HSE 64 MHz with AHB / 2 (HCLK 32 MHz), then minimal latency 4 WS.
 *
 * \par Expected results
 * - AHB / 2: 1 wait state (32 MHz HCLK, range 1).
 * - Minimal latency 4 WS: 4 wait states.
 */
void Ut_Rcc_Set_FlashLatency_AhbDividerAndUserMinimum_Applied( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = 64000000u;
    config.AHB_Divider       = RCC_AHB_DIVIDER_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_1WS, FLASH->ACR & FLASH_ACR_LATENCY );

    config.FlashLatency = RCC_FLASH_LATENCY_4_WS;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_4WS, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Set_FlashLatency() fails if expected system clock is not available.
 *
 * \details PLL system clock without PLL source, without output R divider, with
 *          M divider 0, invalid system clock source, MSI system clock with
 *          invalid range, NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, latency not written.
 */
void Ut_Rcc_Set_FlashLatency_PllNotConfigured_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_HSI;
    config.Pll_Config[ RCC_PLL_1 ].R_Divider  = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 2u;
    config.Pll_Config[ RCC_PLL_1 ].M_Divider = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].M_Divider = 1u;
    config.SystemClockSource                 = RCC_SYSTEM_CLOCK_SOURCE_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_MSI;
    config.MSI_Range         = (rcc_MsiRange_t)RCC_CR_MSIRANGE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Flash prefetch activation and deactivation toggle PRFTEN only.
 *
 * \details Instruction cache bit preset.
 *
 * \par Expected results
 * - Activation sets PRFTEN, deactivation clears it, ICEN kept.
 */
void Ut_Rcc_Set_FlashPrefetchActive_TogglesPrftenOnly( void )
{
    FLASH->ACR = FLASH_ACR_ICEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchActive() );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_ICEN | FLASH_ACR_PRFTEN, FLASH->ACR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchInactive() );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_ICEN, FLASH->ACR );
}

/* ================================ SYSTICK ================================= */

/**
 * \brief   Rcc_Set_SysTickInterval() configures 1 ms interval from HCLK.
 *
 * \details System clock HSI 16 MHz.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SysTick reload 16000 - 1, SysTick enabled with interrupt.
 */
void Ut_Rcc_Set_SysTickInterval_1ms_WritesReload( void )
{
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SYSTICK_CTRL_ACTIVE, SysTick->CTRL );
}


/**
 * \brief   Rcc_Set_SysTickInterval() checks the range of the reload register.
 *
 * \details HSI 16 MHz: 0 ms, longest interval and interval above the range.
 *          Unknown system clock (MSI with reserved range after standby).
 *
 * \par Expected results
 * - 0 ms and too long interval: RCC_REQUEST_ERROR.
 * - Longest interval: RCC_REQUEST_OK.
 * - Unknown system clock: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_SysTickInterval_RangeBoundaries_Checked( void )
{
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_SysTickInterval( TEST_RCC_SYSTICK_MAX_MS_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( TEST_RCC_SYSTICK_MAX_MS_HSI + 1u ) );

    RCC->CFGR = RCC_CFGR_SWS_MSI;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 1u ) );
}


/**
 * \brief   Rcc_Get_SysTickInterval() reads interval from reload register.
 *
 * \details HSI 16 MHz, SysTick reload preset for 10 ms. NULL pointer, unknown
 *          system clock (MSI with reserved range after standby).
 *
 * \par Expected results
 * - RCC_REQUEST_OK and 10 ms; RCC_REQUEST_ERROR for NULL / unknown clock.
 */
void Ut_Rcc_Get_SysTickInterval_ReadsReload( void )
{
    rcc_Time_ms_t interval = 0u;

    RCC->CFGR     = RCC_CFGR_SWS_HSI;
    SysTick->LOAD = ( 10u * ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) ) - 1u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 10u, interval );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( NULL ) );

    RCC->CFGR = RCC_CFGR_SWS_MSI;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( &interval ) );
}

/* ================================== PLL =================================== */

/**
 * \brief   Rcc_Set_PllConfig() configures and locks PLL clocked by HSI.
 *
 * \details HW model emulates ready flags. M 2, N 20 (VCO 160 MHz), P 7 (MCUs
 *          with output P), Q 8, R 4.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLCFGR holds the configuration with all outputs enabled,
 *   P divider 7 in PDIV (MCUs with PDIV) or P bit cleared, PLL on and locked,
 *   HSI on.
 */
void Ut_Rcc_Set_PllConfig_Hsi_ConfiguresAndLocks( void )
{
    rcc_PllConfigStruct_t pllConfig = { RCC_PLL_SRC_HSI, 2u, 20u, TEST_RCC_PLLP_DIV, 8u, 4u };
    rcc_FunctionState_t   state     = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 2u, 20u, TEST_RCC_PLLQR_DIV8, TEST_RCC_PLLQR_DIV4, RCC_PLLCFGR_PLLSRC_HSI ) |
                             TEST_RCC_PLLP_DIV7_BITS | RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_PLLON, RCC->CR & ( RCC_CR_HSION | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Rcc_Set_PllConfig() with no source only deactivates PLL.
 *
 * \details PLL on (not locked), source NONE requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLON cleared, PLLCFGR not written.
 */
void Ut_Rcc_Set_PllConfig_SourceNone_DeactivatesOnly( void )
{
    rcc_PllConfigStruct_t pllConfig = { RCC_PLL_SRC_NONE, 1u, 10u, 0u, 2u, 2u };

    RCC->CR = RCC_CR_PLLON;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects M / N out of range.
 *
 * \details M 0, M 9 (STM32L4) / 17 (STM32L4+), N 7, N 87.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLLCFGR not written, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_MnOutOfRange_ReturnsError( void )
{
    const rcc_PllConfigStruct_t invalidLut[] =
    {
        { RCC_PLL_SRC_HSI,                     0u, 10u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI, TEST_RCC_PLLM_INVALID, 10u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI,                     1u,  7u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI,                     1u, 87u, 0u, 0u, 2u },
    };

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        rcc_PllConfigStruct_t pllConfig = invalidLut[ idx ];

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects input / VCO frequency out of range.
 *
 * \details HSI ready preset. Input 2 MHz (M 8), VCO 352 MHz (N 22), VCO 32 MHz
 *          (M 4, N 8). HSE PLL without configured HSE frequency.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_FrequencyOutOfRange_ReturnsError( void )
{
    const rcc_PllConfigStruct_t invalidLut[] =
    {
        { RCC_PLL_SRC_HSI, 8u, 40u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI, 1u, 22u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI, 4u,  8u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSE, 1u, 20u, 0u, 0u, 2u },
    };

    RCC->CR = RCC_CR_HSIRDY;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        rcc_PllConfigStruct_t pllConfig = invalidLut[ idx ];

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects invalid output divider without PLL activation.
 *
 * \details HSI ready preset. P 1, P 32, Q 3, R 10.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_InvalidOutputDivider_ReturnsErrorWithoutActivation( void )
{
    const rcc_PllConfigStruct_t invalidLut[] =
    {
        { RCC_PLL_SRC_HSI, 1u, 10u,  1u, 0u,  2u },
        { RCC_PLL_SRC_HSI, 1u, 10u, 32u, 0u,  2u },
        { RCC_PLL_SRC_HSI, 1u, 10u,  0u, 3u,  2u },
        { RCC_PLL_SRC_HSI, 1u, 10u,  0u, 0u, 10u },
    };

    RCC->CR = RCC_CR_HSIRDY;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        rcc_PllConfigStruct_t pllConfig = invalidLut[ idx ];

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
    }
}


/**
 * \brief   Output P is rejected on MCUs without SAI (no main PLL output P).
 *
 * \details STM32L41x / L42x: P divider 7 requested. Other MCUs: P divider 17
 *          accepted by P bit (MCUs without PDIV) or PDIV field.
 *
 * \par Expected results
 * - Without output P: RCC_REQUEST_ERROR, P divider 0 accepted.
 * - With output P: RCC_REQUEST_OK, output P frequency VCO / 17.
 */
void Ut_Rcc_Pll_Set_OutP_AvailabilityChecked( void )
{
    rcc_FreqHz_t freq = 0u;

#if defined(RCC_PLLCFGR_PLLPEN)
    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, 17u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Set_OutP( RCC_PLL_1, 17u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutP( RCC_PLL_1, 7u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Pll_Set_OutP( RCC_PLL_1, 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
#endif /* RCC_PLLCFGR_PLLPEN */
}


/**
 * \brief   Unused PLL outputs (divider 0) are disabled.
 *
 * \details All output enable bits preset, HW model emulates ready flags. PLL
 *          configured with P and Q 0 (R used), then with P and R 0 (Q used).
 *
 * \par Expected results
 * - P, Q unused: RCC_REQUEST_OK, PLLPEN and PLLQEN cleared, PLLREN set.
 * - P, R unused: RCC_REQUEST_OK, PLLPEN and PLLREN cleared, PLLQEN set.
 */
void Ut_Rcc_Set_PllConfig_UnusedOutputs_Disabled( void )
{
    rcc_PllConfigStruct_t pllConfig = { RCC_PLL_SRC_HSI, 1u, 10u, 0u, 0u, 2u };

    RCC->PLLCFGR = TEST_RCC_PLL_OUTPUTS_EN;
    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLREN, RCC->PLLCFGR & TEST_RCC_PLL_OUTPUTS_EN );

    pllConfig.Q_Divider = 2u;
    pllConfig.R_Divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLQEN, RCC->PLLCFGR & TEST_RCC_PLL_OUTPUTS_EN );
}


/**
 * \brief   PLL output clocks are calculated from registers.
 *
 * \details PLLCFGR preset: HSI, M 1, N 20 (VCO 320 MHz), Q 6, R 4, outputs Q,
 *          R (and P) enabled. Output P: PDIV 10, PDIV 0 with P bit 0 / 1
 *          (dividers 7 / 17) where available.
 *
 * \par Expected results
 * - VCO 320 MHz, Q 53333333 Hz, R 80 MHz.
 * - P: PDIV 10 - VCO / 10, P bit 0 - VCO / 7, P bit 1 - VCO / 17.
 */
void Ut_Rcc_Get_PllClk_Outputs_CalculatedFromRegisters( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, 20u, TEST_RCC_PLLQR_DIV6, TEST_RCC_PLLQR_DIV4, RCC_PLLCFGR_PLLSRC_HSI ) | TEST_RCC_PLL_OUTPUTS_EN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 320000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 320000000u / 6u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 320000000u / 4u, freq );

#if defined(RCC_PLLCFGR_PLLPDIV)
    RCC->PLLCFGR |= 10u << RCC_PLLCFGR_PLLPDIV_Pos;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 320000000u / 10u, freq );
    RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLPDIV;
#endif /* RCC_PLLCFGR_PLLPDIV */

#if defined(RCC_PLLCFGR_PLLPEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 320000000u / 7u, freq );

    RCC->PLLCFGR |= RCC_PLLCFGR_PLLP;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 320000000u / 17u, freq );
#endif /* RCC_PLLCFGR_PLLPEN */
}


/**
 * \brief   PLL output clock getters reject disabled output and invalid arguments.
 *
 * \details Outputs disabled (enable bits 0), invalid PLL ID, NULL pointer, no
 *          PLL source selected.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 */
void Ut_Rcc_Get_PllClk_DisabledOutputOrInvalidArgs_ReturnsError( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 1u, 20u, TEST_RCC_PLLQR_DIV6, TEST_RCC_PLLQR_DIV4, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutR( RCC_PLL_1, &freq ) );

    RCC->PLLCFGR |= TEST_RCC_PLL_OUTPUTS_EN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutQ( RCC_PLL_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutR( RCC_PLL_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllInternalClk( RCC_PLL_1, NULL ) );

    RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
}


/**
 * \brief   PLL activation waits for ready flag and state follows it.
 *
 * \details HW model emulates PLL lock. PLL activated and deactivated.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, state ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, state INACTIVE.
 */
void Ut_Rcc_Set_PllActive_ReadyFlag_StateFollows( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   PLL activation fails if PLL does not lock.
 *
 * \details No HW model, ready flag stays 0. Then ready flag stuck (deactivation).
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_ERROR.
 * - Deactivation with ready flag set: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PllActive_NotLocked_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllActive( RCC_PLL_1 ) );

    RCC->CR = RCC_CR_PLLRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_1 ) );
}


/**
 * \brief   PLL source is written while PLL is inactive.
 *
 * \details HSE selected, then HSI (HSI ready preset), then MSI (MSI ready
 *          preset), source read back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLSRC = HSE / HSI / MSI, HSION / MSION set by selection.
 */
void Ut_Rcc_Set_PllsSource_PllInactive_WritesPllsrc( void )
{
    rcc_PllClkSrc_t source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSE, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSE, source );

    RCC->CR = RCC_CR_HSIRDY | RCC_CR_MSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSI, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION, RCC->CR & RCC_CR_HSION );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_MSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_MSI, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSION, RCC->CR & RCC_CR_MSION );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_MSI, source );
}


/**
 * \brief   PLL source can not be changed while PLL is active.
 *
 * \details PLL on with HSE source. HSI requested, HSE requested again, NONE
 *          requested. No source selected is read as NONE.
 *
 * \par Expected results
 * - HSI: RCC_REQUEST_ERROR, PLLSRC unchanged. HSE (same): RCC_REQUEST_OK.
 * - NONE: RCC_REQUEST_ERROR. PLLSRC 0: source NONE.
 */
void Ut_Rcc_Set_PllsSource_PllActive_ReturnsError( void )
{
    rcc_PllClkSrc_t source = RCC_PLL_SRC_HSI;

    RCC->CR      = RCC_CR_HSIRDY | RCC_CR_PLLON;
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE;

#if defined(RCC_CR_PLLSAI1ON)
    /* Source is shared - checked with PLLSAI1, main PLL is the active one */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_2, RCC_PLL_SRC_HSI ) );
#endif /* RCC_CR_PLLSAI1ON */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSE, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_NONE ) );

    RCC->PLLCFGR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE, source );
}


/**
 * \brief   Settings shared with the active main PLL protect PLLSAI1 configuration.
 *
 * \details Main PLL on and locked with HSI source and M 2. PLLSAI1 configured
 *          with HSE source, then with HSI and M 1 (STM32L4: M shared with main
 *          PLL, STM32L4+: own M of PLLSAI1), then with HSI and M 2. HW model
 *          emulates ready flags. MCUs without PLLSAI1: test ignored.
 *
 * \par Expected results
 * - HSE: RCC_REQUEST_ERROR (source of the active main PLL), PLLSAI1 off.
 * - M 1: STM32L4 RCC_REQUEST_ERROR (main PLL M kept 2), STM32L4+ RCC_REQUEST_OK
 *   (PLLSAI1M = 0).
 * - M 2: RCC_REQUEST_OK, PLLSAI1 on, main PLL configuration unchanged.
 */
void Ut_Rcc_Set_PllConfig_PllSai1SharedSettings_Protected( void )
{
#if defined(RCC_CR_PLLSAI1ON)
    rcc_PllConfigStruct_t pllConfig  = { RCC_PLL_SRC_HSE, 2u, 24u, 0u, 4u, 0u };
    const uint32_t        mainConfig = TEST_RCC_PLLCFGR( 2u, 20u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) | RCC_PLLCFGR_PLLREN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_FREQ_HZ ) );

    RCC->CR      = RCC_CR_HSION | RCC_CR_PLLON;
    RCC->PLLCFGR = mainConfig;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLSAI1ON );

    pllConfig.Pll_Source   = RCC_PLL_SRC_HSI;
    pllConfig.M_Divider    = 1u;
    pllConfig.N_Multiplier = 12u;
#if defined(RCC_PLLSAI1CFGR_PLLSAI1M)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLSAI1CFGR & RCC_PLLSAI1CFGR_PLLSAI1M );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
#endif /* RCC_PLLSAI1CFGR_PLLSAI1M */

    pllConfig.M_Divider    = 2u;
    pllConfig.N_Multiplier = 24u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_PLLSAI1ON, RCC->CR & RCC_CR_PLLSAI1ON );
    TEST_ASSERT_EQUAL_HEX32( mainConfig, RCC->PLLCFGR );
#else
    TEST_IGNORE_MESSAGE( "MCU without PLLSAI1" );
#endif /* RCC_CR_PLLSAI1ON */
}

/* ============================== OSCILLATORS =============================== */

/**
 * \brief   Oscillator activation sets enable bit and waits for ready flag.
 *
 * \details HW model emulates ready flags of HSI, MSI, HSI48 (where available),
 *          LSI, LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSION, MSION, HSI48ON, LSION, LSEON set, states ACTIVE.
 * - LSE in backup domain: write protection released (DBP).
 */
void Ut_Rcc_Set_OscActive_ReadyFlags_SetsEnableBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Start_HwModel();

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    }

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_MSION, RCC->CR & ( RCC_CR_HSION | RCC_CR_MSION ) );
#if defined(RCC_CRRCR_HSI48ON)
    TEST_ASSERT_EQUAL_HEX32( RCC_CRRCR_HSI48ON, RCC->CRRCR & RCC_CRRCR_HSI48ON );
#endif /* RCC_CRRCR_HSI48ON */
    TEST_ASSERT_EQUAL_HEX32( RCC_CSR_LSION,     RCC->CSR   & RCC_CSR_LSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSEON,    RCC->BDCR  & RCC_BDCR_LSEON );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR1_DBP,       PWR->CR1   & PWR_CR1_DBP );
}


/**
 * \brief   Oscillator activation fails if the oscillator does not get ready.
 *
 * \details No HW model, ready flags stay 0.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for every oscillator, state INACTIVE.
 */
void Ut_Rcc_Set_OscActive_NotReady_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Oscillator deactivation clears enable bit and waits until not ready.
 *
 * \details Oscillators on and ready, HW model emulates ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, enable bits cleared, states INACTIVE.
 */
void Ut_Rcc_Set_OscInactive_AllOscillators_ClearsEnableBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR    = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_MSION | RCC_CR_MSIRDY;
#if defined(RCC_CRRCR_HSI48ON)
    RCC->CRRCR = RCC_CRRCR_HSI48ON | RCC_CRRCR_HSI48RDY;
#endif /* RCC_CRRCR_HSI48ON */
    RCC->CSR   = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    RCC->BDCR  = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;

    Ut_Rcc_Start_HwModel();

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR    & ( RCC_CR_HSION | RCC_CR_MSION ) );
#if defined(RCC_CRRCR_HSI48ON)
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CRRCR & RCC_CRRCR_HSI48ON );
#endif /* RCC_CRRCR_HSI48ON */
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR   & RCC_CSR_LSION );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR  & RCC_BDCR_LSEON );
}


/**
 * \brief   Oscillator deactivation fails if the ready flag stays set.
 *
 * \details No HW model, ready flags stuck at 1.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for every oscillator.
 */
void Ut_Rcc_Set_OscInactive_ReadyFlagStuck_ReturnsError( void )
{
    RCC->CR    = RCC_CR_HSIRDY | RCC_CR_MSIRDY;
#if defined(RCC_CRRCR_HSI48ON)
    RCC->CRRCR = RCC_CRRCR_HSI48RDY;
#endif /* RCC_CRRCR_HSI48ON */
    RCC->CSR   = RCC_CSR_LSIRDY;
    RCC->BDCR  = RCC_BDCR_LSERDY;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( oscId ) );
    }
}


/**
 * \brief   Oscillator state requires enable bit and ready flag.
 *
 * \details Ready flags without enable bits, then enable bits without ready flags.
 *
 * \par Expected results
 * - State INACTIVE in both cases.
 */
void Ut_Rcc_Get_OscState_RequiresOnAndReady( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR    = RCC_CR_HSIRDY | RCC_CR_MSIRDY;
#if defined(RCC_CRRCR_HSI48ON)
    RCC->CRRCR = RCC_CRRCR_HSI48RDY;
#endif /* RCC_CRRCR_HSI48ON */
    RCC->CSR   = RCC_CSR_LSIRDY;
    RCC->BDCR  = RCC_BDCR_LSERDY;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    RCC->CR    = RCC_CR_HSION | RCC_CR_MSION;
#if defined(RCC_CRRCR_HSI48ON)
    RCC->CRRCR = RCC_CRRCR_HSI48ON;
#endif /* RCC_CRRCR_HSI48ON */
    RCC->CSR   = RCC_CSR_LSION;
    RCC->BDCR  = RCC_BDCR_LSEON;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Oscillator functions reject invalid arguments.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for RCC_OSC_CNT and NULL pointers.
 */
void Ut_Rcc_OscFunctions_InvalidArgs_ReturnError( void )
{
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_OscDiv_t        oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( RCC_OSC_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_CNT, &oscDiv ) );

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( oscId, NULL ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( oscId, NULL ) );
    }
}


/**
 * \brief   Oscillators have no output divider - only divider 1 is accepted.
 *
 * \par Expected results
 * - Divider 1: RCC_REQUEST_OK, divider 2 / invalid oscillator: RCC_REQUEST_ERROR.
 * - Rcc_Get_OscDiv() returns 1.
 */
void Ut_Rcc_Set_OscDiv_OnlyDividerOneAccepted( void )
{
    rcc_OscDiv_t oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_OscDiv( RCC_OSC_HSI, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_HSI, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_CNT, 1u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_MSI, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, oscDiv );
}

/* ============================== RTC CLOCK ================================= */

/**
 * \brief   RTC clock source LSE is written to RTCSEL.
 *
 * \details RTCSEL 0 (no clock), LSE requested, same source requested again.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCSEL = LSE, read back as LSE.
 */
void Ut_Rcc_Set_RtcClkSource_Lse_WritesRtcsel( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_0, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_LSE, source );
}


/**
 * \brief   RTC clock source can not be changed after selection.
 *
 * \details RTCSEL preset to LSI, HSE / 32 requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RTCSEL stays LSI.
 */
void Ut_Rcc_Set_RtcClkSource_OtherSelected_ReturnsError( void )
{
    RCC->BDCR = RCC_BDCR_RTCSEL_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_HSE_DIV ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   RTC clock source read - no clock and HSE / 32.
 *
 * \details RTCSEL 0, then RTCSEL = HSE / 32. Invalid arguments.
 *
 * \par Expected results
 * - No clock: RCC_REQUEST_ERROR. HSE / 32: RCC_RTC_CLK_SOURCE_HSE_DIV.
 * - Invalid source / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_RtcClkSource_NoClockAndHse( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( &source ) );

    RCC->BDCR = RCC_BDCR_RTCSEL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_HSE_DIV, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( NULL ) );
}

/* ============================ CLOCK OUTPUTS =============================== */

/**
 * \brief   MCO source PLL R configures MCOSEL and pin PA8.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MCOSEL = PLL, Gpio_Init() called for PA8 (AF0).
 */
void Ut_Rcc_Set_ClkOutSource_McoPllr_ConfiguresPa8( void )
{
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_McoPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_PLLR ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1SOURCE_PLLCLK, RCC->CFGR & RCC_CFGR_MCOSEL );
}


/**
 * \brief   MCO source is not selected if the pin can not be configured.
 *
 * \details Gpio_Init() of PA8 returns error.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, MCOSEL not written.
 */
void Ut_Rcc_Set_ClkOutSource_PinError_SourceNotSelected( void )
{
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_McoPin, GPIO_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_SYSCLK ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );
}


/**
 * \brief   LSCO source LSI enables LSCO and configures pin PA2 analog.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, LSCOEN set, LSCOSEL = LSI, Gpio_Init() called for PA2 (analog).
 */
void Ut_Rcc_Set_ClkOutSource_LscoLsi_ConfiguresPa2Analog( void )
{
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_LscoPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_LSCO_LSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSCOEN, RCC->BDCR & ( RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL ) );
}


/**
 * \brief   Clock output source NONE or source of other output changes nothing.
 *
 * \details NONE for both outputs, LSCO source for MCO, MCO source for LSCO,
 *          invalid output ID.
 *
 * \par Expected results
 * - NONE: RCC_REQUEST_OK, others: RCC_REQUEST_ERROR.
 * - CFGR / BDCR not written, no pin configured.
 */
void Ut_Rcc_Set_ClkOutSource_NoneOrInvalid_NoChange( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_LSCO_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_MCO1_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR | RCC->BDCR );
}


/**
 * \brief   Clock output sources are read from CFGR and BDCR.
 *
 * \details MCOSEL MSI, MCOSEL 0. LSCOSEL set without LSCOEN, then with
 *          LSCOEN. Invalid arguments.
 *
 * \par Expected results
 * - MCO: MSI, then NONE.
 * - LSCO: LSCOSEL without LSCOEN is no valid selection (RCC_REQUEST_ERROR),
 *   LSE when enabled, NONE when cleared.
 * - Invalid output / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkOutSource_ReadsCfgrAndBdcr( void )
{
    rcc_ClkOut_Source_t source = RCC_CLK_SOURCE_CNT;

    RCC->CFGR = LL_RCC_MCO1SOURCE_MSI;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO1_MSI, source );

    RCC->CFGR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    RCC->BDCR = RCC_BDCR_LSCOSEL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );

    RCC->BDCR = RCC_BDCR_LSCOSEL | RCC_BDCR_LSCOEN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_LSCO_LSE, source );

    RCC->BDCR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_CNT,  &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, NULL ) );
}


/**
 * \brief   MCO dividers 1, 2, 4, 8, 16 are written to MCOPRE and read back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MCOPRE = log2 of the divider, divider read back.
 */
void Ut_Rcc_Set_ClkOutDivider_AllDividers_ReadBack( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    for( uint32_t preVal = 0u; 4u >= preVal; preVal++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 1u << preVal ) );
        TEST_ASSERT_EQUAL_HEX32( preVal << RCC_CFGR_MCOPRE_Pos, RCC->CFGR & RCC_CFGR_MCOPRE );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
        TEST_ASSERT_EQUAL_UINT32( 1u << preVal, divider );
    }
}


/**
 * \brief   Invalid clock output dividers are rejected, LSCO has no divider.
 *
 * \details MCO 0, 3, 32. LSCO 2 and 1. Invalid output, NULL pointer, reserved
 *          MCOPRE value.
 *
 * \par Expected results
 * - MCO invalid dividers, LSCO 2, invalid output: RCC_REQUEST_ERROR, CFGR not written.
 * - LSCO 1: RCC_REQUEST_OK, LSCO divider read as 1.
 * - Reserved MCOPRE (5) and NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_ClkOutDivider_InvalidOrLsco_Rejected( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1,  0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1,  3u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 32u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO,  2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_CNT,   1u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_LSCO, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, divider );

    RCC->CFGR = 5u << RCC_CFGR_MCOPRE_Pos;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_CNT,  &divider ) );
}

/* ============================ RESET SOURCES =============================== */

/**
 * \brief   Every reset source flag of CSR is decoded separately.
 *
 * \details Each flag preset alone, all sources read.
 *
 * \par Expected results
 * - Only the source of the preset flag is ACTIVE.
 * - Invalid source / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ResetSource_EveryFlag_DecodedSeparately( void )
{
    const uint32_t flagLut[ RCC_RESET_SRC_CNT ] =
    {
        RCC_CSR_PINRSTF,  RCC_CSR_BORRSTF,  RCC_CSR_SFTRSTF, RCC_CSR_IWDGRSTF,
        RCC_CSR_WWDGRSTF, RCC_CSR_LPWRRSTF, RCC_CSR_OBLRSTF, RCC_CSR_FWRSTF
    };
    rcc_FlagState_t flag = RCC_FLAG_INACTIVE;

    for( uint32_t flagIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > flagIdx; flagIdx++ )
    {
        RCC->CSR = flagLut[ flagIdx ];

        for( uint32_t srcIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > srcIdx; srcIdx++ )
        {
            rcc_FlagState_t expected = RCC_FLAG_INACTIVE;

            if( srcIdx == flagIdx )
            {
                expected = RCC_FLAG_ACTIVE;
            }
            else
            {
                /* Flag of other source is not set */
            }

            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( (rcc_ResetSrc_t)srcIdx, &flag ) );
            TEST_ASSERT_EQUAL( expected, flag );
        }
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_CNT, &flag ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_PIN, NULL ) );
}


/**
 * \brief   Reset source flags are cleared by RMVF, RMVF is released.
 *
 * \details All flags set, HW model clears flags while RMVF is set.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, all flags cleared, RMVF cleared.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsCleared_ReleasesRmvf( void )
{
    RCC->CSR = TEST_RCC_CSR_RESET_FLAGS;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR & ( TEST_RCC_CSR_RESET_FLAGS | RCC_CSR_RMVF ) );
}


/**
 * \brief   Reset source clear fails when flags are kept, RMVF is still released.
 *
 * \details All flags set, no HW model (flags stay set).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RMVF cleared.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsKept_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->CSR = TEST_RCC_CSR_RESET_FLAGS;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR & RCC_CSR_RMVF );
}

/* ============================ COMPONENTS ================================== */

/**
 * \brief   HSE state requires HSEON and HSERDY.
 *
 * \details HSERDY only, HSEON only, both. NULL pointer.
 *
 * \par Expected results
 * - INACTIVE, INACTIVE, ACTIVE; NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_HseState_RequiresOnAndReady( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR = RCC_CR_HSERDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSEON;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseState( NULL ) );
}


/**
 * \brief   HSE activation with type NONE only stops HSE, invalid type is rejected.
 *
 * \details HSE on with bypass preset. Type NONE, then invalid type. HSE
 *          frequency 0 and NULL pointers.
 *
 * \par Expected results
 * - NONE: RCC_REQUEST_OK, HSEON cleared (bypass kept).
 * - Invalid type: RCC_REQUEST_ERROR, HSEON not set.
 * - Frequency 0 / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_Set_HseActive_NoneOrInvalidType( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CR = RCC_CR_HSEON | RCC_CR_HSEBYP;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_NONE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSEBYP, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( (rcc_HseType_t)5u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSEON );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseClk( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseRtcClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_ClkSrc_Get_HseClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
}


/**
 * \brief   HSE activation fails if running HSE can not be stopped.
 *
 * \details HSE on and ready, no HW model - ready flag stays set after HSEON is
 *          cleared. Type NONE (stop only) and crystal are requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases, HSE is not started again (HSEON 0).
 */
void Ut_Rcc_ClkSrc_Set_HseActive_ReadyStuck_ReturnsError( void )
{
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_CRYSTAL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSEON );
}


/**
 * \brief   Oscillator frequency getters of the clock source component.
 *
 * \par Expected results
 * - HSI 16 MHz, HSI48 48 MHz (MCUs with HSI48, error otherwise), LSI 32 kHz,
 *   LSE 32.768 kHz.
 * - NULL pointers: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_OscillatorFrequencies_Returned( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
#if defined(RCC_CRRCR_HSI48ON)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_Hsi48Clk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_Hsi48Clk( &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_Hsi48Active() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_Hsi48Inactive() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_Hsi48State( NULL ) );
#endif /* RCC_CRRCR_HSI48ON */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_LsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_LseClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HsiClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_Hsi48Clk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LsiClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LseClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_MsiClk( NULL ) );
}


/**
 * \brief   MSI frequency is given by MSIRANGE (MSIRGSEL set) or MSISRANGE.
 *
 * \details MSIRGSEL with every MSIRANGE value incl. reserved 1100. MSIRGSEL 0
 *          with MSISRANGE 1 / 8 MHz and reserved 0011 / 1000.
 *
 * \par Expected results
 * - MSIRANGE 0 - 11: 100 kHz ... 48 MHz, 1100: RCC_REQUEST_ERROR and 0 Hz.
 * - MSISRANGE 0100: 1 MHz, 0111: 8 MHz, other values RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_Get_MsiClk_RangeOrStandbyRange( void )
{
    static const rcc_FreqHz_t msiLut[] =
    {
          100000u,   200000u,   400000u,   800000u,  1000000u,  2000000u,
         4000000u,  8000000u, 16000000u, 24000000u, 32000000u, 48000000u
    };
    rcc_FreqHz_t freq = 0u;

    for( uint32_t rangeIdx = 0u; TEST_RCC_ARRAY_CNT( msiLut ) > rangeIdx; rangeIdx++ )
    {
        RCC->CR = RCC_CR_MSIRGSEL | ( rangeIdx << RCC_CR_MSIRANGE_Pos );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_MsiClk( &freq ) );
        TEST_ASSERT_EQUAL_UINT32( msiLut[ rangeIdx ], freq );
    }

    RCC->CR = RCC_CR_MSIRGSEL | ( 12u << RCC_CR_MSIRANGE_Pos );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_MsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );

    RCC->CR  = 0u;
    RCC->CSR = RCC_CSR_MSISRANGE_1;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_MsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 1000000u, freq );

    RCC->CSR = RCC_CSR_MSISRANGE_8;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_MsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 8000000u, freq );

    RCC->CSR = 3u << RCC_CSR_MSISRANGE_Pos;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_MsiClk( &freq ) );

    RCC->CSR = 8u << RCC_CSR_MSISRANGE_Pos;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_MsiClk( &freq ) );
}


/**
 * \brief   MSI range is written only while MSI is off or ready.
 *
 * \details MSI off: range 16 MHz. MSI on and ready: range 24 MHz. MSI on and
 *          not ready: range 32 MHz. Invalid range values.
 *
 * \par Expected results
 * - Off / ready: RCC_REQUEST_OK, MSIRGSEL set, MSIRANGE written.
 * - Starting MSI: RCC_REQUEST_ERROR, MSIRANGE unchanged.
 * - Value out of field or above 48 MHz: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_Set_MsiRange_OnlyWhenOffOrReady( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_MsiRange( RCC_MSI_RANGE_16MHZ ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE_8, RCC->CR );

    RCC->CR |= RCC_CR_MSION | RCC_CR_MSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_MsiRange( RCC_MSI_RANGE_24MHZ ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSIRANGE_9, RCC->CR & RCC_CR_MSIRANGE );

    RCC->CR &= ~RCC_CR_MSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_MsiRange( RCC_MSI_RANGE_32MHZ ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_MSIRANGE_9, RCC->CR & RCC_CR_MSIRANGE );

    RCC->CR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_MsiRange( (rcc_MsiRange_t)( 12u << RCC_CR_MSIRANGE_Pos ) ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_MsiRange( (rcc_MsiRange_t)1u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
}


/**
 * \brief   Initialization / task functions of internal components do not touch the hardware,
 *          configuration tables are consistent.
 *
 * \details Rcc_ClkBus_Init / Deinit / Task, Rcc_ClkMux_Init / Deinit / Task,
 *          Rcc_Pll_Init / Task.
 *
 * \par Expected results
 * - Init functions return RCC_REQUEST_OK (configuration tables indexed correctly).
 * - RCC registers stay 0.
 */
void Ut_Rcc_Components_InitTask_NoRegisterAccess( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Init() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Init() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Init() );

    Rcc_ClkBus_Task();
    Rcc_ClkBus_Deinit();
    Rcc_ClkMux_Task();
    Rcc_ClkMux_Deinit();
    Rcc_Pll_Task();

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR | RCC->CFGR | RCC->PLLCFGR | RCC->BDCR | RCC->CSR | RCC->CCIPR );
}


/**
 * \brief   Rcc_Pll_Deinit() disables all PLLs.
 *
 * \details PLL on and not locked (disable succeeds), then ready flag kept (PLL
 *          can not be stopped).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLON cleared.
 * - Locked PLL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Pll_Deinit_PllDisabled( void )
{
    RCC->CR = RCC_CR_PLLON;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Deinit() );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );

    RCC->CR = RCC_CR_PLLON | RCC_CR_PLLRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Deinit() );
}


/**
 * \brief   Clock multiplexer component - activation and release of RTC clock selection.
 *
 * \details RTCSEL 0 (no clock): LSI selected, selected again, LSE selected while LSI is
 *          selected, multiplexer released (emulated register accepts the write, RTCSEL of HW is
 *          write-once), invalid arguments.
 *
 * \par Expected results
 * - LSI: RTCSEL = LSI, repeated selection accepted, selection read back.
 * - LSE while LSI selected: RCC_REQUEST_ERROR, RTCSEL unchanged.
 * - Release: RTCSEL = 0. Invalid arguments: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkMux_RtcSelectAndRelease( void )
{
    rcc_ClkMuxId_t clkMuxId = RCC_CLK_MUX_LIST_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Set_ClkActive( RCC_CLK_MUX_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, RCC->BDCR & RCC_BDCR_RTCSEL );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Set_ClkActive( RCC_CLK_MUX_RTC_LSI ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Get_ClkSrc( RCC_CLK_MUX_RTC_NONE, &clkMuxId ) );
    TEST_ASSERT_EQUAL( RCC_CLK_MUX_RTC_LSI, clkMuxId );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Set_ClkActive( RCC_CLK_MUX_RTC_LSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Set_ClkInactive( RCC_CLK_MUX_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Set_ClkActive( RCC_CLK_MUX_LIST_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Set_ClkInactive( RCC_CLK_MUX_LIST_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Get_ClkSrc( RCC_CLK_MUX_LIST_CNT, &clkMuxId ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Get_ClkSrc( RCC_CLK_MUX_RTC_NONE, NULL ) );
}


/**
 * \brief   Clock multiplexer component reports error for unknown selection.
 *
 * \details I2C1SEL preset to reserved value 11.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, selection not found.
 */
void Ut_Rcc_ClkMux_ReservedSelection_ReturnsError( void )
{
    rcc_ClkMuxId_t clkMuxId = RCC_CLK_MUX_LIST_CNT;

    RCC->CCIPR = RCC_CCIPR_I2C1SEL;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Get_ClkSrc( RCC_CLK_MUX_I2C1_PCLK1, &clkMuxId ) );
    TEST_ASSERT_EQUAL( RCC_CLK_MUX_LIST_CNT, clkMuxId );
}


/**
 * \brief   Bus clock component rejects invalid arguments.
 *
 * \details NULL pointers of getters, invalid system clock source.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, CFGR unchanged.
 */
void Ut_Rcc_ClkBus_InvalidArgs_ReturnError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_SysClkSource( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_SysClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_AHBDivider( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_AHBClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_APB1Divider( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_APB1Clk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_APB2Divider( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_APB2Clk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_APB1TimClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_APB2TimClk( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );
}


/**
 * \brief   System clock switch fails if the source does not get selected.
 *
 * \details No HW model, SWS stays 00 (MSI).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, SW = HSE written.
 */
void Ut_Rcc_ClkBus_Set_SysClkSource_NotSwitched_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SW_HSE, RCC->CFGR & RCC_CFGR_SW );
}


/**
 * \brief   System clock of HSE, HSI and MSI source is reported by bus clock component.
 *
 * \details SWS = HSE (HSE frequency 8 MHz), SWS = HSI, SWS = MSI (MSI range 48 MHz).
 *
 * \par Expected results
 * - HSE: 8 MHz and source HSE, HSI: 16 MHz, MSI: 48 MHz and source MSI.
 */
void Ut_Rcc_ClkBus_Get_SysClk_HseHsiMsi( void )
{
    rcc_FreqHz_t       sysClk = 0u;
    rcc_SystemClkSrc_t source = RCC_SYSTEM_CLOCK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_FREQ_HZ ) );

    RCC->CFGR = RCC_CFGR_SWS_HSE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Get_SysClk( &sysClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_FREQ_HZ, sysClk );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Get_SysClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_HSE, source );

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Get_SysClk( &sysClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, sysClk );

    RCC->CFGR = RCC_CFGR_SWS_MSI;
    RCC->CR   = RCC_CR_MSIRGSEL | RCC_CR_MSIRANGE_11;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Get_SysClk( &sysClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_MSI_48MHZ_HZ, sysClk );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Get_SysClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_MSI, source );
}


/**
 * \brief   PLL component rejects invalid arguments.
 *
 * \details Invalid PLL ID, NULL pointers, invalid PLL source, invalid output
 *          dividers, RTC clock source. PLLSAI2 output Q of STM32L4 (not
 *          available).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, PLLCFGR unchanged.
 */
void Ut_Rcc_Pll_InvalidArgs_ReturnError( void )
{
    rcc_FreqHz_t        pllClk = 0u;
    rcc_PllClkSrc_t     source = RCC_PLL_SRC_NONE;
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_InternalClk( RCC_PLL_CNT, &pllClk ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Active( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Inactive( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_State( RCC_PLL_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_State( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Source( RCC_PLL_CNT, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Source( RCC_PLL_1, RCC_PLL_SRC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Source( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Source( RCC_PLL_CNT, &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutP( RCC_PLL_CNT, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutQ( RCC_PLL_CNT, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutR( RCC_PLL_CNT, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutQ( RCC_PLL_1, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutR( RCC_PLL_1, 3u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Clk_OutP( RCC_PLL_CNT, &pllClk ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_CNT, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_RtcClkSource( NULL ) );
#if defined(RCC_CR_PLLSAI2ON) && \
    !defined(RCC_PLLSAI2CFGR_PLLSAI2Q)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutQ( RCC_PLL_3, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Pll_Set_OutQ( RCC_PLL_3, 0u ) );
#endif /* RCC_CR_PLLSAI2ON && !RCC_PLLSAI2CFGR_PLLSAI2Q */

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
}


/**
 * \brief   Register component ignores invalid register identification.
 *
 * \details Set / reset / read with RCC_REG_CNT.
 *
 * \par Expected results
 * - Getters return 0, setters do not write any RCC / PWR register.
 */
void Ut_Rcc_Reg_InvalidRegister_Ignored( void )
{
    Rcc_Set_RegBit( RCC_REG_CNT, 0xFFFFFFFFu );
    Rcc_Reset_RegBit( RCC_REG_CNT, 0xFFFFFFFFu );
    Rcc_Set_RegVal( RCC_REG_CNT, 0xFFFFFFFFu, 0xFFFFFFFFu );

    TEST_ASSERT_EQUAL_HEX32( 0u, Rcc_Get_RegBit( RCC_REG_CNT, 0xFFFFFFFFu ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, Rcc_Get_RegVal( RCC_REG_CNT, 0xFFFFFFFFu ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR | RCC->CFGR | RCC->BDCR | RCC->CSR | PWR->CR1 );
}


/**
 * \brief   Write of backup domain register releases write protection once.
 *
 * \details DBP preset (protection already released), BDCR written.
 *
 * \par Expected results
 * - BDCR written, PWR interface clock not touched (APB1ENR1 stays 0).
 */
void Ut_Rcc_Reg_BdcrWrite_DbpAlreadySet_NoPwrAccess( void )
{
    PWR->CR1 = PWR_CR1_DBP;

    Rcc_Set_RegBit( RCC_REG_BDCR, RCC_BDCR_LSEBYP );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSEBYP, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR1 );
}


/**
 * \brief   RTC clock selection is kept by deactivation of RTC clock.
 *
 * \details RTC activated with LSI (LSI ready), deactivated.
 *
 * \par Expected results
 * - RTCEN cleared, RTCSEL keeps LSI (write-once until backup domain reset).
 */
void Ut_Rcc_Set_PeriphInactive_Rtc_ClockSelectionKept( void )
{
    RCC->CSR = RCC_CSR_LSION | RCC_CSR_LSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1 | RCC_BDCR_RTCEN, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief HW model of RCC and PWR (called repeatedly from RegMem model thread).
 *
 * Emulates HW reaction on register writes:
 * - RCC_CR ready flags follow enable bits of HSI, MSI, HSE, PLL, PLLSAI1 and
 *   PLLSAI2 (HSE ready only if \ref utRcc_HseFails is false)
 * - RCC_CRRCR HSI48RDY follows HSI48ON
 * - RCC_CFGR SWS follows SW (AHB prescaler recorded on the switch to PLL)
 * - RCC_CSR LSIRDY follows LSION, reset flags are cleared while RMVF is set
 * - RCC_BDCR LSERDY follows LSEON
 *
 * \note Registers are updated by compare-and-swap - a write of the module
 *       between read and write of the model is never lost.
 */
static void Ut_Rcc_HwModel( void )
{
    const uint32_t cr    = RCC->CR;
    uint32_t       crRdy = 0u;

    if( 0u != ( cr & RCC_CR_HSION ) )
    {
        crRdy |= RCC_CR_HSIRDY;
    }
    else
    {
        /* No action required */
    }

    if( 0u != ( cr & RCC_CR_MSION ) )
    {
        crRdy |= RCC_CR_MSIRDY;
    }
    else
    {
        /* No action required */
    }

    if( ( 0u    != ( cr & RCC_CR_HSEON ) ) &&
        ( false == utRcc_HseFails        )    )
    {
        crRdy |= RCC_CR_HSERDY;
    }
    else
    {
        /* No action required */
    }

    if( 0u != ( cr & RCC_CR_PLLON ) )
    {
        crRdy |= RCC_CR_PLLRDY;
    }
    else
    {
        /* No action required */
    }

#if defined(RCC_CR_PLLSAI1ON)
    if( 0u != ( cr & RCC_CR_PLLSAI1ON ) )
    {
        crRdy |= RCC_CR_PLLSAI1RDY;
    }
    else
    {
        /* No action required */
    }
#endif /* RCC_CR_PLLSAI1ON */

#if defined(RCC_CR_PLLSAI2ON)
    if( 0u != ( cr & RCC_CR_PLLSAI2ON ) )
    {
        crRdy |= RCC_CR_PLLSAI2RDY;
    }
    else
    {
        /* No action required */
    }
#endif /* RCC_CR_PLLSAI2ON */

    Ut_Rcc_HwModel_Update( &RCC->CR, TEST_RCC_CR_RDY_MASK, crRdy );

#if defined(RCC_CRRCR_HSI48ON)
    uint32_t crrcrRdy = 0u;

    if( 0u != ( RCC->CRRCR & RCC_CRRCR_HSI48ON ) )
    {
        crrcrRdy = RCC_CRRCR_HSI48RDY;
    }
    else
    {
        /* No action required */
    }

    Ut_Rcc_HwModel_Update( &RCC->CRRCR, RCC_CRRCR_HSI48RDY, crrcrRdy );
#endif /* RCC_CRRCR_HSI48ON */

    const uint32_t cfgr = RCC->CFGR;
    const uint32_t sws  = ( cfgr & RCC_CFGR_SW ) << ( RCC_CFGR_SWS_Pos - RCC_CFGR_SW_Pos );

    if( ( RCC_CFGR_SWS_PLL == sws                     ) &&
        ( RCC_CFGR_SWS_PLL != ( cfgr & RCC_CFGR_SWS ) )    )
    {
        utRcc_HpreAtPllSwitch = cfgr & RCC_CFGR_HPRE;
    }
    else
    {
        /* No action required */
    }

    Ut_Rcc_HwModel_Update( &RCC->CFGR, RCC_CFGR_SWS, sws );

    const uint32_t csr     = RCC->CSR;
    uint32_t       csrMask = RCC_CSR_LSIRDY;
    uint32_t       csrRdy  = 0u;

    if( 0u != ( csr & RCC_CSR_RMVF ) )
    {
        csrMask |= TEST_RCC_CSR_RESET_FLAGS;
    }
    else
    {
        /* No action required */
    }

    if( 0u != ( csr & RCC_CSR_LSION ) )
    {
        csrRdy = RCC_CSR_LSIRDY;
    }
    else
    {
        /* No action required */
    }

    Ut_Rcc_HwModel_Update( &RCC->CSR, csrMask, csrRdy );

    uint32_t bdcrRdy = 0u;

    if( 0u != ( RCC->BDCR & RCC_BDCR_LSEON ) )
    {
        bdcrRdy = RCC_BDCR_LSERDY;
    }
    else
    {
        /* No action required */
    }

    Ut_Rcc_HwModel_Update( &RCC->BDCR, RCC_BDCR_LSERDY, bdcrRdy );
}


/**
 * \brief Writes masked field of emulated register if it differs (compare-and-swap).
 *
 * If the module writes the register meanwhile, the update is skipped and done
 * in the next model cycle.
 *
 * \param reg   [in]: Emulated register
 * \param mask  [in]: Mask of the field
 * \param value [in]: Required value of the field
 */
static void Ut_Rcc_HwModel_Update( volatile uint32_t * const reg, uint32_t mask, uint32_t value )
{
    uint32_t oldVal = *reg;

    if( ( oldVal & mask ) != value )
    {
        const uint32_t newVal = ( oldVal & ~mask ) | value;

        (void)__atomic_compare_exchange_n( reg, &oldVal, newVal, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
    }
    else
    {
        /* No action required */
    }
}


/**
 * \brief Starts HW model \ref Ut_Rcc_HwModel (stopped by tearDown / RegMem_Reset).
 */
static void Ut_Rcc_Start_HwModel( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
}


/**
 * \brief Configuration with HSI system clock, no PLL, no HSE.
 *
 * \param config [out]: Clock configuration
 */
static void Ut_Rcc_Get_HsiOnlyConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->SystemClockSource                  = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    config->Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;
}


/**
 * \brief Configuration of 80 MHz from 8 MHz HSE crystal PLL.
 *
 * HSE crystal 8 MHz, PLL M 1, N 20, P not used, Q 4, R 2, AHB / 1, APB1 / 2, APB2 / 1.
 *
 * \param config [out]: Clock configuration
 */
static void Ut_Rcc_Get_HsePllConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->HSE_ClockType    = RCC_HSE_TYPE_CRYSTAL;
    config->HSE_Frequency_Hz = TEST_RCC_HSE_FREQ_HZ;
    config->APB1_Divider     = RCC_APB1_DIVIDER_2;

    config->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSE;
    config->Pll_Config[ RCC_PLL_1 ].M_Divider    = 1u;
    config->Pll_Config[ RCC_PLL_1 ].N_Multiplier = 20u;
    config->Pll_Config[ RCC_PLL_1 ].P_Divider    = 0u;
    config->Pll_Config[ RCC_PLL_1 ].Q_Divider    = 4u;
    config->Pll_Config[ RCC_PLL_1 ].R_Divider    = 2u;
}


/**
 * \brief Clears all enable, reset and sleep registers of RCC.
 */
static void Ut_Rcc_Clear_PeriphRegs( void )
{
    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_EnableRegs ) > idx; idx++ )
    {
        *utRcc_EnableRegs[ idx ] = 0u;
    }

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ) > idx; idx++ )
    {
        *utRcc_ResetRegs[ idx ] = 0u;
    }

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_SleepRegs ) > idx; idx++ )
    {
        *utRcc_SleepRegs[ idx ] = 0u;
    }
}


/**
 * \brief Checks that registers of the list (except the used one) are 0.
 *
 * \param regList [in]: List of registers
 * \param regCnt  [in]: Count of registers in the list
 * \param usedReg [in]: Register written by the test (not checked), NULL - all checked
 */
static void Ut_Rcc_Check_OtherRegsClear( volatile uint32_t * const * regList, uint32_t regCnt, volatile uint32_t * const usedReg )
{
    for( uint32_t idx = 0u; regCnt > idx; idx++ )
    {
        if( usedReg != regList[ idx ] )
        {
            TEST_ASSERT_EQUAL_HEX32_MESSAGE( 0u, *regList[ idx ], "Register of other peripheral written" );
        }
        else
        {
            /* No action required */
        }
    }
}


/**
 * \brief Checks flash latency calculated for HSE system clock of the cases.
 *
 * \param caseList [in]: Voltage range, system clock and expected latency
 * \param caseCnt  [in]: Count of cases
 */
static void Ut_Rcc_Check_Latency( const utRcc_LatencyCase_t * const caseList, uint32_t caseCnt )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;

    for( uint32_t idx = 0u; caseCnt > idx; idx++ )
    {
        config.VoltageScaling   = caseList[ idx ].Scale;
        config.HSE_Frequency_Hz = caseList[ idx ].SysClk;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
        TEST_ASSERT_EQUAL_HEX32( caseList[ idx ].Latency, FLASH->ACR & FLASH_ACR_LATENCY );
    }
}
