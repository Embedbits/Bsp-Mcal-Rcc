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
#include "Rcc_ClkSrc.h"                     /* Clock sources internal API (LSE configuration) */
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

/** Register field watched by the monitor of the order of register writes (\ref UtRcc_HwModelOrder) */
typedef enum
{
    UT_RCC_ORDER_FIELD_SYSCLK_SW = 0u,  /**< RCC_CFGR1.SW - requested system clock source */
    UT_RCC_ORDER_FIELD_VOS,             /**< PWR_VOSCR.VOS - voltage scale                */
    UT_RCC_ORDER_FIELD_LATENCY,         /**< FLASH_ACR.LATENCY - flash wait states        */
    UT_RCC_ORDER_FIELD_HPRE,            /**< RCC_CFGR2.HPRE - AHB divider                 */
    UT_RCC_ORDER_FIELD_CNT              /**< Count of watched fields                      */
}   utRcc_OrderField_t;

/** Change of a watched register field recorded by the monitor of the order of register writes */
typedef struct
{
    utRcc_OrderField_t Field;       /**< Changed field                                       */
    uint32_t           Old;         /**< Value of the field before the change                */
    uint32_t           Value;       /**< Value of the field after the change                 */
    uint32_t           SysClkSw;    /**< Requested system clock source (CFGR1.SW) at that time */
}   utRcc_OrderRecord_t;

/* ======================= FORWARD DECLARATIONS ============================= */

static void                 Ut_Rcc_HwModel          ( void );
static void                 Ut_Rcc_Mirror           ( volatile uint32_t *reg, uint32_t onMask, uint32_t rdyMask );
static void                 Ut_Rcc_Set_SysClkHsi    ( void );
static void                 UtRcc_HwModelOrder      ( void );
static void                 UtRcc_HwModelHpreStuck  ( void );
static void                 UtRcc_HwModelAhbOrder   ( void );
static void                 UtRcc_HwModelLatencyStuck( void );
static void                 UtRcc_Log_Change        ( utRcc_OrderField_t field, uint32_t value );
static void                 UtRcc_Clear_OrderLog    ( void );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSI oscillator frequency (HSI_VALUE, without HSIDIV divider) */
#define UT_RCC_HSI_HZ                       ( 64000000u )

/** Timer kernel clock with HSI system clock and APB divider 4 (2 x PCLK, TIMPRE = 0) */
#define UT_RCC_HSI_APB_DIV4_TIM_HZ          ( UT_RCC_HSI_HZ / 2u )

/** PLL1 P output frequency of default configuration: 64 MHz / 4 * 20 / 2 */
#define UT_RCC_PLL1P_DEFAULT_HZ             ( 160000000u )

/** HSE frequency of default configuration */
#define UT_RCC_HSE_DEFAULT_HZ               ( 8000000u )

/** Count of milliseconds in one second */
#define UT_RCC_MS_IN_SECOND                 ( 1000u )

/** Maximal count of state reads until the HW model sets a ready flag */
#define UT_RCC_STATE_READ_CNT               ( 10000000u )

/** Interval too long for 24-bit SysTick reload at 64 MHz */
#define UT_RCC_SYSTICK_TOO_LONG_MS          ( 1000u )

/** Position of AHB prescaler field in LL value (HPRE) */
#define UT_RCC_DEFAULT_PLL_M                ( 4u )

/** Default PLL multiplier */
#define UT_RCC_DEFAULT_PLL_N                ( 20u )

/** Default PLL output divider */
#define UT_RCC_DEFAULT_PLL_DIV              ( 2u )

/** PLL input divider giving a 1.6 MHz reference (medium VCO reference range 1 - 2 MHz) from HSI 64 MHz */
#define UT_RCC_PLL_M_REF_1M6                ( 40u )

/** PLL input divider giving an 8 MHz reference (wide VCO reference range 2 - 16 MHz) from HSI 64 MHz */
#define UT_RCC_PLL_M_REF_8M                 ( 8u )

/** PLL multiplier: 1.6 MHz reference x 200 = 320 MHz VCO (inside the medium VCO range) */
#define UT_RCC_PLL_N_VCO_320M               ( 200u )

/** PLL multiplier: 1.6 MHz reference x 300 = 480 MHz VCO (above the medium VCO range) */
#define UT_RCC_PLL_N_VCO_480M               ( 300u )

/** PLL multiplier: 8 MHz reference x 20 = 160 MHz VCO (inside both VCO ranges) */
#define UT_RCC_PLL_N_VCO_160M               ( 20u )

/** PLL multiplier: 8 MHz reference x 75 = 600 MHz VCO (above the wide VCO range of STM32H5) */
#define UT_RCC_PLL_N_VCO_600M               ( 75u )

/** Flash programming delay WRHIGHFREQ = 00 (0 - 1 wait states, RM0481 Table 48) */
#define UT_RCC_PROG_DELAY_00                ( 0u )

/** Function state out of the rcc_FunctionState_t list */
#define UT_RCC_FUNCTION_STATE_INVALID       ( (rcc_FunctionState_t)( RCC_FUNCTION_ACTIVE + 1u ) )

/** Count of changes of the AHB divider test: wait states and AHB divider */
#define UT_RCC_ORDER_AHB_CNT                ( 2u )

/** Divider 2 of the AHB divider tests */
#define UT_RCC_AHB_DIV_2                    ( 2u )

/** Divider 4 of the AHB divider tests */
#define UT_RCC_AHB_DIV_4                    ( 4u )

/** Sentinel value of SystemCoreClock (must stay unchanged) */
#define UT_RCC_SENTINEL                     ( 12345u )

/** Count of records of the monitor of the order of register writes */
#define UT_RCC_ORDER_LOG_SIZE               ( 16u )

/** Count of leading records of a repeated Rcc_Init(): flash wait states raised, SYSCLK moved to HSI */
#define UT_RCC_ORDER_TRANSITION_CNT         ( 2u )

/** Failure message of a lowered voltage scale or flash wait states while the old SYSCLK source runs */
#define UT_RCC_ORDER_LOWERED_MSG            ( "Voltage scale / flash wait states lowered before SYSCLK moved to HSI" )

/* ========================== LOCAL VARIABLES =============================== */

/** Changes of the watched register fields in the order they were written (\ref UtRcc_HwModelOrder) */
static utRcc_OrderRecord_t utRcc_OrderLog[ UT_RCC_ORDER_LOG_SIZE ];

/** Count of valid records of \ref utRcc_OrderLog */
static uint32_t            utRcc_OrderLogCnt = 0u;

/** Last seen value of every watched register field */
static uint32_t            utRcc_OrderLast[ UT_RCC_ORDER_FIELD_CNT ];

/** Sample of peripherals from every clock bus with their enable bit */
static const utRcc_PeriphEnable_t utRcc_PeriphEnableLut[] =
{
    { .PeriphId = RCC_PERIPH_FLASH, .EnableReg = &RCC->AHB1ENR,  .EnableBit = RCC_AHB1ENR_FLITFEN  },
    { .PeriphId = RCC_PERIPH_GPIOA, .EnableReg = &RCC->AHB2ENR,  .EnableBit = RCC_AHB2ENR_GPIOAEN  },
    { .PeriphId = RCC_PERIPH_TIM2,  .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_TIM2EN  },
    { .PeriphId = RCC_PERIPH_SBS,   .EnableReg = &RCC->APB3ENR,  .EnableBit = RCC_APB3ENR_SBSEN    },
    { .PeriphId = RCC_PERIPH_WWDG,  .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_WWDGEN  },
#if defined(TIM12)
    { .PeriphId = RCC_PERIPH_TIM12, .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_TIM12EN },
#endif /* TIM12 */
#if defined(TIM13)
    { .PeriphId = RCC_PERIPH_TIM13, .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_TIM13EN },
#endif /* TIM13 */
#if defined(TIM14)
    { .PeriphId = RCC_PERIPH_TIM14, .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_TIM14EN },
#endif /* TIM14 */
};

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    /* Stops HW model of previous test and clears registers */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    /* LSE configuration of previous test is replaced by default configuration */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_CRYSTAL, RCC_LSE_DRIVE_MEDIUM_LOW ) );
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
 * - HSE not used (frequency 8 MHz preset), MCO1 output not used.
 * - LSE not used (bug AB#2115 - external oscillators are board specific, given by board
 *   defines), medium-low drive preset (bug AB#1560, lowest drive is forbidden by errata).
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
    TEST_ASSERT_EQUAL( RCC_HSE_TYPE_NONE,           config.HSE_ClockType );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ, config.HSE_Frequency_Hz );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE,         config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource );
    TEST_ASSERT_EQUAL( RCC_LSE_TYPE_NONE,           config.LSE_ClockType );
    TEST_ASSERT_EQUAL( RCC_LSE_DRIVE_MEDIUM_LOW,    config.LSE_Drive );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_DefaultConfig( NULL ) );
}

