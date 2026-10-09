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
 * running in background thread (Ut_Rcc_HwModel). Tests without the model check
 * timeout (error) branches.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "UtCommon.h"                       /* Common test helpers            */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Rcc_ClkMux.h"                     /* Clock MUX internal API (table check) */
#include "MockGpio_Port.h"                  /* GPIO module mock (MCO pins)    */
#include "Stm32_rcc.h"                      /* RCC registers definition       */
#include "Stm32_pwr.h"                      /* PWR registers definition       */
#include "Stm32_system.h"                   /* FLASH registers definition     */
#include "Stm32_icache.h"                   /* ICACHE registers definition    */
#include "Stm32_crs.h"                      /* CRS registers definition       */
/* ============================= TYPEDEFS =================================== */

/** Expected clock enable bit of peripheral */
typedef struct
{
    rcc_PeriphId_t      PeriphId;   /**< Peripheral identification    */
    volatile uint32_t  *EnableReg;  /**< Clock enable register        */
    uint32_t            EnableBit;  /**< Clock enable bit mask        */
}   utRcc_PeriphEnable_t;

/* ======================= FORWARD DECLARATIONS ============================= */

static void                 Ut_Rcc_HwModel          ( void );
static void                 Ut_Rcc_Mirror           ( volatile uint32_t *reg, uint32_t onMask, uint32_t rdyMask );
static void                 Ut_Rcc_Set_SysClkHsi    ( void );

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

/* =========================== DEFAULT CONFIG =============================== */

/**
 * \brief   Rcc_Get_DefaultConfig() fills default configuration.
 *
 * \details Reads default configuration, then calls the function with NULL.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, system clock from PLL1, PLL1 source HSI, M = 4, N = 20.
 * - HSE frequency 8 MHz, MCO1 output not used.
 * - NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_DefaultConfig_ReturnsPll1FromHsi( void )
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

/**
 * \brief   Rcc_Set_PeriphActive() sets only the clock enable bit of the peripheral.
 *
 * \details Enables clock of a sample peripheral from every bus (FLASH - AHB1,
 *          GPIOA - AHB2, TIM2 / WWDG - APB1L, SBS - APB3), registers are cleared
 *          before every peripheral.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, enable register contains only the enable bit of the peripheral.
 */
void Ut_Rcc_Set_PeriphActive_SetsOnlyOwnEnableBit( void )
{
    for( uint32_t lutIdx = 0u; ( sizeof( utRcc_PeriphEnableLut ) / sizeof( utRcc_PeriphEnableLut[ 0 ] ) ) > lutIdx; lutIdx++ )
    {
        const utRcc_PeriphEnable_t * const expected = &utRcc_PeriphEnableLut[ lutIdx ];

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( expected->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( expected->EnableBit, *expected->EnableReg );
    }
}


/**
 * \brief   Rcc_Set_PeriphActive() enables USART1 clock on APB2.
 *
 * \details Enables USART1 with PCLK2 kernel clock.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, APB2ENR.USART1EN is set.
 */
void Ut_Rcc_Set_PeriphActive_Usart1_SetsApb2EnableBit( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_PCLK2 ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphActive() selects kernel clock of the peripheral.
 *
 * \details Enables USART1 with HSI kernel clock, then reads clock source with
 *          USART1 / PCLK2 identification.
 *
 * \par Expected results
 * - CCIPR1.USART1SEL = HSI (0b011), APB2ENR.USART1EN is set.
 * - Any USART1 identification returns the selected source RCC_PERIPH_USART1_HSI.
 */
void Ut_Rcc_Set_PeriphActive_Usart1Hsi_SelectsClockMux( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSION, RCC->CR );     /* Kernel clock oscillator started */

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_1 | RCC_CCIPR1_USART1SEL_0, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );

    /* Any USART1 entry returns the entry of actually selected source */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1_PCLK2, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1_HSI, clkSrc );
}


/**
 * \brief   Rcc_Set_PeriphActive() reports clock MUX set to other source (AB#414).
 *
 * \details USART1 clock MUX is preset to HSI (not default source), then USART1 is
 *          enabled with PLL2 Q kernel clock.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, CCIPR1.USART1SEL stays HSI.
 * - APB2ENR.USART1EN is not set.
 */
void Ut_Rcc_Set_PeriphActive_MuxSetToOtherSource_ReturnsErrorWithoutEnable( void )
{
    RCC->CCIPR1 = RCC_CCIPR1_USART1SEL_1 | RCC_CCIPR1_USART1SEL_0;    /* USART1SEL = HSI */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_PLL2Q ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_1 | RCC_CCIPR1_USART1SEL_0, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_LOW( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() returns source selected by reset value of the MUX.
 *
 * \details Reads clock sources of USART1 and I2C1 (LL_CLKSOURCE() encoded records)
 *          with cleared CCIPR registers.
 *
 * \par Expected results
 * - USART1: RCC_PERIPH_USART1_PCLK2, I2C1: RCC_PERIPH_I2C1_PCLK1.
 */
void Ut_Rcc_Get_PeriphClkSrc_EncodedMuxDefault_ReturnsDefaultSource( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    /* CCIPR1.USART1SEL = 0 (PCLK2), CCIPR4.I2C1SEL = 0 (PCLK1) - LL_CLKSOURCE() encoded records */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1_HSI, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1_PCLK2, clkSrc );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_I2C1_HSI, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_I2C1_PCLK1, clkSrc );
}


/**
 * \brief   I2C1 HSI kernel clock is selected and read back.
 *
 * \details Enables I2C1 with HSI kernel clock, reads the source, then enables it
 *          again with the same source.
 *
 * \par Expected results
 * - CCIPR4.I2C1SEL = HSI, source reads back RCC_PERIPH_I2C1_HSI.
 * - Repeated request keeps the selection.
 */
void Ut_Rcc_Set_PeriphActive_I2c1Hsi_SelectsClockMuxAndSourceReadBack( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR4_I2C1SEL_1, RCC->CCIPR4 & RCC_CCIPR4_I2C1SEL );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_I2C1_PCLK1, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_I2C1_HSI, clkSrc );

    /* Repeated request of the selected source keeps the selection */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR4_I2C1SEL_1, RCC->CCIPR4 & RCC_CCIPR4_I2C1SEL );
}


