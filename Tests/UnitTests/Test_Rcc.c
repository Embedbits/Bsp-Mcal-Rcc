/**
 * \author Mr.Nobody
 * \file Test_Rcc.c
 * \ingroup Rcc
 * \brief Unit tests of STM32U5 Reset and Clock Control (RCC) module.
 *
 * All Rcc sources are compiled unchanged with real LL drivers. RCC, PWR, FLASH,
 * ICACHE and SysTick registers are emulated by RegMem, GPIO module (MCO pin) is
 * mocked by CMock.
 *
 * Oscillator / PLL enable -> ready flags, system clock switch (SW -> SWS),
 * voltage scaling and EPOD booster ready flags and reset flags removal are
 * emulated by HW model running in background thread (Ut_Rcc_HwModel). Tests
 * without the model check timeout (error) branches.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "UtCommon.h"                       /* Common test helpers            */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Rcc_Port.h"                       /* Module under test              */
#include "Rcc_ClkMux.h"                     /* Clock MUX internal API (table check) */
#include "Rcc_ClkSrc.h"                     /* Clock sources internal API (HSE frequency) */
#include "MockGpio_Port.h"                  /* GPIO module mock (MCO pin)     */
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
static void                 Ut_Rcc_Set_ResetState   ( void );
static void                 Ut_Rcc_Set_SysClkHsi    ( void );
static gpio_Config_t        Ut_Rcc_Get_McoPinConfig ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSI16 oscillator frequency */
#define UT_RCC_HSI_HZ                       ( 16000000u )

/** MSIS / MSIK frequency after reset (range 4) */
#define UT_RCC_MSI_RESET_HZ                 ( 4000000u )

/** MSI range 4 (4 MHz) - reset value of MSIS / MSIK range fields */
#define UT_RCC_MSI_RANGE_RESET              ( 4u )

/** System clock of default configuration: MSIS 4 MHz / 1 * 80 / 2 */
#define UT_RCC_SYSCLK_DEFAULT_HZ            ( 160000000u )

/** VCO frequency of default PLL1 configuration */
#define UT_RCC_VCO_DEFAULT_HZ               ( 320000000u )

/** HSE frequency used by tests */
#define UT_RCC_HSE_HZ                       ( 16000000u )

/** HSE frequency requiring EPOD booster prescaler 4 */
#define UT_RCC_HSE_48MHZ                    ( 48000000u )

/** Count of milliseconds in one second */
#define UT_RCC_MS_IN_SECOND                 ( 1000u )

/** Interval too long for 24-bit SysTick reload at 16 MHz */
#define UT_RCC_SYSTICK_TOO_LONG_MS          ( 2000u )

/** Default PLL input divider */
#define UT_RCC_DEFAULT_PLL_M                ( 1u )

/** Default PLL multiplier */
#define UT_RCC_DEFAULT_PLL_N                ( 80u )

/** Default PLL output divider */
#define UT_RCC_DEFAULT_PLL_DIV              ( 2u )

/** LSI frequency */
#define UT_RCC_LSI_HZ                       ( 32000u )

/** LSE frequency */
#define UT_RCC_LSE_HZ                       ( 32768u )

/* ========================== LOCAL VARIABLES =============================== */

/** Sample of peripherals from every clock bus with their enable bit */
static const utRcc_PeriphEnable_t utRcc_PeriphEnableLut[] =
{
    { .PeriphId = RCC_PERIPH_FLASH,           .EnableReg = &RCC->AHB1ENR,  .EnableBit = RCC_AHB1ENR_FLASHEN     },
    { .PeriphId = RCC_PERIPH_GPIOA,           .EnableReg = &RCC->AHB2ENR1, .EnableBit = RCC_AHB2ENR1_GPIOAEN    },
    { .PeriphId = RCC_PERIPH_OCTOSPI1_SYSCLK, .EnableReg = &RCC->AHB2ENR2, .EnableBit = RCC_AHB2ENR2_OCTOSPI1EN },
    { .PeriphId = RCC_PERIPH_LPDMA1,          .EnableReg = &RCC->AHB3ENR,  .EnableBit = RCC_AHB3ENR_LPDMA1EN    },
    { .PeriphId = RCC_PERIPH_TIM2,            .EnableReg = &RCC->APB1ENR1, .EnableBit = RCC_APB1ENR1_TIM2EN     },
    { .PeriphId = RCC_PERIPH_WWDG,            .EnableReg = &RCC->APB1ENR1, .EnableBit = RCC_APB1ENR1_WWDGEN     },
    { .PeriphId = RCC_PERIPH_I2C4_PCLK1,      .EnableReg = &RCC->APB1ENR2, .EnableBit = RCC_APB1ENR2_I2C4EN     },
    { .PeriphId = RCC_PERIPH_TIM1,            .EnableReg = &RCC->APB2ENR,  .EnableBit = RCC_APB2ENR_TIM1EN      },
    { .PeriphId = RCC_PERIPH_SYSCFG,          .EnableReg = &RCC->APB3ENR,  .EnableBit = RCC_APB3ENR_SYSCFGEN    },
};

/** EPOD booster ready flag follows booster enable (cleared - booster never gets ready) */
static volatile uint32_t utRcc_BoostModel = 1u;

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    /* Stops HW model of previous test and clears registers */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    utRcc_BoostModel = 1u;

    Ut_Rcc_Set_ResetState();
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
 * - RCC_REQUEST_OK, system clock from PLL1, PLL1 source MSIS, M = 1, N = 80, R = 2.
 * - PLL2 / PLL3 not used, HSE not used, voltage range 1, clock outputs not used.
 * - NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_DefaultConfig_ReturnsPll1FromMsis( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_PLL, config.SystemClockSource );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_MSIS,            config.Pll_Config[ RCC_PLL_1 ].Pll_Source );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_M,   config.Pll_Config[ RCC_PLL_1 ].M_Divider );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_N,   config.Pll_Config[ RCC_PLL_1 ].N_Multiplier );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_DIV, config.Pll_Config[ RCC_PLL_1 ].R_Divider );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE,            config.Pll_Config[ RCC_PLL_2 ].Pll_Source );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE,            config.Pll_Config[ RCC_PLL_3 ].Pll_Source );
    TEST_ASSERT_EQUAL( RCC_HSE_TYPE_NONE,           config.HSE_ClockType );
    TEST_ASSERT_EQUAL( RCC_PWR_VOLTAGE_SCALE_1,     config.VoltageScaling );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE,         config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE,         config.McoConfig[ RCC_CLK_OUT_LSCO ].ClockSource );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_DefaultConfig( NULL ) );
}

/* ========================= PERIPHERAL CLOCKS ============================== */

/**
 * \brief   Rcc_Set_PeriphActive() sets only the clock enable bit of the peripheral.
 *
 * \details Enables clock of a sample peripheral from every bus (AHB1, AHB2 group 1
 *          and 2, AHB3, APB1 group 1 and 2, APB2, APB3), registers are cleared
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
 * - RCC_REQUEST_OK, APB2ENR.USART1EN is set, USART1SEL stays PCLK2 (0).
 */
void Ut_Rcc_Set_PeriphActive_Usart1_SetsApb2EnableBit( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_PCLK2 ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
    TEST_ASSERT_BITS_LOW( RCC_CCIPR1_USART1SEL, RCC->CCIPR1 );
}


/**
 * \brief   Rcc_Set_PeriphActive() selects kernel clock of the peripheral.
 *
 * \details Enables USART1 with HSI16 kernel clock, then reads clock source with
 *          USART1 / PCLK2 identification.
 *
 * \par Expected results
 * - HSI16 started, CCIPR1.USART1SEL = HSI16 (0b10), APB2ENR.USART1EN is set.
 * - Any USART1 identification returns the selected source RCC_PERIPH_USART1_HSI.
 */
