/**
 * \author Mr.Nobody
 * \file Test_Rcc.c
 * \ingroup Rcc
 * \brief Unit tests of Reset and Clock Control (RCC) module.
 *
 * All Rcc sources are compiled unchanged with real LL drivers. RCC, PWR, FLASH,
 * ICACHE and SysTick registers are emulated by RegMem, GPIO module (MCO pins) is
 * mocked by CMock.
 *
 * Oscillator / PLL enable -> ready flags, system clock switch (SW -> SWS),
 * voltage scaling ready flag and reset flags removal are emulated by HW model
 * running in background thread (utRcc_HwModel). Tests without the model check
 * timeout (error) branches.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "UtCommon.h"                       /* Common test helpers            */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Rcc_Pll.h"                        /* PLL internal API (Rcc_Set_PllConfig is not implemented) */
#include "MockGpio_Port.h"                  /* GPIO module mock (MCO pins)    */
#include "Stm32_rcc.h"                      /* RCC registers definition       */
#include "Stm32_pwr.h"                      /* PWR registers definition       */
#include "Stm32_system.h"                   /* FLASH registers definition     */
#include "Stm32_icache.h"                   /* ICACHE registers definition    */
/* ============================= TYPEDEFS =================================== */

/** Expected clock enable bit of peripheral */
typedef struct
{
    rcc_PeriphId_t      PeriphId;   /**< Peripheral identification    */
    volatile uint32_t  *EnableReg;  /**< Clock enable register        */
    uint32_t            EnableBit;  /**< Clock enable bit mask        */
}   utRcc_PeriphEnable_t;

/* ======================= FORWARD DECLARATIONS ============================= */

static void                 utRcc_HwModel           ( void );
static void                 utRcc_Mirror            ( volatile uint32_t *reg, uint32_t onMask, uint32_t rdyMask );
static void                 utRcc_Set_SysClkHsi     ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSI oscillator frequency (HSI_VALUE, without HSIDIV divider) */
#define UT_RCC_HSI_HZ                       ( 64000000u )

/** PLL1 P output frequency of default configuration: 64 MHz / 4 * 20 / 2 */
#define UT_RCC_PLL1P_DEFAULT_HZ             ( 160000000u )

/** HSE frequency of default configuration */
#define UT_RCC_HSE_DEFAULT_HZ               ( 8000000u )

/** Count of milliseconds in one second */
#define UT_RCC_MS_IN_SECOND                 ( 1000u )

/** Interval too long for 24-bit SysTick reload at 64 MHz */
#define UT_RCC_SYSTICK_TOO_LONG_MS          ( 1000u )

/** Position of AHB prescaler field in LL value (HPRE) */
#define UT_RCC_DEFAULT_PLL_M                ( 4u )

/** Default PLL multiplier */
#define UT_RCC_DEFAULT_PLL_N                ( 20u )

/** Default PLL output divider */
#define UT_RCC_DEFAULT_PLL_DIV              ( 2u )

/* ========================== LOCAL VARIABLES =============================== */

/** Sample of peripherals from every clock bus with their enable bit */
static const utRcc_PeriphEnable_t utRcc_PeriphEnableLut[] =
{
    { .PeriphId = RCC_PERIPH_FLASH, .EnableReg = &RCC->AHB1ENR,  .EnableBit = RCC_AHB1ENR_FLITFEN  },
    { .PeriphId = RCC_PERIPH_GPIOA, .EnableReg = &RCC->AHB2ENR,  .EnableBit = RCC_AHB2ENR_GPIOAEN  },
    { .PeriphId = RCC_PERIPH_TIM2,  .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_TIM2EN  },
    { .PeriphId = RCC_PERIPH_SBS,   .EnableReg = &RCC->APB3ENR,  .EnableBit = RCC_APB3ENR_SBSEN    },
    { .PeriphId = RCC_PERIPH_WWDG,  .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_WWDGEN  },
};

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    /* Stops HW model of previous test and clears registers */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );
}


void tearDown( void )
{
    (void)RegMem_Set_ModelInactive();
}

/* ========================== MODULE VERSION ================================ */

