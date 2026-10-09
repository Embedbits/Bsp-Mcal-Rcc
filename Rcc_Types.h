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
 * \note STM32G4 family - availability of peripherals depends on the selected
 *       MCU. Enumerators are guarded by RCC register bit definitions of the
 *       device header (e.g. RCC_APB1ENR1_TIM5EN), so only items existing on
 *       the selected MCU are available.
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
 * enumerator per clock source (e.g. \c RCC_PERIPH_USART1_HSI), the other
 * peripherals have single enumerator.
 *
 * \note Some peripherals has common clock source. If user tries to change clock
 *       source for this, error will be returned. User has to reset peripheral
 *       clock source and then configure it again.
 * \note ADC1 and ADC2 (ADC12) and ADC3, ADC4, ADC5 (ADC345) share one clock
 *       enable, reset control and kernel clock multiplexer per group. Kernel
 *       clock "HCLK" means that no asynchronous kernel clock is selected - the
 *       ADC is clocked synchronously from AHB (ADC CKMODE configured by ADC).
 * \note RNG and USB share the 48 MHz clock multiplexer (CLK48SEL), SAI1 and
 *       I2S of SPI2 / SPI3 can use the external I2S_CKIN clock. All FDCAN
 *       instances share one clock enable and kernel clock.
 */
typedef enum rcc_PeriphId_t
{
    /*------------------------------ System core -----------------------------*/
    RCC_PERIPH_FLASH          = 0u, /**< Flash memory interface clock enable */

    RCC_PERIPH_SYSCFG             , /**< System configuration controller clock enable (also COMP, OPAMP, VREFBUF) */
    RCC_PERIPH_PWR                , /**< Power interface clock enable */

    RCC_PERIPH_SYSTICK            , /**< System Tick timer clocked by processor clock (HCLK), no clock enable */

    RCC_PERIPH_IWDG               , /**< Independent watchdog clocked by Low Speed Internal (LSI) oscillator, no clock enable */

    RCC_PERIPH_RTC_HSE_DIV32      , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source */
    RCC_PERIPH_RTC_LSE            , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_PERIPH_RTC_LSI            , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

    RCC_PERIPH_RTCAPB             , /**< Real Time Clock APB interface clock enable */

    RCC_PERIPH_SRAM1              , /**< SRAM1 (clock control in sleep / stop mode only) */
#if defined(RCC_AHB2SMENR_SRAM2SMEN)
    RCC_PERIPH_SRAM2              , /**< SRAM2 (clock control in sleep / stop mode only) */
#endif /* RCC_AHB2SMENR_SRAM2SMEN */
    RCC_PERIPH_CCMSRAM            , /**< CCM SRAM (clock control in sleep / stop mode only) */

    RCC_PERIPH_DMA1               , /**< DMA1 clock enable */
    RCC_PERIPH_DMA2               , /**< DMA2 clock enable */
    RCC_PERIPH_DMAMUX1            , /**< DMA request multiplexer (DMAMUX1) clock enable */

    RCC_PERIPH_WWDG               , /**< Window watchdog clock enable */

    RCC_PERIPH_CRS                , /**< Clock Recovery System (CRS) clock enable */

    RCC_PERIPH_GPIOA              , /**< IO port A Clock Enable */
    RCC_PERIPH_GPIOB              , /**< IO port B Clock Enable */
    RCC_PERIPH_GPIOC              , /**< IO port C Clock Enable */
    RCC_PERIPH_GPIOD              , /**< IO port D Clock Enable */
    RCC_PERIPH_GPIOE              , /**< IO port E Clock Enable */
    RCC_PERIPH_GPIOF              , /**< IO port F Clock Enable */
    RCC_PERIPH_GPIOG              , /**< IO port G Clock Enable */

    /*-------------------------------- Timers --------------------------------*/

    RCC_PERIPH_TIM1               , /**< TIM1 Clock Enable */
    RCC_PERIPH_TIM2               , /**< TIM2 Clock Enable */
    RCC_PERIPH_TIM3               , /**< TIM3 Clock Enable */
    RCC_PERIPH_TIM4               , /**< TIM4 Clock Enable */
#if defined(RCC_APB1ENR1_TIM5EN)
    RCC_PERIPH_TIM5               , /**< TIM5 Clock Enable */
#endif /* RCC_APB1ENR1_TIM5EN */
    RCC_PERIPH_TIM6               , /**< TIM6 Clock Enable */
    RCC_PERIPH_TIM7               , /**< TIM7 Clock Enable */
    RCC_PERIPH_TIM8               , /**< TIM8 Clock Enable */
    RCC_PERIPH_TIM15              , /**< TIM15 Clock Enable */
    RCC_PERIPH_TIM16              , /**< TIM16 Clock Enable */
    RCC_PERIPH_TIM17              , /**< TIM17 Clock Enable */
#if defined(RCC_APB2ENR_TIM20EN)
    RCC_PERIPH_TIM20              , /**< TIM20 Clock Enable */
#endif /* RCC_APB2ENR_TIM20EN */
#if defined(RCC_APB2ENR_HRTIM1EN)
    RCC_PERIPH_HRTIM1             , /**< High Resolution Timer (HRTIM1) Clock Enable */
#endif /* RCC_APB2ENR_HRTIM1EN */

    RCC_PERIPH_LPTIM1_PCLK1       , /**< Low Power Timer 1 clock enable with APB1 (PCLK1) as clock source. */
    RCC_PERIPH_LPTIM1_LSI         , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) oscillator as clock source. */
    RCC_PERIPH_LPTIM1_HSI         , /**< Low Power Timer 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source. */
    RCC_PERIPH_LPTIM1_LSE         , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source. */

    /*----------------------------- Connectivity -----------------------------*/

    RCC_PERIPH_SPI1               , /**< SPI 1 Clock Enable (APB2 clock) */
    RCC_PERIPH_SPI2               , /**< SPI/I2S 2 Clock Enable (APB1 clock, I2S kernel clock by RCC_PERIPH_I2S23_xxx) */
