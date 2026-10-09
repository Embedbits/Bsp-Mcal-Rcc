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
 *       (eg. HSION / HSIRDY are not set, SWS is 0 - a reserved value on
 *       STM32G4, HSI is SWS 01).
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
#include "Rcc_ClkOut.h"                     /* Clock output component         */
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

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSE frequency of tests - 24 MHz crystal (NUCLEO-G474RE) */
#define TEST_RCC_HSE_FREQ_HZ                ( 24000000u )

/** HSE frequency of external clock signal tests (oscillator bypassed) */
#define TEST_RCC_HSE_BYPASS_FREQ_HZ         ( 25000000u )

/** HSI16 frequency */
#define TEST_RCC_HSI_FREQ_HZ                ( 16000000u )

/** System clock of default configuration (HSI 16 MHz / 4 * 85 / 2) */
#define TEST_RCC_DEFAULT_SYSCLK_HZ          ( 170000000u )

/** PLL VCO frequency of default configuration */
#define TEST_RCC_DEFAULT_VCO_HZ             ( 340000000u )

/** System clock of HSE PLL configuration (HSE 24 MHz / 6 * 75 / 2) */
#define TEST_RCC_HSE_PLL_SYSCLK_HZ          ( 150000000u )

/** Count of milliseconds in one second */
#define TEST_RCC_MS_IN_SECOND               ( 1000u )

/** SysTick control register value after SysTick_Config() (HCLK, interrupt, enabled) */
#define TEST_RCC_SYSTICK_CTRL_ACTIVE        ( SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk )

/** Longest SysTick interval at HSI 16 MHz (24-bit reload register) */
#define TEST_RCC_SYSTICK_MAX_MS_HSI         ( ( SysTick_LOAD_RELOAD_Msk + 1u ) / ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) )

/** Field value of PLL Q / R divider 2 (field = divider / 2 - 1) */
#define TEST_RCC_PLLQR_DIV2                 ( 0u )

/** Field value of PLL Q / R divider 4 */
#define TEST_RCC_PLLQR_DIV4                 ( 1u )

/** Field value of PLL Q / R divider 6 */
#define TEST_RCC_PLLQR_DIV6                 ( 2u )

/** Ready flags of RCC_CR emulated by HW model */
#define TEST_RCC_CR_RDY_MASK                ( RCC_CR_HSIRDY | RCC_CR_HSERDY | RCC_CR_PLLRDY )

/** All reset source flags of RCC_CSR */
#define TEST_RCC_CSR_RESET_FLAGS            ( RCC_CSR_PINRSTF  | RCC_CSR_BORRSTF  | RCC_CSR_SFTRSTF  | RCC_CSR_IWDGRSTF | \
                                              RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF | RCC_CSR_OBLRSTF )

/** PWR_CR1 VOS value of voltage range 1 */
#define TEST_RCC_VOS_RANGE1                 ( PWR_CR1_VOS_0 )

/** PWR_CR1 VOS value of voltage range 2 */
#define TEST_RCC_VOS_RANGE2                 ( PWR_CR1_VOS_1 )

/** Flash latency used by Rcc_Init() while the clock tree is changed */
#define TEST_RCC_TRANSITION_LATENCY         ( FLASH_ACR_LATENCY_4WS )

/* ============================== MACROS ==================================== */

/** PLLCFGR register value (m, n - divider values, pDiv - PLLPDIV value, qField / rField - field values) */
#define TEST_RCC_PLLCFGR( m, n, pDiv, qField, rField, src )                                     \
                                            ( ( (uint32_t)( ( m ) - 1u ) << RCC_PLLCFGR_PLLM_Pos    ) | \
                                              ( (uint32_t)( n )          << RCC_PLLCFGR_PLLN_Pos    ) | \
                                              ( (uint32_t)( pDiv )       << RCC_PLLCFGR_PLLPDIV_Pos ) | \
                                              ( (uint32_t)( qField )     << RCC_PLLCFGR_PLLQ_Pos    ) | \
                                              ( (uint32_t)( rField )     << RCC_PLLCFGR_PLLR_Pos    ) | \
                                              ( src ) )

/** All PLL output enable bits */
#define TEST_RCC_PLL_OUTPUTS_EN             ( RCC_PLLCFGR_PLLPEN | RCC_PLLCFGR_PLLQEN | RCC_PLLCFGR_PLLREN )

/** Count of items of an array */
#define TEST_RCC_ARRAY_CNT( array )         ( sizeof( array ) / sizeof( ( array )[ 0u ] ) )

/* ========================== LOCAL VARIABLES =============================== */

/** HW model emulates HSE which does not start (crystal missing) */
static volatile bool     utRcc_HseFails = false;

/** AHB prescaler (HPRE) seen by HW model when the system clock switched to PLL */
static volatile uint32_t utRcc_HpreAtPllSwitch = 0xFFFFFFFFu;

/** Peripherals of every register bank (enable, reset and sleep bits) */
static const utRcc_PeriphRegs_t utRcc_PeriphRegs[] =
{
    { RCC_PERIPH_DMA2,          &RCC->AHB1ENR,  RCC_AHB1ENR_DMA2EN,      &RCC->AHB1RSTR,  RCC_AHB1RSTR_DMA2RST,      &RCC->AHB1SMENR,  RCC_AHB1SMENR_DMA2SMEN      },
    { RCC_PERIPH_CRC,           &RCC->AHB1ENR,  RCC_AHB1ENR_CRCEN,       &RCC->AHB1RSTR,  RCC_AHB1RSTR_CRCRST,       &RCC->AHB1SMENR,  RCC_AHB1SMENR_CRCSMEN       },
    { RCC_PERIPH_GPIOA,         &RCC->AHB2ENR,  RCC_AHB2ENR_GPIOAEN,     &RCC->AHB2RSTR,  RCC_AHB2RSTR_GPIOARST,     &RCC->AHB2SMENR,  RCC_AHB2SMENR_GPIOASMEN     },
    { RCC_PERIPH_ADC12_HCLK,    &RCC->AHB2ENR,  RCC_AHB2ENR_ADC12EN,     &RCC->AHB2RSTR,  RCC_AHB2RSTR_ADC12RST,     &RCC->AHB2SMENR,  RCC_AHB2SMENR_ADC12SMEN     },
#if defined(RCC_AHB3ENR_QSPIEN)
    { RCC_PERIPH_QSPI_SYSCLK,   &RCC->AHB3ENR,  RCC_AHB3ENR_QSPIEN,      &RCC->AHB3RSTR,  RCC_AHB3RSTR_QSPIRST,      &RCC->AHB3SMENR,  RCC_AHB3SMENR_QSPISMEN      },
#endif /* RCC_AHB3ENR_QSPIEN */
    { RCC_PERIPH_TIM2,          &RCC->APB1ENR1, RCC_APB1ENR1_TIM2EN,     &RCC->APB1RSTR1, RCC_APB1RSTR1_TIM2RST,     &RCC->APB1SMENR1, RCC_APB1SMENR1_TIM2SMEN     },
    { RCC_PERIPH_USART2_PCLK1,  &RCC->APB1ENR1, RCC_APB1ENR1_USART2EN,   &RCC->APB1RSTR1, RCC_APB1RSTR1_USART2RST,   &RCC->APB1SMENR1, RCC_APB1SMENR1_USART2SMEN   },
    { RCC_PERIPH_PWR,           &RCC->APB1ENR1, RCC_APB1ENR1_PWREN,      &RCC->APB1RSTR1, RCC_APB1RSTR1_PWRRST,      &RCC->APB1SMENR1, RCC_APB1SMENR1_PWRSMEN      },
    { RCC_PERIPH_LPUART1_PCLK1, &RCC->APB1ENR2, RCC_APB1ENR2_LPUART1EN,  &RCC->APB1RSTR2, RCC_APB1RSTR2_LPUART1RST,  &RCC->APB1SMENR2, RCC_APB1SMENR2_LPUART1SMEN  },
    { RCC_PERIPH_TIM1,          &RCC->APB2ENR,  RCC_APB2ENR_TIM1EN,      &RCC->APB2RSTR,  RCC_APB2RSTR_TIM1RST,      &RCC->APB2SMENR,  RCC_APB2SMENR_TIM1SMEN      },
    { RCC_PERIPH_USART1_PCLK2,  &RCC->APB2ENR,  RCC_APB2ENR_USART1EN,    &RCC->APB2RSTR,  RCC_APB2RSTR_USART1RST,    &RCC->APB2SMENR,  RCC_APB2SMENR_USART1SMEN    },
    { RCC_PERIPH_SYSCFG,        &RCC->APB2ENR,  RCC_APB2ENR_SYSCFGEN,    &RCC->APB2RSTR,  RCC_APB2RSTR_SYSCFGRST,    &RCC->APB2SMENR,  RCC_APB2SMENR_SYSCFGSMEN    },
};

/** Clock enable registers of all register banks */
static volatile uint32_t * const utRcc_EnableRegs[] =
{
    &RCC->AHB1ENR, &RCC->AHB2ENR, &RCC->AHB3ENR, &RCC->APB1ENR1, &RCC->APB1ENR2, &RCC->APB2ENR, &RCC->BDCR
};

/** Reset registers of all register banks */
static volatile uint32_t * const utRcc_ResetRegs[] =
{
    &RCC->AHB1RSTR, &RCC->AHB2RSTR, &RCC->AHB3RSTR, &RCC->APB1RSTR1, &RCC->APB1RSTR2, &RCC->APB2RSTR
};

/** Clock enable in sleep mode registers of all register banks */
static volatile uint32_t * const utRcc_SleepRegs[] =
{
    &RCC->AHB1SMENR, &RCC->AHB2SMENR, &RCC->AHB3SMENR, &RCC->APB1SMENR1, &RCC->APB1SMENR2, &RCC->APB2SMENR
};

/** Expected configuration of MCO pin PA8 (alternate function 0) */
static const gpio_Config_t utRcc_McoPin =
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