void test_Rcc_Get_ModuleVersion_ReturnsVersion( void )
{
    rcc_ModuleVersion_t version = Rcc_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Minor );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Patch );
}

/* =========================== DEFAULT CONFIG =============================== */

void test_Rcc_Get_DefaultConfig_ReturnsPll1FromHsi( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_PLL, config.SystemClockSource );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI,             config.Pll_Config[ RCC_PLL_1 ].Pll_Source );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_M, config.Pll_Config[ RCC_PLL_1 ].M_Divider );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_N, config.Pll_Config[ RCC_PLL_1 ].N_Multiplier );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ, config.HSE_Frequency_Hz );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE,         config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_DefaultConfig( NULL ) );
}

/* ========================= PERIPHERAL CLOCKS ============================== */

void test_Rcc_Set_PeriphActive_SetsOnlyOwnEnableBit( void )
{
    for( uint32_t lutIdx = 0u; ( sizeof( utRcc_PeriphEnableLut ) / sizeof( utRcc_PeriphEnableLut[ 0 ] ) ) > lutIdx; lutIdx++ )
    {
        const utRcc_PeriphEnable_t * const expected = &utRcc_PeriphEnableLut[ lutIdx ];

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( expected->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( expected->EnableBit, *expected->EnableReg );
    }
}


void test_Rcc_Set_PeriphActive_Usart1_SetsApb2EnableBit( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_PCLK2 ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


void test_Rcc_Set_PeriphActive_Usart1Hsi_SelectsClockMux( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    UT_KNOWN_DEFECT( "rcc_ClkMuxConfig uses LL_CLKSOURCE-encoded values (LL_RCC_xxx_CLKSOURCE_yyy) as raw register values - "
                     "Rcc_ClkMux_Set_ClkActive never writes CCIPRx (default check fails), error is ignored by Rcc_Set_PeriphActive, "
                     "Rcc_Get_PeriphClkSrc always fails" );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_1 | RCC_CCIPR1_USART1SEL_0, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );

    /* Any USART1 entry returns the entry of actually selected source */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1_PCLK2, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1_HSI, clkSrc );
}


void test_Rcc_PeriphActiveInactive_AllPeripherals_RoundTrip( void )
{
    for( uint32_t periphId = 0u; RCC_PERIPH_ID_CNT > periphId; periphId++ )
    {
        rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

        TEST_ASSERT_EQUAL_MESSAGE( RCC_REQUEST_OK, Rcc_Set_PeriphActive( (rcc_PeriphId_t)periphId ), "Set_PeriphActive" );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( (rcc_PeriphId_t)periphId, &state ) );
        TEST_ASSERT_EQUAL_MESSAGE( RCC_FUNCTION_ACTIVE, state, "State after activation" );

        TEST_ASSERT_EQUAL_MESSAGE( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( (rcc_PeriphId_t)periphId ), "Set_PeriphInactive" );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( (rcc_PeriphId_t)periphId, &state ) );

        /* Peripherals without clock enable bit are reported as always active */
        TEST_ASSERT_TRUE( ( RCC_FUNCTION_INACTIVE == state ) ||
                          ( RCC_PERIPH_IWDG == periphId ) ||
                          ( RCC_PERIPH_SYSTICK_HCLK_DIV8 == periphId ) ||
                          ( RCC_PERIPH_SYSTICK_LSI == periphId ) ||
                          ( RCC_PERIPH_SYSTICK_LSE == periphId ) );
    }
}


void test_Rcc_Set_PeriphInactive_ClearsOnlyOwnBit( void )
{
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_GPIOBEN, RCC->AHB2ENR );
}


void test_Rcc_Periph_InvalidArgs_ReturnError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphInactive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphState( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphState( RCC_PERIPH_GPIOA, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SleepActive( RCC_PERIPH_ID_CNT ) );
}

/* ========================== RESET AND SLEEP =============================== */

void test_Rcc_Set_ResetActiveInactive_Usart1_TogglesResetBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2RSTR_USART1RST, RCC->APB2RSTR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2RSTR );
}