#if defined(RCC_APB1ENR1_SPI3EN)
    RCC_PERIPH_SPI3               , /**< SPI/I2S 3 Clock Enable (APB1 clock, I2S kernel clock by RCC_PERIPH_I2S23_xxx) */
#endif /* RCC_APB1ENR1_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
    RCC_PERIPH_SPI4               , /**< SPI 4 Clock Enable (APB2 clock) */
#endif /* RCC_APB2ENR_SPI4EN */

#if defined(RCC_CCIPR_I2S23SEL)
    RCC_PERIPH_I2S23_SYSCLK       , /**< I2S kernel clock of SPI2 / SPI3 - system clock (SYSCLK), no clock enable */
    RCC_PERIPH_I2S23_PLLQ         , /**< I2S kernel clock of SPI2 / SPI3 - PLL output Q, no clock enable */
    RCC_PERIPH_I2S23_I2S_CKIN     , /**< I2S kernel clock of SPI2 / SPI3 - external I2S_CKIN pin, no clock enable */
    RCC_PERIPH_I2S23_HSI          , /**< I2S kernel clock of SPI2 / SPI3 - 16MHz High Speed Internal (HSI) oscillator, no clock enable */
#endif /* RCC_CCIPR_I2S23SEL */

    RCC_PERIPH_I2C1_PCLK1         , /**< I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C1_SYSCLK        , /**< I2C 1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C1_HSI           , /**< I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_I2C2_PCLK1         , /**< I2C 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C2_SYSCLK        , /**< I2C 2 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C2_HSI           , /**< I2C 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
#if defined(RCC_APB1ENR1_I2C3EN)
    RCC_PERIPH_I2C3_PCLK1         , /**< I2C 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C3_SYSCLK        , /**< I2C 3 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C3_HSI           , /**< I2C 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB1ENR1_I2C3EN */
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_PERIPH_I2C4_PCLK1         , /**< I2C 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C4_SYSCLK        , /**< I2C 4 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C4_HSI           , /**< I2C 4 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB1ENR2_I2C4EN */

    RCC_PERIPH_USART1_PCLK2       , /**< USART 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART1_SYSCLK      , /**< USART 1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART1_HSI         , /**< USART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_USART1_LSE         , /**< USART 1 clock enable with Low Speed External (LSE) oscillator as clock source */
    RCC_PERIPH_USART2_PCLK1       , /**< USART 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART2_SYSCLK      , /**< USART 2 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART2_HSI         , /**< USART 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_USART2_LSE         , /**< USART 2 clock enable with Low Speed External (LSE) oscillator as clock source */
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_PERIPH_USART3_PCLK1       , /**< USART 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART3_SYSCLK      , /**< USART 3 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART3_HSI         , /**< USART 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_USART3_LSE         , /**< USART 3 clock enable with Low Speed External (LSE) oscillator as clock source */
#endif /* RCC_APB1ENR1_USART3EN */
    RCC_PERIPH_UART4_PCLK1        , /**< UART 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_UART4_SYSCLK       , /**< UART 4 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_UART4_HSI          , /**< UART 4 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_UART4_LSE          , /**< UART 4 clock enable with Low Speed External (LSE) oscillator as clock source */
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_PERIPH_UART5_PCLK1        , /**< UART 5 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_UART5_SYSCLK       , /**< UART 5 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_UART5_HSI          , /**< UART 5 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_UART5_LSE          , /**< UART 5 clock enable with Low Speed External (LSE) oscillator as clock source */
#endif /* RCC_APB1ENR1_UART5EN */
    RCC_PERIPH_LPUART1_PCLK1      , /**< Low-Power UART 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_LPUART1_SYSCLK     , /**< Low-Power UART 1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_LPUART1_HSI        , /**< Low-Power UART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_LPUART1_LSE        , /**< Low-Power UART 1 clock enable with Low Speed External (LSE) oscillator as clock source */

    RCC_PERIPH_FDCAN_HSE          , /**< FDCAN clock enable with High Speed External oscillator (HSE) used as clock source */
    RCC_PERIPH_FDCAN_PLLQ         , /**< FDCAN clock enable with PLL output Q used as clock source */
    RCC_PERIPH_FDCAN_PCLK1        , /**< FDCAN clock enable with APB1 (PCLK1) used as clock source */

