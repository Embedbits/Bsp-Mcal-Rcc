/**
 * \author Mr.Nobody
 * \file ItTest_Rcc.c
 * \ingroup Rcc
 * \brief Integration tests of Reset and Clock Control (RCC) module on target.
 *
 * Rcc module runs on the MCU together with real hardware. Tests verify behavior
 * which cannot be verified by unit tests (emulated registers): oscillator start
 * and PLL lock, system clock switching and reconfiguration of running clock
 * tree, hardware protection of clocks in use, peripheral reset, real reset
 * source flags and clock output pins.
 *
 * Every test case starts after system reset with default clock configuration
 * set by StartUp (main PLL from HSI, SYSCLK 84 MHz, APB1 42 MHz, APB2 84 MHz).
 *
 * Used resources (see board configuration below):
 * - IT_RCC_HSE_*      - HSE oscillator of the board
 * - IT_RCC_LSE_FITTED - LSE crystal fitted on the board (backup domain,
 *                       LSE is switched off in tearDown())
 * - IT_RCC_FREE_*     - pin not connected on the board, used by peripheral
 *                       reset test
 * - PA8 (MCO1), PC9 (MCO2) - clock outputs, the pins output clock during the
 *                       test case (free on the board)
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

static void It_Rcc_Get_HsePllConfig ( rcc_ConfigStruct_t * const config );
static void It_Rcc_Check_BusClocks  ( rcc_FreqHz_t ahbClk, rcc_FreqHz_t apb1Clk, rcc_FreqHz_t apb2Clk );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/*----------------------------- Board configuration --------------------------*/
/* Boards are named by their MCU (IT_BOARD_<MCU>, name of the board from the detection) */
#if defined(IT_BOARD_STM32F405xG) || \
    defined(IT_BOARD_STM32F407xG) || \
    defined(IT_BOARD_STM32F415xG) || \
    defined(IT_BOARD_STM32F417xG)

    /** HSE - 8 MHz crystal X2 */
    #define IT_RCC_HSE_TYPE                 ( RCC_HSE_TYPE_CRYSTAL )
    #define IT_RCC_HSE_FREQ_HZ              ( 8000000u )

    /** Main PLL from HSE: 8 MHz / 8 * 336 / 2 = 168 MHz, Q 7 = 48 MHz */
    #define IT_RCC_HSE_PLL_M                ( 8u )
    #define IT_RCC_HSE_PLL_N                ( 336u )
    #define IT_RCC_HSE_PLL_P                ( 2u )
    #define IT_RCC_HSE_PLL_Q                ( 7u )
    #define IT_RCC_HSE_PLL_SYSCLK_HZ        ( 168000000u )

    /** LSE 32.768 kHz crystal X3 (fitted on the board - LSE starts) */
    #define IT_RCC_LSE_FITTED               ( 1u )

    /** Header P2 (PE7) - not connected on the board */
    #define IT_RCC_FREE_PORT                ( GPIO_PORT_E )
    #define IT_RCC_FREE_PIN                 ( GPIO_PIN_ID_7 )
    #define IT_RCC_FREE_PERIPH              ( RCC_PERIPH_GPIOE )

#elif defined(IT_BOARD_STM32F401xE) || \
      defined(IT_BOARD_STM32F411xE)

    /** HSE - 8 MHz MCO of the ST-LINK (oscillator bypassed, X3 not fitted) */
    #define IT_RCC_HSE_TYPE                 ( RCC_HSE_TYPE_SIG_IN )
    #define IT_RCC_HSE_FREQ_HZ              ( 8000000u )

#if defined(IT_BOARD_STM32F401xE)
    /** Main PLL from HSE: 8 MHz / 8 * 336 / 4 = 84 MHz, Q 7 = 48 MHz (STM32F401 maximum) */
    #define IT_RCC_HSE_PLL_M                ( 8u )
    #define IT_RCC_HSE_PLL_N                ( 336u )
    #define IT_RCC_HSE_PLL_P                ( 4u )
    #define IT_RCC_HSE_PLL_Q                ( 7u )
    #define IT_RCC_HSE_PLL_SYSCLK_HZ        ( 84000000u )