/** Expected configuration of LSCO pin PA2 (analog mode) */
static const gpio_Config_t utRcc_LscoPin =
{
    .PortId         = GPIO_PORT_A,
    .PinId          = GPIO_PIN_ID_2,
    .PinMode        = GPIO_PIN_MODE_ANALOG,
    .PinPull        = GPIO_PIN_PULL_NONE,
    .PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH,
    .PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL,
    .PinAltFunction = GPIO_ALT_FUNC_0,
    .PinActiveLevel = GPIO_PIN_LEVEL_HIGH
};

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    utRcc_HseFails        = false;
    utRcc_HpreAtPllSwitch = 0xFFFFFFFFu;

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
 * \brief   Rcc_Get_DefaultConfig() fills configuration of 170 MHz from HSI PLL.
 *
 * \details Reads default configuration into structure filled by pattern.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSE not used, system clock from PLL.
 * - PLL: HSI, M 4, N 85, P 6, Q not used, R 2.
 * - AHB / 1, APB1 / 1, APB2 / 1, SysTick 1 ms, CSS off, voltage range 1 boost.
 * - Clock outputs not used (source NONE, divider 1).
 */
void Ut_Rcc_Get_DefaultConfig_FillsHsiPll170MHz( void )
{
    rcc_ConfigStruct_t config;

    (void)memset( &config, 0xA5, sizeof( config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_HSE_TYPE_NONE,           config.HSE_ClockType );
    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_PLL, config.SystemClockSource );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE,       config.CSS_Enable );
    TEST_ASSERT_EQUAL( RCC_AHB_DIVIDER_1,           config.AHB_Divider );
    TEST_ASSERT_EQUAL( RCC_APB1_DIVIDER_1,          config.APB1_Divider );
    TEST_ASSERT_EQUAL( RCC_APB2_DIVIDER_1,          config.APB2_Divider );
    TEST_ASSERT_EQUAL_UINT32( 1u,                   config.SysTickInterval );
    TEST_ASSERT_EQUAL( RCC_FLASH_LATENCY_0_WS,      config.FlashLatency );
    TEST_ASSERT_EQUAL( RCC_PWR_VOLTAGE_SCALE_0,     config.VoltageScaling );

    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, config.Pll_Config[ RCC_PLL_1 ].Pll_Source );
    TEST_ASSERT_EQUAL_UINT32( 4u,       config.Pll_Config[ RCC_PLL_1 ].M_Divider );
    TEST_ASSERT_EQUAL_UINT32( 85u,      config.Pll_Config[ RCC_PLL_1 ].N_Multiplier );
    TEST_ASSERT_EQUAL_UINT32( 6u,       config.Pll_Config[ RCC_PLL_1 ].P_Divider );
    TEST_ASSERT_EQUAL_UINT32( 0u,       config.Pll_Config[ RCC_PLL_1 ].Q_Divider );
    TEST_ASSERT_EQUAL_UINT32( 2u,       config.Pll_Config[ RCC_PLL_1 ].R_Divider );

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
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Rcc_Init() with default configuration sets 170 MHz from HSI PLL in boost mode.
 *
 * \details HW model emulates ready flags and system clock switch.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, system clock source (SWS) is PLL.
 * - PLLCFGR: HSI source, M 4, N 85, PDIV 6 (output P enabled), R 2 (output R
 *   enabled), output Q disabled. HSI and PLL on, HSE and CSS off.
 * - Flash: 4 wait states (170 MHz, range 1 boost), prefetch and caches enabled.
 * - PWR and SYSCFG interface clocks enabled, voltage range 1 (VOS 01) in
 *   boost mode (R1MODE 0).
 * - AHB / 1, APB1 / 1, APB2 / 1, SysTick reload 170000 - 1, SystemCoreClock 170 MHz.
 */
void Ut_Rcc_Init_DefaultConfig_Configures170MHzBoost( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    PWR->CR5 = PWR_CR5_R1MODE;              /* HW reset value - normal mode */

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 4u, 85u, 6u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                             RCC_PLLCFGR_PLLPEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_PLLON,
                             RCC->CR & ( RCC_CR_HSION | RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_4WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN, FLASH->ACR );

    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN,   RCC->APB1ENR1 );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2ENR_SYSCFGEN, RCC->APB2ENR );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1,  PWR->CR1 & PWR_CR1_VOS );
    TEST_ASSERT_EQUAL_HEX32( 0u,                   PWR->CR5 & PWR_CR5_R1MODE );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1,  RCC->CFGR & RCC_CFGR_HPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE1_DIV1, RCC->CFGR & RCC_CFGR_PPRE1 );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_PPRE2_DIV1, RCC->CFGR & RCC_CFGR_PPRE2 );

    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_DEFAULT_SYSCLK_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SYSTICK_CTRL_ACTIVE, SysTick->CTRL );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() divides HCLK by 2 while the system clock is switched to PLL above 80 MHz.
 *
 * \details Default configuration (170 MHz, AHB / 1). HW model records the AHB
 *          prescaler at the moment the system clock switch status changes to PLL.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE was /2 during the switch (transition state of RM0440),
 *   HPRE is /1 after Rcc_Init().
 */
void Ut_Rcc_Init_PllAbove80MHz_AhbTransitionStateUsed( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV2, utRcc_HpreAtPllSwitch );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1, RCC->CFGR & RCC_CFGR_HPRE );
}


/**
 * \brief   Rcc_Init() switches to PLL of 80 MHz or less without transition state.
 *
 * \details PLL HSI / 4 * 40 / 2 = 80 MHz, AHB / 1.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE was /1 during the switch, flash latency 2 wait states.
 */
void Ut_Rcc_Init_Pll80MHz_NoTransitionState( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = 40u;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV1, utRcc_HpreAtPllSwitch );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_2WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( 80000000u, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() configures 150 MHz from HSE crystal PLL in range 1 normal mode with CSS.
 *
 * \details HSE crystal 24 MHz, PLL M 6, N 75, P 10, Q 6, R 2, range 1 normal
 *          mode, AHB / 1, APB1 / 2, APB2 / 1, CSS enabled. HW model emulates
 *          ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS is PLL, HSE on (not bypassed), CSS on, PLL on.
 * - PLLCFGR: HSE source, M 6, N 75, PDIV 10, Q 6, R 2, all outputs enabled.
 * - Flash latency 4 wait states (120 - 150 MHz, range 1 normal), R1MODE 1.
 * - Clocks: AHB 150 MHz, APB1 75 MHz, APB2 150 MHz, APB1 timers 150 MHz,
 *   APB2 timers 150 MHz, ADC 15 MHz (P), FDCAN 50 MHz (Q). SystemCoreClock 150 MHz.
 */
void Ut_Rcc_Init_HseCrystalPll_Configures150MHzNormal( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       clkFreq = 0u;

    Ut_Rcc_Get_HsePllConfig( &config );
    config.CSS_Enable = RCC_FUNCTION_ACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON,
                             RCC->CR & ( RCC_CR_HSEON | RCC_CR_HSEBYP | RCC_CR_CSSON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 6u, 75u, 10u, TEST_RCC_PLLQR_DIV6, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSE ) |
                             TEST_RCC_PLL_OUTPUTS_EN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_4WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE, PWR->CR5 & PWR_CR5_R1MODE );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, SystemCoreClock );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ / 2u, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );

    /* Timers of APB with prescaler > 1 are clocked by 2 x PCLK */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_PLL_SYSCLK_HZ, clkFreq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_PLLP, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( 30000000u, clkFreq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_FDCAN_PLLQ, &clkFreq ) );
    TEST_ASSERT_EQUAL_UINT32( 50000000u, clkFreq );
}


/**
 * \brief   Rcc_Init() with HSE external clock signal as system clock.
 *
 * \details HSE bypass 25 MHz used directly as system clock, PLL not used.
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
 * \details No HW model - HSI ready flag and SWS of HSI are preset. PLL and HSE
 *          are not used, so the module only waits for flags which are already
 *          in the expected state.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SW / SWS is HSI, PLL and HSE off.
 * - SysTick reload 16000 - 1, SystemCoreClock 16 MHz, flash latency 0 wait states.
 */
void Ut_Rcc_Init_HsiSysClk_NoPllRequired( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SW_HSI | RCC_CFGR_SWS_HSI, RCC->CFGR & ( RCC_CFGR_SW | RCC_CFGR_SWS ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION, RCC->CR & ( RCC_CR_HSION | RCC_CR_HSEON | RCC_CR_PLLON ) );
    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, SystemCoreClock );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_0WS, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Init() in voltage range 2 sets range 2 and normal mode.
 *
 * \details HSI system clock (16 MHz), voltage range 2 (max. 26 MHz). HSI ready
 *          flag and SWS of HSI preset, boost mode preset (R1MODE 0).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOS = range 2, R1MODE set (boost not used).
 * - Flash latency 1 wait state (12 - 24 MHz in range 2).
 */
void Ut_Rcc_Init_Range2_SetsVosRange2AndLatency( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE2, PWR->CR1 & PWR_CR1_VOS );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE,      PWR->CR5 & PWR_CR5_R1MODE );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_1WS, FLASH->ACR & FLASH_ACR_LATENCY );
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
 * - RCC_REQUEST_ERROR, system clock stays HSI, PLL is not activated.
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
 * \brief   Rcc_Init() rejects system clock above limit of voltage range.
 *
 * \details Default 170 MHz configuration with range 1 normal mode (max. 150 MHz).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock stays HSI, flash latency keeps the
 *   transition value (4 wait states, safe for every clock).
 */
void Ut_Rcc_Init_SysClkOverRangeLimit_ReturnsErrorOnHsi( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_HSI, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_TRANSITION_LATENCY, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Init() reconfigures PLL which drives the system clock.
 *
 * \details Default configuration (170 MHz) is initialized first, then PLL is
 *          changed to N 80 (160 MHz) by second Rcc_Init().
 *
 * \par Expected results
 * - Both calls return RCC_REQUEST_OK, SWS is PLL.
 * - PLLCFGR holds the new configuration, flash latency 4 wait states,
 *   SystemCoreClock 160 MHz.
 */
void Ut_Rcc_Init_CalledTwice_ReconfiguresRunningPll( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].N_Multiplier = 80u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SWS_PLL, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 4u, 80u, 6u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                             RCC_PLLCFGR_PLLPEN | RCC_PLLCFGR_PLLREN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_4WS, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_UINT32( 160000000u, SystemCoreClock );
}


/**
 * \brief   Rcc_Init() configures clock outputs and their pins.
 *
 * \details HSI system clock configuration, MCO source HSI divided by 4, LSCO
 *          source LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Gpio_Init() called for PA8 (alternate function 0, very high
 *   speed, push-pull, no pull) and PA2 (analog).
 * - MCOSEL selects HSI, MCOPRE = divider 4, LSCOEN and LSCOSEL (LSE) set.
 */
