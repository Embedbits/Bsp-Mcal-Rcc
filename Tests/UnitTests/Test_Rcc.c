/**
 * \author Mr.Nobody
 * \file Test_Rcc.c
 * \ingroup Rcc
 * \brief Unit tests of Reset and Clock Control (RCC) module (STM32H7 incl. STM32H7R / H7S).
 *
 * All Rcc sources are compiled unchanged with real LL drivers. RCC, PWR, FLASH,
 * SYSCFG, FMC, DBGMCU and SysTick registers are emulated by RegMem, GPIO module
 * (MCO pins) is mocked by CMock.
 *
 * Oscillator / PLL enable -> ready flags, system clock switch (SW -> SWS),
 * voltage scaling (VOS -> ACTVOS, ACTVOSRDY), SMPS external supply ready flag
 * and reset flags removal are emulated by HW model running in background thread
 * (Ut_Rcc_HwModel). Tests without the model check timeout (error) branches.
 *
 * Every test starts from the voltage scale of the reset (VOS3 requested and
 * active, VOS low on STM32H7R / H7S), other registers are cleared.
 *
 * STM32H7R / H7S differences are handled by the UT_ macros below (registers of
 * the line, other kernel clock multiplexers, peripherals of the line) and by
 * line specific parts of the tests.
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
#include "Stm32_system.h"                   /* FLASH / SYSCFG registers definition */
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
static void                 Ut_Rcc_Set_ActiveScale  ( rcc_PwrVoltageScale_t voltageScale );
static void                 Ut_Rcc_Assert_AxiClk    ( rcc_FreqHz_t expectedHz );
static void                 Ut_Rcc_Set_PllConfig    ( rcc_PllConfigStruct_t * const pllConfig, rcc_PllClkSrc_t source,
                                                      uint32_t mDiv, uint32_t nMult, uint32_t pDiv );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** HSI oscillator frequency (HSI_VALUE, without HSIDIV divider) */
#define UT_RCC_HSI_HZ                       ( 64000000u )

/** HSE frequency of default configuration */
#define UT_RCC_HSE_DEFAULT_HZ               ( 8000000u )

/** Count of milliseconds in one second */
#define UT_RCC_MS_IN_SECOND                 ( 1000u )

/** Interval too long for 24-bit SysTick reload at 64 MHz */
#define UT_RCC_SYSTICK_TOO_LONG_MS          ( 1000u )

/** Default PLL input divider */
#define UT_RCC_DEFAULT_PLL_M                ( 4u )

/** Default PLL multiplier */
#define UT_RCC_DEFAULT_PLL_N                ( 25u )

/** Default PLL output divider */
#define UT_RCC_DEFAULT_PLL_DIV              ( 2u )

/** PLL VCO frequency of default PLL configuration from HSI: 64 MHz / 4 * 25 */
#define UT_RCC_PLL_VCO_DEFAULT_HZ           ( 400000000u )

/** PLL output frequency of default PLL configuration from HSI (VCO / 2) */
#define UT_RCC_PLL_OUT_DEFAULT_HZ           ( 200000000u )

/** Frequency of PLL1 P output of HSE 8 MHz configuration: 8 MHz / 1 * 50 / 2 */
#define UT_RCC_PLL_HSE_SYSCLK_HZ            ( 200000000u )

/** HSE frequency used for flash latency calculation tests */
#define UT_RCC_HSE_50MHZ_HZ                 ( 50000000u )

/** RTC prescaler of HSE used by RTC HSE clock test */
#define UT_RCC_RTC_HSE_DIV                  ( 8u )

/** Value of DBGMCU IDCODE of revision Y device (REV_ID 0x1003, DEV_ID 0x450) */
#define UT_RCC_IDCODE_REV_Y                 ( 0x10036450u )

/** Value of DBGMCU IDCODE of revision V device (REV_ID 0x2003, DEV_ID 0x450) */
#define UT_RCC_IDCODE_REV_V                 ( 0x20036450u )

/** Address of AXI interconnect register AXI_TARG7_FN_MOD */
#define UT_RCC_AXI_TARG7_FN_MOD             ( *(volatile uint32_t *)0x51008108u )

/** FMC bank 1 control register value with the bank disabled */
#define UT_RCC_FMC_BCR1_DISABLED            ( 0x000030D2u )

/* PLLxRGE / PLLxVCOSEL field values (RM encoding, LL values are shifted on STM32H7R / H7S) */
#define UT_RCC_PLLRGE_1_2                   ( 0u )
#define UT_RCC_PLLRGE_8_16                  ( 3u )
#define UT_RCC_PLLVCOSEL_WIDE               ( 0u )
#define UT_RCC_PLLVCOSEL_MEDIUM             ( 1u )

/* Voltage scaling register (D3CR on 3-domain devices, SRDCR on STM32H7A3 / H7B0 / H7B3, CSR4 on STM32H7R / H7S) */
#if defined(PWR_CSR4_VOS)
#define UT_PWR_VOS_REG                      ( PWR->CSR4 )
#define UT_PWR_VOS_MSK                      ( PWR_CSR4_VOS )
#elif defined(PWR_SRDCR_VOS)
#define UT_PWR_VOS_REG                      ( PWR->SRDCR )
#define UT_PWR_VOS_MSK                      ( PWR_SRDCR_VOS )
#else
#define UT_PWR_VOS_REG                      ( PWR->D3CR )
#define UT_PWR_VOS_MSK                      ( PWR_D3CR_VOS )
#endif

/* Active voltage scale and voltage level ready flag (PWR_CSR1, PWR_SR1 on STM32H7R / H7S) */
#if defined(STM32H7RS)
#define UT_PWR_ACTVOS_REG                   ( PWR->SR1 )
#define UT_PWR_ACTVOS_MSK                   ( PWR_SR1_ACTVOS )
#define UT_PWR_ACTVOSRDY                    ( PWR_SR1_ACTVOSRDY )
#else
#define UT_PWR_ACTVOS_REG                   ( PWR->CSR1 )
#define UT_PWR_ACTVOS_MSK                   ( PWR_CSR1_ACTVOS )
#define UT_PWR_ACTVOSRDY                    ( PWR_CSR1_ACTVOSRDY )
#endif

/* Voltage scale of the reset (VOS3, VOS low on STM32H7R / H7S) and higher scale used by the tests (VOS1, VOS high) */
#if defined(STM32H7RS)
#define UT_PWR_VOS_RESET                    ( LL_PWR_REGU_VOLTAGE_SCALE1 )
#define UT_RCC_SCALE_RESET                  ( RCC_PWR_VOLTAGE_SCALE_1 )
#define UT_PWR_VOS_HIGHER                   ( LL_PWR_REGU_VOLTAGE_SCALE0 )
#define UT_RCC_SCALE_HIGHER                 ( RCC_PWR_VOLTAGE_SCALE_0 )
#else
#define UT_PWR_VOS_RESET                    ( LL_PWR_REGU_VOLTAGE_SCALE3 )
#define UT_RCC_SCALE_RESET                  ( RCC_PWR_VOLTAGE_SCALE_3 )
#define UT_PWR_VOS_HIGHER                   ( LL_PWR_REGU_VOLTAGE_SCALE1 )
#define UT_RCC_SCALE_HIGHER                 ( RCC_PWR_VOLTAGE_SCALE_1 )
#endif

/* Kernel clock selection registers (D1/D2/D3 names, CD/SRD names on STM32H7A3 / H7B0 / H7B3,
 * CCIPR1 - CCIPR4 on STM32H7R / H7S) */
#if defined(STM32H7RS)
#define UT_RCC_CCIPR1                       ( RCC->CCIPR1 )
#define UT_RCC_CCIPR2                       ( RCC->CCIPR2 )
#define UT_RCC_CCIPR3                       ( RCC->CCIPR3 )
#define UT_RCC_CCIPR4                       ( RCC->CCIPR4 )
#elif defined(RCC_CDCCIPR_CKPERSEL)
#define UT_RCC_CCIPR1                       ( RCC->CDCCIPR  )
#define UT_RCC_CCIPR2                       ( RCC->CDCCIP1R )
#define UT_RCC_CCIPR3                       ( RCC->CDCCIP2R )
#define UT_RCC_CCIPR4                       ( RCC->SRDCCIPR )
#else
#define UT_RCC_CCIPR1                       ( RCC->D1CCIPR  )
#define UT_RCC_CCIPR2                       ( RCC->D2CCIP1R )
#define UT_RCC_CCIPR3                       ( RCC->D2CCIP2R )
#define UT_RCC_CCIPR4                       ( RCC->D3CCIPR  )
#endif

/* Supply configuration register (PWR_CR3, PWR_CSR2 on STM32H7R / H7S), supply configuration of the
 * reset (supply not configured yet), flags set by HW model and LDO enable bit */
#if defined(STM32H7RS)
#define UT_PWR_SUPPLY_REG                   ( PWR->CSR2 )
#define UT_PWR_SUPPLY_RESET                 ( PWR_CSR2_SDEN | PWR_CSR2_LDOEN )
#define UT_PWR_SUPPLY_MODEL_FLAGS           ( PWR_CSR2_SDEXTRDY )
#define UT_PWR_SUPPLY_LDOEN                 ( PWR_CSR2_LDOEN )
#define UT_PWR_HAS_SMPS
#elif defined(SMPS)
#define UT_PWR_SUPPLY_REG                   ( PWR->CR3 )
#define UT_PWR_SUPPLY_RESET                 ( PWR_CR3_SMPSEN | PWR_CR3_LDOEN )
#define UT_PWR_SUPPLY_MODEL_FLAGS           ( PWR_CR3_SMPSEXTRDY )
#define UT_PWR_SUPPLY_LDOEN                 ( PWR_CR3_LDOEN )
#define UT_PWR_HAS_SMPS
#else
#define UT_PWR_SUPPLY_REG                   ( PWR->CR3 )
#define UT_PWR_SUPPLY_RESET                 ( PWR_CR3_SCUEN | PWR_CR3_LDOEN )
#define UT_PWR_SUPPLY_MODEL_FLAGS           ( 0u )
#define UT_PWR_SUPPLY_LDOEN                 ( PWR_CR3_LDOEN )
#endif

/* Registers, bits and LL values of the line used by the tests (STM32H7R / H7S: other kernel clock
 * multiplexers - USART1, I2C1 and SPI2 / SPI3 shared, GPDMA1 instead of DMA1, RNG without kernel
 * clock multiplexer on AHB3) */
#if defined(STM32H7RS)
#define UT_LL_USART1_CLKSOURCE              ( LL_RCC_USART1_CLKSOURCE )
#define UT_LL_USART1_CLKSOURCE_PCLK2        ( LL_RCC_USART1_CLKSOURCE_PCLK2 )
#define UT_LL_USART1_CLKSOURCE_HSI          ( LL_RCC_USART1_CLKSOURCE_HSI )
#define UT_LL_USART1_CLKSOURCE_CSI          ( LL_RCC_USART1_CLKSOURCE_CSI )
#define UT_LL_I2C1_CLKSOURCE                ( LL_RCC_I2C1_CLKSOURCE )
#define UT_LL_I2C1_CLKSOURCE_HSI            ( LL_RCC_I2C1_CLKSOURCE_HSI )
#define UT_RCC_PERIPH_SPI_A_PLL2P           ( RCC_PERIPH_SPI2_PLL2P )
#define UT_RCC_PERIPH_SPI_B_PLL2P           ( RCC_PERIPH_SPI3_PLL2P )
#define UT_LL_SPI_SHARED_CLKSOURCE          ( LL_RCC_SPI23_CLKSOURCE )
#define UT_LL_SPI_SHARED_CLKSOURCE_PLL1Q    ( LL_RCC_SPI23_CLKSOURCE_PLL1Q )
#define UT_LL_SPI_SHARED_CLKSOURCE_PLL2P    ( LL_RCC_SPI23_CLKSOURCE_PLL2P )
#define UT_LL_HSI_DIV2                      ( LL_RCC_HSI_DIV_2 )
#define UT_LL_HSI_DIV4                      ( LL_RCC_HSI_DIV_4 )
#define UT_LL_HSI_DIV8                      ( LL_RCC_HSI_DIV_8 )
#define UT_LL_MCO1SOURCE_PLL1Q              ( LL_RCC_MCO1SOURCE_PLL1Q )
#define UT_RCC_CFGR_MCO1                    ( RCC_CFGR_MCO1SEL )
#define UT_RCC_CFGR_MCO2                    ( RCC_CFGR_MCO2SEL )
#define UT_RCC_CR_CSSHSEON                  ( RCC_CR_HSECSSON )
#define UT_RCC_RSR_IWDGRSTF                 ( RCC_RSR_IWDGRSTF )
#define UT_RCC_PERIPH_AHB1                  ( RCC_PERIPH_GPDMA1 )
#define UT_RCC_AHB1RSTR_AHB1                ( RCC_AHB1RSTR_GPDMA1RST )
#define UT_RCC_PERIPH_PLL1Q_KERNEL          ( RCC_PERIPH_SPI1_PLL1Q )
#define UT_RCC_RNG_EN_REG                   ( RCC->AHB3ENR )
#define UT_RCC_RNG_EN_BIT                   ( RCC_AHB3ENR_RNGEN )
#define UT_RCC_CLK_BUS_APB3_5               ( RCC_CLK_BUS_APB5 )
#define UT_RCC_APB3_5_DIVIDER_16            ( RCC_APB5_DIVIDER_16 )
#define UT_RCC_HAS_CEC
#else
#define UT_LL_USART1_CLKSOURCE              ( LL_RCC_USART16_CLKSOURCE )
#define UT_LL_USART1_CLKSOURCE_PCLK2        ( LL_RCC_USART16_CLKSOURCE_PCLK2 )
#define UT_LL_USART1_CLKSOURCE_HSI          ( LL_RCC_USART16_CLKSOURCE_HSI )
#define UT_LL_USART1_CLKSOURCE_CSI          ( LL_RCC_USART16_CLKSOURCE_CSI )
#define UT_LL_I2C1_CLKSOURCE                ( LL_RCC_I2C123_CLKSOURCE )
#define UT_LL_I2C1_CLKSOURCE_HSI            ( LL_RCC_I2C123_CLKSOURCE_HSI )
#define UT_RCC_PERIPH_SPI_A_PLL2P           ( RCC_PERIPH_SPI1_PLL2P )
#define UT_RCC_PERIPH_SPI_B_PLL2P           ( RCC_PERIPH_SPI2_PLL2P )
#define UT_LL_SPI_SHARED_CLKSOURCE          ( LL_RCC_SPI123_CLKSOURCE )
#define UT_LL_SPI_SHARED_CLKSOURCE_PLL1Q    ( LL_RCC_SPI123_CLKSOURCE_PLL1Q )
#define UT_LL_SPI_SHARED_CLKSOURCE_PLL2P    ( LL_RCC_SPI123_CLKSOURCE_PLL2P )
#define UT_LL_HSI_DIV2                      ( LL_RCC_HSI_DIV2 )
#define UT_LL_HSI_DIV4                      ( LL_RCC_HSI_DIV4 )
#define UT_LL_HSI_DIV8                      ( LL_RCC_HSI_DIV8 )
#define UT_LL_MCO1SOURCE_PLL1Q              ( LL_RCC_MCO1SOURCE_PLL1QCLK )
#define UT_RCC_CFGR_MCO1                    ( RCC_CFGR_MCO1 )
#define UT_RCC_CFGR_MCO2                    ( RCC_CFGR_MCO2 )
#define UT_RCC_CR_CSSHSEON                  ( RCC_CR_CSSHSEON )
#define UT_RCC_RSR_IWDGRSTF                 ( RCC_RSR_IWDG1RSTF )
#define UT_RCC_PERIPH_AHB1                  ( RCC_PERIPH_DMA1 )
#define UT_RCC_AHB1RSTR_AHB1                ( RCC_AHB1RSTR_DMA1RST )
#define UT_RCC_PERIPH_PLL1Q_KERNEL          ( RCC_PERIPH_RNG_PLL1Q )
#define UT_RCC_RNG_EN_REG                   ( RCC->AHB2ENR )
#define UT_RCC_RNG_EN_BIT                   ( RCC_AHB2ENR_RNGEN )
#define UT_RCC_CLK_BUS_APB3_5               ( RCC_CLK_BUS_APB3 )
#define UT_RCC_APB3_5_DIVIDER_16            ( RCC_APB3_DIVIDER_16 )
#if defined(RCC_APB1LENR_CECEN)
#define UT_RCC_HAS_CEC
#endif
#endif

