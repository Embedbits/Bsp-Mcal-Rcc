/**
 * \author Mr.Nobody
 * \file ItTest_Rcc.c
 * \ingroup Rcc
 * \brief Integration tests of Reset and Clock Control (RCC) module on target (STM32H7).
 *
 * Rcc module runs on the MCU together with real hardware. Tests verify behavior
 * which cannot be verified by unit tests (emulated registers): oscillator start
 * and PLL lock, system clock switching and reconfiguration of running clock
 * tree, hardware protection of clocks in use, peripheral reset, real reset
 * source flags and clock output pins.
 *
 * Every test case starts after system reset with default clock configuration
 * set by StartUp (system clock HSI 64 MHz, all PLLs stopped, all bus prescalers 1,
 * voltage scale 3, SysTick interval 1 ms).
 *
 * The tests stay in the voltage scale 3 (reset value) which is valid for every
 * STM32H7 line without the supply configuration of the board: the AXI clock (HCLK)
 * is limited to 85 MHz (STM32H72x / H73x) and 88 MHz (STM32H7A3 / H7B0 / H7B3).
 * The PLL configuration of the tests (80 MHz) fits all lines, the configuration
 * above the limit (240 MHz) is refused by the module on all lines.
 *
 * Boards (named by the MCU as the detection of the connected boards does):
 * - STM32H723xG - NUCLEO-H723ZG (the detection names every board with ID 0x483
 *   and 1 MB flash STM32H723xG)
 * - STM32H7A3xI / STM32H7A3xIQ - NUCLEO-H7A3ZI-Q (the detection names every
 *   board with ID 0x480 and 2 MB flash STM32H7A3xI, STM32H7A3xIQ by a board file
 *   override)
 * Both are Nucleo-144 boards with HSE 8 MHz MCO of the ST-LINK (digital bypass).
 *
 * Used resources (see board configuration below):
 * - IT_RCC_HSE_*      - HSE oscillator of the board
 * - IT_RCC_FREE_*     - pin not used by the tests on the board (PE7 - free on
 *                       Nucleo-144), used by peripheral reset test
 * - PA8 (MCO1), PC9 (MCO2) - clock outputs, the pins output clock during the
 *                       test case (PA8 free / Arduino D10, PC9 free on Nucleo-144)
 *
 * \note RTC clock selection (RTCSEL) is write-once until backup domain reset,
 *       which is not available by public API - it is tested by unit tests only.
 *       LSE is not an oscillator of the module interface of STM32H7.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Gpio_Port.h"                      /* Pins of clock outputs          */
#include "Nvic_Port.h"                      /* System reset                   */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void It_Rcc_Get_HsePllConfig ( rcc_ConfigStruct_t * const config );
static void It_Rcc_Check_BusClocks  ( rcc_FreqHz_t ahbClk, rcc_FreqHz_t apb1Clk, rcc_FreqHz_t apb2Clk );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/*----------------------------- Board configuration --------------------------*/
#if defined(IT_BOARD_STM32H723xG)  || \
    defined(IT_BOARD_STM32H7A3xI)  || \
    defined(IT_BOARD_STM32H7A3xIQ)

    /* NUCLEO-H723ZG / NUCLEO-H7A3ZI-Q (Nucleo-144) */

    /** HSE - 8 MHz MCO of the ST-LINK (oscillator bypassed by a digital signal, X3 not fitted) */
    #define IT_RCC_HSE_TYPE                 ( RCC_HSE_TYPE_SIG_DIGITAL_IN )
    #define IT_RCC_HSE_FREQ_HZ              ( 8000000u )

    /** PE7 - not connected on the board */
    #define IT_RCC_FREE_PORT                ( GPIO_PORT_E )
    #define IT_RCC_FREE_PIN                 ( GPIO_PIN_ID_7 )
    #define IT_RCC_FREE_PERIPH              ( RCC_PERIPH_GPIOE )

#else
    #error "Board of Rcc integration tests is not defined (INTEGRATION_TEST_BOARD)."
#endif

/** Main PLL (PLL1) from HSE: 8 MHz / 2 * 40 = 160 MHz VCO, P 2 = 80 MHz (system clock), Q 4 = 40 MHz */
#define IT_RCC_HSE_PLL_M                    ( 2u )
#define IT_RCC_HSE_PLL_N                    ( 40u )
#define IT_RCC_HSE_PLL_P                    ( 2u )
#define IT_RCC_HSE_PLL_Q                    ( 4u )
#define IT_RCC_HSE_PLL_SYSCLK_HZ            ( 80000000u )
#define IT_RCC_HSE_PLL_Q_HZ                 ( 40000000u )

/** Main PLL above the limit of the voltage scale 3: 8 MHz / 2 * 120 = 480 MHz VCO, P 2 = 240 MHz */
#define IT_RCC_OVER_PLL_N                   ( 120u )

