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
 *       (eg. HSION / HSIRDY are not set, PLLCFGR is 0).
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

/* ======================= FORWARD DECLARATIONS ============================= */

static void Ut_Rcc_HwModel              ( void );
static void Ut_Rcc_HwModel_Update       ( volatile uint32_t * const reg, uint32_t mask, uint32_t value );
static void Ut_Rcc_Start_HwModel        ( void );
static void Ut_Rcc_Get_HsiOnlyConfig    ( rcc_ConfigStruct_t * const config );
static void Ut_Rcc_Get_HsePllConfig     ( rcc_ConfigStruct_t * const config );
static void Ut_Rcc_Clear_PeriphRegs     ( void );
static void Ut_Rcc_Check_OtherRegsClear ( volatile uint32_t * const * regList, uint32_t regCnt, volatile uint32_t * const usedReg );
static void Ut_Rcc_Get_PeriphRegsTable  ( const utRcc_PeriphRegs_t ** const table, uint32_t * const tableCnt );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSE frequency of tests - 8 MHz crystal (STM32F4DISCOVERY) */
#define TEST_RCC_HSE_FREQ_HZ                ( 8000000u )

/** HSE frequency of external clock signal tests (oscillator bypassed) */
#define TEST_RCC_HSE_BYPASS_FREQ_HZ         ( 25000000u )

/** HSI frequency */
#define TEST_RCC_HSI_FREQ_HZ                ( 16000000u )

/** System clock of default configuration (HSI 16 MHz / 16 * 336 / 4) */
#define TEST_RCC_DEFAULT_SYSCLK_HZ          ( 84000000u )

/** MCU with maximal system clock of at least 168 MHz (voltage scale 1) - tests of 168 MHz / 108 MHz
 *  configurations (STM32F405 / 407 / 415 / 417 / 42x / 43x / 446 / 469 / 479) */
#if defined(RCC_MAX_FREQUENCY_SCALE1)
#if ( RCC_MAX_FREQUENCY_SCALE1 >= 168000000u )
    #define TEST_RCC_168MHZ_MCU
#endif
#endif /* RCC_MAX_FREQUENCY_SCALE1 */

/** System clock of HSE PLL configuration (HSE 8 MHz / 8 * 336 / 2) */
#define TEST_RCC_HSE_PLL_SYSCLK_HZ          ( 168000000u )

/** 48 MHz clock (PLL output Q) of default and HSE PLL configuration */
#define TEST_RCC_PLL48_CLK_HZ               ( 48000000u )

/** Count of milliseconds in one second */
#define TEST_RCC_MS_IN_SECOND               ( 1000u )

/** SysTick control register value after SysTick_Config() (HCLK, interrupt, enabled) */
#define TEST_RCC_SYSTICK_CTRL_ACTIVE        ( SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk )

/** Longest SysTick interval at HSI 16 MHz (24-bit reload register) */
#define TEST_RCC_SYSTICK_MAX_MS_HSI         ( ( SysTick_LOAD_RELOAD_Msk + 1u ) / ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) )

/** Field value of PLL P divider 2 (PLLP = divider / 2 - 1) */
#define TEST_RCC_PLLP_DIV2                  ( 0u )

/** Field value of PLL P divider 4 */
#define TEST_RCC_PLLP_DIV4                  ( 1u )

/** Ready flags of RCC_CR emulated by HW model */
#if defined(RCC_CR_PLLI2SON)
    #define TEST_RCC_CR_RDY_MASK            ( RCC_CR_HSIRDY | RCC_CR_HSERDY | RCC_CR_PLLRDY | RCC_CR_PLLI2SRDY )
#else
    #define TEST_RCC_CR_RDY_MASK            ( RCC_CR_HSIRDY | RCC_CR_HSERDY | RCC_CR_PLLRDY )
#endif /* RCC_CR_PLLI2SON */

/** All reset source flags of RCC_CSR */
#define TEST_RCC_CSR_RESET_FLAGS            ( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_PORRSTF | RCC_CSR_SFTRSTF | \
                                              RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF )

/** Flash wait states of the maximal clock of voltage scale 2 (one wait state per 30 MHz of HCLK at
 *  2.7 - 3.6 V, e.g. 144 MHz STM32F407: 4, 168 MHz STM32F446: 5, 84 MHz STM32F401 / F411: 2) */
#define TEST_RCC_SCALE2_MAX_LATENCY         ( ( ( RCC_MAX_FREQUENCY_SCALE2 - 1u ) / 30000000u ) << FLASH_ACR_LATENCY_Pos )

/** Field value of MCOxPRE prescaler for divider 4 (100 - /2, 101 - /3, 110 - /4, 111 - /5) */
#define TEST_RCC_MCOPRE_DIV4                ( 0x6u )

/* ============================== MACROS ==================================== */

/** PLLCFGR register value of main PLL */
#define TEST_RCC_PLLCFGR( m, n, pField, q, src )    ( ( (uint32_t)( m )      << RCC_PLLCFGR_PLLM_Pos ) | \
                                                      ( (uint32_t)( n )      << RCC_PLLCFGR_PLLN_Pos ) | \
                                                      ( (uint32_t)( pField ) << RCC_PLLCFGR_PLLP_Pos ) | \
                                                      ( (uint32_t)( q )      << RCC_PLLCFGR_PLLQ_Pos ) | \
                                                      ( src ) )

/** Count of items of an array */
#define TEST_RCC_ARRAY_CNT( array )         ( sizeof( array ) / sizeof( ( array )[ 0u ] ) )

/* ========================== LOCAL VARIABLES =============================== */

/** HW model emulates HSE which does not start (crystal missing) */
static volatile bool utRcc_HseFails = false;

/** Peripherals of every register bank (enable, reset and sleep bits) */
static const utRcc_PeriphRegs_t utRcc_PeriphRegs[] =
{
    { RCC_PERIPH_GPIOA,      &RCC->AHB1ENR, RCC_AHB1ENR_GPIOAEN,  &RCC->AHB1RSTR, RCC_AHB1RSTR_GPIOARST,  &RCC->AHB1LPENR, RCC_AHB1LPENR_GPIOALPEN  },
    { RCC_PERIPH_DMA2,       &RCC->AHB1ENR, RCC_AHB1ENR_DMA2EN,   &RCC->AHB1RSTR, RCC_AHB1RSTR_DMA2RST,   &RCC->AHB1LPENR, RCC_AHB1LPENR_DMA2LPEN   },
    { RCC_PERIPH_CRC,        &RCC->AHB1ENR, RCC_AHB1ENR_CRCEN,    &RCC->AHB1RSTR, RCC_AHB1RSTR_CRCRST,    &RCC->AHB1LPENR, RCC_AHB1LPENR_CRCLPEN    },
#if defined(RCC_AHB2ENR_OTGFSEN)
    { RCC_PERIPH_USB_OTG_FS, &RCC->AHB2ENR, RCC_AHB2ENR_OTGFSEN,  &RCC->AHB2RSTR, RCC_AHB2RSTR_OTGFSRST,  &RCC->AHB2LPENR, RCC_AHB2LPENR_OTGFSLPEN  },
#endif /* RCC_AHB2ENR_OTGFSEN */
#if defined(RCC_AHB3ENR_FSMCEN)
    { RCC_PERIPH_FSMC,       &RCC->AHB3ENR, RCC_AHB3ENR_FSMCEN,   &RCC->AHB3RSTR, RCC_AHB3RSTR_FSMCRST,   &RCC->AHB3LPENR, RCC_AHB3LPENR_FSMCLPEN   },
#endif /* RCC_AHB3ENR_FSMCEN */
    { RCC_PERIPH_TIM5,       &RCC->APB1ENR, RCC_APB1ENR_TIM5EN,   &RCC->APB1RSTR, RCC_APB1RSTR_TIM5RST,   &RCC->APB1LPENR, RCC_APB1LPENR_TIM5LPEN   },
    { RCC_PERIPH_USART2,     &RCC->APB1ENR, RCC_APB1ENR_USART2EN, &RCC->APB1RSTR, RCC_APB1RSTR_USART2RST, &RCC->APB1LPENR, RCC_APB1LPENR_USART2LPEN },
    { RCC_PERIPH_PWR,        &RCC->APB1ENR, RCC_APB1ENR_PWREN,    &RCC->APB1RSTR, RCC_APB1RSTR_PWRRST,    &RCC->APB1LPENR, RCC_APB1LPENR_PWRLPEN    },
    { RCC_PERIPH_TIM1,       &RCC->APB2ENR, RCC_APB2ENR_TIM1EN,   &RCC->APB2RSTR, RCC_APB2RSTR_TIM1RST,   &RCC->APB2LPENR, RCC_APB2LPENR_TIM1LPEN   },
    { RCC_PERIPH_USART1,     &RCC->APB2ENR, RCC_APB2ENR_USART1EN, &RCC->APB2RSTR, RCC_APB2RSTR_USART1RST, &RCC->APB2LPENR, RCC_APB2LPENR_USART1LPEN },
    { RCC_PERIPH_SYSCFG,     &RCC->APB2ENR, RCC_APB2ENR_SYSCFGEN, &RCC->APB2RSTR, RCC_APB2RSTR_SYSCFGRST, &RCC->APB2LPENR, RCC_APB2LPENR_SYSCFGLPEN },
    { RCC_PERIPH_ADC1,       &RCC->APB2ENR, RCC_APB2ENR_ADC1EN,   &RCC->APB2RSTR, RCC_APB2RSTR_ADCRST,    &RCC->APB2LPENR, RCC_APB2LPENR_ADC1LPEN   },
};