#if defined(RCC_APB1ENR1_USBEN)
    RCC_PERIPH_USB_HSI48          , /**< USB device clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
    RCC_PERIPH_USB_PLLQ           , /**< USB device clock enable with PLL output Q used as clock source */
#endif /* RCC_APB1ENR1_USBEN */
#if defined(RCC_APB1ENR2_UCPD1EN)
    RCC_PERIPH_UCPD1              , /**< USB Type-C / USB Power Delivery interface (UCPD1) clock enable */
#endif /* RCC_APB1ENR2_UCPD1EN */

#if defined(RCC_AHB3ENR_FMCEN)
    RCC_PERIPH_FMC                , /**< Flexible Memory Controller (FMC) clock enable */
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_PERIPH_QSPI_SYSCLK        , /**< Quad SPI memory interface clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_QSPI_HSI           , /**< Quad SPI memory interface clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_QSPI_PLLQ          , /**< Quad SPI memory interface clock enable with PLL output Q as clock source */
#endif /* RCC_AHB3ENR_QSPIEN */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(RCC_APB2ENR_SAI1EN)
    RCC_PERIPH_SAI1_SYSCLK        , /**< Serial Audio Interface 1 clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_SAI1_PLLQ          , /**< Serial Audio Interface 1 clock enable with PLL output Q as clock source */
    RCC_PERIPH_SAI1_I2S_CKIN      , /**< Serial Audio Interface 1 clock enable with external I2S_CKIN pin as clock source */
    RCC_PERIPH_SAI1_HSI           , /**< Serial Audio Interface 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
#endif /* RCC_APB2ENR_SAI1EN */

    /*-------------------------------- Analog --------------------------------*/

    RCC_PERIPH_ADC12_HCLK         , /**< ADC1 / ADC2 clock enable without asynchronous kernel clock - ADC clocked synchronously by HCLK */
    RCC_PERIPH_ADC12_PLLP         , /**< ADC1 / ADC2 clock enable with PLL output P as asynchronous kernel clock */
    RCC_PERIPH_ADC12_SYSCLK       , /**< ADC1 / ADC2 clock enable with system clock (SYSCLK) as asynchronous kernel clock */
#if defined(RCC_AHB2ENR_ADC345EN)
    RCC_PERIPH_ADC345_HCLK        , /**< ADC3 / ADC4 / ADC5 clock enable without asynchronous kernel clock - ADC clocked synchronously by HCLK */
    RCC_PERIPH_ADC345_PLLP        , /**< ADC3 / ADC4 / ADC5 clock enable with PLL output P as asynchronous kernel clock */
    RCC_PERIPH_ADC345_SYSCLK      , /**< ADC3 / ADC4 / ADC5 clock enable with system clock (SYSCLK) as asynchronous kernel clock */
#endif /* RCC_AHB2ENR_ADC345EN */

    RCC_PERIPH_DAC1               , /**< DAC 1 Clock Enable */