/** System clock of default configuration (StartUp) = HSI */
#define IT_RCC_DEFAULT_SYSCLK_HZ            ( 64000000u )

/** LSI frequency */
#define IT_RCC_LSI_FREQ_HZ                  ( 32000u )

/** Count of milliseconds in one second */
#define IT_RCC_MS_IN_SECOND                 ( 1000u )

/** Longest SysTick interval at default HCLK (24-bit reload register) */
#define IT_RCC_SYSTICK_MAX_MS_DEFAULT       ( 0x01000000u / ( IT_RCC_DEFAULT_SYSCLK_HZ / IT_RCC_MS_IN_SECOND ) )

/* ============================== MACROS ==================================== */

/* ========================== LOCAL VARIABLES =============================== */

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    /* Default clock configuration is set by StartUp after every reset */
}


void tearDown( void )
{
    /* Every test case runs after system reset */
}

/* =============================== TESTS ==================================== */

/*----------------------------- Default clock --------------------------------*/

/**
 * \brief   Default configuration of StartUp runs from HSI at 64 MHz.
 *
 * \details Reads state of the clock tree after StartUp.
 *
 * \par Expected results
 * - HSI ACTIVE, all PLLs INACTIVE.
 * - AHB, APB1 and APB2 64 MHz, SysTick interval 1 ms.
 */
void It_Rcc_StartUp_DefaultConfig_Hsi64MHzActive( void )
{
    rcc_FunctionState_t state    = RCC_FUNCTION_INACTIVE;
    rcc_Time_ms_t       interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI64, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    for( uint32_t pllId = 0u; (uint32_t)RCC_PLL_CNT > pllId; pllId++ )
    {
        state = RCC_FUNCTION_ACTIVE;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( (rcc_PllId_t)pllId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Kernel clocks of default configuration.
 *
 * \par Expected results
 * - IWDG 32 kHz (LSI), SysTick (CPU clock) 64 MHz.
 * - APB1 timer TIM2 and APB2 timer TIM1 64 MHz (APB prescalers 1).
 */
void It_Rcc_Get_PeriphClk_DefaultConfig_KernelClocks( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_IWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_LSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SYSTICK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
}

/*----------------------------- Initialization -------------------------------*/

/**
 * \brief   Rcc_Init() switches running clock tree to HSE PLL with CSS.
 *
 * \details HSE of the board, PLL1 to 80 MHz, AHB / 1, APB1 / 2, APB2 / 2, clock
 *          security system enabled.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL source HSE, PLL1 ACTIVE (HSE started, PLL locked), output Q 40 MHz.
 * - AHB 80 MHz, APB1 and APB2 40 MHz, timers TIM2 / TIM1 80 MHz (2 x PCLK),
 *   SysTick interval 1 ms.
 * - No fault (flash latency and AXI clock of the voltage scale 3, no CSS NMI).
 */
void It_Rcc_Init_HsePllWithCss_Frequency( void )
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_Q_HZ, freq );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_PLL_SYSCLK_HZ, IT_RCC_HSE_PLL_SYSCLK_HZ / 2u, IT_RCC_HSE_PLL_SYSCLK_HZ / 2u );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Init() with HSI system clock switches all PLLs off.
 *
 * \details HSE PLL configuration first (PLL1 running), then HSI system clock with PLLs not used.
 *
 * \par Expected results
 * - Both calls RCC_REQUEST_OK, all PLLs INACTIVE after the second one.
 * - AHB, APB1 and APB2 64 MHz.
 */
void It_Rcc_Init_HsiSysClk_PllsInactive( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    It_Rcc_Get_HsePllConfig( &config );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource                    = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_NONE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    for( uint32_t pllId = 0u; (uint32_t)RCC_PLL_CNT > pllId; pllId++ )
    {
        state = RCC_FUNCTION_ACTIVE;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( (rcc_PllId_t)pllId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ );
}


/**
 * \brief   Rcc_Init() with HSE used directly as system clock.
 *
 * \details HSE of the board as system clock, PLLs not used, APB1 / 1, APB2 / 1.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL1 INACTIVE, AHB / APB1 / APB2 clock = HSE frequency.
 */
void It_Rcc_Init_HseSysClk_HseFrequency( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType                        = IT_RCC_HSE_TYPE;
    config.HSE_Frequency_Hz                     = IT_RCC_HSE_FREQ_HZ;
    config.SystemClockSource                    = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_FREQ_HZ );
}


/**
 * \brief   Rcc_Init() rejects frequency above limit of voltage scale.
 *
 * \details HSE PLL configuration of 240 MHz with the voltage scale 3 (AXI clock limit
 *          224 MHz STM32H74x / H75x, 85 MHz STM32H72x / H73x, 88 MHz STM32H7A3 / H7B0 / H7B3).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system keeps running from HSI (AHB 64 MHz).
 */
void It_Rcc_Init_SysClkOverScaleLimit_KeepsHsi( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       ahbClk = 0u;

    It_Rcc_Get_HsePllConfig( &config );
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = IT_RCC_OVER_PLL_N;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, ahbClk );
}