/** Clock enable registers of all register banks */
static volatile uint32_t * const utRcc_EnableRegs[] =
{
    &RCC->AHB1ENR,
#if defined(RCC_AHB2_SUPPORT)
    &RCC->AHB2ENR,
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    &RCC->AHB3ENR,
#endif /* RCC_AHB3_SUPPORT */
    &RCC->APB1ENR, &RCC->APB2ENR, &RCC->BDCR
};

/** Reset registers of all register banks */
static volatile uint32_t * const utRcc_ResetRegs[] =
{
    &RCC->AHB1RSTR,
#if defined(RCC_AHB2_SUPPORT)
    &RCC->AHB2RSTR,
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    &RCC->AHB3RSTR,
#endif /* RCC_AHB3_SUPPORT */
    &RCC->APB1RSTR, &RCC->APB2RSTR
};

/** Clock enable in sleep mode registers of all register banks */
static volatile uint32_t * const utRcc_SleepRegs[] =
{
    &RCC->AHB1LPENR,
#if defined(RCC_AHB2_SUPPORT)
    &RCC->AHB2LPENR,
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    &RCC->AHB3LPENR,
#endif /* RCC_AHB3_SUPPORT */
    &RCC->APB1LPENR, &RCC->APB2LPENR
};

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    utRcc_HseFails = false;

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
 * \brief   Rcc_Get_DefaultConfig() fills configuration of 84 MHz from HSI PLL.
 *
 * \details Reads default configuration into structure filled by pattern.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSE not used, system clock from main PLL.
 * - Main PLL: HSI, M 16, N 336, P 4, Q 7, R not used. Other PLLs not used.
 * - AHB / 1, APB1 / 2, APB2 / 1, SysTick 1 ms, CSS off, voltage scale 1.
 * - Clock outputs not used (source NONE, divider 1).
 */
void Ut_Rcc_Get_DefaultConfig_FillsHsiPll84MHz( void )
{
    rcc_ConfigStruct_t config;

    (void)memset( &config, 0xA5, sizeof( config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_HSE_TYPE_NONE,           config.HSE_ClockType );
    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_PLL, config.SystemClockSource );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE,       config.CSS_Enable );
    TEST_ASSERT_EQUAL( RCC_AHB_DIVIDER_1,           config.AHB_Divider );
    TEST_ASSERT_EQUAL( RCC_APB1_DIVIDER_2,          config.APB1_Divider );
    TEST_ASSERT_EQUAL( RCC_APB2_DIVIDER_1,          config.APB2_Divider );
    TEST_ASSERT_EQUAL_UINT32( 1u,                   config.SysTickInterval );
    TEST_ASSERT_EQUAL( RCC_FLASH_LATENCY_0_WS,      config.FlashLatency );
#if defined(RCC_MAX_FREQUENCY_SCALE1)
    TEST_ASSERT_EQUAL( RCC_PWR_VOLTAGE_SCALE_1,     config.VoltageScaling );
#endif /* RCC_MAX_FREQUENCY_SCALE1 */

    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, config.Pll_Config[ RCC_PLL_MAIN ].Pll_Source );
    TEST_ASSERT_EQUAL_UINT32( 16u,      config.Pll_Config[ RCC_PLL_MAIN ].M_Divider );
    TEST_ASSERT_EQUAL_UINT32( 336u,     config.Pll_Config[ RCC_PLL_MAIN ].N_Multiplier );
    TEST_ASSERT_EQUAL_UINT32( 4u,       config.Pll_Config[ RCC_PLL_MAIN ].P_Divider );
    TEST_ASSERT_EQUAL_UINT32( 7u,       config.Pll_Config[ RCC_PLL_MAIN ].Q_Divider );
    TEST_ASSERT_EQUAL_UINT32( 0u,       config.Pll_Config[ RCC_PLL_MAIN ].R_Divider );

    for( uint32_t pllId = (uint32_t)RCC_PLL_MAIN + 1u; (uint32_t)RCC_PLL_CNT > pllId; pllId++ )
    {
        TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE, config.Pll_Config[ pllId ].Pll_Source );
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
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR );
    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Rcc_Init() with default configuration sets 84 MHz from HSI PLL.
 *
 * \details HW model emulates ready flags and system clock switch.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, system clock source (SWS) is PLL.
 * - PLLCFGR: HSI source, M 16, N 336, P 4, Q 7. HSI and main PLL on, HSE and CSS off.
 * - Flash: 2 wait states (84 MHz, scale 1), prefetch and caches enabled.
 * - PWR and SYSCFG interface clocks enabled, voltage scale 1.
 * - AHB / 1, APB1 / 2, APB2 / 1, SysTick reload 84000 - 1, SystemCoreClock 84 MHz.
 */
void Ut_Rcc_Init_DefaultConfig_Configures84MHzFromHsiPll( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV4, 7u, RCC_PLLCFGR_PLLSRC_HSI ), RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_PLLON,
                             RCC->CR & ( RCC_CR_HSION | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_2WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN, FLASH->ACR );

    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR_PWREN,    RCC->APB1ENR );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR );
#if defined(RCC_MAX_FREQUENCY_SCALE1)
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE1, PWR->CR & PWR_CR_VOS );
#endif /* RCC_MAX_FREQUENCY_SCALE1 */

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1,  RCC->CFGR & RCC_CFGR_HPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE1_DIV2, RCC->CFGR & RCC_CFGR_PPRE1 );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE2_DIV1, RCC->CFGR & RCC_CFGR_PPRE2 );

    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_DEFAULT_SYSCLK_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SYSTICK_CTRL_ACTIVE, SysTick->CTRL );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() configures 168 MHz from HSE crystal PLL with CSS.
 *
 * \details HSE crystal 8 MHz, PLL M 8, N 336, P 2, Q 7, AHB / 1, APB1 / 4,
 *          APB2 / 2, CSS enabled. HW model emulates ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is PLL, HSE on (not bypassed), CSS on, PLL on.
 * - PLLCFGR: HSE source, M 8, N 336, P 2, Q 7.
 * - Flash latency 5 wait states (150 - 168 MHz, scale 1).
 * - Clocks: AHB 168 MHz, APB1 42 MHz, APB2 84 MHz, APB1 timers 84 MHz,
 *   APB2 timers 168 MHz, 48 MHz clock of USB OTG FS. SystemCoreClock 168 MHz.
 */
void Ut_Rcc_Init_HseCrystalPll_Configures168MHz( void )
{
#if defined(TEST_RCC_168MHZ_MCU)
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       clkFreq = 0u;

    Ut_Rcc_Get_HsePllConfig( &config );
    config.CSS_Enable = RCC_FUNCTION_ACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON,
                             RCC->CR & ( RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_CSSON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 8u, 336u, TEST_RCC_PLLP_DIV2, 7u, RCC_PLLCFGR_PLLSRC_HSE ), RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_5WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ / 4u, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ / 2u, clkFreq );

    /* Timers of APB with prescaler > 1 are clocked by 2 x PCLK */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM5, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ / 2u, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );

#if defined(RCC_AHB2ENR_OTGFSEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USB_OTG_FS, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_PLL48_CLK_HZ, clkFreq );
#endif /* RCC_AHB2ENR_OTGFSEN */
#else
    TEST_IGNORE_MESSAGE( "MCU maximal system clock below 168 MHz" );
#endif /* TEST_RCC_168MHZ_MCU */
}


/**
 * \brief   Rcc_Init() with HSE external clock signal as system clock.
 *
 * \details HSE bypass 25 MHz used directly as system clock, PLLs not used.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is HSE, HSE on and bypassed, PLL off, CSS off (not requested).
 * - Flash latency 0 wait states, SystemCoreClock and AHB clock 25 MHz.
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
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_0WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_BYPASS_FREQ_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_BYPASS_FREQ_HZ, ahbClk );
}


/**
 * \brief   Rcc_Init() with HSI system clock does not need PLL lock.
 *
 * \details No HW model - HSI ready flag is preset. All PLLs and HSE are not
 *          used, so the module only waits for flags which are already in the
 *          expected state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SW / SWS is HSI, PLL and HSE off.
 * - SysTick reload 16000 - 1, SystemCoreClock 16 MHz, flash latency 0 wait states.
 */