#else
    /** Main PLL from HSE: 8 MHz / 8 * 384 / 4 = 96 MHz, Q 8 = 48 MHz (USB clock of STM32F411) */
    #define IT_RCC_HSE_PLL_M                ( 8u )
    #define IT_RCC_HSE_PLL_N                ( 384u )
    #define IT_RCC_HSE_PLL_P                ( 4u )
    #define IT_RCC_HSE_PLL_Q                ( 8u )
    #define IT_RCC_HSE_PLL_SYSCLK_HZ        ( 96000000u )
#endif

    /** LSE 32.768 kHz crystal X2 (fitted on the board - LSE starts) */
    #define IT_RCC_LSE_FITTED               ( 1u )

    /** Morpho CN10 pin 2 (PC8) - not connected on the board */
    #define IT_RCC_FREE_PORT                ( GPIO_PORT_C )
    #define IT_RCC_FREE_PIN                 ( GPIO_PIN_ID_8 )
    #define IT_RCC_FREE_PERIPH              ( RCC_PERIPH_GPIOC )

#else
    #error "Board of Rcc integration tests is not defined (INTEGRATION_TEST_BOARD)."
#endif

/** System clock of default configuration (StartUp) */
#define IT_RCC_DEFAULT_SYSCLK_HZ            ( 84000000u )

/** HSI frequency */
#define IT_RCC_HSI_FREQ_HZ                  ( 16000000u )

/** 48 MHz clock of USB OTG FS, SDIO and RNG */
#define IT_RCC_PLL48_CLK_HZ                 ( 48000000u )

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
    /* LSE is in backup domain - not reset by system reset */
    (void)Rcc_Set_OscInactive( RCC_OSC_LSE );
}

/* =============================== TESTS ==================================== */

/*----------------------------- Default clock --------------------------------*/

/**
 * \brief   Default configuration of StartUp runs from HSI PLL at 84 MHz.
 *
 * \details Reads state of the clock tree after StartUp.
 *
 * \par Expected results
 * - Main PLL ACTIVE with HSI source, HSI ACTIVE.
 * - AHB 84 MHz, APB1 42 MHz, APB2 84 MHz, SysTick interval 1 ms.
 */
void It_Rcc_StartUp_DefaultConfig_Pll84MHzActive( void )
{
    rcc_FunctionState_t state     = RCC_FUNCTION_INACTIVE;
    rcc_PllClkSrc_t     pllSource = RCC_PLL_SRC_NONE;
    rcc_Time_ms_t       interval  = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_MAIN, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_MAIN, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, pllSource );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ / 2u, IT_RCC_DEFAULT_SYSCLK_HZ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Kernel clocks of default configuration.
 *
 * \par Expected results
 * - USB OTG FS, RNG (if the MCU has one) and SDIO 48 MHz (main PLL Q), IWDG 32 kHz (LSI).
 * - APB1 timer TIM2 84 MHz (2 x PCLK1), APB2 timer TIM1 84 MHz (PCLK2, not divided).
 */
void It_Rcc_Get_PeriphClk_DefaultConfig_KernelClocks( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USB_OTG_FS, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_PLL48_CLK_HZ, freq );
#if defined(RNG)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_PLL48_CLK_HZ, freq );
#endif /* RNG */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SDIO, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_PLL48_CLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_IWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, freq );
}

/*----------------------------- Initialization -------------------------------*/

/**
 * \brief   Rcc_Init() switches running clock tree to HSE PLL with CSS.
 *
 * \details HSE of the board, main PLL to maximal frequency, AHB / 1, APB1 / 4,
 *          APB2 / 2, clock security system enabled.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL source HSE, PLL ACTIVE (HSE started, PLL locked).
 * - AHB IT_RCC_HSE_PLL_SYSCLK_HZ (STM32F4DISCOVERY 168 MHz), APB1 / 4, APB2 / 2,
 *   TIM2 AHB / 2, TIM1 AHB, USB OTG FS 48 MHz, SysTick interval 1 ms.
 * - No fault (flash latency and voltage scaling sufficient, no CSS NMI).
 */
