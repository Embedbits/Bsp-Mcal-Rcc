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
 * Peripherals with kernel clock multiplexer have one ID per selectable clock
 * source (RCC_PERIPH_<peripheral>_<clock source>), other peripherals have one
 * ID clocked by their bus.
 *
 * \note Some peripherals has common clock source (ADC1 / ADC2, ADC4 and DAC1
 *       share ADCDACSEL, LPTIM3 and LPTIM4 share LPTIM34SEL, OCTOSPI1 and
 *       OCTOSPI2 share OCTOSPISEL). If user tries to change clock source used
 *       by another active peripheral, error will be returned. User has to
 *       release the peripheral clock source and then configure it again. */
typedef enum rcc_PeriphId_t
{
    /*------------------------------ System core -------------------------------*/

    RCC_PERIPH_FLASH       = 0u , /**< Flash memory interface clock enable */
    RCC_PERIPH_SYSCFG           , /**< System configuration controller (SYSCFG) clock enable */
    RCC_PERIPH_PWR              , /**< Power controller (PWR) clock enable */
    RCC_PERIPH_SYSTICK_HCLK_DIV8, /**< System Tick timer (SysTick) external reference clock enable with AHB clock (HCLK) divided by 8 as clock source */
    RCC_PERIPH_SYSTICK_LSI      , /**< System Tick timer (SysTick) external reference clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_SYSTICK_LSE      , /**< System Tick timer (SysTick) external reference clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_IWDG             , /**< Independent watchdog (IWDG), clocked by LSI oscillator - LSI is started, no bus clock */
    RCC_PERIPH_RTC_LSE          , /**< Real Time Clock (RTC) APB interface clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_RTC_LSI          , /**< Real Time Clock (RTC) APB interface clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_RTC_HSE_DIV32    , /**< Real Time Clock (RTC) APB interface clock enable with High Speed External oscillator (HSE) divided by 32 as clock source */
    RCC_PERIPH_CRS              , /**< Clock recovery system (CRS) clock enable */
    RCC_PERIPH_WWDG             , /**< Window watchdog (WWDG) clock enable */
    RCC_PERIPH_RAMCFG           , /**< RAM configuration controller (RAMCFG) clock enable */
    RCC_PERIPH_BKPSRAM          , /**< Backup SRAM clock enable */
    RCC_PERIPH_SRAM1            , /**< SRAM1 clock enable */
    RCC_PERIPH_SRAM2            , /**< SRAM2 clock enable */
#if defined(RCC_AHB2ENR1_SRAM3EN)
    RCC_PERIPH_SRAM3            , /**< SRAM3 clock enable */
#endif /* SRAM3 */
    RCC_PERIPH_SRAM4            , /**< SRAM4 clock enable */
#if defined(RCC_AHB2ENR2_SRAM5EN)
    RCC_PERIPH_SRAM5            , /**< SRAM5 clock enable */
#endif /* SRAM5 */
#if defined(RCC_AHB2ENR2_SRAM6EN)
    RCC_PERIPH_SRAM6            , /**< SRAM6 clock enable */
#endif /* SRAM6 */
    RCC_PERIPH_DCACHE1          , /**< Data cache 1 (DCACHE1) clock enable */
#if defined(DCACHE2)
    RCC_PERIPH_DCACHE2          , /**< Data cache 2 (DCACHE2) clock enable */
#endif /* DCACHE2 */
    RCC_PERIPH_GTZC1            , /**< Global TrustZone controller 1 (GTZC1) clock enable */
    RCC_PERIPH_GTZC2            , /**< Global TrustZone controller 2 (GTZC2) clock enable */
    RCC_PERIPH_GPDMA1           , /**< General purpose DMA 1 (GPDMA1) clock enable */
    RCC_PERIPH_LPDMA1           , /**< Low power DMA 1 (LPDMA1) clock enable */
    RCC_PERIPH_GPIOA            , /**< IO port A clock enable */
    RCC_PERIPH_GPIOB            , /**< IO port B clock enable */
    RCC_PERIPH_GPIOC            , /**< IO port C clock enable */
    RCC_PERIPH_GPIOD            , /**< IO port D clock enable */
    RCC_PERIPH_GPIOE            , /**< IO port E clock enable */
#if defined(GPIOF)
    RCC_PERIPH_GPIOF            , /**< IO port F clock enable */
#endif /* GPIOF */
    RCC_PERIPH_GPIOG            , /**< IO port G clock enable */
    RCC_PERIPH_GPIOH            , /**< IO port H clock enable */
#if defined(GPIOI)
    RCC_PERIPH_GPIOI            , /**< IO port I clock enable */
#endif /* GPIOI */
#if defined(GPIOJ)
    RCC_PERIPH_GPIOJ            , /**< IO port J clock enable */
#endif /* GPIOJ */
    RCC_PERIPH_LPGPIO1          , /**< Low power general purpose IO (LPGPIO1) clock enable */

    /*--------------------------------- Timers ---------------------------------*/

    RCC_PERIPH_TIM1             , /**< Timer 1 (TIM1) clock enable */
    RCC_PERIPH_TIM2             , /**< Timer 2 (TIM2) clock enable */
    RCC_PERIPH_TIM3             , /**< Timer 3 (TIM3) clock enable */
    RCC_PERIPH_TIM4             , /**< Timer 4 (TIM4) clock enable */
    RCC_PERIPH_TIM5             , /**< Timer 5 (TIM5) clock enable */
    RCC_PERIPH_TIM6             , /**< Timer 6 (TIM6) clock enable */
    RCC_PERIPH_TIM7             , /**< Timer 7 (TIM7) clock enable */
    RCC_PERIPH_TIM8             , /**< Timer 8 (TIM8) clock enable */
    RCC_PERIPH_TIM15            , /**< Timer 15 (TIM15) clock enable */
    RCC_PERIPH_TIM16            , /**< Timer 16 (TIM16) clock enable */
    RCC_PERIPH_TIM17            , /**< Timer 17 (TIM17) clock enable */
    RCC_PERIPH_LPTIM1_MSIK      , /**< Low power timer 1 (LPTIM1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_LPTIM1_LSI       , /**< Low power timer 1 (LPTIM1) clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_LPTIM1_HSI       , /**< Low power timer 1 (LPTIM1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_LPTIM1_LSE       , /**< Low power timer 1 (LPTIM1) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_LPTIM2_PCLK1     , /**< Low power timer 2 (LPTIM2) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_LPTIM2_LSI       , /**< Low power timer 2 (LPTIM2) clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_LPTIM2_HSI       , /**< Low power timer 2 (LPTIM2) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_LPTIM2_LSE       , /**< Low power timer 2 (LPTIM2) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_LPTIM3_MSIK      , /**< Low power timer 3 (LPTIM3) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_LPTIM3_LSI       , /**< Low power timer 3 (LPTIM3) clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_LPTIM3_HSI       , /**< Low power timer 3 (LPTIM3) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_LPTIM3_LSE       , /**< Low power timer 3 (LPTIM3) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_LPTIM4_MSIK      , /**< Low power timer 4 (LPTIM4) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_LPTIM4_LSI       , /**< Low power timer 4 (LPTIM4) clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_LPTIM4_HSI       , /**< Low power timer 4 (LPTIM4) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_LPTIM4_LSE       , /**< Low power timer 4 (LPTIM4) clock enable with Low Speed External oscillator (LSE) as clock source */

    /*------------------------------ Connectivity ------------------------------*/

    RCC_PERIPH_SPI1_PCLK2       , /**< Serial peripheral interface 1 (SPI1) clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_SPI1_SYSCLK      , /**< Serial peripheral interface 1 (SPI1) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_SPI1_HSI         , /**< Serial peripheral interface 1 (SPI1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_SPI1_MSIK        , /**< Serial peripheral interface 1 (SPI1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_SPI2_PCLK1       , /**< Serial peripheral interface 2 (SPI2) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_SPI2_SYSCLK      , /**< Serial peripheral interface 2 (SPI2) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_SPI2_HSI         , /**< Serial peripheral interface 2 (SPI2) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_SPI2_MSIK        , /**< Serial peripheral interface 2 (SPI2) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_SPI3_PCLK3       , /**< Serial peripheral interface 3 (SPI3) clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_SPI3_SYSCLK      , /**< Serial peripheral interface 3 (SPI3) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_SPI3_HSI         , /**< Serial peripheral interface 3 (SPI3) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_SPI3_MSIK        , /**< Serial peripheral interface 3 (SPI3) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_I2C1_PCLK1       , /**< Inter-integrated circuit interface 1 (I2C1) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C1_SYSCLK      , /**< Inter-integrated circuit interface 1 (I2C1) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C1_HSI         , /**< Inter-integrated circuit interface 1 (I2C1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_I2C1_MSIK        , /**< Inter-integrated circuit interface 1 (I2C1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_I2C2_PCLK1       , /**< Inter-integrated circuit interface 2 (I2C2) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C2_SYSCLK      , /**< Inter-integrated circuit interface 2 (I2C2) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C2_HSI         , /**< Inter-integrated circuit interface 2 (I2C2) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_I2C2_MSIK        , /**< Inter-integrated circuit interface 2 (I2C2) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_I2C3_PCLK3       , /**< Inter-integrated circuit interface 3 (I2C3) clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_I2C3_SYSCLK      , /**< Inter-integrated circuit interface 3 (I2C3) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C3_HSI         , /**< Inter-integrated circuit interface 3 (I2C3) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_I2C3_MSIK        , /**< Inter-integrated circuit interface 3 (I2C3) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_I2C4_PCLK1       , /**< Inter-integrated circuit interface 4 (I2C4) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C4_SYSCLK      , /**< Inter-integrated circuit interface 4 (I2C4) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C4_HSI         , /**< Inter-integrated circuit interface 4 (I2C4) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_I2C4_MSIK        , /**< Inter-integrated circuit interface 4 (I2C4) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
#if defined(I2C5)
    RCC_PERIPH_I2C5_PCLK1       , /**< Inter-integrated circuit interface 5 (I2C5) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C5_SYSCLK      , /**< Inter-integrated circuit interface 5 (I2C5) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C5_HSI         , /**< Inter-integrated circuit interface 5 (I2C5) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_I2C5_MSIK        , /**< Inter-integrated circuit interface 5 (I2C5) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
#endif /* I2C5 */
#if defined(I2C6)
    RCC_PERIPH_I2C6_PCLK1       , /**< Inter-integrated circuit interface 6 (I2C6) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C6_SYSCLK      , /**< Inter-integrated circuit interface 6 (I2C6) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_I2C6_HSI         , /**< Inter-integrated circuit interface 6 (I2C6) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_I2C6_MSIK        , /**< Inter-integrated circuit interface 6 (I2C6) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
#endif /* I2C6 */
    RCC_PERIPH_USART1_PCLK2     , /**< Universal synchronous asynchronous receiver transmitter 1 (USART1) clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART1_SYSCLK    , /**< Universal synchronous asynchronous receiver transmitter 1 (USART1) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART1_HSI       , /**< Universal synchronous asynchronous receiver transmitter 1 (USART1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_USART1_LSE       , /**< Universal synchronous asynchronous receiver transmitter 1 (USART1) clock enable with Low Speed External oscillator (LSE) as clock source */
#if defined(USART2)
    RCC_PERIPH_USART2_PCLK1     , /**< Universal synchronous asynchronous receiver transmitter 2 (USART2) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART2_SYSCLK    , /**< Universal synchronous asynchronous receiver transmitter 2 (USART2) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART2_HSI       , /**< Universal synchronous asynchronous receiver transmitter 2 (USART2) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_USART2_LSE       , /**< Universal synchronous asynchronous receiver transmitter 2 (USART2) clock enable with Low Speed External oscillator (LSE) as clock source */
#endif /* USART2 */
    RCC_PERIPH_USART3_PCLK1     , /**< Universal synchronous asynchronous receiver transmitter 3 (USART3) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART3_SYSCLK    , /**< Universal synchronous asynchronous receiver transmitter 3 (USART3) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART3_HSI       , /**< Universal synchronous asynchronous receiver transmitter 3 (USART3) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_USART3_LSE       , /**< Universal synchronous asynchronous receiver transmitter 3 (USART3) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_UART4_PCLK1      , /**< Universal asynchronous receiver transmitter 4 (UART4) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_UART4_SYSCLK     , /**< Universal asynchronous receiver transmitter 4 (UART4) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_UART4_HSI        , /**< Universal asynchronous receiver transmitter 4 (UART4) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_UART4_LSE        , /**< Universal asynchronous receiver transmitter 4 (UART4) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_UART5_PCLK1      , /**< Universal asynchronous receiver transmitter 5 (UART5) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_UART5_SYSCLK     , /**< Universal asynchronous receiver transmitter 5 (UART5) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_UART5_HSI        , /**< Universal asynchronous receiver transmitter 5 (UART5) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_UART5_LSE        , /**< Universal asynchronous receiver transmitter 5 (UART5) clock enable with Low Speed External oscillator (LSE) as clock source */
#if defined(USART6)
    RCC_PERIPH_USART6_PCLK1     , /**< Universal synchronous asynchronous receiver transmitter 6 (USART6) clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART6_SYSCLK    , /**< Universal synchronous asynchronous receiver transmitter 6 (USART6) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_USART6_HSI       , /**< Universal synchronous asynchronous receiver transmitter 6 (USART6) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_USART6_LSE       , /**< Universal synchronous asynchronous receiver transmitter 6 (USART6) clock enable with Low Speed External oscillator (LSE) as clock source */
#endif /* USART6 */
    RCC_PERIPH_LPUART1_PCLK3    , /**< Low power universal asynchronous receiver transmitter 1 (LPUART1) clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_LPUART1_SYSCLK   , /**< Low power universal asynchronous receiver transmitter 1 (LPUART1) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_LPUART1_HSI      , /**< Low power universal asynchronous receiver transmitter 1 (LPUART1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_LPUART1_LSE      , /**< Low power universal asynchronous receiver transmitter 1 (LPUART1) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_LPUART1_MSIK     , /**< Low power universal asynchronous receiver transmitter 1 (LPUART1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_FDCAN1_HSE       , /**< Controller area network with flexible data rate 1 (FDCAN1) clock enable with High Speed External oscillator (HSE) as clock source */
    RCC_PERIPH_FDCAN1_PLL1Q     , /**< Controller area network with flexible data rate 1 (FDCAN1) clock enable with PLL1 output Q as clock source */
    RCC_PERIPH_FDCAN1_PLL2P     , /**< Controller area network with flexible data rate 1 (FDCAN1) clock enable with PLL2 output P as clock source */
#if defined(RCC_APB2ENR_USBEN)
    RCC_PERIPH_USB_HSI48        , /**< USB full speed device (USB DRD FS) clock enable with 48 MHz High Speed Internal oscillator (HSI48) as clock source */
    RCC_PERIPH_USB_PLL2Q        , /**< USB full speed device (USB DRD FS) clock enable with PLL2 output Q as clock source */
    RCC_PERIPH_USB_PLL1Q        , /**< USB full speed device (USB DRD FS) clock enable with PLL1 output Q as clock source */
    RCC_PERIPH_USB_MSIK         , /**< USB full speed device (USB DRD FS) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
    RCC_PERIPH_USB_HSI48        , /**< USB on-the-go (OTG FS / OTG HS) clock enable with 48 MHz High Speed Internal oscillator (HSI48) as clock source */
    RCC_PERIPH_USB_PLL2Q        , /**< USB on-the-go (OTG FS / OTG HS) clock enable with PLL2 output Q as clock source */
    RCC_PERIPH_USB_PLL1Q        , /**< USB on-the-go (OTG FS / OTG HS) clock enable with PLL1 output Q as clock source */
    RCC_PERIPH_USB_MSIK         , /**< USB on-the-go (OTG FS / OTG HS) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
#endif /* OTG */
#if defined(RCC_AHB2ENR1_USBPHYCEN)
    RCC_PERIPH_USBPHYC          , /**< USB OTG HS PHY controller (USBPHYC) clock enable */
#endif /* USBPHYC */
#if defined(UCPD1)
    RCC_PERIPH_UCPD1            , /**< USB Type-C power delivery 1 (UCPD1) clock enable */
#endif /* UCPD1 */
    RCC_PERIPH_OCTOSPI1_SYSCLK  , /**< OctoSPI 1 (OCTOSPI1) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_OCTOSPI1_MSIK    , /**< OctoSPI 1 (OCTOSPI1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_OCTOSPI1_PLL1Q   , /**< OctoSPI 1 (OCTOSPI1) clock enable with PLL1 output Q as clock source */
    RCC_PERIPH_OCTOSPI1_PLL2Q   , /**< OctoSPI 1 (OCTOSPI1) clock enable with PLL2 output Q as clock source */
#if defined(OCTOSPI2)
    RCC_PERIPH_OCTOSPI2_SYSCLK  , /**< OctoSPI 2 (OCTOSPI2) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_OCTOSPI2_MSIK    , /**< OctoSPI 2 (OCTOSPI2) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_OCTOSPI2_PLL1Q   , /**< OctoSPI 2 (OCTOSPI2) clock enable with PLL1 output Q as clock source */
    RCC_PERIPH_OCTOSPI2_PLL2Q   , /**< OctoSPI 2 (OCTOSPI2) clock enable with PLL2 output Q as clock source */
#endif /* OCTOSPI2 */
#if defined(OCTOSPIM)
    RCC_PERIPH_OCTOSPIM         , /**< OctoSPI IO manager (OCTOSPIM) clock enable */
#endif /* OCTOSPIM */
#if defined(HSPI1)
    RCC_PERIPH_HSPI1            , /**< Hexadeca-SPI 1 (HSPI1) clock enable */
#endif /* HSPI1 */
#if defined(RCC_AHB2ENR2_FSMCEN)
    RCC_PERIPH_FMC              , /**< Flexible memory controller (FMC) clock enable */
#endif /* FMC */
    RCC_PERIPH_SDMMC1           , /**< SD / SDIO / MMC card host interface 1 (SDMMC1) clock enable */
#if defined(SDMMC2)
    RCC_PERIPH_SDMMC2           , /**< SD / SDIO / MMC card host interface 2 (SDMMC2) clock enable */
#endif /* SDMMC2 */

    /*------------------------------- Multimedia -------------------------------*/

    RCC_PERIPH_DCMI_PSSI        , /**< Digital camera interface (DCMI) and parallel synchronous slave interface (PSSI) clock enable */
    RCC_PERIPH_SAI1_PLL2P       , /**< Serial audio interface 1 (SAI1) clock enable with PLL2 output P as clock source */
    RCC_PERIPH_SAI1_PLL3P       , /**< Serial audio interface 1 (SAI1) clock enable with PLL3 output P as clock source */
    RCC_PERIPH_SAI1_PLL1P       , /**< Serial audio interface 1 (SAI1) clock enable with PLL1 output P as clock source */
    RCC_PERIPH_SAI1_PIN         , /**< Serial audio interface 1 (SAI1) clock enable with external clock input pin as clock source */
    RCC_PERIPH_SAI1_HSI         , /**< Serial audio interface 1 (SAI1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
#if defined(SAI2)
    RCC_PERIPH_SAI2_PLL2P       , /**< Serial audio interface 2 (SAI2) clock enable with PLL2 output P as clock source */
    RCC_PERIPH_SAI2_PLL3P       , /**< Serial audio interface 2 (SAI2) clock enable with PLL3 output P as clock source */
    RCC_PERIPH_SAI2_PLL1P       , /**< Serial audio interface 2 (SAI2) clock enable with PLL1 output P as clock source */
    RCC_PERIPH_SAI2_PIN         , /**< Serial audio interface 2 (SAI2) clock enable with external clock input pin as clock source */
    RCC_PERIPH_SAI2_HSI         , /**< Serial audio interface 2 (SAI2) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
#endif /* SAI2 */
    RCC_PERIPH_MDF1_HCLK        , /**< Multi-function digital filter 1 (MDF1) clock enable with AHB (HCLK) as clock source */
    RCC_PERIPH_MDF1_PLL1P       , /**< Multi-function digital filter 1 (MDF1) clock enable with PLL1 output P as clock source */
    RCC_PERIPH_MDF1_PLL3Q       , /**< Multi-function digital filter 1 (MDF1) clock enable with PLL3 output Q as clock source */
    RCC_PERIPH_MDF1_PIN         , /**< Multi-function digital filter 1 (MDF1) clock enable with external clock input pin as clock source */
    RCC_PERIPH_MDF1_MSIK        , /**< Multi-function digital filter 1 (MDF1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_ADF1_HCLK        , /**< Audio digital filter 1 (ADF1) clock enable with AHB (HCLK) as clock source */
    RCC_PERIPH_ADF1_PLL1P       , /**< Audio digital filter 1 (ADF1) clock enable with PLL1 output P as clock source */
    RCC_PERIPH_ADF1_PLL3Q       , /**< Audio digital filter 1 (ADF1) clock enable with PLL3 output Q as clock source */
    RCC_PERIPH_ADF1_PIN         , /**< Audio digital filter 1 (ADF1) clock enable with external clock input pin as clock source */
    RCC_PERIPH_ADF1_MSIK        , /**< Audio digital filter 1 (ADF1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
#if defined(DMA2D)
    RCC_PERIPH_DMA2D            , /**< Chrom-ART accelerator (DMA2D) clock enable */
#endif /* DMA2D */
#if defined(GPU2D)
    RCC_PERIPH_GPU2D            , /**< Neo-Chrom graphic processor (GPU2D) clock enable */
#endif /* GPU2D */
#if defined(GFXMMU)
    RCC_PERIPH_GFXMMU           , /**< Chrom-GRC graphic MMU (GFXMMU) clock enable */
#endif /* GFXMMU */
#if defined(GFXTIM)
    RCC_PERIPH_GFXTIM           , /**< Graphic timer (GFXTIM) clock enable */
#endif /* GFXTIM */
#if defined(JPEG)
    RCC_PERIPH_JPEG             , /**< JPEG codec (JPEG) clock enable */
#endif /* JPEG */
#if defined(LTDC)
    RCC_PERIPH_LTDC             , /**< LCD-TFT display controller (LTDC) clock enable */
#endif /* LTDC */
#if defined(DSI)
    RCC_PERIPH_DSIHOST          , /**< Display serial interface host (DSI) clock enable */
#endif /* DSIHOST */
    RCC_PERIPH_TSC              , /**< Touch sensing controller (TSC) clock enable */

    /*--------------------------------- Analog ---------------------------------*/

    RCC_PERIPH_ADC_HCLK         , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) clock enable with AHB (HCLK) as clock source */
    RCC_PERIPH_ADC_SYSCLK       , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_ADC_PLL2R        , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) clock enable with PLL2 output R as clock source */
    RCC_PERIPH_ADC_HSE          , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) clock enable with High Speed External oscillator (HSE) as clock source */
    RCC_PERIPH_ADC_HSI          , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_ADC_MSIK         , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_ADC4_HCLK        , /**< Analog-to-digital converter 4 (ADC4) clock enable with AHB (HCLK) as clock source */
    RCC_PERIPH_ADC4_SYSCLK      , /**< Analog-to-digital converter 4 (ADC4) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_ADC4_PLL2R       , /**< Analog-to-digital converter 4 (ADC4) clock enable with PLL2 output R as clock source */
    RCC_PERIPH_ADC4_HSE         , /**< Analog-to-digital converter 4 (ADC4) clock enable with High Speed External oscillator (HSE) as clock source */
    RCC_PERIPH_ADC4_HSI         , /**< Analog-to-digital converter 4 (ADC4) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_ADC4_MSIK        , /**< Analog-to-digital converter 4 (ADC4) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_DAC_HCLK         , /**< Digital-to-analog converter 1 (DAC1) clock enable with AHB (HCLK) as clock source */
    RCC_PERIPH_DAC_SYSCLK       , /**< Digital-to-analog converter 1 (DAC1) clock enable with system clock (SYSCLK) as clock source */
    RCC_PERIPH_DAC_PLL2R        , /**< Digital-to-analog converter 1 (DAC1) clock enable with PLL2 output R as clock source */
    RCC_PERIPH_DAC_HSE          , /**< Digital-to-analog converter 1 (DAC1) clock enable with High Speed External oscillator (HSE) as clock source */
    RCC_PERIPH_DAC_HSI          , /**< Digital-to-analog converter 1 (DAC1) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */
    RCC_PERIPH_DAC_MSIK         , /**< Digital-to-analog converter 1 (DAC1) clock enable with Multi-Speed Internal oscillator kernel output (MSIK) as clock source */
    RCC_PERIPH_DAC_SAH_LSE      , /**< Digital-to-analog converter 1 (DAC1) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_DAC_SAH_LSI      , /**< Digital-to-analog converter 1 (DAC1) clock enable with Low Speed Internal oscillator (LSI) as clock source */
    RCC_PERIPH_COMP             , /**< Comparators (COMP) clock enable */
    RCC_PERIPH_OPAMP            , /**< Operational amplifiers (OPAMP) clock enable */
    RCC_PERIPH_VREF             , /**< Voltage reference buffer (VREFBUF) clock enable */

    /*-------------------------------- Security --------------------------------*/