void test_Rcc_Set_ResetActive_PeriphWithoutResetBit_DoesNotResetOthers( void )
{
    UT_KNOWN_DEFECT( "Rcc_Set_ResetActive/Inactive/Get_ResetState do not check RCC_UNSUPPORTED_FUNCTION (0xFF) reset mask - "
                     "reset of FLASH/SBS/IWDG/SYSTICK/RTC writes 0xFF to RSTR register (eg. resets GPDMA1/2)" );

    (void)Rcc_Set_ResetActive( RCC_PERIPH_FLASH );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB1RSTR );
}


void test_Rcc_Set_SleepActiveInactive_Usart1_TogglesLpEnableBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2LPENR_USART1LPEN, RCC->APB2LPENR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2LPENR );
}

/* ========================= BUS CLOCKS AND DIVIDERS ======================== */

void test_Rcc_Set_ClkBusDivider_WritesPrescalers( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_2, LL_RCC_GetAHBPrescaler() );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB1_DIV_4,   LL_RCC_GetAPB1Prescaler() );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT, RCC_AHB_DIVIDER_2 ) );
}


void test_Rcc_Get_ClkBusClk_HsiSysClk_AppliesDividers( void )
{
    rcc_FreqHz_t freq = 0u;

    utRcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 8u, freq );

    /* Peripheral clock follows its bus */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 8u, freq );
}


void test_Rcc_Get_PeriphClk_InvalidArgs_ReturnsError( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_ID_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2,   NULL  ) );
}


void test_Rcc_Get_ClkBusClk_HsiDividerApplied( void )
{
    rcc_FreqHz_t freq = 0u;

    UT_KNOWN_DEFECT( "Rcc_ClkSrc_Get_Hsi64Clk returns HSI_VALUE and ignores HSIDIV (reset value /2 -> 32 MHz)" );

    utRcc_Set_SysClkHsi();
    RCC->CR |= RCC_CR_HSIDIV_0;     /* HSI / 2 - reset value of HSIDIV */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
}

/* =============================== SYSTICK ================================== */

void test_Rcc_Set_SysTickInterval_1ms_ConfiguresReload( void )
{
    rcc_Time_ms_t interval = 0u;

    utRcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL_UINT32( ( UT_RCC_HSI_HZ / UT_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_BITS_HIGH( SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk, SysTick->CTRL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


void test_Rcc_Set_SysTickInterval_OutOfRange_ReturnsErrorWithoutWrite( void )
{
    utRcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( UT_RCC_SYSTICK_TOO_LONG_MS ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->CTRL );
}

/* ============================ FLASH AND POWER ============================= */

void test_Rcc_Set_FlashPrefetch_TogglesPrefetchBit( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchActive() );
    TEST_ASSERT_BITS_HIGH( FLASH_ACR_PRFTEN, FLASH->ACR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchInactive() );
    TEST_ASSERT_BITS_LOW( FLASH_ACR_PRFTEN, FLASH->ACR );
}


void test_Rcc_Set_PwrRange_RegulatorReady_WritesScale( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( utRcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE0, PWR->VOSCR & PWR_VOSCR_VOS );
}


void test_Rcc_Set_PwrRange_RegulatorNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* VOSRDY never set */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );
}


void test_Rcc_Set_FlashLatency_Pll160MHzScale0_Sets3WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );
}


void test_Rcc_Set_FlashLatency_Pll160MHzScale3_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE3;    /* Max. 100 MHz in scale 3 */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


void test_Rcc_Set_FlashLatency_HsiSysClk_Sets1WaitState( void )
{
    rcc_ConfigStruct_t config;

    UT_KNOWN_DEFECT( "Rcc_Get_ExpectedSysClkFrequency does not set retState OK for HSI/HSE system clock - "
                     "Rcc_Set_FlashLatency (and Rcc_Init) fails for HSI/HSE SYSCLK" );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, LL_FLASH_GetLatency() );     /* 64 MHz */
}


void test_Rcc_Set_FlashLatency_PllUsesPDivider( void )
{
    rcc_ConfigStruct_t config;

    UT_KNOWN_DEFECT( "Rcc_Get_ExpectedSysClkFrequency calculates SYSCLK (PLL1 P output) with R divider" );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 8u;  /* P = 2 -> SYSCLK 160 MHz, R output 40 MHz */
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );
}

/* ================================= PLL ==================================== */