/**
 * \brief   Rcc_Init() reconfigures clock tree initialized by previous Rcc_Init().
 *
 * \details HSE PLL configuration, then default configuration again.
 *
 * \par Expected results
 * - Both calls RCC_REQUEST_OK, PLL1 INACTIVE, AHB, APB1 and APB2 64 MHz.
 */
void It_Rcc_Init_Reinit_ReturnsToDefaultClock( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    It_Rcc_Get_HsePllConfig( &config );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ );
}

/*--------------------- Hardware protection of clocks in use -----------------*/

/**
 * \brief   HSI can not be switched off while it clocks the system.
 *
 * \details Default configuration - HSI is the system clock.
 *
 * \par Expected results
 * - Rcc_Set_OscInactive() returns RCC_REQUEST_ERROR, HSI stays ACTIVE.
 */
void It_Rcc_Set_OscInactive_HsiInUse_KeptRunning( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_HSI64 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI64, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   PLL1 can not be switched off while it is the system clock.
 *
 * \details HSE PLL configuration (PLL1 is the system clock).
 *
 * \par Expected results
 * - Rcc_Set_PllInactive() returns RCC_REQUEST_ERROR, PLL keeps locked,
 *   AHB clock stays 80 MHz.
 */
void It_Rcc_Set_PllInactive_PllIsSysClk_KeptRunning( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t        ahbClk = 0u;

    It_Rcc_Get_HsePllConfig( &config );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_1 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ, ahbClk );
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
 * \brief   HSI48 and CSI are started and stopped.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, state ACTIVE (oscillator ready).
 * - Deactivation: RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_OscActive_Hsi48AndCsi_StartAndStop( void )
{
    const rcc_OscId_t   oscillators[] = { RCC_OSC_HSI48, RCC_OSC_CSI };
    rcc_FunctionState_t state         = RCC_FUNCTION_INACTIVE;

    for( uint32_t oscIdx = 0u; ( sizeof( oscillators ) / sizeof( oscillators[ 0u ] ) ) > oscIdx; oscIdx++ )
    {
        state = RCC_FUNCTION_INACTIVE;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( oscillators[ oscIdx ] ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscillators[ oscIdx ], &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( oscillators[ oscIdx ] ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscillators[ oscIdx ], &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}

/*------------------------------------ PLLs ----------------------------------*/

/**
 * \brief   PLL2 locks from HSI and stops.
 *
 * \details PLL2 from HSI 64 MHz: M 4, N 25 (VCO 400 MHz), P 2, Q 4, R 8.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL2 ACTIVE, outputs P 200 MHz, Q 100 MHz, R 50 MHz.
 * - Deactivation RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_PllConfig_Pll2_LocksAndStops( void )
{
    rcc_PllConfigStruct_t pllConfig;
    rcc_FunctionState_t   state = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t          freq  = 0u;

    pllConfig.Pll_Source   = RCC_PLL_SRC_HSI;
    pllConfig.M_Divider    = 4u;
    pllConfig.N_Multiplier = 25u;
    pllConfig.P_Divider    = 2u;
    pllConfig.Q_Divider    = 4u;
    pllConfig.R_Divider    = 8u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 200000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 100000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 50000000u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   PLL3 locks from HSI and stops.
 *
 * \details PLL3 from HSI 64 MHz: M 8 (reference 8 MHz), N 50 (VCO 400 MHz), R 4.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL3 ACTIVE, output R 100 MHz.
 * - Deactivation RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_PllConfig_Pll3_LocksAndStops( void )
{
    rcc_PllConfigStruct_t pllConfig;
    rcc_FunctionState_t   state = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t          freq  = 0u;

    pllConfig.Pll_Source   = RCC_PLL_SRC_HSI;
    pllConfig.M_Divider    = 8u;
    pllConfig.N_Multiplier = 50u;
    pllConfig.P_Divider    = 0u;
    pllConfig.Q_Divider    = 0u;
    pllConfig.R_Divider    = 4u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_3, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_3, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_3, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 100000000u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_3 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_3, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   PLL source can not be changed while another PLL is running.
 *
 * \details PLL2 from HSI is running, source of PLL3 is requested to be CSI. After PLL2
 *          is stopped the source change is accepted.
 *
 * \par Expected results
 * - While PLL2 runs: RCC_REQUEST_ERROR, source stays HSI, PLL2 ACTIVE.
 * - After PLL2 is stopped: RCC_REQUEST_OK, source CSI.
 */
void It_Rcc_Set_PllsSource_OtherPllRunning_Rejected( void )
{
    rcc_PllConfigStruct_t pllConfig;
    rcc_PllClkSrc_t       pllSource = RCC_PLL_SRC_NONE;
    rcc_FunctionState_t   state     = RCC_FUNCTION_INACTIVE;

    pllConfig.Pll_Source   = RCC_PLL_SRC_HSI;
    pllConfig.M_Divider    = 4u;
    pllConfig.N_Multiplier = 25u;
    pllConfig.P_Divider    = 2u;
    pllConfig.Q_Divider    = 0u;
    pllConfig.R_Divider    = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_3, RCC_PLL_SRC_CSI ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_3, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, pllSource );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_3, RCC_PLL_SRC_CSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_3, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_CSI, pllSource );
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
 * - After reset: pin mode ANALOG (reset value of MODER of STM32H7), port clock ACTIVE.
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
 * \brief   MCO1 outputs HSI / 4 on PA8.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source HSI and divider 4 read back.
 * - PA8 in alternate function mode, alternate function 0.
 */
void It_Rcc_Set_ClkOutSource_Mco1Hsi_PinConfigured( void )
{
    rcc_ClkOut_Source_t source  = RCC_CLK_SOURCE_NONE;
    rcc_ClkOut_Div_t    divider = 0u;
    gpio_PinMode_t      pinMode = GPIO_PIN_MODE_INPUT;
    gpio_AltFunction_t  altFunc = GPIO_ALT_FUNC_15;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_HSI64 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 4u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO1_HSI64, source );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, divider );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( GPIO_PORT_A, GPIO_PIN_ID_8, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ALTERNATE, pinMode );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinAltFunction( GPIO_PORT_A, GPIO_PIN_ID_8, &altFunc ) );
    TEST_ASSERT_EQUAL( GPIO_ALT_FUNC_0, altFunc );
}