#if defined(AES)
    RCC_PERIPH_AES              , /**< Advanced encryption standard hardware accelerator (AES) clock enable */
#endif /* AES */
#if defined(SAES)
    RCC_PERIPH_SAES             , /**< Secure advanced encryption standard hardware accelerator (SAES) clock enable */
#endif /* SAES */
    RCC_PERIPH_HASH             , /**< Hash processor (HASH) clock enable */
#if defined(PKA)
    RCC_PERIPH_PKA              , /**< Public key accelerator (PKA) clock enable */
#endif /* PKA */
#if defined(OTFDEC1)
    RCC_PERIPH_OTFDEC1          , /**< On-the-fly decryption engine 1 (OTFDEC1) clock enable */
#endif /* OTFDEC1 */
#if defined(OTFDEC2)
    RCC_PERIPH_OTFDEC2          , /**< On-the-fly decryption engine 2 (OTFDEC2) clock enable */
#endif /* OTFDEC2 */
    RCC_PERIPH_RNG_HSI48        , /**< True random number generator (RNG) clock enable with 48 MHz High Speed Internal oscillator (HSI48) as clock source */
    RCC_PERIPH_RNG_HSI48_DIV2   , /**< True random number generator (RNG) clock enable with 48 MHz High Speed Internal oscillator (HSI48) divided by 2 as clock source */
    RCC_PERIPH_RNG_HSI          , /**< True random number generator (RNG) clock enable with 16 MHz High Speed Internal oscillator (HSI16) as clock source */

    /*------------------------------- Computing --------------------------------*/

    RCC_PERIPH_CORDIC           , /**< CORDIC co-processor (CORDIC) clock enable */
    RCC_PERIPH_CRC              , /**< Cyclic redundancy check calculation unit (CRC) clock enable */
    RCC_PERIPH_FMAC             , /**< Filter mathematical accelerator (FMAC) clock enable */

    RCC_PERIPH_ID_CNT
}   rcc_PeriphId_t;