void Ut_Rcc_Init_HsiSysClk_NoPllRequired( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );

    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR & ( RCC_CFGR_SW | RCC_CFGR_SWS ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION, RCC->CR & ( RCC_CR_HSION | RCC_CR_HSEON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, SystemCoreClock );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_0WS, FLASH->ACR & FLASH_ACR_LATENCY );
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
 * - RCC_REQUEST_ERROR, system clock stays HSI (SWS 0), PLL is not activated.
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
 * \brief   Rcc_Init() rejects system clock above limit of voltage scale.
 *
 * \details 168 MHz HSE PLL configuration with voltage scale 2 (max. 144 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock stays HSI (SWS 0), flash latency not changed.
 */
void Ut_Rcc_Init_SysClkOverScaleLimit_ReturnsErrorOnHsi( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsePllConfig( &config );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_HSI, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_0WS, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Init() reconfigures PLL which drives the system clock.
 *
 * \details Default configuration (84 MHz) is initialized first, then main PLL
 *          is changed to N 432, P 4, Q 9 (108 MHz, 48 MHz) by second Rcc_Init().
 *
 * \par Expected results
 * - Both calls return RCC_REQUEST_OK, SWS is PLL.
 * - PLLCFGR holds the new configuration, flash latency 3 wait states,
 *   SystemCoreClock 108 MHz.
 */
void Ut_Rcc_Init_CalledTwice_ReconfiguresRunningPll( void )
{
#if defined(TEST_RCC_168MHZ_MCU)
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    config.Pll_Config[ RCC_PLL_MAIN ].N_Multiplier = 432u;
    config.Pll_Config[ RCC_PLL_MAIN ].Q_Divider    = 9u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 16u, 432u, TEST_RCC_PLLP_DIV4, 9u, RCC_PLLCFGR_PLLSRC_HSI ), RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_3WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( 108000000u, SystemCoreClock );
#else
    TEST_IGNORE_MESSAGE( "MCU maximal system clock below 168 MHz" );
#endif /* TEST_RCC_168MHZ_MCU */
}


/**
 * \brief   Rcc_Init() configures clock output and its pin.
 *
 * \details HSI system clock configuration, MCO1 source HSI divided by 4.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Gpio_Init() called once for PA8 (alternate function 0,
 *   very high speed, push-pull, no pull).
 * - MCO1 field selects HSI, MCO1PRE = divider 4, MCO2 not changed.
 */
void Ut_Rcc_Init_ClockOutput_ConfiguresMcoAndPin( void )
{
    rcc_ConfigStruct_t config;
    gpio_Config_t      expectedPin =
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

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLK_SOURCE_MCO1_HSI;
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 4u;

    RCC->CR = RCC_CR_HSIRDY;

    Gpio_Init_ExpectAndReturn( &expectedPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR & ( RCC_CFGR_MCO1 | RCC_CFGR_MCO2 | RCC_CFGR_MCO2PRE ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_MCOPRE_DIV4 << RCC_CFGR_MCO1PRE_Pos, RCC->CFGR & RCC_CFGR_MCO1PRE );
}

/* ========================= PERIPHERAL CLOCKS ============================== */

/**
 * \brief   Rcc_Set_PeriphActive() sets enable bit of the peripheral.
 *
 * \details Activates and deactivates peripherals of every register bank (AHB1,
 *          AHB2, AHB3, APB1, APB2) one by one and reads their state.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, only own enable bit is set in all enable
 *   registers, state is ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, enable register is 0, state is INACTIVE.
 */
void Ut_Rcc_Set_PeriphActive_EveryRegBank_SetsOwnEnableBit( void )
{
    const utRcc_PeriphRegs_t * table    = NULL;
    uint32_t                   tableCnt = 0u;
    rcc_FunctionState_t        state    = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Get_PeriphRegsTable( &table, &tableCnt );

    for( uint32_t idx = 0u; tableCnt > idx; idx++ )
    {
        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( table[ idx ].PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( table[ idx ].EnableMask, *table[ idx ].EnableReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_EnableRegs, TEST_RCC_ARRAY_CNT( utRcc_EnableRegs ), table[ idx ].EnableReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( table[ idx ].PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( table[ idx ].PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *table[ idx ].EnableReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( table[ idx ].PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Rcc_Set_PeriphInactive() clears only own enable bit.
 *
 * \details GPIOA and GPIOB clocks are preset, GPIOA is deactivated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, AHB1ENR holds GPIOB enable bit only.
 */
void Ut_Rcc_Set_PeriphInactive_KeepsOtherEnableBits( void )
{
    RCC->AHB1ENR = RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1ENR_GPIOBEN, RCC->AHB1ENR );
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
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR_PWREN, RCC->APB1ENR & RCC_APB1ENR_PWREN );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR_DBP, PWR->CR & PWR_CR_DBP );
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
 * \brief   RTC activation with divided HSE configures RTC prescaler.
 *
 * \details HSE frequency 25 MHz, RTC prescaler (RTCPRE) is not configured.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCPRE = 25 (RTC clock 1 MHz), RTCSEL = HSE, RTCEN set.
 * - Rcc_Get_PeriphClk() of the RTC returns 1 MHz.
 */
void Ut_Rcc_Set_PeriphActive_RtcHseDiv_ConfiguresRtcPrescaler( void )
{
    rcc_FreqHz_t rtcClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_BYPASS_FREQ_HZ ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_HSE_DIV ) );

    TEST_ASSERT_EQUAL_HEX32( 25u << RCC_CFGR_RTCPRE_Pos, RCC->CFGR & RCC_CFGR_RTCPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV, &rtcClk ) );
    TEST_ASSERT_EQUAL_UINT32( 1000000u, rtcClk );
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
 * - GPIOA (AHB1) 8 MHz, USART2 (APB1) 4 MHz, USART1 (APB2) 2 MHz.
 * - TIM5 (APB1 timer) 8 MHz, TIM1 (APB2 timer) 4 MHz (2 x PCLK).
 */
void Ut_Rcc_Get_PeriphClk_BusClockedPeriphs_ReturnBusFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CFGR = RCC_CFGR_SWS_HSI | RCC_CFGR_HPRE_DIV2 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV4;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_GPIOA, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 8000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 4000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 2000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM5, &freq ) );
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
 * - TIM5 and TIM1 clock 16 MHz.
 */
void Ut_Rcc_Get_PeriphClk_ApbNotDivided_TimerClockEqualsPclk( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM5, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
}


/**
 * \brief   48 MHz clock peripherals are clocked by main PLL output Q.
 *
 * \details PLLCFGR preset: HSI, M 16, N 336, Q 7.
 *
 * \par Expected results
 * - USB OTG FS, RNG and SDIO (where available) clock 48 MHz.
 */
void Ut_Rcc_Get_PeriphClk_Pll48ClkPeriphs_ReturnPllQ( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV4, 7u, RCC_PLLCFGR_PLLSRC_HSI );

#if defined(RCC_AHB2ENR_OTGFSEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USB_OTG_FS, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_PLL48_CLK_HZ, freq );
#endif /* RCC_AHB2ENR_OTGFSEN */
#if defined(RCC_AHB1ENR_RNGEN) || defined(RCC_AHB2ENR_RNGEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_PLL48_CLK_HZ, freq );
#endif /* RCC_AHB1ENR_RNGEN OR RCC_AHB2ENR_RNGEN */
#if defined(RCC_APB2ENR_SDIOEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SDIO, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_PLL48_CLK_HZ, freq );
#endif /* RCC_APB2ENR_SDIOEN */
}


/**
 * \brief   Oscillator clocked peripherals return oscillator frequency.
 *
 * \par Expected results
 * - IWDG and RTC with LSI clock 32 kHz, RTC with LSE clock 32.768 kHz.
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
}


/**
 * \brief   Rcc_Get_PeriphClk() rejects peripheral without handled kernel clock.
 *
 * \details Ethernet TX is clocked by external PHY clock (where available),
 *          RTC with HSE has no clock while RTCPRE is 0.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, frequency 0.
 */
void Ut_Rcc_Get_PeriphClk_NoKernelClock_ReturnsErrorAndZero( void )
{
    rcc_FreqHz_t freq = 1u;

#if defined(RCC_AHB1ENR_ETHMACEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_ETH_TX, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
    freq = 1u;
#endif /* RCC_AHB1ENR_ETHMACEN */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
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

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1, srcId );
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
    const utRcc_PeriphRegs_t * table    = NULL;
    uint32_t                   tableCnt = 0u;
    rcc_FunctionState_t        state    = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Get_PeriphRegsTable( &table, &tableCnt );

    for( uint32_t idx = 0u; tableCnt > idx; idx++ )
    {
        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( table[ idx ].PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( table[ idx ].ResetMask, *table[ idx ].ResetReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs, TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ), table[ idx ].ResetReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( table[ idx ].PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( table[ idx ].PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *table[ idx ].ResetReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( table[ idx ].PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Reset of peripheral without reset control is rejected.
 *
 * \details Flash interface (no reset bit) and RTC (backup domain, no reset
 *          register) are reset.
 *
 * \par Expected results
 * - Reset active: RCC_REQUEST_ERROR, no reset register written.
 * - Reset inactive: RCC_REQUEST_OK (never in reset), reset state INACTIVE.
 */
void Ut_Rcc_Set_ResetActive_NoResetControl_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_RTC_LSE ) );
    Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs, TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ), NULL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_FLASH ) );
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
    const utRcc_PeriphRegs_t * table    = NULL;
    uint32_t                   tableCnt = 0u;
    rcc_FunctionState_t        state    = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Get_PeriphRegsTable( &table, &tableCnt );

    for( uint32_t idx = 0u; tableCnt > idx; idx++ )
    {
        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( table[ idx ].PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( table[ idx ].SleepMask, *table[ idx ].SleepReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_SleepRegs, TEST_RCC_ARRAY_CNT( utRcc_SleepRegs ), table[ idx ].SleepReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( table[ idx ].PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( table[ idx ].PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *table[ idx ].SleepReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( table[ idx ].PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Sleep-only blocks are controlled in sleep register.
 *
 * \details Flash interface has sleep mode clock control only (FLITFLPEN).
 *
 * \par Expected results
 * - Sleep inactive clears FLITFLPEN, sleep active sets it, other bits kept.
 */
void Ut_Rcc_Set_SleepInactive_FlashInterface_ClearsFlitfBit( void )
{
    RCC->AHB1LPENR = RCC_AHB1LPENR_FLITFLPEN | RCC_AHB1LPENR_GPIOALPEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1LPENR_GPIOALPEN, RCC->AHB1LPENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_FLASH ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1LPENR_FLITFLPEN | RCC_AHB1LPENR_GPIOALPEN, RCC->AHB1LPENR );
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
 * \details Sets AHB2 / 8 (shared AHB divider), APB1 / 4 and APB2 / 16.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE = /8, PPRE1 = /4, PPRE2 = /16.
 * - Rcc_Get_ClkBusDivider() returns the set dividers, AHB1 equals AHB2.
 */
void Ut_Rcc_Set_ClkBusDivider_AllBuses_WritesCfgr( void )
{
    rcc_ClkBusDiv_t divider = 0u;

#if defined(RCC_AHB2_SUPPORT)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB2, RCC_AHB_DIVIDER_8 ) );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_8 ) );
#endif /* RCC_AHB2_SUPPORT */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB2_DIVIDER_16 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV8 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV16, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB_DIVIDER_8, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1, &divider ) );
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

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1, RCC_APB2_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT,  RCC_AHB_DIVIDER_2  ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_CNT,  &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1, NULL     ) );
}


/**
 * \brief   Rcc_Set_ClkBusDivider() does not slow down a bus clock while a DMA stream is
 *          enabled (STM32F405 / 407 / 415 / 417).
 *
 * \details DMA1 stream 6 enabled, APB1 / 2 preset. Then DMA2 stream 0 enabled instead.
 *
 * \note    Device errata bug AB#655 (ES0182 2.2.11 "Slowing down APB clock during a DMA
 *          transfer"): when the AHB / APB prescaler slows down the APB clock while a DMA writes
 *          to a peripheral of the bus, the DMA transfer is blocked and only a system reset
 *          recovers. Workaround - wait for the end of the DMA transfer, so slowing down is
 *          refused while a DMA stream is enabled. Other devices: test ignored.
 *
 * \par Expected results
 * - DMA1 stream 6 enabled: APB1 / 4, APB2 / 2 and AHB / 2 refused (CFGR not changed),
 *   APB1 / 1 (faster) and unchanged APB1 / 2 accepted.
 * - DMA2 stream 0 enabled: APB2 / 2 refused.
 * - All streams disabled: APB1 / 4 accepted.
 */
void Ut_Rcc_Set_ClkBusDivider_DmaRunning_SlowDownRefused( void )
{
#if defined(STM32F405xx) || defined(STM32F407xx) || defined(STM32F415xx) || defined(STM32F417xx)
    RCC->CFGR          = RCC_CFGR_PPRE1_DIV2;
    DMA1_Stream6->CR   = DMA_SxCR_EN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB2_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1, RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE1_DIV2, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1, RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1, RCC_APB1_DIVIDER_1 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE1_DIV1, RCC->CFGR );

    DMA1_Stream6->CR = 0u;
    DMA2_Stream0->CR = DMA_SxCR_EN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, RCC_APB2_DIVIDER_2 ) );

    DMA2_Stream0->CR = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE1_DIV4, RCC->CFGR );
#else
    TEST_IGNORE_MESSAGE( "Device errata of STM32F405 / 407 / 415 / 417 only" );
#endif /* STM32F405xx || STM32F407xx || STM32F415xx || STM32F417xx */
}


/**
 * \brief   Rcc_Get_ClkBusClk() calculates bus clocks from PLL system clock.
 *
 * \details Registers preset: main PLL HSI / 16 * 336 / 4 (84 MHz) is system
 *          clock, AHB / 1, APB1 / 2, APB2 / 1.
 *
 * \par Expected results
 * - AHB1 84 MHz, APB1 42 MHz, APB2 84 MHz.
 * - Invalid bus or NULL pointer returns RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkBusClk_PllSysClk_ReturnsBusFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV4, 7u, RCC_PLLCFGR_PLLSRC_HSI );
    RCC->CFGR    = RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_CNT,  &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1, NULL  ) );
}

/* ========================= POWER RANGE AND FLASH ========================== */

/**
 * \brief   Rcc_Set_PwrRange() writes voltage scale while main PLL is off.
 *
 * \details VOS preset to scale 1, scale 2 is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOS = scale 2, PWR interface clock enabled.
 */
void Ut_Rcc_Set_PwrRange_PllInactive_WritesVos( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    PWR->CR = LL_PWR_REGU_VOLTAGE_SCALE1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE2, PWR->CR & PWR_CR_VOS );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR_PWREN, RCC->APB1ENR );
}