/* ========================= PERIPHERAL CLOCKS ============================== */

/**
 * \brief   Rcc_Set_PeriphActive() sets only the clock enable bit of the peripheral.
 *
 * \details Enables clock of a sample peripheral from every bus (FLASH - AHB1,
 *          GPIOA - AHB2, TIM2 / WWDG - APB1L, SBS - APB3) and of TIM12 / TIM13 /
 *          TIM14 (APB1L, bug AB#2257) when the MCU has them, registers are cleared
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
 * \brief   Rcc_Set_PeriphActive() enables the RTC kernel clock.
 *
 * \details RTC enabled with LSI clock from reset state (bug AB#1559 - RCC_BDCR.RTCEN was
 *          never written, RTC calendar did not run).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, APB3ENR.RTCAPBEN set, BDCR.RTCEN set, BDCR.RTCSEL = LSI.
 * - Rcc_Get_PeriphState() reports RCC_FUNCTION_ACTIVE.
 */
void Ut_Rcc_Set_PeriphActive_RtcLsi_KernelClockEnabled( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB3ENR_RTCAPBEN, RCC->APB3ENR );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_RTCEN, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_LSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Rcc_Set_PeriphInactive() disables the RTC kernel clock.
 *
 * \details RTC enabled with LSI clock, then disabled (bug AB#1559).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, BDCR.RTCEN and APB3ENR.RTCAPBEN cleared, BDCR.RTCSEL stays LSI.
 * - Rcc_Get_PeriphState() reports RCC_FUNCTION_INACTIVE.
 */
void Ut_Rcc_Set_PeriphInactive_RtcLsi_KernelClockDisabled( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_LSI ) );

    TEST_ASSERT_BITS_LOW( RCC_BDCR_RTCEN, RCC->BDCR );
    TEST_ASSERT_BITS_LOW( RCC_APB3ENR_RTCAPBEN, RCC->APB3ENR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_LSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Rcc_Get_PeriphState() reports RTC inactive without the kernel clock.
 *
 * \details APB3ENR.RTCAPBEN = 1 and BDCR.RTCEN = 0 preset (interface clock only), then
 *          BDCR.RTCEN = 1 preset (bug AB#1559).
 *
 * \par Expected results
 * - Interface clock only: RCC_REQUEST_OK, RCC_FUNCTION_INACTIVE.
 * - Interface and kernel clock: RCC_REQUEST_OK, RCC_FUNCTION_ACTIVE.
 */
void Ut_Rcc_Get_PeriphState_Rtc_KernelClockRequired( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->APB3ENR = RCC_APB3ENR_RTCAPBEN;
    RCC->BDCR    = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->BDCR = RCC_BDCR_RTCEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Peripheral clock (CKPER) of the RTC block does not enable the RTC kernel clock.
 *
 * \details RCC_PERIPH_LPCLK_HSI activated and deactivated with BDCR.RTCEN = 1 preset
 *          (bug AB#1559 - only RTC peripheral IDs control RTCEN).
 *
 * \par Expected results
 * - Activation from reset state: RCC_REQUEST_OK, BDCR.RTCEN stays cleared.
 * - Deactivation with RTCEN preset: RCC_REQUEST_OK, BDCR.RTCEN stays set.
 */
void Ut_Rcc_Set_PeriphActive_LpClk_RtcKernelClockNotChanged( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_LPCLK_HSI ) );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_RTCEN, RCC->BDCR );

    (void)__atomic_or_fetch( &RCC->BDCR, RCC_BDCR_RTCEN, __ATOMIC_SEQ_CST );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_LPCLK_HSI ) );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_RTCEN, RCC->BDCR );
}


/**
 * \brief   RCC_PERIPH_RTC_APB enables the RTC / TAMP register interface clock alone.
 *
 * \details Activation from reset state, state read-back and deactivation with BDCR.RTCEN
 *          preset (the RTC kernel clock and the RTC clock selection belong to the RTC IDs).
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, APB3ENR.RTCAPBEN set, BDCR.RTCEN and BDCR.RTCSEL stay cleared.
 * - Rcc_Get_PeriphState() reports RCC_FUNCTION_ACTIVE without the RTC kernel clock.
 * - Deactivation: RCC_REQUEST_OK, APB3ENR.RTCAPBEN cleared, BDCR.RTCEN stays set.
 */
void Ut_Rcc_Set_PeriphActive_RtcApb_InterfaceClockOnly( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_APB ) );
    TEST_ASSERT_BITS_HIGH( RCC_APB3ENR_RTCAPBEN, RCC->APB3ENR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_RTCEN, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_APB, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    (void)__atomic_or_fetch( &RCC->BDCR, RCC_BDCR_RTCEN, __ATOMIC_SEQ_CST );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_APB ) );
    TEST_ASSERT_BITS_LOW( RCC_APB3ENR_RTCAPBEN, RCC->APB3ENR );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_RTCEN, RCC->BDCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_RTC_APB, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
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
 * \brief   Peripheral reset of CRS uses the CRS reset bit (AB#958).
 *
 * \details Activates CRS reset, reads the reset state and deactivates the reset. The CRS
 *          reset bit exists on all STM32H5 devices, the CEC reset bit is not touched.
 *
 * \par Expected results
 * - APB1LRSTR = CRSRST, reset state active.
 * - APB1LRSTR = 0 after release.
 */
void Ut_Rcc_Set_ResetActiveInactive_Crs_TogglesCrsResetBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( RCC_PERIPH_CRS ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1LRSTR_CRSRST, RCC->APB1LRSTR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_CRS, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_CRS ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1LRSTR );
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
 * \brief   Rcc_Set_ClkBusDivider() refreshes SystemCoreClock for the AHB buses.
 *
 * \details System clock HSI 64 MHz, SystemCoreClock preset to a different value. Sets AHB divider 2
 *          through AHB1, then divider 4 through AHB2, reads SystemCoreClock after each call.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SystemCoreClock = 32 MHz (HCLK = HSI / 2), then 16 MHz (HSI / 4).
 */
void Ut_Rcc_Set_ClkBusDivider_AhbDivider_RefreshesSystemCoreClock( void )
{
    Ut_Rcc_Set_SysClkHsi();
    SystemCoreClock = 12345u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB2, RCC_AHB_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, SystemCoreClock );
}


/**
 * \brief   Rcc_Set_ClkBusDivider() keeps SystemCoreClock when the AHB divider is not applied.
 *
 * \details System clock HSI 64 MHz, SystemCoreClock preset to a sentinel value. The HW model
 *          clears the AHB prescaler after every write, so the read back never matches. The test is
 *          ignored where the HW model does not follow every write (REGMEM_MODEL_MODE_THREAD).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, SystemCoreClock keeps the sentinel value.
 */
void Ut_Rcc_Set_ClkBusDivider_AhbDividerNotApplied_KeepsSystemCoreClock( void )
{
    Ut_Rcc_Set_SysClkHsi();
    SystemCoreClock = 12345u;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( UtRcc_HwModelHpreStuck ) );

    if( REGMEM_MODEL_MODE_SYNC != RegMem_Get_ModelMode() )
    {
        TEST_IGNORE_MESSAGE( "Failing AHB divider write is emulated in the synchronous HW model mode only" );
    }
    else
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_2 ) );
        TEST_ASSERT_EQUAL_UINT32( 12345u, SystemCoreClock );
    }
}


/**
 * \brief   Rcc_Set_ClkBusDivider() raises the flash wait states before the AHB divider when HCLK grows.
 *
 * \details System clock HSI 64 MHz, AHB divider 4 (HCLK 16 MHz, 0 wait states, voltage scale 3), SysTick not
 *          configured. Sets AHB divider 1 (HCLK 64 MHz). The HW model records the order of the writes of
 *          FLASH_ACR.LATENCY and RCC_CFGR2.HPRE. The test is ignored where the HW model does not follow
 *          every write (REGMEM_MODEL_MODE_THREAD).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, two recorded changes: the wait states first (3 WS for 64 MHz), then HPRE = /1.
 * - SystemCoreClock = 64 MHz, SysTick stays unconfigured (LOAD = 0).
 */