void Ut_Rcc_Set_PeriphActive_Usart1Hsi_SelectsClockMux( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSION, RCC->CR );     /* Kernel clock oscillator started */

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_1, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );

    /* Any USART1 entry returns the entry of actually selected source */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1_PCLK2, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1_HSI, clkSrc );
}


/**
 * \brief   Rcc_Set_PeriphActive() reports clock MUX set to other source.
 *
 * \details USART1 clock MUX is preset to HSI16 (not default source), then USART1 is
 *          enabled with SYSCLK kernel clock.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, CCIPR1.USART1SEL stays HSI16.
 * - APB2ENR.USART1EN is not set.
 */
void Ut_Rcc_Set_PeriphActive_MuxSetToOtherSource_ReturnsErrorWithoutEnable( void )
{
    RCC->CCIPR1 = RCC_CCIPR1_USART1SEL_1;    /* USART1SEL = HSI16 */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_SYSCLK ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_1, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_LOW( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() returns source selected by reset value of the MUX.
 *
 * \details Reads clock sources of USART1, I2C1, LPTIM1 and LPUART1 with cleared
 *          CCIPR registers.
 *
 * \par Expected results
 * - USART1: PCLK2, I2C1: PCLK1, LPTIM1: MSIK, LPUART1: PCLK3.
 */
void Ut_Rcc_Get_PeriphClkSrc_MuxDefault_ReturnsDefaultSource( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1_HSI, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1_PCLK2, clkSrc );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_I2C1_HSI, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_I2C1_PCLK1, clkSrc );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_LPTIM1_LSE, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_LPTIM1_MSIK, clkSrc );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_LPUART1_LSE, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_LPUART1_PCLK3, clkSrc );
}


/**
 * \brief   I2C1 MSIK kernel clock is selected and read back.
 *
 * \details MSIK is switched off. Enables I2C1 with MSIK kernel clock, reads the
 *          source and the kernel clock frequency.
 *
 * \par Expected results
 * - MSIK started, CCIPR1.I2C1SEL = MSIK (0b11), APB1ENR1.I2C1EN is set.
 * - Source RCC_PERIPH_I2C1_MSIK, kernel clock 4 MHz (MSIK reset range).
 */
void Ut_Rcc_Set_PeriphActive_I2c1Msik_SelectsClockMuxAndSourceReadBack( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;
    rcc_FreqHz_t   freq   = 0u;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C1_MSIK ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_MSIKON, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_I2C1SEL, RCC->CCIPR1 & RCC_CCIPR1_I2C1SEL );
    TEST_ASSERT_BITS_HIGH( RCC_APB1ENR1_I2C1EN, RCC->APB1ENR1 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_I2C1_PCLK1, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_I2C1_MSIK, clkSrc );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_I2C1_MSIK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_MSI_RESET_HZ, freq );
}


/**
 * \brief   ADC1 / ADC2 HSI16 kernel clock is selected and VDDA validated.
 *
 * \details Enables ADC1 / ADC2 with HSI16 kernel clock and reads the source.
 *
 * \par Expected results
 * - CCIPR3.ADCDACSEL = HSI16 (0b100), AHB2ENR1.ADC12EN is set.
 * - PWR clock enabled and SVMCR.ASV (VDDA valid) is set.
 * - Source RCC_PERIPH_ADC_HSI.
 */
void Ut_Rcc_Set_PeriphActive_AdcHsi_MuxSelectedAndVddaValid( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR3_ADCDACSEL_2, RCC->CCIPR3 & RCC_CCIPR3_ADCDACSEL );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR1_ADC12EN, RCC->AHB2ENR1 );
    TEST_ASSERT_BITS_HIGH( RCC_AHB3ENR_PWREN, RCC->AHB3ENR );
    TEST_ASSERT_BITS_HIGH( PWR_SVMCR_ASV, PWR->SVMCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ADC_HCLK, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_ADC_HSI, clkSrc );
}


/**
 * \brief   Independent supplies are validated with the peripheral clock.
 *
 * \details Enables port G, DAC1 and USB (USB DRD FS or OTG by the device) clocks.
 *
 * \par Expected results
 * - SVMCR.IO2SV (VDDIO2 - PG[15:2]) set by port G, SVMCR.ASV by DAC1,
 *   SVMCR.USV by USB.
 * - Port A does not validate any supply.
 */
void Ut_Rcc_Set_PeriphActive_IndependentSupplies_Validated( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_GPIOA ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->SVMCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_GPIOG ) );
    TEST_ASSERT_EQUAL_HEX32( PWR_SVMCR_IO2SV, PWR->SVMCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_DAC_HCLK ) );
    TEST_ASSERT_BITS_HIGH( PWR_SVMCR_ASV, PWR->SVMCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USB_PLL1Q ) );
    TEST_ASSERT_BITS_HIGH( PWR_SVMCR_USV, PWR->SVMCR );
}


/**
 * \brief   RNG kernel clock HSI48 starts the oscillator.
 *
 * \details HSI48 is switched off. Enables RNG with HSI48 kernel clock.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSI48 enabled and ready, AHB2ENR1.RNGEN is set.
 */
void Ut_Rcc_Set_PeriphActive_RngHsi48_StartsOscillator( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSI48ON | RCC_CR_HSI48RDY, RCC->CR );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR1_RNGEN, RCC->AHB2ENR1 );
}


/**
 * \brief   Running kernel clock oscillator is not touched.
 *
 * \details HSI16 enabled and ready, no HW model. Enables USART1 with HSI16.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSI16 stays enabled, USART1 clock enabled.
 */
void Ut_Rcc_Set_PeriphActive_OscillatorRunning_KeepsOscillator( void )
{
    RCC->CR |= RCC_CR_HSION | RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSION | RCC_CR_HSIRDY, RCC->CR );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Kernel clock oscillator which does not start is reported.
 *
 * \details No HW model - HSI48 never becomes ready. Enables RNG with HSI48.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RNG clock is not enabled, RNGSEL unchanged.
 */
void Ut_Rcc_Set_PeriphActive_OscillatorNotReady_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_BITS_LOW( RCC_AHB2ENR1_RNGEN, RCC->AHB2ENR1 );
    TEST_ASSERT_BITS_LOW( RCC_CCIPR2_RNGSEL, RCC->CCIPR2 );
}


/**
 * \brief   External kernel clock source is not started.
 *
 * \details Enables USART1 with LSE kernel clock (no HW model).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, USART1SEL = LSE (0b11), LSE stays off.
 */
void Ut_Rcc_Set_PeriphActive_ExternalSource_NotStarted( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_LSE ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );
}


/**
 * \brief   Clock multiplexer table is consistent.
 *
 * \details Calls Rcc_ClkMux_Init() which checks index and field value of every record.
 *
 * \par Expected results
 * - RCC_REQUEST_OK.
 */
void Ut_Rcc_ClkMux_Init_AllRecords_Valid( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Init() );
}


/**
 * \brief   Every peripheral can be activated and deactivated.
 *
 * \details With HW model, every peripheral ID is activated, its state read, then
 *          deactivated and its state read again (registers cleared before every ID).
 *          External clock sources (HSE, LSE, PLL outputs) are not started by Rcc,
 *          external pin source has no frequency - only states are checked.
 *
 * \par Expected results
 * - Activation and deactivation return RCC_REQUEST_OK.
 * - Peripheral state ACTIVE after activation, INACTIVE after deactivation for
 *   peripherals with clock enable bit.
 */
