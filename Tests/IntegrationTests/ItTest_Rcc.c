/**
 * \author Mr.Nobody
 * \file ItTest_Rcc.c
 * \ingroup Rcc
 * \brief Integration tests of Reset and clock control (RCC) module on target.
 *
 * Rcc module runs on the MCU (initialized by StartUp with default configuration)
 * together with real NVIC, GPIO and TIM modules and hardware. Tests verify
 * behavior which cannot be verified by unit tests (emulated registers):
 * peripheral clock / reset / sleep control, PLL and bus clock frequencies,
 * SysTick interval, timer kernel clock with APB prescaler, clock output and
 * real frequency of the clock tree compared with an independent reference
 * (LSI on MCO2 pin) and reset source flags.
 *
 * Used resources:
 * - TIM2 - counter clocked from the frequency reported by RCC (Tim module)
 * - TIM7 - peripheral clock / reset / sleep control (not used otherwise)
 * - SysTick - core clock based time base
 * - MCO2 pin (PC9) - LSI clock output, not connected on the board, read back
 *   by GPIO input data register, no external wiring
 * - LSE crystal X2 (32.768 kHz) of the Nucleo board, started by the test (the
 *   default configuration of StartUp starts no external oscillator)
 * - HSE crystal of the board, started by the test and supervised by the clock
 *   security system (NMI reported as fault)
 *
 * \note LSI accuracy (STM32H5 datasheet, ~ +-5 % over temperature / voltage)
 *       defines tolerance of the frequency measurement.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Nvic_Port.h"                      /* SysTick handler                */
#include "Gpio_Port.h"                      /* MCO pin level                  */
#include "Tim_Port.h"                       /* Timer clocked from RCC         */
#include "Stm32_rcc.h"                      /* RTC kernel clock enable bit    */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void             It_Rcc_Init_Timer           ( tim_FreqHz_t timerFreq );
static tim_Counter_t    It_Rcc_Get_TimerCounter     ( void );
static void             It_Rcc_Start_SysTick        ( void );
static void             It_Rcc_Start_Lsi            ( void );
static tim_Counter_t    It_Rcc_Measure_SysTickPeriod ( uint32_t sysTickCnt );
static void             It_Rcc_Wait_SysTicks        ( uint32_t sysTickCnt );
static rcc_RequestState_t ItRcc_Get_BackupDomainResetResult( void );

static void             It_Rcc_SysTickHandler       ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Timer clocked from frequency reported by RCC (APB1, 32-bit) */
#define IT_RCC_TIM                          ( TIM_PERIPH_2 )

/** Peripheral used for clock / reset / sleep control tests (not used otherwise) */
#define IT_RCC_PERIPH                       ( RCC_PERIPH_TIM7 )

/** Counter frequency of the timer [Hz] (1 count = 1 us) */
#define IT_RCC_TIM_FREQ_HZ                  ( 1000000u )

/** Count of SysTick periods (1 ms) of the measurement */
#define IT_RCC_SYSTICK_MEAS_CNT             ( 100u )

/** Tolerance of the timer / SysTick ratio (both derived from HCLK) [counts] */
#define IT_RCC_SYSTICK_TOL                  ( IT_RCC_TIM_FREQ_HZ / 1000u )

/** Nominal LSI frequency [Hz] */
#define IT_RCC_LSI_FREQ_HZ                  ( 32000u )

/** Count of LSI periods of the measurement (100 ms) */
#define IT_RCC_LSI_PERIODS                  ( 3200u )

/** LSI accuracy [%] */
#define IT_RCC_LSI_TOL_PCT                  ( 5u )

/** MCO2 pin (PC9) */
#define IT_RCC_MCO2_PORT                    ( GPIO_PORT_C )
#define IT_RCC_MCO2_PIN                     ( GPIO_PIN_ID_9 )

/** Maximal count of wait loop iterations (timeout ~ seconds) */
#define IT_RCC_WAIT_LOOPS                   ( 20000000u )

/** Wait of the test for the LSE crystal start-up [ms] (tSU(LSE) 2 s typical, datasheet) */
#define IT_RCC_LSE_STARTUP_WAIT_MS          ( 5000u )