void Ut_Rcc_Init_ClockOutputs_ConfiguresMcoLscoAndPins( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLK_SOURCE_MCO1_HSI;
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 4u;
    config.McoConfig[ RCC_CLK_OUT_LSCO ].ClockSource  = RCC_CLK_SOURCE_LSCO_LSE;

    RCC->CR   = RCC_CR_HSIRDY;
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_McoPin,  GPIO_REQUEST_OK );
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_LscoPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1SOURCE_HSI, RCC->CFGR & RCC_CFGR_MCOSEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_MCOPRE_DIV4,  RCC->CFGR & RCC_CFGR_MCOPRE );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL, RCC->BDCR & ( RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL ) );
}

/* ========================= PERIPHERAL CLOCKS ============================== */

/**
 * \brief   Rcc_Set_PeriphActive() sets enable bit of the peripheral.
 *
 * \details Activates and deactivates peripherals of every register bank (AHB1,
 *          AHB2, AHB3, APB1 1 / 2, APB2) one by one and reads their state.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, only own enable bit is set in all enable
 *   registers, state is ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, enable register is 0, state is INACTIVE.
 */
void Ut_Rcc_Set_PeriphActive_EveryRegBank_SetsOwnEnableBit( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs ) > idx; idx++ )
    {
        const utRcc_PeriphRegs_t * const periph = &utRcc_PeriphRegs[ idx ];

        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( periph->EnableMask, *periph->EnableReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_EnableRegs, TEST_RCC_ARRAY_CNT( utRcc_EnableRegs ), periph->EnableReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *periph->EnableReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Rcc_Set_PeriphInactive() clears only own enable bit.
 *
 * \details GPIOA and GPIOB clocks are preset, GPIOA is deactivated.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, AHB2ENR holds GPIOB enable bit only.
 */
void Ut_Rcc_Set_PeriphInactive_KeepsOtherEnableBits( void )
{
    RCC->AHB2ENR = RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_GPIOBEN, RCC->AHB2ENR );
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
 * \brief   Activation of HSI48 clocked peripheral starts HSI48 oscillator.
 *
 * \details RNG with HSI48 kernel clock, HSI48 ready flag preset (HW reaction).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSI48ON set, CLK48SEL = HSI48 (0), RNG clock enabled.
 * - Rcc_Get_PeriphClk() returns 48 MHz.
 */
void Ut_Rcc_Set_PeriphActive_Hsi48ClockedPeriph_StartsHsi48( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CRRCR = RCC_CRRCR_HSI48RDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CRRCR_HSI48ON, RCC->CRRCR & RCC_CRRCR_HSI48ON );
    TEST_ASSERT_EQUAL_HEX32( 0u,                RCC->CCIPR & RCC_CCIPR_CLK48SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_HSI48, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );
}


/**
 * \brief   Peripheral activation selects kernel clock source in CCIPR.
 *
 * \details USART1 with HSI kernel clock (HSI ready preset), then activation
 *          with SYSCLK while HSI is selected, deactivation and activation with
 *          SYSCLK.
 *
 * \par Expected results
 * - HSI: RCC_REQUEST_OK, HSION set, USART1SEL = HSI, USART1 clock enabled.
 * - SYSCLK while HSI selected: RCC_REQUEST_ERROR, USART1SEL unchanged.
 * - Deactivation: clock disabled, USART1SEL = PCLK2 (default).
 * - SYSCLK: RCC_REQUEST_OK, USART1SEL = SYSCLK.
 */
void Ut_Rcc_Set_PeriphActive_KernelClockMux_SelectsAndReleasesSource( void )
{
    RCC->CR = RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION,            RCC->CR & RCC_CR_HSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_USART1SEL_1,   RCC->CCIPR & RCC_CCIPR_USART1SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2ENR_USART1EN,    RCC->APB2ENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_USART1SEL_1,   RCC->CCIPR & RCC_CCIPR_USART1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB2ENR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR & RCC_CCIPR_USART1SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_USART1SEL_0,   RCC->CCIPR & RCC_CCIPR_USART1SEL );
}


/**
 * \brief   Kernel clock multiplexers of CCIPR and CCIPR2 are written by activation.
 *
 * \details ADC12 with PLL P, LPTIM1 with LSE, FDCAN with PCLK1, I2C4 with
 *          SYSCLK (CCIPR2, where available).
 *
 * \par Expected results
 * - ADC12SEL = PLLP (01), LPTIM1SEL = LSE (11), FDCANSEL = PCLK1 (10),
 *   I2C4SEL = SYSCLK (01), clocks of the peripherals enabled.
 */
void Ut_Rcc_Set_PeriphActive_CcipRegisters_SelectSources( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC12_PLLP ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_LPTIM1_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_FDCAN_PCLK1 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_ADC12SEL_0, RCC->CCIPR & RCC_CCIPR_ADC12SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_LPTIM1SEL,  RCC->CCIPR & RCC_CCIPR_LPTIM1SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_FDCANSEL_1, RCC->CCIPR & RCC_CCIPR_FDCANSEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2ENR_ADC12EN,  RCC->AHB2ENR );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_LPTIM1EN | RCC_APB1ENR1_FDCANEN, RCC->APB1ENR1 );

#if defined(RCC_APB1ENR2_I2C4EN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C4_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR2_I2C4SEL_0, RCC->CCIPR2 & RCC_CCIPR2_I2C4SEL );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR2_I2C4EN,  RCC->APB1ENR2 );
#endif /* RCC_APB1ENR2_I2C4EN */
}


/**
 * \brief   48 MHz clock multiplexer shared by RNG and USB is kept while the other block is enabled.
 *
 * \details RNG and USB activated with PLL Q kernel clock, RNG deactivated,
 *          then USB deactivated. MCUs without USB: test ignored.
 *
 * \par Expected results
 * - Activation: CLK48SEL = PLLQ (10), RNG and USB clocks enabled.
 * - RNG deactivation: RNG clock disabled, CLK48SEL kept (USB still enabled).
 * - USB deactivation: USB clock disabled, CLK48SEL = HSI48 (default).
 */
void Ut_Rcc_Set_PeriphInactive_SharedClk48Mux_KeptWhileOtherBlockEnabled( void )
{
#if defined(RCC_APB1ENR1_USBEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_PLLQ ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USB_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL_1, RCC->CCIPR & RCC_CCIPR_CLK48SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RNG_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB2ENR & RCC_AHB2ENR_RNGEN );
    TEST_ASSERT_EQUAL_HEX32( RCC_CCIPR_CLK48SEL_1, RCC->CCIPR & RCC_CCIPR_CLK48SEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USB_PLLQ ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->APB1ENR1 & RCC_APB1ENR1_USBEN );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CCIPR & RCC_CCIPR_CLK48SEL );
#else
    TEST_IGNORE_MESSAGE( "MCU without USB device" );
#endif /* RCC_APB1ENR1_USBEN */
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
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN, RCC->APB1ENR1 & RCC_APB1ENR1_PWREN );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR1_DBP, PWR->CR1 & PWR_CR1_DBP );
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
 * \brief   RTC activation with HSE divided by 32.
 *
 * \details HSE frequency 24 MHz is configured.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCSEL = HSE / 32, RTCEN set.
 * - Rcc_Get_PeriphClk() of the RTC returns 750 kHz.
 */
void Ut_Rcc_Set_PeriphActive_RtcHseDiv32_SelectsHseClock( void )
{
    rcc_FreqHz_t rtcClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_FREQ_HZ ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_HSE_DIV32 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN, RCC->BDCR & ( RCC_BDCR_RTCSEL | RCC_BDCR_RTCEN ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV32, &rtcClk ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_FREQ_HZ / 32u, rtcClk );
}


/**
 * \brief   RTC activation with HSE fails when HSE frequency is not known.
 *
 * \details HSE frequency is not configured (0).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, BDCR is not written.
 */
void Ut_Rcc_Set_PeriphActive_RtcHseDiv32NoHse_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_HSE_DIV32 ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
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
 * - GPIOA (AHB2) 8 MHz, USART2 (APB1) 4 MHz, USART1 (APB2) 2 MHz.
 * - TIM2 (APB1 timer) 8 MHz, TIM1 (APB2 timer) 4 MHz (2 x PCLK).
 */
void Ut_Rcc_Get_PeriphClk_BusClockedPeriphs_ReturnBusFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CFGR = RCC_CFGR_SWS_HSI | RCC_CFGR_HPRE_DIV2 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV4;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_GPIOA, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 8000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART2_PCLK1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 4000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_PCLK2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 2000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
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
 * - TIM2 and TIM1 clock 16 MHz.
 */
void Ut_Rcc_Get_PeriphClk_ApbNotDivided_TimerClockEqualsPclk( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TIM1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
}


/**
 * \brief   PLL output clocked peripherals return frequency of the output.
 *
 * \details PLLCFGR preset: HSI, M 4, N 85 (VCO 340 MHz), PDIV 6, Q 4, R 2, all
 *          outputs enabled.
 *
 * \par Expected results
 * - ADC12 PLLP 56666666 Hz, FDCAN / RNG PLLQ 85 MHz.
 */
void Ut_Rcc_Get_PeriphClk_PllOutputs_ReturnPllFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 4u, 85u, 6u, TEST_RCC_PLLQR_DIV4, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) | TEST_RCC_PLL_OUTPUTS_EN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_PLLP, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 6u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_FDCAN_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 4u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 4u, freq );
}


/**
 * \brief   Peripheral clocked by disabled PLL output has no clock.
 *
 * \details PLLCFGR preset with outputs P and R enabled, Q disabled.
 *
 * \par Expected results
 * - FDCAN PLLQ: RCC_REQUEST_ERROR. ADC12 PLLP: RCC_REQUEST_OK.
 */
void Ut_Rcc_Get_PeriphClk_PllOutputDisabled_ReturnsError( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 4u, 85u, 6u, TEST_RCC_PLLQR_DIV4, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) |
                   RCC_PLLCFGR_PLLPEN | RCC_PLLCFGR_PLLREN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_FDCAN_PLLQ, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_PLLP, &freq ) );
}