void Ut_Rcc_PeriphActiveInactive_AllPeripherals_RoundTrip( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( rcc_PeriphId_t periphId = (rcc_PeriphId_t)0u; RCC_PERIPH_ID_CNT > periphId; periphId++ )
    {
        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );
        Ut_Rcc_Set_ResetState();
        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

        TEST_ASSERT_EQUAL_MESSAGE( RCC_REQUEST_OK, Rcc_Set_PeriphActive( periphId ), "activation" );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( periphId, &state ) );
        TEST_ASSERT_EQUAL_MESSAGE( RCC_FUNCTION_ACTIVE, state, "state after activation" );

        TEST_ASSERT_EQUAL_MESSAGE( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( periphId ), "deactivation" );

        if( ( RCC_PERIPH_IWDG              != periphId ) &&
            ( RCC_PERIPH_SYSTICK_HCLK_DIV8 != periphId ) &&
            ( RCC_PERIPH_SYSTICK_LSI       != periphId ) &&
            ( RCC_PERIPH_SYSTICK_LSE       != periphId )    )
        {
            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( periphId, &state ) );
            TEST_ASSERT_EQUAL_MESSAGE( RCC_FUNCTION_INACTIVE, state, "state after deactivation" );
        }
        else
        {
            /* Blocks without clock enable bit are always clocked */
        }

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelInactive() );
    }
}


/**
 * \brief   Rcc_Set_PeriphInactive() clears only the enable bit of the peripheral.
 *
 * \details APB2ENR = all bits set, USART1 is deactivated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, only USART1EN cleared.
 */
void Ut_Rcc_Set_PeriphInactive_ClearsOnlyOwnBit( void )
{
    RCC->APB2ENR = 0xFFFFFFFFu;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USART1_PCLK2 ) );

    TEST_ASSERT_EQUAL_HEX32( ~RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Deactivation releases kernel clock multiplexer.
 *
 * \details USART1 activated with HSI16, then deactivated and activated with SYSCLK.
 *
 * \par Expected results
 * - After deactivation USART1SEL = PCLK2 (default), activation with SYSCLK succeeds.
 */
void Ut_Rcc_Set_PeriphInactive_KernelClockMux_Released( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USART1_HSI ) );

    TEST_ASSERT_BITS_LOW( RCC_CCIPR1_USART1SEL, RCC->CCIPR1 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR1_USART1SEL_0, RCC->CCIPR1 & RCC_CCIPR1_USART1SEL );
}


/**
 * \brief   Shared multiplexer is kept while another block uses it.
 *
 * \details ADC1 / ADC2 and DAC1 are activated with SYSCLK kernel clock (shared
 *          ADCDACSEL), DAC1 is deactivated, then ADC1 / ADC2.
 *
 * \par Expected results
 * - ADCDACSEL stays SYSCLK after DAC1 deactivation (ADC still enabled).
 * - ADCDACSEL returns to HCLK after ADC1 / ADC2 deactivation.
 */
void Ut_Rcc_Set_PeriphInactive_SharedMux_KeptWhileOtherBlockEnabled( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC_SYSCLK ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_DAC_SYSCLK ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_DAC_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR3_ADCDACSEL_0, RCC->CCIPR3 & RCC_CCIPR3_ADCDACSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_ADC_SYSCLK ) );
    TEST_ASSERT_BITS_LOW( RCC_CCIPR3_ADCDACSEL, RCC->CCIPR3 );
}


/**
 * \brief   RTC clock selection is kept on deactivation.
 *
 * \details RTCSEL preset to LSE, RTC with LSE source is deactivated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCAPBEN cleared, RTCSEL stays LSE.
 */
void Ut_Rcc_Set_PeriphInactive_Rtc_ClockSelectionKept( void )
{
    RCC->BDCR    = RCC_BDCR_RTCSEL_0;           /* RTCSEL = LSE */
    RCC->APB3ENR = RCC_APB3ENR_RTCAPBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_LSE ) );

    TEST_ASSERT_BITS_LOW( RCC_APB3ENR_RTCAPBEN, RCC->APB3ENR );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_0, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   Peripheral functions reject invalid arguments.
 *
 * \details Calls peripheral functions with ID out of range and NULL pointers.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for every call, registers unchanged.
 */