/*
 * Flash wait states expected by the tests (RM0433 / RM0468 / RM0455):
 *  - HSI 64 MHz and AXI clock 32 MHz in VOS3,
 *  - HSE 50 MHz in VOS3,
 *  - PLL 200 MHz (AXI clock 200 MHz) in VOS1,
 *  - maximal AXI clock of the line in VOS0 (PLL from HSI 64 MHz).
 */
#if defined(STM32H7RS)
/* RM0477 - VOS low (scale 1) 36 MHz, VOS high (scale 0) 40 MHz per wait state, max AXI clock 200 / 300 MHz */
#define UT_RCC_WS_HSI_VOS3                  ( LL_FLASH_LATENCY_1 )      /*  36 <  64 MHz <=  72 (VOS low)  */
#define UT_RCC_WS_32MHZ_VOS3                ( LL_FLASH_LATENCY_0 )      /*       32 MHz <=  36 (VOS low)  */
#define UT_RCC_WS_HSE50_VOS3                ( LL_FLASH_LATENCY_1 )      /*  36 <  50 MHz <=  72 (VOS low)  */
#define UT_RCC_WS_100MHZ_VOS1               ( LL_FLASH_LATENCY_2 )      /*  72 < 100 MHz <= 108 (VOS low)  */
#define UT_RCC_WS_200MHZ_VOS1               ( LL_FLASH_LATENCY_5 )      /* 180 < 200 MHz <= 200 (VOS low)  */
#define UT_RCC_MAX_PLL_M                    ( 4u  )
#define UT_RCC_MAX_PLL_N                    ( 75u )
#define UT_RCC_MAX_PLL_P                    ( 2u  )
#define UT_RCC_MAX_AHB_DIV                  ( RCC_AHB_DIVIDER_2 )
#define UT_RCC_WS_MAX_VOS0                  ( LL_FLASH_LATENCY_7 )      /* 280 < 300 MHz <= 300 (VOS high) */
#elif (STM32H7_DEV_ID == 0x450UL)
#define UT_RCC_WS_HSI_VOS3                  ( LL_FLASH_LATENCY_1 )      /*  45 <  64 MHz <=  90 */
#define UT_RCC_WS_32MHZ_VOS3                ( LL_FLASH_LATENCY_0 )      /*       32 MHz <=  45 */
#define UT_RCC_WS_HSE50_VOS3                ( LL_FLASH_LATENCY_1 )      /*  45 <  50 MHz <=  90 */
#define UT_RCC_WS_100MHZ_VOS1               ( LL_FLASH_LATENCY_1 )      /*  70 < 100 MHz <= 140 */
#define UT_RCC_WS_200MHZ_VOS1               ( LL_FLASH_LATENCY_2 )      /* 140 < 200 MHz <= 210 */
#define UT_RCC_MAX_PLL_M                    ( 4u  )
#define UT_RCC_MAX_PLL_N                    ( 60u )
#define UT_RCC_MAX_PLL_P                    ( 2u  )
#define UT_RCC_MAX_AHB_DIV                  ( RCC_AHB_DIVIDER_2 )
#define UT_RCC_WS_MAX_VOS0                  ( LL_FLASH_LATENCY_4 )      /* 225 < 240 MHz <= 240 */
#elif (STM32H7_DEV_ID == 0x483UL)
#define UT_RCC_WS_HSI_VOS3                  ( LL_FLASH_LATENCY_1 )      /*  35 <  64 MHz <=  70 */
#define UT_RCC_WS_32MHZ_VOS3                ( LL_FLASH_LATENCY_0 )      /*       32 MHz <=  35 */
#define UT_RCC_WS_HSE50_VOS3                ( LL_FLASH_LATENCY_1 )      /*  35 <  50 MHz <=  70 */
#define UT_RCC_WS_100MHZ_VOS1               ( LL_FLASH_LATENCY_1 )      /*  67 < 100 MHz <= 133 */
#define UT_RCC_WS_200MHZ_VOS1               ( LL_FLASH_LATENCY_2 )      /* 133 < 200 MHz <= 200 */
#define UT_RCC_MAX_PLL_M                    ( 32u  )
#define UT_RCC_MAX_PLL_N                    ( 275u )
#define UT_RCC_MAX_PLL_P                    ( 1u   )
#define UT_RCC_MAX_AHB_DIV                  ( RCC_AHB_DIVIDER_2 )
#define UT_RCC_WS_MAX_VOS0                  ( LL_FLASH_LATENCY_3 )      /* 210 < 275 MHz <= 275 */
#elif (STM32H7_DEV_ID == 0x480UL)
#define UT_RCC_WS_HSI_VOS3                  ( LL_FLASH_LATENCY_2 )      /*  44 <  64 MHz <=  66 */
#define UT_RCC_WS_32MHZ_VOS3                ( LL_FLASH_LATENCY_1 )      /*  22 <  32 MHz <=  44 */
#define UT_RCC_WS_HSE50_VOS3                ( LL_FLASH_LATENCY_2 )      /*  44 <  50 MHz <=  66 */
#define UT_RCC_WS_100MHZ_VOS1               ( LL_FLASH_LATENCY_2 )      /*  76 < 100 MHz <= 114 */
#define UT_RCC_WS_200MHZ_VOS1               ( LL_FLASH_LATENCY_5 )      /* 190 < 200 MHz <= 225 */
#define UT_RCC_MAX_PLL_M                    ( 16u  )
#define UT_RCC_MAX_PLL_N                    ( 140u )
#define UT_RCC_MAX_PLL_P                    ( 2u   )
#define UT_RCC_MAX_AHB_DIV                  ( RCC_AHB_DIVIDER_1 )
#define UT_RCC_WS_MAX_VOS0                  ( LL_FLASH_LATENCY_6 )      /* 252 < 280 MHz <= 280 */
#else
#error "Test_Rcc: device line is not supported."
#endif

/* ========================== LOCAL VARIABLES =============================== */

/** Sample of peripherals from every clock bus with their enable bit */
static const utRcc_PeriphEnable_t utRcc_PeriphEnableLut[] =
{
#if defined(STM32H7RS)
    { .PeriphId = RCC_PERIPH_GPDMA1,   .EnableReg = &RCC->AHB1ENR,  .EnableBit = RCC_AHB1ENR_GPDMA1EN   },
    { .PeriphId = RCC_PERIPH_SRAM1,    .EnableReg = &RCC->AHB2ENR,  .EnableBit = RCC_AHB2ENR_SRAM1EN    },
    { .PeriphId = RCC_PERIPH_HASH,     .EnableReg = &RCC->AHB3ENR,  .EnableBit = RCC_AHB3ENR_HASHEN     },
    { .PeriphId = RCC_PERIPH_GPIOA,    .EnableReg = &RCC->AHB4ENR,  .EnableBit = RCC_AHB4ENR_GPIOAEN    },
    { .PeriphId = RCC_PERIPH_DMA2D,    .EnableReg = &RCC->AHB5ENR,  .EnableBit = RCC_AHB5ENR_DMA2DEN    },
    { .PeriphId = RCC_PERIPH_TIM2,     .EnableReg = &RCC->APB1ENR1, .EnableBit = RCC_APB1ENR1_TIM2EN    },
    { .PeriphId = RCC_PERIPH_CRS,      .EnableReg = &RCC->APB1ENR2, .EnableBit = RCC_APB1ENR2_CRSEN     },
    { .PeriphId = RCC_PERIPH_TIM1,     .EnableReg = &RCC->APB2ENR,  .EnableBit = RCC_APB2ENR_TIM1EN     },
    { .PeriphId = RCC_PERIPH_SBS,      .EnableReg = &RCC->APB4ENR,  .EnableBit = RCC_APB4ENR_SBSEN      },
    { .PeriphId = RCC_PERIPH_GFXTIM,   .EnableReg = &RCC->APB5ENR,  .EnableBit = RCC_APB5ENR_GFXTIMEN   },
#else
    { .PeriphId = RCC_PERIPH_DMA1,     .EnableReg = &RCC->AHB1ENR,  .EnableBit = RCC_AHB1ENR_DMA1EN     },
#if defined(RCC_AHB2ENR_SRAM1EN)
    { .PeriphId = RCC_PERIPH_SRAM1,    .EnableReg = &RCC->AHB2ENR,  .EnableBit = RCC_AHB2ENR_SRAM1EN    },
#else
    { .PeriphId = RCC_PERIPH_AHBSRAM1, .EnableReg = &RCC->AHB2ENR,  .EnableBit = RCC_AHB2ENR_AHBSRAM1EN },
#endif
    { .PeriphId = RCC_PERIPH_MDMA,     .EnableReg = &RCC->AHB3ENR,  .EnableBit = RCC_AHB3ENR_MDMAEN     },
    { .PeriphId = RCC_PERIPH_GPIOA,    .EnableReg = &RCC->AHB4ENR,  .EnableBit = RCC_AHB4ENR_GPIOAEN    },
    { .PeriphId = RCC_PERIPH_TIM2,     .EnableReg = &RCC->APB1LENR, .EnableBit = RCC_APB1LENR_TIM2EN    },
    { .PeriphId = RCC_PERIPH_CRS,      .EnableReg = &RCC->APB1HENR, .EnableBit = RCC_APB1HENR_CRSEN     },
    { .PeriphId = RCC_PERIPH_TIM1,     .EnableReg = &RCC->APB2ENR,  .EnableBit = RCC_APB2ENR_TIM1EN     },
#if defined(RCC_APB3ENR_WWDG1EN)
    { .PeriphId = RCC_PERIPH_WWDG1,    .EnableReg = &RCC->APB3ENR,  .EnableBit = RCC_APB3ENR_WWDG1EN    },
#else
    { .PeriphId = RCC_PERIPH_WWDG,     .EnableReg = &RCC->APB3ENR,  .EnableBit = RCC_APB3ENR_WWDGEN     },
#endif
    { .PeriphId = RCC_PERIPH_SYSCFG,   .EnableReg = &RCC->APB4ENR,  .EnableBit = RCC_APB4ENR_SYSCFGEN   },
#endif
};

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    /* Stops HW model of previous test and clears registers */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    /* Voltage scale of the reset - VOS3 (VOS low on STM32H7R / H7S) requested and active */
    UT_PWR_VOS_REG    = UT_PWR_VOS_RESET;
    UT_PWR_ACTVOS_REG = UT_PWR_VOS_RESET;
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
 * \brief   Rcc_Get_DefaultConfig() fills default configuration valid for every board.
 *
 * \details Reads default configuration, then calls the function with NULL.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, system clock from HSI, all PLLs stopped (source NONE), HSE not used.
 * - Supply configuration of the reset, voltage scale 3 (1 - VOS low on STM32H7R / H7S),
 *   all prescalers 1, PLL outputs S / T not used (STM32H7R / H7S).
 * - HSE frequency 8 MHz, clock outputs not used.
 * - NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_DefaultConfig_ReturnsHsiWithoutPll( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_SYSTEM_CLOCK_SOURCE_HSI, config.SystemClockSource );
    TEST_ASSERT_EQUAL( RCC_PWR_SUPPLY_DEFAULT,      config.PowerSupply );
    TEST_ASSERT_EQUAL( UT_RCC_SCALE_RESET,          config.VoltageScaling );
    TEST_ASSERT_EQUAL( RCC_HSE_TYPE_NONE,           config.HSE_ClockType );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE,       config.CSS_Enable );
    TEST_ASSERT_EQUAL( RCC_SYS_DIVIDER_1,           config.SYS_Divider );
    TEST_ASSERT_EQUAL( RCC_AHB_DIVIDER_1,           config.AHB_Divider );
    TEST_ASSERT_EQUAL( RCC_APB4_DIVIDER_1,          config.APB4_Divider );