/**
 * \brief   ADC HSI kernel clock is selected and read back (raw MUX value record).
 *
 * \details Enables ADC with HSI kernel clock and reads the source.
 *
 * \par Expected results
 * - CCIPR5.ADCDACSEL = HSI (0b100), source reads back RCC_PERIPH_ADC_HSI.
 */
void Ut_Rcc_Set_PeriphActive_AdcHsi_RawMuxSelectedAndSourceReadBack( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    /* ADCDACSEL records hold raw field values */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR5_ADCDACSEL_2, RCC->CCIPR5 & RCC_CCIPR5_ADCDACSEL );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ADC_HCLK, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_ADC_HSI, clkSrc );
}


/**
 * \brief   Rcc_Set_PeriphActive() starts internal oscillator of the kernel clock.
 *
 * \details Enables RNG with HSI48 kernel clock, HSI48 is off (HW model sets ready flag).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSI48 started (CR.HSI48ON), CCIPR5.RNGSEL = HSI48, AHB2ENR.RNGEN set.
 */
void Ut_Rcc_Set_PeriphActive_RngHsi48_StartsOscillator( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSI48ON, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RNG_CLKSOURCE_HSI48, RCC->CCIPR5 & RCC_CCIPR5_RNGSEL );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphActive() keeps running oscillator.
 *
 * \details HSI48 is preset as running (ON + RDY), no HW model. Enables RNG with
 *          HSI48 kernel clock.
 *
 * \par Expected results
 * - RCC_REQUEST_OK (no oscillator activation wait), HSI48 still on, AHB2ENR.RNGEN set.
 */
void Ut_Rcc_Set_PeriphActive_OscillatorRunning_KeepsOscillator( void )
{
    RCC->CR = RCC_CR_HSI48ON | RCC_CR_HSI48RDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSI48ON | RCC_CR_HSI48RDY, RCC->CR );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphActive() reports oscillator start failure.
 *
 * \details HSI48 is off, no HW model - ready flag is never set.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, peripheral clock not enabled.
 */
void Ut_Rcc_Set_PeriphActive_OscillatorNotReady_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_BITS_LOW( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphActive() does not start external oscillator.
 *
 * \details Enables RNG with LSE kernel clock, no HW model.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, BDCR.LSEON not set, AHB2ENR.RNGEN set.
 */
void Ut_Rcc_Set_PeriphActive_ExternalSource_NotStarted( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_LSE ) );

    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );
}


/**
 * \brief   Clock MUX table consistency check passes.
 *
 * \details Calls Rcc_ClkMux_Init(), which checks all records of the clock MUX table.
 *
 * \par Expected results
 * - RCC_REQUEST_OK.
 */
void Ut_Rcc_ClkMux_Init_AllRecords_Valid( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Init() );
}


/**
 * \brief   All peripherals can be activated and deactivated.
 *
 * \details For every peripheral identification enables the clock, reads the state,
 *          disables the clock and reads the state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK for all calls, state active after enabling.
 * - State inactive after disabling, except peripherals without clock enable bit
 *   (IWDG, SysTick sources) reported as always active.
 */
void Ut_Rcc_PeriphActiveInactive_AllPeripherals_RoundTrip( void )
{
    /* Internal oscillators of kernel clocks are started by the HW model */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

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

        /* Kernel clock MUX is not released by Rcc_Set_PeriphInactive (AB#416) -
         * reset values are restored, so next source of the peripheral can be selected */
        RCC->CCIPR1 = 0u;
        RCC->CCIPR2 = 0u;
        RCC->CCIPR3 = 0u;
        RCC->CCIPR4 = 0u;
        RCC->CCIPR5 = 0u;
        (void)__atomic_and_fetch( &RCC->BDCR, ~RCC_BDCR_RTCSEL, __ATOMIC_SEQ_CST );
    }
}


/**
 * \brief   Rcc_Set_PeriphInactive() clears only the clock enable bit of the peripheral.
 *
 * \details Presets GPIOA and GPIOB enable bits and disables GPIOA.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, only GPIOB enable bit stays set.
 */
