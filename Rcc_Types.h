/**
 * \author Mr.Nobody
 * \file Rcc_Types.h
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module global types definition
 *
 * This file contains the types definitions used across the module and are
 * available for other modules through Port file.
 *
 * \note STM32L4 / STM32L4+ family - availability of peripherals, PLLs and clock
 *       sources depends on the selected MCU. Enumerators are guarded by RCC
 *       register bit definitions of the device header (e.g. RCC_APB1ENR1_TIM3EN),
 *       so only items existing on the selected MCU are available.
 *
 */

#ifndef RCC_RCC_TYPES_H
#define RCC_RCC_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
#include "Stm32_rcc.h"                      /* RCC utilities functionality    */
#include "Stm32_system.h"                   /* System utilities functionality */
#include "Stm32_pwr.h"                      /* PWR utilities functionality    */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define RCC_NULL_PTR                        ( ( void* ) 0u )

#if defined(RCC_APB1ENR1_USBFSEN) || \
    defined(RCC_AHB2ENR_OTGFSEN)
/** USB peripheral (USB full speed device or USB OTG full speed) is available */
#define RCC_TYPES_USB_SUPPORT
#endif /* RCC_APB1ENR1_USBFSEN || RCC_AHB2ENR_OTGFSEN */

#if defined(RCC_APB2ENR_SDMMC1EN) || \
    defined(RCC_AHB2ENR_SDMMC1EN)
/** SD / MMC card interface 1 is available (APB2 on STM32L4, AHB2 on STM32L4+) */
#define RCC_TYPES_SDMMC1_SUPPORT
#endif /* RCC_APB2ENR_SDMMC1EN || RCC_AHB2ENR_SDMMC1EN */

#if defined(RCC_PLLSAI2CFGR_PLLSAI2R) && \
    !defined(DMAMUX1)
/** PLLSAI2 output R can be selected as ADC clock (STM32L47x / L48x / L49x / L4A6) */
#define RCC_TYPES_ADC_PLLSAI2R_SUPPORT
#endif /* RCC_PLLSAI2CFGR_PLLSAI2R && !DMAMUX1 */

/* ========================== EXPORTED MACROS =============================== */

/* ============================== TYPEDEFS ================================== */

/** \brief Type signaling major version of SW module */
typedef uint8_t rcc_MajorVersion_t;


/** \brief Type signaling minor version of SW module */
typedef uint8_t rcc_MinorVersion_t;


/** \brief Type signaling patch version of SW module */
typedef uint8_t rcc_PatchVersion_t;


/** \brief Type signaling actual version of SW module */
typedef struct
{
    rcc_MajorVersion_t Major; /**< Major version */
    rcc_MinorVersion_t Minor; /**< Minor version */
    rcc_PatchVersion_t Patch; /**< Patch version */
}   rcc_ModuleVersion_t;


/** Function status enumeration */
typedef enum
{
    RCC_FUNCTION_INACTIVE = 0u, /**< Function status is inactive */
    RCC_FUNCTION_ACTIVE         /**< Function status is active   */
}   rcc_FunctionState_t;


/** Enumeration used to signal request processing state */
typedef enum
{
    RCC_REQUEST_ERROR = 0u, /**< Processing request failed  */
    RCC_REQUEST_OK          /**< Processing request succeed */
}   rcc_RequestState_t;


/** Flag states enumeration */
typedef enum
{
    RCC_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    RCC_FLAG_ACTIVE         /**< Active flag state   */
}   rcc_FlagState_t;


/** Frequency values type represented in Hz */
typedef uint32_t rcc_FreqHz_t;


/** \brief Type used to signal time values
 *
 * Use this datatype for time values, where the following characteristics
 * are sufficent:
 *  - Range of values : 0 ms to 49 days in mili-seconds [0.001 s]
 *  - Offset          : 0 ms
 *  - Step size       : 1 ms
 */
typedef uint32_t rcc_Time_ms_t;


/** \brief Enumeration of all peripheral IDs with corresponding clock sources.
 *
 * Peripherals with selectable kernel clock (clock multiplexer) have one
 * enumerator per clock source (e.g. \c RCC_PERIPH_USART2_HSI), the other
 * peripherals have single enumerator.
 *
 * \note Some peripherals have common clock source. USB, RNG and SDMMC share
 *       the 48 MHz clock multiplexer (CLK48SEL) - a change of the clock source
 *       of one of them changes the clock of the others. Clock source of a
 *       peripheral can be changed only after the peripheral was deactivated
 *       (the multiplexer is returned to its reset selection).
 * \note ADC1, ADC2 and ADC3 share one clock enable and one reset control.
 */