#if defined(STM32H7RS)
    TEST_ASSERT_EQUAL( RCC_APB5_DIVIDER_1,          config.APB5_Divider );
#endif
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ, config.HSE_Frequency_Hz );

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        TEST_ASSERT_EQUAL( RCC_PLL_SRC_NONE, config.Pll_Config[ pllId ].Pll_Source );
        TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_M, config.Pll_Config[ pllId ].M_Divider );
        TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_N, config.Pll_Config[ pllId ].N_Multiplier );
#if defined(STM32H7RS)
        TEST_ASSERT_EQUAL_UINT32( 0u, config.Pll_Config[ pllId ].S_Divider );
        TEST_ASSERT_EQUAL_UINT32( 0u, config.Pll_Config[ pllId ].T_Divider );
#endif
    }

    for( rcc_ClkOut_Id_t outId = RCC_CLK_OUT_MCO1; RCC_CLK_OUT_CNT > outId; outId++ )
    {
        TEST_ASSERT_EQUAL( RCC_CLK_SOURCE_NONE, config.McoConfig[ outId ].ClockSource );
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_DefaultConfig( NULL ) );
}

/* ========================= PERIPHERAL CLOCKS ============================== */

/**
 * \brief   Rcc_Set_PeriphActive() sets only the clock enable bit of the peripheral.
 *
 * \details Enables clock of a sample peripheral from every bus (DMA1 - AHB1, SRAM1 -
 *          AHB2, MDMA - AHB3, GPIOA - AHB4, TIM2 - APB1L, CRS - APB1H, TIM1 - APB2,
 *          WWDG1 - APB3, SYSCFG - APB4; STM32H7R / H7S: GPDMA1 - AHB1, SRAM1 - AHB2,
 *          HASH - AHB3, GPIOA - AHB4, DMA2D - AHB5, TIM2 - APB1 1, CRS - APB1 2, TIM1 -
 *          APB2, SBS - APB4, GFXTIM - APB5), registers are cleared before every peripheral.
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
 * \brief   Rcc_Set_PeriphActive() selects kernel clock of the peripheral.
 *
 * \details Enables USART1 with HSI kernel clock (HSI off, started by HW model), then
 *          reads clock source with USART1 / PCLK2 identification.
 *
 * \par Expected results
 * - HSI started, USART16(910)SEL = HSI, APB2ENR.USART1EN is set.
 * - Any USART1 identification returns the selected source RCC_PERIPH_USART1_HSI.
 */
void Ut_Rcc_Set_PeriphActive_Usart1Hsi_SelectsClockMux( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSION, RCC->CR );     /* Kernel clock oscillator started */

    TEST_ASSERT_EQUAL_HEX32( UT_LL_USART1_CLKSOURCE_HSI, LL_RCC_GetUSARTClockSource( UT_LL_USART1_CLKSOURCE ) );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );

    /* Any USART1 entry returns the entry of actually selected source */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_USART1_PCLK2, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_USART1_HSI, clkSrc );
}


/**
 * \brief   Rcc_Set_PeriphActive() reports clock MUX set to other source.
 *
 * \details USART1 clock MUX is preset to HSI (not default source), then USART1 is
 *          enabled with PLL2 Q kernel clock.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, USART1 clock MUX stays HSI.
 * - APB2ENR.USART1EN is not set.
 */
void Ut_Rcc_Set_PeriphActive_MuxSetToOtherSource_ReturnsErrorWithoutEnable( void )
{
    LL_RCC_SetUSARTClockSource( UT_LL_USART1_CLKSOURCE_HSI );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_PLL2Q ) );

    TEST_ASSERT_EQUAL_HEX32( UT_LL_USART1_CLKSOURCE_HSI, LL_RCC_GetUSARTClockSource( UT_LL_USART1_CLKSOURCE ) );
    TEST_ASSERT_BITS_LOW( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Rcc_Get_PeriphClkSrc() returns source selected by reset value of the MUX.
 *
 * \details Reads clock sources of USART1 and I2C1 (LL_CLKSOURCE() encoded records)
 *          with cleared kernel clock selection registers.
 *
 * \par Expected results
 * - USART1: RCC_PERIPH_USART1_PCLK2, I2C1: RCC_PERIPH_I2C1_PCLK1.
 */
void Ut_Rcc_Get_PeriphClkSrc_EncodedMuxDefault_ReturnsDefaultSource( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

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
 * - I2C123(5)SEL = HSI, source reads back RCC_PERIPH_I2C1_HSI.
 * - Repeated request keeps the selection.
 */
void Ut_Rcc_Set_PeriphActive_I2c1Hsi_SelectsClockMuxAndSourceReadBack( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C1_HSI ) );

    TEST_ASSERT_EQUAL_HEX32( UT_LL_I2C1_CLKSOURCE_HSI, LL_RCC_GetI2CClockSource( UT_LL_I2C1_CLKSOURCE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_I2C1_PCLK1, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_I2C1_HSI, clkSrc );

    /* Repeated request of the selected source keeps the selection */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_I2C1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_I2C1_CLKSOURCE_HSI, LL_RCC_GetI2CClockSource( UT_LL_I2C1_CLKSOURCE ) );
}


/**
 * \brief   ADC PLL3 R kernel clock is selected and read back (raw MUX value record).
 *
 * \details Enables ADC1 / ADC2 with PLL3 R kernel clock and reads the source.
 *
 * \par Expected results
 * - ADCSEL = PLL3 R, source reads back RCC_PERIPH_ADC12_PLL3R.
 */
void Ut_Rcc_Set_PeriphActive_AdcPll3r_RawMuxSelectedAndSourceReadBack( void )
{
    rcc_PeriphId_t clkSrc = RCC_PERIPH_ID_CNT;

    /* ADCSEL records hold raw field values */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC12_PLL3R ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_ADC_CLKSOURCE_PLL3R, LL_RCC_GetADCClockSource( LL_RCC_ADC_CLKSOURCE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClkSrc( RCC_PERIPH_ADC12_PLL2P, &clkSrc ) );
    TEST_ASSERT_EQUAL( RCC_PERIPH_ADC12_PLL3R, clkSrc );
}


/**
 * \brief   ADC synchronous clock (HCLK) enables the ADC clock without kernel clock selection.
 *
 * \details ADCSEL preset to PLL3 R, ADC1 / ADC2 enabled with RCC_PERIPH_ADC12_HCLK (ADC
 *          synchronous clock mode of the Adc module - CKMODE), frequency read for the
 *          synchronous clock and for DMA1 (AHB clock), then the clock is disabled.
 *
 * \par Expected results
 * - AHB1ENR.ADC12EN set, ADCSEL kept (PLL3 R).
 * - Frequency of RCC_PERIPH_ADC12_HCLK equals the AHB clock.
 * - Rcc_Set_PeriphInactive(): AHB1ENR.ADC12EN cleared.
 */
void Ut_Rcc_Set_PeriphActive_AdcSyncHclk_ClockEnabledMuxKept( void )
{
    rcc_FreqHz_t adcFreq = 0u;
    rcc_FreqHz_t ahbFreq = 0u;

    LL_RCC_SetADCClockSource( LL_RCC_ADC_CLKSOURCE_PLL3R );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_ADC12_HCLK ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB1ENR_ADC12EN, RCC->AHB1ENR & RCC_AHB1ENR_ADC12EN );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_ADC_CLKSOURCE_PLL3R, LL_RCC_GetADCClockSource( LL_RCC_ADC_CLKSOURCE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_HCLK, &adcFreq ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( UT_RCC_PERIPH_AHB1, &ahbFreq ) );
    TEST_ASSERT_NOT_EQUAL( 0u, ahbFreq );
    TEST_ASSERT_EQUAL_UINT32( ahbFreq, adcFreq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_ADC12_HCLK ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB1ENR & RCC_AHB1ENR_ADC12EN );
}


/**
 * \brief   Rcc_Set_PeriphActive() starts internal oscillator of the kernel clock.
 *
 * \details Enables RNG with HSI48 kernel clock, HSI48 is off (HW model sets ready flag).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, HSI48 started (CR.HSI48ON), RNGSEL = HSI48, AHB2ENR.RNGEN set.
 * - STM32H7R / H7S (RNG kernel clock HSI48 without multiplexer): HSI48 started,
 *   AHB3ENR.RNGEN set.
 */
void Ut_Rcc_Set_PeriphActive_RngHsi48_StartsOscillator( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_HSI48 ) );

    TEST_ASSERT_BITS_HIGH( RCC_CR_HSI48ON, RCC->CR );
#if defined(LL_RCC_RNG_CLKSOURCE_HSI48)
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RNG_CLKSOURCE_HSI48, LL_RCC_GetRNGClockSource( LL_RCC_RNG_CLKSOURCE ) );
#endif
    TEST_ASSERT_BITS_HIGH( UT_RCC_RNG_EN_BIT, UT_RCC_RNG_EN_REG );
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
    TEST_ASSERT_BITS_HIGH( UT_RCC_RNG_EN_BIT, UT_RCC_RNG_EN_REG );
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

    TEST_ASSERT_BITS_LOW( UT_RCC_RNG_EN_BIT, UT_RCC_RNG_EN_REG );
}


/**
 * \brief   Rcc_Set_PeriphActive() does not start external oscillator.
 *
 * \details Enables RNG with LSE kernel clock (USART1 on STM32H7R / H7S - RNG without
 *          kernel clock multiplexer), no HW model.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, BDCR.LSEON not set, RNGSEL = LSE, AHB2ENR.RNGEN set.
 * - STM32H7R / H7S: RCC_REQUEST_OK, BDCR.LSEON not set, USART1SEL = LSE,
 *   APB2ENR.USART1EN set.
 */
void Ut_Rcc_Set_PeriphActive_ExternalSource_NotStarted( void )
{
#if defined(LL_RCC_RNG_CLKSOURCE_LSE)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RNG_LSE ) );

    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RNG_CLKSOURCE_LSE, LL_RCC_GetRNGClockSource( LL_RCC_RNG_CLKSOURCE ) );
    TEST_ASSERT_BITS_HIGH( RCC_AHB2ENR_RNGEN, RCC->AHB2ENR );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_LSE ) );

    TEST_ASSERT_BITS_LOW( RCC_BDCR_LSEON, RCC->BDCR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_USART1_CLKSOURCE_LSE, LL_RCC_GetUSARTClockSource( LL_RCC_USART1_CLKSOURCE ) );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
#endif
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
 *          disables the clock and reads the state. Kernel clock selection registers
 *          are cleared after every peripheral (RTC / CKPER selection is kept by
 *          Rcc_Set_PeriphInactive()).
 *
 * \par Expected results
 * - RCC_REQUEST_OK for all calls, state active after enabling.
 * - State inactive after disabling, except peripherals without clock enable bit
 *   (SysTick, IWDG, peripheral clock CKPER, trace clock) reported as always active.
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
        if( RCC_FUNCTION_INACTIVE != state )
        {
            TEST_ASSERT_TRUE_MESSAGE( ( RCC_PERIPH_SYSTICK   == periphId ) ||
                                      ( RCC_PERIPH_IWDG      == periphId ) ||
                                      ( RCC_PERIPH_LPCLK_HSI == periphId ) ||
                                      ( RCC_PERIPH_LPCLK_CSI == periphId ) ||
                                      ( RCC_PERIPH_LPCLK_HSE == periphId ) ||
                                      ( RCC_PERIPH_TRACE     == periphId ), "State after deactivation" );
        }
        else
        {
            /* Peripheral with clock enable bit is inactive */
        }

        /* RTC / CKPER selection is kept - reset values are restored, so next source can be selected */
        UT_RCC_CCIPR1 = 0u;
        UT_RCC_CCIPR2 = 0u;
        UT_RCC_CCIPR3 = 0u;
        UT_RCC_CCIPR4 = 0u;
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
    RCC->AHB4ENR = RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_GPIOA ) );

    TEST_ASSERT_EQUAL_HEX32( RCC_AHB4ENR_GPIOBEN, RCC->AHB4ENR );
}


/**
 * \brief   Rcc_Set_PeriphInactive() releases the kernel clock multiplexer.
 *
 * \details USART1 enabled with HSI kernel clock, disabled, then enabled with CSI kernel
 *          clock.
 *
 * \par Expected results
 * - Disable: APB2ENR.USART1EN cleared, USART1 MUX back to the default (PCLK2).
 * - Second enable with CSI: RCC_REQUEST_OK, USART1 MUX = CSI.
 */
void Ut_Rcc_Set_PeriphInactive_KernelClockMux_Released( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_USART1_CLKSOURCE_HSI, LL_RCC_GetUSARTClockSource( UT_LL_USART1_CLKSOURCE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_USART1_HSI ) );
    TEST_ASSERT_BITS_LOW( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_USART1_CLKSOURCE_PCLK2, LL_RCC_GetUSARTClockSource( UT_LL_USART1_CLKSOURCE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_USART1_CSI ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_USART1_CLKSOURCE_CSI, LL_RCC_GetUSARTClockSource( UT_LL_USART1_CLKSOURCE ) );
    TEST_ASSERT_BITS_HIGH( RCC_APB2ENR_USART1EN, RCC->APB2ENR );
}