void Ut_Rcc_Set_ClkBusDivider_AhbHclkRaised_LatencyBeforeDivider( void )
{
    Ut_Rcc_Set_SysClkHsi();
    LL_RCC_SetAHBPrescaler( LL_RCC_SYSCLK_DIV_4 );
    SystemCoreClock = UT_RCC_HSI_HZ / UT_RCC_AHB_DIV_4;

    UtRcc_Clear_OrderLog();

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( UtRcc_HwModelAhbOrder ) );

    const regMem_ModelMode_t modelMode = RegMem_Get_ModelMode();

    if( REGMEM_MODEL_MODE_SYNC != modelMode )
    {
        TEST_IGNORE_MESSAGE( "Order of register writes is observed in the synchronous HW model mode only" );
    }
    else
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_1 ) );

        TEST_ASSERT_EQUAL_UINT32( UT_RCC_ORDER_AHB_CNT, utRcc_OrderLogCnt );
        TEST_ASSERT_EQUAL( UT_RCC_ORDER_FIELD_LATENCY, utRcc_OrderLog[ 0u ].Field );
        TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, utRcc_OrderLog[ 0u ].Value );
        TEST_ASSERT_EQUAL( UT_RCC_ORDER_FIELD_HPRE, utRcc_OrderLog[ 1u ].Field );
        TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_1, utRcc_OrderLog[ 1u ].Value );

        TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, SystemCoreClock );
        TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->LOAD );
    }
}


/**
 * \brief   Rcc_Set_ClkBusDivider() lowers the flash wait states after the AHB divider when HCLK falls.
 *
 * \details System clock HSI 64 MHz, AHB divider 1 (HCLK 64 MHz, 3 wait states, voltage scale 3), SysTick not
 *          configured. Sets AHB divider 4 (HCLK 16 MHz). The HW model records the order of the writes of
 *          FLASH_ACR.LATENCY and RCC_CFGR2.HPRE. The test is ignored where the HW model does not follow
 *          every write (REGMEM_MODEL_MODE_THREAD).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, two recorded changes: HPRE = /4 first, then the wait states (0 WS for 16 MHz).
 * - SystemCoreClock = 16 MHz, SysTick stays unconfigured (LOAD = 0).
 */
void Ut_Rcc_Set_ClkBusDivider_AhbHclkLowered_DividerBeforeLatency( void )
{
    Ut_Rcc_Set_SysClkHsi();
    LL_FLASH_SetLatency( LL_FLASH_LATENCY_3 );
    SystemCoreClock = UT_RCC_HSI_HZ;

    UtRcc_Clear_OrderLog();

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( UtRcc_HwModelAhbOrder ) );

    const regMem_ModelMode_t modelMode = RegMem_Get_ModelMode();

    if( REGMEM_MODEL_MODE_SYNC != modelMode )
    {
        TEST_IGNORE_MESSAGE( "Order of register writes is observed in the synchronous HW model mode only" );
    }
    else
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_4 ) );

        TEST_ASSERT_EQUAL_UINT32( UT_RCC_ORDER_AHB_CNT, utRcc_OrderLogCnt );
        TEST_ASSERT_EQUAL( UT_RCC_ORDER_FIELD_HPRE, utRcc_OrderLog[ 0u ].Field );
        TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_4, utRcc_OrderLog[ 0u ].Value );
        TEST_ASSERT_EQUAL( UT_RCC_ORDER_FIELD_LATENCY, utRcc_OrderLog[ 1u ].Field );
        TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, utRcc_OrderLog[ 1u ].Value );

        TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / UT_RCC_AHB_DIV_4, SystemCoreClock );
        TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->LOAD );
    }
}


/**
 * \brief   Rcc_Set_ClkBusDivider() keeps the SysTick interval when the AHB divider changes.
 *
 * \details System clock HSI 64 MHz, SysTick interval 1 ms. Sets AHB divider 2, then divider 1.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SysTick LOAD = 31999 (HCLK 32 MHz), then 63999 (HCLK 64 MHz), counter still enabled.
 * - The interval read back is 1 ms after both changes.
 */
void Ut_Rcc_Set_ClkBusDivider_AhbDivider_KeepsSysTickInterval( void )
{
    rcc_Time_ms_t interval = 0u;

    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL_UINT32( ( UT_RCC_HSI_HZ / UT_RCC_AHB_DIV_2 / UT_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_BITS_HIGH( SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk, SysTick->CTRL );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_1 ) );
    TEST_ASSERT_EQUAL_UINT32( ( UT_RCC_HSI_HZ / UT_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Set_ClkBusDivider() does not write the AHB divider when the flash wait states are not applied.
 *
 * \details System clock HSI 64 MHz, AHB divider 4, SystemCoreClock preset to a sentinel value. The HW model
 *          clears FLASH_ACR.LATENCY after every write, so the raise of the wait states for the AHB divider 1
 *          never reads back. The test is ignored where the HW model does not follow every write
 *          (REGMEM_MODEL_MODE_THREAD).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, HPRE still /4 (divider not written), SystemCoreClock keeps the sentinel value.
 */
void Ut_Rcc_Set_ClkBusDivider_AhbLatencyNotApplied_DividerNotWritten( void )
{
    Ut_Rcc_Set_SysClkHsi();
    LL_RCC_SetAHBPrescaler( LL_RCC_SYSCLK_DIV_4 );
    SystemCoreClock = UT_RCC_SENTINEL;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( UtRcc_HwModelLatencyStuck ) );

    const regMem_ModelMode_t modelMode = RegMem_Get_ModelMode();

    if( REGMEM_MODEL_MODE_SYNC != modelMode )
    {
        TEST_IGNORE_MESSAGE( "Failing wait states write is emulated in the synchronous HW model mode only" );
    }
    else
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_1 ) );
        TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYSCLK_DIV_4, LL_RCC_GetAHBPrescaler() );
        TEST_ASSERT_EQUAL_UINT32( UT_RCC_SENTINEL, SystemCoreClock );
    }
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
 * \brief   TIM12 / TIM13 / TIM14 kernel clock is the APB1 timer clock (bug AB#2257).
 *
 * \details System clock HSI 64 MHz, APB1 divider 4, APB2 divider 2 (TIMPRE = 0).
 *          Reads the clock of the timers the MCU has.
 *
 * \par Expected results
 * - TIM12 / TIM13 / TIM14 = 2 x 16 MHz = 32 MHz (APB2 timer clock is 64 MHz).
 * - MCU without TIM12: test ignored.
 */
void Ut_Rcc_Get_PeriphClk_Tim12To14_Apb1TimerClock( void )
{
#if defined(TIM12)
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB2_DIVIDER_2 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM12, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_APB_DIV4_TIM_HZ, freq );
#if defined(TIM13)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM13, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_APB_DIV4_TIM_HZ, freq );
#endif /* TIM13 */
#if defined(TIM14)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM14, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_APB_DIV4_TIM_HZ, freq );
#endif /* TIM14 */
#else
    TEST_IGNORE_MESSAGE( "TIM12 is not available on this MCU" );
#endif /* TIM12 */
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
 * - Programming delay WRHIGHFREQ 00 / 01 / 10 for 0 - 1 / 2 - 3 / 4 - 5 wait states (RM0481
 *   Table 48, bug AB#2111 - WRHIGHFREQ was never written).
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
    /* Programming delay (WRHIGHFREQ) of the wait states, RM0481 Table 48 */
    const uint32_t progDelayLut[ 6u ] =
    {
        UT_RCC_PROG_DELAY_00,   UT_RCC_PROG_DELAY_00,
        FLASH_ACR_WRHIGHFREQ_0, FLASH_ACR_WRHIGHFREQ_0,
        FLASH_ACR_WRHIGHFREQ_1, FLASH_ACR_WRHIGHFREQ_1
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
            TEST_ASSERT_EQUAL_HEX32_MESSAGE( progDelayLut[ band ],
                                             FLASH->ACR & FLASH_ACR_WRHIGHFREQ,
                                             "Programming delay, upper limit of the band" );

            config.HSE_Frequency_Hz = scaleLut[ scaleIdx ].Limit[ band ] + 1u;

            if( ( scaleLut[ scaleIdx ].BandCnt - 1u ) > band )
            {
                TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
                TEST_ASSERT_EQUAL_HEX32_MESSAGE( latencyLut[ band + 1u ], LL_FLASH_GetLatency(), "Limit + 1 Hz" );
                TEST_ASSERT_EQUAL_HEX32_MESSAGE( progDelayLut[ band + 1u ],
                                                 FLASH->ACR & FLASH_ACR_WRHIGHFREQ,
                                                 "Programming delay, limit + 1 Hz" );
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
 * - Input range 8 - 16 MHz, VCO range wide (medium VCO needs a 1 - 2 MHz reference, bug AB#2109),
 *   PLL1ON set.
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
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLVCORANGE_WIDE,
                              ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1VCOSEL ) >> RCC_PLL1CFGR_PLL1VCOSEL_Pos );
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
 * \brief   A stopped PLL has no output frequency (AB#2234).
 *
 * \details HW model sets ready flags. Configures PLL1 with default configuration, stops it and
 *          reads the Q output frequency and the kernel clock of RNG clocked by PLL1 Q into
 *          variables preset to 160 MHz - the divider registers keep the configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_OK and 0 Hz for both requests.
 */
void Ut_Rcc_Get_PeriphClk_Pll1Stopped_ReturnsZero( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = UT_RCC_PLL1P_DEFAULT_HZ;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_1 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, pllClk );

    pllClk = UT_RCC_PLL1P_DEFAULT_HZ;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLL1Q, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, pllClk );
}


/**
 * \brief   A disabled PLL output has no frequency (AB#2234).
 *
 * \details HW model sets ready flags. Configures PLL1 with default configuration except the Q
 *          output (Q divider 0 - output not enabled) and reads the Q and R output frequencies.
 *
 * \par Expected results
 * - Q output: RCC_REQUEST_OK and 0 Hz.
 * - R output: RCC_REQUEST_OK and 160 MHz (VCO 320 MHz / 2).
 */
void Ut_Rcc_Get_PllClk_OutQ_OutputDisabled_ReturnsZero( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = UT_RCC_PLL1P_DEFAULT_HZ;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].Q_Divider = 0u;
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, pllClk );

    pllClk = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, pllClk );
}


