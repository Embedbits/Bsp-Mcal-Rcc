/**
 * \defgroup Rcc Rcc
 * \brief Rcc module
 */

/**
 * \author Mr.Nobody
 * \file Rcc_Types.h
 * \ingroup Rcc
 * \brief Reset and Clock Control (RCC) module global types definition
 *
 * This file contains the types definitions used across the module and are
 * available for other modules through Port file.
 *
 * \note STM32F4 family - availability of peripherals, PLLs and clock sources
 *       depends on the selected MCU. Enumerators are guarded by RCC register
 *       bit definitions of the device header (e.g. RCC_APB1ENR_TIM2EN), so
 *       only items existing on the selected MCU are available.
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
 * enumerator per clock source (e.g. \c RCC_PERIPH_LPTIM1_LSE), the other
 * peripherals have single enumerator.
 *
 * \note Some peripherals has common clock source. If user tries to change clock
 *       source for this, error will be returned. User has to reset peripheral
 *       clock source and then configure it again.
 * \note ADC1, ADC2 and ADC3 share one reset control - reset of any ADC resets
 *       all of them. */
typedef enum rcc_PeriphId_t
{
    /*------------------------------ System core -----------------------------*/
    RCC_PERIPH_FLASH          = 0u, /**< Flash interface (clock control in sleep mode only) */

    RCC_PERIPH_SYSCFG             , /**< System configuration controller clock enable */
    RCC_PERIPH_PWR                , /**< Power interface clock enable */

    RCC_PERIPH_SYSTICK            , /**< System Tick timer clocked by processor clock (HCLK), no clock enable */

    RCC_PERIPH_IWDG               , /**< Independent watchdog clocked by Low Speed Internal (LSI) oscillator, no clock enable */

    RCC_PERIPH_RTC_HSE_DIV        , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by RTCPRE used as clock source */
    RCC_PERIPH_RTC_LSE            , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_PERIPH_RTC_LSI            , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

#if defined(RCC_APB1ENR_RTCAPBEN)
    RCC_PERIPH_RTCAPB             , /**< Real Time Clock APB interface clock enable */
#endif /* RCC_APB1ENR_RTCAPBEN */

#if defined(RCC_AHB1LPENR_SRAM1LPEN)
    RCC_PERIPH_SRAM1              , /**< SRAM1 (clock control in sleep mode only) */
#endif /* RCC_AHB1LPENR_SRAM1LPEN */
#if defined(RCC_AHB1LPENR_SRAM2LPEN)
    RCC_PERIPH_SRAM2              , /**< SRAM2 (clock control in sleep mode only) */
#endif /* RCC_AHB1LPENR_SRAM2LPEN */
#if defined(RCC_AHB1LPENR_SRAM3LPEN)
    RCC_PERIPH_SRAM3              , /**< SRAM3 (clock control in sleep mode only) */
#endif /* RCC_AHB1LPENR_SRAM3LPEN */

#if defined(RCC_AHB1ENR_BKPSRAMEN)
    RCC_PERIPH_BKPSRAM            , /**< Backup SRAM interface clock enable */
#endif /* RCC_AHB1ENR_BKPSRAMEN */
#if defined(RCC_AHB1ENR_CCMDATARAMEN)
    RCC_PERIPH_CCMDATARAM         , /**< CCM data RAM clock enable */
#endif /* RCC_AHB1ENR_CCMDATARAMEN */

    RCC_PERIPH_DMA1               , /**< DMA1 clock enable */
    RCC_PERIPH_DMA2               , /**< DMA2 clock enable */
#if defined(RCC_AHB1ENR_DMA2DEN)
    RCC_PERIPH_DMA2D              , /**< DMA2D (Chrom-Art accelerator) clock enable */
#endif /* RCC_AHB1ENR_DMA2DEN */

    RCC_PERIPH_WWDG               , /**< Window watchdog clock enable */

#if defined(RCC_APB2ENR_EXTITEN)
    RCC_PERIPH_EXTIT              , /**< EXTI and external interrupt controller clock enable */
#endif /* RCC_APB2ENR_EXTITEN */

    RCC_PERIPH_GPIOA              , /**< IO port A Clock Enable */
    RCC_PERIPH_GPIOB              , /**< IO port B Clock Enable */
    RCC_PERIPH_GPIOC              , /**< IO port C Clock Enable */
#if defined(RCC_AHB1ENR_GPIODEN)
    RCC_PERIPH_GPIOD              , /**< IO port D Clock Enable */
#endif /* RCC_AHB1ENR_GPIODEN */
#if defined(RCC_AHB1ENR_GPIOEEN)
    RCC_PERIPH_GPIOE              , /**< IO port E Clock Enable */
#endif /* RCC_AHB1ENR_GPIOEEN */
#if defined(RCC_AHB1ENR_GPIOFEN)
    RCC_PERIPH_GPIOF              , /**< IO port F Clock Enable */
#endif /* RCC_AHB1ENR_GPIOFEN */
#if defined(RCC_AHB1ENR_GPIOGEN)
    RCC_PERIPH_GPIOG              , /**< IO port G Clock Enable */
#endif /* RCC_AHB1ENR_GPIOGEN */
    RCC_PERIPH_GPIOH              , /**< IO port H Clock Enable */
#if defined(RCC_AHB1ENR_GPIOIEN)
    RCC_PERIPH_GPIOI              , /**< IO port I Clock Enable */
#endif /* RCC_AHB1ENR_GPIOIEN */
#if defined(RCC_AHB1ENR_GPIOJEN)
    RCC_PERIPH_GPIOJ              , /**< IO port J Clock Enable */
#endif /* RCC_AHB1ENR_GPIOJEN */
#if defined(RCC_AHB1ENR_GPIOKEN)
    RCC_PERIPH_GPIOK              , /**< IO port K Clock Enable */
#endif /* RCC_AHB1ENR_GPIOKEN */

    /*-------------------------------- Timers --------------------------------*/

    RCC_PERIPH_TIM1               , /**< TIM1 Clock Enable */
#if defined(RCC_APB1ENR_TIM2EN)
    RCC_PERIPH_TIM2               , /**< TIM2 Clock Enable */
#endif /* RCC_APB1ENR_TIM2EN */
#if defined(RCC_APB1ENR_TIM3EN)
    RCC_PERIPH_TIM3               , /**< TIM3 Clock Enable */
#endif /* RCC_APB1ENR_TIM3EN */
#if defined(RCC_APB1ENR_TIM4EN)
    RCC_PERIPH_TIM4               , /**< TIM4 Clock Enable */
#endif /* RCC_APB1ENR_TIM4EN */
    RCC_PERIPH_TIM5               , /**< TIM5 Clock Enable */
#if defined(RCC_APB1ENR_TIM6EN)
    RCC_PERIPH_TIM6               , /**< TIM6 Clock Enable */
#endif /* RCC_APB1ENR_TIM6EN */
#if defined(RCC_APB1ENR_TIM7EN)
    RCC_PERIPH_TIM7               , /**< TIM7 Clock Enable */
#endif /* RCC_APB1ENR_TIM7EN */
#if defined(RCC_APB2ENR_TIM8EN)
    RCC_PERIPH_TIM8               , /**< TIM8 Clock Enable */
#endif /* RCC_APB2ENR_TIM8EN */
    RCC_PERIPH_TIM9               , /**< TIM9 Clock Enable */
#if defined(RCC_APB2ENR_TIM10EN)
    RCC_PERIPH_TIM10              , /**< TIM10 Clock Enable */
#endif /* RCC_APB2ENR_TIM10EN */
    RCC_PERIPH_TIM11              , /**< TIM11 Clock Enable */
#if defined(RCC_APB1ENR_TIM12EN)
    RCC_PERIPH_TIM12              , /**< TIM12 Clock Enable */
#endif /* RCC_APB1ENR_TIM12EN */
#if defined(RCC_APB1ENR_TIM13EN)
    RCC_PERIPH_TIM13              , /**< TIM13 Clock Enable */
#endif /* RCC_APB1ENR_TIM13EN */
#if defined(RCC_APB1ENR_TIM14EN)
    RCC_PERIPH_TIM14              , /**< TIM14 Clock Enable */
#endif /* RCC_APB1ENR_TIM14EN */

#if defined(RCC_APB1ENR_LPTIM1EN)
    RCC_PERIPH_LPTIM1_PCLK1       , /**< Low Power Timer 1 clock enable with APB1 (PCLK1) as clock source. */
    RCC_PERIPH_LPTIM1_HSI         , /**< Low Power Timer 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source. */
    RCC_PERIPH_LPTIM1_LSI         , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_PERIPH_LPTIM1_LSE         , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source. */
#endif /* RCC_APB1ENR_LPTIM1EN */

    /*----------------------------- Connectivity -----------------------------*/

    RCC_PERIPH_SPI1               , /**< SPI/I2S 1 Clock Enable */
#if defined(RCC_APB1ENR_SPI2EN)
    RCC_PERIPH_SPI2               , /**< SPI/I2S 2 Clock Enable */
#endif /* RCC_APB1ENR_SPI2EN */
#if defined(RCC_APB1ENR_SPI3EN)
    RCC_PERIPH_SPI3               , /**< SPI/I2S 3 Clock Enable */
#endif /* RCC_APB1ENR_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
    RCC_PERIPH_SPI4               , /**< SPI/I2S 4 Clock Enable */
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_APB2ENR_SPI5EN)
    RCC_PERIPH_SPI5               , /**< SPI/I2S 5 Clock Enable */