/**
 * \brief   Oscillator clocked peripherals return oscillator frequency.
 *
 * \par Expected results
 * - IWDG and RTC with LSI clock 32 kHz, RTC with LSE clock 32.768 kHz, USART1
 *   with HSI 16 MHz, LPUART1 with LSE 32.768 kHz, UCPD1 (where available) HSI.
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_HSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPUART1_LSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );
#if defined(RCC_APB1ENR2_UCPD1EN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_UCPD1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
#endif /* RCC_APB1ENR2_UCPD1EN */
}


/**
 * \brief   Rcc_Get_PeriphClk() rejects peripheral without known kernel clock.
 *
 * \details I2S of SPI2 / SPI3 with external I2S_CKIN clock (where available),
 *          RTC with HSE while HSE frequency is not configured.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, frequency 0.
 */
void Ut_Rcc_Get_PeriphClk_NoKernelClock_ReturnsErrorAndZero( void )
{
    rcc_FreqHz_t freq = 1u;

#if defined(RCC_CCIPR_I2S23SEL)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_I2S23_I2S_CKIN, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
    freq = 1u;
#endif /* RCC_CCIPR_I2S23SEL */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV32, &freq ) );
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

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_GPIOA, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_GPIOA, srcId );
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


/**
 * \brief   Rcc_Get_PeriphClkSrc() returns ID matching selected kernel clock of CCIPR.
 *
 * \details USART2SEL preset to SYSCLK, ADC12SEL to SYSCLK, CLK48SEL to PLL Q.
 *
 * \par Expected results
 * - USART2 (queried by PCLK1 ID): RCC_PERIPH_USART2_SYSCLK.
 * - ADC12 (queried by HCLK ID): RCC_PERIPH_ADC12_SYSCLK.
 * - RNG (queried by HSI48 ID): RCC_PERIPH_RNG_PLLQ, USB (where available):
 *   RCC_PERIPH_USB_PLLQ - own ID of the block sharing the multiplexer.
 */
void Ut_Rcc_Get_PeriphClkSrc_KernelMux_ReturnsSelectedSource( void )
{
    rcc_PeriphId_t srcId = RCC_PERIPH_ID_CNT;

    RCC->CCIPR = RCC_CCIPR_USART2SEL_0 | RCC_CCIPR_ADC12SEL_1 | RCC_CCIPR_CLK48SEL_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART2_PCLK1, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART2_SYSCLK, srcId );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ADC12_HCLK, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_ADC12_SYSCLK, srcId );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_RNG_HSI48, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_RNG_PLLQ, srcId );
#if defined(RCC_APB1ENR1_USBEN)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USB_HSI48, &srcId ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USB_PLLQ, srcId );
#endif /* RCC_APB1ENR1_USBEN */
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
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs ) > idx; idx++ )
    {
        const utRcc_PeriphRegs_t * const periph = &utRcc_PeriphRegs[ idx ];

        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetActive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( periph->ResetMask, *periph->ResetReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs, TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ), periph->ResetReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *periph->ResetReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Reset of peripheral without reset control is rejected.
 *
 * \details RTC APB interface and WWDG (no reset bit on STM32G4) and RTC
 *          (backup domain, no reset register) are reset.
 *
 * \par Expected results
 * - Reset active: RCC_REQUEST_ERROR, no reset register written.
 * - Reset inactive: RCC_REQUEST_OK (never in reset), reset state INACTIVE.
 */
void Ut_Rcc_Set_ResetActive_NoResetControl_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_RTCAPB ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_WWDG ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_RTC_LSE ) );
    Ut_Rcc_Check_OtherRegsClear( utRcc_ResetRegs, TEST_RCC_ARRAY_CNT( utRcc_ResetRegs ), NULL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_WWDG ) );
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
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( utRcc_PeriphRegs ) > idx; idx++ )
    {
        const utRcc_PeriphRegs_t * const periph = &utRcc_PeriphRegs[ idx ];

        Ut_Rcc_Clear_PeriphRegs();

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( periph->SleepMask, *periph->SleepReg );
        Ut_Rcc_Check_OtherRegsClear( utRcc_SleepRegs, TEST_RCC_ARRAY_CNT( utRcc_SleepRegs ), periph->SleepReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( periph->PeriphId ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, *periph->SleepReg );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( periph->PeriphId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Sleep-only blocks (SRAM) are controlled in sleep register.
 *
 * \details SRAM1 (AHB1SMENR) and CCM SRAM (AHB2SMENR) have sleep mode clock
 *          control only. Other sleep bits of the registers are preset.
 *
 * \par Expected results
 * - Sleep inactive clears SRAM1SMEN / CCMSRAMSMEN, sleep active sets them,
 *   other bits kept.
 * - Clock enable state of the blocks is always ACTIVE (no enable bit).
 */
void Ut_Rcc_Set_SleepInactive_SramBlocks_ClearSleepBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    RCC->AHB1SMENR = RCC_AHB1SMENR_SRAM1SMEN   | RCC_AHB1SMENR_DMA1SMEN;
    RCC->AHB2SMENR = RCC_AHB2SMENR_CCMSRAMSMEN | RCC_AHB2SMENR_GPIOASMEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_SRAM1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_CCMSRAM ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1SMENR_DMA1SMEN,  RCC->AHB1SMENR );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2SMENR_GPIOASMEN, RCC->AHB2SMENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_SRAM1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_CCMSRAM ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1SMENR_SRAM1SMEN   | RCC_AHB1SMENR_DMA1SMEN,  RCC->AHB1SMENR );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB2SMENR_CCMSRAMSMEN | RCC_AHB2SMENR_GPIOASMEN, RCC->AHB2SMENR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_SRAM1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
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
 * \details Sets AHB3 / 8 (shared AHB divider), APB1 group 2 / 4 (shared APB1
 *          divider) and APB2 / 16.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HPRE = /8, PPRE1 = /4, PPRE2 = /16.
 * - Rcc_Get_ClkBusDivider() returns the set dividers, AHB1 equals AHB3,
 *   APB1 group 1 equals group 2.
 */
void Ut_Rcc_Set_ClkBusDivider_AllBuses_WritesCfgr( void )
{
    rcc_ClkBusDiv_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB3,   RCC_AHB_DIVIDER_8   ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_2, RCC_APB1_DIVIDER_4  ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB2_DIVIDER_16 ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_HPRE_DIV8 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV16, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB_DIVIDER_8, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_1, &divider ) );
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

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB2_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_APB1_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT,    RCC_AHB_DIVIDER_2  ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_CNT,    &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_1, NULL     ) );
}


/**
 * \brief   Rcc_Get_ClkBusClk() calculates bus clocks from PLL system clock.
 *
 * \details Registers preset: PLL HSI / 4 * 85 / 2 (170 MHz) is system clock,
 *          AHB / 1, APB1 / 2, APB2 / 1.
 *
 * \par Expected results
 * - AHB1 / AHB2 / AHB3 170 MHz, APB1 groups 85 MHz, APB2 170 MHz.
 * - Invalid bus or NULL pointer returns RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkBusClk_PllSysClk_ReturnsBusFreq( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 4u, 85u, 6u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) | RCC_PLLCFGR_PLLREN;
    RCC->CFGR    = RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV2;

    for( rcc_ClkBusId_t busId = RCC_CLK_BUS_AHB1; RCC_CLK_BUS_AHB3 >= busId; busId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( busId, &freq ) );
        TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );
    }
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_SYSCLK_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_CNT,    &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, NULL  ) );
}

/* ========================= POWER RANGE AND FLASH ========================== */

/**
 * \brief   Rcc_Set_PwrRange() writes voltage range 2 and normal mode.
 *
 * \details System clock HSI 16 MHz (below 26 MHz), VOS preset to range 1 in
 *          boost mode, range 2 is requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOS = range 2, R1MODE set, PWR interface clock enabled.
 */
void Ut_Rcc_Set_PwrRange_Range2_WritesVosAndNormalMode( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    PWR->CR1  = TEST_RCC_VOS_RANGE1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE2, PWR->CR1 & PWR_CR1_VOS );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE,      PWR->CR5 & PWR_CR5_R1MODE );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1ENR1_PWREN,  RCC->APB1ENR1 );
}


/**
 * \brief   Rcc_Set_PwrRange() sets range 1 and selects boost or normal mode.
 *
 * \details System clock HSI 16 MHz, VOS preset to range 2 with R1MODE set.
 *          Range 1 boost (scale 0) is requested, then range 1 normal (scale 1).
 *
 * \par Expected results
 * - Scale 0: RCC_REQUEST_OK, VOS = range 1, R1MODE cleared (boost).
 * - Scale 1: RCC_REQUEST_OK, VOS = range 1, R1MODE set (normal).
 */
void Ut_Rcc_Set_PwrRange_Range1_SelectsBoostOrNormal( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    PWR->CR1  = TEST_RCC_VOS_RANGE2;
    PWR->CR5  = PWR_CR5_R1MODE;

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_0;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1, PWR->CR1 & PWR_CR1_VOS );
    TEST_ASSERT_EQUAL_HEX32( 0u,                  PWR->CR5 & PWR_CR5_R1MODE );

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1, PWR->CR1 & PWR_CR1_VOS );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR5_R1MODE,      PWR->CR5 & PWR_CR5_R1MODE );
}


/**
 * \brief   Rcc_Set_PwrRange() rejects range whose maximal clock is below the actual system clock.
 *
 * \details PLL 170 MHz is the system clock (registers preset), VOS range 1
 *          boost. Range 1 normal (max. 150 MHz) and range 2 are requested,
 *          then boost again. NULL configuration and unknown range.
 *
 * \par Expected results
 * - Range 1 normal / range 2: RCC_REQUEST_ERROR, VOS and R1MODE not changed.
 * - Range 1 boost: RCC_REQUEST_OK.
 * - NULL / unknown range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PwrRange_SysClkAboveRange_ChangeRejected( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 4u, 85u, 6u, 0u, TEST_RCC_PLLQR_DIV2, RCC_PLLCFGR_PLLSRC_HSI ) | RCC_PLLCFGR_PLLREN;
    RCC->CFGR    = RCC_CFGR_SWS_PLL;
    PWR->CR1     = TEST_RCC_VOS_RANGE1;

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_VOS_RANGE1, PWR->CR1 & PWR_CR1_VOS );
    TEST_ASSERT_EQUAL_HEX32( 0u,                  PWR->CR5 & PWR_CR5_R1MODE );

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_0;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );

    config.VoltageScaling = (rcc_PwrVoltageScale_t)3u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
}


/**
 * \brief   Rcc_Set_PwrRange() fails when the voltage scaling does not finish.
 *
 * \details PWR_SR2 VOSF preset (regulator not ready), HSI system clock.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR is returned.
 */