/**
 * \brief   Rcc_Set_PllConfig() disables the outputs that are not used (bug AB#2279).
 *
 * \details HW model sets ready flags. Configures PLL1 with default configuration (P, Q, R used),
 *          then again with Q and R divider 0 (not used), then with the R output used again.
 *
 * \par Expected results
 * - Default configuration: PLL1PEN, PLL1QEN and PLL1REN set.
 * - Q and R not used: RCC_REQUEST_OK, PLL1PEN set, PLL1QEN and PLL1REN cleared, Q output 0 Hz.
 * - R used again: PLL1PEN and PLL1REN set, PLL1QEN cleared.
 */
void Ut_Rcc_Set_PllConfig_UnusedOutputs_Disabled( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = UT_RCC_PLL1P_DEFAULT_HZ;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_HIGH( RCC_PLL1CFGR_PLL1PEN | RCC_PLL1CFGR_PLL1QEN | RCC_PLL1CFGR_PLL1REN, RCC->PLL1CFGR );

    config.Pll_Config[ RCC_PLL_1 ].Q_Divider = 0u;
    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_HIGH( RCC_PLL1CFGR_PLL1PEN, RCC->PLL1CFGR );
    TEST_ASSERT_BITS_LOW( RCC_PLL1CFGR_PLL1QEN | RCC_PLL1CFGR_PLL1REN, RCC->PLL1CFGR );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, pllClk );

    config.Pll_Config[ RCC_PLL_1 ].R_Divider = UT_RCC_DEFAULT_PLL_DIV;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_HIGH( RCC_PLL1CFGR_PLL1PEN | RCC_PLL1CFGR_PLL1REN, RCC->PLL1CFGR );
    TEST_ASSERT_BITS_LOW( RCC_PLL1CFGR_PLL1QEN, RCC->PLL1CFGR );
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
 * \brief   Rcc_Set_PllConfig() selects the VCO range by the reference and the VCO frequency.
 *
 * \details HW model sets ready flags. PLL1 from HSI (64 MHz in the emulated registers) is
 *          configured with different M / N values (bug AB#2109 - the medium VCO range was
 *          selected by the VCO frequency only, wide limits 192 - 836 MHz). DS14258 Tables 48 / 49:
 *          medium VCO 150 - 420 MHz with a 1 - 2 MHz reference, wide VCO 128 - 560 MHz with a
 *          2 - 16 MHz reference.
 *
 * \par Expected results
 * - Reference 1.6 MHz, VCO 320 MHz: RCC_REQUEST_OK, medium VCO range.
 * - Reference 8 MHz, VCO 160 MHz (inside the medium VCO limits too): RCC_REQUEST_OK, wide VCO range.
 * - Reference 8 MHz, VCO 600 MHz (above the wide range): RCC_REQUEST_ERROR, PLL1 is not enabled.
 * - Reference 1.6 MHz, VCO 480 MHz (above the medium range, reference too low for the wide
 *   range): RCC_REQUEST_ERROR, PLL1 is not enabled.
 */
void Ut_Rcc_Set_PllConfig_VcoRangeFromReferenceAndVco( void )
{
    rcc_ConfigStruct_t config;
    uint32_t           vcoRange = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    config.Pll_Config[ RCC_PLL_1 ].M_Divider    = UT_RCC_PLL_M_REF_1M6;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = UT_RCC_PLL_N_VCO_320M;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    vcoRange = ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1VCOSEL ) >> RCC_PLL1CFGR_PLL1VCOSEL_Pos;
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLVCORANGE_MEDIUM, vcoRange );

    config.Pll_Config[ RCC_PLL_1 ].M_Divider    = UT_RCC_PLL_M_REF_8M;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = UT_RCC_PLL_N_VCO_160M;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    vcoRange = ( RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1VCOSEL ) >> RCC_PLL1CFGR_PLL1VCOSEL_Pos;
    TEST_ASSERT_EQUAL_UINT32( LL_RCC_PLLVCORANGE_WIDE, vcoRange );

    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = UT_RCC_PLL_N_VCO_600M;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );

    config.Pll_Config[ RCC_PLL_1 ].M_Divider    = UT_RCC_PLL_M_REF_1M6;
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = UT_RCC_PLL_N_VCO_480M;
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
 *          LSE activation does not wait for the ready flag - the state is read repeatedly
 *          until the HW model sets the flag.
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

        state = RCC_FUNCTION_INACTIVE;

        for( uint32_t readIdx = 0u;
             ( UT_RCC_STATE_READ_CNT > readIdx ) &&
             ( RCC_FUNCTION_INACTIVE == state  );
             readIdx++ )
        {
            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        }

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

/* ============================ LSE OSCILLATOR ============================== */

/**
 * \brief   LSE crystal is enabled through the oscillator interface without waiting for start-up.
 *
 * \details PWR_DBPCR.DBP = 0 (RCC_BDCR is write protected on HW), no HW model - LSE ready
 *          flag is not set. Activates LSE with default configuration (crystal, medium-low
 *          drive) and reads its state (bug AB#1560 - RCC_OSC_LSE was missing). Then sets
 *          RCC_BDCR.LSERDY (crystal stable) and reads the state again.
 *
 * \par Expected results
 * - RCC_REQUEST_OK without waiting for LSERDY, PWR_DBPCR.DBP set, RCC_BDCR.LSEON set,
 *   LSEBYP / LSEEXT cleared, LSEDRV medium-low, state inactive (not ready yet).
 * - State active after LSERDY is set.
 */
void Ut_Rcc_Set_OscActive_LseCrystal_StartUpNotAwaited( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEBYP | RCC_BDCR_LSEEXT | RCC_BDCR_LSERDY, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_LSEDRIVE_MEDIUMLOW, RCC->BDCR & RCC_BDCR_LSEDRV );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->BDCR |= RCC_BDCR_LSERDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   LSE is deactivated through the oscillator interface.
 *
 * \details PWR_DBPCR.DBP = 0, RCC_BDCR.LSEON = 1 preset. HW model clears the ready flag.
 *          Deactivates LSE and reads its state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.LSEON cleared, state inactive.
 */