void Ut_Rcc_Periph_InvalidArgs_ReturnError( void )
{
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_PeriphId_t      clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphInactive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphState( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphState( RCC_PERIPH_GPIOA, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ID_CNT, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClkSrc( RCC_PERIPH_GPIOA, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetInactive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetState( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetState( RCC_PERIPH_GPIOA, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SleepActive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SleepInactive( RCC_PERIPH_ID_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SleepState( RCC_PERIPH_ID_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SleepState( RCC_PERIPH_GPIOA, NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB2ENR1 );
}


/**
 * \brief   Reset of USART1 is activated and released.
 *
 * \details Activates reset of USART1, reads the state, releases the reset.
 *
 * \par Expected results
 * - APB2RSTR.USART1RST set and state ACTIVE, then cleared and state INACTIVE.
 */
void Ut_Rcc_Set_ResetActiveInactive_Usart1_TogglesResetBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2RSTR_USART1RST, RCC->APB2RSTR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_USART1_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2RSTR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Peripheral without reset control is not reset.
 *
 * \details Activates and releases reset of FLASH interface (no reset bit) and
 *          reads the state.
 *
 * \par Expected results
 * - Reset activation: RCC_REQUEST_ERROR, no reset register changed.
 * - Reset release: RCC_REQUEST_OK, state INACTIVE.
 */
void Ut_Rcc_Set_ResetActive_PeriphWithoutResetBit_DoesNotResetOthers( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB1RSTR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_FLASH, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   USART1 clock in Sleep / Stop mode is enabled and disabled.
 *
 * \details Activates and deactivates USART1 Sleep / Stop mode clock and reads the state.
 *
 * \par Expected results
 * - APB2SMENR.USART1SMEN set and state ACTIVE, then cleared and state INACTIVE.
 */
void Ut_Rcc_Set_SleepActiveInactive_Usart1_TogglesSmEnableBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2SMENR_USART1SMEN, RCC->APB2SMENR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_USART1_PCLK2 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2SMENR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}

/* ============================== CLOCK BUSES =============================== */

/**
 * \brief   Bus dividers are written to CFGR2 / CFGR3.
 *
 * \details Sets AHB /2 (by AHB2 group 2 ID), APB1 /4, APB2 /8 and APB3 /16, then
 *          calls the setter with bus out of range.
 *
 * \par Expected results
 * - CFGR2.HPRE, PPRE1, PPRE2 and CFGR3.PPRE3 hold the dividers.
 * - Bus out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_ClkBusDivider_WritesPrescalers( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB2_2, RCC_AHB_DIVIDER_2   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_2, RCC_APB1_DIVIDER_4  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB2_DIVIDER_8  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB3,   RCC_APB3_DIVIDER_16 ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_2, RCC->CFGR2 & RCC_CFGR2_HPRE  );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB1_DIV_4,   RCC->CFGR2 & RCC_CFGR2_PPRE1 );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB2_DIV_8,   RCC->CFGR2 & RCC_CFGR2_PPRE2 );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB3_DIV_16,  RCC->CFGR3 & RCC_CFGR3_PPRE3 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT, RCC_AHB_DIVIDER_1 ) );
}


/**
 * \brief   Bus clocks are calculated from system clock and dividers.
 *
 * \details System clock HSI16 (16 MHz), AHB /2, APB1 /2, APB2 /4, APB3 /8.
 *
 * \par Expected results
 * - SYSCLK 16 MHz, AHB 8 MHz, APB1 4 MHz, APB2 2 MHz, APB3 1 MHz.
 * - Bus out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkBusClk_HsiSysClk_AppliesDividers( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CFGR2 = LL_RCC_SYSCLK_DIV_2 | LL_RCC_APB1_DIV_2 | LL_RCC_APB2_DIV_4;
    RCC->CFGR3 = LL_RCC_APB3_DIV_8;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_SYSCLK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB3, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 8u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB3, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 16u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_CNT, &freq ) );
}


/**
 * \brief   Bus dividers of all buses are read back.
 *
 * \details CFGR2 / CFGR3 preset with different dividers, every bus divider is read.
 *
 * \par Expected results
 * - AHB groups: AHB divider, APB1 groups: APB1 divider, APB2 and APB3 dividers.
 * - Bus out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkBusDivider_AllBuses_ReadBack( void )
{
    rcc_ClkBusDiv_t divider = 0u;

    RCC->CFGR2 = LL_RCC_SYSCLK_DIV_4 | LL_RCC_APB1_DIV_8 | LL_RCC_APB2_DIV_2;
    RCC->CFGR3 = LL_RCC_APB3_DIV_16;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_4, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB2_1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_4, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB3, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_4, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB1_DIV_8, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB2_DIV_2, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB3, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB3_DIV_16, divider );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_CNT, &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB2, NULL ) );
}


/**
 * \brief   Timer kernel clock equals APB clock when APB is not divided.
 *
 * \details System clock HSI16, APB1 / APB2 dividers 1.
 *
 * \par Expected results
 * - TIM2 (APB1) and TIM1 (APB2) kernel clock 16 MHz.
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
 * \brief   Timer kernel clock is twice the APB clock when APB is divided.
 *
 * \details System clock HSI16, APB1 /4, APB2 /2.
 *
 * \par Expected results
 * - TIM2 (APB1 4 MHz): 8 MHz, TIM1 (APB2 8 MHz): 16 MHz.
 */
void Ut_Rcc_Get_PeriphClk_TimerApbDivided_TwicePclk( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CFGR2 = LL_RCC_APB1_DIV_4 | LL_RCC_APB2_DIV_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );
}


/**
 * \brief   Peripheral clock reader rejects invalid arguments and unknown frequency.
 *
 * \details Reads clock of peripheral out of range, with NULL pointer and of MDF1
 *          clocked from external pin.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, frequency of pin source 0.
 */
void Ut_Rcc_Get_PeriphClk_InvalidArgs_ReturnsError( void )
{
    rcc_FreqHz_t freq = 1u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_ID_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_GPIOA, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_MDF1_PIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
}


/**
 * \brief   Oscillator kernel clock sources report nominal frequency.
 *
 * \details Reads kernel clock of peripherals clocked by HSI16, HSI48, HSI48 / 2,
 *          MSIK, LSI and LSE, SysTick reference HCLK / 8 (system clock HSI16).
 *
 * \par Expected results
 * - 16 MHz, 48 MHz, 24 MHz, 4 MHz, 32 kHz, 32.768 kHz, 2 MHz.
 */
void Ut_Rcc_Get_PeriphClk_OscillatorSources_NominalFrequency( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SPI1_HSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_HSI48, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_HSI48_DIV2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 24000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPTIM1_MSIK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_MSI_RESET_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_IWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_LSI_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPUART1_LSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_LSE_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SYSTICK_HCLK_DIV8, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 8u, freq );
}

/* =============================== SYSTICK ================================== */

/**
 * \brief   SysTick interval 1 ms is configured from HCLK.
 *
 * \details System clock HSI16, interval 1 ms.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SysTick reload 15999, enabled with processor clock.
 * - Interval read back 1 ms.
 */
void Ut_Rcc_Set_SysTickInterval_1ms_ConfiguresReload( void )
{
    rcc_Time_ms_t interval = 0u;

    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL_UINT32( ( UT_RCC_HSI_HZ / UT_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_BITS_HIGH( SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk, SysTick->CTRL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   SysTick interval out of range is rejected.
 *
 * \details System clock HSI16, intervals 0 ms and 2000 ms (reload > 24 bits),
 *          getter with NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, SysTick not configured.
 */
void Ut_Rcc_Set_SysTickInterval_OutOfRange_ReturnsErrorWithoutWrite( void )
{
    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( UT_RCC_SYSTICK_TOO_LONG_MS ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->CTRL );
    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->LOAD );
}

/* ========================= FLASH AND POWER ================================ */

/**
 * \brief   Flash prefetch is enabled and disabled.
 *
 * \par Expected results
 * - FLASH ACR.PRFTEN set, then cleared.
 */
void Ut_Rcc_Set_FlashPrefetch_TogglesPrefetchBit( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchActive() );
    TEST_ASSERT_BITS_HIGH( FLASH_ACR_PRFTEN, FLASH->ACR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashPrefetchInactive() );
    TEST_ASSERT_BITS_LOW( FLASH_ACR_PRFTEN, FLASH->ACR );
}


/**
 * \brief   Voltage range 1 enables EPOD booster and is reached.
 *
 * \details With HW model (VOSRDY), voltage range 1 is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR clock enabled, VOSR.VOS = range 1, VOSR.BOOSTEN set.
 */
void Ut_Rcc_Set_PwrRange_Range1_EnablesBoosterAndWritesScale( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_BITS_HIGH( RCC_AHB3ENR_PWREN, RCC->AHB3ENR );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE1, PWR->VOSR & PWR_VOSR_VOS );
    TEST_ASSERT_BITS_HIGH( PWR_VOSR_BOOSTEN, PWR->VOSR );
}


/**
 * \brief   Voltage range 3 disables EPOD booster.
 *
 * \details Booster enabled, with HW model voltage range 3 is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOSR.VOS = range 3, VOSR.BOOSTEN cleared.
 */
void Ut_Rcc_Set_PwrRange_Range3_DisablesBooster( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_3;
    PWR->VOSR             = PWR_VOSR_BOOSTEN;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE3, PWR->VOSR & PWR_VOSR_VOS );
    TEST_ASSERT_BITS_LOW( PWR_VOSR_BOOSTEN, PWR->VOSR );
}


/**
 * \brief   Voltage range not reached is reported.
 *
 * \details No HW model - VOSRDY never set. Range 2 is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PwrRange_RegulatorNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
}


/**
 * \brief   Unsupported voltage range and NULL configuration are rejected.
 *
 * \details Requests voltage range with invalid value and NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, VOSR unchanged, PWR clock not enabled.
 */
void Ut_Rcc_Set_PwrRange_InvalidArgs_ReturnsErrorWithoutWrite( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.VoltageScaling = (rcc_PwrVoltageScale_t)PWR_VOSR_BOOSTEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->VOSR );
    TEST_ASSERT_BITS_LOW( RCC_AHB3ENR_PWREN, RCC->AHB3ENR );
}


/**
 * \brief   Flash latency for 160 MHz in voltage range 1 is 4 wait states.
 *
 * \details Voltage range 1, default configuration (PLL1 160 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 4 wait states.
 */
void Ut_Rcc_Set_FlashLatency_Pll160MHzRange1_Sets4WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_4, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency for 160 MHz in voltage range 2 is rejected.
 *
 * \details Voltage range 2 (maximum 110 MHz), default configuration (160 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, latency unchanged.
 */
void Ut_Rcc_Set_FlashLatency_Pll160MHzRange2_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency of HSI16 system clock in voltage range 4 is 1 wait state.
 *
 * \details Voltage range 4 (reset value), system clock HSI16 requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 1 wait state.
 */
void Ut_Rcc_Set_FlashLatency_HsiRange4_Sets1WaitState( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency of MSIS 4 MHz system clock is 0 wait states.
 *
 * \details Latency 2 preset, voltage range 4, system clock MSIS (reset range 4 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 0 wait states.
 */
void Ut_Rcc_Set_FlashLatency_MsisSysClk_Sets0WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_MSIS;
    FLASH->ACR               = LL_FLASH_LATENCY_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency uses HSE frequency, AHB divider and configured minimum.
 *
 * \details Voltage range 1, system clock HSE 48 MHz: AHB /1, AHB /2, minimum 3 wait states.
 *
 * \par Expected results
 * - 48 MHz: 1 wait state, 24 MHz (AHB /2): 0 wait states, minimum: 3 wait states.
 */
void Ut_Rcc_Set_FlashLatency_HseSysClk_FrequencyDividerAndMinimum( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = UT_RCC_HSE_48MHZ;
    PWR->VOSR                = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, LL_FLASH_GetLatency() );

    config.AHB_Divider = RCC_AHB_DIVIDER_2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );

    config.FlashLatency = RCC_FLASH_LATENCY_3_WS;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( NULL ) );
}

/* ================================= PLL ==================================== */

/**
 * \brief   Default PLL1 configuration is written and PLL1 enabled.
 *
 * \details With HW model, voltage range 1 and booster enabled, PLL1 is configured
 *          by default configuration (MSIS 4 MHz, M = 1, N = 80, P = Q = R = 2).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MSIS running, PLL1 enabled and locked.
 * - PLL1CFGR: source MSIS, input range 4 - 8 MHz, M field 0, booster prescaler 1,
 *   outputs P, Q, R enabled. PLL1DIVR: N field 79, P / Q / R fields 1.
 * - Booster stays enabled.
 */
void Ut_Rcc_Set_PllConfig_DefaultPll1_ConfiguresAndEnables( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1 | PWR_VOSR_BOOSTEN;
    RCC->CR   = 0u;     /* MSIS switched off - started by PLL source selection */

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_MSISON | RCC_CR_PLL1ON | RCC_CR_PLL1RDY, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLL1SOURCE_MSIS | LL_RCC_PLL1MBOOST_DIV_1 |
                             RCC_PLL1CFGR_PLL1PEN | RCC_PLL1CFGR_PLL1QEN | RCC_PLL1CFGR_PLL1REN, RCC->PLL1CFGR );
    TEST_ASSERT_EQUAL_HEX32( ( 79u << RCC_PLL1DIVR_PLL1N_Pos ) | ( 1u << RCC_PLL1DIVR_PLL1P_Pos ) |
                             ( 1u << RCC_PLL1DIVR_PLL1Q_Pos ) | ( 1u << RCC_PLL1DIVR_PLL1R_Pos ), RCC->PLL1DIVR );
    TEST_ASSERT_BITS_HIGH( PWR_VOSR_BOOSTEN, PWR->VOSR );
}


/**
 * \brief   PLL output frequencies are calculated from registers.
 *
 * \details Default PLL1 configured (VCO 320 MHz), outputs and VCO are read.
 *
 * \par Expected results
 * - VCO 320 MHz, outputs P, Q, R 160 MHz.
 */
void Ut_Rcc_Get_PllClk_DefaultPll1_ReturnsFrequency( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_VCO_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_SYSCLK_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_SYSCLK_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_SYSCLK_DEFAULT_HZ, freq );
}


/**
 * \brief   PLL with input frequency 8 - 16 MHz selects the upper input range.
 *
 * \details PLL2 from HSI16, M = 1, N = 20 (VCO 320 MHz), only output R (R = 4).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL2CFGR: source HSI16, input range 8 - 16 MHz, output R
 *   enabled, outputs P and Q disabled.
 * - PLL2 R output 80 MHz used as ADC kernel clock.
 */
void Ut_Rcc_Set_PllConfig_Pll2Hsi_UpperInputRangeAndKernelClock( void )
{
    rcc_PllConfigStruct_t pllConfig = { .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 1u, .N_Multiplier = 20u,
                                        .P_Divider  = 0u,              .Q_Divider = 0u, .R_Divider    = 4u };
    rcc_FreqHz_t          freq      = 0u;

    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLL2SOURCE_HSI | RCC_PLL2CFGR_PLL2RGE | RCC_PLL2CFGR_PLL2REN, RCC->PLL2CFGR );
    TEST_ASSERT_BITS_HIGH( RCC_CR_PLL2ON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC_PLL2R, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 80000000u, freq );
}


/**
 * \brief   PLL reference frequency out of range is rejected.
 *
 * \details PLL1 from MSIS 4 MHz with M = 2 (2 MHz reference).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL1 is not enabled.
 */
void Ut_Rcc_Set_PllConfig_ReferenceOutOfRange_ReturnsErrorWithoutEnable( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].M_Divider = 2u;
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   VCO frequency out of the range of the voltage range is rejected.
 *
 * \details PLL1 from MSIS 4 MHz, N = 100 (VCO 400 MHz): voltage range 3 (VCO up to
 *          330 MHz), voltage range 4 (PLL not allowed); N = 140 (VCO 560 MHz) in range 1.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, PLL1 is not enabled.
 */
void Ut_Rcc_Set_PllConfig_VcoOutOfRange_ReturnsErrorWithoutEnable( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = 100u;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE3;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE4;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = 140u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   PLL1 R output accepts 1 or even divider only.
 *
 * \details PLL1 default configuration with R = 3, then PLL2 with R = 3.
 *
 * \par Expected results
 * - PLL1: RCC_REQUEST_ERROR, PLL1 not enabled.
 * - PLL2 (no restriction): RCC_REQUEST_OK.
 */
void Ut_Rcc_Set_PllConfig_Pll1OddRDivider_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 3u;
    config.Pll_Config[ RCC_PLL_2 ]           = config.Pll_Config[ RCC_PLL_1 ];
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &config.Pll_Config[ RCC_PLL_2 ] ) );
}