/** Maximal count of LSE state reads (backup of the SysTick time base) */
#define IT_RCC_LSE_WAIT_LOOPS               ( 100000000u )

/** Time of HSE supervised by the clock security system [ms] */
#define IT_RCC_CSS_SUPERVISION_MS           ( 10u )

/** Value written to the TIM12 auto-reload register (reset value 0xFFFF, unclocked register reads 0) */
#define IT_RCC_TIM12_ARR_VALUE              ( 0x1234u )

/* ============================== MACROS ==================================== */

/* ========================== LOCAL VARIABLES =============================== */

/** Count of SysTick interrupts */
static volatile uint32_t itRcc_SysTickCnt;

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    itRcc_SysTickCnt = 0u;
}

void tearDown( void )
{
    /* Every test case runs after system reset (clock configuration of StartUp) */
}

/* =============================== TESTS ==================================== */

/*------------------------- Peripheral clock control -------------------------*/

/**
 * \brief   Peripheral clock on target is enabled and disabled.
 *
 * \details Reads TIM7 clock state after reset, enables the clock, reads the state,
 *          disables the clock and reads the state.
 *
 * \par Expected results
 * - State inactive after reset, active after enabling, inactive after disabling.
 */
void It_Rcc_Set_PeriphActive_PeriphClock_StateReadBack( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( IT_RCC_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( IT_RCC_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/**
 * \brief   TIM12 on target is clocked from APB1.
 *
 * \details Enables TIM12 clock, reads the state, writes the TIM12 auto-reload register and
 *          reads it back (TIM12 register accessed directly, the test does not depend on the
 *          Tim module). Sets APB2 divider 4 (APB2 timer clock = AHB1 / 2) and reads the TIM2
 *          (APB1) and TIM12 kernel clock. Bug AB#2257: the clock enable bit was written to
 *          RCC_APB2ENR, TIM12 registers stayed 0 and the APB2 timer clock was reported.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, state active, TIM12_ARR reads back the written value.
 * - TIM12 kernel clock = TIM2 kernel clock.
 * - MCU without TIM12 (STM32H503): test ignored.
 */
void It_Rcc_Set_PeriphActive_Tim12_Apb1ClockRegistersWritable( void )
{
#if defined(TIM12)
    rcc_FunctionState_t state    = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t        tim2Clk  = 0u;
    rcc_FreqHz_t        tim12Clk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_TIM12 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_TIM12, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TIM12->ARR = IT_RCC_TIM12_ARR_VALUE;
    TEST_ASSERT_EQUAL_HEX32( IT_RCC_TIM12_ARR_VALUE, TIM12->ARR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB2_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2,  &tim2Clk ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM12, &tim12Clk ) );
    TEST_ASSERT_EQUAL_UINT32( tim2Clk, tim12Clk );
#else
    TEST_IGNORE_MESSAGE( "TIM12 is not available on this MCU" );
#endif /* TIM12 */
}

/**
 * \brief   RTC kernel clock on target is enabled and the RTC sub-second counter runs.
 *
 * \details Resets the backup domain first (RTCSEL is written once per backup domain life and
 *          kept over resets, an earlier run or another module may have selected a source).
 *          Enables RTC with LSI clock, reads the state and waits until the RTC sub-second
 *          register (RTC_SSR, decremented at LSI / 128 = 250 Hz with reset prescalers)
 *          changes. RTC registers are read directly (the test does not depend on the Rtc
 *          module), RTC_DR is read after RTC_SSR to unlock the calendar shadow registers.
 *          Disables RTC and reads the state. Resets the backup domain again, the board is
 *          left without an RTC clock source. Bug AB#1559: RCC_BDCR.RTCEN was never set, the
 *          RTC counter did not run. Where the stack lies in the SRAM2 (STM32H503) the backup
 *          domain reset erases the stack, the request is refused there and the RTC clock
 *          source of the backup domain is kept (LSI expected).
 *
 * \par Expected results
 * - Backup domain reset: RCC_REQUEST_OK, RCC_BDCR.RTCSEL (no clock), RTCEN and LSEON cleared;
 *   stack in the SRAM2: RCC_REQUEST_ERROR, RCC_BDCR.VSWRST not set.
 * - RCC_REQUEST_OK, state active after enabling, RCC_BDCR.RTCEN set, RTCSEL = LSI.
 * - RTC_SSR changes before timeout.
 * - State inactive after disabling, RCC_BDCR.RTCEN cleared.
 * - Backup domain reset at the end: RCC_REQUEST_OK, RCC_BDCR.RTCSEL cleared; stack in the SRAM2:
 *   RCC_REQUEST_ERROR, RCC_BDCR.VSWRST not set.
 */
void It_Rcc_Set_PeriphActive_RtcLsi_RtcCounterRuns( void )
{
    rcc_FunctionState_t      state         = RCC_FUNCTION_INACTIVE;
    uint32_t                 startSubSec   = 0u;
    uint32_t                 subSec        = 0u;
    const rcc_RequestState_t resetExpected = ItRcc_Get_BackupDomainResetResult();

    TEST_ASSERT_EQUAL( resetExpected, Rcc_Set_BackupDomainReset() );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_VSWRST, RCC->BDCR );

    if( RCC_REQUEST_OK == resetExpected )
    {
        TEST_ASSERT_BITS_LOW( ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN | RCC_BDCR_LSEON ), RCC->BDCR );
    }
    else
    {
        /* Stack in the SRAM2: the reset was refused, the backup domain is kept */
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_LSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_RTCEN, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, ( RCC->BDCR & RCC_BDCR_RTCSEL ) );

    startSubSec = RTC->SSR;
    (void)RTC->DR;
    subSec      = startSubSec;

    for( uint32_t loopIdx = 0u;
         ( IT_RCC_WAIT_LOOPS > loopIdx ) &&
         ( startSubSec == subSec );
         loopIdx++ )
    {
        subSec = RTC->SSR;
        (void)RTC->DR;
    }

    TEST_ASSERT_NOT_EQUAL_MESSAGE( startSubSec, subSec, "RTC sub-second counter does not run" );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_LSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_RTCEN, RCC->BDCR );

    TEST_ASSERT_EQUAL( resetExpected, Rcc_Set_BackupDomainReset() );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_VSWRST, RCC->BDCR );

    if( RCC_REQUEST_OK == resetExpected )
    {
        TEST_ASSERT_BITS_LOW( RCC_BDCR_RTCSEL, RCC->BDCR );
    }
    else
    {
        /* Stack in the SRAM2: the reset was refused, the RTC clock source stays selected */
    }
}