void Ut_Rcc_Set_OscInactive_Lse_StoppedWithBackupDomainAccess( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_LSE ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_LSE, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Rcc_Init() enables LSE with the configured type and drive.
 *
 * \details HW model sets ready flags. Initializes the module with default configuration,
 *          LSE digital bypass and highest drive.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR.LSEON, LSEBYP and LSEEXT set, LSEDRV highest.
 */
void Ut_Rcc_Init_LseDigitalBypass_LseEnabledWithConfig( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    config.LSE_ClockType = RCC_LSE_TYPE_SIG_DIGITAL_IN;
    config.LSE_Drive     = RCC_LSE_DRIVE_HIGH;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSEON | RCC_BDCR_LSEBYP | RCC_BDCR_LSEEXT, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_LSEDRIVE_HIGH, RCC->BDCR & RCC_BDCR_LSEDRV );
}


/**
 * \brief   Rcc_Init() without LSE input leaves LSE untouched.
 *
 * \details RCC_BDCR.LSEON = 1, LSERDY = 1, LSEDRV medium-high preset (LSE kept running in
 *          backup domain). HW model sets ready flags. Initializes the module with default
 *          configuration and LSE type none.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR LSE bits unchanged.
 */
void Ut_Rcc_Init_LseTypeNone_LseUntouched( void )
{
    const uint32_t     bdcrLse  = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | LL_RCC_LSEDRIVE_MEDIUMHIGH;
    const uint32_t     lseMask  = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | RCC_BDCR_LSEDRV | RCC_BDCR_LSEBYP | RCC_BDCR_LSEEXT;
    rcc_ConfigStruct_t config;

    RCC->BDCR = bdcrLse;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    config.LSE_ClockType = RCC_LSE_TYPE_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( bdcrLse, RCC->BDCR & lseMask );
}


/**
 * \brief   Rcc_Init() rejects invalid LSE configuration.
 *
 * \details HW model sets ready flags. Initializes the module with LSE type out of range,
 *          then with LSE drive out of range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for both configurations.
 */
void Ut_Rcc_Init_LseConfigOutOfRange_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    config.LSE_ClockType = RCC_LSE_TYPE_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    config.LSE_ClockType = RCC_LSE_TYPE_CRYSTAL;
    config.LSE_Drive     = RCC_LSE_DRIVE_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
}


/**
 * \brief   LSE crystal clears LSEEXT and applies the drive capability.
 *
 * \details RCC_BDCR.LSEEXT = 1 and LSEBYP = 1 preset (LSEEXT reset value of STM32H543 /
 *          H553), LSE stopped. Configures LSE crystal with medium-high drive and activates LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR.LSEON set, LSEBYP and LSEEXT cleared, LSEDRV medium-high.
 */
void Ut_Rcc_Set_OscActive_LseCrystal_BypassAndLseExtCleared( void )
{
    RCC->BDCR = RCC_BDCR_LSEEXT | RCC_BDCR_LSEBYP;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_CRYSTAL, RCC_LSE_DRIVE_MEDIUM_HIGH ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEBYP | RCC_BDCR_LSEEXT, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_LSEDRIVE_MEDIUMHIGH, RCC->BDCR & RCC_BDCR_LSEDRV );
}


/**
 * \brief   Running LSE with forbidden lowest drive is restarted with the configured drive.
 *
 * \details RCC_BDCR.LSEON = 1, LSERDY = 1, LSEDRV = 00 preset (LSE started by another
 *          firmware, kept in backup domain). HW model follows LSEON. Activates LSE with
 *          default configuration (crystal, medium-low drive).
 *
 * \note    Device errata bug AB#1560 (ES0561 2.2.5, ES0565 2.2.17, ES0621 2.2.8 "LSE low drive
 *          mode is not functional"): LSE may not start or may stop with LSEDRV = 00, the mode
 *          is forbidden - the module never keeps LSE enabled with the lowest drive.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR.LSEON set, LSEDRV medium-low.
 */
void Ut_Rcc_Set_OscActive_LseRunningLowestDrive_RestartedWithMediumLow( void )
{
    RCC->BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_LSEDRIVE_MEDIUMLOW, RCC->BDCR & RCC_BDCR_LSEDRV );
}


/**
 * \brief   LSE type and drive lists contain only functional modes.
 *
 * \details Checks the count of entries of rcc_LseType_t and rcc_LseDrive_t.
 *
 * \note    Device errata bug AB#1560 (ES0561 2.2.5 / 2.2.16, ES0565 2.2.17 / 2.2.25,
 *          ES0621 2.2.8 / 2.2.11 "LSE low drive mode is not functional", "Low-speed external
 *          clock in analog bypass mode might not work properly"): lowest drive (LSEDRV = 00)
 *          and analog bypass (LSEBYP = 1, LSEEXT = 0) are not offered.
 *
 * \par Expected results
 * - 3 LSE types (none, crystal, digital bypass), 3 drives (medium-low, medium-high, highest).
 */
void Ut_Rcc_LseLists_ErrataModesNotOffered( void )
{
    TEST_ASSERT_EQUAL_UINT32( 3u, (uint32_t)RCC_LSE_TYPE_CNT );
    TEST_ASSERT_EQUAL_UINT32( 3u, (uint32_t)RCC_LSE_DRIVE_CNT );
}


/**
 * \brief   Running LSE with the requested configuration is kept.
 *
 * \details RCC_BDCR.LSEON = 1, LSERDY = 1, LSEDRV medium-low preset (LSE kept running in
 *          backup domain over system reset), no HW model - stopped LSE would never
 *          report ready flag low. Configures LSE crystal with medium-low drive and
 *          activates LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR unchanged (LSE not restarted).
 */
void Ut_Rcc_Set_OscActive_LseRunningSameConfig_LseKept( void )
{
    const uint32_t bdcrRunning = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | LL_RCC_LSEDRIVE_MEDIUMLOW;

    RCC->BDCR = bdcrRunning;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_CRYSTAL, RCC_LSE_DRIVE_MEDIUM_LOW ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_EQUAL_HEX32( bdcrRunning, RCC->BDCR );
}


/**
 * \brief   Starting LSE with the requested configuration is kept.
 *
 * \details RCC_BDCR.LSEON = 1, LSERDY = 0, LSEDRV medium-low preset (crystal start-up in
 *          progress, e.g. LSE enabled by Rcc_Init()). Activates LSE with default configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR unchanged (start-up is not restarted).
 */
void Ut_Rcc_Set_OscActive_LseStartingSameConfig_LseKept( void )
{
    const uint32_t bdcrStarting = RCC_BDCR_LSEON | LL_RCC_LSEDRIVE_MEDIUMLOW;

    RCC->BDCR = bdcrStarting;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_EQUAL_HEX32( bdcrStarting, RCC->BDCR );
}


/**
 * \brief   Running LSE with another configuration is restarted with the requested one.
 *
 * \details RCC_BDCR.LSEON = 1, LSERDY = 1, LSEDRV medium-low preset. HW model follows LSEON.
 *          Configures LSE crystal with highest drive and activates LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RCC_BDCR.LSEON set, LSEDRV highest, LSEBYP cleared.
 */
void Ut_Rcc_Set_OscActive_LseRunningOtherConfig_LseRestarted( void )
{
    RCC->BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY | LL_RCC_LSEDRIVE_MEDIUMLOW;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_CRYSTAL, RCC_LSE_DRIVE_HIGH ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEBYP, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_LSEDRIVE_HIGH, RCC->BDCR & RCC_BDCR_LSEDRV );
}


/**
 * \brief   LSE without input is not activated.
 *
 * \details Configures LSE type none, activates LSE. Then calls the configuration with
 *          type and drive out of range.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_ERROR, RCC_BDCR.LSEON cleared.
 * - Configuration out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_OscActive_LseTypeNone_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_NONE, RCC_LSE_DRIVE_MEDIUM_LOW ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( RCC_OSC_LSE ) );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_CNT, RCC_LSE_DRIVE_MEDIUM_LOW ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_LseConfig( RCC_LSE_TYPE_CRYSTAL, RCC_LSE_DRIVE_CNT ) );
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


/**
 * \brief   Backup domain reset pulses RCC_BDCR.VSWRST with the write access released.
 *
 * \details PWR_DBPCR.DBP = 0 (reset state - RCC_BDCR is write protected on HW). Resets the
 *          backup domain.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.VSWRST cleared at the end.
 */