void test_Rcc_Set_PllConfig_DefaultPll1_ConfiguresAndEnables( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( utRcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Set_Config( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLL1SOURCE_HSI, LL_RCC_PLL1_GetSource() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_M,   LL_RCC_PLL1_GetM() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_N,   LL_RCC_PLL1_GetN() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_DIV, LL_RCC_PLL1_GetP() );
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLINPUTRANGE_8_16, ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1RGE ) >> RCC_PLL1CFGR_PLL1RGE_Pos );
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLVCORANGE_MEDIUM, ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1VCOSEL ) >> RCC_PLL1CFGR_PLL1VCOSEL_Pos );
    TEST_ASSERT_BITS_HIGH( RCC_CR_PLL1ON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Get_Clk_OutP( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, pllClk );
}


void test_Rcc_Set_PllConfig_ReferenceOutOfRange_ReturnsErrorWithoutEnable( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].M_Divider = 1u;  /* 64 MHz reference - above 16 MHz input range */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


void test_Rcc_Set_PllConfig_InvalidArgs_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_CNT, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_1,   NULL ) );
}


void test_Rcc_Set_PllConfig_PllNotLocked_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    UT_KNOWN_DEFECT( "Rcc_Pll_Set_Config ignores result of Rcc_Pll_Set_Active - OK is returned although PLL did not lock" );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* No HW model - PLL1RDY is never set */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
}

/* ============================ CLOCK OUTPUTS =============================== */

void test_Rcc_Set_ClkOutSource_Mco1Hse_SelectsSourceAndConfiguresPin( void )
{
    gpio_Config_t expectedGpio = { 0 };

    expectedGpio.PortId         = GPIO_PORT_A;
    expectedGpio.PinId          = GPIO_PIN_ID_8;
    expectedGpio.PinMode        = GPIO_PIN_MODE_ALTERNATE;
    expectedGpio.PinPull        = GPIO_PIN_PULL_NONE;
    expectedGpio.PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH;
    expectedGpio.PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL;
    expectedGpio.PinAltFunction = GPIO_ALT_FUNC_0;
    expectedGpio.PinActiveLevel = GPIO_PIN_LEVEL_HIGH;

    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR1_MCO1SEL_1, RCC->CFGR1 & RCC_CFGR1_MCO1SEL );     /* HSE = 0b010 */
}


void test_Rcc_Set_ClkOutSource_Mco2InvalidSource_ReturnsErrorWithoutPin( void )
{
    /* No Gpio_Init expected */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSE ) );
}


void test_Rcc_Set_ClkOutSource_Mco1None_DoesNotConfigurePin( void )
{
    UT_KNOWN_DEFECT( "Rcc_ClkOut_Set_ClockSource with RCC_CLK_SOURCE_NONE configures MCO1 (PA8) / LSCO (PB2) pin "
                     "as alternate function - Rcc_Init with default config takes over PA8 and PB2" );

    /* No Gpio_Init expected - strict mock fails on the call */
    (void)Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE );
    (void)Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_NONE );
}

/* ============================ RESET SOURCE ================================ */

void test_Rcc_Get_ResetSource_ReadsFlags( void )
{
    rcc_FlagState_t flag = RCC_FLAG_INACTIVE;

    RCC->RSR = RCC_RSR_IWDGRSTF | RCC_RSR_PINRSTF;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_IWDG, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flag );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_PIN, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flag );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_BOR, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flag );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_CNT, &flag ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_IWDG, NULL ) );
}


void test_Rcc_Set_ResetSourceClear_FlagsRemoved_ReturnsOkAndReleasesRmvf( void )
{
    RCC->RSR = RCC_RSR_IWDGRSTF | RCC_RSR_PINRSTF;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( utRcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->RSR );
}


void test_Rcc_Set_ResetSourceClear_FlagsStay_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->RSR = RCC_RSR_IWDGRSTF;    /* No HW model - flags are not removed */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );
    TEST_ASSERT_BITS_LOW( RCC_RSR_RMVF, RCC->RSR );
}

/* ============================ INITIALIZATION ============================== */