void Ut_Rcc_Set_PeriphInactive_ClearsOnlyOwnBit( void )
{
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_GPIOBEN, RCC->AHB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphInactive() releases the kernel clock multiplexer.
 *
 * \details USART1 enabled with HSI kernel clock, disabled, then enabled with CSI kernel
 *          clock.
 *
 * \par Expected results
 * - Disable: APB2ENR.USART1EN cleared, CCIPR1.USART1SEL back to the default (PCLK2).
 * - Second enable with CSI: RCC_REQUEST_OK, CCIPR1.USART1SEL = CSI (0b100).
 */
void Ut_Rcc_Set_PeriphInactive_KernelClockMux_Released( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_1 | RCC_CCIPR1_USART1SEL_0, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_BITS_LOW( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_CSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_2, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphInactive() keeps a multiplexer shared with an enabled block.
 *
 * \details ADC and DAC enabled with SYSCLK kernel clock (common field ADCDACSEL), DAC
 *          disabled, then ADC disabled.
 *
 * \par Expected results
 * - DAC disabled: RCC_REQUEST_OK, ADCDACSEL stays SYSCLK (ADC still enabled).
 * - ADC disabled: ADCDACSEL back to the default (HCLK).
 */
void Ut_Rcc_Set_PeriphInactive_SharedMux_KeptWhileOtherBlockEnabled( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC_SYSCLK ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_DAC_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_ADCDAC_CLKSOURCE_SYSCLK & RCC_CCIPR5_ADCDACSEL, RCC->CCIPR5 & RCC_CCIPR5_ADCDACSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_DAC_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_ADCDAC_CLKSOURCE_SYSCLK & RCC_CCIPR5_ADCDACSEL, RCC->CCIPR5 & RCC_CCIPR5_ADCDACSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_ADC_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR5 & RCC_CCIPR5_ADCDACSEL );
}


/**
 * \brief   Rcc_Set_PeriphInactive() keeps the RTC clock selection.
 *
 * \details RTC enabled with LSI clock (RTCSEL is write-once until backup domain reset),
 *          then disabled.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, BDCR.RTCSEL stays LSI.
 */
void Ut_Rcc_Set_PeriphInactive_Rtc_ClockSelectionKept( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   Peripheral functions reject invalid arguments.
 *
 * \details Calls enable, disable, state, reset and sleep functions with peripheral
 *          out of range and state getter with NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 */
void Ut_Rcc_Periph_InvalidArgs_ReturnError( void )
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

/**
 * \brief   Peripheral reset of USART1 is set, read back and released.
 *
 * \details Activates USART1 reset, reads the reset state and deactivates the reset.
 *
 * \par Expected results
 * - APB2RSTR = USART1RST, reset state active.
 * - APB2RSTR = 0 after release.
 */
void Ut_Rcc_Set_ResetActiveInactive_Usart1_TogglesResetBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2RSTR_USART1RST, RCC->APB2RSTR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2RSTR );
}


/**
 * \brief   Reset of peripheral without reset bit does not reset other peripherals (AB#415).
 *
 * \details Activates and deactivates reset of FLASH and IWDG (no reset bit, AHB1
 *          block), reads reset state of FLASH with reset of GPDMA1 active.
 *
 * \par Expected results
 * - Rcc_Set_ResetActive(): RCC_REQUEST_ERROR, AHB1RSTR stays 0.
 * - Rcc_Set_ResetInactive(): RCC_REQUEST_OK, active reset of GPDMA1 is kept.
 * - Rcc_Get_ResetState(): RCC_REQUEST_OK, FLASH reset inactive.
 */
void Ut_Rcc_Set_ResetActive_PeriphWithoutResetBit_DoesNotResetOthers( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_IWDG ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB1RSTR );

    RCC->AHB1RSTR = RCC_AHB1RSTR_GPDMA1RST;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1RSTR_GPDMA1RST, RCC->AHB1RSTR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_FLASH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Sleep mode clock of USART1 is enabled, read back and disabled.
 *
 * \details Activates USART1 clock in sleep mode, reads the sleep state and deactivates
 *          it.
 *
 * \par Expected results
 * - APB2LPENR = USART1LPEN, sleep state active.
 * - APB2LPENR = 0 after deactivation.
 */
void Ut_Rcc_Set_SleepActiveInactive_Usart1_TogglesLpEnableBit( void )
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

/**
 * \brief   Rcc_Set_ClkBusDivider() writes bus prescalers.
 *
 * \details Sets AHB divider 2 and APB1 divider 4, then calls the function with bus
 *          out of range.
 *
 * \par Expected results
 * - HPRE = /2, PPRE1 = /4.
 * - Bus out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_ClkBusDivider_WritesPrescalers( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_2, LL_RCC_GetAHBPrescaler() );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB1_DIV_4,   LL_RCC_GetAPB1Prescaler() );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT, RCC_AHB_DIVIDER_2 ) );
}


/**
 * \brief   Bus clocks are calculated from system clock and dividers.
 *
 * \details System clock HSI 64 MHz, AHB divider 2, APB1 divider 4. Reads AHB2 and
 *          APB1 bus clocks and WWDG (APB1) peripheral clock.
 *
 * \par Expected results
 * - AHB2 = 32 MHz, APB1 = 8 MHz, WWDG = 8 MHz.
 */
void Ut_Rcc_Get_ClkBusClk_HsiSysClk_AppliesDividers( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 8u, freq );

    /* Peripheral clock follows its bus */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_WWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 8u, freq );
}


/**
 * \brief   Timer kernel clock equals PCLK when APB is not divided.
 *
 * \details System clock HSI 64 MHz, all dividers 1. Reads TIM2 (APB1) and TIM1 (APB2)
 *          clock.
 *
 * \par Expected results
 * - TIM2 = TIM1 = 64 MHz.
 */
void Ut_Rcc_Get_PeriphClk_TimerApbNotDivided_EqualsPclk( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );
}


/**
 * \brief   Timer kernel clock is 2 x PCLK when APB is divided (TIMPRE = 0).
 *
 * \details System clock HSI 64 MHz, APB1 divider 4, APB2 divider 2. Reads TIM2 and
 *          TIM1 clock.
 *
 * \par Expected results
 * - TIM2 = 2 x 16 MHz = 32 MHz, TIM1 = 2 x 32 MHz = 64 MHz.
 */
void Ut_Rcc_Get_PeriphClk_TimerApbDivided_TwicePclk( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB2_DIVIDER_2 ) );

    /* TIMPRE = 0: 2 x PCLK */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );
}


/**
 * \brief   Timer kernel clock with TIMPRE = 1.
 *
 * \details System clock HSI 64 MHz, TIMPRE set. Reads TIM2 clock with APB1 divider 4
 *          and 16.
 *
 * \par Expected results
 * - APB1 / 4: TIM2 = HCLK = 64 MHz.
 * - APB1 / 16: TIM2 = 4 x PCLK = 16 MHz.
 */
void Ut_Rcc_Get_PeriphClk_TimerTimpre_HclkOrFourTimesPclk( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CFGR1 |= RCC_CFGR1_TIMPRE;

    /* TIMPRE = 1, APB1 / 4: HCLK */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );

    /* TIMPRE = 1, APB1 / 16: 4 x PCLK */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_16 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, freq );
}


/**
 * \brief   Rcc_Get_PeriphClk() rejects invalid arguments.
 *
 * \details Calls the function with peripheral out of range and with NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases.
 */
void Ut_Rcc_Get_PeriphClk_InvalidArgs_ReturnsError( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_ID_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2,   NULL  ) );
}


/**
 * \brief   HSI divider is applied to bus clocks.
 *
 * \details System clock HSI with HSIDIV / 2 (reset value). Reads AHB1 clock.
 *
 * \par Expected results
 * - AHB1 = 32 MHz.
 */
void Ut_Rcc_Get_ClkBusClk_HsiDividerApplied( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CR |= RCC_CR_HSIDIV_0;     /* HSI / 2 - reset value of HSIDIV */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
}

/* =============================== SYSTICK ================================== */

/**
 * \brief   Rcc_Set_SysTickInterval() configures 1 ms SysTick.
 *
 * \details System clock HSI 64 MHz. Sets 1 ms interval and reads it back.
 *
 * \par Expected results
 * - LOAD = 63999, CTRL: counter enabled, interrupt enabled, processor clock.
 * - Interval reads back 1 ms.
 */
