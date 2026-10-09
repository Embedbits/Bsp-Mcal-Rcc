/**
 * \author Mr.Nobody
 * \file ItTest_Rcc.c
 * \ingroup Rcc
 * \brief Integration tests of Reset and Clock Control (RCC) module on target.
 *
 * Rcc module runs on the MCU together with real hardware. Tests verify behavior
 * which cannot be verified by unit tests (emulated registers): oscillator start
 * and PLL lock, system clock switching and reconfiguration of running clock
 * tree (voltage range 1 boost / normal mode, range 2, flash latency, AHB / 2
 * transition state above 80 MHz), hardware protection of clocks in use,
 * peripheral reset, real reset source flags and clock output pins.
 *
 * Real frequency of the clock tree is compared with an independent reference:
 * LSE crystal (32.768 kHz) is routed to MCO pin (PA8), its periods are counted
 * by polling of the pin input register and their duration is measured by DWT
 * cycle counter (processor clock HCLK).
 *
 * Every test case starts after system reset with default clock configuration
 * set by StartUp (PLL from HSI, SYSCLK 170 MHz in range 1 boost mode, APB1 and
 * APB2 not divided).
 *
 * Used resources (see board configuration below):
 * - IT_RCC_HSE_*      - HSE oscillator of the board
 * - IT_RCC_LSE_FITTED - LSE crystal fitted on the board (backup domain,
 *                       LSE is switched off in tearDown()), frequency
 *                       reference of HCLK measurement
 * - IT_RCC_FREE_*     - pin not connected on the board, used by peripheral
 *                       reset test
 * - PA8 (MCO)         - clock output, the pin outputs clock during the test
 *                       case (free on the board), LSE reference of HCLK
 *                       measurement read back by the input data register
 * - DWT cycle counter - duration of the LSE periods in HCLK cycles
 *
 * \note Tolerance of HCLK measurement: 2 % for clocks derived from HSI16
 *       (HSI16 accuracy over temperature), 0.2 % for clocks derived from the
 *       HSE crystal.
 * - PA2 (LSCO)        - low speed clock output, the pin outputs LSI during the
 *                       test case (LPUART1 TX of ST-LINK virtual COM port on
 *                       NUCLEO-G474RE - not used by integration testing).
 *                       LSCO is in backup domain - switched off in tearDown().
 *
 * \note RTC clock selection (RTCSEL) is write-once until backup domain reset,
 *       which is not available by public API - it is tested by unit tests only.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Gpio_Port.h"                      /* Pins of clock outputs          */
#include "Nvic_Port.h"                      /* System reset                   */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void         It_Rcc_Get_HsePllConfig    ( rcc_ConfigStruct_t * const config );
static void         It_Rcc_Check_BusClocks     ( rcc_FreqHz_t ahbClk, rcc_FreqHz_t apb1Clk, rcc_FreqHz_t apb2Clk );
static void         It_Rcc_Check_HclkMeasured  ( rcc_FreqHz_t expectedHz, uint32_t tolPermille );
static rcc_FreqHz_t It_Rcc_Measure_HclkHz      ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/*----------------------------- Board configuration --------------------------*/
/* Boards are named by their MCU (IT_BOARD_<MCU>, name of the board from the detection), MCUs
 * with the same DEV_ID and flash size share the block. Board MB1367:
 * NUCLEO-G474RE - DEV_ID 0x469, 512 KB flash (STM32G471xE / G473xE / G474xE / G483xE / G484xE)
 * NUCLEO-G431RB - DEV_ID 0x468, 128 KB flash (STM32G411xB / G431xB / G441xB) */