typedef enum rcc_PeriphId_t
{

    /*------------------------------ System core -------------------------------*/

    RCC_PERIPH_FLASH          , /**< Flash memory interface clock enable */
    RCC_PERIPH_SYSCFG         , /**< System configuration controller (SYSCFG, COMP, VREFBUF) clock enable */
    RCC_PERIPH_PWR            , /**< Power interface clock enable */
#if defined(RCC_APB2ENR_FWEN)
    RCC_PERIPH_FW             , /**< Firewall clock enable (can not be disabled by software) */
#endif
    RCC_PERIPH_SYSTICK        , /**< System Tick timer clocked by processor clock (HCLK), no clock enable */
    RCC_PERIPH_IWDG           , /**< Independent watchdog clocked by Low Speed Internal (LSI) oscillator, no clock enable */
    RCC_PERIPH_RTC_HSE_DIV32  , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source */
    RCC_PERIPH_RTC_LSE        , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_PERIPH_RTC_LSI        , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */
#if defined(RCC_APB1ENR1_RTCAPBEN)
    RCC_PERIPH_RTCAPB         , /**< Real Time Clock APB interface clock enable */
#endif
#if defined(RCC_AHB1SMENR_SRAM1SMEN)
    RCC_PERIPH_SRAM1          , /**< SRAM1 (clock control in sleep mode only) */
#endif
#if defined(RCC_AHB2SMENR_SRAM2SMEN)
    RCC_PERIPH_SRAM2          , /**< SRAM2 (clock control in sleep mode only) */
#endif
#if defined(RCC_AHB2SMENR_SRAM3SMEN)
    RCC_PERIPH_SRAM3          , /**< SRAM3 (clock control in sleep mode only) */
#endif
    RCC_PERIPH_DMA1           , /**< DMA1 clock enable */
    RCC_PERIPH_DMA2           , /**< DMA2 clock enable */
#if defined(RCC_AHB1ENR_DMAMUX1EN)
    RCC_PERIPH_DMAMUX1        , /**< DMA request multiplexer (DMAMUX1) clock enable */
#endif
#if defined(RCC_AHB1ENR_DMA2DEN)
    RCC_PERIPH_DMA2D          , /**< DMA2D (Chrom-ART accelerator) clock enable */
#endif
#if defined(RCC_AHB1ENR_GFXMMUEN)
    RCC_PERIPH_GFXMMU         , /**< Graphic MMU (GFXMMU) clock enable */
#endif
#if defined(RCC_APB1ENR1_CRSEN)
    RCC_PERIPH_CRS            , /**< Clock recovery system (CRS) clock enable */
#endif
    RCC_PERIPH_WWDG           , /**< Window watchdog clock enable */
    RCC_PERIPH_GPIOA          , /**< IO port A clock enable */
    RCC_PERIPH_GPIOB          , /**< IO port B clock enable */
    RCC_PERIPH_GPIOC          , /**< IO port C clock enable */
#if defined(RCC_AHB2ENR_GPIODEN)
    RCC_PERIPH_GPIOD          , /**< IO port D clock enable */
#endif
#if defined(RCC_AHB2ENR_GPIOEEN)
    RCC_PERIPH_GPIOE          , /**< IO port E clock enable */
#endif
#if defined(RCC_AHB2ENR_GPIOFEN)
    RCC_PERIPH_GPIOF          , /**< IO port F clock enable */
#endif
#if defined(RCC_AHB2ENR_GPIOGEN)
    RCC_PERIPH_GPIOG          , /**< IO port G clock enable (PG[15:2] supplied by VDDIO2 - validated with the clock) */
#endif
    RCC_PERIPH_GPIOH          , /**< IO port H clock enable */
#if defined(RCC_AHB2ENR_GPIOIEN)
    RCC_PERIPH_GPIOI          , /**< IO port I clock enable */
#endif
    RCC_PERIPH_TSC            , /**< Touch sensing controller (TSC) clock enable */

    /*--------------------------------- Timers ---------------------------------*/

    RCC_PERIPH_TIM1           , /**< TIM1 clock enable */
    RCC_PERIPH_TIM2           , /**< TIM2 clock enable */
#if defined(RCC_APB1ENR1_TIM3EN)
    RCC_PERIPH_TIM3           , /**< TIM3 clock enable */
#endif
#if defined(RCC_APB1ENR1_TIM4EN)
    RCC_PERIPH_TIM4           , /**< TIM4 clock enable */
#endif
#if defined(RCC_APB1ENR1_TIM5EN)
    RCC_PERIPH_TIM5           , /**< TIM5 clock enable */
#endif
    RCC_PERIPH_TIM6           , /**< TIM6 clock enable */
#if defined(RCC_APB1ENR1_TIM7EN)
    RCC_PERIPH_TIM7           , /**< TIM7 clock enable */
#endif
#if defined(RCC_APB2ENR_TIM8EN)
    RCC_PERIPH_TIM8           , /**< TIM8 clock enable */
#endif
    RCC_PERIPH_TIM15          , /**< TIM15 clock enable */
    RCC_PERIPH_TIM16          , /**< TIM16 clock enable */
#if defined(RCC_APB2ENR_TIM17EN)
    RCC_PERIPH_TIM17          , /**< TIM17 clock enable */
#endif
    RCC_PERIPH_LPTIM1_PCLK1   , /**< Low Power Timer 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_LPTIM1_LSI     , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) oscillator as clock source */
    RCC_PERIPH_LPTIM1_HSI     , /**< Low Power Timer 1 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
    RCC_PERIPH_LPTIM1_LSE     , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source */
    RCC_PERIPH_LPTIM2_PCLK1   , /**< Low Power Timer 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_LPTIM2_LSI     , /**< Low Power Timer 2 clock enable with Low Speed Internal (LSI) oscillator as clock source */
    RCC_PERIPH_LPTIM2_HSI     , /**< Low Power Timer 2 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
    RCC_PERIPH_LPTIM2_LSE     , /**< Low Power Timer 2 clock enable with Low Speed External (LSE) oscillator as clock source */

    /*------------------------------ Connectivity ------------------------------*/

    RCC_PERIPH_SPI1           , /**< SPI1 clock enable */
#if defined(RCC_APB1ENR1_SPI2EN)
    RCC_PERIPH_SPI2           , /**< SPI2 clock enable */
#endif
#if defined(RCC_APB1ENR1_SPI3EN)
    RCC_PERIPH_SPI3           , /**< SPI3 clock enable */
#endif
    RCC_PERIPH_I2C1_PCLK1     , /**< I2C1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C1_SYSCLK    , /**< I2C1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C1_HSI       , /**< I2C1 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_PERIPH_I2C2_PCLK1     , /**< I2C2 clock enable with APB1 (PCLK1) as clock source */
#endif
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_PERIPH_I2C2_SYSCLK    , /**< I2C2 clock enable with system clock (SYSCLK) as clock source */
#endif
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_PERIPH_I2C2_HSI       , /**< I2C2 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#endif
    RCC_PERIPH_I2C3_PCLK1     , /**< I2C3 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C3_SYSCLK    , /**< I2C3 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C3_HSI       , /**< I2C3 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_PERIPH_I2C4_PCLK1     , /**< I2C4 clock enable with APB1 (PCLK1) as clock source */
#endif
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_PERIPH_I2C4_SYSCLK    , /**< I2C4 clock enable with system clock (SYSCLK) as clock source */
#endif
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_PERIPH_I2C4_HSI       , /**< I2C4 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#endif
    RCC_PERIPH_USART1_PCLK2   , /**< USART1 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART1_SYSCLK  , /**< USART1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART1_HSI     , /**< USART1 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
    RCC_PERIPH_USART1_LSE     , /**< USART1 clock enable with Low Speed External (LSE) oscillator as clock source */
    RCC_PERIPH_USART2_PCLK1   , /**< USART2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART2_SYSCLK  , /**< USART2 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART2_HSI     , /**< USART2 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
    RCC_PERIPH_USART2_LSE     , /**< USART2 clock enable with Low Speed External (LSE) oscillator as clock source */
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_PERIPH_USART3_PCLK1   , /**< USART3 clock enable with APB1 (PCLK1) as clock source */
#endif
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_PERIPH_USART3_SYSCLK  , /**< USART3 clock enable with system clock (SYSCLK) as clock source */
#endif
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_PERIPH_USART3_HSI     , /**< USART3 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#endif
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_PERIPH_USART3_LSE     , /**< USART3 clock enable with Low Speed External (LSE) oscillator as clock source */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_PERIPH_UART4_PCLK1    , /**< UART4 clock enable with APB1 (PCLK1) as clock source */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_PERIPH_UART4_SYSCLK   , /**< UART4 clock enable with system clock (SYSCLK) as clock source */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_PERIPH_UART4_HSI      , /**< UART4 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_PERIPH_UART4_LSE      , /**< UART4 clock enable with Low Speed External (LSE) oscillator as clock source */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_PERIPH_UART5_PCLK1    , /**< UART5 clock enable with APB1 (PCLK1) as clock source */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_PERIPH_UART5_SYSCLK   , /**< UART5 clock enable with system clock (SYSCLK) as clock source */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_PERIPH_UART5_HSI      , /**< UART5 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_PERIPH_UART5_LSE      , /**< UART5 clock enable with Low Speed External (LSE) oscillator as clock source */
#endif
    RCC_PERIPH_LPUART1_PCLK1  , /**< LPUART1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_LPUART1_SYSCLK , /**< LPUART1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_LPUART1_HSI    , /**< LPUART1 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
    RCC_PERIPH_LPUART1_LSE    , /**< LPUART1 clock enable with Low Speed External (LSE) oscillator as clock source */
#if defined(RCC_APB1ENR1_CAN1EN)
    RCC_PERIPH_CAN1           , /**< CAN1 clock enable */
#endif
#if defined(RCC_APB1ENR1_CAN2EN)
    RCC_PERIPH_CAN2           , /**< CAN2 clock enable */
#endif
#if defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CRRCR_HSI48ON)
    RCC_PERIPH_USB_HSI48      , /**< USB clock enable with 48 MHz High Speed Internal (HSI48) oscillator as 48 MHz clock (CLK48) source */
#endif
#if defined(RCC_TYPES_USB_SUPPORT) && \
    defined(RCC_CR_PLLSAI1ON)
    RCC_PERIPH_USB_PLLSAI1Q   , /**< USB clock enable with PLLSAI1 output Q as 48 MHz clock (CLK48) source */
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
    RCC_PERIPH_USB_PLLQ       , /**< USB clock enable with main PLL output Q as 48 MHz clock (CLK48) source */
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
    RCC_PERIPH_USB_MSI        , /**< USB clock enable with Multi Speed Internal (MSI) oscillator as 48 MHz clock (CLK48) source */
#endif
#if defined(RCC_TYPES_SDMMC1_SUPPORT)
    RCC_PERIPH_SDMMC1         , /**< SD / MMC card interface 1 clock enable (kernel clock CLK48 or PLLP) */
#endif
#if defined(RCC_AHB2ENR_SDMMC2EN)
    RCC_PERIPH_SDMMC2         , /**< SD / MMC card interface 2 clock enable (kernel clock CLK48 or PLLP) */
#endif
#if defined(RCC_AHB3ENR_FMCEN)
    RCC_PERIPH_FMC            , /**< Flexible Memory Controller (FMC) clock enable */
#endif
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_PERIPH_QSPI           , /**< Quad SPI memory interface (QUADSPI) clock enable */
#endif
#if defined(RCC_AHB3ENR_OSPI1EN)
    RCC_PERIPH_OSPI1          , /**< OctoSPI 1 clock enable (kernel clock OSPISEL not handled) */
#endif
#if defined(RCC_AHB3ENR_OSPI2EN)
    RCC_PERIPH_OSPI2          , /**< OctoSPI 2 clock enable (kernel clock OSPISEL not handled) */
#endif
#if defined(RCC_AHB2ENR_OSPIMEN)
    RCC_PERIPH_OSPIM          , /**< OctoSPI IO manager (OCTOSPIM) clock enable */
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
    RCC_PERIPH_SWPMI1_PCLK1   , /**< Single wire protocol master interface 1 clock enable with APB1 (PCLK1) as clock source */
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
    RCC_PERIPH_SWPMI1_HSI     , /**< Single wire protocol master interface 1 clock enable with 16 MHz High Speed Internal (HSI16) oscillator as clock source */
#endif

    /*------------------------------- Multimedia -------------------------------*/

#if defined(RCC_AHB2ENR_DCMIEN)
    RCC_PERIPH_DCMI           , /**< Digital Camera Interface (DCMI) clock enable */
#endif
#if defined(RCC_APB2ENR_LTDCEN)
    RCC_PERIPH_LTDC           , /**< LCD-TFT Display Controller (LTDC) clock enable (kernel clock not handled) */
#endif
#if defined(RCC_APB2ENR_DSIEN)
    RCC_PERIPH_DSI            , /**< Display Serial Interface (DSI) host clock enable (kernel clock not handled) */
#endif
#if defined(RCC_APB2ENR_SAI1EN)
    RCC_PERIPH_SAI1           , /**< Serial Audio Interface 1 (SAI1) clock enable (kernel clock not handled) */
#endif
#if defined(RCC_APB2ENR_SAI2EN)
    RCC_PERIPH_SAI2           , /**< Serial Audio Interface 2 (SAI2) clock enable (kernel clock not handled) */
#endif
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_PERIPH_DFSDM1_PCLK2   , /**< Digital Filter for Sigma-Delta Modulators 1 clock enable with APB2 (PCLK2) as clock source */
#endif
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_PERIPH_DFSDM1_SYSCLK  , /**< Digital Filter for Sigma-Delta Modulators 1 clock enable with system clock (SYSCLK) as clock source */
#endif
#if defined(RCC_APB1ENR1_LCDEN)
    RCC_PERIPH_LCD            , /**< LCD controller clock enable (clocked by RTC clock, not handled) */
#endif

    /*--------------------------------- Analog ---------------------------------*/

    RCC_PERIPH_ADC_HCLK       , /**< ADC clock enable, synchronous clock HCLK (no asynchronous clock selected) */
#if defined(RCC_CR_PLLSAI1ON)
    RCC_PERIPH_ADC_PLLSAI1R   , /**< ADC clock enable with PLLSAI1 output R as asynchronous clock */
#endif
#if defined(RCC_TYPES_ADC_PLLSAI2R_SUPPORT)
    RCC_PERIPH_ADC_PLLSAI2R   , /**< ADC clock enable with PLLSAI2 output R as asynchronous clock */
#endif
    RCC_PERIPH_ADC_SYSCLK     , /**< ADC clock enable with system clock (SYSCLK) as asynchronous clock */
#if defined(RCC_APB1ENR1_DAC1EN)
    RCC_PERIPH_DAC1           , /**< DAC1 clock enable */
#endif
    RCC_PERIPH_OPAMP          , /**< Operational amplifiers (OPAMP) clock enable */

    /*-------------------------------- Security --------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
    RCC_PERIPH_AES            , /**< Advanced Encryption Standard (AES) HW accelerator clock enable */
#endif
#if defined(RCC_AHB2ENR_HASHEN)
    RCC_PERIPH_HASH           , /**< HASH processor clock enable */
#endif
#if defined(RCC_AHB2ENR_PKAEN)
    RCC_PERIPH_PKA            , /**< Public Key Accelerator (PKA) clock enable */
#endif
#if defined(RCC_CRRCR_HSI48ON)
    RCC_PERIPH_RNG_HSI48      , /**< Random Number Generator clock enable with 48 MHz High Speed Internal (HSI48) oscillator as 48 MHz clock (CLK48) source */
#endif
#if defined(RCC_CR_PLLSAI1ON)
    RCC_PERIPH_RNG_PLLSAI1Q   , /**< Random Number Generator clock enable with PLLSAI1 output Q as 48 MHz clock (CLK48) source */
#endif
    RCC_PERIPH_RNG_PLLQ       , /**< Random Number Generator clock enable with main PLL output Q as 48 MHz clock (CLK48) source */
    RCC_PERIPH_RNG_MSI        , /**< Random Number Generator clock enable with Multi Speed Internal (MSI) oscillator as 48 MHz clock (CLK48) source */

    /*------------------------------- Computing --------------------------------*/

    RCC_PERIPH_CRC            , /**< CRC clock enable */

    RCC_PERIPH_ID_CNT
}   rcc_PeriphId_t;