/**
 * \brief   Rcc_Set_PeriphInactive() keeps a multiplexer shared with an enabled block.
 *
 * \details SPI1 and SPI2 enabled with PLL2 P kernel clock (common field SPI123SEL), SPI2
 *          disabled, then SPI1 disabled. STM32H7R / H7S: SPI2 and SPI3 (common field
 *          SPI23SEL), SPI3 disabled, then SPI2.
 *
 * \par Expected results
 * - SPI2 disabled: RCC_REQUEST_OK, SPI123SEL stays PLL2 P (SPI1 still enabled).
 * - SPI1 disabled: SPI123SEL back to the default (PLL1 Q).
 */
void Ut_Rcc_Set_PeriphInactive_SharedMux_KeptWhileOtherBlockEnabled( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( UT_RCC_PERIPH_SPI_A_PLL2P ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( UT_RCC_PERIPH_SPI_B_PLL2P ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_SPI_SHARED_CLKSOURCE_PLL2P, LL_RCC_GetSPIClockSource( UT_LL_SPI_SHARED_CLKSOURCE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( UT_RCC_PERIPH_SPI_B_PLL2P ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_SPI_SHARED_CLKSOURCE_PLL2P, LL_RCC_GetSPIClockSource( UT_LL_SPI_SHARED_CLKSOURCE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( UT_RCC_PERIPH_SPI_A_PLL2P ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_SPI_SHARED_CLKSOURCE_PLL1Q, LL_RCC_GetSPIClockSource( UT_LL_SPI_SHARED_CLKSOURCE ) );
}


/**
 * \brief   Rcc_Set_PeriphInactive() keeps the RTC clock selection.
 *
 * \details RTC enabled with LSI clock (RTCSEL is write-once until backup domain reset),
 *          then disabled.
 *
 * \par Expected results
 * - LSI started, RTC APB clock enabled (APB4ENR.RTCAPBEN).
 * - RCC_REQUEST_OK, BDCR.RTCSEL stays LSI after disabling.
 */
void Ut_Rcc_Set_PeriphInactive_Rtc_ClockSelectionKept( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_BITS_HIGH( RCC_CSR_LSION, RCC->CSR );
    TEST_ASSERT_BITS_HIGH( RCC_APB4ENR_RTCAPBEN, RCC->APB4ENR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_RTC_LSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_RTC_CLKSOURCE_LSI, RCC->BDCR & RCC_BDCR_RTCSEL );
}


/**
 * \brief   Peripheral clock (CKPER) selection is kept by Rcc_Set_PeriphInactive().
 *
 * \details CKPER enabled with CSI (CSI started by HW model), then disabled.
 *
 * \par Expected results
 * - CKPERSEL = CSI after enabling and after disabling, state always active.
 */
void Ut_Rcc_Set_PeriphInactive_Ckper_SelectionKept( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_INACTIVE;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphActive( RCC_PERIPH_LPCLK_CSI ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_CSION, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_CLKP_CLKSOURCE_CSI, LL_RCC_GetCLKPClockSource( LL_RCC_CLKP_CLKSOURCE ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PeriphInactive( RCC_PERIPH_LPCLK_CSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_CLKP_CLKSOURCE_CSI, LL_RCC_GetCLKPClockSource( LL_RCC_CLKP_CLKSOURCE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphState( RCC_PERIPH_LPCLK_CSI, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}


/**
 * \brief   Peripheral functions reject invalid arguments.
 *
 * \details Calls enable, disable, state, reset and sleep functions with peripheral
 *          out of range and getters with NULL pointer.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
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
}

/* ========================== RESET AND SLEEP =============================== */

/**
 * \brief   Peripheral reset of USART1 is set, read back and released.
 *
 * \details Activates USART1 reset, reads the reset state and deactivates the reset.
 *
 * \par Expected results
 * - APB2RSTR = USART1RST, reset state active.
 * - APB2RSTR = 0 after release, reset state inactive.
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Reset of peripheral without reset bit does not reset other peripherals.
 *
 * \details Activates and deactivates reset of SysTick and IWDG (no reset bit, AHB1
 *          block), reads reset state of IWDG with reset of DMA1 active.
 *
 * \par Expected results
 * - Rcc_Set_ResetActive(): RCC_REQUEST_ERROR, AHB1RSTR stays 0.
 * - Rcc_Set_ResetInactive(): RCC_REQUEST_OK, active reset of DMA1 is kept.
 * - Rcc_Get_ResetState(): RCC_REQUEST_OK, IWDG reset inactive.
 */
void Ut_Rcc_Set_ResetActive_PeriphWithoutResetBit_DoesNotResetOthers( void )
{
    rcc_FunctionState_t state = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_SYSTICK ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetActive( RCC_PERIPH_IWDG ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->AHB1RSTR );

    RCC->AHB1RSTR = UT_RCC_AHB1RSTR_AHB1;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetInactive( RCC_PERIPH_SYSTICK ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_AHB1RSTR_AHB1, RCC->AHB1RSTR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ResetState( RCC_PERIPH_IWDG, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );
}


/**
 * \brief   Sleep mode clock of USART1 is enabled, read back and disabled.
 *
 * \details Activates USART1 clock in sleep mode, reads the sleep state and deactivates
 *          it. Sleep state of IWDG (no sleep control) is read.
 *
 * \par Expected results
 * - APB2LPENR = USART1LPEN, sleep state active.
 * - APB2LPENR = 0 after deactivation, sleep state inactive.
 * - IWDG: RCC_REQUEST_OK for activation / deactivation, always active.
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_USART1_PCLK2, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_INACTIVE, state );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepActive( RCC_PERIPH_IWDG ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_SleepInactive( RCC_PERIPH_IWDG ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SleepState( RCC_PERIPH_IWDG, &state ) );
    TEST_ASSERT_EQUAL( RCC_FUNCTION_ACTIVE, state );
}

/* ========================= BUS CLOCKS AND DIVIDERS ======================== */

/**
 * \brief   Rcc_Set_ClkBusDivider() writes bus prescalers.
 *
 * \details System clock HSI 64 MHz. Sets AHB divider 2, APB1 divider 4 and APB4
 *          divider 8, then calls the function with bus out of range.
 *
 * \par Expected results
 * - HPRE = /2, D2PPRE1 = /4, D3PPRE = /8.
 * - Flash latency set for AXI clock 32 MHz in VOS3 of the device line, SystemD2Clock 32 MHz.
 * - Bus out of range: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_ClkBusDivider_WritesPrescalers( void )
{
    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB4,   RCC_APB4_DIVIDER_8 ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_AHB_DIV_2,  LL_RCC_GetAHBPrescaler() );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB1_DIV_4, LL_RCC_GetAPB1Prescaler() );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB4_DIV_8, LL_RCC_GetAPB4Prescaler() );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_32MHZ_VOS3, LL_FLASH_GetLatency() );
    Ut_Rcc_Assert_AxiClk( UT_RCC_HSI_HZ / 2u );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_CNT, RCC_AHB_DIVIDER_2 ) );
}


/**
 * \brief   Invalid prescaler value is rejected.
 *
 * \details Sets APB2 divider 3 (not a prescaler value).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, APB2 prescaler stays /1.
 */
void Ut_Rcc_Set_ClkBusDivider_InvalidValue_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2, 3u ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_APB2_DIV_1, LL_RCC_GetAPB2Prescaler() );
}


/**
 * \brief   Bus clocks are calculated from system clock and dividers.
 *
 * \details System clock HSI 64 MHz, system clock divider 2, AHB divider 2, APB1
 *          divider 4. Reads CPU clock (SysTick), AHB2 and APB1 bus clocks and CRS
 *          (APB1) peripheral clock.
 *
 * \par Expected results
 * - CPU = 32 MHz, AHB2 = 16 MHz, APB1 = 4 MHz, CRS = 4 MHz.
 */
void Ut_Rcc_Get_ClkBusClk_HsiSysClk_AppliesDividers( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    LL_RCC_SetSysPrescaler( LL_RCC_SYSCLK_DIV_2 );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB1,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_1, RCC_APB1_DIVIDER_4 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SYSTICK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 16u, freq );

    /* Peripheral clock follows its bus */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_CRS, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 16u, freq );
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
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB2_DIVIDER_2 ) );

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
    RCC->CFGR |= RCC_CFGR_TIMPRE;

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
 * \details System clock HSI with HSIDIV / 2. Reads AHB1 clock.
 *
 * \par Expected results
 * - AHB1 = 32 MHz.
 */
void Ut_Rcc_Get_ClkBusClk_HsiDividerApplied( void )
{
    rcc_FreqHz_t freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CR |= UT_LL_HSI_DIV2;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
}

/* =============================== SYSTICK ================================== */

/**
 * \brief   Rcc_Set_SysTickInterval() configures 1 ms SysTick from CPU clock.
 *
 * \details System clock HSI 64 MHz, AHB divider 2 (SysTick uses CPU clock, not HCLK).
 *          Sets 1 ms interval and reads it back.
 *
 * \par Expected results
 * - LOAD = 63999, CTRL: counter enabled, interrupt enabled, processor clock.
 * - Interval reads back 1 ms.
 */
void Ut_Rcc_Set_SysTickInterval_1ms_ConfiguresReload( void )
{
    rcc_Time_ms_t interval = 0u;

    Ut_Rcc_Set_SysClkHsi();
    LL_RCC_SetAHBPrescaler( LL_RCC_AHB_DIV_2 );

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
 *          24 bits), reads interval to NULL.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, SysTick LOAD and CTRL stay 0.
 */
void Ut_Rcc_Set_SysTickInterval_OutOfRange_ReturnsErrorWithoutWrite( void )
{
    Ut_Rcc_Set_SysClkHsi();

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_SysTickInterval( UT_RCC_SYSTICK_TOO_LONG_MS ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_SysTickInterval( NULL ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->LOAD );
    TEST_ASSERT_EQUAL_HEX32( 0u, SysTick->CTRL );
}

/* ============================ FLASH AND POWER ============================= */

/**
 * \brief   Flash prefetch is not available on STM32H7.
 *
 * \details Calls Rcc_Set_FlashPrefetchActive() and Rcc_Set_FlashPrefetchInactive().
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases, FLASH_ACR not changed.
 */
void Ut_Rcc_Set_FlashPrefetch_NotSupported_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashPrefetchActive() );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashPrefetchInactive() );

    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR );
}


/**
 * \brief   Rcc_Set_PwrRange() sets voltage scaling.
 *
 * \details HW model applies the voltage scale (ACTVOS, ACTVOSRDY). Sets voltage
 *          scale 1, then voltage scale 3 (STM32H7R / H7S: VOS high, then VOS low).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, VOS = scale 1, then scale 3 (VOS high, then VOS low).
 */
void Ut_Rcc_Set_PwrRange_RegulatorReady_WritesScale( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    config.VoltageScaling = UT_RCC_SCALE_HIGHER;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_VOS_HIGHER, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );

    config.VoltageScaling = UT_RCC_SCALE_RESET;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_VOS_RESET, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );
}


/**
 * \brief   Rcc_Set_PwrRange() reports regulator not ready.
 *
 * \details Without HW model (ACTVOSRDY never set) sets voltage scale 1 (VOS high on
 *          STM32H7R / H7S), then calls the function with NULL and with voltage scale out
 *          of range. STM32H7R / H7S: voltage scales 2 and 3 (not available).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases (timeout, NULL pointer, invalid scale).
 */
void Ut_Rcc_Set_PwrRange_RegulatorNotReady_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    /* ACTVOSRDY never set */
    config.VoltageScaling = UT_RCC_SCALE_HIGHER;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( NULL ) );

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );

#if defined(STM32H7RS)
    /* Voltage scales not available on STM32H7R / H7S - VOS of the first request (VOS high) not changed */
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_2;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_3;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_VOS_HIGHER, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );
#endif
}


/**
 * \brief   Rcc_Set_PwrRange() does not write the active voltage scale.
 *
 * \details Supply not configured after reset (ACTVOSRDY = 0), VOS3 (VOS low) active.
 *          Sets the same voltage scale without HW model.
 *
 * \par Expected results
 * - RCC_REQUEST_OK without waiting for ACTVOSRDY, VOS register unchanged.
 */
void Ut_Rcc_Set_PwrRange_ScaleActive_NothingWritten( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_VOS_RESET, UT_PWR_VOS_REG );
}


/**
 * \brief   Voltage scale 0 is set (overdrive on STM32H742 / H743 / H745 / H747 / H750 /
 *          H753 / H755 / H757).
 *
 * \details HW model applies the voltage scale, LDO supply. Sets voltage scale 0, then
 *          voltage scale 1. On devices with overdrive sets voltage scale 0 without LDO.
 *
 * \par Expected results
 * - Overdrive devices: VOS = scale 1 + SYSCFG ODEN (SYSCFG clock enabled), ODEN
 *   cleared by voltage scale 1, voltage scale 0 without LDO rejected.
 * - Other devices: VOS = scale 0, then scale 1.
 */
void Ut_Rcc_Set_PwrRange_Scale0_OverdriveOrScale0( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_LDOEN;

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_0;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE0, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );
#if defined(SYSCFG_PWRCR_ODEN)
    TEST_ASSERT_BITS_HIGH( SYSCFG_PWRCR_ODEN, SYSCFG->PWRCR );
    TEST_ASSERT_BITS_HIGH( RCC_APB4ENR_SYSCFGEN, RCC->APB4ENR );
#endif

    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_1;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE1, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );
#if defined(SYSCFG_PWRCR_ODEN)
    TEST_ASSERT_BITS_LOW( SYSCFG_PWRCR_ODEN, SYSCFG->PWRCR );

    /* Voltage scale 0 needs LDO supply */
    UT_PWR_SUPPLY_REG = 0u;
    config.VoltageScaling = RCC_PWR_VOLTAGE_SCALE_0;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PwrRange( &config ) );
    TEST_ASSERT_BITS_LOW( SYSCFG_PWRCR_ODEN, SYSCFG->PWRCR );
#endif
}


/**
 * \brief   Flash latency for HSI 64 MHz system clock in voltage scale 3.
 *
 * \details Default configuration (HSI system clock), VOS3 active.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency of the device line (1 WS, 2 WS on STM32H7A3 / H7B0 / H7B3).
 */