#if defined(IT_BOARD_STM32G471xE) || \
    defined(IT_BOARD_STM32G473xE) || \
    defined(IT_BOARD_STM32G474xE) || \
    defined(IT_BOARD_STM32G483xE) || \
    defined(IT_BOARD_STM32G484xE) || \
    defined(IT_BOARD_STM32G411xB) || \
    defined(IT_BOARD_STM32G431xB) || \
    defined(IT_BOARD_STM32G441xB)

    /** HSE - 24 MHz crystal X3 */
    #define IT_RCC_HSE_TYPE                 ( RCC_HSE_TYPE_CRYSTAL )
    #define IT_RCC_HSE_FREQ_HZ              ( 24000000u )

    /** PLL from HSE: 24 MHz / 6 * 85 = 340 MHz VCO, R 2 = 170 MHz, P 10 = 34 MHz, Q 4 = 85 MHz */
    #define IT_RCC_HSE_PLL_M                ( 6u )
    #define IT_RCC_HSE_PLL_N                ( 85u )
    #define IT_RCC_HSE_PLL_P                ( 10u )
    #define IT_RCC_HSE_PLL_Q                ( 4u )
    #define IT_RCC_HSE_PLL_R                ( 2u )
    #define IT_RCC_HSE_PLL_VCO_HZ           ( 340000000u )

    /** PLL from HSE in range 1 normal mode: 24 MHz / 6 * 75 / 2 = 150 MHz */
    #define IT_RCC_HSE_PLL_NORMAL_N         ( 75u )
    #define IT_RCC_HSE_PLL_NORMAL_SYSCLK_HZ ( 150000000u )

    /** LSE 32.768 kHz crystal X2 (fitted on the board - LSE starts) */
    #define IT_RCC_LSE_FITTED               ( 1u )

    /** Morpho CN10 pin 2 (PC8) - not connected on the board */
    #define IT_RCC_FREE_PORT                ( GPIO_PORT_C )
    #define IT_RCC_FREE_PIN                 ( GPIO_PIN_ID_8 )
    #define IT_RCC_FREE_PERIPH              ( RCC_PERIPH_GPIOC )

#else
    #error "Board of Rcc integration tests is not defined (INTEGRATION_TEST_BOARD)."
#endif

/** System clock of HSE PLL configuration (output R) */
#define IT_RCC_HSE_PLL_SYSCLK_HZ            ( IT_RCC_HSE_PLL_VCO_HZ / IT_RCC_HSE_PLL_R )

/** System clock of default configuration (StartUp) - HSI 16 MHz / 4 * 85 / 2 */
#define IT_RCC_DEFAULT_SYSCLK_HZ            ( 170000000u )

/** PLL VCO frequency of default configuration */
#define IT_RCC_DEFAULT_VCO_HZ               ( 340000000u )

/** PLL output P divider of default configuration (ADC kernel clock) */
#define IT_RCC_DEFAULT_PLL_P                ( 6u )

/** HSI16 frequency */
#define IT_RCC_HSI_FREQ_HZ                  ( 16000000u )

/** HSI48 frequency (RNG / USB kernel clock of default configuration) */
#define IT_RCC_HSI48_FREQ_HZ                ( 48000000u )

/** Count of milliseconds in one second */
#define IT_RCC_MS_IN_SECOND                 ( 1000u )

/** Longest SysTick interval at default HCLK (24-bit reload register) */
#define IT_RCC_SYSTICK_MAX_MS_DEFAULT       ( 0x01000000u / ( IT_RCC_DEFAULT_SYSCLK_HZ / IT_RCC_MS_IN_SECOND ) )

/** Count of LSE periods of HCLK measurement (~ 100 ms) */
#define IT_RCC_LSE_PERIODS                  ( 3277u )

/** Tolerance of HCLK measurement for clocks derived from HSI16 [per mille] */
#define IT_RCC_HSI_TOL_PERMILLE             ( 20u )

/** Tolerance of HCLK measurement for clocks derived from HSE crystal [per mille] */
#define IT_RCC_HSE_TOL_PERMILLE             ( 2u )

/** Maximal count of polling loop iterations of HCLK measurement (timeout ~ seconds) */
#define IT_RCC_WAIT_LOOPS                   ( 20000000u )

/* ============================== MACROS ==================================== */

/* ========================== LOCAL VARIABLES =============================== */

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    /* Default clock configuration is set by StartUp after every reset */
}


void tearDown( void )
{
    /* LSCO and LSE are in backup domain - not reset by system reset. Clock
     * output can not be switched off by public API (source NONE keeps the
     * output unchanged) - LSCO is disabled by LL directly. */
    LL_PWR_EnableBkUpAccess();
    LL_RCC_LSCO_Disable();

    (void)Rcc_Set_OscInactive( RCC_OSC_LSE );
}

/* =============================== TESTS ==================================== */

/*----------------------------- Default clock --------------------------------*/