/**
 * \brief   Peripheral reset on target is activated and released.
 *
 * \details Activates TIM7 reset, reads the reset state, releases the reset and reads
 *          the state.
 *
 * \par Expected results
 * - Reset state active after activation, inactive after release.
 */
void It_Rcc_Set_ResetActive_PeriphReset_StateReadBack( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( IT_RCC_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( IT_RCC_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/**
 * \brief   Peripheral clock in sleep mode on target is disabled and enabled.
 *
 * \details Reads TIM7 sleep clock state after reset, disables the sleep clock, reads
 *          the state, enables it and reads the state.
 *
 * \par Expected results
 * - Sleep state active after reset (hardware reset value).
 * - Inactive after disabling, active after enabling.
 */
void It_Rcc_Set_SleepInactive_PeriphSleepClock_StateReadBack( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    /* Clock in sleep mode is enabled after reset */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( IT_RCC_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( IT_RCC_PERIPH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( IT_RCC_PERIPH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}

/*--------------------------- Clock tree frequencies -------------------------*/

/**
 * \brief   Bus clocks of default configuration equal system clock.
 *
 * \details Reads AHB1, APB1, APB2 and APB3 clocks of default configuration (StartUp,
 *          all bus dividers 1).
 *
 * \par Expected results
 * - AHB1 clock is not 0.
 * - APB1, APB2 and APB3 clocks equal AHB1 clock.
 */
void It_Rcc_Get_ClkBusClk_DefaultConfig_BusClocksEqualSystemClock( void )
{
    rcc_FreqHz_t ahbClk = 0u;
    rcc_FreqHz_t apbClk = 0u;

    /* Default configuration: all bus dividers 1 */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_GREATER_THAN_UINT32( 0u, ahbClk );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &apbClk ) );
    TEST_ASSERT_EQUAL_UINT32( ahbClk, apbClk );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &apbClk ) );
    TEST_ASSERT_EQUAL_UINT32( ahbClk, apbClk );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB3, &apbClk ) );
    TEST_ASSERT_EQUAL_UINT32( ahbClk, apbClk );
}