void Ut_Rcc_Set_SysTickInterval_1ms_ConfiguresReload( void )
{
    rcc_Time_ms_t interval = 0u;

    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL_UINT32( ( UT_RCC_HSI_HZ / UT_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_BITS_HIGH( SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_CLKSOURCE_Msk, SysTick->CTRL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Set_SysTickInterval() rejects interval out of range.
 *
 * \details System clock HSI 64 MHz. Sets interval 0 ms and 1000 ms (reload above
 *          24 bits).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases, SysTick LOAD and CTRL stay 0.
 */
void Ut_Rcc_Set_SysTickInterval_OutOfRange_ReturnsErrorWithoutWrite( void )
{
    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( UT_RCC_SYSTICK_TOO_LONG_MS ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->CTRL );
}

/* ============================ FLASH AND POWER ============================= */

/**
 * \brief   Flash prefetch is enabled and disabled.
 *
 * \details Calls Rcc_Set_FlashPrefetchActive() and Rcc_Set_FlashPrefetchInactive().
 *
 * \par Expected results
 * - FLASH_ACR.PRFTEN set, then cleared.
 */
void Ut_Rcc_Set_FlashPrefetch_TogglesPrefetchBit( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchActive() );
    TEST_ASSERT_BITS_HIGH( FLASH_ACR_PRFTEN, FLASH->ACR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchInactive() );
    TEST_ASSERT_BITS_LOW( FLASH_ACR_PRFTEN, FLASH->ACR );
}


/**
 * \brief   Rcc_Set_PwrRange() sets voltage scaling for default configuration.
 *
 * \details HW model sets regulator ready flag. Sets power range of default
 *          configuration (PLL1 160 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_VOSCR.VOS = scale 0.
 */
void Ut_Rcc_Set_PwrRange_RegulatorReady_WritesScale( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE0, PWR->VOSCR & PWR_VOSCR_VOS );
}


/**
 * \brief   Rcc_Set_PwrRange() reports regulator not ready.
 *
 * \details Without HW model (VOSRDY never set) sets power range of default
 *          configuration, then calls the function with NULL.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases (timeout, NULL pointer).
 */
void Ut_Rcc_Set_PwrRange_RegulatorNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* VOSRDY never set */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );
}


/**
 * \brief   Flash latency for PLL1 160 MHz in voltage scale 0.
 *
 * \details Sets flash latency of default configuration with VOS scale 0.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 3 wait states.
 */
void Ut_Rcc_Set_FlashLatency_Pll160MHzScale0_Sets3WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency rejects frequency not allowed in voltage scale.
 *
 * \details Sets flash latency of default configuration (160 MHz) with VOS scale 3
 *          (maximum 100 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, latency stays 0 wait states.
 */
void Ut_Rcc_Set_FlashLatency_Pll160MHzScale3_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE3;    /* Max. 100 MHz in scale 3 */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency for HSI 64 MHz system clock.
 *
 * \details System clock source HSI (HSIDIV / 1), VOS scale 0.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 1 wait state.
 */
void Ut_Rcc_Set_FlashLatency_HsiSysClk_Sets1WaitState( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, LL_FLASH_GetLatency() );     /* 64 MHz */
}


/**
 * \brief   Flash latency respects HSI divider.
 *
 * \details System clock source HSI with HSIDIV / 2 (32 MHz), VOS scale 0.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 0 wait states.
 */
void Ut_Rcc_Set_FlashLatency_HsiDiv2SysClk_Sets0WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;
    RCC->CR    = RCC_CR_HSIDIV_0;   /* HSI / 2 = 32 MHz */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency for HSE system clock is calculated from HSE frequency.
 *
 * \details System clock source HSE 50 MHz, VOS scale 0.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 1 wait state (42 MHz < 50 MHz <= 84 MHz).
 */
void Ut_Rcc_Set_FlashLatency_HseSysClk_LatencyFromHseFrequency( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = 50000000u;
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, LL_FLASH_GetLatency() );     /* 42 < 50 MHz <= 84 */
}


/**
 * \brief   Flash latency for PLL system clock uses P output.
 *
 * \details Default configuration with PLL1 R divider 8 (R output 40 MHz, P output
 *          160 MHz), VOS scale 0.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 3 wait states (P output frequency).
 */
void Ut_Rcc_Set_FlashLatency_PllUsesPDivider( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 8u;  /* P = 2 -> SYSCLK 160 MHz, R output 40 MHz */
    PWR->VOSCR = LL_PWR_REGU_VOLTAGE_SCALE0;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );
}

/**
 * \brief   Flash latency follows the wait state table of every voltage scale.
 *
 * \details For every voltage scale (VOS 0 - 3) and every wait state band the system clock
 *          source HSE with the upper limit of the band and with the limit + 1 Hz is
 *          configured and the flash latency is calculated. Limits of the bands (RM0481):
 *          VOS0 42 / 84 / 126 / 168 / 210 / 250 MHz, VOS1 34 / 68 / 102 / 136 / 170 / 200 MHz,
 *          VOS2 30 / 60 / 90 / 120 / 150 MHz, VOS3 20 / 40 / 60 / 80 / 100 MHz.
 *
 * \par Expected results
 * - Limit of band n: RCC_REQUEST_OK, n wait states.
 * - Limit + 1 Hz: RCC_REQUEST_OK, n + 1 wait states, behind the last band RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_FlashLatency_AllScales_BandLimits( void )
{
    const struct
    {
        uint32_t     Scale;
        uint32_t     BandCnt;
        rcc_FreqHz_t Limit[ 6u ];
    }   scaleLut[] =
    {
        { LL_PWR_REGU_VOLTAGE_SCALE0, 6u, { 42000000u, 84000000u, 126000000u, 168000000u, 210000000u, 250000000u } },
        { LL_PWR_REGU_VOLTAGE_SCALE1, 6u, { 34000000u, 68000000u, 102000000u, 136000000u, 170000000u, 200000000u } },
        { LL_PWR_REGU_VOLTAGE_SCALE2, 5u, { 30000000u, 60000000u,  90000000u, 120000000u, 150000000u,         0u } },
        { LL_PWR_REGU_VOLTAGE_SCALE3, 5u, { 20000000u, 40000000u,  60000000u,  80000000u, 100000000u,         0u } },
    };
    const uint32_t latencyLut[ 6u ] =
    {
        LL_FLASH_LATENCY_0, LL_FLASH_LATENCY_1, LL_FLASH_LATENCY_2, LL_FLASH_LATENCY_3, LL_FLASH_LATENCY_4, LL_FLASH_LATENCY_5
    };
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;

    for( uint32_t scaleIdx = 0u; ( sizeof( scaleLut ) / sizeof( scaleLut[ 0u ] ) ) > scaleIdx; scaleIdx++ )
    {
        PWR->VOSCR = scaleLut[ scaleIdx ].Scale;

        for( uint32_t band = 0u; scaleLut[ scaleIdx ].BandCnt > band; band++ )
        {
            config.HSE_Frequency_Hz = scaleLut[ scaleIdx ].Limit[ band ];
            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
            TEST_ASSERT_EQUAL_HEX32_MESSAGE( latencyLut[ band ], LL_FLASH_GetLatency(), "Upper limit of the band" );

            config.HSE_Frequency_Hz = scaleLut[ scaleIdx ].Limit[ band ] + 1u;

            if( ( scaleLut[ scaleIdx ].BandCnt - 1u ) > band )
            {
                TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
                TEST_ASSERT_EQUAL_HEX32_MESSAGE( latencyLut[ band + 1u ], LL_FLASH_GetLatency(), "Limit + 1 Hz" );
            }
            else
            {
                TEST_ASSERT_EQUAL_MESSAGE( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ), "Behind the last band" );
            }
        }
    }
}

/* ================================= PLL ==================================== */