/**
 * \brief   Default configuration of StartUp runs from HSI PLL at 170 MHz.
 *
 * \details Reads state of the clock tree after StartUp.
 *
 * \par Expected results
 * - PLL ACTIVE with HSI source, HSI ACTIVE, system clock source PLL.
 * - AHB, APB1 and APB2 170 MHz, SysTick interval 1 ms.
 * - Measured HCLK 170 MHz +- 2 % (LSE reference).
 */
void It_Rcc_StartUp_DefaultConfig_Pll170MHzActive( void )
{
    rcc_FunctionState_t state     = RCC_FUNCTION_INACTIVE;
    rcc_PllClkSrc_t     pllSource = RCC_PLL_SRC_NONE;
    rcc_Time_ms_t       interval  = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, pllSource );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );

    It_Rcc_Check_HclkMeasured( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_HSI_TOL_PERMILLE );
}


/**
 * \brief   Kernel clocks of default configuration.
 *
 * \par Expected results
 * - ADC12 asynchronous clock 56.67 MHz (PLL P 340 / 6), RNG 48 MHz (HSI48 -
 *   reset value of CLK48SEL), IWDG 32 kHz (LSI).
 * - APB1 timer TIM2 and APB2 timer TIM1 170 MHz (PCLK, APB not divided),
 *   USART1 with PCLK2 170 MHz.
 */
void It_Rcc_Get_PeriphClk_DefaultConfig_KernelClocks( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_PLLP, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_VCO_HZ / IT_RCC_DEFAULT_PLL_P, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_HSI48, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSI48_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_IWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_PCLK2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
}

/*----------------------------- Initialization -------------------------------*/

/**
 * \brief   Rcc_Init() switches running clock tree to HSE PLL at 170 MHz with CSS.
 *
 * \details HSE of the board, PLL to 170 MHz (range 1 boost), AHB / 1, APB1 / 2,
 *          APB2 / 2, clock security system enabled. Running PLL (system clock)
 *          is reconfigured - the module switches to HSI, reconfigures the PLL
 *          and switches back through AHB / 2 transition state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL source HSE, PLL ACTIVE (HSE started, PLL locked).
 * - AHB 170 MHz, APB1 / APB2 85 MHz, TIM2 / TIM1 170 MHz (2 x PCLK).
 * - ADC12 34 MHz (P), FDCAN 85 MHz (Q), SysTick interval 1 ms.
 * - Measured HCLK 170 MHz +- 0.2 % (LSE reference).
 * - No fault (flash latency and voltage range sufficient, no CSS NMI).
 */
void It_Rcc_Init_HsePllWithCss_170MHzBoost( void )
{
    rcc_ConfigStruct_t  config;
    rcc_PllClkSrc_t     pllSource = RCC_PLL_SRC_NONE;
    rcc_FunctionState_t state     = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t        freq      = 0u;
    rcc_Time_ms_t       interval  = 0u;

    It_Rcc_Get_HsePllConfig( &config );
    config.CSS_Enable = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSE, pllSource );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_PLL_SYSCLK_HZ, IT_RCC_HSE_PLL_SYSCLK_HZ / 2u, IT_RCC_HSE_PLL_SYSCLK_HZ / 2u );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_PLLP, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_VCO_HZ / IT_RCC_HSE_PLL_P, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_FDCAN_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_VCO_HZ / IT_RCC_HSE_PLL_Q, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );

    It_Rcc_Check_HclkMeasured( IT_RCC_HSE_PLL_SYSCLK_HZ, IT_RCC_HSE_TOL_PERMILLE );
}


/**
 * \brief   Rcc_Init() runs PLL at 150 MHz in voltage range 1 normal mode.
 *
 * \details HSE PLL configuration with N 75 (150 MHz), voltage scale 1 (range 1
 *          normal mode, max. 150 MHz) - boost mode of StartUp is switched off.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, AHB 150 MHz, APB1 / APB2 75 MHz.
 * - Measured HCLK 150 MHz +- 0.2 % (LSE reference).
 * - No fault (flash latency of normal mode sufficient).
 */
void It_Rcc_Init_HsePllRange1Normal_150MHz( void )
{
    rcc_ConfigStruct_t config;

    It_Rcc_Get_HsePllConfig( &config );
    config.VoltageScaling                       = RCC_PWR_VOLTAGE_SCALE_1;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = IT_RCC_HSE_PLL_NORMAL_N;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_PLL_NORMAL_SYSCLK_HZ, IT_RCC_HSE_PLL_NORMAL_SYSCLK_HZ / 2u, IT_RCC_HSE_PLL_NORMAL_SYSCLK_HZ / 2u );

    It_Rcc_Check_HclkMeasured( IT_RCC_HSE_PLL_NORMAL_SYSCLK_HZ, IT_RCC_HSE_TOL_PERMILLE );
}