/**
 * \brief   Rcc_Set_PwrRange() rejects change of scale while main PLL is on.
 *
 * \details Main PLL is on and locked, VOS scale 2, scale 1 (scale 3 on MCUs without scale 1) is
 *          requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, VOS not changed.
 * - Request of already configured scale returns RCC_REQUEST_OK.
 */
void Ut_Rcc_Set_PwrRange_PllActive_ChangeRejected( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->CR = RCC_CR_PLLON | RCC_CR_PLLRDY;
    PWR->CR = LL_PWR_REGU_VOLTAGE_SCALE2;

#if defined(RCC_MAX_FREQUENCY_SCALE1)
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;
#else
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_3;
#endif /* RCC_MAX_FREQUENCY_SCALE1 */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE2, PWR->CR & PWR_CR_VOS );

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() calculates wait states from HCLK (scale 1).
 *
 * \details HSE system clock with frequencies around thresholds of voltage
 *          scale 1 (30, 60, 90, 120, 150 MHz), AHB / 1.
 *
 * \par Expected results
 * - Every exceeded threshold adds one wait state.
 */
void Ut_Rcc_Set_FlashLatency_Scale1Thresholds_SetsWaitStates( void )
{
#if defined(TEST_RCC_168MHZ_MCU)
    const struct
    {
        rcc_FreqHz_t SysClk;
        uint32_t     Latency;
    }   latencyLut[] =
    {
        {  16000000u, FLASH_ACR_LATENCY_0WS },
        {  30000000u, FLASH_ACR_LATENCY_0WS },
        {  30000001u, FLASH_ACR_LATENCY_1WS },
        {  60000001u, FLASH_ACR_LATENCY_2WS },
        {  90000001u, FLASH_ACR_LATENCY_3WS },
        { 120000001u, FLASH_ACR_LATENCY_4WS },
        { 150000000u, FLASH_ACR_LATENCY_4WS },
        { 150000001u, FLASH_ACR_LATENCY_5WS },
        { 168000000u, FLASH_ACR_LATENCY_5WS },
    };
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( latencyLut ) > idx; idx++ )
    {
        config.HSE_Frequency_Hz = latencyLut[ idx ].SysClk;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
        TEST_ASSERT_EQUAL_HEX32( latencyLut[ idx ].Latency, FLASH->ACR & FLASH_ACR_LATENCY );
    }
#else
    TEST_IGNORE_MESSAGE( "MCU maximal system clock below 168 MHz" );
#endif /* TEST_RCC_168MHZ_MCU */
}


/**
 * \brief   Rcc_Set_FlashLatency() uses thresholds of voltage scale 2.
 *
 * \details HSE system clock of maximum of scale 2 (144 MHz STM32F407) and maximum + 1 Hz.
 *
 * \par Expected results
 * - Maximum: RCC_REQUEST_OK, wait states of the maximum (4 on STM32F407).
 * - Maximum + 1 Hz: RCC_REQUEST_ERROR, latency not changed.
 */
void Ut_Rcc_Set_FlashLatency_Scale2_LimitChecked( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_2;

    config.HSE_Frequency_Hz = RCC_MAX_FREQUENCY_SCALE2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SCALE2_MAX_LATENCY, FLASH->ACR & FLASH_ACR_LATENCY );

    config.HSE_Frequency_Hz = RCC_MAX_FREQUENCY_SCALE2 + 1u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SCALE2_MAX_LATENCY, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Set_FlashLatency() uses HCLK (AHB divider) and user minimum.
 *
 * \details 168 MHz system clock with AHB / 2 (HCLK 84 MHz), then HSI 16 MHz
 *          with minimal latency 7 wait states.
 *
 * \par Expected results
 * - AHB / 2: 2 wait states.
 * - User minimum higher than calculated: 7 wait states.
 */
void Ut_Rcc_Set_FlashLatency_AhbDividerAndUserMinimum_Applied( void )
{
#if defined(TEST_RCC_168MHZ_MCU)
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = TEST_RCC_HSE_PLL_SYSCLK_HZ;
    config.AHB_Divider       = RCC_AHB_DIVIDER_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_2WS, FLASH->ACR & FLASH_ACR_LATENCY );

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.FlashLatency = RCC_FLASH_LATENCY_7_WS;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_7WS, FLASH->ACR & FLASH_ACR_LATENCY );
#else
    TEST_IGNORE_MESSAGE( "MCU maximal system clock below 168 MHz" );
#endif /* TEST_RCC_168MHZ_MCU */
}


/**
 * \brief   Rcc_Set_FlashLatency() rejects configuration without valid clock.
 *
 * \details System clock from PLL which is not used (source NONE), NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, FLASH_ACR not written.
 */
void Ut_Rcc_Set_FlashLatency_PllNotConfigured_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Flash prefetch buffer activation and deactivation.
 *
 * \details Instruction cache bit is preset, prefetch is activated and deactivated.
 *
 * \par Expected results
 * - Both calls RCC_REQUEST_OK, PRFTEN follows the request, ICEN kept.
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
 * \brief   Rcc_Set_SysTickInterval() configures SysTick from HCLK.
 *
 * \details HCLK is HSI 16 MHz (registers after reset), interval 1 ms.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, LOAD = 16000 - 1, SysTick enabled with interrupt and HCLK source.
 * - Rcc_Get_SysTickInterval() returns 1 ms.
 */