/**
 * \brief   Rcc_Set_PllConfig() configures and enables PLL1.
 *
 * \details HW model sets ready flags. Configures PLL1 with default configuration
 *          and reads P output frequency.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source HSI, M = 4, N = 20, P = 2.
 * - Input range 8 - 16 MHz, VCO range medium, PLL1ON set.
 * - P output 160 MHz.
 */
void Ut_Rcc_Set_PllConfig_DefaultPll1_ConfiguresAndEnables( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLL1SOURCE_HSI, LL_RCC_PLL1_GetSource() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_M,   LL_RCC_PLL1_GetM() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_N,   LL_RCC_PLL1_GetN() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_DIV, LL_RCC_PLL1_GetP() );
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLINPUTRANGE_8_16, ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1RGE ) >> RCC_PLL1CFGR_PLL1RGE_Pos );
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLVCORANGE_MEDIUM, ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1VCOSEL ) >> RCC_PLL1CFGR_PLL1VCOSEL_Pos );
    TEST_ASSERT_BITS_HIGH( RCC_CR_PLL1ON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, pllClk );
}


/**
 * \brief   PLL1 Q and R output frequencies are returned (AB#413).
 *
 * \details HW model sets ready flags. Configures PLL1 with default configuration
 *          (Q = R = 2) and reads Q and R output frequencies into variables preset
 *          to 0. Reads kernel clock of RNG clocked by PLL1 Q.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Q and R output 160 MHz (VCO 320 MHz / 2).
 * - RNG with PLL1 Q kernel clock reports 160 MHz.
 */
void Ut_Rcc_Get_PllClk_OutQR_DefaultPll1_ReturnsFrequency( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, pllClk );

    pllClk = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, pllClk );

    pllClk = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLL1Q, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, pllClk );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects PLL reference out of range.
 *
 * \details Configures PLL1 with M divider 1 (reference 64 MHz, above 16 MHz input
 *          range).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL1 is not enabled.
 */
void Ut_Rcc_Set_PllConfig_ReferenceOutOfRange_ReturnsErrorWithoutEnable( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].M_Divider = 1u;  /* 64 MHz reference - above 16 MHz input range */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects invalid arguments.
 *
 * \details Calls the function with PLL out of range and with NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases.
 */
void Ut_Rcc_Set_PllConfig_InvalidArgs_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_CNT, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1,   NULL ) );
}


/**
 * \brief   Rcc_Set_PllConfig() reports PLL not locked.
 *
 * \details Without HW model (PLL1RDY never set) configures PLL1 with default
 *          configuration.
 * \note    Bug AB#733: result of the PLL activation was ignored - OK was returned
 *          although the PLL did not lock.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PllConfig_PllNotLocked_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* No HW model - PLL1RDY is never set */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
}


/**
 * \brief   PLL source and state are set and read back.
 *
 * \details HW model sets ready flags. Sets PLL2 source HSE, enables and disables
 *          PLL2.
 *
 * \par Expected results
 * - Source reads back HSE.
 * - State active after enabling, inactive after disabling.
 */
void Ut_Rcc_Set_PllActive_StateAndSourceReadBack( void )
{
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_PllClkSrc_t     source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_2, RCC_PLL_SRC_HSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_2, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSE, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/* ============================= OSCILLATORS ================================ */

/**
 * \brief   All oscillators are enabled and disabled.
 *
 * \details HW model sets ready flags. Enables, reads state, disables and reads state
 *          of every oscillator, then calls the functions with oscillator out of range.
 *
 * \par Expected results
 * - State active after enabling, inactive after disabling.
 * - Oscillator out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_OscActive_AllOscillators_StateReadBack( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    for( rcc_OscId_t oscId = RCC_OSC_HSI64; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( RCC_OSC_CNT, &state ) );
}


/**
 * \brief   HSI divider is set and applied to system clock.
 *
 * \details System clock HSI 64 MHz, divider ready flag preset. Sets HSI divider 4,
 *          reads the divider and AHB1 clock.
 *
 * \par Expected results
 * - CR.HSIDIV = /4, divider reads back 4.
 * - AHB1 = SystemCoreClock = 16 MHz.
 */
void Ut_Rcc_Set_OscDiv_Hsi64_DividerAppliedAndReported( void )
{
    rcc_OscDiv_t oscDiv = 0u;
    rcc_FreqHz_t freq   = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CR |= RCC_CR_HSIDIVF;      /* Divider ready flag - set by HW after divider change */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_HSI64, 4u ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_HSI_DIV_4, RCC->CR & RCC_CR_HSIDIV );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_HSI64, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, oscDiv );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, freq );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, SystemCoreClock );
}


/**
 * \brief   Oscillator divider rejects invalid values.
 *
 * \details
 * 1. Sets HSI divider 3 (not supported).
 * 2. Sets CSI divider 2 and 1 (CSI without divider), reads LSI divider.
 * 3. Calls setter / getter with oscillator out of range and getter with NULL.
 *
 * \par Expected results
 * 1. RCC_REQUEST_ERROR, HSIDIV stays 0.
 * 2. CSI divider 2: RCC_REQUEST_ERROR, divider 1: RCC_REQUEST_OK, LSI divider 1.
 * 3. RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_OscDiv_InvalidDivider_ReturnsErrorWithoutChange( void )
{
    rcc_OscDiv_t oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_HSI64, 3u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSIDIV );

    /* Oscillators without divider */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_CSI, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_OscDiv( RCC_OSC_CSI, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Get_OscDiv( RCC_OSC_LSI, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, oscDiv );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_CNT, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_CNT, &oscDiv ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_HSI64, NULL ) );
}