/**
 * \brief   Rcc_Init() with HSI system clock switches PLL off.
 *
 * \details HSI system clock, PLL not used.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL INACTIVE.
 * - AHB, APB1 and APB2 16 MHz, measured HCLK 16 MHz +- 2 % (LSE reference).
 */
void It_Rcc_Init_HsiSysClk_PllInactive( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource                  = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_HSI_FREQ_HZ, IT_RCC_HSI_FREQ_HZ, IT_RCC_HSI_FREQ_HZ );

    It_Rcc_Check_HclkMeasured( IT_RCC_HSI_FREQ_HZ, IT_RCC_HSI_TOL_PERMILLE );
}


/**
 * \brief   Rcc_Init() with HSE used directly as system clock in voltage range 2.
 *
 * \details HSE of the board (24 MHz) as system clock, PLL not used, voltage
 *          scale 2 (range 2, max. 26 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL INACTIVE, AHB / APB1 / APB2 clock = HSE frequency.
 * - SysTick interval 1 ms, measured HCLK 24 MHz +- 0.2 % (LSE reference).
 * - No fault (range 2 flash latency sufficient).
 */
void It_Rcc_Init_HseSysClkRange2_HseFrequency( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state    = RCC_FUNCTION_ACTIVE;
    rcc_Time_ms_t       interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType                      = IT_RCC_HSE_TYPE;
    config.HSE_Frequency_Hz                   = IT_RCC_HSE_FREQ_HZ;
    config.SystemClockSource                  = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.VoltageScaling                     = RCC_PWR_VOLTAGE_SCALE_2;
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_FREQ_HZ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );

    It_Rcc_Check_HclkMeasured( IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_TOL_PERMILLE );
}


/**
 * \brief   Rcc_Init() rejects frequency above limit of voltage range.
 *
 * \details HSE PLL configuration of 170 MHz with voltage scale 1 (range 1
 *          normal mode, max. 150 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system keeps running from HSI (AHB 16 MHz, measured
 *   HCLK 16 MHz +- 2 %).
 */
void It_Rcc_Init_SysClkOverRangeLimit_KeepsHsi( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       ahbClk = 0u;

    It_Rcc_Get_HsePllConfig( &config );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSI_FREQ_HZ, ahbClk );

    It_Rcc_Check_HclkMeasured( IT_RCC_HSI_FREQ_HZ, IT_RCC_HSI_TOL_PERMILLE );
}


/**
 * \brief   Rcc_Init() reconfigures clock tree initialized by previous Rcc_Init().
 *
 * \details HSE PLL configuration, then default configuration again.
 *
 * \par Expected results
 * - Both calls RCC_REQUEST_OK, PLL source HSI, AHB / APB1 / APB2 170 MHz.
 * - Measured HCLK 170 MHz +- 2 % (LSE reference).
 */
void It_Rcc_Init_Reinit_ReturnsToDefaultClock( void )
{
    rcc_ConfigStruct_t config;
    rcc_PllClkSrc_t    pllSource = RCC_PLL_SRC_NONE;

    It_Rcc_Get_HsePllConfig( &config );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, pllSource );

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ );

    It_Rcc_Check_HclkMeasured( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_HSI_TOL_PERMILLE );
}

/*--------------------- Hardware protection of clocks in use -----------------*/

/**
 * \brief   HSI can not be switched off while it clocks the system.
 *
 * \details Default configuration - HSI is source of the PLL (system clock),
 *          HSION is kept set by hardware.
 *
 * \par Expected results
 * - Rcc_Set_OscInactive() returns RCC_REQUEST_ERROR, HSI stays ACTIVE.
 */
void It_Rcc_Set_OscInactive_HsiInUse_KeptRunning( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_HSI ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    /* Enable bit cleared by the request is restored */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_HSI ) );
}


/**
 * \brief   PLL can not be switched off while it is the system clock.
 *
 * \par Expected results
 * - Rcc_Set_PllInactive() returns RCC_REQUEST_ERROR, PLL keeps locked,
 *   AHB clock stays 170 MHz.
 */