/*---------------------------- Reset source flags ----------------------------*/

/** \brief List of reset sources stored in RCC control / status register (CSR)
 *
 * \note BOR flag is set also after power-on reset (POR). */
typedef enum
{
    RCC_RESET_SRC_PIN   = 0u, /**< Reset from NRST pin                                  */
    RCC_RESET_SRC_BOR       , /**< Brown-out reset (BOR), also set after power-on reset */
    RCC_RESET_SRC_SW        , /**< System reset requested by software                   */
    RCC_RESET_SRC_IWDG      , /**< Independent watchdog reset                           */
    RCC_RESET_SRC_WWDG      , /**< Window watchdog reset                                */
    RCC_RESET_SRC_LPWR      , /**< Illegal Stop / Standby / Shutdown mode entry reset   */
    RCC_RESET_SRC_OBL       , /**< Option byte loader reset                             */
    RCC_RESET_SRC_FW        , /**< Firewall reset                                       */
    RCC_RESET_SRC_CNT         /**< Number of reset sources                              */
}   rcc_ResetSrc_t;

/*------------------------ Clock sources configuration -----------------------*/

/** \brief Enumeration of High Speed External (HSE) input configuration */
typedef enum
{
    RCC_HSE_TYPE_NONE     = 0u , /**< No external clock connected to HSE pin             */
    RCC_HSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator (4 - 48 MHz)    */
    RCC_HSE_TYPE_SIG_IN        , /**< External clock signal (oscillator bypassed)         */
}   rcc_HseType_t;