void It_Rcc_Init_HsePllWithCss_MaxFrequency( void )
{
    rcc_ConfigStruct_t  config;
    rcc_PllClkSrc_t     pllSource = RCC_PLL_SRC_NONE;
    rcc_FunctionState_t state     = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t        freq      = 0u;
    rcc_Time_ms_t       interval  = 0u;

    It_Rcc_Get_HsePllConfig( &config );
    config.CSS_Enable = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_MAIN, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSE, pllSource );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_MAIN, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_PLL_SYSCLK_HZ, IT_RCC_HSE_PLL_SYSCLK_HZ / 4u, IT_RCC_HSE_PLL_SYSCLK_HZ / 2u );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSE_PLL_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USB_OTG_FS, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_PLL48_CLK_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Init() with HSI system clock switches all PLLs off.
 *
 * \details HSI system clock, PLLs not used.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, main PLL and PLLI2S INACTIVE.
 * - AHB 16 MHz, APB1 8 MHz, APB2 16 MHz.
 */
void It_Rcc_Init_HsiSysClk_PllsInactive( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource                      = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    config.Pll_Config[ RCC_PLL_MAIN ].Pll_Source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    for( uint32_t pllId = 0u; (uint32_t)RCC_PLL_CNT > pllId; pllId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( (rcc_PllId_t)pllId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    It_Rcc_Check_BusClocks( IT_RCC_HSI_FREQ_HZ, IT_RCC_HSI_FREQ_HZ / 2u, IT_RCC_HSI_FREQ_HZ );
}


/**
 * \brief   Rcc_Init() with HSE used directly as system clock.
 *
 * \details HSE of the board as system clock, PLLs not used, APB1 / 1.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, main PLL INACTIVE, AHB / APB1 / APB2 clock = HSE frequency.
 */
void It_Rcc_Init_HseSysClk_HseFrequency( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType                          = IT_RCC_HSE_TYPE;
    config.HSE_Frequency_Hz                       = IT_RCC_HSE_FREQ_HZ;
    config.SystemClockSource                      = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.APB1_Divider                           = RCC_APB1_DIVIDER_1;
    config.Pll_Config[ RCC_PLL_MAIN ].Pll_Source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_MAIN, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    It_Rcc_Check_BusClocks( IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_FREQ_HZ, IT_RCC_HSE_FREQ_HZ );
}


/**
 * \brief   Rcc_Init() rejects frequency above limit of voltage scale.
 *
 * \details HSE PLL configuration of the board with voltage scale 2 (STM32F407:
 *          168 MHz, scale 2 max. 144 MHz; STM32F411: 96 MHz, scale 2 max. 84 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system keeps running from HSI (AHB 16 MHz).
 */
void It_Rcc_Init_SysClkOverScaleLimit_KeepsHsi( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       ahbClk = 0u;

    It_Rcc_Get_HsePllConfig( &config );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_HSI_FREQ_HZ, ahbClk );
}


/**
 * \brief   Rcc_Init() reconfigures clock tree initialized by previous Rcc_Init().
 *
 * \details HSE PLL configuration, then default configuration again.
 *
 * \par Expected results
 * - Both calls RCC_REQUEST_OK, PLL source HSI, AHB 84 MHz, APB1 42 MHz, APB2 84 MHz.
 */
void It_Rcc_Init_Reinit_ReturnsToDefaultClock( void )
{
    rcc_ConfigStruct_t config;
    rcc_PllClkSrc_t    pllSource = RCC_PLL_SRC_NONE;

    It_Rcc_Get_HsePllConfig( &config );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_MAIN, &pllSource ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, pllSource );

    It_Rcc_Check_BusClocks( IT_RCC_DEFAULT_SYSCLK_HZ, IT_RCC_DEFAULT_SYSCLK_HZ / 2u, IT_RCC_DEFAULT_SYSCLK_HZ );
}

/*--------------------- Hardware protection of clocks in use -----------------*/