#endif /* RCC_APB2ENR_SPI5EN */
#if defined(RCC_APB2ENR_SPI6EN)
    RCC_PERIPH_SPI6               , /**< SPI 6 Clock Enable */
#endif /* RCC_APB2ENR_SPI6EN */

    RCC_PERIPH_I2C1               , /**< I2C 1 Clock Enable */
    RCC_PERIPH_I2C2               , /**< I2C 2 Clock Enable */
#if defined(RCC_APB1ENR_I2C3EN)
    RCC_PERIPH_I2C3               , /**< I2C 3 Clock Enable */
#endif /* RCC_APB1ENR_I2C3EN */

#if defined(RCC_APB1ENR_FMPI2C1EN)
    RCC_PERIPH_FMPI2C1_PCLK1      , /**< Fast-mode Plus I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_FMPI2C1_SYSCLK     , /**< Fast-mode Plus I2C 1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_FMPI2C1_HSI        , /**< Fast-mode Plus I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB1ENR_FMPI2C1EN */

    RCC_PERIPH_USART1             , /**< USART 1 Clock Enable */
    RCC_PERIPH_USART2             , /**< USART 2 Clock Enable */
#if defined(RCC_APB1ENR_USART3EN)
    RCC_PERIPH_USART3             , /**< USART 3 Clock Enable */