/* ========================== POWER SUPPLY VALIDITY ========================= */

/**
 * \brief   VDDUSB supply is validated and invalidated.
 *
 * \details Reads the state of the reset configuration, validates the supply, reads
 *          the state, invalidates the supply and reads the state again.
 *
 * \par Expected results
 * - Inactive at reset (PWR_USBSCR.USB33SV = 0).
 * - After validation: RCC_REQUEST_OK, USB33SV set, state active.
 * - After invalidation: RCC_REQUEST_OK, USB33SV cleared, state inactive.
 */
void Ut_Rcc_Set_PwrSupplyActive_Vddusb_ValidityBitFollowsRequest( void )
{
#if defined(PWR_USBSCR_USB33SV)
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_VDDUSB, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrSupplyActive( RCC_PWR_SUPPLY_VDDUSB ) );
    TEST_ASSERT_BITS_HIGH( PWR_USBSCR_USB33SV, PWR->USBSCR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_VDDUSB, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrSupplyInactive( RCC_PWR_SUPPLY_VDDUSB ) );
    TEST_ASSERT_BITS_LOW( PWR_USBSCR_USB33SV, PWR->USBSCR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_VDDUSB, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
#else
    TEST_IGNORE_MESSAGE( "VDDUSB supply validity is not available on this MCU" );
#endif /* PWR_USBSCR_USB33SV */
}


/**
 * \brief   Power supply functions reject invalid arguments.
 *
 * \details Calls the functions with supply out of range and the getter with NULL.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, no register is changed.
 */
void Ut_Rcc_PwrSupply_InvalidArguments_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrSupplyActive( RCC_PWR_SUPPLY_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrSupplyInactive( RCC_PWR_SUPPLY_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_CNT, &state ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->VOSCR );

#if defined(PWR_USBSCR_USB33SV)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_VDDUSB, NULL ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->USBSCR );
#endif /* PWR_USBSCR_USB33SV */
}

/* ======================== HSI48 AUTOMATIC TRIMMING ======================== */

/**
 * \brief   Automatic trimming of HSI48 by USB start of frame is configured.
 *
 * \details HW model sets oscillator ready flag. Activates the trimming with USB SOF
 *          synchronization (1 kHz), reads the state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, CRS clock and HSI48 enabled.
 * - CRS_CFGR: RELOAD = 47999 (48 MHz / 1 kHz - 1), FELIM = 0x22, SYNCDIV = /1, USB SOF source,
 *   rising edge. CRS_CR: TRIM = 0x20, AUTOTRIMEN and CEN set.
 * - State active.
 */
void Ut_Rcc_Set_Hsi48TrimActive_UsbSof_ConfiguresCrs( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_Hsi48TrimActive( RCC_HSI48_TRIM_SRC_USB_SOF ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB1LENR_CRSEN, RCC->APB1LENR );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSI48ON, RCC->CR );

    TEST_ASSERT_EQUAL_UINT32( 47999u, CRS->CFGR & CRS_CFGR_RELOAD );
    TEST_ASSERT_EQUAL_UINT32( LL_CRS_ERRORLIMIT_DEFAULT, ( CRS->CFGR & CRS_CFGR_FELIM ) >> CRS_CFGR_FELIM_Pos );
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_DIV_1, CRS->CFGR & CRS_CFGR_SYNCDIV );
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_POLARITY_RISING, CRS->CFGR & CRS_CFGR_SYNCPOL );
#if defined(USB_DRD_FS)
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_SOURCE_USB, CRS->CFGR & CRS_CFGR_SYNCSRC );
#else
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_SOURCE_OTG_FS, CRS->CFGR & CRS_CFGR_SYNCSRC );
#endif
    TEST_ASSERT_EQUAL_UINT32( LL_CRS_HSI48CALIBRATION_DEFAULT, ( CRS->CR & CRS_CR_TRIM ) >> CRS_CR_TRIM_Pos );
    TEST_ASSERT_BITS_HIGH( CRS_CR_AUTOTRIMEN | CRS_CR_CEN, CRS->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_Hsi48TrimState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Automatic trimming of HSI48 by LSE is configured.
 *
 * \details HW model sets oscillator ready flag. Activates the trimming with LSE
 *          synchronization (32.768 kHz).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, CRS_CFGR: RELOAD = 1463 (48 MHz / 32768 Hz - 1), LSE source.
 */
void Ut_Rcc_Set_Hsi48TrimActive_Lse_ReloadForLseFrequency( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_Hsi48TrimActive( RCC_HSI48_TRIM_SRC_LSE ) );

    TEST_ASSERT_EQUAL_UINT32( 1463u, CRS->CFGR & CRS_CFGR_RELOAD );
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_SOURCE_LSE, CRS->CFGR & CRS_CFGR_SYNCSRC );
    TEST_ASSERT_BITS_HIGH( CRS_CR_AUTOTRIMEN | CRS_CR_CEN, CRS->CR );
}


/**
 * \brief   Automatic trimming of HSI48 rejects invalid synchronization source.
 *
 * \details Activates the trimming with source out of range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, CRS registers and the CRS clock are not changed.
 */
void Ut_Rcc_Set_Hsi48TrimActive_InvalidSource_ReturnsErrorWithoutChange( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_Hsi48TrimActive( RCC_HSI48_TRIM_SRC_CNT ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, CRS->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, CRS->CFGR );
    TEST_ASSERT_BITS_LOW( RCC_APB1LENR_CRSEN, RCC->APB1LENR );
}


/**
 * \brief   Automatic trimming of HSI48 reports oscillator that does not start.
 *
 * \details Activates the trimming without HW model - HSI48 ready flag is never set.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, trimming is not started (CRS_CR.AUTOTRIMEN and CEN cleared).
 */
void Ut_Rcc_Set_Hsi48TrimActive_OscillatorNotReady_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_Hsi48TrimActive( RCC_HSI48_TRIM_SRC_USB_SOF ) );

    TEST_ASSERT_BITS_LOW( CRS_CR_AUTOTRIMEN | CRS_CR_CEN, CRS->CR );
}


/**
 * \brief   Automatic trimming of HSI48 is stopped.
 *
 * \details HW model sets oscillator ready flag. Activates the trimming, deactivates it
 *          and reads the state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, CRS_CR.AUTOTRIMEN and CEN cleared, CRS clock disabled, state inactive.
 */