/**
 * \brief   HSI can not be switched off while it clocks the system.
 *
 * \details Default configuration - HSI is source of main PLL (system clock).
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
 * \brief   Main PLL can not be switched off while it is the system clock.
 *
 * \par Expected results
 * - Rcc_Set_PllInactive() returns RCC_REQUEST_ERROR, PLL keeps locked,
 *   AHB clock stays 84 MHz.
 */
void It_Rcc_Set_PllInactive_PllIsSysClk_KeptRunning( void )
{
    rcc_FreqHz_t ahbClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_MAIN ) );

    /* Enable bit cleared by the request is restored */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_MAIN ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, ahbClk );
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

/*------------------------------------ PLLs ----------------------------------*/

/**
 * \brief   PLLI2S locks with M divider shared with running main PLL.
 *
 * \details PLLI2S HSI, M 16 (equal to main PLL), N 192, R 2.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLI2S ACTIVE, output R 96 MHz.
 * - Deactivation RCC_REQUEST_OK, state INACTIVE.
 */
void It_Rcc_Set_PllConfig_PllI2s_LocksAndStops( void )
{
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 16u, .N_Multiplier = 192u,
        .P_Divider  = 0u,              .Q_Divider = 0u,  .R_Divider    = 2u
    };
    rcc_FunctionState_t   state = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t          freq  = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_I2S, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_I2S, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_I2S, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 96000000u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_I2S ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_I2S, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   PLLI2S can not change M divider used by running main PLL.
 *
 * \details PLLI2S requests M 8, main PLL runs with M 16.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLLI2S INACTIVE, system clock unchanged (84 MHz).
 * - Ignored on MCUs with own PLLI2S M divider (RCC_PLLI2SCFGR_PLLI2SM, e.g.
 *   STM32F411 - test function exists always, the runner collects test functions
 *   without preprocessor).
 */
void It_Rcc_Set_PllConfig_PllI2sSharedMInUse_Rejected( void )
{
#if defined(RCC_PLLI2SCFGR_PLLI2SM)
    TEST_IGNORE_MESSAGE( "MCU has own PLLI2S M divider" );
#else
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 8u, .N_Multiplier = 96u,
        .P_Divider  = 0u,              .Q_Divider = 0u, .R_Divider    = 2u
    };
    rcc_FunctionState_t   state  = RCC_FUNCTION_ACTIVE;
    rcc_FreqHz_t          ahbClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_I2S, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_I2S, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( IT_RCC_DEFAULT_SYSCLK_HZ, ahbClk );
#endif /* RCC_PLLI2SCFGR_PLLI2SM */
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
 * - After reset: pin mode INPUT (reset value of MODER), port clock ACTIVE.
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
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_INPUT, pinMode );
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
 * \details HCLK 84 MHz - longest interval is 199 ms.
 *
 * \par Expected results
 * - 199 ms: RCC_REQUEST_OK, interval read back.
 * - 200 ms: RCC_REQUEST_ERROR, previous interval kept.
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
 * \brief Configuration of maximal frequency from HSE PLL of the board.
 *
 * AHB / 1, APB1 / 4, APB2 / 2, voltage scale 1, CSS off.
 *
 * \param config [out]: Clock configuration
 */
static void It_Rcc_Get_HsePllConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->HSE_ClockType    = IT_RCC_HSE_TYPE;
    config->HSE_Frequency_Hz = IT_RCC_HSE_FREQ_HZ;
    config->APB1_Divider     = RCC_APB1_DIVIDER_4;
    config->APB2_Divider     = RCC_APB2_DIVIDER_2;

    config->Pll_Config[ RCC_PLL_MAIN ].Pll_Source   = RCC_PLL_SRC_HSE;
    config->Pll_Config[ RCC_PLL_MAIN ].M_Divider    = IT_RCC_HSE_PLL_M;
    config->Pll_Config[ RCC_PLL_MAIN ].N_Multiplier = IT_RCC_HSE_PLL_N;
    config->Pll_Config[ RCC_PLL_MAIN ].P_Divider    = IT_RCC_HSE_PLL_P;
    config->Pll_Config[ RCC_PLL_MAIN ].Q_Divider    = IT_RCC_HSE_PLL_Q;
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( apb1Clk, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( apb2Clk, freq );
}