void Ut_Rcc_Set_FlashLatency_HsiSysClkScale3_LineLatency( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_HSI_VOS3, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency respects HSI divider.
 *
 * \details System clock source HSI with HSIDIV / 8 (8 MHz), VOS3 active.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 0 wait states.
 */
void Ut_Rcc_Set_FlashLatency_HsiDiv8SysClk_Sets0WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    RCC->CR    = UT_LL_HSI_DIV8;
    FLASH->ACR = LL_FLASH_LATENCY_7;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency for HSE system clock is calculated from HSE frequency.
 *
 * \details System clock source HSE 50 MHz, VOS3 active.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency of the device line.
 */
void Ut_Rcc_Set_FlashLatency_HseSysClk_LatencyFromHseFrequency( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_HSE;
    config.HSE_Frequency_Hz  = UT_RCC_HSE_50MHZ_HZ;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_HSE50_VOS3, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency for maximal AXI clock in voltage scale 0.
 *
 * \details PLL1 from HSI configured for maximal AXI clock of the device line (240 MHz,
 *          275 MHz, 280 MHz), VOS0 active (VOS1 + overdrive on STM32H742 / H743 / H745 /
 *          H747 / H750 / H753 / H755 / H757). PLL1 R divider does not influence the result.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency 4 WS / 3 WS / 6 WS (ST LL driver sets 2 WS for 240 MHz).
 */
void Ut_Rcc_Set_FlashLatency_MaxClkScale0_LineLatency( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    config.AHB_Divider       = UT_RCC_MAX_AHB_DIV;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSI, UT_RCC_MAX_PLL_M, UT_RCC_MAX_PLL_N, UT_RCC_MAX_PLL_P );
    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 8u;
    Ut_Rcc_Set_ActiveScale( RCC_PWR_VOLTAGE_SCALE_0 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_MAX_VOS0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash programming delay of STM32H7R / H7S follows the wait states.
 *
 * \details
 * 1. Maximal AXI clock (300 MHz) in VOS high.
 * 2. HSI system clock (64 MHz) in VOS low, FLASH_ACR preset to 7 WS / WRHIGHFREQ 3.
 *
 * \par Expected results
 * 1. RCC_REQUEST_OK, LATENCY 7 WS, WRHIGHFREQ 3.
 * 2. RCC_REQUEST_OK, LATENCY 1 WS, WRHIGHFREQ 0.
 * - Other lines: test ignored (programming delay keeps its reset value).
 */
void Ut_Rcc_Set_FlashLatency_ProgrammingDelay_FollowsWaitStates( void )
{
#if defined(STM32H7RS)
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    config.AHB_Divider       = UT_RCC_MAX_AHB_DIV;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSI, UT_RCC_MAX_PLL_M, UT_RCC_MAX_PLL_N, UT_RCC_MAX_PLL_P );
    Ut_Rcc_Set_ActiveScale( RCC_PWR_VOLTAGE_SCALE_0 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_7, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_HEX32( 3u << FLASH_ACR_WRHIGHFREQ_Pos, FLASH->ACR & FLASH_ACR_WRHIGHFREQ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    Ut_Rcc_Set_ActiveScale( RCC_PWR_VOLTAGE_SCALE_1 );
    FLASH->ACR = LL_FLASH_LATENCY_7 | FLASH_ACR_WRHIGHFREQ;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_1, FLASH->ACR & FLASH_ACR_LATENCY );
    TEST_ASSERT_EQUAL_HEX32( 0u, FLASH->ACR & FLASH_ACR_WRHIGHFREQ );
#else
    TEST_IGNORE_MESSAGE( "Flash programming delay is set on STM32H7R / H7S only" );
#endif
}


/**
 * \brief   Flash latency rejects frequency not allowed in voltage scale.
 *
 * \details Maximal AXI clock of the device line (see the previous test) with VOS3
 *          active, then NULL configuration.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, latency not changed.
 */
void Ut_Rcc_Set_FlashLatency_MaxClkScale3_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    config.AHB_Divider       = UT_RCC_MAX_AHB_DIV;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSI, UT_RCC_MAX_PLL_M, UT_RCC_MAX_PLL_N, UT_RCC_MAX_PLL_P );
    FLASH->ACR = LL_FLASH_LATENCY_7;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( NULL ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_7, LL_FLASH_GetLatency() );
}


/**
 * \brief   Flash latency for PLL without P output is rejected.
 *
 * \details System clock PLL1 with P divider 0 (output disabled), then PLL source NONE.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in both cases.
 */
void Ut_Rcc_Set_FlashLatency_PllWithoutOutput_ReturnsError( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_CSI, 1u, 50u, 0u );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );

    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_NONE;
    config.Pll_Config[ RCC_PLL_1 ].P_Divider  = 2u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_FlashLatency( &config ) );
}


/**
 * \brief   Flash latency for CSI system clock and PLL from CSI.
 *
 * \details System clock CSI (4 MHz) with VOS3 active, then PLL1 from CSI
 *          (4 MHz / 1 * 50 / 2 = 100 MHz) with VOS1 active.
 *
 * \par Expected results
 * - CSI system clock: RCC_REQUEST_OK, 0 wait states.
 * - PLL from CSI: RCC_REQUEST_OK, 1 wait state (2 WS on STM32H7A3 / H7B0 / H7B3 and
 *   STM32H7R / H7S in VOS low).
 */
void Ut_Rcc_Set_FlashLatency_CsiSysClk_Sets0WaitStates( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_CSI;
    FLASH->ACR = LL_FLASH_LATENCY_7;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );

    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_CSI, 1u, 50u, 2u );
    Ut_Rcc_Set_ActiveScale( RCC_PWR_VOLTAGE_SCALE_1 );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_100MHZ_VOS1, LL_FLASH_GetLatency() );
}

/* ================================= PLL ==================================== */

/**
 * \brief   Rcc_Set_PllConfig() configures and enables PLL1.
 *
 * \details HW model sets ready flags. Configures PLL1 from HSI (M = 4, N = 25, P = 2)
 *          and reads P output frequency.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source HSI, M = 4, N = 25, P = 2.
 * - Input range 8 - 16 MHz, VCO range wide, PLL1ON set.
 * - P output 200 MHz.
 */
void Ut_Rcc_Set_PllConfig_Pll1FromHsi_ConfiguresAndEnables( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_HSI;
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLLSOURCE_HSI, LL_RCC_PLL_GetSource() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_M,   LL_RCC_PLL1_GetM() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_N,   LL_RCC_PLL1_GetN() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_DEFAULT_PLL_DIV, LL_RCC_PLL1_GetP() );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_PLLRGE_8_16, ( ( RCC->PLLCFGR & RCC_PLLCFGR_PLL1RGE ) >> RCC_PLLCFGR_PLL1RGE_Pos ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_PLLVCOSEL_WIDE,   ( ( RCC->PLLCFGR & RCC_PLLCFGR_PLL1VCOSEL ) >> RCC_PLLCFGR_PLL1VCOSEL_Pos ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_PLL1ON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutP( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, pllClk );
}


/**
 * \brief   PLL with reference frequency 1 - 2 MHz uses medium VCO range.
 *
 * \details HW model sets ready flags. PLL2 from HSI with M = 32 (2 MHz reference),
 *          N = 150 (VCO 300 MHz), then N = 300 (VCO 600 MHz - wide range not allowed).
 *
 * \par Expected results
 * - N = 150: RCC_REQUEST_OK, input range 1 - 2 MHz, VCO range medium.
 * - N = 300: RCC_REQUEST_ERROR, PLL2 stays stopped.
 */
void Ut_Rcc_Set_PllConfig_Reference2MHz_MediumVcoOnly( void )
{
    rcc_PllConfigStruct_t pllConfig;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, 32u, 150u, 2u );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_PLLRGE_1_2,  ( ( RCC->PLLCFGR & RCC_PLLCFGR_PLL2RGE ) >> RCC_PLLCFGR_PLL2RGE_Pos ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_PLLVCOSEL_MEDIUM, ( ( RCC->PLLCFGR & RCC_PLLCFGR_PLL2VCOSEL ) >> RCC_PLLCFGR_PLL2VCOSEL_Pos ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, 32u, 300u, 2u );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL2ON, RCC->CR );
}


/**
 * \brief   PLL1 Q and R output frequencies are returned.
 *
 * \details HW model sets ready flags. Configures PLL1 from HSI (Q = R = 2) and reads
 *          Q and R output frequencies into variables preset to 0. Reads kernel clock
 *          of RNG (SPI1 on STM32H7R / H7S) clocked by PLL1 Q.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, Q and R output 200 MHz (VCO 400 MHz / 2).
 * - Peripheral with PLL1 Q kernel clock reports 200 MHz.
 */
void Ut_Rcc_Get_PllClk_OutQR_Pll1FromHsi_ReturnsFrequency( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       pllClk = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.Pll_Config[ RCC_PLL_1 ].Pll_Source = RCC_PLL_SRC_HSI;
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &config.Pll_Config[ RCC_PLL_1 ] ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutQ( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, pllClk );

    pllClk = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllClk_OutR( RCC_PLL_1, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, pllClk );

    pllClk = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( UT_RCC_PERIPH_PLL1Q_KERNEL, &pllClk ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, pllClk );
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
    rcc_PllConfigStruct_t pllConfig;

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, 1u, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
}


/**
 * \brief   Rcc_Set_PllConfig() rejects invalid arguments.
 *
 * \details Calls the function with PLL out of range, with NULL configuration and with
 *          multiplier out of range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases.
 */
void Ut_Rcc_Set_PllConfig_InvalidArgs_ReturnsError( void )
{
    rcc_PllConfigStruct_t pllConfig;

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_CNT, &pllConfig ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1,   NULL ) );

    pllConfig.N_Multiplier = 1000u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
}


/**
 * \brief   Rcc_Set_PllConfig() reports PLL not locked.
 *
 * \details HSI running, without HW model (PLL1RDY never set) configures PLL1 from HSI.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Set_PllConfig_PllNotLocked_ReturnsError( void )
{
    rcc_PllConfigStruct_t pllConfig;

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;

    /* No HW model - PLL1RDY is never set */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
}


/**
 * \brief   PLL1 P output divider rule of the device line.
 *
 * \details HW model sets ready flags. PLL1 from HSI with P divider 1 and 3, PLL2 with
 *          P divider 3.
 *
 * \par Expected results
 * - STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757: even dividers only -
 *   RCC_REQUEST_ERROR for 1 and 3.
 * - Other lines: P divider 1 accepted, 3 rejected.
 * - STM32H7R / H7S: P dividers 1 and 3 accepted.
 * - PLL2 P divider 3: RCC_REQUEST_OK.
 */
void Ut_Rcc_Set_PllConfig_Pll1POddDivider_LineRule( void )
{
    rcc_PllConfigStruct_t pllConfig;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, 1u );
#if (STM32H7_DEV_ID == 0x450UL)
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
#endif

    pllConfig.P_Divider = 3u;
#if defined(STM32H7RS)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
#endif

    /* Odd divider is allowed on other PLLs */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
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


/**
 * \brief   PLL source common for all PLLs is not changed while a PLL runs.
 *
 * \details HW model sets ready flags. PLL1 configured from HSI, then PLL2 configured
 *          from HSE and source of PLL3 set to CSI. PLL1 stopped, source of PLL3 set to
 *          CSI again.
 *
 * \par Expected results
 * - PLL2 / PLL3 with other source: RCC_REQUEST_ERROR, source stays HSI.
 * - All PLLs stopped: source changed to CSI.
 */
void Ut_Rcc_Set_PllsSource_PllRunning_CommonSourceKept( void )
{
    rcc_PllConfigStruct_t pllConfig;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );

    pllConfig.Pll_Source = RCC_PLL_SRC_HSE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllsSource( RCC_PLL_3, RCC_PLL_SRC_CSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLLSOURCE_HSI, LL_RCC_PLL_GetSource() );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllInactive( RCC_PLL_1 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllsSource( RCC_PLL_3, RCC_PLL_SRC_CSI ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_PLLSOURCE_CSI, LL_RCC_PLL_GetSource() );
}


/**
 * \brief   PLL outputs S and T of STM32H7R / H7S clock the peripherals.
 *
 * \details HW model sets ready flags.
 * 1. PLL2 from HSI (VCO 400 MHz) with S divider 4 and T divider 8, kernel clocks of XSPI1
 *    (PLL2 S) and SDMMC1 (PLL2 T) read.
 * 2. PLL2 with T divider 0.
 * 3. PLL1 with T divider 2 (PLL1 has no output T), PLL3 with S divider 9.
 *
 * \par Expected results
 * 1. RCC_REQUEST_OK, PLL2SEN / PLL2TEN set, DIVS2 = 4, DIVT2 = 8, XSPI1 100 MHz, SDMMC1 50 MHz.
 * 2. RCC_REQUEST_OK, PLL2TEN cleared.
 * 3. RCC_REQUEST_ERROR, PLL1 / PLL3 not enabled.
 * - Other lines: test ignored (no outputs S / T).
 */
void Ut_Rcc_Set_PllConfig_OutputsST_KernelClocks( void )
{
#if defined(STM32H7RS)
    rcc_PllConfigStruct_t pllConfig;
    rcc_FreqHz_t          freq = 0u;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    pllConfig.S_Divider = 4u;
    pllConfig.T_Divider = 8u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_BITS_HIGH( RCC_PLLCFGR_PLL2SEN | RCC_PLLCFGR_PLL2TEN, RCC->PLLCFGR );
    TEST_ASSERT_EQUAL_UINT32( 4u, LL_RCC_PLL2_GetS() );
    TEST_ASSERT_EQUAL_UINT32( 8u, LL_RCC_PLL2_GetT() );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_XSPI1_PLL2S, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_VCO_DEFAULT_HZ / 4u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SDMMC1_PLL2T, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_VCO_DEFAULT_HZ / 8u, freq );

    /* Output disabled by divider 0 */
    pllConfig.T_Divider = 0u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );
    TEST_ASSERT_BITS_LOW( RCC_PLLCFGR_PLL2TEN, RCC->PLLCFGR );

    /* PLL1 without output T, S divider out of range */
    pllConfig.T_Divider = 2u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );

    pllConfig.T_Divider = 0u;
    pllConfig.S_Divider = 9u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_3, &pllConfig ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL3ON, RCC->CR );