/** \brief Enumeration of Low Speed External (LSE) input configuration */
typedef enum
{
    RCC_LSE_TYPE_NONE     = 0u , /**< No external clock connected to LSE pin             */
    RCC_LSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator                 */
    RCC_LSE_TYPE_SIG_IN        , /**< External clock signal (oscillator bypassed)         */
}   rcc_LseType_t;


/** \brief List of all available oscillators (except HSE, configured by \ref rcc_ConfigStruct_t)
 *
 * \note LSE is located in backup domain. The backup domain write protection
 *       is released by the module automatically. */
typedef enum
{
    RCC_OSC_HSI = 0u, /**< 16 MHz High Speed Internal (HSI16) oscillator.                 */
    RCC_OSC_MSI,      /**< Multi Speed Internal (MSI) oscillator, 100 kHz - 48 MHz.       */
#if defined(RCC_CRRCR_HSI48ON)
    RCC_OSC_HSI48,    /**< 48 MHz High Speed Internal (HSI48) oscillator (USB, RNG).      */
#endif /* RCC_CRRCR_HSI48ON */
    RCC_OSC_LSI,      /**< 32 kHz Low Speed Internal (LSI) oscillator.                    */
    RCC_OSC_LSE,      /**< 32.768 kHz Low Speed External (LSE) crystal.                   */
    RCC_OSC_CNT       /**< Count of available oscillators                                 */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition.
 * \note Oscillators of STM32L4 family have no output divider - only value 1 is
 *       valid. Frequency of MSI oscillator is selected by its range
 *       (\ref rcc_ConfigStruct_t::MSI_Range). */
typedef uint32_t rcc_OscDiv_t;


/** \brief Multi Speed Internal (MSI) oscillator frequency range
 * \note Values are equal to the RCC_CR MSIRANGE field values. */
typedef enum
{
    RCC_MSI_RANGE_100KHZ = LL_RCC_MSIRANGE_0 , /**< MSI 100 kHz                       */
    RCC_MSI_RANGE_200KHZ = LL_RCC_MSIRANGE_1 , /**< MSI 200 kHz                       */
    RCC_MSI_RANGE_400KHZ = LL_RCC_MSIRANGE_2 , /**< MSI 400 kHz                       */
    RCC_MSI_RANGE_800KHZ = LL_RCC_MSIRANGE_3 , /**< MSI 800 kHz                       */
    RCC_MSI_RANGE_1MHZ   = LL_RCC_MSIRANGE_4 , /**< MSI 1 MHz                         */
    RCC_MSI_RANGE_2MHZ   = LL_RCC_MSIRANGE_5 , /**< MSI 2 MHz                         */
    RCC_MSI_RANGE_4MHZ   = LL_RCC_MSIRANGE_6 , /**< MSI 4 MHz (reset value)           */
    RCC_MSI_RANGE_8MHZ   = LL_RCC_MSIRANGE_7 , /**< MSI 8 MHz                         */
    RCC_MSI_RANGE_16MHZ  = LL_RCC_MSIRANGE_8 , /**< MSI 16 MHz                        */
    RCC_MSI_RANGE_24MHZ  = LL_RCC_MSIRANGE_9 , /**< MSI 24 MHz                        */
    RCC_MSI_RANGE_32MHZ  = LL_RCC_MSIRANGE_10, /**< MSI 32 MHz                        */
    RCC_MSI_RANGE_48MHZ  = LL_RCC_MSIRANGE_11, /**< MSI 48 MHz                        */
}   rcc_MsiRange_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration */
typedef enum
{
    RCC_PLL_1 = 0u, /**< Main Phase Locked Loop (PLL), system clock source (output R) */
#if defined(RCC_CR_PLLSAI1ON)
    RCC_PLL_2,      /**< PLLSAI1 (SAI, USB / RNG 48 MHz clock, ADC)                     */
#endif /* RCC_CR_PLLSAI1ON */
#if defined(RCC_CR_PLLSAI2ON)
    RCC_PLL_3,      /**< PLLSAI2 (SAI, ADC, LTDC)                                       */
#endif /* RCC_CR_PLLSAI2ON */
    RCC_PLL_CNT     /**< Count of available PLLs                                        */
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 *
 * \note All PLLs are clocked from the common PLL source multiplexer (PLLSRC).
 *       Clock source of a PLL can be changed only if no other PLL is active
 *       (or the source is equal).
 * \note If the PLL is not used, select RCC_PLL_SRC_NONE. Otherwise will be
 *       PLL activated. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                                  */
    RCC_PLL_SRC_MSI      , /**< PLL will be clocked by MSI oscillator            */
    RCC_PLL_SRC_HSI      , /**< PLL will be clocked by 16 MHz HSI16 oscillator   */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator            */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                      */
}   rcc_PllClkSrc_t;


/** \brief Phase Locked Loop (PLL) M Divider value.
 * This is clock input divider for PLL. PLL input frequency (after divider)
 * must be in range 4 - 16 MHz.
 * Step size: 1
 * Range    : 1 - 8 (1 - 16 on STM32L4+)
 * \note PLLSAI1 / PLLSAI2 share M divider with main PLL on STM32L4 - the value
 *       must be equal to main PLL M divider (STM32L4+ has own M dividers).
 */
typedef uint32_t rcc_PllMDivider_t;


/** \brief Type used to signal values of PLL N multiplier
 * Phase Locked Loop (PLL) Feedback multiplier. VCO frequency must be in range
 * 64 - 344 MHz.
 * Step size: 1
 * Range    : 8 - 86 (8 - 127 for PLLSAI1 / PLLSAI2 of STM32L4+)
 */
typedef uint32_t rcc_PllNMult_t;


/** \brief Phase Locked Loop (PLL) P output divider value.
 * Range    : 2 - 31 (MCUs with PLLPDIV), otherwise 7 or 17
 * Value 0  : Output P is not used (disabled)
 */
typedef uint32_t rcc_PllPDivider_t;


/** \brief Phase Locked Loop (PLL) Q output divider value.
 * Range    : 2, 4, 6, 8
 * Value 0  : Output Q is not used (disabled)
 */
typedef uint32_t rcc_PllQDivider_t;


/** \brief Phase Locked Loop (PLL) R output divider value.
 * Range    : 2, 4, 6, 8
 * Value 0  : Output R is not used (disabled). Output R of main PLL is the
 *            system clock source.
 */
typedef uint32_t rcc_PllRDivider_t;

/*--------------- Real Time Clock (RTC) clock source configuration -----------*/

typedef enum
{
    RCC_RTC_CLK_SOURCE_HSE_DIV = 0u, /**< High Speed External (HSE) clock divided by 32 will be used as RTC clock source. */
    RCC_RTC_CLK_SOURCE_LSE,          /**< Low Speed External (LSE) will be used as RTC clock source                      */
    RCC_RTC_CLK_SOURCE_LSI,          /**< Low Speed Internal (LSI) will be used as RTC clock source                      */
    RCC_RTC_CLK_SOURCE_CNT           /**< Count of RTC clock sources                                                     */
}   rcc_Rtc_ClkSource_t;


/**
 * \brief Divider value of HSE clock source for RTC peripheral.
 * \note STM32L4 family has fixed HSE divider 32 for RTC clock.
 */
typedef uint16_t rcc_Rtc_HseDiv_t;

/*------------------------- Clock outputs configuration ----------------------*/

/**
 * \brief Clock Output identification enumeration.
 */
typedef enum
{
    RCC_CLK_OUT_MCO1 = 0u, /**< Master Clock Output (MCO, PA8)             */
    RCC_CLK_OUT_LSCO,      /**< Low Speed Clock Output (LSCO, PA2)         */
    RCC_CLK_OUT_CNT        /**< Count of Clock Outputs                     */
}   rcc_ClkOut_Id_t;


/**
 * \brief Clock Output's source configuration enumeration.
 */
typedef enum
{
    RCC_CLK_SOURCE_NONE   = 0u, /**< No clock source selected (clock output is not configured) */

    RCC_CLK_SOURCE_MCO1_SYSCLK , /**< System clock (SYSCLK) will be used as Master Clock Output (MCO) clock source            */
    RCC_CLK_SOURCE_MCO1_MSI    , /**< Multi Speed Internal (MSI) will be used as Master Clock Output (MCO) clock source       */
    RCC_CLK_SOURCE_MCO1_HSI    , /**< 16 MHz High Speed Internal (HSI16) will be used as Master Clock Output (MCO) clock source */
    RCC_CLK_SOURCE_MCO1_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output (MCO) clock source        */
    RCC_CLK_SOURCE_MCO1_PLLR   , /**< Main PLL output R (PLLCLK) will be used as Master Clock Output (MCO) clock source       */
    RCC_CLK_SOURCE_MCO1_LSI    , /**< Low Speed Internal (LSI) will be used as Master Clock Output (MCO) clock source         */
    RCC_CLK_SOURCE_MCO1_LSE    , /**< Low Speed External (LSE) will be used as Master Clock Output (MCO) clock source         */
#if defined(RCC_CRRCR_HSI48ON)
    RCC_CLK_SOURCE_MCO1_HSI48  , /**< 48 MHz High Speed Internal (HSI48) will be used as Master Clock Output (MCO) clock source */
#endif /* RCC_CRRCR_HSI48ON */

    RCC_CLK_SOURCE_LSCO_LSI    , /**< Low Speed Internal (LSI) will be used as Low Speed Clock Output (LSCO) clock source     */
    RCC_CLK_SOURCE_LSCO_LSE    , /**< Low Speed External (LSE) will be used as Low Speed Clock Output (LSCO) clock source     */

    RCC_CLK_SOURCE_CNT           /**< Count of clock output sources                                                         */
}   rcc_ClkOut_Source_t;


/**
 * \brief Master Clock Output (MCO) divider value type.
 *
 * Output clock divider value for Clock Output's.
 * Range of values: 1, 2, 4, 8, 16 (MCO), 1 (LSCO)
 */
typedef uint32_t rcc_ClkOut_Div_t;

/*-------------------------- Clock buses configuration -----------------------*/

/** \brief List of all available clock buses */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u, /**< Advanced High-performance Bus 1   */
    RCC_CLK_BUS_AHB2,      /**< Advanced High-performance Bus 2   */
    RCC_CLK_BUS_AHB3,      /**< Advanced High-performance Bus 3   */
    RCC_CLK_BUS_APB1_1,    /**< Advanced Peripheral Bus 1 group 1 */
    RCC_CLK_BUS_APB1_2,    /**< Advanced Peripheral Bus 1 group 2 */
    RCC_CLK_BUS_APB2,      /**< Advanced Peripheral Bus 2         */
    RCC_CLK_BUS_CNT        /**< Count of available clock buses    */
}   rcc_ClkBusId_t;