/*---------------------------- Reset source flags ----------------------------*/

/** \brief List of reset sources stored in RCC control / status register (CSR)
 *
 * \note BOR flag is set also after power-on reset. */
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

/** \brief Enumeration of High Speed External (HSE) input configuration */
typedef enum
{
    RCC_HSE_TYPE_NONE     = 0u , /**< No external clock connected to HSE pin                    */
    RCC_HSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator                        */
    RCC_HSE_TYPE_SIG_ANALOG_IN , /**< External clock signal, analog (sine) input - HSE bypassed */
    RCC_HSE_TYPE_SIG_DIGITAL_IN, /**< External clock signal, digital input - HSE bypassed       */
}   rcc_HseType_t;


/** \brief Enumeration of Low Speed External (LSE) input configuration */
typedef enum
{
    RCC_LSE_TYPE_NONE     = 0u , /**< No external clock connected to LSE pin */
    RCC_LSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_LSE_TYPE_SIG_ANALOG_IN , /**< External low voltage swing signal      */
    RCC_LSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_LseType_t;


/** \brief List of all available oscillators (except HSE, configured by \ref rcc_ConfigStruct_t)
 *
 * \note LSE and LSI are located in backup domain. The backup domain write
 *       protection is released by the module automatically. */
typedef enum
{
    RCC_OSC_HSI16 = 0u, /**< 16 MHz High Speed Internal (HSI16) oscillator.                          */
    RCC_OSC_HSI48,      /**< 48 MHz High Speed Internal (HSI48) oscillator.                          */
    RCC_OSC_MSIS,       /**< Multi-Speed Internal oscillator, system clock output (MSIS, 4 MHz reset) */
    RCC_OSC_MSIK,       /**< Multi-Speed Internal oscillator, kernel clock output (MSIK, 4 MHz reset) */
    RCC_OSC_LSI,        /**< 32 kHz Low Speed Internal (LSI) oscillator.                             */
    RCC_OSC_LSE,        /**< 32.768 kHz Low Speed External (LSE) crystal.                            */
    RCC_OSC_CNT         /**< Count of available oscillators                                          */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition.
 *
 * - MSIS / MSIK: division of 48 MHz MSI reference (MSIRC0 / MSIRC1 / MSIRC3
 *   ranges): 1, 2, 3, 4 (48 / 24 / 16 / 12 MHz), 12, 24, 36, 48 (4 / 2 /
 *   1.33 / 1 MHz) and 120, 240, 360, 480 (400 / 200 / 133 / 100 kHz). Ranges
 *   of 3.072 MHz MSI reference (MSIRC2) are not supported.
 * - LSI: 1 or 128 (LSI prescaler LSIPREDIV).
 * - Other oscillators have no output divider - only value 1 is valid.
 */
typedef uint32_t rcc_OscDiv_t;

/*--------------- Power supply validity and HSI48 automatic trimming ---------*/

/**
 * \brief Supplies validated by software (PWR register access is part of the RCC module)
 *
 * Independent supply domains are electrically and logically connected to the core only
 * after the software validates the supply (PWR_SVMCR). \ref Rcc_Set_PeriphActive validates
 * the supply of the peripheral automatically, the functions of the power supply validity
 * allow the application to validate / isolate the domain explicitly.
 */
typedef enum
{
    RCC_PWR_SUPPLY_VDDUSB = 0u, /**< VDDUSB supply (PWR_SVMCR.USV) of the USB peripheral                 */
    RCC_PWR_SUPPLY_VDDIO2,      /**< VDDIO2 supply (PWR_SVMCR.IO2SV) of the pins PG[15:2]                 */
    RCC_PWR_SUPPLY_VDDA,        /**< VDDA supply (PWR_SVMCR.ASV) of ADC, DAC, COMP, OPAMP and VREFBUF     */
    RCC_PWR_SUPPLY_CNT          /**< Count of available supplies                                         */
}   rcc_PwrSupplyId_t;


/**
 * \brief Synchronization source of the HSI48 automatic trimming (Clock Recovery System, CRS)
 *
 * The CRS compares the HSI48 frequency with the synchronization signal and trims the
 * oscillator, so the 48 MHz clock keeps the accuracy required by USB.
 */
typedef enum
{
    RCC_HSI48_TRIM_SRC_USB_SOF = 0u, /**< USB start of frame (1 kHz)       */
    RCC_HSI48_TRIM_SRC_LSE,          /**< LSE oscillator (32.768 kHz)      */
    RCC_HSI48_TRIM_SRC_CNT           /**< Count of synchronization sources */
}   rcc_Hsi48TrimSrc_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration */
typedef enum
{
    RCC_PLL_1 = 0u, /**< Phase Locked Loop 1 (system clock source - output R) */
    RCC_PLL_2,      /**< Phase Locked Loop 2                                  */
    RCC_PLL_3,      /**< Phase Locked Loop 3                                  */
    RCC_PLL_CNT     /**< Count of available PLLs                              */
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 * \note If the PLL is not used, select RCC_PLL_SRC_NONE. Otherwise will be
 *       PLL activated. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                                     */
    RCC_PLL_SRC_MSIS     , /**< PLL will be clocked by Multi-Speed Internal (MSIS)  */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator               */
    RCC_PLL_SRC_HSI      , /**< PLL will be clocked by 16MHz HSI oscillator         */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                         */
}   rcc_PllClkSrc_t;


/** \brief Phase Locked Loop (PLL) M Divider value.
 * This is clock input divider for PLL. PLL input frequency (after divider)
 * must be in range 4 - 16 MHz.
 * Step size: 1
 * Range    : 1 - 16
 */
typedef uint32_t rcc_PllMDivider_t;


/** \brief Type used to signal values of PLL N multiplier
 * Phase Locked Loop (PLL) Feedback multiplier. VCO frequency must be in range
 * 128 - 544 MHz.
 * Step size: 1
 * Range    : 4 - 512
 */
typedef uint32_t rcc_PllNMult_t;


/** \brief Phase Locked Loop (PLL) P output divider value.
 * Step size: 1
 * Range    : 1 - 128
 * Value 0  : Output divider is not configured (output stays disabled)
 */
typedef uint32_t rcc_PllPDivider_t;


/** \brief Phase Locked Loop (PLL) Q output divider value.
 * Step size: 1
 * Range    : 1 - 128
 * Value 0  : Output divider is not configured (output stays disabled)
 */
typedef uint32_t rcc_PllQDivider_t;


/** \brief Phase Locked Loop (PLL) R output divider value.
 * Step size: 1
 * Range    : 1 - 128 (PLL1 R output drives system clock - 1 or even values)
 * Value 0  : Output divider is not configured (output stays disabled)
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
 * \note  STM32U5 divides HSE for RTC by fixed value 32.
 */
typedef uint16_t rcc_Rtc_HseDiv_t;

/*------------------------- Clock outputs configuration ----------------------*/

/**
 * \brief Clock Output identification enumeration.
 */
typedef enum
{
    RCC_CLK_OUT_MCO1 = 0u, /**< Master Clock Output (MCO, PA8)       */
    RCC_CLK_OUT_LSCO,      /**< Low Speed Clock Output (LSCO, PA2)   */
    RCC_CLK_OUT_CNT        /**< Count of Clock Outputs               */
}   rcc_ClkOut_Id_t;


/**
 * \brief Clock Output's source configuration enumeration.
 */
typedef enum
{
    RCC_CLK_SOURCE_NONE   = 0u, /**< No clock source selected */

    RCC_CLK_SOURCE_MCO1_SYSCLK , /**< System clock (SYSCLK) will be used as Master Clock Output (MCO) clock source                */
    RCC_CLK_SOURCE_MCO1_MSIS   , /**< Multi-Speed Internal oscillator (MSIS) will be used as Master Clock Output (MCO) clock source */
    RCC_CLK_SOURCE_MCO1_HSI    , /**< 16MHz High Speed Internal (HSI16) will be used as Master Clock Output (MCO) clock source    */
    RCC_CLK_SOURCE_MCO1_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output (MCO) clock source            */
    RCC_CLK_SOURCE_MCO1_PLL1R  , /**< Phase Locked Loop 1 output R (PLL1R) will be used as Master Clock Output (MCO) clock source */
    RCC_CLK_SOURCE_MCO1_LSI    , /**< Low Speed Internal (LSI) will be used as Master Clock Output (MCO) clock source             */
    RCC_CLK_SOURCE_MCO1_LSE    , /**< Low Speed External (LSE) will be used as Master Clock Output (MCO) clock source             */
    RCC_CLK_SOURCE_MCO1_HSI48  , /**< 48MHz High Speed Internal (HSI48) will be used as Master Clock Output (MCO) clock source    */
    RCC_CLK_SOURCE_MCO1_MSIK   , /**< Multi-Speed Internal oscillator (MSIK) will be used as Master Clock Output (MCO) clock source */

    RCC_CLK_SOURCE_LSCO_LSI    , /**< Low Speed Internal (LSI) will be used as Low Speed Clock Output (LSCO) clock source */
    RCC_CLK_SOURCE_LSCO_LSE    , /**< Low Speed External (LSE) will be used as Low Speed Clock Output (LSCO) clock source */
    RCC_CLK_SOURCE_CNT           /**< Count of clock output sources                                                       */
}   rcc_ClkOut_Source_t;


/**
 * \brief Master Clock Output (MCO) divider value type.
 *
 * Output clock divider value for Clock Output's.
 * Values: 1, 2, 4, 8, 16 (MCO). LSCO has no divider - only value 1 is valid.
 */
typedef uint32_t rcc_ClkOut_Div_t;

/*-------------------------- Clock buses configuration -----------------------*/

/** \brief List of all available clock buses */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u, /**< Advanced High-performance Bus 1           */
    RCC_CLK_BUS_AHB2_1,    /**< Advanced High-performance Bus 2 group 1   */
    RCC_CLK_BUS_AHB2_2,    /**< Advanced High-performance Bus 2 group 2   */
    RCC_CLK_BUS_AHB3,      /**< Advanced High-performance Bus 3           */
    RCC_CLK_BUS_APB1_1,    /**< Advanced Peripheral Bus 1 group 1         */
    RCC_CLK_BUS_APB1_2,    /**< Advanced Peripheral Bus 1 group 2         */
    RCC_CLK_BUS_APB2,      /**< Advanced Peripheral Bus 2                 */
    RCC_CLK_BUS_APB3,      /**< Advanced Peripheral Bus 3                 */
    RCC_CLK_BUS_CNT        /**< Count of available clock buses            */
}   rcc_ClkBusId_t;


/** \brief Clock bus divider value type definition.
 * Used for AHB, APB1, APB2 and APB3 clock bus dividers (all available clock
 * busses in \ref rcc_ClkBusId_t ) */
typedef uint32_t rcc_ClkBusDiv_t;


/** \brief System clock source multiplexer configuration list
 * \note Values are equal to the RCC_CFGR1 SW field values. */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_MSIS = 0u, /**< MSIS will be used as system clock source           */
    RCC_SYSTEM_CLOCK_SOURCE_HSI      , /**< HSI16 will be used as system clock source          */
    RCC_SYSTEM_CLOCK_SOURCE_HSE      , /**< HSE will be used as system clock source            */
    RCC_SYSTEM_CLOCK_SOURCE_PLL      , /**< PLL1 output R will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_CNT        /**< Count of available system clock sources            */
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


/** \brief Type representing numerical value of APB3 divider.
 * The value of APB3 is divided from HCLK */
typedef enum
{
    RCC_APB3_DIVIDER_1  = LL_RCC_APB3_DIV_1 ,
    RCC_APB3_DIVIDER_2  = LL_RCC_APB3_DIV_2 ,
    RCC_APB3_DIVIDER_4  = LL_RCC_APB3_DIV_4 ,
    RCC_APB3_DIVIDER_8  = LL_RCC_APB3_DIV_8 ,
    RCC_APB3_DIVIDER_16 = LL_RCC_APB3_DIV_16,
}   rcc_APB3_Div_t;

/*------------------------ Flash and power configuration ---------------------*/

/**
 * \brief Defines number of wait states for Flash memory access.
 *
 * Number of wait states is calculated automatically from expected processor
 * clock (HCLK) and voltage scaling (\ref Rcc_Set_FlashLatency):
 *
 * \cond INTERNAL
 *  ==================================================================
 * | Wait states |  Range 1     |  Range 2     |  Range 3   |  Range 4 |
 * |=============|==============|==============|============|==========|
 * |    0 WS     | <=  32 MHz   | <=  25 MHz   | <= 12.5 MHz| <=  8 MHz|
 * |    1 WS     | <=  64 MHz   | <=  50 MHz   | <= 25 MHz  | <= 16 MHz|
 * |    2 WS     | <=  96 MHz   | <=  75 MHz   | <= 37.5 MHz| <= 24 MHz|
 * |    3 WS     | <= 128 MHz   | <= 100 MHz   | <= 50 MHz  | <= 25 MHz|
 * |    4 WS     | <= 160 MHz   | <= 110 MHz   | <= 55 MHz  |    -     |
 *  ==================================================================
 * \endcond
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
 * \brief PWR Voltage scaling configuration.
 *
 * Maximal system clock frequency: Range 1 - 160 MHz, Range 2 - 110 MHz,
 * Range 3 - 55 MHz, Range 4 - 25 MHz.
 *
 * \note Embedded power distribution booster (EPOD) is activated automatically
 *       in range 1 and range 2 (booster clock is derived from PLL1 input clock).
 */
typedef enum
{
    RCC_PWR_VOLTAGE_SCALE_1 = LL_PWR_REGU_VOLTAGE_SCALE1, /**< Range 1 - core voltage 1.2 V (high performance) */
    RCC_PWR_VOLTAGE_SCALE_2 = LL_PWR_REGU_VOLTAGE_SCALE2, /**< Range 2 - core voltage 1.1 V                    */
    RCC_PWR_VOLTAGE_SCALE_3 = LL_PWR_REGU_VOLTAGE_SCALE3, /**< Range 3 - core voltage 1.0 V                    */
    RCC_PWR_VOLTAGE_SCALE_4 = LL_PWR_REGU_VOLTAGE_SCALE4, /**< Range 4 - core voltage 0.9 V (low power)        */
}   rcc_PwrVoltageScale_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - input divider (1 - 16). PLL input frequency must be in range 4 - 16 MHz */
    rcc_PllMDivider_t       M_Divider;

    /** N multiplier (4 - 512). VCO frequency must be in range 128 - 544 MHz */
    rcc_PllNMult_t          N_Multiplier;

    /** Output P prescaler - divider (1 - 128). Value 0 keeps the output disabled. */
    rcc_PllPDivider_t       P_Divider;

    /** Output Q prescaler - divider (1 - 128). Value 0 keeps the output disabled. */
    rcc_PllQDivider_t       Q_Divider;

    /** Output R prescaler - divider (1 - 128). PLL1 output R is the system
     *  clock source. Value 0 keeps the output disabled. */
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

    /** Specified frequency of the HSE oscillator (4 - 50 MHz). Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Configuration of PLL1, PLL2 and PLL3 */
    rcc_PllConfigStruct_t   Pll_Config[ RCC_PLL_CNT ];

    /**
    * \brief Enables or disables Clock Security System (CSS).
    *
    * If the CSS is enabled and a failure of HSE is detected, the HSE is
    * switched off, system clock is switched to HSI16 and NMI is generated.
    *
    * \warning When using the CSS, NMI handler has to handle the HSE failure
    *          (clear CSSF flag). Otherwise the NMI is generated repeatedly.
    * \note CSS is activated only if HSE is used (HSE_ClockType is not NONE).
    *       Once enabled, the CSS can't be turned off by software.
    */
    rcc_FunctionState_t     CSS_Enable;

    /** AHB prescaler - divider */
    rcc_AHB_Div_t           AHB_Divider;
    /** APB1 prescaler - divider */
    rcc_APB1_Div_t          APB1_Divider;
    /** APB2 prescaler - divider */
    rcc_APB2_Div_t          APB2_Divider;
    /** APB3 prescaler - divider */
    rcc_APB3_Div_t          APB3_Divider;

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