void Ut_Rcc_Set_Hsi48TrimInactive_AfterActive_StopsTrimmingAndClock( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_Hsi48TrimActive( RCC_HSI48_TRIM_SRC_USB_SOF ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_Hsi48TrimInactive() );

    TEST_ASSERT_BITS_LOW( CRS_CR_AUTOTRIMEN | CRS_CR_CEN, CRS->CR );
    TEST_ASSERT_BITS_LOW( RCC_APB1LENR_CRSEN, RCC->APB1LENR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_Hsi48TrimState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   State of the automatic trimming of HSI48 depends on the CRS clock.
 *
 * \details Reads the state at reset, with CRS counter enabled but CRS clock off, and with NULL.
 *
 * \par Expected results
 * - State inactive in both cases (CRS without clock can not trim), NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_Hsi48TrimState_ClockOff_ReportsInactive( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_Hsi48TrimState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    CRS->CR = CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
    state   = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_Hsi48TrimState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_Hsi48TrimState( NULL ) );
}

/* ============================== RTC CLOCK ================================= */

/**
 * \brief   RTC clock source is set and read back.
 *
 * \details Sets RTC clock source LSE and reads it back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source reads back LSE.
 */
void Ut_Rcc_Set_RtcClkSource_Lse_SourceReadBack( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_HSE_DIV;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_LSE, source );
}

/* ============================ CLOCK OUTPUTS =============================== */

/**
 * \brief   MCO1 output with HSE source configures source and pin.
 *
 * \details Sets MCO1 source HSE, GPIO mock expects initialization of the pin.
 *
 * \par Expected results
 * - Gpio_Init() called with PA8, alternate function 0, push-pull, very high speed,
 *   no pull.
 * - RCC_REQUEST_OK, CFGR1.MCO1SEL = HSE (0b010).
 */
void Ut_Rcc_Set_ClkOutSource_Mco1Hse_SelectsSourceAndConfiguresPin( void )
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


/**
 * \brief   Clock output rejects source of other output.
 *
 * \details MCO2 = HSE, LSCO = LSI preset. Sets MCO1 source to MCO2 output, MCO2
 *          source to MCO1 output, LSCO source to MCO2 output and output out of range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, Gpio_Init() not called.
 * - CFGR1 and BDCR unchanged.
 */
void Ut_Rcc_Set_ClkOutSource_SourceOfOtherOutput_ReturnsErrorWithoutChange( void )
{
    RCC->CFGR1 = RCC_CFGR1_MCO2SEL_1;   /* MCO2 = HSE */
    RCC->BDCR  = 0u;                    /* LSCO = LSI */

    /* No Gpio_Init expected - strict mock fails on the call */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO1_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO2_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_MCO2_CSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSE ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR1_MCO2SEL_1, RCC->CFGR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
}


/**
 * \brief   Clock output without source is not configured.
 *
 * \details Sets source NONE of MCO1, MCO2 and LSCO.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Gpio_Init() not called, CFGR1 and BDCR stay 0.
 */
void Ut_Rcc_Set_ClkOutSource_None_OutputNotConfigured( void )
{
    /* No Gpio_Init expected - strict mock fails on the call */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_NONE ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
}


/**
 * \brief   All clock output sources are set and read back.
 *
 * \details Sets every source of MCO1, MCO2 and LSCO and reads it back, then calls the
 *          getter with output out of range and with NULL pointer.
 *
 * \par Expected results
 * - Every source reads back.
 * - Output out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkOutSource_AllSources_ReadBack( void )
{
    const rcc_ClkOut_Id_t     outIds[]  = { RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1,
                                            RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2,
                                            RCC_CLK_OUT_MCO2, RCC_CLK_OUT_LSCO, RCC_CLK_OUT_LSCO };
    const rcc_ClkOut_Source_t sources[] = { RCC_CLK_SOURCE_MCO1_HSI64, RCC_CLK_SOURCE_MCO1_LSE, RCC_CLK_SOURCE_MCO1_HSE,
                                            RCC_CLK_SOURCE_MCO1_PLL1Q, RCC_CLK_SOURCE_MCO1_HSI48,
                                            RCC_CLK_SOURCE_MCO2_SYSCLK, RCC_CLK_SOURCE_MCO2_PLL2P, RCC_CLK_SOURCE_MCO2_HSE,
                                            RCC_CLK_SOURCE_MCO2_PLL1P, RCC_CLK_SOURCE_MCO2_CSI, RCC_CLK_SOURCE_MCO2_LSI,
                                            RCC_CLK_SOURCE_LSCO_LSI, RCC_CLK_SOURCE_LSCO_LSE };
    rcc_ClkOut_Source_t       source    = RCC_CLK_SOURCE_NONE;

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );

    for( uint32_t idx = 0u; ( sizeof( sources ) / sizeof( sources[ 0u ] ) ) > idx; idx++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( outIds[ idx ], sources[ idx ] ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( outIds[ idx ], &source ) );
        TEST_ASSERT_EQUAL( sources[ idx ], source );
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_CNT, &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, NULL ) );
}

/* ============================ RESET SOURCE ================================ */

/**
 * \brief   Rcc_Get_ResetSource() reads reset flags.
 *
 * \details RSR = IWDG and PIN reset flags. Reads IWDG, PIN and BOR reset source, then
 *          calls the function with source out of range and with NULL pointer.
 *
 * \par Expected results
 * - IWDG, PIN: RCC_FLAG_ACTIVE, BOR: RCC_FLAG_INACTIVE.
 * - Source out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ResetSource_ReadsFlags( void )
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


/**
 * \brief   Rcc_Set_ResetSourceClear() removes reset flags.
 *
 * \details RSR = IWDG and PIN reset flags, HW model removes flags while RMVF is set.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RSR = 0 (flags removed, RMVF released).
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsRemoved_ReturnsOkAndReleasesRmvf( void )
{
    RCC->RSR = RCC_RSR_IWDGRSTF | RCC_RSR_PINRSTF;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->RSR );
}


/**
 * \brief   Rcc_Set_ResetSourceClear() reports flags not removed.
 *
 * \details RSR = IWDG reset flag, without HW model (flags are not removed).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RMVF is released.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsStay_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->RSR = RCC_RSR_IWDGRSTF;    /* No HW model - flags are not removed */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );
    TEST_ASSERT_BITS_LOW( RCC_RSR_RMVF, RCC->RSR );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Rcc_Init() with default configuration switches system clock to PLL1.
 *
 * \details HW model sets ready flags and switch status. Initializes the module with
 *          default configuration (GPIO mock ignored).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS = PLL1, HSE and PLL1 enabled.
 * - Flash prefetch enabled, latency 3 wait states, ICACHE enabled.
 * - SystemCoreClock = AHB1 = 160 MHz, SysTick interval 1 ms.
 */
