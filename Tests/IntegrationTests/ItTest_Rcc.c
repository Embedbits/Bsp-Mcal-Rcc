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
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void             It_Rcc_Init_Timer           ( tim_FreqHz_t timerFreq );
static tim_Counter_t    It_Rcc_Get_TimerCounter     ( void );
static void             It_Rcc_Start_SysTick        ( void );
static void             It_Rcc_Start_Lsi            ( void );
static tim_Counter_t    It_Rcc_Measure_SysTickPeriod ( uint32_t sysTickCnt );

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
    for( uint32_t loopIdx = 0u; ( IT_RCC_WAIT_LOOPS > loopIdx ) && ( IT_RCC_LSI_PERIODS >= risingEdges ); loopIdx++ )
    {
        TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinLevel( IT_RCC_MCO2_PORT, IT_RCC_MCO2_PIN, &pinLevel ) );

        if( ( GPIO_PIN_LEVEL_LOW == prevLevel ) && ( GPIO_PIN_LEVEL_HIGH == pinLevel ) )
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
    if( 0u == IntegrationTesting_Get_Stage() )
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
    for( uint32_t loopIdx = 0u; ( IT_RCC_WAIT_LOOPS > loopIdx ) && ( startTick == itRcc_SysTickCnt ); loopIdx++ )
    {
        /* Wait for next SysTick */
    }

    startCnt  = It_Rcc_Get_TimerCounter();
    startTick = itRcc_SysTickCnt;

    for( uint32_t loopIdx = 0u; ( IT_RCC_WAIT_LOOPS > loopIdx ) && ( ( itRcc_SysTickCnt - startTick ) < sysTickCnt ); loopIdx++ )
    {
        /* Wait for SysTick periods */
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE( sysTickCnt, itRcc_SysTickCnt - startTick, "SysTick interrupt not running" );

    return ( It_Rcc_Get_TimerCounter() - startCnt );
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