#endif /* RCC_APB1ENR_USART3EN */
#if defined(RCC_APB1ENR_UART4EN)
    RCC_PERIPH_UART4              , /**< UART 4 Clock Enable */
#endif /* RCC_APB1ENR_UART4EN */
#if defined(RCC_APB1ENR_UART5EN)
    RCC_PERIPH_UART5              , /**< UART 5 Clock Enable */
#endif /* RCC_APB1ENR_UART5EN */
#if defined(RCC_APB2ENR_USART6EN)
    RCC_PERIPH_USART6             , /**< USART 6 Clock Enable */
#endif /* RCC_APB2ENR_USART6EN */
#if defined(RCC_APB1ENR_UART7EN)
    RCC_PERIPH_UART7              , /**< UART 7 Clock Enable */
#endif /* RCC_APB1ENR_UART7EN */
#if defined(RCC_APB1ENR_UART8EN)
    RCC_PERIPH_UART8              , /**< UART 8 Clock Enable */
#endif /* RCC_APB1ENR_UART8EN */
#if defined(RCC_APB2ENR_UART9EN)
    RCC_PERIPH_UART9              , /**< UART 9 Clock Enable */
#endif /* RCC_APB2ENR_UART9EN */
#if defined(RCC_APB2ENR_UART10EN)
    RCC_PERIPH_UART10             , /**< UART 10 Clock Enable */
#endif /* RCC_APB2ENR_UART10EN */

#if defined(RCC_APB1ENR_CAN1EN)
    RCC_PERIPH_CAN1               , /**< CAN 1 Clock Enable */
#endif /* RCC_APB1ENR_CAN1EN */
#if defined(RCC_APB1ENR_CAN2EN)
    RCC_PERIPH_CAN2               , /**< CAN 2 Clock Enable */