void It_Rcc_Set_PllInactive_PllIsSysClk_KeptRunning( void )
{
    rcc_FreqHz_t ahbClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_1 ) );

    /* Enable bit cleared by the request is restored */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_1 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, ahbClk );
}


/**
 * \brief   Running system clock PLL is not reconfigured by Rcc_Set_PllConfig().
 *
 * \details Default configuration - PLL is the system clock. New configuration
 *          (N 80) is requested directly by Rcc_Set_PllConfig().
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR (PLL can not be stopped), PLL ACTIVE.
 * - VCO stays 340 MHz (PLLCFGR not written), AHB clock stays 170 MHz.
 */
void It_Rcc_Set_PllConfig_PllIsSysClk_Rejected( void )
{
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 4u, .N_Multiplier = 80u,
        .P_Divider  = 0u,              .Q_Divider = 0u, .R_Divider    = 2u
    };
    rcc_FunctionState_t   state = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t          freq  = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    /* Enable bit cleared by the request is restored */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_1 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_VCO_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
}

/*-------------------------------- Oscillators -------------------------------*/

/**
 * \brief   LSI is started and stopped.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, state ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_OscActive_Lsi_StartsAndStops( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   HSI48 is started and stopped.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, state ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_OscActive_Hsi48_StartsAndStops( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_HSI48 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI48, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_HSI48 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI48, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   LSE start matches the crystal of the board.
 *
 * \details LSE is activated (backup domain write protection released by the
 *          module), switched off by tearDown().
 *
 * \par Expected results
 * - Crystal fitted: RCC_REQUEST_OK, state ACTIVE.
 * - Crystal not fitted: RCC_REQUEST_ERROR (start-up timeout), state INACTIVE.
 */
void It_Rcc_Set_OscActive_Lse_MatchesBoard( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

#if ( 0u != IT_RCC_LSE_FITTED )
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( RCC_OSC_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
#endif /* IT_RCC_LSE_FITTED */
}

/*------------------------------------- PLL ----------------------------------*/

/**
 * \brief   PLL is configured, locks and stops while it is not the system clock.
 *
 * \details System clock is switched to HSI by Rcc_Init() (PLL not used), then
 *          PLL HSI, M 4, N 80, Q 4, R 2 is configured.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL ACTIVE, output R 160 MHz, output Q 80 MHz, AHB stays
 *   16 MHz (PLL is not the system clock).
 * - Deactivation RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_PllConfig_PllNotSysClk_LocksAndStops( void )
{
    rcc_ConfigStruct_t    config;
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 4u, .N_Multiplier = 80u,
        .P_Divider  = 0u,              .Q_Divider = 4u, .R_Divider    = 2u
    };
    rcc_FunctionState_t   state = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t          freq  = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource                  = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 160000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 80000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSI_FREQ_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/*---------------------------- Peripheral control ----------------------------*/

/**
 * \brief   Peripheral clock, reset and sleep control of real registers.
 *
 * \details TIM2 clock is activated / deactivated, reset is activated /
 *          deactivated, sleep mode clock is deactivated / activated.
 *
 * \par Expected results
 * - Every request RCC_REQUEST_OK, states read back as requested.
 */
void It_Rcc_Set_PeriphActive_Tim2_StatesFollowRequests( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_TIM2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_TIM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( RCC_PERIPH_TIM2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_TIM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_TIM2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_TIM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_TIM2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_TIM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_TIM2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_TIM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_TIM2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_TIM2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Peripheral reset returns peripheral registers to reset values.
 *
 * \details Free pin is switched to output mode, then its GPIO port is reset
 *          by RCC (clock stays enabled).
 *
 * \par Expected results
 * - Before reset: pin mode OUTPUT.
 * - After reset: pin mode ANALOG (reset value of MODER on STM32G4), port clock
 *   ACTIVE.
 */
void It_Rcc_Set_ResetActive_GpioPort_RegistersReset( void )
{
    gpio_PinMode_t      pinMode = GPIO_PIN_MODE_INPUT;
    rcc_FunctionState_t state   = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Set_PortActive( IT_RCC_FREE_PORT ) );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Set_PinMode( IT_RCC_FREE_PORT, IT_RCC_FREE_PIN, GPIO_PIN_MODE_OUTPUT ) );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( IT_RCC_FREE_PORT, IT_RCC_FREE_PIN, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_OUTPUT, pinMode );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( IT_RCC_FREE_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( IT_RCC_FREE_PERIPH ) );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( IT_RCC_FREE_PORT, IT_RCC_FREE_PIN, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ANALOG, pinMode );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( IT_RCC_FREE_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}

/*------------------------------- Clock outputs ------------------------------*/

/**
 * \brief   MCO outputs HSI / 4 on PA8.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source HSI and divider 4 read back.
 * - PA8 in alternate function mode, alternate function 0.
 */
void It_Rcc_Set_ClkOutSource_McoHsi_PinConfigured( void )
{
    rcc_ClkOut_Source_t source  = RCC_CLK_SOURCE_NONE;
    rcc_ClkOut_Div_t    divider = 0u;
    gpio_PinMode_t      pinMode = GPIO_PIN_MODE_INPUT;
    gpio_AltFunction_t  altFunc = GPIO_ALT_FUNC_15;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 4u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO1_HSI, source );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, divider );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( GPIO_PORT_A, GPIO_PIN_ID_8, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ALTERNATE, pinMode );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinAltFunction( GPIO_PORT_A, GPIO_PIN_ID_8, &altFunc ) );
    TEST_ASSERT_EQUAL( GPIO_ALT_FUNC_0, altFunc );
}