/**
 * \brief   EPOD booster prescaler keeps booster clock at most 16 MHz.
 *
 * \details Booster enabled, PLL1 from HSE 48 MHz (M = 4, N = 20).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLL1MBOOST = divider 4 (12 MHz), booster enabled again.
 */
void Ut_Rcc_Set_PllConfig_Hse48MHz_BoosterPrescaler4( void )
{
    rcc_PllConfigStruct_t pllConfig = { .Pll_Source = RCC_PLL_SRC_HSE, .M_Divider = 4u, .N_Multiplier = 20u,
                                        .P_Divider  = 0u,              .Q_Divider = 0u, .R_Divider    = 2u };

    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1 | PWR_VOSR_BOOSTEN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( UT_RCC_HSE_48MHZ ) );

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLL1MBOOST_DIV_4, RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1MBOOST );
    TEST_ASSERT_BITS_HIGH( PWR_VOSR_BOOSTEN, PWR->VOSR );
}


/**
 * \brief   PLL configuration rejects invalid arguments.
 *
 * \details PLL out of range, NULL configuration, M = 17, N = 3.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 */
void Ut_Rcc_Set_PllConfig_InvalidArgs_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_CNT, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, NULL ) );

    config.Pll_Config[ RCC_PLL_1 ].M_Divider = 17u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    config.Pll_Config[ RCC_PLL_1 ].M_Divider    = 1u;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = 3u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
}


/**
 * \brief   PLL which does not lock is reported.
 *
 * \details MSIS running, PLL1 ready flag never set (no HW model).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PllConfig_PllNotLocked_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
}


/**
 * \brief   PLL activation state and source are read back.
 *
 * \details With HW model PLL3 source HSI16 is selected, PLL3 activated and
 *          deactivated.
 *
 * \par Expected results
 * - Source HSI16 read back, state ACTIVE after activation, INACTIVE after
 *   deactivation.
 * - PLL out of range and NULL pointers: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PllActive_StateAndSourceReadBack( void )
{
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_PllClkSrc_t     source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_3, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_3, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_3 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_3, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_3 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_3, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllActive( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllState( RCC_PLL_3, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_CNT, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_3, RCC_PLL_SRC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllsSource( RCC_PLL_3, NULL ) );
}

/* ============================= OSCILLATORS ================================ */

/**
 * \brief   All oscillators are activated and deactivated.
 *
 * \details With HW model every oscillator is activated, its state read, then
 *          deactivated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, state ACTIVE after activation and INACTIVE after
 *   deactivation.
 * - Backup domain access (PWR DBP) released for LSI / LSE.
 * - Oscillator out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_OscActive_AllOscillators_StateReadBack( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    for( rcc_OscId_t oscId = RCC_OSC_HSI16; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    TEST_ASSERT_BITS_HIGH( PWR_DBPR_DBP, PWR->DBPR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( RCC_OSC_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( RCC_OSC_HSI16, NULL ) );
}


/**
 * \brief   MSIS / MSIK dividers select MSI ranges.
 *
 * \details With HW model MSIS divider 12 (4 MHz) and 1 (48 MHz), MSIK divider 24
 *          (2 MHz) are set and read back.
 *
 * \par Expected results
 * - ICSCR1.MSIRGSEL set, MSISRANGE / MSIKRANGE hold range 4 / 0 / 5.
 * - Dividers and MSIK kernel clock frequency read back.
 */