void Ut_Rcc_Set_BackupDomainReset_WriteAccess_ResetBitCleared( void )
{
    TEST_ASSERT_BITS_LOW( PWR_DBPCR_DBP, PWR->DBPCR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_BackupDomainReset() );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_VSWRST, RCC->BDCR );
}

/* ====================== BACKUP DOMAIN WRITE ACCESS ======================== */

/**
 * \brief   LSI activation releases the backup domain write protection.
 *
 * \details PWR_DBPCR.DBP = 0 (reset state - RCC_BDCR is write protected on HW).
 *          HW model sets oscillator ready flag. Activates LSI.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.LSION set.
 */
void Ut_Rcc_Set_OscActive_Lsi_ReleasesBackupDomainWriteAccess( void )
{
    TEST_ASSERT_BITS_LOW( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSI ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSION, RCC->BDCR );
}


/**
 * \brief   LSI deactivation releases the backup domain write protection.
 *
 * \details PWR_DBPCR.DBP = 0, RCC_BDCR.LSION = 1 preset. Deactivates LSI.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.LSION cleared.
 */
void Ut_Rcc_Set_OscInactive_Lsi_ReleasesBackupDomainWriteAccess( void )
{
    RCC->BDCR = RCC_BDCR_LSION;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_LSI ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSION, RCC->BDCR );
}


/**
 * \brief   Backup domain write access already released is kept.
 *
 * \details PWR_DBPCR.DBP = 1 preset. HW model sets oscillator ready flag. Activates LSI.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR = DBP only, RCC_BDCR.LSION set.
 */
void Ut_Rcc_Set_OscActive_Lsi_BackupDomainAccessReleased_AccessKept( void )
{
    PWR->DBPCR = PWR_DBPCR_DBP;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSI ) );

    TEST_ASSERT_EQUAL_HEX32( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_LSION, RCC->BDCR );
}


/**
 * \brief   RTC clock source selection releases the backup domain write protection.
 *
 * \details PWR_DBPCR.DBP = 0. Sets RTC clock source LSI.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.RTCSEL = LSI.
 */
void Ut_Rcc_Set_RtcClkSource_Lsi_ReleasesBackupDomainWriteAccess( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSI ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   RTC peripheral activation releases the backup domain write protection.
 *
 * \details PWR_DBPCR.DBP = 0. HW model sets oscillator ready flag. Enables RTC with
 *          LSI clock (RTCSEL written through the clock multiplexer table).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.RTCSEL = LSI, RCC_BDCR.RTCEN set.
 */
void Ut_Rcc_Set_PeriphActive_RtcLsi_ReleasesBackupDomainWriteAccess( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );
    TEST_ASSERT_BITS_HIGH( RCC_BDCR_RTCEN, RCC->BDCR );
}


/**
 * \brief   LSCO source selection releases the backup domain write protection.
 *
 * \details PWR_DBPCR.DBP = 0. Sets LSCO source LSE, GPIO mock accepts the pin
 *          initialization.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR.DBP set, RCC_BDCR.LSCOSEL = LSE.
 */
void Ut_Rcc_Set_ClkOutSource_LscoLse_ReleasesBackupDomainWriteAccess( void )
{
    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_LSCO_LSE ) );

    TEST_ASSERT_BITS_HIGH( PWR_DBPCR_DBP, PWR->DBPCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_LSCO_CLKSOURCE_LSE, RCC->BDCR & RCC_BDCR_LSCOSEL );
}


/**
 * \brief   Write of register outside of the backup domain keeps the write protection.
 *
 * \details PWR_DBPCR.DBP = 0. Sets MCO1 source HSE (RCC_CFGR1), GPIO mock accepts
 *          the pin initialization.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PWR_DBPCR stays 0.
 */
void Ut_Rcc_Set_ClkOutSource_Mco1Hse_BackupDomainAccessNotChanged( void )
{
    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_HSE ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, PWR->DBPCR );
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
 * - RCC_REQUEST_OK, SWS = PLL1, PLL1 enabled, HSE and LSE not enabled (bug AB#2115).
 * - Flash prefetch enabled, latency 3 wait states, programming delay 01 (bug AB#2111), ICACHE enabled
 *   in the 2-way mode (bug AB#2112).
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
    TEST_ASSERT_BITS_HIGH( RCC_CR_PLL1ON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_BITS_HIGH( FLASH_ACR_PRFTEN, FLASH->ACR );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_WRHIGHFREQ_0, FLASH->ACR & FLASH_ACR_WRHIGHFREQ );
    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL_HEX32( LL_ICACHE_2WAYS, LL_ICACHE_GetMode() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL1P_DEFAULT_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Init() keeps an enabled ICACHE as it is.
 *
 * \details HW model sets ready flags and switch status. ICACHE is enabled in the 1-way
 *          mode before Rcc_Init() with default configuration (bug AB#2112 - Rcc_Init()
 *          wrote WAYSEL also while the cache was enabled; RM0481 8.4.4: WAYSEL is writable
 *          only with EN = 0).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, ICACHE stays enabled in the 1-way mode.
 */
void Ut_Rcc_Init_IcacheEnabled_KeptAsIs( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    LL_ICACHE_SetMode( LL_ICACHE_1WAY );
    LL_ICACHE_Enable();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_UINT32( 1u, LL_ICACHE_IsEnabled() );
    TEST_ASSERT_EQUAL_HEX32( LL_ICACHE_1WAY, LL_ICACHE_GetMode() );
}


/**
 * \brief   Rcc_Init() stops when oscillator does not start.
 *
 * \details Regulator ready flag preset, without HW model (no oscillator becomes ready).
 *          Initializes the module with default configuration and HSE crystal.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock is not switched, PLL1 is not enabled.
 */
void Ut_Rcc_Init_HseNotStarting_ReturnsErrorBeforeSysClkSwitch( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;

    /* Voltage scaling ready, but no oscillator ever becomes ready */
    PWR->VOSSR = PWR_VOSSR_VOSRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_LOW( RCC_CFGR1_SW, RCC->CFGR1 );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   Rcc_Init() configures HSE input type, bypass of a previous configuration
 *          is not kept.
 *
 * \details HW model sets ready flags. Rcc_Init() with default configuration and HSE
 *          digital input, analog input and crystal, in this order (GPIO mock ignored;
 *          bug AB#976 - crystal configuration kept HSEBYP of the external clock).
 *
 * \par Expected results
 * - Digital input: RCC_REQUEST_OK, HSEON, HSEBYP and HSEEXT set.
 * - Analog input: RCC_REQUEST_OK, HSEON and HSEBYP set, HSEEXT cleared.
 * - Crystal: RCC_REQUEST_OK, HSEON set, HSEBYP cleared.
 */
void Ut_Rcc_Init_HseType_BypassConfigured( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    config.HSE_ClockType = RCC_HSE_TYPE_SIG_DIGITAL_IN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_HSEEXT, RCC->CR );

    config.HSE_ClockType = RCC_HSE_TYPE_SIG_ANALOG_IN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_HSEBYP, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEEXT, RCC->CR );

    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEBYP, RCC->CR );
}


/**
 * \brief   Rcc_Init() enables the clock security system of HSE.
 *
 * \details HW model sets ready flags (GPIO mock ignored; bug AB#978 - CSS_Enable of the
 *          configuration was ignored, HSECSSON was never set).
 *          1. Default configuration with HSE crystal (CSS inactive).
 *          2. CSS enabled, HSE not used.
 *          3. CSS enabled, HSE crystal.
 *          4. Rcc_Init() repeated with the same configuration.
 *
 * \par Expected results
 * 1. RCC_REQUEST_OK, HSEON set, HSECSSON not set.
 * 2. RCC_REQUEST_ERROR (CSS needs HSE), HSECSSON not set.
 * 3. RCC_REQUEST_OK, HSEON and HSECSSON set.
 * 4. RCC_REQUEST_OK, HSEON, HSERDY and HSECSSON set.
 */
void Ut_Rcc_Init_CssEnabled_RequiresHse( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSECSSON, RCC->CR );

    config.CSS_Enable    = RCC_FUNCTION_ACTIVE;
    config.HSE_ClockType = RCC_HSE_TYPE_NONE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSECSSON, RCC->CR );

    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_HSECSSON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSECSSON, RCC->CR );
}