/** \brief Clock bus divider value type definition.
 * Used for AHB, APB1 and APB2 clock bus dividers (all available clock
 * busses in \ref rcc_ClkBusId_t ) */
typedef uint32_t rcc_ClkBusDiv_t;


/** \brief System clock source multiplexer configuration list
 * \note Values are equal to the RCC_CFGR SW field values. */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_MSI = 0u, /**< MSI will be used as system clock source              */
    RCC_SYSTEM_CLOCK_SOURCE_HSI     , /**< HSI16 will be used as system clock source            */
    RCC_SYSTEM_CLOCK_SOURCE_HSE     , /**< HSE will be used as system clock source              */
    RCC_SYSTEM_CLOCK_SOURCE_PLL     , /**< Main PLL output R will be used as system clock       */
    RCC_SYSTEM_CLOCK_SOURCE_CNT       /**< Count of available system clock sources              */
}   rcc_SystemClkSrc_t;


/** \brief Type representing numerical value of AHB divider.
 * The value of AHB is divided from SYSCLK */
typedef enum
{
    RCC_AHB_DIVIDER_1   = LL_RCC_SYSCLK_DIV_1  ,
    RCC_AHB_DIVIDER_2   = LL_RCC_SYSCLK_DIV_2  ,
    RCC_AHB_DIVIDER_4   = LL_RCC_SYSCLK_DIV_4  ,
    RCC_AHB_DIVIDER_8   = LL_RCC_SYSCLK_DIV_8  ,
    RCC_AHB_DIVIDER_16  = LL_RCC_SYSCLK_DIV_16 ,
    RCC_AHB_DIVIDER_64  = LL_RCC_SYSCLK_DIV_64 ,
    RCC_AHB_DIVIDER_128 = LL_RCC_SYSCLK_DIV_128,
    RCC_AHB_DIVIDER_256 = LL_RCC_SYSCLK_DIV_256,
    RCC_AHB_DIVIDER_512 = LL_RCC_SYSCLK_DIV_512
}   rcc_AHB_Div_t;