/**
 * \brief   PLL1 P output of default configuration is system clock.
 *
 * \details Reads PLL1 state, PLL1 P output frequency and AHB1 clock.
 *
 * \par Expected results
 * - PLL1 is active, P output frequency equals AHB1 clock.
 */
void It_Rcc_Get_PllClk_OutP_DefaultConfig_EqualsSystemBusClock( void )
{
    rcc_FunctionState_t pllState = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t        pllClk   = 0u;
    rcc_FreqHz_t        ahbClk   = 0u;

    /* Default configuration: PLL1 P output is system clock */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &pllState ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, pllState );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( pllClk, ahbClk );
}

/**
 * \brief   PLL output with divider 0 is disabled on target.
 *
 * \details Configures PLL2 with the PLL1 values of the default configuration (P, Q, R used) and
 *          reads the Q output frequency, configures PLL2 again with Q divider 0 and reads the Q
 *          and P output frequencies, stops PLL2. Bug AB#2279: the Q output stayed enabled from
 *          the first configuration.
 *
 * \par Expected results
 * - All requests RCC_REQUEST_OK.
 * - Q output frequency not 0 when used, 0 Hz with divider 0 (output disabled).
 * - P output frequency not 0 in both configurations.
 */
void It_Rcc_Set_PllConfig_Pll2UnusedOutput_Disabled( void )
{
    rcc_ConfigStruct_t    config;
    rcc_PllConfigStruct_t pllConfig;
    rcc_FreqHz_t          outQClk = 0u;
    rcc_FreqHz_t          outPClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    pllConfig = config.Pll_Config[ RCC_PLL_1 ];

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_2, &outQClk ) );
    TEST_ASSERT_NOT_EQUAL( 0u, outQClk );

    pllConfig.Q_Divider = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_2, &outQClk ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, outQClk );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_2, &outPClk ) );
    TEST_ASSERT_NOT_EQUAL( 0u, outPClk );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_2 ) );
}

/**
 * \brief   SysTick 1 ms interval corresponds to timer clocked from RCC frequency.
 *
 * \details Starts TIM2 with 1 MHz counter (prescaler from frequency reported by RCC)
 *          and SysTick with 1 ms interval, reads the interval and measures 100 SysTick
 *          periods by TIM2.
 *
 * \par Expected results
 * - Interval reads back 1 ms.
 * - 100 SysTick periods = 100000 timer counts +- 1000.
 */
void It_Rcc_Set_SysTickInterval_1ms_TimerCountsMatchSysTick( void )
{
    rcc_Time_ms_t interval = 0u;

    It_Rcc_Init_Timer( IT_RCC_TIM_FREQ_HZ );
    It_Rcc_Start_SysTick();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );

    /* Both SysTick and timer are derived from frequencies reported by RCC */
    TEST_ASSERT_UINT32_WITHIN( IT_RCC_SYSTICK_TOL, IT_RCC_SYSTICK_MEAS_CNT * 1000u,
                               It_Rcc_Measure_SysTickPeriod( IT_RCC_SYSTICK_MEAS_CNT ) );
}

/**
 * \brief   Timer kernel clock is doubled for divided APB1.
 *
 * \details Sets APB1 divider 2, reads AHB1, APB1 and TIM2 clock, then measures 100
 *          SysTick periods by TIM2 (1 MHz counter).
 *
 * \par Expected results
 * - APB1 = AHB1 / 2, TIM2 kernel clock = AHB1 (2 x PCLK1, TIMPRE = 0).
 * - 100 SysTick periods = 100000 timer counts +- 1000 (timer prescaler calculated
 *   from the doubled kernel clock is correct).
 */