/**
 * \brief   Rcc_Init() rejects invalid state of the clock security system.
 *
 * \details HW model sets ready flags. Initializes the module with default configuration
 *          and CSS state out of the rcc_FunctionState_t list.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, HSECSSON not set, system clock is not switched to PLL1.
 */
void Ut_Rcc_Init_CssStateOutOfRange_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    config.CSS_Enable = UT_RCC_FUNCTION_STATE_INVALID;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_LOW( RCC_CR_HSECSSON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CFGR1_SW, RCC->CFGR1 );
}


/**
 * \brief   Rcc_Init() repeated while PLL1 drives SYSCLK moves SYSCLK to HSI before it lowers
 *          the voltage scale and the flash wait states.
 *
 * \details HW model with the monitor of the order of register writes (RCC_CFGR1.SW, PWR_VOSCR.VOS,
 *          FLASH_ACR.LATENCY). Rcc_Init() with default configuration (PLL1 160 MHz, voltage scale 0,
 *          3 wait states), then Rcc_Init() repeated with HSE 8 MHz as SYSCLK at voltage scale 3
 *          (0 wait states). The old clock must never run with the lowered voltage scale or
 *          wait states (STM32H5 Rcc bug: the voltage scale and the wait states were lowered before
 *          SYSCLK was moved to HSI). The test is ignored where the HW model does not follow every
 *          write (REGMEM_MODEL_MODE_THREAD).
 *
 * \par Expected results
 * - Both calls: RCC_REQUEST_OK.
 * - The voltage scale and the wait states are lowered once each, SW = HSI at that time.
 * - The first change is the raise of the wait states to 5 (PLL1 still requested), the second change
 *   moves SYSCLK to HSI.
 * - Final state: SWS = HSE, voltage scale 3, 0 wait states, SystemCoreClock = 8 MHz.
 */
void Ut_Rcc_Init_ReinitFromPll1ToLowScale_SysClkToHsiBeforeVoltageAndLatencyLowered( void )
{
    rcc_ConfigStruct_t config;
    uint32_t           loweredVosCnt     = 0u;
    uint32_t           loweredLatencyCnt = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    UtRcc_Clear_OrderLog();

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( UtRcc_HwModelOrder ) );

    /* Order of the writes is observed by the synchronous model only */
    const regMem_ModelMode_t modelMode = RegMem_Get_ModelMode();

    if( REGMEM_MODEL_MODE_SYNC != modelMode )
    {
        TEST_IGNORE_MESSAGE( "Order of register writes is observed in the synchronous HW model mode only" );
    }
    else
    {
        /* Every write is followed by the model */
    }

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    /* First initialization: PLL1 160 MHz, voltage scale 0, 3 wait states */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, RCC->CFGR1 & RCC_CFGR1_SWS );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE0, PWR->VOSCR & PWR_VOSCR_VOS );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_3, LL_FLASH_GetLatency() );

    /* Second initialization while PLL1 drives SYSCLK: HSE 8 MHz, voltage scale 3, 0 wait states */
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_3;

    UtRcc_Clear_OrderLog();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_GREATER_THAN_UINT32( UT_RCC_ORDER_TRANSITION_CNT, utRcc_OrderLogCnt );
    TEST_ASSERT_LESS_THAN_UINT32( UT_RCC_ORDER_LOG_SIZE, utRcc_OrderLogCnt );

    for( uint32_t recIdx = 0u; utRcc_OrderLogCnt > recIdx; recIdx++ )
    {
        const utRcc_OrderRecord_t record = utRcc_OrderLog[ recIdx ];

        if( ( UT_RCC_ORDER_FIELD_SYSCLK_SW != record.Field ) &&
            ( record.Old                    > record.Value ) )
        {
            /* Voltage scale or wait states lowered: the old SYSCLK source must be left already */
            TEST_ASSERT_EQUAL_HEX32_MESSAGE( LL_RCC_SYS_CLKSOURCE_HSI, record.SysClkSw, UT_RCC_ORDER_LOWERED_MSG );

            if( UT_RCC_ORDER_FIELD_VOS == record.Field )
            {
                loweredVosCnt++;
            }
            else
            {
                loweredLatencyCnt++;
            }
        }
        else
        {
            /* SYSCLK source switch or raised voltage scale / wait states */
        }
    }

    TEST_ASSERT_EQUAL_UINT32( 1u, loweredVosCnt );
    TEST_ASSERT_EQUAL_UINT32( 1u, loweredLatencyCnt );

    /* Wait states valid for every clock are set while the old clock still runs, then SYSCLK moves to HSI */
    TEST_ASSERT_EQUAL( UT_RCC_ORDER_FIELD_LATENCY, utRcc_OrderLog[ 0u ].Field );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_5, utRcc_OrderLog[ 0u ].Value );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_PLL1, utRcc_OrderLog[ 0u ].SysClkSw );
    TEST_ASSERT_EQUAL( UT_RCC_ORDER_FIELD_SYSCLK_SW, utRcc_OrderLog[ 1u ].Field );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_HSI, utRcc_OrderLog[ 1u ].Value );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSE, RCC->CFGR1 & RCC_CFGR1_SWS );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE3, PWR->VOSCR & PWR_VOSCR_VOS );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() repeated with a frequency over the limit of the voltage scale leaves SYSCLK on HSI.
 *
 * \details HW model sets ready flags. Rcc_Init() with default configuration (PLL1 160 MHz), then
 *          Rcc_Init() repeated with voltage scale 3 (limit 100 MHz) and the same PLL1 as SYSCLK.
 *
 * \par Expected results
 * - First call: RCC_REQUEST_OK, SWS = PLL1.
 * - Second call: RCC_REQUEST_ERROR (frequency is rejected before PLL1 is reconfigured and SYSCLK is
 *   switched), SWS = HSI (the old 160 MHz clock does not run at voltage scale 3),
 *   5 wait states (valid for every clock).
 */
void Ut_Rcc_Init_ReinitFrequencyOverScaleLimit_ReturnsErrorWithSysClkOnHsi( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );  /* MCO pins, see ClkOut defect test */

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, RCC->CFGR1 & RCC_CFGR1_SWS );

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_3;    /* PLL1 160 MHz is over the limit of scale 3 */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSI, RCC->CFGR1 & RCC_CFGR1_SWS );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_5, LL_FLASH_GetLatency() );
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


/**
 * \brief   Rcc_Set_PeriphActive() sets both enable bits of the Playback engine 1 (PLAY1) block.
 *
 * \details Enables PLAY1 with APB3 (PCLK3) kernel clock on devices with PLAY1
 *          (AB#1845). On other devices the block does not exist, test is a no-op.
 *
 * \par Expected results
 * - APB3ENR.PLAY1EN and APB3ENR.PLAY1APBEN are both set by a single request.
 */
void Ut_Rcc_Set_PeriphActive_Play1Pclk3_BothEnableBitsSet( void )
{
#if defined(PLAY1)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_PLAY1_PCLK3 ) );

    TEST_ASSERT_BITS_HIGH( RCC_APB3ENR_PLAY1EN | RCC_APB3ENR_PLAY1APBEN, RCC->APB3ENR );
#else
    /* PLAY1 not present on this device line */
#endif
}


/**
 * \brief   Rcc_Set_PeriphActive() selects the ETH RMII reference feedback source.
 *
 * \details Enables the Ethernet RMII reference clock with the eth_clk_fb loopback
 *          as source (AB#1845). The LL driver names this field's two states
 *          "PLL1Q"/"PLL1R", which contradicts the RM description of the bit, so
 *          this test proves the module-local value (not the misleading LL name)
 *          ends up in CCIPR5.ETHREFCLKSEL. On other devices the mux does not
 *          exist, test is a no-op.
 *
 * \par Expected results
 * - CCIPR5.ETHREFCLKSEL is set (eth_clk_fb selected).
 */
void Ut_Rcc_Set_PeriphActive_EthRefClkFb_SelectsFeedbackBit( void )
{
#if defined(RCC_CCIPR5_ETHREFCLKSEL)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ETH_REF_CLK_FB ) );

    TEST_ASSERT_BITS_HIGH( RCC_CCIPR5_ETHREFCLKSEL, RCC->CCIPR5 );
#else
    /* ETH RMII reference mux not present on this device line */
#endif
}