void Ut_Rcc_Set_PwrRange_VoltageNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    RCC->CFGR = RCC_CFGR_SWS_HSI;
    PWR->SR2  = PWR_SR2_VOSF;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() calculates wait states from HCLK (range 1 boost).
 *
 * \details HSE system clock with frequencies around thresholds of range 1
 *          boost mode (34, 68, 102, 136, 170 MHz), AHB / 1.
 *
 * \par Expected results
 * - Every exceeded threshold adds one wait state, above 170 MHz error.
 */
void Ut_Rcc_Set_FlashLatency_BoostThresholds_SetsWaitStates( void )
{
    const struct
    {
        rcc_FreqHz_t SysClk;
        uint32_t     Latency;
    }   latencyLut[] =
    {
        {  16000000u, FLASH_ACR_LATENCY_0WS },
        {  34000000u, FLASH_ACR_LATENCY_0WS },
        {  34000001u, FLASH_ACR_LATENCY_1WS },
        {  68000001u, FLASH_ACR_LATENCY_2WS },
        { 102000001u, FLASH_ACR_LATENCY_3WS },
        { 136000001u, FLASH_ACR_LATENCY_4WS },
        { 170000000u, FLASH_ACR_LATENCY_4WS },
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

    config.HSE_Frequency_Hz = 170000001u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() thresholds of range 1 normal mode and range 2.
 *
 * \details HSE system clock around thresholds of range 1 normal (30, 60, 90,
 *          120, 150 MHz) and range 2 (12, 24, 26 MHz).
 *
 * \par Expected results
 * - Every exceeded threshold adds one wait state.
 * - Frequency above the maximum of the range (150 MHz / 26 MHz): error.
 */
void Ut_Rcc_Set_FlashLatency_NormalAndRange2Thresholds_SetsWaitStates( void )
{
    const struct
    {
        rcc_PwrVoltageScale_t Scale;
        rcc_FreqHz_t          SysClk;
        uint32_t              Latency;
    }   latencyLut[] =
    {
        { RCC_PWR_VOLTAGE_SCALE_1,  30000000u, FLASH_ACR_LATENCY_0WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  30000001u, FLASH_ACR_LATENCY_1WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  60000001u, FLASH_ACR_LATENCY_2WS },
        { RCC_PWR_VOLTAGE_SCALE_1,  90000001u, FLASH_ACR_LATENCY_3WS },
        { RCC_PWR_VOLTAGE_SCALE_1, 120000001u, FLASH_ACR_LATENCY_4WS },
        { RCC_PWR_VOLTAGE_SCALE_1, 150000000u, FLASH_ACR_LATENCY_4WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  12000000u, FLASH_ACR_LATENCY_0WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  12000001u, FLASH_ACR_LATENCY_1WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  24000001u, FLASH_ACR_LATENCY_2WS },
        { RCC_PWR_VOLTAGE_SCALE_2,  26000000u, FLASH_ACR_LATENCY_2WS },
    };
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( latencyLut ) > idx; idx++ )
    {
        config.VoltageScaling   = latencyLut[ idx ].Scale;
        config.HSE_Frequency_Hz = latencyLut[ idx ].SysClk;

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
        TEST_ASSERT_EQUAL_HEX32( latencyLut[ idx ].Latency, FLASH->ACR & FLASH_ACR_LATENCY );
    }

    config.VoltageScaling   = RCC_PWR_VOLTAGE_SCALE_1;
    config.HSE_Frequency_Hz = 150000001u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.VoltageScaling   = RCC_PWR_VOLTAGE_SCALE_2;
    config.HSE_Frequency_Hz = 26000001u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
}


/**
 * \brief   Rcc_Set_FlashLatency() uses HCLK (AHB divider) and minimal latency of the user.
 *
 * \details HSE 136 MHz with AHB / 2 (HCLK 68 MHz), then minimal latency 6 WS.
 *
 * \par Expected results
 * - AHB / 2: 1 wait state (68 MHz HCLK, range 1 boost).
 * - Minimal latency 6 WS: 6 wait states.
 */
void Ut_Rcc_Set_FlashLatency_AhbDividerAndUserMinimum_Applied( void )
{
    rcc_ConfigStruct_t config;

    Ut_Rcc_Get_HsiOnlyConfig( &config );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = 136000000u;
    config.AHB_Divider       = RCC_AHB_DIVIDER_2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_1WS, FLASH->ACR & FLASH_ACR_LATENCY );

    config.FlashLatency = RCC_FLASH_LATENCY_6_WS;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( FLASH_ACR_LATENCY_6WS, FLASH->ACR & FLASH_ACR_LATENCY );
}


/**
 * \brief   Rcc_Set_FlashLatency() fails if expected system clock is not available.
 *
 * \details PLL system clock without PLL source, without output R divider,
 *          invalid system clock source, NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, latency not written.
 */
void Ut_Rcc_Set_FlashLatency_PllNotConfigured_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_HSI;
    config.Pll_Config[ RCC_PLL_1 ].R_Divider  = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 2u;
    config.SystemClockSource                 = RCC_SYSTEM_CLOCK_SOURCE_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Flash prefetch activation and deactivation toggle PRFTEN only.
 *
 * \details Instruction cache bit preset.
 *
 * \par Expected results
 * - Activation sets PRFTEN, deactivation clears it, ICEN kept.
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
 * \brief   Rcc_Set_SysTickInterval() configures 1 ms interval from HCLK.
 *
 * \details System clock HSI 16 MHz.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SysTick reload 16000 - 1, SysTick enabled with interrupt.
 */
void Ut_Rcc_Set_SysTickInterval_1ms_WritesReload( void )
{
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SysTickInterval( 1u ) );

    TEST_ASSERT_EQUAL_UINT32( ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) - 1u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_SYSTICK_CTRL_ACTIVE, SysTick->CTRL );
}


/**
 * \brief   Rcc_Set_SysTickInterval() checks the range of the reload register.
 *
 * \details HSI 16 MHz: 0 ms, longest interval and interval above the range.
 *          Unknown system clock (reserved SWS 0).
 *
 * \par Expected results
 * - 0 ms and too long interval: RCC_REQUEST_ERROR.
 * - Longest interval: RCC_REQUEST_OK.
 * - Unknown system clock: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_SysTickInterval_RangeBoundaries_Checked( void )
{
    RCC->CFGR = RCC_CFGR_SWS_HSI;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_SysTickInterval( TEST_RCC_SYSTICK_MAX_MS_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( TEST_RCC_SYSTICK_MAX_MS_HSI + 1u ) );

    RCC->CFGR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 1u ) );
}


/**
 * \brief   Rcc_Get_SysTickInterval() reads interval from reload register.
 *
 * \details HSI 16 MHz, SysTick reload preset for 10 ms. NULL pointer, unknown
 *          system clock.
 *
 * \par Expected results
 * - RCC_REQUEST_OK and 10 ms; RCC_REQUEST_ERROR for NULL / unknown clock.
 */
void Ut_Rcc_Get_SysTickInterval_ReadsReload( void )
{
    rcc_Time_ms_t interval = 0u;

    RCC->CFGR     = RCC_CFGR_SWS_HSI;
    SysTick->LOAD = ( 10u * ( TEST_RCC_HSI_FREQ_HZ / TEST_RCC_MS_IN_SECOND ) ) - 1u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 10u, interval );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( NULL ) );

    RCC->CFGR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( &interval ) );
}

/* ================================== PLL =================================== */

/**
 * \brief   Rcc_Set_PllConfig() configures and locks PLL clocked by HSI.
 *
 * \details HW model emulates ready flags. M 4, N 85, P 17, Q 8, R 4.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLCFGR holds the configuration with all outputs enabled,
 *   PLL on and locked, HSI on.
 */