void It_Rcc_Set_ClkBusDivider_Apb1Div2_TimerKernelClockDoubled( void )
{
    rcc_FreqHz_t ahbClk = 0u;
    rcc_FreqHz_t apbClk = 0u;
    rcc_FreqHz_t timClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_2 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1,   &ahbClk ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &apbClk ) );
    TEST_ASSERT_EQUAL_UINT32( ahbClk / 2u, apbClk );

    /* Timer kernel clock is 2 x PCLK when APB prescaler is not 1 (TIMPRE = 0) */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &timClk ) );
    TEST_ASSERT_EQUAL_UINT32( ahbClk, timClk );

    It_Rcc_Init_Timer( IT_RCC_TIM_FREQ_HZ );
    It_Rcc_Start_SysTick();

    TEST_ASSERT_UINT32_WITHIN( IT_RCC_SYSTICK_TOL, IT_RCC_SYSTICK_MEAS_CNT * 1000u,
                               It_Rcc_Measure_SysTickPeriod( IT_RCC_SYSTICK_MEAS_CNT ) );
}

/**
 * \brief   Real clock tree frequency matches independent LSI reference.
 *
 * \details Starts LSI and routes it to MCO2 pin (PC9, divider 1). Starts TIM2 with
 *          1 MHz counter (frequency reported by RCC). Pin level is polled, timer
 *          counter is captured at the first rising edge and after 3200 LSI periods
 *          (100 ms).
 *
 * \par Expected results
 * - LSI clock is present on MCO2 pin (more than 3200 rising edges before timeout).
 * - Measured duration = 100000 timer counts +- 5 % (LSI accuracy).
 */
void It_Rcc_Get_PeriphClk_Tim2_MatchesLsiReference( void )
{
    gpio_PinLevel_t     pinLevel    = GPIO_PIN_LEVEL_LOW;
    gpio_PinLevel_t     prevLevel   = GPIO_PIN_LEVEL_LOW;
    uint32_t            risingEdges = 0u;
    tim_Counter_t       startCnt    = 0u;
    tim_Counter_t       endCnt      = 0u;

    const tim_Counter_t expectedCnt = ( IT_RCC_LSI_PERIODS / ( IT_RCC_LSI_FREQ_HZ / 1000u ) ) * ( IT_RCC_TIM_FREQ_HZ / 1000u );

    /* LSI on MCO2 pin - independent of the HSI / PLL clock tree (LSI started by RCC) */
    It_Rcc_Start_Lsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO2_LSI ) );

    It_Rcc_Init_Timer( IT_RCC_TIM_FREQ_HZ );

    /* Synchronization to rising edge, then counting of LSI periods */
    for( uint32_t loopIdx = 0u;
         ( IT_RCC_WAIT_LOOPS > loopIdx ) &&
         ( IT_RCC_LSI_PERIODS >= risingEdges );
         loopIdx++ )
    {
        TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinLevel( IT_RCC_MCO2_PORT, IT_RCC_MCO2_PIN, &pinLevel ) );

        if( ( GPIO_PIN_LEVEL_LOW  == prevLevel ) &&
            ( GPIO_PIN_LEVEL_HIGH == pinLevel  )    )
        {
            if( 0u == risingEdges )
            {
                startCnt = It_Rcc_Get_TimerCounter();
            }
            else
            {
                endCnt = It_Rcc_Get_TimerCounter();
            }

            risingEdges++;
        }
        else
        {
            /* No rising edge */
        }

        prevLevel = pinLevel;
    }

    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE( IT_RCC_LSI_PERIODS, risingEdges, "LSI clock not present on MCO2 pin" );
    TEST_ASSERT_UINT32_WITHIN( ( expectedCnt * IT_RCC_LSI_TOL_PCT ) / 100u, expectedCnt, endCnt - startCnt );
}

/*---------------------------- Oscillators -----------------------------------*/

/**
 * \brief   LSI oscillator on target is started and stopped.
 *
 * \details Enables LSI, reads the state, disables LSI and reads the state.
 *
 * \par Expected results
 * - State active after enabling (ready flag), inactive after disabling.
 */