void Ut_Rcc_Set_SysTickInterval_1ms_WritesReload( void )
{
    rcc_Time_ms_t interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SYSTICK_CTRL_ACTIVE, SysTick->CTRL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Set_SysTickInterval() checks range of 24-bit reload register.
 *
 * \details HCLK 16 MHz - longest interval is 1048 ms. Intervals 0, 1048 and
 *          1049 ms are requested.
 *
 * \par Expected results
 * - 0 ms: RCC_REQUEST_ERROR, SysTick not configured.
 * - 1048 ms: RCC_REQUEST_OK, interval read back.
 * - 1049 ms: RCC_REQUEST_ERROR, previous reload kept.
 */
void Ut_Rcc_Set_SysTickInterval_RangeBoundaries_Checked( void )
{
    rcc_Time_ms_t interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->CTRL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( TEST_RCC_SYSTICK_MAX_MS_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_SYSTICK_MAX_MS_HSI, interval );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( TEST_RCC_SYSTICK_MAX_MS_HSI + 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_SYSTICK_MAX_MS_HSI, interval );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( NULL ) );
}


/**
 * \brief   Rcc_Get_SysTickInterval() calculates interval from reload and HCLK.
 *
 * \details LOAD preset to 160000 - 1, HCLK 16 MHz.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, interval 10 ms.
 */
void Ut_Rcc_Get_SysTickInterval_ReadsReload( void )
{
    rcc_Time_ms_t interval = 0u;

    SysTick->LOAD = 160000u - 1u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 10u, interval );
}

/* ================================== PLL =================================== */

/**
 * \brief   Rcc_Set_PllConfig() configures main PLL and waits for lock.
 *
 * \details HSI source, M 16, N 336, P 4, Q 7. HW model emulates ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLCFGR holds the configuration, HSI and PLL on.
 * - PLL state ACTIVE, internal clock 336 MHz, P 84 MHz, Q 48 MHz.
 */
void Ut_Rcc_Set_PllConfig_MainHsi_ConfiguresAndLocks( void )
{
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 16u, .N_Multiplier = 336u,
        .P_Divider  = 4u,              .Q_Divider = 7u,  .R_Divider    = 0u
    };
    rcc_FunctionState_t   state = RCC_FUNCTION_INACTIVE;
    rcc_FreqHz_t          freq  = 0u;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_MAIN, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV4, 7u, RCC_PLLCFGR_PLLSRC_HSI ), RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_PLLON, RCC->CR & ( RCC_CR_HSION | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_MAIN, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 336000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_PLL48_CLK_HZ, freq );
}


/**
 * \brief   Rcc_Set_PllConfig() with source NONE only deactivates the PLL.
 *
 * \details Main PLL on (ready flag already 0), configuration with source NONE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLON cleared, PLLCFGR not written.
 */
void Ut_Rcc_Set_PllConfig_SourceNone_DeactivatesOnly( void )
{
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_NONE, .M_Divider = 16u, .N_Multiplier = 336u,
        .P_Divider  = 4u,               .Q_Divider = 7u,  .R_Divider    = 0u
    };

    RCC->CR = RCC_CR_PLLON;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_MAIN, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects M and N out of range.
 *
 * \details M 1 and 64, N below and above limits of the MCU.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, PLLCFGR not written, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_MnOutOfRange_ReturnsError( void )
{
    const struct
    {
        rcc_PllMDivider_t M;
        rcc_PllNMult_t    N;
    }   invalidLut[] =
    {
        { 1u,  336u                   },
        { 64u, 336u                   },
        { 16u, RCC_PLLN_MIN_VALUE - 1u },
        { 16u, RCC_PLLN_MAX_VALUE + 1u },
    };
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 16u, .N_Multiplier = 336u,
        .P_Divider  = 4u,              .Q_Divider = 7u,  .R_Divider    = 0u
    };

    RCC->CR = RCC_CR_HSIRDY;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        pllConfig.M_Divider    = invalidLut[ idx ].M;
        pllConfig.N_Multiplier = invalidLut[ idx ].N;

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_MAIN, &pllConfig ) );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_CNT,  &pllConfig ) );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects PLL input and VCO frequency out of range.
 *
 * \details HSI 16 MHz: M 4 (input 4 MHz > 2.1 MHz), M 16 with N 50 (VCO 50 MHz
 *          below minimum).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL dividers not written, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_FrequencyOutOfRange_ReturnsError( void )
{
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 4u, .N_Multiplier = 100u,
        .P_Divider  = 4u,              .Q_Divider = 7u, .R_Divider    = 0u
    };

    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_MAIN, &pllConfig ) );

    pllConfig.M_Divider    = 16u;
    pllConfig.N_Multiplier = 50u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_MAIN, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects odd P divider.
 *
 * \details P divider 3 (only 2, 4, 6, 8 are valid).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_InvalidP_ReturnsErrorWithoutActivation( void )
{
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 16u, .N_Multiplier = 336u,
        .P_Divider  = 3u,              .Q_Divider = 7u,  .R_Divider    = 0u
    };

    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_MAIN, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
}


/**
 * \brief   PLLI2S configuration can not change M divider shared with active main PLL.
 *
 * \details Main PLL is on with M 16, PLLI2S requests M 8.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, main PLL M divider not changed, PLLI2S not activated.
 */
void Ut_Rcc_Set_PllConfig_I2sSharedMInUse_ReturnsError( void )
{
#if defined(RCC_CR_PLLI2SON) && !defined(RCC_PLLI2SCFGR_PLLI2SM)
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 8u, .N_Multiplier = 192u,
        .P_Divider  = 0u,              .Q_Divider = 0u, .R_Divider    = 2u
    };

    RCC->CR      = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV4, 7u, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_I2S, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 16u << RCC_PLLCFGR_PLLM_Pos, RCC->PLLCFGR & RCC_PLLCFGR_PLLM );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLI2SON );
#else
    TEST_IGNORE_MESSAGE( "MCU without PLLI2S or with own PLLI2S M divider" );
#endif /* RCC_CR_PLLI2SON AND NOT RCC_PLLI2SCFGR_PLLI2SM */
}


/**
 * \brief   PLLI2S with M divider equal to active main PLL is configured.
 *
 * \details Main PLL on with M 16, PLLI2S HSI, M 16, N 192, R 2. HW model
 *          emulates ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLI2SCFGR N 192, R 2, PLLI2S on.
 * - PLLI2S output R 96 MHz.
 */
void Ut_Rcc_Set_PllConfig_I2sSameSharedM_Configured( void )
{
#if defined(RCC_CR_PLLI2SON) && !defined(RCC_PLLI2SCFGR_PLLI2SM)
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 16u, .N_Multiplier = 192u,
        .P_Divider  = 0u,              .Q_Divider = 0u,  .R_Divider    = 2u
    };
    rcc_FreqHz_t          freq = 0u;

    RCC->CR      = RCC_CR_HSION | RCC_CR_PLLON;
    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV4, 7u, RCC_PLLCFGR_PLLSRC_HSI );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_I2S, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( ( 192u << RCC_PLLI2SCFGR_PLLI2SN_Pos ) | ( 2u << RCC_PLLI2SCFGR_PLLI2SR_Pos ), RCC->PLLI2SCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_PLLI2SON | RCC_CR_PLLON, RCC->CR & ( RCC_CR_PLLI2SON | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_I2S, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 96000000u, freq );
#else
    TEST_IGNORE_MESSAGE( "MCU without PLLI2S or with own PLLI2S M divider" );
#endif /* RCC_CR_PLLI2SON AND NOT RCC_PLLI2SCFGR_PLLI2SM */
}


/**
 * \brief   PLLI2S configuration rejects output Q not available on the MCU.
 *
 * \details PLLI2S HSI, M 16, N 192, Q 4 (PLLI2S has outputs N and R only).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLLI2S not activated.
 */
void Ut_Rcc_Set_PllConfig_I2sOutputQNotAvailable_ReturnsError( void )
{
#if defined(RCC_CR_PLLI2SON) && !defined(RCC_PLLI2SCFGR_PLLI2SQ)
    rcc_PllConfigStruct_t pllConfig =
    {
        .Pll_Source = RCC_PLL_SRC_HSI, .M_Divider = 16u, .N_Multiplier = 192u,
        .P_Divider  = 0u,              .Q_Divider = 4u,  .R_Divider    = 0u
    };

    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_I2S, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLI2SON );
#else
    TEST_IGNORE_MESSAGE( "MCU without PLLI2S or with PLLI2S output Q" );
#endif /* RCC_CR_PLLI2SON AND NOT RCC_PLLI2SCFGR_PLLI2SQ */
}


/**
 * \brief   PLL output frequencies are calculated from registers.
 *
 * \details PLLCFGR preset: HSI, M 16, N 336, P 2, Q 7.
 *
 * \par Expected results
 * - Internal (VCO) 336 MHz, P 168 MHz, Q 48 MHz.
 * - Output R: RCC_REQUEST_ERROR on MCUs without main PLL output R.
 */
void Ut_Rcc_Get_PllClk_MainOutputs_CalculatedFromRegisters( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV2, 7u, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 336000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 168000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_PLL48_CLK_HZ, freq );

#if !defined(RCC_PLLCFGR_PLLR)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutR( RCC_PLL_MAIN, &freq ) );
#endif /* RCC_PLLCFGR_PLLR */
}