void Ut_Rcc_Set_OscDiv_Msi_RangeAppliedAndReported( void )
{
    rcc_OscDiv_t oscDiv = 0u;
    rcc_FreqHz_t freq   = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CR |= RCC_CR_MSISON | RCC_CR_MSISRDY | RCC_CR_MSIKON | RCC_CR_MSIKRDY;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_MSIS, 12u ) );
    TEST_ASSERT_BITS_HIGH( RCC_ICSCR1_MSIRGSEL, RCC->ICSCR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MSISRANGE_4, RCC->ICSCR1 & RCC_ICSCR1_MSISRANGE );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_MSIS, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 12u, oscDiv );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_MSIS, 1u ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MSISRANGE_0, RCC->ICSCR1 & RCC_ICSCR1_MSISRANGE );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_MSIK, 24u ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MSIKRANGE_5, RCC->ICSCR1 & RCC_ICSCR1_MSIKRANGE );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_MSIK, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 24u, oscDiv );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_I2C1_MSIK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 2000000u, freq );
}


/**
 * \brief   MSIS divider change of the system clock updates flash latency and SystemCoreClock.
 *
 * \details System clock MSIS 4 MHz, voltage range 1. MSIS divider 1 (48 MHz) is set.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 1 wait state, SystemCoreClock 48 MHz.
 */
void Ut_Rcc_Set_OscDiv_MsisSysClk_LatencyAndCoreClockUpdated( void )
{
    PWR->VOSR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_MSIS, 1u ) );

    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, SystemCoreClock );
}


/**
 * \brief   Unsupported oscillator divider is rejected without change.
 *
 * \details MSIS divider 5, HSI16 divider 2, LSE divider 4, oscillator out of range.
 *          HSI16 divider 1 accepted. MSI range of 3.072 MHz reference is not
 *          reported as divider.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, ICSCR1 unchanged; HSI16 divider 1: RCC_REQUEST_OK.
 * - Getter: HSI16 divider 1, MSIS range 8: RCC_REQUEST_ERROR, NULL: error.
 */
void Ut_Rcc_Set_OscDiv_InvalidDivider_ReturnsErrorWithoutChange( void )
{
    rcc_OscDiv_t   oscDiv = 0u;
    const uint32_t icscr1 = RCC->ICSCR1;

    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_MSIS, 5u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_HSI16, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_LSE, 4u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_CNT, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_OscDiv( RCC_OSC_HSI16, 1u ) );
    TEST_ASSERT_EQUAL_HEX32( icscr1, RCC->ICSCR1 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_HSI16, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, oscDiv );

    RCC->ICSCR1 = RCC_ICSCR1_MSIRGSEL | LL_RCC_MSISRANGE_8;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_MSIS, &oscDiv ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_MSIS, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_CNT, &oscDiv ) );
}


/**
 * \brief   LSI prescaler is changed only while LSI is off.
 *
 * \details LSI off: divider 128 set. LSI running: divider 1 requested.
 *
 * \par Expected results
 * - LSI off: RCC_REQUEST_OK, BDCR.LSIPREDIV set, LSI clock 250 Hz.
 * - LSI running: RCC_REQUEST_ERROR, LSIPREDIV stays set.
 */
void Ut_Rcc_Set_OscDiv_Lsi_PrescalerOnlyWhileOff( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_LSI, 128u ) );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSIPREDIV, RCC->BDCR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_IWDG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_LSI_HZ / 128u, freq );

    RCC->BDCR |= RCC_BDCR_LSION | RCC_BDCR_LSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_LSI, 1u ) );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSIPREDIV, RCC->BDCR );
}

/* ========================== POWER SUPPLY VALIDITY ========================= */

/**
 * \brief   Independent supplies are validated and invalidated.
 *
 * \details For VDDUSB, VDDIO2 and VDDA reads the state of the reset configuration,
 *          validates the supply, reads the state, invalidates the supply and reads
 *          the state again.
 *
 * \par Expected results
 * - Inactive at reset (PWR_SVMCR.USV / IO2SV / ASV = 0).
 * - After validation: RCC_REQUEST_OK, supply valid bit set, state active.
 * - After invalidation: RCC_REQUEST_OK, supply valid bit cleared, state inactive.
 */
void Ut_Rcc_Set_PwrSupplyActive_AllSupplies_ValidityBitFollowsRequest( void )
{
    const uint32_t validBit[ RCC_PWR_SUPPLY_CNT ] =
    {
        [RCC_PWR_SUPPLY_VDDUSB] = PWR_SVMCR_USV,
        [RCC_PWR_SUPPLY_VDDIO2] = PWR_SVMCR_IO2SV,
        [RCC_PWR_SUPPLY_VDDA]   = PWR_SVMCR_ASV,
    };

    for( rcc_PwrSupplyId_t supplyId = RCC_PWR_SUPPLY_VDDUSB; RCC_PWR_SUPPLY_CNT > supplyId; supplyId++ )
    {
        rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PwrSupplyState( supplyId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrSupplyActive( supplyId ) );
        TEST_ASSERT_BITS_HIGH( validBit[ supplyId ], PWR->SVMCR );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PwrSupplyState( supplyId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrSupplyInactive( supplyId ) );
        TEST_ASSERT_BITS_LOW( validBit[ supplyId ], PWR->SVMCR );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PwrSupplyState( supplyId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Power supply functions reject invalid arguments.
 *
 * \details Calls the functions with supply out of range and the getter with NULL.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PWR_SVMCR is not changed.
 */
void Ut_Rcc_PwrSupply_InvalidArguments_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrSupplyActive( RCC_PWR_SUPPLY_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrSupplyInactive( RCC_PWR_SUPPLY_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PwrSupplyState( RCC_PWR_SUPPLY_VDDUSB, NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->SVMCR );
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
 *   rising edge. CRS_CR: TRIM = 0x40, AUTOTRIMEN and CEN set.
 * - State active.
 */
void Ut_Rcc_Set_Hsi48TrimActive_UsbSof_ConfiguresCrs( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_Hsi48TrimActive( RCC_HSI48_TRIM_SRC_USB_SOF ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB1ENR1_CRSEN, RCC->APB1ENR1 );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSI48ON, RCC->CR );

    TEST_ASSERT_EQUAL_UINT32( 47999u, CRS->CFGR & CRS_CFGR_RELOAD );
    TEST_ASSERT_EQUAL_UINT32( LL_CRS_ERRORLIMIT_DEFAULT, ( CRS->CFGR & CRS_CFGR_FELIM ) >> CRS_CFGR_FELIM_Pos );
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_DIV_1, CRS->CFGR & CRS_CFGR_SYNCDIV );
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_POLARITY_RISING, CRS->CFGR & CRS_CFGR_SYNCPOL );
    TEST_ASSERT_EQUAL_HEX32( LL_CRS_SYNC_SOURCE_USB, CRS->CFGR & CRS_CFGR_SYNCSRC );
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
    TEST_ASSERT_BITS_LOW( RCC_APB1ENR1_CRSEN, RCC->APB1ENR1 );
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
    TEST_ASSERT_BITS_LOW( RCC_APB1ENR1_CRSEN, RCC->APB1ENR1 );
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

/* ================================ RTC ===================================== */

/**
 * \brief   RTC clock source is selected once and read back.
 *
 * \details RTC source LSE selected, read back, the same source selected again,
 *          then LSI requested.
 *
 * \par Expected results
 * - LSE: RCC_REQUEST_OK, read back LSE, backup domain access released.
 * - LSE again: RCC_REQUEST_OK.
 * - LSI: RCC_REQUEST_ERROR (backup domain reset needed), RTCSEL stays LSE.
 * - No source selected: getter returns error.
 */
void Ut_Rcc_Set_RtcClkSource_Lse_SelectedOnce( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_HSE_DIV;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( &source ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_LSE, source );
    TEST_ASSERT_BITS_HIGH( PWR_DBPR_DBP, PWR->DBPR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSE, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( NULL ) );
}

/* ============================ CLOCK OUTPUTS =============================== */

/**
 * \brief   MCO output with HSE source configures source and pin.
 *
 * \details Sets MCO source HSE, GPIO mock expects initialization of the pin.
 *
 * \par Expected results
 * - Gpio_Init() called with PA8, alternate function 0, push-pull, very high speed,
 *   no pull.
 * - RCC_REQUEST_OK, CFGR1.MCOSEL = HSE (0b0100).
 */
void Ut_Rcc_Set_ClkOutSource_Mco1Hse_SelectsSourceAndConfiguresPin( void )
{
    gpio_Config_t expectedGpio = Ut_Rcc_Get_McoPinConfig();

    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR1_MCOSEL_2, RCC->CFGR1 & RCC_CFGR1_MCOSEL );
}


/**
 * \brief   LSCO output with LSE source is enabled in backup domain without GPIO.
 *
 * \details Sets LSCO source LSE (no GPIO expectation - strict mock).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, BDCR.LSCOSEL and BDCR.LSCOEN set, PWR DBP set.
 */
void Ut_Rcc_Set_ClkOutSource_LscoLse_EnabledWithoutPin( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_LSCO_LSE ) );

    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSCOSEL | RCC_BDCR_LSCOEN, RCC->BDCR );
    TEST_ASSERT_BITS_HIGH( PWR_DBPR_DBP, PWR->DBPR );
}