/** \brief Type representing numerical value of APB1 divider.
 * The value of APB1 is divided from HCLK */
typedef enum
{
    RCC_APB1_DIVIDER_1  = LL_RCC_APB1_DIV_1 ,
    RCC_APB1_DIVIDER_2  = LL_RCC_APB1_DIV_2 ,
    RCC_APB1_DIVIDER_4  = LL_RCC_APB1_DIV_4 ,
    RCC_APB1_DIVIDER_8  = LL_RCC_APB1_DIV_8 ,
    RCC_APB1_DIVIDER_16 = LL_RCC_APB1_DIV_16,
}   rcc_APB1_Div_t;


/** \brief Type representing numerical value of APB2 divider.
 * The value of APB2 is divided from HCLK */
typedef enum
{
    RCC_APB2_DIVIDER_1  = LL_RCC_APB2_DIV_1 ,
    RCC_APB2_DIVIDER_2  = LL_RCC_APB2_DIV_2 ,
    RCC_APB2_DIVIDER_4  = LL_RCC_APB2_DIV_4 ,
    RCC_APB2_DIVIDER_8  = LL_RCC_APB2_DIV_8 ,
    RCC_APB2_DIVIDER_16 = LL_RCC_APB2_DIV_16,
}   rcc_APB2_Div_t;