#if defined(RCC_AHB2ENR_DAC2EN)
    RCC_PERIPH_DAC2               , /**< DAC 2 Clock Enable */
#endif /* RCC_AHB2ENR_DAC2EN */
    RCC_PERIPH_DAC3               , /**< DAC 3 Clock Enable */
#if defined(RCC_AHB2ENR_DAC4EN)
    RCC_PERIPH_DAC4               , /**< DAC 4 Clock Enable */
#endif /* RCC_AHB2ENR_DAC4EN */

    /*------------------------------- Security -------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
    RCC_PERIPH_AES                , /**< Advanced Encryption Standard (AES) HW accelerator Clock Enable */
#endif /* RCC_AHB2ENR_AESEN */

    RCC_PERIPH_RNG_HSI48          , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
    RCC_PERIPH_RNG_PLLQ           , /**< Random Number Generator (RNG) clock enable with PLL output Q used as clock source */

    /*------------------------------- Computing ------------------------------*/

    RCC_PERIPH_CORDIC             , /**< CORDIC co-processor Clock Enable */
    RCC_PERIPH_CRC                , /**< CRC Clock Enable */
    RCC_PERIPH_FMAC               , /**< Filter Math (FMAC) accelerator Clock Enable */

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
    RCC_RESET_SRC_LPWR      , /**< Illegal Stop / Standby / Shutdown mode entry reset   */
    RCC_RESET_SRC_OBL       , /**< Option byte loader reset                             */
    RCC_RESET_SRC_CNT         /**< Number of reset sources                              */
}   rcc_ResetSrc_t;

/*------------------------ Clock sources configuration -----------------------*/

/** \brief Enumeration of High Speed External (HSE) input configuration
 *
 * \note STM32G4 HSE has one bypass mode for an external clock signal - analog
 *       and digital signal enumerators of STM32H5 interface select the same
 *       bypass mode as \ref RCC_HSE_TYPE_SIG_IN. */
typedef enum
{
    RCC_HSE_TYPE_NONE           = 0u,                  /**< No external clock connected to HSE pin              */
    RCC_HSE_TYPE_CRYSTAL            ,                  /**< External crystal/ceramic resonator                  */
    RCC_HSE_TYPE_SIG_IN             ,                  /**< External clock signal (oscillator bypassed)         */
    RCC_HSE_TYPE_SIG_ANALOG_IN  = RCC_HSE_TYPE_SIG_IN, /**< External clock signal (same as RCC_HSE_TYPE_SIG_IN) */
    RCC_HSE_TYPE_SIG_DIGITAL_IN = RCC_HSE_TYPE_SIG_IN, /**< External clock signal (same as RCC_HSE_TYPE_SIG_IN) */
}   rcc_HseType_t;


/** \brief Enumeration of Low Speed External (LSE) input configuration
 *
 * \note STM32G4 LSE has one bypass mode for an external clock signal - analog
 *       and digital signal enumerators of STM32H5 interface select the same
 *       bypass mode as \ref RCC_LSE_TYPE_SIG_IN. */
typedef enum
{
    RCC_LSE_TYPE_NONE           = 0u,                  /**< No external clock connected to LSE pin              */
    RCC_LSE_TYPE_CRYSTAL            ,                  /**< External crystal/ceramic resonator                  */
    RCC_LSE_TYPE_SIG_IN             ,                  /**< External clock signal (oscillator bypassed)         */
    RCC_LSE_TYPE_SIG_ANALOG_IN  = RCC_LSE_TYPE_SIG_IN, /**< External clock signal (same as RCC_LSE_TYPE_SIG_IN) */
    RCC_LSE_TYPE_SIG_DIGITAL_IN = RCC_LSE_TYPE_SIG_IN, /**< External clock signal (same as RCC_LSE_TYPE_SIG_IN) */
}   rcc_LseType_t;


/** \brief List of all available oscillators (except HSE, configured by \ref rcc_ConfigStruct_t)
 *
 * \note LSE is located in backup domain. The backup domain write protection
 *       is released by the module automatically. */