void test_Rcc_Init_DefaultConfig_SysClkFromPll1( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq     = 0u;
    rcc_Time_ms_t      interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( utRcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, RCC->CFGR1 & RCC_CFGR1_SWS );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_PLL1ON, RCC->CR );
    TEST_ASSERT_BITS_HIGH( FLASH_ACR_PRFTEN, FLASH->ACR );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


void test_Rcc_Init_HseNotStarting_ReturnsErrorBeforeSysClkSwitch( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* Voltage scaling ready, but no oscillator ever becomes ready */
    PWR->VOSSR = PWR_VOSSR_VOSRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_LOW( RCC_CFGR1_SW, RCC->CFGR1 );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


void test_Rcc_Init_NullConfig_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( NULL ) );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief HW model of RCC and PWR - runs in background thread.
 *
 * Ready flags follow enable bits, SWS follows SW, regulator is always ready,
 * reset flags are removed while RMVF is set.
 */
static void utRcc_HwModel( void )
{
    utRcc_Mirror( &RCC->CR, RCC_CR_HSION,   RCC_CR_HSIRDY   );
    utRcc_Mirror( &RCC->CR, RCC_CR_HSEON,   RCC_CR_HSERDY   );
    utRcc_Mirror( &RCC->CR, RCC_CR_CSION,   RCC_CR_CSIRDY   );
    utRcc_Mirror( &RCC->CR, RCC_CR_HSI48ON, RCC_CR_HSI48RDY );
    utRcc_Mirror( &RCC->CR, RCC_CR_PLL1ON,  RCC_CR_PLL1RDY  );
    utRcc_Mirror( &RCC->CR, RCC_CR_PLL2ON,  RCC_CR_PLL2RDY  );
#if defined(RCC_CR_PLL3ON)
    utRcc_Mirror( &RCC->CR, RCC_CR_PLL3ON,  RCC_CR_PLL3RDY  );
#endif
    utRcc_Mirror( &RCC->BDCR, RCC_BDCR_LSEON, RCC_BDCR_LSERDY );
    utRcc_Mirror( &RCC->BDCR, RCC_BDCR_LSION, RCC_BDCR_LSIRDY );

    /* System clock switch status follows the request */
    const uint32_t cfgr1 = RCC->CFGR1;
    const uint32_t sws   = ( ( cfgr1 & RCC_CFGR1_SW ) >> RCC_CFGR1_SW_Pos ) << RCC_CFGR1_SWS_Pos;

    if( sws != ( cfgr1 & RCC_CFGR1_SWS ) )
    {
        (void)__atomic_and_fetch( &RCC->CFGR1, ~RCC_CFGR1_SWS, __ATOMIC_SEQ_CST );
        (void)__atomic_or_fetch( &RCC->CFGR1, sws, __ATOMIC_SEQ_CST );
    }
    else
    {
        /* Switch done */
    }

    (void)__atomic_or_fetch( &PWR->VOSSR, PWR_VOSSR_VOSRDY, __ATOMIC_SEQ_CST );

    if( 0u != ( RCC->RSR & RCC_RSR_RMVF ) )
    {
        (void)__atomic_and_fetch( &RCC->RSR, ~( RCC_RSR_PINRSTF | RCC_RSR_BORRSTF | RCC_RSR_SFTRSTF |
                                                RCC_RSR_IWDGRSTF | RCC_RSR_WWDGRSTF | RCC_RSR_LPWRRSTF ), __ATOMIC_SEQ_CST );
    }
    else
    {
        /* Flags are latched */
    }
}


/**
 * \brief Sets ready bit(s) if enable bit is set, otherwise clears them.
 *
 * Atomic bit operations do not overwrite concurrent writes of the module.
 */
static void utRcc_Mirror( volatile uint32_t *reg, uint32_t onMask, uint32_t rdyMask )
{
    if( 0u != ( *reg & onMask ) )
    {
        (void)__atomic_or_fetch( reg, rdyMask, __ATOMIC_SEQ_CST );
    }
    else
    {
        (void)__atomic_and_fetch( reg, ~rdyMask, __ATOMIC_SEQ_CST );
    }
}


/** Emulates HSI (64 MHz, HSIDIV /1) selected as system clock */
static void utRcc_Set_SysClkHsi( void )
{
    RCC->CR    = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CFGR1 = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
}