/*------------------------ Flash and power configuration ---------------------*/

/**
 * \brief Defines number of wait states for Flash memory access.
 *
 * Number of wait states is calculated automatically from expected processor
 * clock (HCLK) and voltage range (e.g. 16 MHz per wait state in range 1 of
 * STM32L4, 20 MHz per wait state in range 1 of STM32L4+). Configured value is
 * used as minimal number of wait states.
 */
typedef enum
{
    RCC_FLASH_LATENCY_0_WS  = LL_FLASH_LATENCY_0,
    RCC_FLASH_LATENCY_1_WS  = LL_FLASH_LATENCY_1,
    RCC_FLASH_LATENCY_2_WS  = LL_FLASH_LATENCY_2,
    RCC_FLASH_LATENCY_3_WS  = LL_FLASH_LATENCY_3,
    RCC_FLASH_LATENCY_4_WS  = LL_FLASH_LATENCY_4,
#if defined(FLASH_ACR_LATENCY_5WS)
    RCC_FLASH_LATENCY_5_WS  = LL_FLASH_LATENCY_5,
    RCC_FLASH_LATENCY_6_WS  = LL_FLASH_LATENCY_6,
    RCC_FLASH_LATENCY_7_WS  = LL_FLASH_LATENCY_7,
    RCC_FLASH_LATENCY_8_WS  = LL_FLASH_LATENCY_8,
    RCC_FLASH_LATENCY_9_WS  = LL_FLASH_LATENCY_9,
    RCC_FLASH_LATENCY_10_WS = LL_FLASH_LATENCY_10,
    RCC_FLASH_LATENCY_11_WS = LL_FLASH_LATENCY_11,
    RCC_FLASH_LATENCY_12_WS = LL_FLASH_LATENCY_12,
    RCC_FLASH_LATENCY_13_WS = LL_FLASH_LATENCY_13,
    RCC_FLASH_LATENCY_14_WS = LL_FLASH_LATENCY_14,
    RCC_FLASH_LATENCY_15_WS = LL_FLASH_LATENCY_15,
#endif /* FLASH_ACR_LATENCY_5WS */
}   rcc_FlashLatency_t;