/**
 * \brief   PLL output with divider field 0 has no valid frequency.
 *
 * \details PLLCFGR preset: HSI, M 16, N 336, Q 0.
 *
 * \par Expected results
 * - Output Q: RCC_REQUEST_ERROR, frequency 0.
 * - Invalid PLL ID and NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_PllClk_ZeroDividerOrInvalidArgs_ReturnsError( void )
{
    rcc_FreqHz_t freq = 1u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 16u, 336u, TEST_RCC_PLLP_DIV2, 0u, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutQ( RCC_PLL_MAIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP   ( RCC_PLL_CNT,  &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutQ   ( RCC_PLL_CNT,  &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutR   ( RCC_PLL_CNT,  &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllInternalClk( RCC_PLL_CNT,  &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP   ( RCC_PLL_MAIN, NULL  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllInternalClk( RCC_PLL_MAIN, NULL  ) );
}


/**
 * \brief   PLL activation, deactivation and state reading.
 *
 * \details Ready flag preset / not preset.
 *
 * \par Expected results
 * - Activation with ready flag: RCC_REQUEST_OK, PLLON set, state ACTIVE.
 * - Deactivation while still locked (eg. used as system clock): RCC_REQUEST_ERROR.
 * - State with PLLON but without ready flag: INACTIVE.
 */
void Ut_Rcc_Set_PllActive_ReadyFlag_StateFollows( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    RCC->CR = RCC_CR_PLLRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_MAIN ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_PLLON, RCC->CR & RCC_CR_PLLON );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_MAIN, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    /* Ready flag stays set - HW keeps PLL running */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_MAIN ) );

    RCC->CR = RCC_CR_PLLON;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_MAIN, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   PLL activation fails if PLL does not lock.
 *
 * \details Ready flag stays 0. Invalid PLL ID and NULL pointer are used.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 */
void Ut_Rcc_Set_PllActive_NotLocked_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllActive  ( RCC_PLL_MAIN ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllActive  ( RCC_PLL_CNT  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_CNT  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllState   ( RCC_PLL_CNT,  &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllState   ( RCC_PLL_MAIN, NULL   ) );
}


/**
 * \brief   PLL source selection writes common PLLSRC bit.
 *
 * \details No PLL is active, HSE is selected and read back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLSRC = HSE, Rcc_Get_PllsSource() returns HSE.
 * - Source NONE is rejected.
 */
void Ut_Rcc_Set_PllsSource_NoPllActive_WritesPllsrc( void )
{
    rcc_PllClkSrc_t source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_MAIN, RCC_PLL_SRC_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSE, RCC->PLLCFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_MAIN, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSE, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_MAIN, RCC_PLL_SRC_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllsSource( RCC_PLL_MAIN, NULL ) );
}


/**
 * \brief   Common PLL source can not be changed while other PLL is active.
 *
 * \details PLLI2S is on with HSI source, HSE is requested for main PLL.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLLSRC stays HSI.
 */
void Ut_Rcc_Set_PllsSource_OtherPllActive_ReturnsError( void )
{
#if defined(RCC_CR_PLLI2SON)
    RCC->CR = RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_MAIN, RCC_PLL_SRC_HSE ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSI, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );
#else
    TEST_IGNORE_MESSAGE( "MCU without PLLI2S" );
#endif /* RCC_CR_PLLI2SON */
}

/* ============================== OSCILLATORS =============================== */

/**
 * \brief   Oscillator activation sets enable bit and waits for ready flag.
 *
 * \details Ready flags of HSI, LSI and LSE are preset.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSION, LSION and LSEON set, all states ACTIVE.
 * - LSE (backup domain): write protection released (PWREN, DBP).
 */
void Ut_Rcc_Set_OscActive_ReadyFlags_SetsEnableBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CSR  = RCC_CSR_LSIRDY;
    RCC->BDCR = RCC_BDCR_LSERDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( RCC_OSC_LSE ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION,   RCC->CR   & RCC_CR_HSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_CSR_LSION,  RCC->CSR  & RCC_CSR_LSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSEON, RCC->BDCR & RCC_BDCR_LSEON );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR_DBP,     PWR->CR   & PWR_CR_DBP );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR_PWREN, RCC->APB1ENR & RCC_APB1ENR_PWREN );

    for( uint32_t oscId = 0u; (uint32_t)RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( (rcc_OscId_t)oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    }
}


/**
 * \brief   Oscillator activation fails if oscillator does not get ready.
 *
 * \details Ready flags stay 0 (eg. LSE crystal not fitted).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for HSI, LSI and LSE, states INACTIVE.
 */
void Ut_Rcc_Set_OscActive_NotReady_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    for( uint32_t oscId = 0u; (uint32_t)RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( (rcc_OscId_t)oscId ) );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( (rcc_OscId_t)oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Oscillator deactivation clears enable bit and waits for ready flag.
 *
 * \details HSI on: ready flag cleared (deactivation possible), then ready flag
 *          kept (HSI used as system clock - HW keeps it running).
 *
 * \par Expected results
 * - Ready flag 0: RCC_REQUEST_OK, HSION cleared.
 * - Ready flag kept: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_OscInactive_ReadyFlag_ResultFollows( void )
{
    RCC->CR = RCC_CR_HSION;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );

    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_HSI ) );
}


/**
 * \brief   Oscillator state requires both enable bit and ready flag.
 *
 * \par Expected results
 * - HSION only: INACTIVE, HSIRDY only: INACTIVE, both: ACTIVE.
 */
void Ut_Rcc_Get_OscState_RequiresOnAndReady( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR = RCC_CR_HSION;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( RCC_OSC_HSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Oscillator functions reject invalid arguments.
 *
 * \par Expected results
 * - Invalid oscillator ID and NULL pointers return RCC_REQUEST_ERROR.
 */
void Ut_Rcc_OscFunctions_InvalidArgs_ReturnError( void )
{
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_OscDiv_t        oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive  ( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState   ( RCC_OSC_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState   ( RCC_OSC_HSI, NULL   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv     ( RCC_OSC_CNT, &oscDiv ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv     ( RCC_OSC_HSI, NULL    ) );
}


/**
 * \brief   Oscillators have no output divider - only divider 1 is accepted.
 *
 * \par Expected results
 * - Divider 1: RCC_REQUEST_OK, divider 2 or invalid oscillator: RCC_REQUEST_ERROR.
 * - Rcc_Get_OscDiv() returns 1 for every oscillator.
 */
void Ut_Rcc_Set_OscDiv_OnlyDividerOneAccepted( void )
{
    rcc_OscDiv_t oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_OscDiv( RCC_OSC_HSI, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_HSI, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_CNT, 1u ) );

    for( uint32_t oscId = 0u; (uint32_t)RCC_OSC_CNT > oscId; oscId++ )
    {
        oscDiv = 0u;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( (rcc_OscId_t)oscId, &oscDiv ) );
        TEST_ASSERT_EQUAL_UINT32( 1u, oscDiv );
    }
}

/* ============================== RTC CLOCK ================================= */

/**
 * \brief   Rcc_Set_RtcClkSource() selects RTC clock in backup domain.
 *
 * \details RTCSEL is 0 (no clock), LSE is selected. Backup domain is write
 *          protected.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCSEL = LSE, write protection released (DBP).
 * - Rcc_Get_RtcClkSource() returns LSE, repeated selection of LSE is accepted.
 */
void Ut_Rcc_Set_RtcClkSource_Lse_WritesRtcsel( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_0, RCC->BDCR & RCC_BDCR_RTCSEL );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR_DBP, PWR->CR & PWR_CR_DBP );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_LSE, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
}


/**
 * \brief   RTC clock can be selected only once (write-once RTCSEL).
 *
 * \details RTCSEL preset to LSI, LSE is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RTCSEL stays LSI. Invalid source is rejected.
 */
void Ut_Rcc_Set_RtcClkSource_OtherSelected_ReturnsError( void )
{
    RCC->BDCR = RCC_BDCR_RTCSEL_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
}


/**
 * \brief   Selection of divided HSE configures RTC prescaler first.
 *
 * \details HSE frequency 8 MHz, RTCPRE not configured.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCPRE = 8 (RTC clock 1 MHz), RTCSEL = HSE.
 */
void Ut_Rcc_Set_RtcClkSource_HseDiv_ConfiguresPrescaler( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_FREQ_HZ ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_HSE_DIV ) );

    TEST_ASSERT_EQUAL_HEX32( 8u << RCC_CFGR_RTCPRE_Pos, RCC->CFGR & RCC_CFGR_RTCPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   Rcc_Get_RtcClkSource() fails if RTC has no clock.
 *
 * \par Expected results
 * - RTCSEL 0 or NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_RtcClkSource_NoClock_ReturnsError( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( NULL ) );
}

/* ============================ CLOCK OUTPUTS =============================== */

/**
 * \brief   Rcc_Set_ClkOutSource() selects MCO1 source and configures PA8.
 *
 * \details MCO1 source main PLL.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MCO1 field = PLL, Gpio_Init() of PA8 (AF0, very high speed).
 * - Rcc_Get_ClkOutSource() returns MCO1 PLL.
 */
void Ut_Rcc_Set_ClkOutSource_Mco1Pll_ConfiguresPa8( void )
{
    rcc_ClkOut_Source_t source      = RCC_CLK_SOURCE_NONE;
    gpio_Config_t       expectedPin =
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

    Gpio_Init_ExpectAndReturn( &expectedPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_PLL ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_MCO1, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO1_PLL, source );
}


/**
 * \brief   Rcc_Set_ClkOutSource() selects MCO2 source and configures PC9.
 *
 * \details MCO2 field preset to PLL, SYSCLK is selected.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MCO2 field = SYSCLK (0), MCO1 not changed, Gpio_Init() of PC9.
 */