#endif /* RCC_APB1ENR_CAN2EN */
#if defined(RCC_APB1ENR_CAN3EN)
    RCC_PERIPH_CAN3               , /**< CAN 3 Clock Enable */
#endif /* RCC_APB1ENR_CAN3EN */

#if defined(RCC_APB2ENR_SDIOEN)
    RCC_PERIPH_SDIO               , /**< Secure Digital Input/Output interface (SDIO) Clock Enable */
#endif /* RCC_APB2ENR_SDIOEN */

#if defined(RCC_AHB3ENR_FSMCEN)
    RCC_PERIPH_FSMC               , /**< Flexible Static Memory Controller (FSMC) Clock Enable */
#endif /* RCC_AHB3ENR_FSMCEN */
#if defined(RCC_AHB3ENR_FMCEN)
    RCC_PERIPH_FMC                , /**< Flexible Memory Controller (FMC) Clock Enable */
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_PERIPH_QSPI               , /**< Quad SPI memory interface (QUADSPI) Clock Enable */
#endif /* RCC_AHB3ENR_QSPIEN */

#if defined(RCC_AHB2ENR_OTGFSEN)
    RCC_PERIPH_USB_OTG_FS         , /**< USB On-The-Go Full Speed Clock Enable (48 MHz clock PLL48CLK) */
#endif /* RCC_AHB2ENR_OTGFSEN */
#if defined(RCC_AHB1ENR_OTGHSEN)
    RCC_PERIPH_USB_OTG_HS         , /**< USB On-The-Go High Speed Clock Enable */
    RCC_PERIPH_USB_OTG_HS_ULPI    , /**< USB On-The-Go High Speed ULPI Clock Enable */
#endif /* RCC_AHB1ENR_OTGHSEN */

#if defined(RCC_AHB1ENR_ETHMACEN)
    RCC_PERIPH_ETH                , /**< Ethernet MAC Clock Enable */
    RCC_PERIPH_ETH_TX             , /**< Ethernet Transmission Clock Enable */
    RCC_PERIPH_ETH_RX             , /**< Ethernet Reception Clock Enable */
    RCC_PERIPH_ETH_PTP            , /**< Ethernet PTP Clock Enable */
#endif /* RCC_AHB1ENR_ETHMACEN */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(RCC_AHB2ENR_DCMIEN)
    RCC_PERIPH_DCMI               , /**< Digital Camera Media Interface (DCMI) Clock Enable */
#endif /* RCC_AHB2ENR_DCMIEN */
#if defined(RCC_APB2ENR_LTDCEN)
    RCC_PERIPH_LTDC               , /**< LCD-TFT Display Controller (LTDC) Clock Enable */
#endif /* RCC_APB2ENR_LTDCEN */
#if defined(RCC_APB2ENR_DSIEN)
    RCC_PERIPH_DSI                , /**< Display Serial Interface (DSI) Host Clock Enable */
#endif /* RCC_APB2ENR_DSIEN */
#if defined(RCC_APB2ENR_SAI1EN)
    RCC_PERIPH_SAI1               , /**< Serial Audio Interface 1 (SAI1) Clock Enable */
#endif /* RCC_APB2ENR_SAI1EN */
#if defined(RCC_APB2ENR_SAI2EN)
    RCC_PERIPH_SAI2               , /**< Serial Audio Interface 2 (SAI2) Clock Enable */
#endif /* RCC_APB2ENR_SAI2EN */
#if defined(RCC_APB1ENR_CECEN)
    RCC_PERIPH_CEC                , /**< Consumer Electronics Control (CEC) Clock Enable */
#endif /* RCC_APB1ENR_CECEN */
#if defined(RCC_APB1ENR_SPDIFRXEN)
    RCC_PERIPH_SPDIFRX            , /**< SPDIF receiver (SPDIFRX) Clock Enable */
#endif /* RCC_APB1ENR_SPDIFRXEN */
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_PERIPH_DFSDM1             , /**< Digital Filter for Sigma-Delta Modulators 1 (DFSDM1) Clock Enable */
#endif /* RCC_APB2ENR_DFSDM1EN */
#if defined(RCC_APB2ENR_DFSDM2EN)
    RCC_PERIPH_DFSDM2             , /**< Digital Filter for Sigma-Delta Modulators 2 (DFSDM2) Clock Enable */