typedef enum
{
    RCC_OSC_HSI = 0u, /**< 16 MHz High Speed Internal (HSI16) oscillator.             */
    RCC_OSC_HSI48,    /**< 48 MHz High Speed Internal (HSI48) oscillator (USB, RNG).  */
    RCC_OSC_LSI,      /**< 32 kHz Low Speed Internal (LSI) oscillator.                */
    RCC_OSC_LSE,      /**< 32.768 kHz Low Speed External (LSE) crystal.               */
    RCC_OSC_CNT       /**< Count of available oscillators                             */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition.
 * \note Oscillators of STM32G4 family have no output divider - only value 1 is valid. */
typedef uint32_t rcc_OscDiv_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration
 * \note STM32G4 has one PLL (main PLL, outputs P, Q, R). */
typedef enum
{
    RCC_PLL_1 = 0u, /**< Main Phase Locked Loop (PLL) */
    RCC_PLL_CNT     /**< Count of available PLLs      */
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 *
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
 * must be in range 2.66 - 16 MHz.
 * Step size: 1
 * Range    : 1 - 16
 */
typedef uint32_t rcc_PllMDivider_t;


/** \brief Type used to signal values of PLL N multiplier
 * Phase Locked Loop (PLL) Feedback multiplier. VCO frequency must be in range
 * 96 - 344 MHz.
 * Step size: 1
 * Range    : 8 - 127
 */
typedef uint32_t rcc_PllNMult_t;


/** \brief Phase Locked Loop (PLL) P output divider value (ADC kernel clock).
 * Step size: 1
 * Range    : 2 - 31
 * Value 0  : Output is not configured (output disabled)
 */
typedef uint32_t rcc_PllPDivider_t;


/** \brief Phase Locked Loop (PLL) Q output divider value (48 MHz clock, FDCAN,
 * QSPI, I2S, SAI kernel clock).
 * Step size: 2
 * Range    : 2 - 8
 * Value 0  : Output is not configured (output disabled)
 */
typedef uint32_t rcc_PllQDivider_t;


/** \brief Phase Locked Loop (PLL) R output divider value (system clock).
 * Step size: 2
 * Range    : 2 - 8
 * Value 0  : Output is not configured (output disabled)
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
 * \note STM32G4 HSE divider for RTC is fixed - value 32.
 */
typedef uint16_t rcc_Rtc_HseDiv_t;

/*------------------------- Clock outputs configuration ----------------------*/

/**
 * \brief Clock Output identification enumeration.
 */
typedef enum
{
    RCC_CLK_OUT_MCO1 = 0u, /**< Master Clock Output (MCO, PA8)     */
    RCC_CLK_OUT_LSCO,      /**< Low Speed Clock Output (LSCO, PA2) */
    RCC_CLK_OUT_CNT        /**< Count of Clock Outputs             */
}   rcc_ClkOut_Id_t;


/**
 * \brief Clock Output's source configuration enumeration.
 */
typedef enum
{
    RCC_CLK_SOURCE_NONE   = 0u, /**< No clock source selected */

    RCC_CLK_SOURCE_MCO1_SYSCLK , /**< System clock (SYSCLK) will be used as Master Clock Output (MCO) clock source             */
    RCC_CLK_SOURCE_MCO1_HSI    , /**< 16MHz High Speed Internal (HSI) will be used as Master Clock Output (MCO) clock source   */
    RCC_CLK_SOURCE_MCO1_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output (MCO) clock source        */
    RCC_CLK_SOURCE_MCO1_PLLR   , /**< PLL output R will be used as Master Clock Output (MCO) clock source                     */
    RCC_CLK_SOURCE_MCO1_LSI    , /**< Low Speed Internal (LSI) will be used as Master Clock Output (MCO) clock source         */
    RCC_CLK_SOURCE_MCO1_LSE    , /**< Low Speed External (LSE) will be used as Master Clock Output (MCO) clock source         */
    RCC_CLK_SOURCE_MCO1_HSI48  , /**< 48MHz High Speed Internal (HSI48) will be used as Master Clock Output (MCO) clock source */

    RCC_CLK_SOURCE_LSCO_LSI    , /**< Low Speed Internal (LSI) will be used as Low Speed Clock Output (LSCO) clock source */
    RCC_CLK_SOURCE_LSCO_LSE    , /**< Low Speed External (LSE) will be used as Low Speed Clock Output (LSCO) clock source */
    RCC_CLK_SOURCE_CNT           /**< Count of clock output sources                                                       */
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


/** \brief System clock source multiplexer configuration list */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_HSI = 0u, /**< 16MHz HSI will be used as system clock source    */
    RCC_SYSTEM_CLOCK_SOURCE_HSE     , /**< HSE will be used as system clock source          */
    RCC_SYSTEM_CLOCK_SOURCE_PLL     , /**< PLL output R will be used as system clock source */
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
 * clock (HCLK) and voltage range (RM0440 "Number of wait states according to
 * CPU clock (HCLK) frequency"):
 *
 * | Wait states | Range 1 boost | Range 1 normal | Range 2   |
 * |-------------|---------------|----------------|-----------|
 * | 0 WS        | <= 34 MHz     | <= 30 MHz      | <= 12 MHz |
 * | 1 WS        | <= 68 MHz     | <= 60 MHz      | <= 24 MHz |
 * | 2 WS        | <= 102 MHz    | <= 90 MHz      | <= 26 MHz |
 * | 3 WS        | <= 136 MHz    | <= 120 MHz     |           |
 * | 4 WS        | <= 170 MHz    | <= 150 MHz     |           |
 *
 * Configured value is used as minimal number of wait states.
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
    RCC_FLASH_LATENCY_8_WS  = LL_FLASH_LATENCY_8,
    RCC_FLASH_LATENCY_9_WS  = LL_FLASH_LATENCY_9,
    RCC_FLASH_LATENCY_10_WS = LL_FLASH_LATENCY_10,
    RCC_FLASH_LATENCY_11_WS = LL_FLASH_LATENCY_11,
    RCC_FLASH_LATENCY_12_WS = LL_FLASH_LATENCY_12,
    RCC_FLASH_LATENCY_13_WS = LL_FLASH_LATENCY_13,
    RCC_FLASH_LATENCY_14_WS = LL_FLASH_LATENCY_14,
    RCC_FLASH_LATENCY_15_WS = LL_FLASH_LATENCY_15,
}   rcc_FlashLatency_t;


/**
 * \brief PWR Voltage scaling configuration (dynamic voltage scaling ranges).
 *
 * | Value                   | STM32G4 voltage range | Maximal system clock |
 * |-------------------------|-----------------------|----------------------|
 * | RCC_PWR_VOLTAGE_SCALE_0 | Range 1 boost mode    | 170 MHz              |
 * | RCC_PWR_VOLTAGE_SCALE_1 | Range 1 normal mode   | 150 MHz              |
 * | RCC_PWR_VOLTAGE_SCALE_2 | Range 2               | 26 MHz               |
 *
 * \note Power voltage scaling has to be chosen according to desired performance.
 */
typedef enum
{
    RCC_PWR_VOLTAGE_SCALE_0 = 0u, /**< Range 1 boost mode - highest performance (170 MHz) */
    RCC_PWR_VOLTAGE_SCALE_1     , /**< Range 1 normal mode (150 MHz)                      */
    RCC_PWR_VOLTAGE_SCALE_2     , /**< Range 2 - lowest consumption (26 MHz)              */
}   rcc_PwrVoltageScale_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - input divider (1 - 16). PLL input frequency must be in range 2.66 - 16 MHz */
    rcc_PllMDivider_t       M_Divider;

    /** N multiplier (8 - 127). VCO frequency must be in range 96 - 344 MHz */
    rcc_PllNMult_t          N_Multiplier;

    /** Output P prescaler - divider (2 - 31), ADC kernel clock. Value 0 keeps
     *  the output disabled. */
    rcc_PllPDivider_t       P_Divider;

    /** Output Q prescaler - divider (2, 4, 6, 8), 48 MHz clock (USB, RNG),
     *  FDCAN, QSPI, I2S, SAI kernel clock. Value 0 keeps the output disabled. */
    rcc_PllQDivider_t       Q_Divider;

    /** Output R prescaler - divider (2, 4, 6, 8), system clock. Value 0 keeps
     *  the output disabled. */
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

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Configuration of the PLL */
    rcc_PllConfigStruct_t   Pll_Config[ RCC_PLL_CNT ];

    /**
    * \brief Enables or disables Clock Security System (CSS).
    *
    * If the CSS is enabled and a failure of HSE is detected, the HSE is
    * switched off, system clock is switched to HSI and NMI is generated.
    *
    * \warning When using the CSS, NMI handler has to handle the HSE failure
    *          (clear CSSF flag). Otherwise the NMI is generated repeatedly.
    * \warning Once enabled, the CSS can't be turned off by software (only by
    *          reset or by HSE de-activation).
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
    * Scaling of internal voltage supply (dynamic voltage scaling range)
    */
    rcc_PwrVoltageScale_t    VoltageScaling;

    rcc_ClkOutConfigStruct_t McoConfig[ RCC_CLK_OUT_CNT ];

}   rcc_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* RCC_RCC_TYPES_H */