/**
 * \brief   LSCO outputs LSI on PA2.
 *
 * \details LSI is started, LSCO source LSI is selected (backup domain write
 *          protection released by the module). LSCO is switched off by
 *          tearDown().
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source LSI read back (LSCOEN and LSCOSEL written to the
 *   backup domain), divider 1.
 * - PA2 in analog mode.
 */
void It_Rcc_Set_ClkOutSource_LscoLsi_PinConfigured( void )
{
    rcc_ClkOut_Source_t source  = RCC_CLK_SOURCE_NONE;
    rcc_ClkOut_Div_t    divider = 0u;
    gpio_PinMode_t      pinMode = GPIO_PIN_MODE_INPUT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_LSCO_LSI ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_LSCO_LSI, source );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_LSCO, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, divider );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( GPIO_PORT_A, GPIO_PIN_ID_2, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ANALOG, pinMode );
}

/*---------------------------------- SysTick ---------------------------------*/

/**
 * \brief   SysTick interval is limited by 24-bit reload at real HCLK.
 *
 * \details HCLK 170 MHz - longest interval is 98 ms.
 *
 * \par Expected results
 * - 98 ms: RCC_REQUEST_OK, interval read back.
 * - 99 ms: RCC_REQUEST_ERROR, previous interval kept.
 */
void It_Rcc_Set_SysTickInterval_MaxAtDefaultClock( void )
{
    rcc_Time_ms_t interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( IT_RCC_SYSTICK_MAX_MS_DEFAULT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_SYSTICK_MAX_MS_DEFAULT, interval );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( IT_RCC_SYSTICK_MAX_MS_DEFAULT + 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_SYSTICK_MAX_MS_DEFAULT, interval );
}

/*------------------------------ Reset source --------------------------------*/

/**
 * \brief   Software reset is reported by reset source flags.
 *
 * \details Stage 0: reset flags are cleared and checked, software system reset
 *          is requested. Stage 1 (after the reset): reset flags are read.
 *
 * \par Expected results
 * - Stage 0: all flags INACTIVE after clear.
 * - Stage 1: software reset flag ACTIVE, brown-out, watchdog, low-power and
 *   option byte loader flags INACTIVE.
 */
void It_Rcc_Get_ResetSource_SoftwareReset_SwFlagActive( void )
{
    rcc_FlagState_t flagState = RCC_FLAG_ACTIVE;

    if( 0u == IntegrationTesting_Get_Stage() )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

        for( uint32_t srcIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > srcIdx; srcIdx++ )
        {
            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( (rcc_ResetSrc_t)srcIdx, &flagState ) );
            TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
        }

        IntegrationTesting_Set_ResetExpected();
        Nvic_Set_SystemReset();

        TEST_FAIL_MESSAGE( "Reset did not occur" );
    }
    else
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_SW, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flagState );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_BOR, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_IWDG, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_WWDG, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_LPWR, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_OBL, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
    }
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Configuration of 170 MHz from HSE PLL of the board.
 *
 * AHB / 1, APB1 / 2, APB2 / 2, voltage range 1 boost mode (default), CSS off.
 *
 * \param config [out]: Clock configuration
 */