/**
 * \brief PWR voltage scaling (regulator voltage range) configuration.
 * \note Range 1: system clock up to 80 MHz (STM32L4) / 120 MHz (STM32L4+, boost
 *       mode is activated automatically above 80 MHz). Range 2: up to 26 MHz.
 */
typedef enum
{
    RCC_PWR_VOLTAGE_SCALE_1 = LL_PWR_REGU_VOLTAGE_SCALE1, /**< Range 1 - highest performance   */
    RCC_PWR_VOLTAGE_SCALE_2 = LL_PWR_REGU_VOLTAGE_SCALE2, /**< Range 2 - lowest consumption    */
}   rcc_PwrVoltageScale_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL's (common for all PLLs) */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - input divider. PLL input frequency must be in range 4 - 16 MHz */
    rcc_PllMDivider_t       M_Divider;

    /** N multiplier. VCO frequency must be in range 64 - 344 MHz */
    rcc_PllNMult_t          N_Multiplier;

    /** Output P prescaler - divider (SAI clock). Value 0 - output disabled. */
    rcc_PllPDivider_t       P_Divider;

    /** Output Q prescaler - divider (48 MHz clock of USB / RNG / SDMMC). Value
     *  0 - output disabled. */
    rcc_PllQDivider_t       Q_Divider;

    /** Output R prescaler - divider (system clock of main PLL, ADC clock of
     *  PLLSAI1 / PLLSAI2). Value 0 - output disabled. */
    rcc_PllRDivider_t       R_Divider;

}   rcc_PllConfigStruct_t;


/** \brief Clock outputs configuration structure */
typedef struct
{
    rcc_ClkOut_Source_t ClockSource;
    rcc_ClkOut_Div_t    ClockDivider;

}   rcc_ClkOutConfigStruct_t;


/** \brief Reset and Clock Control configuration structure */
typedef struct rcc_ConfigStruct_t
{
    /** Specifies HSE clock type. Ignored if HSE is not used */
    rcc_HseType_t           HSE_ClockType;

    /** Specified frequency of the HSE oscillator (4 - 48 MHz). Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Frequency range of MSI oscillator. Applied if MSI is used as system clock
     *  or PLL source. */
    rcc_MsiRange_t          MSI_Range;

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Configuration of PLLs (main PLL, PLLSAI1 and PLLSAI2 if available) */
    rcc_PllConfigStruct_t   Pll_Config[ RCC_PLL_CNT ];

    /**
    * \brief Enables or disables Clock Security System (CSS).
    *
    * If the CSS is enabled and a failure of HSE is detected, the HSE is
    * switched off, system clock is switched to HSI16 and NMI is generated.
    *
    * \warning When using the CSS, NMI handler has to handle the HSE failure
    *          (clear CSSF flag). Otherwise the NMI is generated repeatedly.
    * \warning Once enabled, the CSS can't be turned off by software on L4 MCU
    *          family (just by reset).
    * \note CSS is activated only if HSE is used (HSE_ClockType is not NONE).
    * \note CSS also sends an event to break inputs of advanced-control timers
    *       in case of HSE failure.
    */
    rcc_FunctionState_t     CSS_Enable;

    /** AHB prescaler - divider */
    rcc_AHB_Div_t           AHB_Divider;
    /** APB1 prescaler - divider */
    rcc_APB1_Div_t          APB1_Divider;
    /** APB2 prescaler - divider */
    rcc_APB2_Div_t          APB2_Divider;

    /** Value of time in ms [0.001s] between SysTicks */
    rcc_Time_ms_t           SysTickInterval;

    /** Minimal Flash latency - number of wait states. Required number of wait
     *  states is calculated automatically, higher value can be forced. */
    rcc_FlashLatency_t      FlashLatency;

    /**
    * Scaling of internal voltage supply
    */
    rcc_PwrVoltageScale_t    VoltageScaling;

    rcc_ClkOutConfigStruct_t McoConfig[ RCC_CLK_OUT_CNT ];

}   rcc_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* RCC_RCC_TYPES_H */