/**
 * \brief   Clock output rejects source of other output.
 *
 * \details Sets MCO source to LSCO source, LSCO source to MCO source and output
 *          out of range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, Gpio_Init() not called, CFGR1 and BDCR unchanged.
 */
void Ut_Rcc_Set_ClkOutSource_SourceOfOtherOutput_ReturnsErrorWithoutChange( void )
{
    /* No Gpio_Init expected - strict mock fails on the call */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_LSCO_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_MCO1_HSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSE ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
}


/**
 * \brief   Clock output without source is not configured.
 *
 * \details Sets source NONE of MCO and LSCO.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Gpio_Init() not called, CFGR1 and BDCR stay 0.
 */
void Ut_Rcc_Set_ClkOutSource_None_OutputNotConfigured( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_NONE ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
}


/**
 * \brief   All clock output sources are set and read back.
 *
 * \details Sets every source of MCO and LSCO and reads it back, reads MCO without
 *          source and disabled LSCO, then calls the getter with output out of range and
 *          NULL pointer.
 * \note    Bug AB#1154: disabled LSCO (LSCOEN = 0) was reported as LSI (LSCOSEL reset value).
 *
 * \par Expected results
 * - Every source reads back, MCO without source and disabled LSCO read RCC_CLK_SOURCE_NONE.
 * - Output out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkOutSource_AllSources_ReadBack( void )
{
    rcc_ClkOut_Source_t source = RCC_CLK_SOURCE_CNT;

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    source = RCC_CLK_SOURCE_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    for( rcc_ClkOut_Source_t srcIdx = RCC_CLK_SOURCE_MCO1_SYSCLK; RCC_CLK_SOURCE_CNT > srcIdx; srcIdx++ )
    {
        rcc_ClkOut_Id_t outId = RCC_CLK_OUT_MCO1;

        if( RCC_CLK_SOURCE_LSCO_LSI <= srcIdx )
        {
            outId = RCC_CLK_OUT_LSCO;
        }
        else
        {
            /* Source of MCO */
        }

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( outId, srcIdx ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( outId, &source ) );
        TEST_ASSERT_EQUAL( srcIdx, source );
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_CNT, &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, NULL ) );
}


/**
 * \brief   Disabled LSCO reads no source independent of the selected one.
 *
 * \details Presets BDCR with LSCOSEL = LSE and LSCOEN = 0, reads the source, then sets
 *          LSCOEN and reads the source again.
 * \note    Bug AB#1154: getter ignored LSCOEN.
 *
 * \par Expected results
 * - LSCOEN = 0: RCC_CLK_SOURCE_NONE. LSCOEN = 1: RCC_CLK_SOURCE_LSCO_LSE.
 */
void Ut_Rcc_Get_ClkOutSource_LscoDisabled_ReturnsNone( void )
{
    rcc_ClkOut_Source_t source = RCC_CLK_SOURCE_CNT;

    RCC->BDCR = RCC_BDCR_LSCOSEL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    RCC->BDCR = RCC_BDCR_LSCOSEL | RCC_BDCR_LSCOEN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_LSCO_LSE, source );
}


/**
 * \brief   MCO divider is set and read back, LSCO accepts divider 1 only.
 *
 * \details Sets MCO dividers 1, 2, 4, 8, 16 and 3, LSCO dividers 1 and 2.
 *
 * \par Expected results
 * - MCO valid dividers read back from CFGR1.MCOPRE, divider 3: RCC_REQUEST_ERROR.
 * - LSCO divider 1: OK and read back 1, divider 2: RCC_REQUEST_ERROR.
 * - Output out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_ClkOutDivider_ReadBack( void )
{
    const rcc_ClkOut_Div_t dividers[] = { 1u, 2u, 4u, 8u, 16u };
    rcc_ClkOut_Div_t       divider    = 0u;

    for( uint32_t idx = 0u; ( sizeof( dividers ) / sizeof( dividers[ 0u ] ) ) > idx; idx++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, dividers[ idx ] ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
        TEST_ASSERT_EQUAL_UINT32( dividers[ idx ], divider );
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 3u ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1_DIV_16, RCC->CFGR1 & RCC_CFGR1_MCOPRE );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_LSCO, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO, 2u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_CNT, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_CNT, &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, NULL ) );
}

/* ============================ RESET SOURCE ================================ */

/**
 * \brief   Rcc_Get_ResetSource() reads reset flags.
 *
 * \details CSR = IWDG, PIN and OBL reset flags. Reads IWDG, PIN, OBL and BOR reset
 *          source, then calls the function with source out of range and NULL pointer.
 *
 * \par Expected results
 * - IWDG, PIN, OBL: RCC_FLAG_ACTIVE, BOR: RCC_FLAG_INACTIVE.
 * - Source out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ResetSource_ReadsFlags( void )
{
    rcc_FlagState_t flag = RCC_FLAG_INACTIVE;

    RCC->CSR |= RCC_CSR_IWDGRSTF | RCC_CSR_PINRSTF | RCC_CSR_OBLRSTF;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_IWDG, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flag );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_PIN, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flag );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_OBL, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_ACTIVE, flag );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( RCC_RESET_SRC_BOR, &flag ) );
    TEST_ASSERT_EQUAL( RCC_FLAG_INACTIVE, flag );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_CNT, &flag ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_PIN, NULL ) );
}


/**
 * \brief   Reset flags are removed and remove flag released.
 *
 * \details CSR = PIN and SW reset flags, with HW model (RMVF clears flags).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, all reset flags and RMVF cleared, MSI ranges kept.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsRemoved_ReturnsOkAndReleasesRmvf( void )
{
    RCC->CSR |= RCC_CSR_PINRSTF | RCC_CSR_SFTRSTF;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_BITS_LOW( RCC_CSR_PINRSTF | RCC_CSR_SFTRSTF | RCC_CSR_RMVF, RCC->CSR );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_MSI_RANGE_RESET << RCC_CSR_MSISSRANGE_Pos, RCC->CSR & RCC_CSR_MSISSRANGE );
}


/**
 * \brief   Reset flags which are not removed are reported.
 *
 * \details CSR = BOR reset flag, no HW model.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RMVF released.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsStay_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->CSR |= RCC_CSR_BORRSTF;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_BITS_LOW( RCC_CSR_RMVF, RCC->CSR );
}

/* ============================== INIT ====================================== */