/**
 * \brief   Rcc_Set_PeriphActive() selects OCTOSPI2 kernel clock through CCIPR5 (not CCIPR4).
 *
 * \details Enables OCTOSPI2 with PLL2 R kernel clock (AB#1845). OCTOSPI1 shares a
 *          mux field in CCIPR4, OCTOSPI2 has its own field in CCIPR5; this test
 *          proves the correct register is used. On other devices OCTOSPI2 does
 *          not exist, test is a no-op.
 *
 * \par Expected results
 * - CCIPR5.OCTOSPI2SEL = PLL2R, AHB4ENR.OCTOSPI2EN is set.
 */
void Ut_Rcc_Set_PeriphActive_Octospi2Pll2R_SelectsCcipr5Mux( void )
{
#if defined(OCTOSPI2)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_OCTOSPI2_PLL2R ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR5_OCTOSPI2SEL_1, RCC->CCIPR5 & RCC_CCIPR5_OCTOSPI2SEL );
    TEST_ASSERT_BITS_HIGH( RCC_AHB4ENR_OCTOSPI2EN, RCC->AHB4ENR );
#else
    /* OCTOSPI2 not present on this device line */
#endif
}


/**
 * \brief   The I/O manager OCTOSPIM has its own clock enable and reset bit (AHB4ENR / AHB4RSTR bit 22).
 *
 * \details Activates RCC_PERIPH_OCTOSPIM, resets it, and deactivates it. Present only on
 *          STM32H5E4 / H5E5 / H5F4 / H5F5, on other devices the test is a no-op.
 *
 * \par Expected results
 * - AHB4ENR.OCTOSPIMEN is set after Rcc_Set_PeriphActive() and cleared after Rcc_Set_PeriphInactive().
 * - AHB4RSTR.OCTOSPIMRST is set by Rcc_Set_ResetActive() and cleared by Rcc_Set_ResetInactive().
 * - The enable bits of OCTOSPI1 / OCTOSPI2 are not touched.
 */
void Ut_Rcc_Set_PeriphActive_Octospim_TogglesEnableAndResetBit( void )
{
#if defined(OCTOSPIM)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_OCTOSPIM ) );
    TEST_ASSERT_BITS_HIGH( RCC_AHB4ENR_OCTOSPIMEN, RCC->AHB4ENR );
    TEST_ASSERT_BITS_LOW( ( RCC_AHB4ENR_OCTOSPI1EN | RCC_AHB4ENR_OCTOSPI2EN ), RCC->AHB4ENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( RCC_PERIPH_OCTOSPIM ) );
    TEST_ASSERT_BITS_HIGH( RCC_AHB4RSTR_OCTOSPIMRST, RCC->AHB4RSTR );
    TEST_ASSERT_BITS_LOW( ( RCC_AHB4RSTR_OCTOSPI1RST | RCC_AHB4RSTR_OCTOSPI2RST ), RCC->AHB4RSTR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_OCTOSPIM ) );
    TEST_ASSERT_BITS_LOW( RCC_AHB4RSTR_OCTOSPIMRST, RCC->AHB4RSTR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_OCTOSPIM ) );
    TEST_ASSERT_BITS_LOW( RCC_AHB4ENR_OCTOSPIMEN, RCC->AHB4ENR );
#else
    /* OCTOSPIM not present on this device line */
#endif
}


/**
 * \brief   Rcc_Set_PeriphActive() selects the ADF1 clock input pin (AUDIOCLK) source.
 *
 * \details Enables ADF1 with the pin source (SAI1 / SAI2 _CKIN naming convention
 *          applied to ADF1, AB#1845). On other devices ADF1 does not exist, test
 *          is a no-op.
 *
 * \par Expected results
 * - CCIPR5.ADF1SEL selects the pin source, AHB2ENR.ADF1EN is set.
 */
void Ut_Rcc_Set_PeriphActive_Adf1Ckin_SelectsPinSource( void )
{
#if defined(ADF1)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADF1_CKIN ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR5_ADF1SEL_0 | RCC_CCIPR5_ADF1SEL_1, RCC->CCIPR5 & RCC_CCIPR5_ADF1SEL );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR_ADF1EN, RCC->AHB2ENR );
#else
    /* ADF1 not present on this device line */
#endif
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


/**
 * \brief HW model of RCC with the AHB prescaler (CFGR2.HPRE) stuck at "not divided".
 *
 * Clears HPRE after every write, so the AHB divider read back differs from the written value.
 */
static void UtRcc_HwModelHpreStuck( void )
{
    (void)__atomic_and_fetch( &RCC->CFGR2, ~RCC_CFGR2_HPRE, __ATOMIC_SEQ_CST );
}


/**
 * \brief HW model of RCC and PWR extended by the monitor of the order of the AHB divider and wait states writes.
 *
 * Behaves as \ref Ut_Rcc_HwModel and records every change of RCC_CFGR2.HPRE and FLASH_ACR.LATENCY in
 * \ref utRcc_OrderLog, in the order of the writes of the code under test (synchronous model mode).
 */
static void UtRcc_HwModelAhbOrder( void )
{
    Ut_Rcc_HwModel();

    UtRcc_Log_Change( UT_RCC_ORDER_FIELD_HPRE,    RCC->CFGR2 & RCC_CFGR2_HPRE );
    UtRcc_Log_Change( UT_RCC_ORDER_FIELD_LATENCY, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief HW model of FLASH with the wait states (ACR.LATENCY) stuck at 0 wait states.
 *
 * Clears LATENCY after every write, so the wait states read back differ from the written value.
 */
static void UtRcc_HwModelLatencyStuck( void )
{
    (void)__atomic_and_fetch( &FLASH->ACR, ~FLASH_ACR_LATENCY, __ATOMIC_SEQ_CST );
}


/**
 * \brief HW model of RCC and PWR extended by the monitor of the order of register writes.
 *
 * Behaves as \ref Ut_Rcc_HwModel and records every change of RCC_CFGR1.SW, PWR_VOSCR.VOS and
 * FLASH_ACR.LATENCY in \ref utRcc_OrderLog. The model is called after every write in the
 * synchronous model mode, so the records are in the order of the writes of the code under test.
 */
static void UtRcc_HwModelOrder( void )
{
    Ut_Rcc_HwModel();

    UtRcc_Log_Change( UT_RCC_ORDER_FIELD_SYSCLK_SW, RCC->CFGR1 & RCC_CFGR1_SW );
    UtRcc_Log_Change( UT_RCC_ORDER_FIELD_VOS,       PWR->VOSCR & PWR_VOSCR_VOS );
    UtRcc_Log_Change( UT_RCC_ORDER_FIELD_LATENCY,   FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief Records the new value of a watched register field when it differs from the last seen value.
 *
 * \param field [in]: Watched register field.
 * \param value [in]: Actual value of the field.
 */
static void UtRcc_Log_Change( utRcc_OrderField_t field, uint32_t value )
{
    const uint32_t oldValue = utRcc_OrderLast[ field ];

    if( oldValue != value )
    {
        utRcc_OrderLast[ field ] = value;

        if( UT_RCC_ORDER_LOG_SIZE > utRcc_OrderLogCnt )
        {
            utRcc_OrderLog[ utRcc_OrderLogCnt ].Field    = field;
            utRcc_OrderLog[ utRcc_OrderLogCnt ].Old      = oldValue;
            utRcc_OrderLog[ utRcc_OrderLogCnt ].Value    = value;
            utRcc_OrderLog[ utRcc_OrderLogCnt ].SysClkSw = RCC->CFGR1 & RCC_CFGR1_SW;

            utRcc_OrderLogCnt++;
        }
        else
        {
            /* Log is full, the test checks the count of records */
        }
    }
    else
    {
        /* Field is unchanged */
    }
}


/** Empties the log of the order of register writes and takes the actual register values as the last seen ones */
static void UtRcc_Clear_OrderLog( void )
{
    utRcc_OrderLogCnt = 0u;

    utRcc_OrderLast[ UT_RCC_ORDER_FIELD_SYSCLK_SW ] = RCC->CFGR1 & RCC_CFGR1_SW;
    utRcc_OrderLast[ UT_RCC_ORDER_FIELD_VOS ]       = PWR->VOSCR & PWR_VOSCR_VOS;
    utRcc_OrderLast[ UT_RCC_ORDER_FIELD_LATENCY ]   = FLASH->ACR & FLASH_ACR_LATENCY;
    utRcc_OrderLast[ UT_RCC_ORDER_FIELD_HPRE ]      = RCC->CFGR2 & RCC_CFGR2_HPRE;
}