#else
    TEST_IGNORE_MESSAGE( "PLL outputs S / T are available on STM32H7R / H7S only" );
#endif
}


/**
 * \brief   Wide VCO range of STM32H7R / H7S ends at 1600 MHz.
 *
 * \details HW model sets ready flags. PLL1 from HSI with M = 4 (reference 16 MHz),
 *          N = 100 (VCO 1600 MHz), then N = 101 (VCO 1616 MHz).
 *
 * \par Expected results
 * - N = 100: RCC_REQUEST_OK, wide VCO range.
 * - N = 101: RCC_REQUEST_ERROR (above wide range, not in medium range), PLL1 stopped.
 * - Other lines: test ignored (other wide VCO range).
 */
void Ut_Rcc_Set_PllConfig_WideVcoLimit_H7rs( void )
{
#if defined(STM32H7RS)
    rcc_PllConfigStruct_t pllConfig;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, 100u, UT_RCC_DEFAULT_PLL_DIV );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_PLLVCOSEL_WIDE, ( ( RCC->PLLCFGR & RCC_PLLCFGR_PLL1VCOSEL ) >> RCC_PLLCFGR_PLL1VCOSEL_Pos ) );

    pllConfig.N_Multiplier = 101u;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_BITS_LOW( RCC_CR_PLL1ON, RCC->CR );
#else
    TEST_IGNORE_MESSAGE( "VCO range 400 - 1600 MHz of STM32H7R / H7S only" );
#endif
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
 * \details System clock HSI 64 MHz, VOS3, HW model sets divider ready flag. Sets HSI
 *          divider 4, reads the divider and AHB1 clock.
 *
 * \par Expected results
 * - CR.HSIDIV = /4, divider reads back 4.
 * - AHB1 = SystemCoreClock = SystemD2Clock (not on STM32H7R / H7S) = 16 MHz, latency 0
 *   wait states.
 */
void Ut_Rcc_Set_OscDiv_Hsi64_DividerAppliedAndReported( void )
{
    rcc_OscDiv_t oscDiv = 0u;
    rcc_FreqHz_t freq   = 0u;

    Ut_Rcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_OscDiv( RCC_OSC_HSI64, 4u ) );
    TEST_ASSERT_EQUAL_HEX32( UT_LL_HSI_DIV4, RCC->CR & RCC_CR_HSIDIV );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_OscDiv( RCC_OSC_HSI64, &oscDiv ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, oscDiv );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, freq );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, SystemCoreClock );
    Ut_Rcc_Assert_AxiClk( UT_RCC_HSI_HZ / 4u );
    TEST_ASSERT_EQUAL_HEX32( LL_FLASH_LATENCY_0, LL_FLASH_GetLatency() );
}


/**
 * \brief   Oscillator divider rejects invalid values.
 *
 * \details
 * 1. Sets HSI divider 3 (not supported).
 * 2. Sets CSI divider 2 and 1 (CSI without divider), reads LSI divider.
 * 3. Calls setter / getter with oscillator out of range and getter with NULL.
 * 4. Sets HSI divider 2 without HW model (divider ready flag never set).
 *
 * \par Expected results
 * 1. RCC_REQUEST_ERROR, HSIDIV stays 0.
 * 2. CSI divider 2: RCC_REQUEST_ERROR, divider 1: RCC_REQUEST_OK, LSI divider 1.
 * 3. RCC_REQUEST_ERROR.
 * 4. RCC_REQUEST_ERROR.
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

    /* Divider ready flag never set */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_OscDiv( RCC_OSC_HSI64, 2u ) );
}

/* ============================== RTC CLOCK ================================= */

/**
 * \brief   RTC clock source is set and read back.
 *
 * \details Sets RTC clock source LSE and reads it back.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, source reads back LSE, backup domain access enabled.
 */
void Ut_Rcc_Set_RtcClkSource_Lse_SourceReadBack( void )
{
    rcc_Rtc_ClkSource_t source = RCC_RTC_CLK_SOURCE_HSE_DIV;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_RtcClkSource( RCC_RTC_CLK_SOURCE_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_RtcClkSource( &source ) );
    TEST_ASSERT_EQUAL( RCC_RTC_CLK_SOURCE_LSE, source );
    TEST_ASSERT_BITS_HIGH( PWR_CR1_DBP, PWR->CR1 );
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
 * - RCC_REQUEST_OK, CFGR.MCO1 = HSE.
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
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO1SOURCE_HSE & UT_RCC_CFGR_MCO1, RCC->CFGR & UT_RCC_CFGR_MCO1 );
}


/**
 * \brief   MCO2 output configures PC9.
 *
 * \details Sets MCO2 source SYSCLK, GPIO mock expects initialization of the pin.
 *
 * \par Expected results
 * - Gpio_Init() called with PC9, alternate function 0.
 * - RCC_REQUEST_OK, CFGR.MCO2 = SYSCLK.
 */
void Ut_Rcc_Set_ClkOutSource_Mco2SysClk_ConfiguresPc9( void )
{
    gpio_Config_t expectedGpio = { 0 };

    expectedGpio.PortId         = GPIO_PORT_C;
    expectedGpio.PinId          = GPIO_PIN_ID_9;
    expectedGpio.PinMode        = GPIO_PIN_MODE_ALTERNATE;
    expectedGpio.PinPull        = GPIO_PIN_PULL_NONE;
    expectedGpio.PinSpeed       = GPIO_PIN_SPEED_VERY_HIGH;
    expectedGpio.PinOutType     = GPIO_PIN_OUTPUT_PUSHPULL;
    expectedGpio.PinAltFunction = GPIO_ALT_FUNC_0;
    expectedGpio.PinActiveLevel = GPIO_PIN_LEVEL_HIGH;

    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO2_SYSCLK ) );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO2SOURCE_SYSCLK & UT_RCC_CFGR_MCO2, RCC->CFGR & UT_RCC_CFGR_MCO2 );
}


/**
 * \brief   Clock output rejects source of other output and LSCO.
 *
 * \details MCO2 = HSE preset. Sets MCO1 source to MCO2 output, MCO2 source to MCO1
 *          output, LSCO (not available on STM32H7) and output out of range.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR in all cases, Gpio_Init() not called.
 * - CFGR and BDCR unchanged.
 */
void Ut_Rcc_Set_ClkOutSource_SourceOfOtherOutput_ReturnsErrorWithoutChange( void )
{
    RCC->CFGR = LL_RCC_MCO2SOURCE_HSE & UT_RCC_CFGR_MCO2;

    /* No Gpio_Init expected - strict mock fails on the call */
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_MCO1_LSE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_MCO2_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_LSCO_LSI ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_CNT,  RCC_CLK_SOURCE_MCO1_HSE ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_MCO2SOURCE_HSE & UT_RCC_CFGR_MCO2, RCC->CFGR );
    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->BDCR );
}


/**
 * \brief   Clock output without source is not configured.
 *
 * \details Sets source NONE of MCO1 and MCO2, then of LSCO.
 *
 * \par Expected results
 * - MCO1 / MCO2: RCC_REQUEST_OK, Gpio_Init() not called, CFGR stays 0.
 * - LSCO: RCC_REQUEST_ERROR (not available).
 */
void Ut_Rcc_Set_ClkOutSource_None_OutputNotConfigured( void )
{
    /* No Gpio_Init expected - strict mock fails on the call */
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO1, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK,    Rcc_Set_ClkOutSource( RCC_CLK_OUT_MCO2, RCC_CLK_SOURCE_NONE ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutSource( RCC_CLK_OUT_LSCO, RCC_CLK_SOURCE_NONE ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->CFGR );
}


/**
 * \brief   All clock output sources are set and read back.
 *
 * \details Sets every source of MCO1 and MCO2 and reads it back, then calls the
 *          getter with LSCO, output out of range and with NULL pointer.
 *
 * \par Expected results
 * - Every source reads back.
 * - LSCO, output out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ClkOutSource_AllSources_ReadBack( void )
{
    const rcc_ClkOut_Id_t     outIds[]  = { RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1, RCC_CLK_OUT_MCO1,
                                            RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2, RCC_CLK_OUT_MCO2,
                                            RCC_CLK_OUT_MCO2 };
    const rcc_ClkOut_Source_t sources[] = { RCC_CLK_SOURCE_MCO1_HSI64, RCC_CLK_SOURCE_MCO1_LSE, RCC_CLK_SOURCE_MCO1_HSE,
                                            RCC_CLK_SOURCE_MCO1_PLL1Q, RCC_CLK_SOURCE_MCO1_HSI48,
                                            RCC_CLK_SOURCE_MCO2_SYSCLK, RCC_CLK_SOURCE_MCO2_PLL2P, RCC_CLK_SOURCE_MCO2_HSE,
                                            RCC_CLK_SOURCE_MCO2_PLL1P, RCC_CLK_SOURCE_MCO2_CSI, RCC_CLK_SOURCE_MCO2_LSI };
    rcc_ClkOut_Source_t       source    = RCC_CLK_SOURCE_NONE;

    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );

    for( uint32_t idx = 0u; ( sizeof( sources ) / sizeof( sources[ 0u ] ) ) > idx; idx++ )
    {
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutSource( outIds[ idx ], sources[ idx ] ) );
        TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutSource( outIds[ idx ], &source ) );
        TEST_ASSERT_EQUAL( sources[ idx ], source );
    }

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_LSCO, &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_CNT,  &source ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutSource( RCC_CLK_OUT_MCO1, NULL ) );
}


/**
 * \brief   Clock output divider is written and read back.
 *
 * \details MCO1 divider 4, MCO2 divider 15, divider 0 and 16, LSCO and invalid output.
 *
 * \par Expected results
 * - Rcc_Get_ClkOutDivider() returns the divider, MCO1PRE = 4.
 * - Divider 0 / 16, LSCO, invalid output: RCC_REQUEST_ERROR, divider unchanged.
 */
void Ut_Rcc_Set_ClkOutDivider_ReadBack( void )
{
    rcc_ClkOut_Div_t divider = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 4u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO2, 15u ) );
    TEST_ASSERT_EQUAL_HEX32( 4u << RCC_CFGR_MCO1PRE_Pos, RCC->CFGR & RCC_CFGR_MCO1PRE );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO2, &divider ) );
    TEST_ASSERT_EQUAL_UINT32( 15u, divider );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 0u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_MCO1, 16u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ClkOutDivider( RCC_CLK_OUT_LSCO, 1u ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_LSCO, &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_CNT,  &divider ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkOutDivider( RCC_CLK_OUT_MCO1, NULL ) );
    TEST_ASSERT_EQUAL_HEX32( 4u << RCC_CFGR_MCO1PRE_Pos, RCC->CFGR & RCC_CFGR_MCO1PRE );
}

/* ============================ RESET SOURCE ================================ */

/**
 * \brief   Rcc_Get_ResetSource() reads reset flags.
 *
 * \details RSR = IWDG1 and PIN reset flags. Reads IWDG, PIN and BOR reset source, then
 *          calls the function with source out of range and with NULL pointer.
 *
 * \par Expected results
 * - IWDG, PIN: RCC_FLAG_ACTIVE, BOR: RCC_FLAG_INACTIVE.
 * - Source out of range, NULL pointer: RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_ResetSource_ReadsFlags( void )
{
    rcc_FlagState_t flag = RCC_FLAG_INACTIVE;

    RCC->RSR = UT_RCC_RSR_IWDGRSTF | RCC_RSR_PINRSTF;

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
 * \details RSR = IWDG1 and PIN reset flags, HW model removes flags while RMVF is set.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, RSR = 0 (flags removed, RMVF released).
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsRemoved_ReturnsOkAndReleasesRmvf( void )
{
    RCC->RSR = UT_RCC_RSR_IWDGRSTF | RCC_RSR_PINRSTF;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ResetSourceClear() );

    TEST_ASSERT_EQUAL_HEX32( 0u, RCC->RSR );
}


/**
 * \brief   Rcc_Set_ResetSourceClear() reports flags not removed.
 *
 * \details RSR = IWDG1 reset flag, without HW model (flags are not removed).
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, RMVF is released.
 */
void Ut_Rcc_Set_ResetSourceClear_FlagsStay_ReturnsErrorAndReleasesRmvf( void )
{
    RCC->RSR = UT_RCC_RSR_IWDGRSTF;    /* No HW model - flags are not removed */

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Set_ResetSourceClear() );
    TEST_ASSERT_BITS_LOW( RCC_RSR_RMVF, RCC->RSR );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Rcc_Init() with default configuration keeps system clock on HSI.
 *
 * \details HW model sets ready flags and switch status. Initializes the module with
 *          default configuration (no clock output - Gpio_Init() not called).
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS = HSI, HSE and PLLs stopped, VOS3 (VOS low) kept, supply not written.
 * - Latency for 64 MHz in VOS3 of the device line.
 * - SystemCoreClock = SystemD2Clock = 64 MHz, SysTick interval 1 ms.
 */
void Ut_Rcc_Init_DefaultConfig_SysClkFromHsi( void )
{
    rcc_ConfigStruct_t config;
    rcc_Time_ms_t      interval = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_RESET;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSI, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEON | RCC_CR_PLL1ON | RCC_CR_PLL2ON | RCC_CR_PLL3ON, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_VOS_RESET, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_SUPPLY_RESET, UT_PWR_SUPPLY_REG & ~UT_PWR_SUPPLY_MODEL_FLAGS );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_HSI_VOS3, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, SystemCoreClock );
    Ut_Rcc_Assert_AxiClk( UT_RCC_HSI_HZ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_SysTickInterval( &interval ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, interval );
}