void Ut_Rcc_Set_ClkOutSource_Mco2Sysclk_ConfiguresPc9( void )
{
    gpio_Config_t expectedPin =
    {
        .PortId         = GPIO_PORT_C,
        .PinId          = GPIO_PIN_ID_9,
        .PinMode        = GPIO_PIN_MODE_ALTERNATE,
        .PinPull        = GPIO_PIN_PULL_NONE,
        .PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH,
        .PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL,
        .PinAltFunction = GPIO_ALT_FUNC_0,
        .PinActiveLevel = GPIO_PIN_LEVEL_HIGH
    };

    RCC->CFGR = RCC_CFGR_MCO2 | RCC_CFGR_MCO1_0;

    Gpio_Init_ExpectAndReturn( &expectedPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO2_SYSCLK ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_MCO1_0, RCC->CFGR );
}


/**
 * \brief   Unused clock output and invalid sources do not change anything.
 *
 * \details MCO1 preset to HSE. Source NONE, source of MCO2 for MCO1 and
 *          invalid output ID are requested.
 *
 * \par Expected results
 * - NONE: RCC_REQUEST_OK, other: RCC_REQUEST_ERROR.
 * - CFGR not changed, Gpio_Init() not called (strict mock).
 */
void Ut_Rcc_Set_ClkOutSource_NoneOrInvalid_NoChange( void )
{
    RCC->CFGR = RCC_CFGR_MCO1_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO2_SYSCLK ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_MCO1_1, RCC->CFGR );
}


/**
 * \brief   Rcc_Get_ClkOutSource() decodes multiplexer of both outputs.
 *
 * \details CFGR preset: MCO1 field 0 (HSI), MCO2 field HSE.
 *
 * \par Expected results
 * - MCO1 HSI, MCO2 HSE. Invalid output and NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkOutSource_ReadsCfgr( void )
{
    rcc_ClkOut_Source_t source = RCC_CLK_SOURCE_NONE;

    RCC->CFGR = RCC_CFGR_MCO2_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO1_HSI, source );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO2, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO2_HSE, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_CNT,  &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, NULL    ) );
}


/**
 * \brief   Rcc_Set_ClkOutDivider() writes prescaler of every divider value.
 *
 * \details Dividers 1 - 5 of MCO1 and MCO2.
 *
 * \par Expected results
 * - MCOxPRE: /1 = 0, /2 = 100b, /3 = 101b, /4 = 110b, /5 = 111b.
 * - Rcc_Get_ClkOutDivider() returns the set divider, other output not changed.
 */
void Ut_Rcc_Set_ClkOutDivider_AllDividers_ReadBack( void )
{
    const uint32_t   preFieldLut[] = { 0x0u, 0x4u, 0x5u, 0x6u, 0x7u };
    rcc_ClkOut_Div_t divider       = 0u;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( preFieldLut ) > idx; idx++ )
    {
        const rcc_ClkOut_Div_t setDivider = idx + 1u;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, setDivider ) );
        TEST_ASSERT_EQUAL_HEX32( preFieldLut[ idx ] << RCC_CFGR_MCO1PRE_Pos, RCC->CFGR );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
        TEST_ASSERT_EQUAL_UINT32( setDivider, divider );

        RCC->CFGR = 0u;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, setDivider ) );
        TEST_ASSERT_EQUAL_HEX32( preFieldLut[ idx ] << RCC_CFGR_MCO2PRE_Pos, RCC->CFGR );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO2, &divider ) );
        TEST_ASSERT_EQUAL_UINT32( setDivider, divider );

        RCC->CFGR = 0u;
    }
}


/**
 * \brief   Rcc_Set_ClkOutDivider() rejects divider out of range.
 *
 * \details Dividers 0 and 6, invalid output ID.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, CFGR not written. NULL pointer rejected by getter.
 */
void Ut_Rcc_Set_ClkOutDivider_OutOfRange_ReturnsErrorWithoutWrite( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 6u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_CNT,  2u ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, NULL ) );
}

/* ========================== RESET SOURCE FLAGS ============================ */

/**
 * \brief   Rcc_Get_ResetSource() reads every reset flag from CSR.
 *
 * \details Every flag is preset alone and all sources are read.
 *
 * \par Expected results
 * - Only the source of the preset flag is ACTIVE.
 */
void Ut_Rcc_Get_ResetSource_EveryFlag_DecodedSeparately( void )
{
    const uint32_t  flagLut[ RCC_RESET_SRC_CNT ] =
    {
        [RCC_RESET_SRC_PIN]  = RCC_CSR_PINRSTF,
        [RCC_RESET_SRC_BOR]  = RCC_CSR_BORRSTF,
        [RCC_RESET_SRC_SW]   = RCC_CSR_SFTRSTF,
        [RCC_RESET_SRC_IWDG] = RCC_CSR_IWDGRSTF,
        [RCC_RESET_SRC_WWDG] = RCC_CSR_WWDGRSTF,
        [RCC_RESET_SRC_LPWR] = RCC_CSR_LPWRRSTF,
    };
    rcc_FlagState_t flagState = RCC_FLAG_INACTIVE;

    for( uint32_t setIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > setIdx; setIdx++ )
    {
        RCC->CSR = flagLut[ setIdx ];

        for( uint32_t srcIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > srcIdx; srcIdx++ )
        {
            const rcc_FlagState_t expected = ( setIdx == srcIdx ) ? RCC_FLAG_ACTIVE : RCC_FLAG_INACTIVE;

            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( (rcc_ResetSrc_t)srcIdx, &flagState ) );
            TEST_ASSERT_EQUAL( expected, flagState );
        }
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_CNT, &flagState ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_PIN, NULL ) );
}


/**
 * \brief   Rcc_Set_ResetSourceClear() clears flags and releases RMVF.
 *
 * \details Flags of software, pin, power-on and brown-out reset are preset,
 *          HW model clears flags while RMVF is set.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, all reset flags and RMVF cleared, LSION kept.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsCleared_ReleasesRmvf( void )
{
    RCC->CSR = RCC_CSR_SFTRSTF | RCC_CSR_PINRSTF | RCC_CSR_PORRSTF | RCC_CSR_BORRSTF | RCC_CSR_LSION;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( RCC_CSR_LSION, RCC->CSR & ( TEST_RCC_CSR_RESET_FLAGS | RCC_CSR_RMVF | RCC_CSR_LSION ) );
}


/**
 * \brief   Rcc_Set_ResetSourceClear() reports flags which are not cleared.
 *
 * \details No HW model - preset flags stay set.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RMVF released anyway (next resets are latched).
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsKept_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->CSR = RCC_CSR_IWDGRSTF;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( RCC_CSR_IWDGRSTF, RCC->CSR );
}

/* ===================== LOW SPEED OSCILLATORS / HSE ======================== */

/**
 * \brief   Deactivation of LSI and LSE clears enable bit and waits for ready flag.
 *
 * \details LSION / LSEON set without ready flag (oscillator stopped), then with ready flag
 *          kept (oscillator still running).
 *
 * \par Expected results
 * - Ready flag 0: RCC_REQUEST_OK, LSION / LSEON cleared, backup domain write access released
 *   for LSE (DBP).
 * - Ready flag kept: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_OscInactive_LsiLse_ClearsEnableBits( void )
{
    RCC->CSR  = RCC_CSR_LSION;
    RCC->BDCR = RCC_BDCR_LSEON;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR & RCC_CSR_LSION );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( RCC_OSC_LSE ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR & RCC_BDCR_LSEON );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR_DBP, PWR->CR & PWR_CR_DBP );

    RCC->CSR  = RCC_CSR_LSION  | RCC_CSR_LSIRDY;
    RCC->BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_LSE ) );
}


/**
 * \brief   HSE state requires both enable bit and ready flag, oscillator functions of the
 *          clock source component reject invalid arguments.
 *
 * \details Rcc_ClkSrc component (HSE is controlled by Rcc_Init() configuration only).
 *
 * \par Expected results
 * - HSEON only / HSERDY only: INACTIVE, both: ACTIVE.
 * - HSE frequency 0 and NULL pointers: RCC_REQUEST_ERROR, LSE frequency LSE_VALUE.
 */