void Ut_Rcc_Set_PllConfig_Hsi_ConfiguresAndLocks( void )
{
    rcc_PllConfigStruct_t pllConfig = { RCC_PLL_SRC_HSI, 4u, 85u, 17u, 8u, 4u };
    rcc_FunctionState_t   state     = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( TEST_RCC_PLLCFGR( 4u, 85u, 17u, 3u, TEST_RCC_PLLQR_DIV4, RCC_PLLCFGR_PLLSRC_HSI ) |
                             TEST_RCC_PLL_OUTPUTS_EN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_PLLON, RCC->CR & ( RCC_CR_HSION | RCC_CR_PLLON ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Rcc_Set_PllConfig() with no source only deactivates PLL.
 *
 * \details PLL on (not locked), source NONE requested.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLON cleared, PLLCFGR not written.
 */
void Ut_Rcc_Set_PllConfig_SourceNone_DeactivatesOnly( void )
{
    rcc_PllConfigStruct_t pllConfig = { RCC_PLL_SRC_NONE, 4u, 85u, 2u, 2u, 2u };

    RCC->CR = RCC_CR_PLLON;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects M / N out of range.
 *
 * \details M 0, M 17, N 7, N 128.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLLCFGR not written, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_MnOutOfRange_ReturnsError( void )
{
    const rcc_PllConfigStruct_t invalidLut[] =
    {
        { RCC_PLL_SRC_HSI,  0u,  85u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI, 17u,  85u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI,  4u,   7u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI,  4u, 128u, 0u, 0u, 2u },
    };

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        rcc_PllConfigStruct_t pllConfig = invalidLut[ idx ];

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects input / VCO frequency out of range.
 *
 * \details HSI ready preset. Input 2 MHz (M 8), VCO 352 MHz (N 88), VCO 80 MHz
 *          (N 20). HSE PLL without configured HSE frequency.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_FrequencyOutOfRange_ReturnsError( void )
{
    const rcc_PllConfigStruct_t invalidLut[] =
    {
        { RCC_PLL_SRC_HSI, 8u, 85u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI, 4u, 88u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSI, 4u, 20u, 0u, 0u, 2u },
        { RCC_PLL_SRC_HSE, 4u, 85u, 0u, 0u, 2u },
    };

    RCC->CR = RCC_CR_HSIRDY;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        rcc_PllConfigStruct_t pllConfig = invalidLut[ idx ];

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects invalid output divider without PLL activation.
 *
 * \details HSI ready preset. P 1, P 32, Q 3, R 10.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, PLL not activated.
 */
void Ut_Rcc_Set_PllConfig_InvalidOutputDivider_ReturnsErrorWithoutActivation( void )
{
    const rcc_PllConfigStruct_t invalidLut[] =
    {
        { RCC_PLL_SRC_HSI, 4u, 85u,  1u, 0u,  2u },
        { RCC_PLL_SRC_HSI, 4u, 85u, 32u, 0u,  2u },
        { RCC_PLL_SRC_HSI, 4u, 85u,  0u, 3u,  2u },
        { RCC_PLL_SRC_HSI, 4u, 85u,  0u, 0u, 10u },
    };

    RCC->CR = RCC_CR_HSIRDY;

    for( uint32_t idx = 0u; TEST_RCC_ARRAY_CNT( invalidLut ) > idx; idx++ )
    {
        rcc_PllConfigStruct_t pllConfig = invalidLut[ idx ];

        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
        TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_PLLON );
    }
}


/**
 * \brief   Unused PLL outputs (divider 0) are disabled.
 *
 * \details All output enable bits preset, HW model emulates ready flags. PLL
 *          configured with P and Q 0 (R used), then with Q and R 0 (P used).
 *
 * \par Expected results
 * - P, Q unused: RCC_REQUEST_OK, PLLPEN and PLLQEN cleared, PLLREN set.
 * - Q, R unused: RCC_REQUEST_OK, PLLQEN and PLLREN cleared, PLLPEN set.
 */
void Ut_Rcc_Set_PllConfig_UnusedOutputs_Disabled( void )
{
    rcc_PllConfigStruct_t pllConfig = { RCC_PLL_SRC_HSI, 4u, 85u, 0u, 0u, 2u };

    RCC->PLLCFGR = TEST_RCC_PLL_OUTPUTS_EN;
    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLREN, RCC->PLLCFGR & TEST_RCC_PLL_OUTPUTS_EN );

    pllConfig.P_Divider = 6u;
    pllConfig.R_Divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLPEN, RCC->PLLCFGR & TEST_RCC_PLL_OUTPUTS_EN );
}


/**
 * \brief   PLL output clocks are calculated from registers.
 *
 * \details PLLCFGR preset: HSI, M 4, N 85, PDIV 10, Q 6, R 4, outputs enabled.
 *          Then PDIV 0 with PLLP 0 / 1 (dividers 7 / 17).
 *
 * \par Expected results
 * - VCO 340 MHz, P 34 MHz, Q 56666666 Hz, R 85 MHz.
 * - PDIV 0: P = VCO / 7, PLLP set: P = VCO / 17.
 */
void Ut_Rcc_Get_PllClk_Outputs_CalculatedFromRegisters( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 4u, 85u, 10u, TEST_RCC_PLLQR_DIV6, TEST_RCC_PLLQR_DIV4, RCC_PLLCFGR_PLLSRC_HSI ) | TEST_RCC_PLL_OUTPUTS_EN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 10u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 6u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 4u, freq );

    RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLPDIV;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 7u, freq );

    RCC->PLLCFGR |= RCC_PLLCFGR_PLLP;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_DEFAULT_VCO_HZ / 17u, freq );
}


/**
 * \brief   PLL output clock getters reject disabled output and invalid arguments.
 *
 * \details Outputs disabled (enable bits 0), invalid PLL ID, NULL pointer, no
 *          PLL source selected.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 */
void Ut_Rcc_Get_PllClk_DisabledOutputOrInvalidArgs_ReturnsError( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->PLLCFGR = TEST_RCC_PLLCFGR( 4u, 85u, 10u, TEST_RCC_PLLQR_DIV6, TEST_RCC_PLLQR_DIV4, RCC_PLLCFGR_PLLSRC_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutR( RCC_PLL_1, &freq ) );

    RCC->PLLCFGR |= TEST_RCC_PLL_OUTPUTS_EN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutQ( RCC_PLL_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutR( RCC_PLL_CNT, &freq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllClk_OutP( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllInternalClk( RCC_PLL_1, NULL ) );

    RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
}


/**
 * \brief   PLL activation waits for ready flag and state follows it.
 *
 * \details HW model emulates PLL lock. PLL activated and deactivated.
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_OK, state ACTIVE.
 * - Deactivation: RCC_REQUEST_OK, state INACTIVE.
 */
void Ut_Rcc_Set_PllActive_ReadyFlag_StateFollows( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllActive( RCC_PLL_1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllState( RCC_PLL_1, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   PLL activation fails if PLL does not lock.
 *
 * \details No HW model, ready flag stays 0. Then ready flag stuck (deactivation).
 *
 * \par Expected results
 * - Activation: RCC_REQUEST_ERROR.
 * - Deactivation with ready flag set: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PllActive_NotLocked_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllActive( RCC_PLL_1 ) );

    RCC->CR = RCC_CR_PLLRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllInactive( RCC_PLL_1 ) );
}


/**
 * \brief   PLL source is written while PLL is inactive.
 *
 * \details HSE selected, then HSI (HSI ready preset), source read back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLSRC = HSE / HSI, HSION set by HSI selection.
 */
void Ut_Rcc_Set_PllsSource_PllInactive_WritesPllsrc( void )
{
    rcc_PllClkSrc_t source = RCC_PLL_SRC_NONE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSE, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSE, source );

    RCC->CR = RCC_CR_HSIRDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSI, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION, RCC->CR & RCC_CR_HSION );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_HSI, source );
}


/**
 * \brief   PLL source can not be changed while PLL is active.
 *
 * \details PLL on with HSE source. HSI requested, HSE requested again, NONE
 *          requested. No source selected is read as NONE.
 *
 * \par Expected results
 * - HSI: RCC_REQUEST_ERROR, PLLSRC unchanged. HSE (same): RCC_REQUEST_OK.
 * - NONE: RCC_REQUEST_ERROR. PLLSRC 0: source NONE.
 */
void Ut_Rcc_Set_PllsSource_PllActive_ReturnsError( void )
{
    rcc_PllClkSrc_t source = RCC_PLL_SRC_HSI;

    RCC->CR      = RCC_CR_HSIRDY | RCC_CR_PLLON;
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_PLLCFGR_PLLSRC_HSE, RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_HSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_1, RCC_PLL_SRC_NONE ) );

    RCC->PLLCFGR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllsSource( RCC_PLL_1, &source ) );
    TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE, source );
}

/* ============================== OSCILLATORS =============================== */

/**
 * \brief   Oscillator activation sets enable bit and waits for ready flag.
 *
 * \details HW model emulates ready flags of HSI, HSI48, LSI, LSE.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSION, HSI48ON, LSION, LSEON set, states ACTIVE.
 * - LSE in backup domain: write protection released (DBP).
 */
void Ut_Rcc_Set_OscActive_ReadyFlags_SetsEnableBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    Ut_Rcc_Start_HwModel();

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscActive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
    }

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION,      RCC->CR    & RCC_CR_HSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_CRRCR_HSI48ON, RCC->CRRCR & RCC_CRRCR_HSI48ON );
    TEST_ASSERT_EQUAL_HEX32( RCC_CSR_LSION,     RCC->CSR   & RCC_CSR_LSION );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSEON,    RCC->BDCR  & RCC_BDCR_LSEON );
    TEST_ASSERT_EQUAL_HEX32( PWR_CR1_DBP,       PWR->CR1   & PWR_CR1_DBP );
}


/**
 * \brief   Oscillator activation fails if the oscillator does not get ready.
 *
 * \details No HW model, ready flags stay 0.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for every oscillator, state INACTIVE.
 */
void Ut_Rcc_Set_OscActive_NotReady_ReturnsError( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Oscillator deactivation clears enable bit and waits until not ready.
 *
 * \details Oscillators on and ready, HW model emulates ready flags.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, enable bits cleared, states INACTIVE.
 */
void Ut_Rcc_Set_OscInactive_AllOscillators_ClearsEnableBits( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR    = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CRRCR = RCC_CRRCR_HSI48ON | RCC_CRRCR_HSI48RDY;
    RCC->CSR   = RCC_CSR_LSION | RCC_CSR_LSIRDY;
    RCC->BDCR  = RCC_BDCR_LSEON | RCC_BDCR_LSERDY;

    Ut_Rcc_Start_HwModel();

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscInactive( oscId ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR    & RCC_CR_HSION );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CRRCR & RCC_CRRCR_HSI48ON );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR   & RCC_CSR_LSION );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR  & RCC_BDCR_LSEON );
}


/**
 * \brief   Oscillator deactivation fails if the ready flag stays set.
 *
 * \details No HW model, ready flags stuck at 1.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for every oscillator.
 */
void Ut_Rcc_Set_OscInactive_ReadyFlagStuck_ReturnsError( void )
{
    RCC->CR    = RCC_CR_HSIRDY;
    RCC->CRRCR = RCC_CRRCR_HSI48RDY;
    RCC->CSR   = RCC_CSR_LSIRDY;
    RCC->BDCR  = RCC_BDCR_LSERDY;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( oscId ) );
    }
}


/**
 * \brief   Oscillator state requires enable bit and ready flag.
 *
 * \details Ready flags without enable bits, then enable bits without ready flags.
 *
 * \par Expected results
 * - State INACTIVE in both cases.
 */
void Ut_Rcc_Get_OscState_RequiresOnAndReady( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR    = RCC_CR_HSIRDY;
    RCC->CRRCR = RCC_CRRCR_HSI48RDY;
    RCC->CSR   = RCC_CSR_LSIRDY;
    RCC->BDCR  = RCC_BDCR_LSERDY;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }

    RCC->CR    = RCC_CR_HSION;
    RCC->CRRCR = RCC_CRRCR_HSI48ON;
    RCC->CSR   = RCC_CSR_LSION;
    RCC->BDCR  = RCC_BDCR_LSEON;

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscState( oscId, &state ) );
        TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
    }
}


/**
 * \brief   Oscillator functions reject invalid arguments.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR for RCC_OSC_CNT and NULL pointers.
 */