/**
 * \brief   MCO2 outputs SYSCLK / 5 on PC9.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source SYSCLK and divider 5 read back.
 * - PC9 in alternate function mode, alternate function 0.
 */
void It_Rcc_Set_ClkOutSource_Mco2Sysclk_PinConfigured( void )
{
    rcc_ClkOut_Source_t source  = RCC_CLK_SOURCE_NONE;
    rcc_ClkOut_Div_t    divider = 0u;
    gpio_PinMode_t      pinMode = GPIO_PIN_MODE_INPUT;
    gpio_AltFunction_t  altFunc = GPIO_ALT_FUNC_15;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO2_SYSCLK ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, 5u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO2, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO2_SYSCLK, source );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO2, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 5u, divider );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( GPIO_PORT_C, GPIO_PIN_ID_9, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ALTERNATE, pinMode );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinAltFunction( GPIO_PORT_C, GPIO_PIN_ID_9, &altFunc ) );
    TEST_ASSERT_EQUAL( GPIO_ALT_FUNC_0, altFunc );
}

/*---------------------------------- SysTick ---------------------------------*/

/**
 * \brief   SysTick interval is limited by 24-bit reload at real HCLK.
 *
 * \details HCLK 64 MHz - longest interval is 262 ms.
 *
 * \par Expected results
 * - 262 ms: RCC_REQUEST_OK, interval read back.
 * - 263 ms: RCC_REQUEST_ERROR, previous interval kept.
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
 * - Stage 1: software reset flag ACTIVE, brown-out, watchdog and low-power
 *   flags INACTIVE.
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
    }
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Configuration of the system clock from HSE PLL of the board (80 MHz).
 *
 * AHB / 1, APB1 / 2, APB2 / 2, voltage scale 3 (reset value), CSS off.
 *
 * \param config [out]: Clock configuration
 */
static void It_Rcc_Get_HsePllConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->HSE_ClockType     = IT_RCC_HSE_TYPE;
    config->HSE_Frequency_Hz  = IT_RCC_HSE_FREQ_HZ;
    config->SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    config->APB1_Divider      = RCC_APB1_DIVIDER_2;
    config->APB2_Divider      = RCC_APB2_DIVIDER_2;

    config->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSE;
    config->Pll_Config[ RCC_PLL_1 ].M_Divider    = IT_RCC_HSE_PLL_M;
    config->Pll_Config[ RCC_PLL_1 ].N_Multiplier = IT_RCC_HSE_PLL_N;
    config->Pll_Config[ RCC_PLL_1 ].P_Divider    = IT_RCC_HSE_PLL_P;
    config->Pll_Config[ RCC_PLL_1 ].Q_Divider    = IT_RCC_HSE_PLL_Q;
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