void Ut_Rcc_ClkSrc_HseState_RequiresOnAndReady( void )
{
    rcc_FunctionState_t state   = RCC_FUNCTION_ACTIVE;
    rcc_FreqHz_t        clkFreq = 0u;

    RCC->CR = RCC_CR_HSEON;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSERDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseState( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseClk( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseRtcDiv( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseRtcClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HsiClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LseClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LsiClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LseState( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LsiState( NULL ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_LseClk( &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, clkFreq );
}


/**
 * \brief   HSE activation without HSE (type NONE) and with unsupported type.
 *
 * \details HSE is off (HSERDY 0).
 *
 * \par Expected results
 * - Type NONE: RCC_REQUEST_OK, HSEON not set.
 * - Unsupported type: RCC_REQUEST_ERROR, HSEON not set.
 */
void Ut_Rcc_ClkSrc_Set_HseActive_NoneOrInvalidType( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_NONE ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSEON );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( (rcc_HseType_t)( RCC_HSE_TYPE_SIG_IN + 1u ) ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSEON );
}


/**
 * \brief   RTC HSE divider limits and divider of low HSE frequency.
 *
 * \details HSE frequency 1 MHz (RTC clock limit) - required divider 1 is raised to the
 *          minimal divider 2. Dividers out of range are rejected.
 *
 * \par Expected results
 * - RTCPRE = 2, divided HSE clock 500 kHz, second activation keeps the divider.
 * - Dividers out of range: RCC_REQUEST_ERROR, RTCPRE unchanged.
 */
void Ut_Rcc_ClkSrc_HseRtc_LowHseMinimalDivider( void )
{
    rcc_FreqHz_t     rtcClk = 0u;
    rcc_Rtc_HseDiv_t hseDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( 1000000u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseRtcActive() );
    TEST_ASSERT_EQUAL_HEX32( 2u << RCC_CFGR_RTCPRE_Pos, RCC->CFGR & RCC_CFGR_RTCPRE );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseRtcDiv( &hseDiv ) );
    TEST_ASSERT_EQUAL_UINT16( 2u, hseDiv );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseRtcClk( &rtcClk ) );
    TEST_ASSERT_EQUAL_UINT32( 500000u, rtcClk );

    /* Divider already provides clock - unchanged */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseRtcActive() );
    TEST_ASSERT_EQUAL_HEX32( 2u << RCC_CFGR_RTCPRE_Pos, RCC->CFGR & RCC_CFGR_RTCPRE );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseRtcDiv( 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseRtcDiv( (rcc_Rtc_HseDiv_t)( ( RCC_CFGR_RTCPRE >> RCC_CFGR_RTCPRE_Pos ) + 1u ) ) );
    TEST_ASSERT_EQUAL_HEX32( 2u << RCC_CFGR_RTCPRE_Pos, RCC->CFGR & RCC_CFGR_RTCPRE );
}

/* ====================== INTERNAL COMPONENTS =============================== */

/**
 * \brief   Initialization / task functions of internal components do not touch the hardware,
 *          configuration tables are consistent.
 *
 * \details Rcc_ClkBus_Init / Deinit / Task, Rcc_ClkMux_Init / Deinit, Rcc_Pll_Init / Task.
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
    Rcc_ClkMux_Deinit();
    Rcc_Pll_Task();

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR | RCC->CFGR | RCC->PLLCFGR | RCC->BDCR | RCC->CSR );
}


/**
 * \brief   Rcc_Pll_Deinit() disables all PLLs.
 *
 * \details PLLs are on and not locked (ready flags 0 - disable succeeds), then main PLL ready
 *          flag kept (PLL can not be stopped).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLON (and PLLI2SON) cleared.
 * - Locked main PLL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Pll_Deinit_AllPllsDisabled( void )
{
#if defined(RCC_CR_PLLI2SON)
    RCC->CR = RCC_CR_PLLON | RCC_CR_PLLI2SON;
#else
    RCC->CR = RCC_CR_PLLON;
#endif /* RCC_CR_PLLI2SON */

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
 * \brief   System clock of HSE and HSI source is reported by bus clock component.
 *
 * \details SWS = HSE (HSE frequency 8 MHz), then SWS = HSI.
 *
 * \par Expected results
 * - HSE: 8 MHz and source HSE, HSI: 16 MHz.
 */
void Ut_Rcc_ClkBus_Get_SysClk_HseAndHsi( void )
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
}


/**
 * \brief   PLL component rejects invalid arguments.
 *
 * \details Invalid PLL ID, NULL pointers, invalid PLL source and RTC clock source.
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_InternalClk( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Active( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Inactive( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_State( RCC_PLL_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_State( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Source( RCC_PLL_CNT, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Source( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Source( RCC_PLL_CNT, &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Clk_OutP( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Clk_OutQ( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Clk_OutR( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_CNT, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_MAIN, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_RtcClkSource( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
}


/**
 * \brief   PLL internal clock is 0 without input divider.
 *
 * \details PLLCFGR with M = 0 (not configured), HSI source.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, clock 0.
 */
void Ut_Rcc_Pll_Get_InternalClk_ZeroDivider_ReturnsZero( void )
{
    rcc_FreqHz_t pllClk = 1u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 0u, 336u, TEST_RCC_PLLP_DIV2, 7u, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Get_InternalClk( RCC_PLL_MAIN, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, pllClk );
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
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR | RCC->CFGR | RCC->BDCR | RCC->CSR | PWR->CR );
}

/**
 * \brief   Peripheral deactivation releases the kernel clock multiplexer (AB#633).
 *
 * \details MCU with FMPI2C1 multiplexer (DCKCFGR2.FMPI2C1SEL, e.g. STM32F446): FMPI2C1
 *          activated with HSI kernel clock (HSI ready), deactivated, activated with SYSCLK.
 *          Other MCUs: test ignored.
 *
 * \par Expected results
 * - HSI activation: FMPI2C1SEL = HSI, clock enabled.
 * - Deactivation: clock disabled, FMPI2C1SEL = PCLK1 (default).
 * - SYSCLK activation: RCC_REQUEST_OK, FMPI2C1SEL = SYSCLK.
 */
void Ut_Rcc_Set_PeriphInactive_KernelClockMux_Released( void )
{
#if defined(RCC_DCKCFGR2_FMPI2C1SEL)
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_FMPI2C1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_DCKCFGR2_FMPI2C1SEL_1, RCC->DCKCFGR2 & RCC_DCKCFGR2_FMPI2C1SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR_FMPI2C1EN, RCC->APB1ENR & RCC_APB1ENR_FMPI2C1EN );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_FMPI2C1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR & RCC_APB1ENR_FMPI2C1EN );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->DCKCFGR2 & RCC_DCKCFGR2_FMPI2C1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_FMPI2C1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_DCKCFGR2_FMPI2C1SEL_0, RCC->DCKCFGR2 & RCC_DCKCFGR2_FMPI2C1SEL );
#else
    TEST_IGNORE_MESSAGE( "MCU without kernel clock multiplexer of FMPI2C1" );
#endif /* RCC_DCKCFGR2_FMPI2C1SEL */
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
 * - RCC_CR ready flags follow enable bits (HSE ready only if
 *   \ref utRcc_HseFails is false)
 * - RCC_CFGR SWS follows SW
 * - RCC_CSR LSIRDY follows LSION, reset flags are cleared while RMVF is set
 * - RCC_BDCR LSERDY follows LSEON
 * - PWR_CSR VOSRDY is set while main PLL is locked
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

    if( ( 0u != ( cr & RCC_CR_HSEON ) ) && ( false == utRcc_HseFails ) )
    {
        crRdy |= RCC_CR_HSERDY;
    }

    if( 0u != ( cr & RCC_CR_PLLON ) )
    {
        crRdy |= RCC_CR_PLLRDY;
    }

#if defined(RCC_CR_PLLI2SON)
    if( 0u != ( cr & RCC_CR_PLLI2SON ) )
    {
        crRdy |= RCC_CR_PLLI2SRDY;
    }
#endif /* RCC_CR_PLLI2SON */

    Ut_Rcc_HwModel_Update( &RCC->CR, TEST_RCC_CR_RDY_MASK, crRdy );

    Ut_Rcc_HwModel_Update( &RCC->CFGR, RCC_CFGR_SWS, ( RCC->CFGR & RCC_CFGR_SW ) << ( RCC_CFGR_SWS_Pos - RCC_CFGR_SW_Pos ) );

    const uint32_t csr     = RCC->CSR;
    const uint32_t csrMask = ( 0u != ( csr & RCC_CSR_RMVF ) ) ? ( RCC_CSR_LSIRDY | TEST_RCC_CSR_RESET_FLAGS ) : RCC_CSR_LSIRDY;

    Ut_Rcc_HwModel_Update( &RCC->CSR, csrMask, ( 0u != ( csr & RCC_CSR_LSION ) ) ? RCC_CSR_LSIRDY : 0u );

    Ut_Rcc_HwModel_Update( &RCC->BDCR, RCC_BDCR_LSERDY, ( 0u != ( RCC->BDCR & RCC_BDCR_LSEON ) ) ? RCC_BDCR_LSERDY : 0u );

    Ut_Rcc_HwModel_Update( &PWR->CSR, PWR_CSR_VOSRDY, ( 0u != ( RCC->CR & RCC_CR_PLLRDY ) ) ? PWR_CSR_VOSRDY : 0u );
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
    config->APB1_Divider                       = RCC_APB1_DIVIDER_1;
    config->Pll_Config[ RCC_PLL_MAIN ].Pll_Source = RCC_PLL_SRC_NONE;
}


/**
 * \brief Configuration of 168 MHz from 8 MHz HSE crystal PLL.
 *
 * HSE crystal 8 MHz, main PLL M 8, N 336, P 2, Q 7, AHB / 1, APB1 / 4, APB2 / 2.
 *
 * \param config [out]: Clock configuration
 */
static void Ut_Rcc_Get_HsePllConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->HSE_ClockType    = RCC_HSE_TYPE_CRYSTAL;
    config->HSE_Frequency_Hz = TEST_RCC_HSE_FREQ_HZ;
    config->APB1_Divider     = RCC_APB1_DIVIDER_4;
    config->APB2_Divider     = RCC_APB2_DIVIDER_2;

    config->Pll_Config[ RCC_PLL_MAIN ].Pll_Source   = RCC_PLL_SRC_HSE;
    config->Pll_Config[ RCC_PLL_MAIN ].M_Divider    = 8u;
    config->Pll_Config[ RCC_PLL_MAIN ].N_Multiplier = 336u;
    config->Pll_Config[ RCC_PLL_MAIN ].P_Divider    = 2u;
    config->Pll_Config[ RCC_PLL_MAIN ].Q_Divider    = 7u;
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
    }
}


/**
 * \brief Returns table of peripherals of every register bank.
 *
 * \param table    [out]: Table of peripheral registers
 * \param tableCnt [out]: Count of table items
 */
static void Ut_Rcc_Get_PeriphRegsTable( const utRcc_PeriphRegs_t ** const table, uint32_t * const tableCnt )
{
    *table    = utRcc_PeriphRegs;
    *tableCnt = TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs );
}