void It_Rcc_Set_OscActive_Lsi_StateReadBack( void )
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
 * \brief   LSE crystal on target is enabled by Rcc_Init() and becomes ready.
 *
 * \details Rcc_Init() with default configuration and LSE crystal (medium-low drive; the
 *          default configuration of StartUp starts no LSE, bug AB#2115) - LSE is enabled
 *          without waiting for the crystal start-up (bug AB#1560 - LSE could not be
 *          activated). The test waits for the ready state (SysTick time base), activates
 *          LSE through the oscillator interface (running LSE is kept), disables LSE and
 *          reads the state. NUCLEO-H503RB (MB1814) and NUCLEO-H563ZI (MB1404) have the
 *          32.768 kHz crystal X2 connected to OSC32_IN / OSC32_OUT.
 *
 * \note    Device errata bug AB#1560 (ES0561 2.2.5, ES0565 2.2.17, ES0621 2.2.8 "LSE low drive
 *          mode is not functional"): the crystal is started with medium-low drive, not with
 *          the forbidden LSEDRV = 00 reset value.
 *
 * \par Expected results
 * - Rcc_Init(): RCC_REQUEST_OK, RCC_BDCR.LSEON set, LSEDRV medium-low, LSEBYP cleared.
 * - State active within the start-up wait of the test.
 * - Activation of running LSE: RCC_REQUEST_OK, RCC_BDCR unchanged, state active.
 * - State inactive after disabling.
 */
void It_Rcc_Set_OscActive_LseCrystal_StateReadBack( void )
{
    rcc_ConfigStruct_t  config;
    rcc_FunctionState_t state     = RCC_FUNCTION_INACTIVE;
    uint32_t            startTick = 0u;
    uint32_t            bdcrValue = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.LSE_ClockType = RCC_LSE_TYPE_CRYSTAL;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSEDRV_0, RCC->BDCR & RCC_BDCR_LSEDRV );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEBYP, RCC->BDCR );

    It_Rcc_Start_SysTick();

    startTick = itRcc_SysTickCnt;

    for( uint32_t loopIdx = 0u;
         ( IT_RCC_LSE_WAIT_LOOPS            >  loopIdx                          ) &&
         ( IT_RCC_LSE_STARTUP_WAIT_MS       > ( itRcc_SysTickCnt - startTick ) ) &&
         ( RCC_FUNCTION_INACTIVE            == state                            );
         loopIdx++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    }

    TEST_ASSERT_EQUAL_MESSAGE( RCC_FUNCTION_ACTIVE, state, "LSE crystal not ready" );

    bdcrValue = RCC->BDCR;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );
    TEST_ASSERT_EQUAL_HEX32( bdcrValue, RCC->BDCR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/**
 * \brief   CSI oscillator on target is started and stopped.
 *
 * \details Enables CSI, reads the state, disables CSI and reads the state.
 *
 * \par Expected results
 * - State active after enabling (ready flag), inactive after disabling.
 */
void It_Rcc_Set_OscActive_Csi_StateReadBack( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_CSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_CSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_CSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_CSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/*------------------------- Clock security system ----------------------------*/

/**
 * \brief   Clock security system of HSE on target is enabled by Rcc_Init().
 *
 * \details Rcc_Init() with default configuration (PLL1 from HSI), HSE crystal (not started
 *          by the default configuration, bug AB#2115) and CSS
 *          enabled (bug AB#978 - CSS_Enable of the configuration was ignored), HSE
 *          supervised for 10 ms. Rcc_Init() repeated with the same configuration - HSE
 *          is stopped and restarted, the clock detector is disabled while HSE is
 *          stopped (RM0481 Rev 5 11.4.10, RM0492 Rev 3 10.4.10), HSE supervised for
 *          10 ms. A detected HSE failure generates NMI, reported as fault of the test.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_CR.HSECSSON and HSERDY set, RCC_CIFR.HSECSSF not set.
 * - Repeated Rcc_Init(): RCC_REQUEST_OK, HSECSSON and HSERDY set, HSECSSF not set.
 */
void It_Rcc_Init_HseCss_EnabledWithoutFailure( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    config.CSS_Enable    = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    It_Rcc_Start_SysTick();
    It_Rcc_Wait_SysTicks( IT_RCC_CSS_SUPERVISION_MS );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSECSSON | RCC_CR_HSERDY, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CIFR_HSECSSF, RCC->CIFR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    It_Rcc_Wait_SysTicks( IT_RCC_CSS_SUPERVISION_MS );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSECSSON | RCC_CR_HSERDY, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CIFR_HSECSSF, RCC->CIFR );
}

/*----------------------------- Clock output ---------------------------------*/

/**
 * \brief   MCO2 divider on target is set and read back.
 *
 * \details Sets MCO2 divider 4 and 1 and reads them back.
 *
 * \par Expected results
 * - Divider reads back 4, then 1.
 */
void It_Rcc_Set_ClkOutDivider_Mco2_DividerReadBack( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, 4u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO2, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, divider );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO2, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, divider );
}