/**
 * \brief   Rcc_Init() switches system clock to PLL1 from HSE with LDO supply and VOS1.
 *
 * \details HW model sets ready flags. Configuration: LDO supply, VOS1, HSE crystal
 *          8 MHz, PLL1 from HSE (M = 1, N = 50, P = 2), PLL2 / PLL3 stopped, APB
 *          dividers 2, MCO1 = PLL1 Q / 4.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, SWS = PLL1, HSE and PLL1 enabled, supply LDO, VOS1.
 * - Latency for 200 MHz in VOS1 of the device line.
 * - SystemCoreClock = SystemD2Clock = 200 MHz, APB1 = 100 MHz.
 * - MCO1 source PLL1 Q, divider 4, pin configured.
 */
void Ut_Rcc_Init_PllFromHse_LdoScale1( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.PowerSupply       = RCC_PWR_SUPPLY_LDO;
    config.VoltageScaling    = RCC_PWR_VOLTAGE_SCALE_1;
    config.HSE_ClockType     = RCC_HSE_TYPE_CRYSTAL;
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    config.APB1_Divider      = RCC_APB1_DIVIDER_2;
    config.APB2_Divider      = RCC_APB2_DIVIDER_2;
#if defined(STM32H7RS)
    config.APB5_Divider      = RCC_APB5_DIVIDER_2;
#else
    config.APB3_Divider      = RCC_APB3_DIVIDER_2;
#endif
    config.APB4_Divider      = RCC_APB4_DIVIDER_2;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSE, 1u, 50u, 2u );
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockSource  = RCC_CLK_SOURCE_MCO1_PLL1Q;
    config.McoConfig[ RCC_CLK_OUT_MCO1 ].ClockDivider = 4u;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_RESET;
    Gpio_Init_IgnoreAndReturn( GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_PLL1, RCC->CFGR & RCC_CFGR_SWS );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_PLL1ON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEBYP | RCC_CR_PLL2ON | RCC_CR_PLL3ON, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_LDO_SUPPLY, LL_PWR_GetSupply() );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_REGU_VOLTAGE_SCALE1, UT_PWR_VOS_REG & UT_PWR_VOS_MSK );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_200MHZ_VOS1, LL_FLASH_GetLatency() );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_HSE_SYSCLK_HZ, SystemCoreClock );
    Ut_Rcc_Assert_AxiClk( UT_RCC_PLL_HSE_SYSCLK_HZ );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB1_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_HSE_SYSCLK_HZ / 2u, freq );

    TEST_ASSERT_EQUAL_HEX32( UT_LL_MCO1SOURCE_PLL1Q & UT_RCC_CFGR_MCO1, RCC->CFGR & UT_RCC_CFGR_MCO1 );
    TEST_ASSERT_EQUAL_HEX32( 4u << RCC_CFGR_MCO1PRE_Pos, RCC->CFGR & RCC_CFGR_MCO1PRE );
}


/**
 * \brief   Rcc_Init() stops when oscillator does not start.
 *
 * \details HSI preset as running, without HW model (HSE never becomes ready).
 *          Initializes the module with PLL1 from HSE.
 *
 * \par Expected results
 * - RCC_REQUEST_ERROR, system clock stays HSI, PLL1 is not enabled.
 */
void Ut_Rcc_Init_HseNotStarting_ReturnsErrorBeforeSysClkSwitch( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType     = RCC_HSE_TYPE_CRYSTAL;
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSE, 1u, 50u, 2u );

    /* HSI running, but HSE never becomes ready */
    RCC->CR = RCC_CR_HSION | RCC_CR_HSIRDY;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );

    TEST_ASSERT_BITS_LOW( RCC_CFGR_SW, RCC->CFGR );
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


/**
 * \brief   Rcc_Init() enables HSE clock security system.
 *
 * \details HW model sets ready flags.
 * 1. Default configuration with CSS enabled and HSE not used.
 * 2. HSE crystal 8 MHz with CSS enabled.
 * 3. Rcc_Init() repeated with HSE crystal.
 *
 * \par Expected results
 * 1. RCC_REQUEST_ERROR (CSS needs HSE), CSSHSEON not set.
 * 2. RCC_REQUEST_OK, CSSHSEON set.
 * 3. RCC_REQUEST_OK, HSE supervised by CSS kept running.
 */
void Ut_Rcc_Init_CssEnabled_RequiresHse( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    config.CSS_Enable = RCC_FUNCTION_ACTIVE;

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_LOW( UT_RCC_CR_CSSHSEON, RCC->CR );

    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( UT_RCC_CR_CSSHSEON | RCC_CR_HSEON, RCC->CR );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( UT_RCC_CR_CSSHSEON | RCC_CR_HSEON | RCC_CR_HSERDY, RCC->CR );
}


/**
 * \brief   Rcc_Init() applies workarounds of the device reset state.
 *
 * \details HW model sets ready flags, default configuration.
 * 1. Revision Y device (STM32H742 / H743 / H745 / H747 / H750 / H753 / H755 / H757 only).
 * 2. Revision V device.
 * 3. FMC clock enabled by application before Rcc_Init().
 *
 * \par Expected results
 * 1. AXI_TARG7_FN_MOD = 1 (other lines: not written), FMC bank 1 disabled, FMC clock
 *    disabled again (STM32H7R / H7S: FMC bank 1 and FMC clock not changed).
 * 2. AXI_TARG7_FN_MOD not written.
 * 3. FMC bank 1 configuration kept (not tested on STM32H7R / H7S).
 */
void Ut_Rcc_Init_DeviceWorkarounds_AxiAndFmc( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    DBGMCU->IDCODE = UT_RCC_IDCODE_REV_Y;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
#if (STM32H7_DEV_ID == 0x450UL)
    TEST_ASSERT_EQUAL_HEX32( 1u, UT_RCC_AXI_TARG7_FN_MOD );
#else
    TEST_ASSERT_EQUAL_HEX32( 0u, UT_RCC_AXI_TARG7_FN_MOD );
#endif
#if defined(RCC_AHB3ENR_FMCEN)
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_FMC_BCR1_DISABLED, FMC_Bank1_R->BTCR[ 0u ] );
    TEST_ASSERT_BITS_LOW( RCC_AHB3ENR_FMCEN, RCC->AHB3ENR );
#else
    TEST_ASSERT_EQUAL_HEX32( 0u, FMC_Bank1_R->BTCR[ 0u ] );
    TEST_ASSERT_BITS_LOW( RCC_AHB5ENR_FMCEN, RCC->AHB5ENR );
#endif

    UT_RCC_AXI_TARG7_FN_MOD = 0u;
    DBGMCU->IDCODE          = UT_RCC_IDCODE_REV_V;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, UT_RCC_AXI_TARG7_FN_MOD );

#if defined(RCC_AHB3ENR_FMCEN)
    /* FMC used by application */
    FMC_Bank1_R->BTCR[ 0u ] = 0u;
    RCC->AHB3ENR            = RCC_AHB3ENR_FMCEN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, FMC_Bank1_R->BTCR[ 0u ] );
    TEST_ASSERT_BITS_HIGH( RCC_AHB3ENR_FMCEN, RCC->AHB3ENR );
#endif
}


/**
 * \brief   Locked supply configuration is accepted only if it matches.
 *
 * \details HW model sets ready flags. Supply configured as LDO and locked (written after
 *          power-on reset). Rcc_Init() with external source supply, then with LDO supply.
 *
 * \par Expected results
 * - External source: RCC_REQUEST_ERROR, PWR_CR3 unchanged.
 * - LDO: RCC_REQUEST_OK, PWR_CR3 unchanged.
 */
void Ut_Rcc_Init_SupplyLocked_OnlyMatchingAccepted( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_LDOEN;   /* Locked LDO supply */

    config.PowerSupply = RCC_PWR_SUPPLY_EXTERNAL_SOURCE;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_SUPPLY_LDOEN, UT_PWR_SUPPLY_REG & ~UT_PWR_SUPPLY_MODEL_FLAGS );

    config.PowerSupply = RCC_PWR_SUPPLY_LDO;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_SUPPLY_LDOEN, UT_PWR_SUPPLY_REG & ~UT_PWR_SUPPLY_MODEL_FLAGS );
}


/**
 * \brief   SMPS supply configuration depends on the device.
 *
 * \details HW model sets ready flags, supply not configured yet.
 *  - Devices with SMPS: direct SMPS supply, then (new reset state) SMPS 1.8 V supplying
 *    external circuits.
 *  - Devices without SMPS: direct SMPS supply.
 *  - All devices: supply out of range.
 *
 * \par Expected results
 * - SMPS devices: RCC_REQUEST_OK, PWR_CR3 supply = requested configuration.
 * - STM32H7R / H7S: SMPS 2.5 V supply rejected.
 * - Devices without SMPS, supply out of range: RCC_REQUEST_ERROR, PWR_CR3 unchanged.
 */
void Ut_Rcc_Init_SmpsSupply_DeviceDependent( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_RESET;

    config.PowerSupply = RCC_PWR_SUPPLY_DIRECT_SMPS;
#if defined(UT_PWR_HAS_SMPS)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_DIRECT_SMPS_SUPPLY, LL_PWR_GetSupply() );

    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_RESET;
    config.PowerSupply = RCC_PWR_SUPPLY_SMPS_1V8_EXT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( LL_PWR_SMPS_1V8_SUPPLIES_EXT, LL_PWR_GetSupply() );
#if defined(STM32H7RS)
    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_RESET;
    config.PowerSupply = RCC_PWR_SUPPLY_SMPS_2V5_LDO;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_SUPPLY_RESET, UT_PWR_SUPPLY_REG & ~UT_PWR_SUPPLY_MODEL_FLAGS );
#endif
#else
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_SUPPLY_RESET, UT_PWR_SUPPLY_REG );
#endif

    UT_PWR_SUPPLY_REG = UT_PWR_SUPPLY_RESET;
    config.PowerSupply = RCC_PWR_SUPPLY_CNT;
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Init( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_PWR_SUPPLY_RESET, UT_PWR_SUPPLY_REG & ~UT_PWR_SUPPLY_MODEL_FLAGS );
}

/**
 * \brief   Rcc_Init() configures HSE input type.
 *
 * \details HW model sets ready flags. Rcc_Init() with HSE digital input, analog input
 *          and crystal.
 *
 * \par Expected results
 * - Digital input: HSEBYP set, HSEEXT set (devices with HSEEXT).
 * - Analog input: HSEBYP set, HSEEXT cleared.
 * - Crystal: HSEBYP cleared (bypass of the previous configuration not kept).
 */
void Ut_Rcc_Init_HseType_BypassConfigured( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    config.HSE_ClockType = RCC_HSE_TYPE_SIG_DIGITAL_IN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_HSEBYP, RCC->CR );
#if defined(RCC_CR_HSEEXT)
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEEXT, RCC->CR );
#endif

    config.HSE_ClockType = RCC_HSE_TYPE_SIG_ANALOG_IN;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON | RCC_CR_HSEBYP, RCC->CR );
#if defined(RCC_CR_HSEEXT)
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEEXT, RCC->CR );
#endif

    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );
    TEST_ASSERT_BITS_HIGH( RCC_CR_HSEON, RCC->CR );
    TEST_ASSERT_BITS_LOW( RCC_CR_HSEBYP, RCC->CR );
}


/**
 * \brief   Flash latency for PLL from HSE is calculated from HSE frequency of the
 *          configuration.
 *
 * \details PLL1 from HSE 8 MHz (M = 1, N = 50, P = 2 - 200 MHz), VOS1 active.
 *
 * \par Expected results
 * - RCC_REQUEST_OK, latency for 200 MHz in VOS1 of the device line.
 */
void Ut_Rcc_Set_FlashLatency_PllFromHse_LineLatency( void )
{
    rcc_ConfigStruct_t config;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSE, 1u, 50u, 2u );
    Ut_Rcc_Set_ActiveScale( RCC_PWR_VOLTAGE_SCALE_1 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_FlashLatency( &config ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_WS_200MHZ_VOS1, LL_FLASH_GetLatency() );
}

/**
 * \brief   Trace clock (TRACECLKIN) follows the system clock multiplexer.
 *
 * \details HW model sets ready flags.
 * 1. System clock HSI with HSI divider 2 and system clock prescaler 2.
 * 2. PLL1 from HSI (VCO 400 MHz, P = 2, R = 4) selected as system clock, AHB prescaler 4.
 *
 * \par Expected results
 * 1. Trace clock = SYSCLK = 32 MHz (not divided by the system clock prescaler).
 * 2. Trace clock = PLL1 R output = 100 MHz (SYSCLK = PLL1 P = 200 MHz).
 */
void Ut_Rcc_Get_PeriphClk_TraceClk_FollowsSysClkSource( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    Ut_Rcc_Set_SysClkHsi();
    RCC->CR |= UT_LL_HSI_DIV2;
    LL_RCC_SetSysPrescaler( LL_RCC_SYSCLK_DIV_2 );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TRACE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.SystemClockSource = RCC_SYSTEM_CLOCK_SOURCE_PLL;
    Ut_Rcc_Set_PllConfig( &config.Pll_Config[ RCC_PLL_1 ], RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    config.Pll_Config[ RCC_PLL_1 ].R_Divider = 4u;
    config.AHB_Divider = RCC_AHB_DIVIDER_4;    /* AXI clock 50 MHz allowed in VOS3 of every line */
    RCC->CR &= ~RCC_CR_HSIDIV;      /* HSI 64 MHz as PLL input */
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_TRACE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_VCO_DEFAULT_HZ / 4u, freq );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, SystemCoreClock );
}

/* ============================ COVERAGE COMPLETION ========================= */