void Ut_Rcc_Init_DefaultConfig_SysClkFromPll1( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq     = 0u;
    rcc_Time_ms_t      interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

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


/**
 * \brief   Rcc_Init() stops when oscillator does not start.
 *
 * \details Regulator ready flag preset, without HW model (no oscillator becomes ready).
 *          Initializes the module with default configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock is not switched, PLL1 is not enabled.
 */
void Ut_Rcc_Init_HseNotStarting_ReturnsErrorBeforeSysClkSwitch( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* Voltage scaling ready, but no oscillator ever becomes ready */
    PWR->VOSSR = PWR_VOSSR_VOSRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_LOW( RCC_CFGR1_SW, RCC->CFGR1 );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   Rcc_Init() rejects NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Init_NullConfig_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( NULL ) );
}

/* ============================ COVERAGE COMPLETION ========================= */

/**
 * \brief   Bus dividers are read back for every bus.
 *
 * \details HSI system clock (64 MHz), AHB /2, APB1 /4, APB2 /8, APB3 /16.
 *
 * \par Expected results
 * - Rcc_Get_ClkBusDivider() returns the dividers (AHB1 / AHB2, APB1_1 / APB1_2, APB2,
 *   APB3), invalid bus rejected.
 * - APB2 clock 4 MHz, APB3 clock 2 MHz.
 */
void Ut_Rcc_Get_ClkBusDivider_AllBuses_ReadBack( void )
{
    rcc_ClkBusDiv_t divider = 0u;
    rcc_FreqHz_t    freq    = 0u;

    Ut_Rcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB2_DIVIDER_8 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB3,   RCC_APB3_DIVIDER_16 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB_DIVIDER_2, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1_DIVIDER_4, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2_DIVIDER_8, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB3, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB3_DIVIDER_16, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_CNT, &divider ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 16u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB3, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 32u, freq );
}


/**
 * \brief   Clock output divider is written and read back.
 *
 * \details MCO1 and MCO2 divider 4, invalid output.
 *
 * \par Expected results
 * - Rcc_Get_ClkOutDivider() returns divider 4 for both outputs, invalid output rejected.
 */
void Ut_Rcc_Set_ClkOutDivider_ReadBack( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 4u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, 4u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO2, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, divider );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_CNT, &divider ) );
}


/**
 * \brief   PLL internal (VCO) frequency and PLL2 outputs are reported.
 *
 * \details PLL1 and PLL2 configured with the default configuration (HSI 64 MHz / 4 * 20,
 *          outputs / 2).
 *
 * \par Expected results
 * - Rcc_Get_PllInternalClk(): 320 MHz for PLL1 and PLL2.
 * - Kernel clock of LPTIM1 (PLL2 P), USART1 (PLL2 Q) and ADC (PLL2 R): 160 MHz.
 */
void Ut_Rcc_Get_PllInternalClk_Pll2OutputsAsKernelClock( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &config.Pll_Config[ RCC_PLL_2 ] ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 2u * UT_RCC_PLL1P_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 2u * UT_RCC_PLL1P_DEFAULT_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPTIM1_PLL2P, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_PLL2Q, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC_PLL2R, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, freq );
}


/**
 * \brief   Kernel clock of oscillator sources is reported.
 *
 * \details Kernel clocks of USART1 (CSI), USB (HSI48), RTC (LSI, LSE).
 *
 * \par Expected results
 * - CSI_VALUE, HSI48_VALUE, LSI_VALUE, LSE_VALUE.
 */
void Ut_Rcc_Get_PeriphClk_OscillatorSources_NominalFrequency( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_CSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( CSI_VALUE, freq );
#if defined(USB_DRD_FS)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USB_HSI48, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( HSI48_VALUE, freq );
#endif /* USB_DRD_FS */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_LSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_LSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );
}


/**
 * \brief   Kernel clock of HSE follows the HSE frequency of Rcc_Init().
 *
 * \details Default configuration (HSE 8 MHz) initialized, kernel clock of ADC with HSE
 *          source read.
 *
 * \par Expected results
 * - 8 MHz.
 */
void Ut_Rcc_Get_PeriphClk_HseSource_InitFrequency( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC_HSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ, freq );
}


/**
 * \brief   Rcc_Deinit() and Rcc_Task() do not change the clock configuration.
 *
 * \details HSI system clock preset, Rcc_Deinit() with NULL and Rcc_Task() called.
 *
 * \par Expected results
 * - CR and CFGR1 not changed.
 */
void Ut_Rcc_Deinit_Task_NoClockChange( void )
{
    Ut_Rcc_Set_SysClkHsi();

    Rcc_Deinit( NULL );
    Rcc_Task();

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_HSIRDY, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSI, RCC->CFGR1 );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief HW model of RCC and PWR - runs in background thread.
 *
 * Ready flags follow enable bits, SWS follows SW, regulator is always ready,
 * reset flags are removed while RMVF is set.
 */
static void Ut_Rcc_HwModel( void )
{
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSION,   RCC_CR_HSIRDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSEON,   RCC_CR_HSERDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_CSION,   RCC_CR_CSIRDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSI48ON, RCC_CR_HSI48RDY );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL1ON,  RCC_CR_PLL1RDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL2ON,  RCC_CR_PLL2RDY  );
#if defined(RCC_CR_PLL3ON)
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL3ON,  RCC_CR_PLL3RDY  );
#endif
    Ut_Rcc_Mirror( &RCC->BDCR, RCC_BDCR_LSEON, RCC_BDCR_LSERDY );
    Ut_Rcc_Mirror( &RCC->BDCR, RCC_BDCR_LSION, RCC_BDCR_LSIRDY );

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
static void Ut_Rcc_Mirror( volatile uint32_t *reg, uint32_t onMask, uint32_t rdyMask )
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
static void Ut_Rcc_Set_SysClkHsi( void )
{
    RCC->CR    = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CFGR1 = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
}