/**
 * \brief   Rcc_Init() with default configuration runs system clock from PLL1.
 *
 * \details With HW model, module is initialized with default configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS = PLL1, PLL1 enabled, voltage range 1, booster enabled.
 * - Flash prefetch enabled, latency 4 wait states, ICACHE enabled.
 * - SystemCoreClock = AHB = 160 MHz, SysTick interval 1 ms.
 */
void Ut_Rcc_Init_DefaultConfig_SysClkFromPll1( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq     = 0u;
    rcc_Time_ms_t      interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, RCC->CFGR1 & RCC_CFGR1_SWS );
    TEST_ASSERT_BITS_HIGH( RCC_CR_PLL1ON, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE1, PWR->VOSR & PWR_VOSR_VOS );
    TEST_ASSERT_BITS_HIGH( PWR_VOSR_BOOSTEN, PWR->VOSR );
    TEST_ASSERT_BITS_HIGH( FLASH_ACR_PRFTEN, FLASH->ACR );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_4, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_SYSCLK_DEFAULT_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_SYSCLK_DEFAULT_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Init() with HSE system clock and clock security system.
 *
 * \details With HW model, configuration: HSE crystal 16 MHz as system clock, CSS
 *          enabled, PLL1 not used, voltage range 2, MCO = SYSCLK / 2.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS = HSE, HSE and CSS enabled, PLL1 off.
 * - Latency 0 wait states, SystemCoreClock 16 MHz, MCO configured (pin initialized).
 */
void Ut_Rcc_Init_HseSysClk_CssAndMcoConfigured( void )
{
    rcc_ConfigStruct_t config;
    gpio_Config_t      expectedGpio = Ut_Rcc_Get_McoPinConfig();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType                              = RCC_HSE_TYPE_CRYSTAL;
    config.HSE_Frequency_Hz                           = UT_RCC_HSE_HZ;
    config.CSS_Enable                                 = RCC_FUNCTION_ACTIVE;
    config.SystemClockSource                          = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.VoltageScaling                             = RCC_PWR_VOLTAGE_SCALE_2;
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source         = RCC_PLL_SRC_NONE;
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLK_SOURCE_MCO1_SYSCLK;
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 2u;

    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSE, RCC->CFGR1 & RCC_CFGR1_SWS );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_CSSON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON | RCC_CR_HSEBYP, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_HZ, SystemCoreClock );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1SOURCE_SYSCLK | LL_RCC_MCO1_DIV_2, RCC->CFGR1 & ( RCC_CFGR1_MCOSEL | RCC_CFGR1_MCOPRE ) );
}


/**
 * \brief   Rcc_Init() stops when oscillator does not start.
 *
 * \details Voltage scaling ready flag preset, without HW model (no oscillator
 *          becomes ready). Initializes the module with default configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock is not switched (stays MSIS), PLL1 is not enabled.
 */
void Ut_Rcc_Init_OscillatorNotStarting_ReturnsErrorBeforeSysClkSwitch( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* Voltage scaling ready, but no oscillator ever becomes ready */
    PWR->VOSR = PWR_VOSR_VOSRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_LOW( RCC_CFGR1_SW, RCC->CFGR1 );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   Switch to PLL1 waits for EPOD booster.
 *
 * \details With HW model but booster never ready, module is initialized with
 *          default configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock stays HSI16 (not switched to PLL1).
 */
void Ut_Rcc_Init_BoosterNotReady_SysClkNotSwitchedToPll( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    utRcc_BoostModel = 0u;
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSI, RCC->CFGR1 & RCC_CFGR1_SWS );
}


/**
 * \brief   Rcc_Init() rejects NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, no register written.
 */
void Ut_Rcc_Init_NullConfig_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( NULL ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB1ENR );
}


/**
 * \brief   HSE frequency of initialization is used as kernel clock frequency.
 *
 * \details With HW model module initialized with HSE crystal 16 MHz (PLL1 from
 *          MSIS as system clock), FDCAN1 HSE kernel clock read.
 *
 * \par Expected results
 * - FDCAN1 kernel clock 16 MHz, RTC HSE / 32 source 500 kHz.
 */
void Ut_Rcc_Get_PeriphClk_HseSource_InitFrequency( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType    = RCC_HSE_TYPE_SIG_DIGITAL_IN;
    config.HSE_Frequency_Hz = UT_RCC_HSE_HZ;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEBYP | RCC_CR_HSEEXT, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_FDCAN1_HSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV32, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_HZ / 32u, freq );
}


/**
 * \brief   Rcc_Deinit() and Rcc_Task() do not change clocks.
 *
 * \par Expected results
 * - Registers stay unchanged.
 */
void Ut_Rcc_Deinit_Task_NoClockChange( void )
{
    rcc_ConfigStruct_t config;
    const uint32_t     cr = RCC->CR;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Rcc_Deinit( &config );
    Rcc_Task();

    TEST_ASSERT_EQUAL_HEX32( cr, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR1 );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief HW model: ready flags follow enable bits, SWS follows SW, voltage
 *        scaling / booster ready, reset flags removed by RMVF.
 */
static void Ut_Rcc_HwModel( void )
{
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_MSISON,  RCC_CR_MSISRDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_MSIKON,  RCC_CR_MSIKRDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSION,   RCC_CR_HSIRDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSEON,   RCC_CR_HSERDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSI48ON, RCC_CR_HSI48RDY );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL1ON,  RCC_CR_PLL1RDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL2ON,  RCC_CR_PLL2RDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL3ON,  RCC_CR_PLL3RDY  );
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

    (void)__atomic_or_fetch( &PWR->VOSR, PWR_VOSR_VOSRDY, __ATOMIC_SEQ_CST );

    if( 0u != utRcc_BoostModel )
    {
        Ut_Rcc_Mirror( &PWR->VOSR, PWR_VOSR_BOOSTEN, PWR_VOSR_BOOSTRDY );
    }
    else
    {
        /* Booster never gets ready */
    }

    if( 0u != ( RCC->CSR & RCC_CSR_RMVF ) )
    {
        (void)__atomic_and_fetch( &RCC->CSR, ~( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_SFTRSTF |
                                                RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF |
                                                RCC_CSR_OBLRSTF ), __ATOMIC_SEQ_CST );
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


/** Emulates reset state of RCC: MSIS 4 MHz running as system clock, MSI ranges 4 MHz */
static void Ut_Rcc_Set_ResetState( void )
{
    RCC->CR     = RCC_CR_MSISON | RCC_CR_MSISRDY;
    RCC->ICSCR1 = ( UT_RCC_MSI_RANGE_RESET << RCC_ICSCR1_MSISRANGE_Pos ) | ( UT_RCC_MSI_RANGE_RESET << RCC_ICSCR1_MSIKRANGE_Pos );
    RCC->CSR    = ( UT_RCC_MSI_RANGE_RESET << RCC_CSR_MSISSRANGE_Pos ) | ( UT_RCC_MSI_RANGE_RESET << RCC_CSR_MSIKSRANGE_Pos );
}


/** Emulates HSI16 selected as system clock */
static void Ut_Rcc_Set_SysClkHsi( void )
{
    RCC->CR    = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CFGR1 = LL_RCC_SYS_CLKSOURCE_HSI | LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
}


/** Returns expected GPIO configuration of MCO pin (PA8, alternate function 0) */
static gpio_Config_t Ut_Rcc_Get_McoPinConfig( void )
{
    gpio_Config_t gpioConfig = { 0 };

    gpioConfig.PortId         = GPIO_PORT_A;
    gpioConfig.PinId          = GPIO_PIN_ID_8;
    gpioConfig.PinMode        = GPIO_PIN_MODE_ALTERNATE;
    gpioConfig.PinPull        = GPIO_PIN_PULL_NONE;
    gpioConfig.PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH;
    gpioConfig.PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL;
    gpioConfig.PinAltFunction = GPIO_ALT_FUNC_0;
    gpioConfig.PinActiveLevel = GPIO_PIN_LEVEL_HIGH;

    return ( gpioConfig );
}