/**
 * \brief   Bus dividers are read back for every bus.
 *
 * \details HSI system clock (64 MHz), AHB /2, APB1 /4, APB2 /8, APB3 (APB5 on STM32H7R /
 *          H7S) /16, APB4 /2.
 *
 * \par Expected results
 * - Rcc_Get_ClkBusDivider() returns the dividers of every bus, invalid bus rejected.
 * - APB2 clock 4 MHz, APB3 (APB5) clock 2 MHz, APB4 clock 16 MHz, AHB3 / AHB4 (AHB5)
 *   clock 32 MHz.
 */
void Ut_Rcc_Get_ClkBusDivider_AllBuses_ReadBack( void )
{
    rcc_ClkBusDiv_t divider = 0u;
    rcc_FreqHz_t    freq    = 0u;

    Ut_Rcc_Set_SysClkHsi();
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_AHB3,   RCC_AHB_DIVIDER_2 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB1_2, RCC_APB1_DIVIDER_4 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB2,   RCC_APB2_DIVIDER_8 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( UT_RCC_CLK_BUS_APB3_5, UT_RCC_APB3_5_DIVIDER_16 ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_ClkBusDivider( RCC_CLK_BUS_APB4,   RCC_APB4_DIVIDER_2 ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB4, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB_DIVIDER_2, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB1_1, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB1_DIVIDER_4, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB2, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB2_DIVIDER_8, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( UT_RCC_CLK_BUS_APB3_5, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( UT_RCC_APB3_5_DIVIDER_16, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_APB4, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_APB4_DIVIDER_2, divider );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_CNT, &divider ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 16u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( UT_RCC_CLK_BUS_APB3_5, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 32u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_APB4, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 4u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB3, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB4, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
#if defined(STM32H7RS)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB5, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ / 2u, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusDivider( RCC_CLK_BUS_AHB5, &divider ) );
    TEST_ASSERT_EQUAL_HEX32( RCC_AHB_DIVIDER_2, divider );
#endif
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_ClkBusClk( RCC_CLK_BUS_CNT, &freq ) );
}


/**
 * \brief   PLL internal (VCO) frequency and PLL2 outputs are reported.
 *
 * \details PLL1 and PLL2 configured from HSI 64 MHz / 4 * 25, outputs / 2.
 *
 * \par Expected results
 * - Rcc_Get_PllInternalClk(): 400 MHz for PLL1 and PLL2.
 * - Kernel clock of LPTIM1 (PLL2 P), USART1 (PLL2 Q), ADC (PLL2 P) and FMC (PLL2 R): 200 MHz.
 */
void Ut_Rcc_Get_PllInternalClk_Pll2OutputsAsKernelClock( void )
{
    rcc_PllConfigStruct_t pllConfig;
    rcc_FreqHz_t          freq = 0u;

    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );

    Ut_Rcc_Set_PllConfig( &pllConfig, RCC_PLL_SRC_HSI, UT_RCC_DEFAULT_PLL_M, UT_RCC_DEFAULT_PLL_N, UT_RCC_DEFAULT_PLL_DIV );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_1, &pllConfig ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Set_PllConfig( RCC_PLL_2, &pllConfig ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_1, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_VCO_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PllInternalClk( RCC_PLL_2, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_VCO_DEFAULT_HZ, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPTIM1_PLL2P, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_PLL2Q, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_ADC12_PLL2P, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_FMC_PLL2R, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_PLL_OUT_DEFAULT_HZ, freq );
}


/**
 * \brief   Kernel clock of oscillator sources is reported.
 *
 * \details Kernel clocks of USART1 (CSI), RNG (HSI48), RTC (LSI, LSE), HDMI-CEC
 *          (CSI / 122, devices with HDMI-CEC) and peripheral clock CKPER (HSI).
 *
 * \par Expected results
 * - CSI_VALUE, HSI48_VALUE, LSI_VALUE, LSE_VALUE, CSI_VALUE / 122, HSI_VALUE.
 */
void Ut_Rcc_Get_PeriphClk_OscillatorSources_NominalFrequency( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_USART1_CSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( CSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RNG_HSI48, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( HSI48_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_LSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSI_VALUE, freq );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_LSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( LSE_VALUE, freq );
#if defined(UT_RCC_HAS_CEC)
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_CEC_CSI_DIV122, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( CSI_VALUE / 122u, freq );
#endif
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPCLK_HSI, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( HSI_VALUE, freq );
}


/**
 * \brief   Kernel clock from peripheral clock CKPER follows the CKPER selection.
 *
 * \details SPI1 clocked by CKPER: CKPER selection HSI (reset value), then CSI. SPI1
 *          clocked by external input (I2S_CKIN).
 *
 * \par Expected results
 * - CKPER HSI: 64 MHz, CKPER CSI: CSI_VALUE.
 * - External input: RCC_REQUEST_ERROR (frequency unknown).
 */
void Ut_Rcc_Get_PeriphClk_PerClkAndPinClk_Reported( void )
{
    rcc_FreqHz_t freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SPI1_LPCLK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSI_HZ, freq );

    LL_RCC_SetCLKPClockSource( LL_RCC_CLKP_CLKSOURCE_CSI );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_SPI1_LPCLK, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( CSI_VALUE, freq );

    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_SPI1_PIN, &freq ) );
}


/**
 * \brief   Kernel clock of HSE follows the HSE frequency of Rcc_Init().
 *
 * \details Default configuration with HSE crystal 8 MHz initialized. Kernel clock of
 *          CKPER (HSE) read, RTC HSE clock read with RTCPRE 8 and 1.
 *
 * \par Expected results
 * - CKPER HSE: 8 MHz.
 * - RTC HSE / 8: 1 MHz, RTCPRE 1 (no clock): RCC_REQUEST_ERROR.
 */
void Ut_Rcc_Get_PeriphClk_HseSource_InitFrequency( void )
{
    rcc_ConfigStruct_t config;
    rcc_FreqHz_t       freq = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_DefaultConfig( &config ) );
    config.HSE_ClockType = RCC_HSE_TYPE_CRYSTAL;
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Set_ModelActive( Ut_Rcc_HwModel ) );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Init( &config ) );

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_LPCLK_HSE, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ, freq );

    LL_RCC_SetRTC_HSEPrescaler( UT_RCC_RTC_HSE_DIV << RCC_CFGR_RTCPRE_Pos );
    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV, &freq ) );
    TEST_ASSERT_EQUAL_UINT32( UT_RCC_HSE_DEFAULT_HZ / UT_RCC_RTC_HSE_DIV, freq );

    LL_RCC_SetRTC_HSEPrescaler( 1u << RCC_CFGR_RTCPRE_Pos );
    TEST_ASSERT_EQUAL( RCC_REQUEST_ERROR, Rcc_Get_PeriphClk( RCC_PERIPH_RTC_HSE_DIV, &freq ) );
}


/**
 * \brief   Rcc_Deinit() and Rcc_Task() do not change the clock configuration.
 *
 * \details HSI system clock preset, Rcc_Deinit() with NULL and Rcc_Task() called.
 *
 * \par Expected results
 * - CR and CFGR not changed.
 */
void Ut_Rcc_Deinit_Task_NoClockChange( void )
{
    Ut_Rcc_Set_SysClkHsi();

    Rcc_Deinit( NULL );
    Rcc_Task();

    TEST_ASSERT_EQUAL_HEX32( RCC_CR_HSION | RCC_CR_HSIRDY, RCC->CR );
    TEST_ASSERT_EQUAL_HEX32( LL_RCC_SYS_CLKSOURCE_STATUS_HSI, RCC->CFGR );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief HW model of RCC and PWR - runs in background thread.
 *
 * Ready flags follow enable bits, SWS follows SW, HSI divider is always ready,
 * active voltage scale follows the requested scale and is always ready, SMPS
 * external supply is always ready, reset flags are removed while RMVF is set.
 * STM32H7R / H7S: active voltage scale in PWR_SR1, SMPS external supply ready in
 * PWR_CSR2.
 */
static void Ut_Rcc_HwModel( void )
{
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSION,   RCC_CR_HSIRDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSEON,   RCC_CR_HSERDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_CSION,   RCC_CR_CSIRDY   );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_HSI48ON, RCC_CR_HSI48RDY );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL1ON,  RCC_CR_PLL1RDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL2ON,  RCC_CR_PLL2RDY  );
    Ut_Rcc_Mirror( &RCC->CR, RCC_CR_PLL3ON,  RCC_CR_PLL3RDY  );
    Ut_Rcc_Mirror( &RCC->BDCR, RCC_BDCR_LSEON, RCC_BDCR_LSERDY );
    Ut_Rcc_Mirror( &RCC->CSR,  RCC_CSR_LSION,  RCC_CSR_LSIRDY  );

    (void)__atomic_or_fetch( &RCC->CR, RCC_CR_HSIDIVF, __ATOMIC_SEQ_CST );

    /* System clock switch status follows the request */
    const uint32_t cfgr = RCC->CFGR;
    const uint32_t sws  = ( ( cfgr & RCC_CFGR_SW ) >> RCC_CFGR_SW_Pos ) << RCC_CFGR_SWS_Pos;

    if( sws != ( cfgr & RCC_CFGR_SWS ) )
    {
        (void)__atomic_and_fetch( &RCC->CFGR, ~RCC_CFGR_SWS, __ATOMIC_SEQ_CST );
        (void)__atomic_or_fetch( &RCC->CFGR, sws, __ATOMIC_SEQ_CST );
    }
    else
    {
        /* Switch done */
    }

    /* Active voltage scale follows the requested scale (same field position) */
    const uint32_t actVos = UT_PWR_ACTVOS_REG;
    const uint32_t reqVos = UT_PWR_VOS_REG & UT_PWR_VOS_MSK;

    if( ( actVos & UT_PWR_ACTVOS_MSK ) != reqVos )
    {
        (void)__atomic_and_fetch( &UT_PWR_ACTVOS_REG, ~UT_PWR_ACTVOS_MSK, __ATOMIC_SEQ_CST );
        (void)__atomic_or_fetch( &UT_PWR_ACTVOS_REG, reqVos, __ATOMIC_SEQ_CST );
    }
    else
    {
        /* Voltage scale reached */
    }

    (void)__atomic_or_fetch( &UT_PWR_ACTVOS_REG, UT_PWR_ACTVOSRDY, __ATOMIC_SEQ_CST );
#if defined(UT_PWR_HAS_SMPS)
    (void)__atomic_or_fetch( &UT_PWR_SUPPLY_REG, UT_PWR_SUPPLY_MODEL_FLAGS, __ATOMIC_SEQ_CST );
#endif

    if( 0u != ( RCC->RSR & RCC_RSR_RMVF ) )
    {
        (void)__atomic_and_fetch( &RCC->RSR, RCC_RSR_RMVF, __ATOMIC_SEQ_CST );
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
    RCC->CR   = RCC_CR_HSION | RCC_CR_HSIRDY;
    RCC->CFGR = LL_RCC_SYS_CLKSOURCE_STATUS_HSI;
}


/** Emulates active voltage scale (VOS0 is VOS1 with SYSCFG overdrive on devices with overdrive,
 *  STM32H7R / H7S: scale 0 - VOS high, other scales - VOS low) */
static void Ut_Rcc_Set_ActiveScale( rcc_PwrVoltageScale_t voltageScale )
{
    static const uint32_t scaleLut[ RCC_PWR_VOLTAGE_SCALE_CNT ] =
    {
#if defined(STM32H7RS)
        LL_PWR_REGU_VOLTAGE_SCALE0, LL_PWR_REGU_VOLTAGE_SCALE1, LL_PWR_REGU_VOLTAGE_SCALE1, LL_PWR_REGU_VOLTAGE_SCALE1
#else
        LL_PWR_REGU_VOLTAGE_SCALE0, LL_PWR_REGU_VOLTAGE_SCALE1, LL_PWR_REGU_VOLTAGE_SCALE2, LL_PWR_REGU_VOLTAGE_SCALE3
#endif
    };

    UT_PWR_VOS_REG    = scaleLut[ voltageScale ];
    UT_PWR_ACTVOS_REG = scaleLut[ voltageScale ];
#if defined(SYSCFG_PWRCR_ODEN)
    SYSCFG->PWRCR  = ( RCC_PWR_VOLTAGE_SCALE_0 == voltageScale ) ? SYSCFG_PWRCR_ODEN : 0u;
#endif
}


/** Fills PLL configuration (Q and R dividers 2) */
static void Ut_Rcc_Set_PllConfig( rcc_PllConfigStruct_t * const pllConfig, rcc_PllClkSrc_t source,
                                  uint32_t mDiv, uint32_t nMult, uint32_t pDiv )
{
    pllConfig->Pll_Source   = source;
    pllConfig->M_Divider    = mDiv;
    pllConfig->N_Multiplier = nMult;
    pllConfig->P_Divider    = pDiv;
    pllConfig->Q_Divider    = UT_RCC_DEFAULT_PLL_DIV;
    pllConfig->R_Divider    = UT_RCC_DEFAULT_PLL_DIV;
#if defined(STM32H7RS)
    pllConfig->S_Divider    = 0u;
    pllConfig->T_Divider    = 0u;
#endif
}


/** Checks the AXI clock (HCLK) - CMSIS variable SystemD2Clock, on STM32H7R / H7S (without
 *  SystemD2Clock) the AHB clock reported by the module */
static void Ut_Rcc_Assert_AxiClk( rcc_FreqHz_t expectedHz )
{
#if defined(STM32H7RS)
    rcc_FreqHz_t ahbHz = 0u;

    TEST_ASSERT_EQUAL( RCC_REQUEST_OK, Rcc_Get_ClkBusClk( RCC_CLK_BUS_AHB1, &ahbHz ) );
    TEST_ASSERT_EQUAL_UINT32( expectedHz, ahbHz );
#else
    TEST_ASSERT_EQUAL_UINT32( expectedHz, SystemD2Clock );
#endif
}