#endif /* RCC_APB2ENR_DFSDM2EN */

    /*-------------------------------- Analog --------------------------------*/

    RCC_PERIPH_ADC1               , /**< ADC 1 Clock Enable */
#if defined(RCC_APB2ENR_ADC2EN)
    RCC_PERIPH_ADC2               , /**< ADC 2 Clock Enable */
#endif /* RCC_APB2ENR_ADC2EN */
#if defined(RCC_APB2ENR_ADC3EN)
    RCC_PERIPH_ADC3               , /**< ADC 3 Clock Enable */
#endif /* RCC_APB2ENR_ADC3EN */

#if defined(RCC_APB1ENR_DACEN)
    RCC_PERIPH_DAC                , /**< DAC Clock Enable */
#endif /* RCC_APB1ENR_DACEN */

    /*------------------------------- Security -------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
    RCC_PERIPH_AES                , /**< Advanced Encryption Standard (AES) HW accelerator Clock Enable */
#endif /* RCC_AHB2ENR_AESEN */
#if defined(RCC_AHB2ENR_CRYPEN)
    RCC_PERIPH_CRYP               , /**< Cryptographic processor (CRYP) Clock Enable */
#endif /* RCC_AHB2ENR_CRYPEN */
#if defined(RCC_AHB2ENR_HASHEN)
    RCC_PERIPH_HASH               , /**< HASH processor Clock Enable */
#endif /* RCC_AHB2ENR_HASHEN */
#if defined(RCC_AHB1ENR_RNGEN) || defined(RCC_AHB2ENR_RNGEN)
    RCC_PERIPH_RNG                , /**< Random Number Generator (RNG) Clock Enable (48 MHz clock PLL48CLK) */
#endif /* RCC_AHB1ENR_RNGEN OR RCC_AHB2ENR_RNGEN */

    /*------------------------------- Computing ------------------------------*/

    RCC_PERIPH_CRC                , /**< CRC Clock Enable */

    RCC_PERIPH_ID_CNT
}   rcc_PeriphId_t;

/*---------------------------- Reset source flags ----------------------------*/

/** \brief List of reset sources stored in RCC clock control & status register (CSR)
 *
 * \note BOR flag is set also after power-on / power-down reset (POR / PDR). */
typedef enum
{
    RCC_RESET_SRC_PIN   = 0u, /**< Reset from NRST pin                                  */
    RCC_RESET_SRC_BOR       , /**< Brown-out reset (BOR), also set after power-on reset */
    RCC_RESET_SRC_SW        , /**< System reset requested by software                   */
    RCC_RESET_SRC_IWDG      , /**< Independent watchdog reset                           */
    RCC_RESET_SRC_WWDG      , /**< Window watchdog reset                                */
    RCC_RESET_SRC_LPWR      , /**< Illegal Stop / Standby mode entry reset              */
    RCC_RESET_SRC_CNT         /**< Number of reset sources                              */
}   rcc_ResetSrc_t;

/*------------------------ Clock sources configuration -----------------------*/

/** \brief Enumeration of High Speed External (HSE) input configuration */
typedef enum
{
    RCC_HSE_TYPE_NONE     = 0u , /**< No external clock connected to HSE pin            */
    RCC_HSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator                */
    RCC_HSE_TYPE_SIG_IN        , /**< External clock signal (oscillator bypassed)        */
}   rcc_HseType_t;


/** \brief Enumeration of Low Speed External (LSE) input configuration */
typedef enum
{
    RCC_LSE_TYPE_NONE     = 0u , /**< No external clock connected to LSE pin            */
    RCC_LSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator                */
    RCC_LSE_TYPE_SIG_IN        , /**< External clock signal (oscillator bypassed)        */
}   rcc_LseType_t;


/** \brief List of all available oscillators (except HSE, configured by \ref rcc_ConfigStruct_t)
 *
 * \note LSE is located in backup domain. The backup domain write protection
 *       is released by the module automatically. */