static void It_Rcc_Get_HsePllConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->HSE_ClockType    = IT_RCC_HSE_TYPE;
    config->HSE_Frequency_Hz = IT_RCC_HSE_FREQ_HZ;
    config->APB1_Divider     = RCC_APB1_DIVIDER_2;
    config->APB2_Divider     = RCC_APB2_DIVIDER_2;

    config->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSE;
    config->Pll_Config[ RCC_PLL_1 ].M_Divider    = IT_RCC_HSE_PLL_M;
    config->Pll_Config[ RCC_PLL_1 ].N_Multiplier = IT_RCC_HSE_PLL_N;
    config->Pll_Config[ RCC_PLL_1 ].P_Divider    = IT_RCC_HSE_PLL_P;
    config->Pll_Config[ RCC_PLL_1 ].Q_Divider    = IT_RCC_HSE_PLL_Q;
    config->Pll_Config[ RCC_PLL_1 ].R_Divider    = IT_RCC_HSE_PLL_R;
}


/**
 * \brief Checks frequencies of AHB, APB1 and APB2 buses.
 *
 * \param ahbClk  [in]: Expected AHB clock in Hz
 * \param apb1Clk [in]: Expected APB1 clock in Hz
 * \param apb2Clk [in]: Expected APB2 clock in Hz
 */
static void It_Rcc_Check_BusClocks( rcc_FreqHz_t ahbClk, rcc_FreqHz_t apb1Clk, rcc_FreqHz_t apb2Clk )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( ahbClk, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( apb1Clk, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( apb2Clk, freq );
}


/**
 * \brief Checks real HCLK frequency measured against LSE reference.
 *
 * \param expectedHz  [in]: Expected HCLK frequency in Hz
 * \param tolPermille [in]: Tolerance of the measurement in per mille of the expected frequency
 */
static void It_Rcc_Check_HclkMeasured( rcc_FreqHz_t expectedHz, uint32_t tolPermille )
{
    const rcc_FreqHz_t tolHz = ( expectedHz / 1000u ) * tolPermille;

    TEST_ASSERT_UINT32_WITHIN_MESSAGE( tolHz, expectedHz, It_Rcc_Measure_HclkHz(), "Measured HCLK frequency" );
}


/**
 * \brief Measures real HCLK frequency against LSE crystal reference.
 *
 * LSE is started and routed to MCO pin (PA8, divider 1). The pin is polled by
 * the input data register (single load - the poll period is much shorter than
 * a half period of LSE even at 16 MHz), DWT cycle counter is captured at the
 * first rising edge and after \ref IT_RCC_LSE_PERIODS periods.
 *
 * \return Measured HCLK frequency in Hz
 */
static rcc_FreqHz_t It_Rcc_Measure_HclkHz( void )
{
    uint32_t risingEdges = 0u;
    uint32_t prevLevel   = GPIO_IDR_ID8;    /* Measurement starts by a rising edge */
    uint32_t startCycles = 0u;
    uint32_t endCycles   = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_LSE ) );

    /* DWT cycle counter counts processor clock (HCLK) cycles */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0u;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    for( uint32_t loopIdx = 0u;
         ( IT_RCC_WAIT_LOOPS  >  loopIdx     ) &&
         ( IT_RCC_LSE_PERIODS >= risingEdges );
         loopIdx++ )
    {
        const uint32_t pinLevel = GPIOA->IDR & GPIO_IDR_ID8;

        if( ( 0u == prevLevel ) &&
            ( 0u != pinLevel  )    )
        {
            if( 0u == risingEdges )
            {
                startCycles = DWT->CYCCNT;
            }
            else
            {
                endCycles = DWT->CYCCNT;
            }

            risingEdges++;
        }

        prevLevel = pinLevel;
    }

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE( IT_RCC_LSE_PERIODS, risingEdges, "LSE clock not present on MCO pin" );

    /* Unsigned difference is valid across one counter overflow (25 s at 170 MHz) */
    return ( (rcc_FreqHz_t)( ( (uint64_t)( endCycles - startCycles ) * LSE_VALUE ) / IT_RCC_LSE_PERIODS ) );
}