void Ut_Rcc_OscFunctions_InvalidArgs_ReturnError( void )
{
    rcc_FunctionState_t state  = RCC_FUNCTION_INACTIVE;
    rcc_OscDiv_t        oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscActive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscInactive( RCC_OSC_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( RCC_OSC_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( RCC_OSC_CNT, &oscDiv ) );

    for( rcc_OscId_t oscId = RCC_OSC_HSI; RCC_OSC_CNT > oscId; oscId++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscState( oscId, NULL ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_OscDiv( oscId, NULL ) );
    }
}


/**
 * \brief   Oscillators have no output divider - only divider 1 is accepted.
 *
 * \par Expected results
 * - Divider 1: RCC_REQUEST_OK, divider 2 / invalid oscillator: RCC_REQUEST_ERROR.
 * - Rcc_Get_OscDiv() returns 1.
 */
void Ut_Rcc_Set_OscDiv_OnlyDividerOneAccepted( void )
{
    rcc_OscDiv_t oscDiv = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_OscDiv( RCC_OSC_HSI, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_HSI, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_CNT, 1u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_HSI48, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, oscDiv );
}

/* ============================== RTC CLOCK ================================= */

/**
 * \brief   RTC clock source LSE is written to RTCSEL.
 *
 * \details RTCSEL 0 (no clock), LSE requested, same source requested again.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RTCSEL = LSE, read back as LSE.
 */
void Ut_Rcc_Set_RtcClkSource_Lse_WritesRtcsel( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_0, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_LSE, source );
}


/**
 * \brief   RTC clock source can not be changed after selection.
 *
 * \details RTCSEL preset to LSI, HSE / 32 requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RTCSEL stays LSI.
 */
void Ut_Rcc_Set_RtcClkSource_OtherSelected_ReturnsError( void )
{
    RCC->BDCR = RCC_BDCR_RTCSEL_1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_HSE_DIV ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_RTCSEL_1, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   RTC clock source read - no clock and HSE / 32.
 *
 * \details RTCSEL 0, then RTCSEL = HSE / 32. Invalid arguments.
 *
 * \par Expected results
 * - No clock: RCC_REQUEST_ERROR. HSE / 32: RCC_RTC_CLK_SOURCE_HSE_DIV.
 * - Invalid source / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_RtcClkSource_NoClockAndHse( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( &source ) );

    RCC->BDCR = RCC_BDCR_RTCSEL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_HSE_DIV, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_RtcClkSource( NULL ) );
}

/* ============================ CLOCK OUTPUTS =============================== */

/**
 * \brief   MCO source PLL R configures MCOSEL and pin PA8.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MCOSEL = PLL, Gpio_Init() called for PA8 (AF0).
 */
void Ut_Rcc_Set_ClkOutSource_McoPllr_ConfiguresPa8( void )
{
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_McoPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO1_PLLR ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1SOURCE_PLLCLK, RCC->CFGR & RCC_CFGR_MCOSEL );
}


/**
 * \brief   LSCO source LSI enables LSCO and configures pin PA2 analog.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, LSCOEN set, LSCOSEL = LSI, Gpio_Init() called for PA2 (analog).
 */
void Ut_Rcc_Set_ClkOutSource_LscoLsi_ConfiguresPa2Analog( void )
{
    Gpio_Init_ExpectAndReturn( (gpio_Config_t *)&utRcc_LscoPin, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_LSCO_LSI ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_BDCR_LSCOEN, RCC->BDCR & ( RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL ) );
}


/**
 * \brief   Clock output source NONE or source of other output changes nothing.
 *
 * \details NONE for both outputs, LSCO source for MCO, MCO source for LSCO,
 *          invalid output ID.
 *
 * \par Expected results
 * - NONE: RCC_REQUEST_OK, others: RCC_REQUEST_ERROR.
 * - CFGR / BDCR not written, no pin configured.
 */
void Ut_Rcc_Set_ClkOutSource_NoneOrInvalid_NoChange( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_LSCO_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_MCO1_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR | RCC->BDCR );
}


/**
 * \brief   Clock output sources are read from CFGR and BDCR.
 *
 * \details MCOSEL HSI48, MCOSEL 0. LSCOSEL set without LSCOEN, then with
 *          LSCOEN. Invalid arguments.
 *
 * \par Expected results
 * - MCO: HSI48, then NONE.
 * - LSCO: NONE while disabled, LSE when enabled.
 * - Invalid output / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkOutSource_ReadsCfgrAndBdcr( void )
{
    rcc_ClkOut_Source_t source = RCC_CLK_SOURCE_CNT;

    RCC->CFGR = LL_RCC_MCO1SOURCE_HSI48;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_MCO1_HSI48, source );

    RCC->CFGR = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    RCC->BDCR = RCC_BDCR_LSCOSEL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, source );

    RCC->BDCR = RCC_BDCR_LSCOSEL | RCC_BDCR_LSCOEN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_LSCO_LSE, source );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_CNT,  &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, NULL ) );
}


/**
 * \brief   MCO dividers 1, 2, 4, 8, 16 are written to MCOPRE and read back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, MCOPRE = log2 of the divider, divider read back.
 */
void Ut_Rcc_Set_ClkOutDivider_AllDividers_ReadBack( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    for( uint32_t preVal = 0u; 4u >= preVal; preVal++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 1u << preVal ) );
        TEST_ASSERT_EQUAL_HEX32( preVal << RCC_CFGR_MCOPRE_Pos, RCC->CFGR & RCC_CFGR_MCOPRE );

        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
        TEST_ASSERT_EQUAL_UINT32( 1u << preVal, divider );
    }
}


/**
 * \brief   Invalid clock output dividers are rejected, LSCO has no divider.
 *
 * \details MCO 0, 3, 32. LSCO 2 and 1. Invalid output, NULL pointer, reserved
 *          MCOPRE value.
 *
 * \par Expected results
 * - MCO invalid dividers, LSCO 2, invalid output: RCC_REQUEST_ERROR, CFGR not written.
 * - LSCO 1: RCC_REQUEST_OK, LSCO divider read as 1.
 * - Reserved MCOPRE (5) and NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_ClkOutDivider_InvalidOrLsco_Rejected( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1,  0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1,  3u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 32u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO,  2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_CNT,   1u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_LSCO, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, divider );

    RCC->CFGR = 5u << RCC_CFGR_MCOPRE_Pos;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_CNT,  &divider ) );
}

/* ============================ RESET SOURCES =============================== */

/**
 * \brief   Every reset source flag of CSR is decoded separately.
 *
 * \details Each flag preset alone, all sources read.
 *
 * \par Expected results
 * - Only the source of the preset flag is ACTIVE.
 * - Invalid source / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ResetSource_EveryFlag_DecodedSeparately( void )
{
    const uint32_t flagLut[ RCC_RESET_SRC_CNT ] =
    {
        RCC_CSR_PINRSTF, RCC_CSR_BORRSTF, RCC_CSR_SFTRSTF, RCC_CSR_IWDGRSTF,
        RCC_CSR_WWDGRSTF, RCC_CSR_LPWRRSTF, RCC_CSR_OBLRSTF
    };
    rcc_FlagState_t flag = RCC_FLAG_INACTIVE;

    for( uint32_t flagIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > flagIdx; flagIdx++ )
    {
        RCC->CSR = flagLut[ flagIdx ];

        for( uint32_t srcIdx = 0u; (uint32_t)RCC_RESET_SRC_CNT > srcIdx; srcIdx++ )
        {
            TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetSource( (rcc_ResetSrc_t)srcIdx, &flag ) );
            TEST_ASSERT_EQUAL( ( srcIdx == flagIdx ) ? RCC_FLAG_ACTIVE : RCC_FLAG_INACTIVE, flag );
        }
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_CNT, &flag ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ResetSource( RCC_RESET_SRC_PIN, NULL ) );
}


/**
 * \brief   Reset source flags are cleared by RMVF, RMVF is released.
 *
 * \details All flags set, HW model clears flags while RMVF is set.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, all flags cleared, RMVF cleared.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsCleared_ReleasesRmvf( void )
{
    RCC->CSR = TEST_RCC_CSR_RESET_FLAGS;

    Ut_Rcc_Start_HwModel();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR & ( TEST_RCC_CSR_RESET_FLAGS | RCC_CSR_RMVF ) );
}


/**
 * \brief   Reset source clear fails when flags are kept, RMVF is still released.
 *
 * \details All flags set, no HW model (flags stay set).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RMVF cleared.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsKept_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->CSR = TEST_RCC_CSR_RESET_FLAGS;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CSR & RCC_CSR_RMVF );
}

/* ============================ COMPONENTS ================================== */

/**
 * \brief   HSE state requires HSEON and HSERDY.
 *
 * \details HSERDY only, HSEON only, both. NULL pointer.
 *
 * \par Expected results
 * - INACTIVE, INACTIVE, ACTIVE; NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_HseState_RequiresOnAndReady( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    RCC->CR = RCC_CR_HSERDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSEON;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseState( &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseState( NULL ) );
}


/**
 * \brief   HSE activation with type NONE only stops HSE, invalid type is rejected.
 *
 * \details HSE on with bypass preset. Type NONE, then invalid type. HSE
 *          frequency 0 and NULL pointers.
 *
 * \par Expected results
 * - NONE: RCC_REQUEST_OK, HSEON cleared (bypass kept).
 * - Invalid type: RCC_REQUEST_ERROR, HSEON not set.
 * - Frequency 0 / NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_Set_HseActive_NoneOrInvalidType( void )
{
    rcc_FreqHz_t freq = 0u;

    RCC->CR = RCC_CR_HSEON | RCC_CR_HSEBYP;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_NONE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSEBYP, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( (rcc_HseType_t)5u ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSEON );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseClk( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_ClkSrc_Get_HseClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, freq );
}


/**
 * \brief   HSE activation fails if running HSE can not be stopped.
 *
 * \details HSE on and ready, no HW model - ready flag stays set after HSEON is
 *          cleared. Type NONE (stop only) and crystal are requested.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases, HSE is not started again (HSEON 0).
 */
void Ut_Rcc_ClkSrc_Set_HseActive_ReadyStuck_ReturnsError( void )
{
    RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseActive( RCC_HSE_TYPE_CRYSTAL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR & RCC_CR_HSEON );
}