/**
 * \brief   MCO2 source on target is set and read back.
 *
 * \details Starts LSI, sets MCO2 source LSI and reads it back.
 *
 * \par Expected results
 * - Source reads back RCC_CLK_SOURCE_MCO2_LSI.
 */
void It_Rcc_Get_ClkOutSource_Mco2Lsi_SourceReadBack( void )
{
    rcc_ClkOut_Source_t source = RCC_CLK_SOURCE_NONE;

    It_Rcc_Start_Lsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO2_LSI ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO2, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO2_LSI, source );
}

/*------------------------------ Reset source --------------------------------*/

/**
 * \brief   Rcc_Set_ResetSourceClear() on target clears all reset flags.
 *
 * \details Clears reset flags and reads all reset sources.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, all reset sources RCC_FLAG_INACTIVE.
 */
void It_Rcc_Set_ResetSourceClear_AllFlagsInactive( void )
{
    rcc_FlagState_t flagState = RCC_FLAG_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    for( rcc_ResetSrc_t resetSrc = RCC_RESET_SRC_PIN; RCC_RESET_SRC_CNT > resetSrc; resetSrc++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( resetSrc, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
    }
}

/**
 * \brief   Software reset is reported by reset source flags.
 *
 * \details
 * 1. Stage 0: expects reset and requests system reset (NVIC).
 * 2. Stage 1 (after reset): reads SW, IWDG and BOR reset source.
 *
 * \par Expected results
 * 1. MCU is reset, code after the request is not executed.
 * 2. SW flag active, IWDG and BOR flags inactive.
 */
void It_Rcc_Get_ResetSource_SoftwareReset_OnlySwAndPinFlags( void )
{
    const integrationTesting_Stage_t stage = IntegrationTesting_Get_Stage();

    if( 0u == stage )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );
        IntegrationTesting_Set_ResetExpected();

        Nvic_Set_SystemReset();

        TEST_FAIL_MESSAGE( "System reset did not occur" );
    }
    else
    {
        rcc_FlagState_t flagState = RCC_FLAG_INACTIVE;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_SW, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flagState );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_IWDG, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_BOR, &flagState ) );
        TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flagState );
    }
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Expected result of Rcc_Set_BackupDomainReset() on this target: the request is refused when
 *        the stack lies in the SRAM2 (erased by the backup domain reset, STM32H503: the stack is at
 *        the end of the RAM = end of the SRAM2), otherwise it succeeds.
 *
 * \return RCC_REQUEST_ERROR when the stack lies in the SRAM2, otherwise RCC_REQUEST_OK
 */
static rcc_RequestState_t ItRcc_Get_BackupDomainResetResult( void )
{
    rcc_RequestState_t expected    = RCC_REQUEST_OK;
    volatile uint32_t  stackMarker = 0u;
    const uintptr_t    stackAddr   = (uintptr_t)&stackMarker;

    if( ( SRAM2_BASE                  <= stackAddr ) &&
        ( ( SRAM2_BASE + SRAM2_SIZE ) >  stackAddr )    )
    {
        expected = RCC_REQUEST_ERROR;
    }
    else
    {
        /* The stack lies outside of the SRAM2, the reset is allowed */
    }

    return ( expected );
}