typedef enum
{
    RCC_OSC_HSI = 0u, /**< 16 MHz High Speed Internal (HSI) oscillator.        */
    RCC_OSC_LSI,      /**< 32 kHz Low Speed Internal (LSI) oscillator.         */
    RCC_OSC_LSE,      /**< 32.768 kHz Low Speed External (LSE) crystal.        */
    RCC_OSC_CNT       /**< Count of available oscillators                      */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition.
 * \note Oscillators of STM32F4 family have no output divider - only value 1 is valid. */
typedef uint32_t rcc_OscDiv_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration */
typedef enum
{
    RCC_PLL_MAIN = 0u, /**< Main Phase Locked Loop (PLL), system clock source */
#if defined(RCC_CR_PLLI2SON)
    RCC_PLL_I2S,       /**< I2S Phase Locked Loop (PLLI2S)                    */
#endif /* RCC_CR_PLLI2SON */
#if defined(RCC_CR_PLLSAION)
    RCC_PLL_SAI,       /**< SAI Phase Locked Loop (PLLSAI)                    */
#endif /* RCC_CR_PLLSAION */
    RCC_PLL_CNT        /**< Count of available PLLs                           */
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 *
 * \note All PLLs are clocked from the common PLL source multiplexer. Clock
 *       source of a PLL can be changed only if no other PLL is active (or
 *       the source is equal).
 * \note If the PLL is not used, select RCC_PLL_SRC_NONE. Otherwise will be
 *       PLL activated. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                              */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator        */
    RCC_PLL_SRC_HSI      , /**< PLL will be clocked by 16MHz HSI oscillator  */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                  */
}   rcc_PllClkSrc_t;


/** \brief Phase Locked Loop (PLL) M Divider value.
 * This is clock input divider for PLL. PLL input frequency (after divider)
 * must be in range 0.95 - 2.1 MHz (2 MHz recommended).
 * Step size: 1
 * Range    : 2 - 63
 * \note PLLSAI (and PLLI2S on devices without PLLI2SM) shares M divider with
 *       main PLL - the value must be equal to main PLL M divider.
 */
typedef uint32_t rcc_PllMDivider_t;


/** \brief Type used to signal values of PLL N multiplier
 * Phase Locked Loop (PLL) Feedback multiplier.
 * Step size: 1
 * Range    : 50 - 432 (192 - 432 on STM32F401)
 */
typedef uint32_t rcc_PllNMult_t;


/** \brief Phase Locked Loop (PLL) P output divider value.
 * Step size: 2
 * Range    : 2 - 8
 * Value 0  : Output divider is not configured (reset value kept)
 */
typedef uint32_t rcc_PllPDivider_t;


/** \brief Phase Locked Loop (PLL) Q output divider value.
 * Step size: 1
 * Range    : 2 - 15
 * Value 0  : Output divider is not configured (reset value kept)
 */
typedef uint32_t rcc_PllQDivider_t;


/** \brief Phase Locked Loop (PLL) R output divider value.
 * Step size: 1
 * Range    : 2 - 7
 * Value 0  : Output divider is not configured (reset value kept)
 */
typedef uint32_t rcc_PllRDivider_t;

/*--------------- Real Time Clock (RTC) clock source configuration -----------*/

typedef enum
{
    RCC_RTC_CLK_SOURCE_HSE_DIV = 0u, /**< Divided High Speed External (HSE) clock will be used as RTC clock source. */
    RCC_RTC_CLK_SOURCE_LSE,          /**< Low Speed External (LSE) will be used as RTC clock source                 */
    RCC_RTC_CLK_SOURCE_LSI,          /**< Low Speed Internal (LSI) will be used as RTC clock source                 */
    RCC_RTC_CLK_SOURCE_CNT           /**< Count of RTC clock sources                                                */
}   rcc_Rtc_ClkSource_t;


/**
 * \brief Divider value of HSE clock source for RTC peripheral (RTCPRE).
 * Divided clock must be lower or equal to 1 MHz.
 * Step size: 1
 * Range    : 2-31
 */
typedef uint16_t rcc_Rtc_HseDiv_t;

/*------------------------- Clock outputs configuration ----------------------*/

/**
 * \brief Clock Output identification enumeration.
 */
typedef enum
{
    RCC_CLK_OUT_MCO1 = 0u, /**< Master Clock Output 1 (MCO1, PA8) */
    RCC_CLK_OUT_MCO2,      /**< Master Clock Output 2 (MCO2, PC9) */
    RCC_CLK_OUT_CNT        /**< Count of Clock Outputs            */
}   rcc_ClkOut_Id_t;


/**
 * \brief Clock Output's source configuration enumeration.
 */
typedef enum
{
    RCC_CLK_SOURCE_NONE   = 0u, /**< No clock source selected */

    RCC_CLK_SOURCE_MCO1_HSI    , /**< 16MHz High Speed Internal (HSI) will be used as Master Clock Output 1 (MCO1) clock source */
    RCC_CLK_SOURCE_MCO1_LSE    , /**< Low Speed External (LSE) will be used as Master Clock Output 1 (MCO1) clock source        */
    RCC_CLK_SOURCE_MCO1_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output 1 (MCO1) clock source       */
    RCC_CLK_SOURCE_MCO1_PLL    , /**< Main PLL output P will be used as Master Clock Output 1 (MCO1) clock source               */

    RCC_CLK_SOURCE_MCO2_SYSCLK , /**< System clock (SYSCLK) will be used as Master Clock Output 2 (MCO2) clock source           */
#if defined(RCC_CR_PLLI2SON)
    RCC_CLK_SOURCE_MCO2_PLLI2S , /**< PLLI2S output R will be used as Master Clock Output 2 (MCO2) clock source                 */
#endif /* RCC_CR_PLLI2SON */
    RCC_CLK_SOURCE_MCO2_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output 2 (MCO2) clock source       */
    RCC_CLK_SOURCE_MCO2_PLL    , /**< Main PLL output P will be used as Master Clock Output 2 (MCO2) clock source               */

    RCC_CLK_SOURCE_CNT           /**< Count of clock output sources                                                             */
}   rcc_ClkOut_Source_t;


/**
 * \brief Master Clock Output (MCO) divider value type.
 *
 * Output clock divider value for Clock Output's.
 * Step size: 1
 * Range of values: 1 - 5
 */
typedef uint32_t rcc_ClkOut_Div_t;

/*-------------------------- Clock buses configuration -----------------------*/

/** \brief List of all available clock buses */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u, /**< Advanced High-performance Bus 1   */
#if defined(RCC_AHB2_SUPPORT)
    RCC_CLK_BUS_AHB2,      /**< Advanced High-performance Bus 2   */
#endif /* RCC_AHB2_SUPPORT */
#if defined(RCC_AHB3_SUPPORT)
    RCC_CLK_BUS_AHB3,      /**< Advanced High-performance Bus 3   */
#endif /* RCC_AHB3_SUPPORT */
    RCC_CLK_BUS_APB1,      /**< Advanced Peripheral Bus 1         */
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
    RCC_SYSTEM_CLOCK_SOURCE_HSI = 0u, /**< HSI will be used as system clock source          */
    RCC_SYSTEM_CLOCK_SOURCE_HSE     , /**< HSE will be used as system clock source          */
    RCC_SYSTEM_CLOCK_SOURCE_PLL     , /**< Main PLL output P will be used as system clock   */
#if defined(RCC_CFGR_SW_PLLR)
    RCC_SYSTEM_CLOCK_SOURCE_PLLR    , /**< Main PLL output R will be used as system clock   */
#endif /* RCC_CFGR_SW_PLLR */
    RCC_SYSTEM_CLOCK_SOURCE_CNT       /**< Count of available system clock sources          */
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
 * clock (HCLK) and voltage scaling, assuming supply voltage 2.7 - 3.6 V
 * (e.g. 30 MHz per wait state on STM32F405/F407/F42x/F43x/F446/F469/F479).
 * Configured value is used as minimal number of wait states - higher value
 * has to be configured for lower supply voltage (see reference manual,
 * "Number of wait states according to CPU clock (HCLK) frequency").
 */
typedef enum
{
    RCC_FLASH_LATENCY_0_WS  = LL_FLASH_LATENCY_0,
    RCC_FLASH_LATENCY_1_WS  = LL_FLASH_LATENCY_1,
    RCC_FLASH_LATENCY_2_WS  = LL_FLASH_LATENCY_2,
    RCC_FLASH_LATENCY_3_WS  = LL_FLASH_LATENCY_3,
    RCC_FLASH_LATENCY_4_WS  = LL_FLASH_LATENCY_4,
    RCC_FLASH_LATENCY_5_WS  = LL_FLASH_LATENCY_5,
    RCC_FLASH_LATENCY_6_WS  = LL_FLASH_LATENCY_6,
    RCC_FLASH_LATENCY_7_WS  = LL_FLASH_LATENCY_7,
#if defined(FLASH_ACR_LATENCY_8WS)
    RCC_FLASH_LATENCY_8_WS  = LL_FLASH_LATENCY_8,
    RCC_FLASH_LATENCY_9_WS  = LL_FLASH_LATENCY_9,
    RCC_FLASH_LATENCY_10_WS = LL_FLASH_LATENCY_10,
    RCC_FLASH_LATENCY_11_WS = LL_FLASH_LATENCY_11,
    RCC_FLASH_LATENCY_12_WS = LL_FLASH_LATENCY_12,
    RCC_FLASH_LATENCY_13_WS = LL_FLASH_LATENCY_13,
    RCC_FLASH_LATENCY_14_WS = LL_FLASH_LATENCY_14,
    RCC_FLASH_LATENCY_15_WS = LL_FLASH_LATENCY_15,
#endif /* FLASH_ACR_LATENCY_8WS */
}   rcc_FlashLatency_t;


/**
 * \brief PWR Voltage scaling configuration.
 * \note Power voltage scaling has to be chosen according to desired performance.
 *       Maximal system clock frequency per scale depends on MCU (e.g. STM32F429:
 *       Scale 1 - 180 MHz (over-drive), Scale 2 - 168 MHz, Scale 3 - 120 MHz).
 * \note Voltage scaling is applied by hardware only when main PLL is active.
 * \note Over-drive mode (where available) is activated automatically when
 *       required system clock frequency exceeds the limit without over-drive.
 */
typedef enum
{
#if defined(RCC_MAX_FREQUENCY_SCALE1)
    RCC_PWR_VOLTAGE_SCALE_1 = LL_PWR_REGU_VOLTAGE_SCALE1, /**< Scale 1 mode - highest performance */
#endif /* RCC_MAX_FREQUENCY_SCALE1 */
    RCC_PWR_VOLTAGE_SCALE_2 = LL_PWR_REGU_VOLTAGE_SCALE2, /**< Scale 2 mode                       */
#if defined(LL_PWR_REGU_VOLTAGE_SCALE3)
    RCC_PWR_VOLTAGE_SCALE_3 = LL_PWR_REGU_VOLTAGE_SCALE3, /**< Scale 3 mode - lowest consumption  */
#endif /* LL_PWR_REGU_VOLTAGE_SCALE3 */
}   rcc_PwrVoltageScale_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL's (common for all PLLs) */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - input divider. PLL input frequency must be in range 0.95 - 2.1 MHz */
    rcc_PllMDivider_t       M_Divider;

    /** N multiplier. VCO frequency must be in range given by the MCU (100/192 - 432 MHz) */
    rcc_PllNMult_t          N_Multiplier;

    /** Output P prescaler - divider (2, 4, 6, 8). Main PLL output P is the
     *  system clock source. Value 0 keeps the divider unchanged. */
    rcc_PllPDivider_t       P_Divider;

    /** Output Q prescaler - divider (2 - 15). Main PLL output Q is the 48 MHz
     *  clock (USB OTG FS, SDIO, RNG). Value 0 keeps the divider unchanged. */
    rcc_PllQDivider_t       Q_Divider;

    /** Output R prescaler - divider (2 - 7), available on selected MCUs only.
     *  Value 0 keeps the divider unchanged. */
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

    /** Specified frequency of the HSE oscillator (4 - 26 MHz). Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Configuration of PLLs (main PLL, PLLI2S and PLLSAI if available) */
    rcc_PllConfigStruct_t   Pll_Config[ RCC_PLL_CNT ];

    /**
    * \brief Enables or disables Clock Security System (CSS).
    *
    * If the CSS is enabled and a failure of HSE is detected, the HSE is
    * switched off, system clock is switched to HSI and NMI is generated.
    *
    * \warning When using the CSS, NMI handler has to handle the HSE failure
    *          (clear CSSF flag). Otherwise the NMI is generated repeatedly.
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
     *  states is calculated automatically, higher value can be forced (e.g.
     *  for supply voltage lower than 2.7 V). */
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