/**
 * \brief   HSE divider of RTC is fixed to 32.
 *
 * \details Divider 32 / 31 requested, divider read, HSE RTC clock with 24 MHz
 *          HSE, NULL pointers.
 *
 * \par Expected results
 * - 32: RCC_REQUEST_OK, 31: RCC_REQUEST_ERROR, read divider 32.
 * - HSE RTC clock 750 kHz, NULL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_HseRtc_FixedDivider32( void )
{
    rcc_Rtc_HseDiv_t hseDiv = 0u;
    rcc_FreqHz_t     freq   = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_ClkSrc_Set_HseRtcDiv( 32u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Set_HseRtcDiv( 31u ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseRtcDiv( &hseDiv ) );
    TEST_ASSERT_EQUAL_UINT16( 32u, hseDiv );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseClk( TEST_RCC_HSE_FREQ_HZ ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Set_HseRtcActive() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HseRtcClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSE_FREQ_HZ / 32u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseRtcDiv( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HseRtcClk( NULL ) );
}


/**
 * \brief   Oscillator frequency getters of the clock source component.
 *
 * \par Expected results
 * - HSI 16 MHz, HSI48 48 MHz, LSI 32 kHz, LSE 32.768 kHz.
 * - NULL pointers: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_ClkSrc_OscillatorFrequencies_Returned( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_HsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_RCC_HSI_FREQ_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_Hsi48Clk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( 48000000u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_LsiClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkSrc_Get_LseClk( &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_HsiClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_Hsi48Clk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LsiClk( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkSrc_Get_LseClk( NULL ) );
}


/**
 * \brief   Initialization / task functions of internal components do not touch the hardware,
 *          configuration tables are consistent.
 *
 * \details Rcc_ClkBus_Init / Deinit / Task, Rcc_ClkMux_Init / Deinit,
 *          Rcc_Pll_Init / Task, Rcc_ClkOut_Init / Deinit / Task,
 *          Rcc_ClkSrc_Init / Deinit / Task, Rcc_Reg_Init / Deinit / Task.
 *
 * \par Expected results
 * - Init functions return RCC_REQUEST_OK (configuration tables indexed correctly).
 * - RCC registers stay 0.
 */
void Ut_Rcc_Components_InitTask_NoRegisterAccess( void )
{
    extern void               Rcc_ClkMux_Task  ( void );
    extern rcc_RequestState_t Rcc_ClkOut_Init  ( void );
    extern void               Rcc_ClkOut_Deinit( void );
    extern void               Rcc_ClkOut_Task  ( void );
    extern void               Rcc_ClkSrc_Init  ( void );
    extern void               Rcc_ClkSrc_Deinit( void );
    extern void               Rcc_ClkSrc_Task  ( void );
    extern void               Rcc_Reg_Init     ( void );
    extern void               Rcc_Reg_Deinit   ( void );
    extern void               Rcc_Reg_Task     ( void );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkBus_Init() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkMux_Init() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Pll_Init() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_ClkOut_Init() );

    Rcc_ClkBus_Task();
    Rcc_ClkBus_Deinit();
    Rcc_ClkMux_Task();
    Rcc_ClkMux_Deinit();
    Rcc_Pll_Task();
    Rcc_ClkOut_Deinit();
    Rcc_ClkOut_Task();
    Rcc_ClkSrc_Init();
    Rcc_ClkSrc_Deinit();
    Rcc_ClkSrc_Task();
    Rcc_Reg_Init();
    Rcc_Reg_Deinit();
    Rcc_Reg_Task();

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR | RCC->CFGR | RCC->PLLCFGR | RCC->BDCR | RCC->CSR | RCC->CCIPR );
}


/**
 * \brief   Rcc_Pll_Deinit() disables the PLL.
 *
 * \details PLL on and not locked (disable succeeds), then ready flag kept (PLL
 *          can not be stopped).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, PLLON cleared.
 * - Locked PLL: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Pll_Deinit_PllDisabled( void )
{
    RCC->CR = RCC_CR_PLLON;

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
 * \brief   Clock multiplexer component reports error for unknown selection.
 *
 * \details I2C1SEL preset to reserved value 11.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, selection not found.
 */
void Ut_Rcc_ClkMux_ReservedSelection_ReturnsError( void )
{
    rcc_ClkMuxId_t clkMuxId = RCC_CLK_MUX_LIST_CNT;

    RCC->CCIPR = RCC_CCIPR_I2C1SEL;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkMux_Get_ClkSrc( RCC_CLK_MUX_I2C1_PCLK1, &clkMuxId ) );
    TEST_ASSERT_EQUAL( RCC_CLK_MUX_LIST_CNT, clkMuxId );
}


/**
 * \brief   Bus clock component rejects invalid arguments.
 *
 * \details NULL pointers of getters, invalid system clock source, reserved SWS.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, CFGR unchanged.
 */
void Ut_Rcc_ClkBus_InvalidArgs_ReturnError( void )
{
    rcc_SystemClkSrc_t source = RCC_SYSTEM_CLOCK_SOURCE_CNT;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_SysClkSource( NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Get_SysClkSource( &source ) );
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
 * \brief   System clock switch fails if the source does not get selected.
 *
 * \details No HW model, SWS stays 0 (reserved).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, SW = HSE written.
 */
void Ut_Rcc_ClkBus_Set_SysClkSource_NotSwitched_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_ClkBus_Set_SysClkSource( RCC_SYSTEM_CLOCK_SOURCE_HSE ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_CFGR_SW_HSE, RCC->CFGR & RCC_CFGR_SW );
}


/**
 * \brief   System clock of HSE and HSI source is reported by bus clock component.
 *
 * \details SWS = HSE (HSE frequency 24 MHz), then SWS = HSI.
 *
 * \par Expected results
 * - HSE: 24 MHz and source HSE, HSI: 16 MHz.
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
 * \details Invalid PLL ID, NULL pointers, invalid PLL source, invalid output
 *          dividers and RTC clock source.
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Active( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Inactive( RCC_PLL_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_State( RCC_PLL_CNT, &state ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_State( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Source( RCC_PLL_CNT, RCC_PLL_SRC_HSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Source( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_Source( RCC_PLL_CNT, &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutP( RCC_PLL_CNT, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutQ( RCC_PLL_CNT, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutR( RCC_PLL_CNT, 2u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutQ( RCC_PLL_1, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_OutR( RCC_PLL_1, 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_CNT, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_Config( RCC_PLL_1, NULL ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_CNT ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Pll_Get_RtcClkSource( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->PLLCFGR );
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
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CR | RCC->CFGR | RCC->BDCR | RCC->CSR | PWR->CR1 );
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
 * - RCC_CRRCR HSI48RDY follows HSI48ON
 * - RCC_CFGR SWS follows SW (AHB prescaler recorded on the switch to PLL)
 * - RCC_CSR LSIRDY follows LSION, reset flags are cleared while RMVF is set
 * - RCC_BDCR LSERDY follows LSEON
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

    if( ( 0u    != ( cr & RCC_CR_HSEON ) ) &&
        ( false == utRcc_HseFails        )    )
    {
        crRdy |= RCC_CR_HSERDY;
    }

    if( 0u != ( cr & RCC_CR_PLLON ) )
    {
        crRdy |= RCC_CR_PLLRDY;
    }

    Ut_Rcc_HwModel_Update( &RCC->CR, TEST_RCC_CR_RDY_MASK, crRdy );

    Ut_Rcc_HwModel_Update( &RCC->CRRCR, RCC_CRRCR_HSI48RDY, ( 0u != ( RCC->CRRCR & RCC_CRRCR_HSI48ON ) ) ? RCC_CRRCR_HSI48RDY : 0u );

    const uint32_t cfgr = RCC->CFGR;
    const uint32_t sws  = ( cfgr & RCC_CFGR_SW ) << ( RCC_CFGR_SWS_Pos - RCC_CFGR_SW_Pos );

    if( ( RCC_CFGR_SWS_PLL == sws                     ) &&
        ( RCC_CFGR_SWS_PLL != ( cfgr & RCC_CFGR_SWS ) )    )
    {
        utRcc_HpreAtPllSwitch = cfgr & RCC_CFGR_HPRE;
    }

    Ut_Rcc_HwModel_Update( &RCC->CFGR, RCC_CFGR_SWS, sws );

    const uint32_t csr     = RCC->CSR;
    const uint32_t csrMask = ( 0u != ( csr & RCC_CSR_RMVF ) ) ? ( RCC_CSR_LSIRDY | TEST_RCC_CSR_RESET_FLAGS ) : RCC_CSR_LSIRDY;

    Ut_Rcc_HwModel_Update( &RCC->CSR, csrMask, ( 0u != ( csr & RCC_CSR_LSION ) ) ? RCC_CSR_LSIRDY : 0u );

    Ut_Rcc_HwModel_Update( &RCC->BDCR, RCC_BDCR_LSERDY, ( 0u != ( RCC->BDCR & RCC_BDCR_LSEON ) ) ? RCC_BDCR_LSERDY : 0u );
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
    config->Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;
}


/**
 * \brief Configuration of 150 MHz from 24 MHz HSE crystal PLL (range 1 normal mode).
 *
 * HSE crystal 24 MHz, PLL M 6, N 75, P 10, Q 6, R 2, AHB / 1, APB1 / 2, APB2 / 1.
 *
 * \param config [out]: Clock configuration
 */
static void Ut_Rcc_Get_HsePllConfig( rcc_ConfigStruct_t * const config )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( config ) );

    config->HSE_ClockType    = RCC_HSE_TYPE_CRYSTAL;
    config->HSE_Frequency_Hz = TEST_RCC_HSE_FREQ_HZ;
    config->APB1_Divider     = RCC_APB1_DIVIDER_2;
    config->VoltageScaling   = RCC_PWR_VOLTAGE_SCALE_1;

    config->Pll_Config[ RCC_PLL_1 ].Pll_Source   = RCC_PLL_SRC_HSE;
    config->Pll_Config[ RCC_PLL_1 ].M_Divider    = 6u;
    config->Pll_Config[ RCC_PLL_1 ].N_Multiplier = 75u;
    config->Pll_Config[ RCC_PLL_1 ].P_Divider    = 10u;
    config->Pll_Config[ RCC_PLL_1 ].Q_Divider    = 6u;
    config->Pll_Config[ RCC_PLL_1 ].R_Divider    = 2u;
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