/**
 * \brief Initializes and starts the timer (free running 32-bit counter).
 *
 * \param timerFreq [in]: Counter frequency [Hz] calculated by Tim module from RCC
 */
static void It_Rcc_Init_Timer( tim_FreqHz_t timerFreq )
{
    tim_PeriphConfig_t timConfig;

    TEST_ASSERT_EQUAL( TIM_REQUEST_OK, Tim_Get_DefaultConfig( &timConfig ) );
    timConfig.PeriphId         = IT_RCC_TIM;
    timConfig.TimerFrequency   = timerFreq;
    timConfig.RefreshFrequency = 1u;

    TEST_ASSERT_EQUAL( TIM_REQUEST_OK, Tim_Init( &timConfig ) );
    TEST_ASSERT_EQUAL( TIM_REQUEST_OK, Tim_Set_Period( IT_RCC_TIM, 0xFFFFFFFFu ) );
    TEST_ASSERT_EQUAL( TIM_REQUEST_OK, Tim_Start( IT_RCC_TIM ) );
}

/**
 * \brief Returns counter of the timer.
 *
 * \return Counter value
 */
static tim_Counter_t It_Rcc_Get_TimerCounter( void )
{
    tim_Counter_t counter = 0u;

    TEST_ASSERT_EQUAL( TIM_REQUEST_OK, Tim_Get_Counter( IT_RCC_TIM, &counter ) );

    return ( counter );
}

/**
 * \brief Registers SysTick handler counting interrupts and sets 1 ms interval.
 */
static void It_Rcc_Start_SysTick( void )
{
    TEST_ASSERT_EQUAL( NVIC_REQUEST_OK, Nvic_Set_CoreIrq_Handler( NVIC_CORE_IRQ_SYSTICK, It_Rcc_SysTickHandler ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );
}

/**
 * \brief Measures duration of required count of SysTick periods by the timer.
 *
 * \param sysTickCnt [in]: Count of SysTick periods
 *
 * \return Timer counts during the SysTick periods
 */
static tim_Counter_t It_Rcc_Measure_SysTickPeriod( uint32_t sysTickCnt )
{
    tim_Counter_t startCnt = 0u;
    uint32_t      startTick = itRcc_SysTickCnt;

    /* Synchronization to SysTick interrupt */
    for( uint32_t loopIdx = 0u;
         ( IT_RCC_WAIT_LOOPS > loopIdx ) &&
         ( startTick == itRcc_SysTickCnt );
         loopIdx++ )
    {
        /* Wait for next SysTick */
    }

    startCnt  = It_Rcc_Get_TimerCounter();
    startTick = itRcc_SysTickCnt;

    for( uint32_t loopIdx = 0u;
         ( IT_RCC_WAIT_LOOPS > loopIdx ) &&
         ( ( itRcc_SysTickCnt - startTick ) < sysTickCnt );
         loopIdx++ )
    {
        /* Wait for SysTick periods */
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE( sysTickCnt, itRcc_SysTickCnt - startTick, "SysTick interrupt not running" );

    return ( It_Rcc_Get_TimerCounter() - startCnt );
}

/**
 * \brief Waits for required count of SysTick periods.
 *
 * \param sysTickCnt [in]: Count of SysTick periods
 */
static void It_Rcc_Wait_SysTicks( uint32_t sysTickCnt )
{
    const uint32_t startTick = itRcc_SysTickCnt;

    for( uint32_t loopIdx = 0u;
         ( IT_RCC_WAIT_LOOPS >  loopIdx                          ) &&
         ( sysTickCnt        > ( itRcc_SysTickCnt - startTick ) );
         loopIdx++ )
    {
        /* Wait for SysTick periods */
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE( sysTickCnt, itRcc_SysTickCnt - startTick, "SysTick interrupt not running" );
}

/**
 * \brief SysTick handler - counts interrupts.
 */
static void It_Rcc_SysTickHandler( void )
{
    itRcc_SysTickCnt++;
}

/**
 * \brief Starts LSI oscillator by RCC oscillator API.
 */
static void It_Rcc_Start_Lsi( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSI ) );
}
