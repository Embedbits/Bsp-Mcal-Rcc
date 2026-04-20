/*
 *    Mr.Nobody, COPYRIGHT (c) 2021
 *    ALL RIGHTS RESERVED
 *
 */

/**
 * \file Rcc_Types.h
 * \ingroup Rcc
 * \brief Rcc module global types definition
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
 * \note Some peripherals has common clock source. If user tries to change clock
 *       source for this, error will be returned. User has to reset peripheral
 *       clock source and then configure it again. */
typedef enum rcc_PeriphId_t
{
    /*------------------------------ System core -----------------------------*/
    RCC_PERIPH_SYSCFG = 0u        , /**< SYSCFG Clock Enable */

    RCC_PERIPH_SYSTICK_HCLK_DIV8  , /**< System Tick timer active with HCLK divided by 8 */
    RCC_PERIPH_SYSTICK_LSI        , /**< System Tick timer active with Low Speed Internal (LSI) Oscillator as clock source */
    RCC_PERIPH_SYSTICK_LSE        , /**< System Tick timer active with Low Speed External (LSE) Oscillator as clock source */

    RCC_PERIPH_PWR                , /**< PWR Clock Enable */

    RCC_PERIPH_RTC_HSE_DIV32      , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source  */
    RCC_PERIPH_RTC_LSE            , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_PERIPH_RTC_LSI            , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

    RCC_PERIPH_CRS                , /**< CRS Clock Enable */

#if defined(DCACHE1)
    RCC_PERIPH_DCACHE1            , /**< DCACHE1 Clock Enable */
#endif /* DCACHE1 */
#if defined(DCACHE2)
    RCC_PERIPH_DCACHE2            , /**< DCACHE2 Clock Enable */
#endif /* DCACHE2 */

    RCC_PERIPH_RAMCFG             , /**< RAMCFG Clock Enable */

#if defined(RAMCFG_BKPRAM)
    RCC_PERIPH_BKPSRAM            , /**< BKPSRAM Clock Enable */
#endif /* RAMCFG_BKPRAM */

#if defined(RAMCFG_SRAM1)
    RCC_PERIPH_SRAM1              , /**< SRAM1 Clock Enable */
#endif /* RAMCFG_SRAM1 */
#if defined(RAMCFG_SRAM2)
    RCC_PERIPH_SRAM2              , /**< SRAM2 Clock Enable */
#endif /* RAMCFG_SRAM2 */
#if defined(RAMCFG_SRAM3)
    RCC_PERIPH_SRAM3              , /**< SRAM3 Clock Enable */
#endif /* RAMCFG_SRAM3 */
#if defined(RAMCFG_SRAM4)
    RCC_PERIPH_SRAM4              , /**< SRAM4 Clock Enable */
#endif /* RAMCFG_SRAM4 */
#if defined(RAMCFG_SRAM5)
    RCC_PERIPH_SRAM5              , /**< SRAM5 Clock Enable */
#endif /* RAMCFG_SRAM5 */
#if defined(RAMCFG_SRAM6)
    RCC_PERIPH_SRAM6              , /**< SRAM6 Clock Enable */
#endif /* RAMCFG_SRAM6 */

    RCC_PERIPH_FLASH              , /**< FLASH Clock Enable */

#if defined(GPDMA1)
    RCC_PERIPH_GPDMA1             , /**< GPDMA1 Clock Enable */
#endif /* GPDMA1 */

#if defined(LPDMA1)
    RCC_PERIPH_LPDMA1             , /**< LPDMA1 Clock Enable */
#endif /* LPDMA1 */

#if defined(WWDG)
    RCC_PERIPH_WWDG               , /**< WWDG Clock Enable */
#endif /* WWDG */

    RCC_PERIPH_ICLK_PLL2Q         , /**< Intermediate Clock (ICLK) from PLL 2 Q output Clock Enable */
    RCC_PERIPH_ICLK_PLL1Q         , /**< Intermediate Clock (ICLK) from PLL 1 Q output Clock Enable */
    RCC_PERIPH_ICLK_MSIK          , /**< Intermediate Clock (ICLK) from Multi-Speed Internal Oscillator output K (MSIK) Clock Enable */
    RCC_PERIPH_ICLK_HSI48         , /**< Intermediate Clock (ICLK) from 48MHz High Speed Internal (HSI48) oscillator Clock Enable */

#if defined(LPGPIO1)
    RCC_PERIPH_LPGPIO1            , /**< Low Power GPIO 1 (LPGPIO) Clock Enable */
#endif /* LPGPIO1 */

#if defined(GPIOA)
    RCC_PERIPH_GPIOA              , /**< IO port A Clock Enable */
#endif /* GPIOA */
#if defined(GPIOB)
    RCC_PERIPH_GPIOB              , /**< IO port B Clock Enable */
#endif /* GPIOB */
#if defined(GPIOC)
    RCC_PERIPH_GPIOC              , /**< IO port C Clock Enable */
#endif /* GPIOC */
#if defined(GPIOD)
    RCC_PERIPH_GPIOD              , /**< IO port D Clock Enable */
#endif /* GPIOD */
#if defined(GPIOE)
    RCC_PERIPH_GPIOE              , /**< IO port E Clock Enable */
#endif /* GPIOE */
#if defined(GPIOF)
    RCC_PERIPH_GPIOF              , /**< IO port F Clock Enable */
#endif /* GPIOF */
#if defined(GPIOG)
    RCC_PERIPH_GPIOG              , /**< IO port G Clock Enable */
#endif /* GPIOG */
#if defined(GPIOH)
    RCC_PERIPH_GPIOH              , /**< IO port H Clock Enable */
#endif /* GPIOG */
#if defined(GPIOI)
    RCC_PERIPH_GPIOI              , /**< IO port I Clock Enable */
#endif /* GPIOG */
#if defined(GPIOJ)
    RCC_PERIPH_GPIOJ              , /**< IO port J Clock Enable */
#endif /* GPIOG */

    /*-------------------------------- Timers --------------------------------*/

#if defined(GFXTIM)
    RCC_PERIPH_GFXTIM             , /**< GFXTIM Clock Enable */
#endif /* GFXTIM */

#if defined(TIM1)
    RCC_PERIPH_TIM1               , /**< TIM1 Clock Enable */
#endif /* TIM1 */
#if defined(TIM2)
    RCC_PERIPH_TIM2               , /**< TIM2 Clock Enable */
#endif /* TIM2 */
#if defined(TIM3)
    RCC_PERIPH_TIM3               , /**< TIM3 Clock Enable */
#endif /* TIM3 */
#if defined(TIM4)
    RCC_PERIPH_TIM4               , /**< TIM4 Clock Enable */
#endif /* TIM4 */
#if defined(TIM5)
    RCC_PERIPH_TIM5               , /**< TIM5 Clock Enable */
#endif /* TIM5 */
#if defined(TIM6)
    RCC_PERIPH_TIM6               , /**< TIM6 Clock Enable */
#endif /* TIM6 */
#if defined(TIM7)
    RCC_PERIPH_TIM7               , /**< TIM7 Clock Enable */
#endif /* TIM7 */
#if defined(TIM8)
    RCC_PERIPH_TIM8               , /**< TIM8 Clock Enable */
#endif /* TIM8 */
#if defined(TIM15)
    RCC_PERIPH_TIM15              , /**< TIM15 Clock Enable */
#endif /* TIM15 */
#if defined(TIM16)
    RCC_PERIPH_TIM16              , /**< TIM16 Clock Enable */
#endif /* TIM16 */
#if defined(TIM17)
    RCC_PERIPH_TIM17              , /**< TIM17 Clock Enable */
#endif /* TIM17 */


#if defined(LPTIM1)
    RCC_PERIPH_LPTIM1_MSIK        , /**< Low Power Timer 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for LPTIM 1. */
    RCC_PERIPH_LPTIM1_LSI         , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) as clock source for LPTIM 1. */
    RCC_PERIPH_LPTIM1_HSI16       , /**< Low Power Timer 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 1. */
    RCC_PERIPH_LPTIM1_LSE         , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 1. */
#endif /* LPTIM1 */
#if defined(LPTIM2)
    RCC_PERIPH_LPTIM2_PCLK1       , /**< Low Power Timer 2 clock enable with APB1 (PCLK1) as clock source for LPTIM 2. */
    RCC_PERIPH_LPTIM2_LSI         , /**< Low Power Timer 2 clock enable with Low Speed Internal (LSI) as clock source for LPTIM 2. */
    RCC_PERIPH_LPTIM2_HSI16       , /**< Low Power Timer 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 2. */
    RCC_PERIPH_LPTIM2_LSE         , /**< Low Power Timer 2 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 2. */
#endif /* LPTIM2 */
#if defined(LPTIM3)
    RCC_PERIPH_LPTIM3_MSIK        , /**< Low Power Timer 3 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for LPTIM 3&4. */
    RCC_PERIPH_LPTIM3_LSI         , /**< Low Power Timer 3 clock enable with Low Speed External (LSE) as clock source for LPTIM 3&4. */
    RCC_PERIPH_LPTIM3_HSI16       , /**< Low Power Timer 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 3&4. */
    RCC_PERIPH_LPTIM3_LSE         , /**< Low Power Timer 3 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 3&4. */
#endif /* LPTIM3 */
#if defined(LPTIM4)
    RCC_PERIPH_LPTIM4_MSIK        , /**< Low Power Timer 4 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for LPTIM 3&4. */
    RCC_PERIPH_LPTIM4_LSI         , /**< Low Power Timer 4 clock enable with Low Speed External (LSE) as clock source for LPTIM 3&4. */
    RCC_PERIPH_LPTIM4_HSI16       , /**< Low Power Timer 4 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 3&4. */
    RCC_PERIPH_LPTIM4_LSE         , /**< Low Power Timer 4 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 3&4. */
#endif /* LPTIM4 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
    RCC_PERIPH_SPI1_PCLK2         , /**< SPI 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_SPI1_SYSCLK        , /**< SPI 1 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_SPI1_HSI           , /**< SPI 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_SPI1_MSIK          , /**< SPI 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* SPI1 */
#if defined(SPI2)
    RCC_PERIPH_SPI2_PCLK1         , /**< SPI 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_SPI2_SYSCLK        , /**< SPI 2 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_SPI2_HSI           , /**< SPI 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_SPI2_MSIK          , /**< SPI 2 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* SPI2 */
#if defined(SPI3)
    RCC_PERIPH_SPI3_PCLK3         , /**< SPI 3 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_SPI3_SYSCLK        , /**< SPI 3 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_SPI3_HSI           , /**< SPI 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_SPI3_MSIK          , /**< SPI 3 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* SPI3 */

#if defined(I2C1)
    RCC_PERIPH_I2C1_PCLK1         , /**< I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C1_SYSCLK        , /**< I2C 1 clock enable with SysClk (AHB) as clock source*/
    RCC_PERIPH_I2C1_HSI           , /**< I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C1_MSIK          , /**< I2C 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_PERIPH_I2C2_PCLK1         , /**< I2C 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C2_SYSCLK        , /**< I2C 2 clock enable with SysClk (AHB) as clock source*/
    RCC_PERIPH_I2C2_HSI           , /**< I2C 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C2_MSIK          , /**< I2C 2 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_PERIPH_I2C3_PCLK3         , /**< I2C 3 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_I2C3_SYSCLK        , /**< I2C 3 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_I2C3_HSI           , /**< I2C 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C3_MSIK          , /**, I2C 3 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_PERIPH_I2C4_PCLK1         , /**< I2C 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C4_SYSCLK        , /**< I2C 4 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_I2C4_HSI           , /**< I2C 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C4_MSIK          , /**, I2C 4 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C4 */
#if defined(I2C5)
    RCC_PERIPH_I2C5_PCLK1         , /**< I2C 5 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C5_SYSCLK        , /**< I2C 5 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_I2C5_HSI           , /**< I2C 5 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C5_MSIK          , /**< I2C 5 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C5 */
#if defined(I2C6)
    RCC_PERIPH_I2C6_PCLK1         , /**< I2C 6 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C6_SYSCLK        , /**< I2C 6 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_I2C6_HSI           , /**< I2C 6 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C6_MSIK          , /**< I2C 6 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C6 */

#if defined(USART1)
    RCC_PERIPH_USART1_APB2        , /**< USART 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART1_SYSCLK      , /**< USART 1 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_USART1_HSI         , /**< USART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART1_LSE         , /**< USART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART1 */
#if defined(USART2)
    RCC_PERIPH_USART2_APB1        , /**< USART 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART2_SYSCLK      , /**< USART 2 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_USART2_HSI         , /**< USART 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART2_LSE         , /**< USART 2 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART2 */
#if defined(USART3)
    RCC_PERIPH_USART3_APB1        , /**< USART 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART3_SYSCLK      , /**< USART 3 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_USART3_HSI         , /**< USART 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART3_LSE         , /**< USART 3 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART3 */
#if defined(USART6)
    RCC_PERIPH_USART6_APB1        , /**< USART 6 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART6_SYSCLK      , /**< USART 6 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_USART6_HSI         , /**< USART 6 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART6_LSE         , /**< USART 6 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART6 */

#if defined(UART4)
    RCC_PERIPH_UART4_PCLK1        , /**< UART 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART4_SYSCLK      , /**< UART 4 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_UART4_HSI          , /**< UART 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART4_LSE          , /**< UART 4 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* UART4 */
#if defined(UART5)
    RCC_PERIPH_UART5_PCLK1        , /**< UART 5 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART5_SYSCLK      , /**< UART 5 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_UART5_HSI16        , /**< UART 5 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART5_LSE          , /**< UART 5 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART5 */

#if defined(LPUART1)
    RCC_PERIPH_LPUART1_PCLK3      , /**< Low-Power UART 1 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_LPUART1_SYSCLK     , /**< Low-Power UART 1 clock enable with SysClk (AHB) as clock source */
    RCC_PERIPH_LPUART1_HSI16      , /**< Low-Power UART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_LPUART1_LSE        , /**< Low-Power UART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_LPUART1_MSIK       , /**< Low-Power UART 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* LPUART1 */

#if defined(FDCAN1)

    RCC_PERIPH_FDCAN_PLL1Q        , /**< FDCAN clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source */
    RCC_PERIPH_FDCAN_PLL2P        , /**< FDCAN clock enable with Phase Locked Loop 2 output P (PLL2P) used as clock source */
    RCC_PERIPH_FDCAN_HSE          , /**< FDCAN clock enable with High Speed External oscillator (HSE) used as clock source */
#endif /* FDCAN1 */

#if defined(SDMMC1)
    RCC_PERIPH_SDMMC1_ICLK        , /**< SDMMC1 clock enable with Intermediate Clock (ICLK) bus used as clock source for SDMMC 1&2 */
    RCC_PERIPH_SDMMC1_PLL1P       , /**< SDMMC1 clock enable with Phase Locked Loop 1 output P (PLL1P) used as clock source for SDMMC 1&2 */
#endif /* SDMMC1 */
#if defined(SDMMC2)
    RCC_PERIPH_SDMMC2_ICLK        , /**< SDMMC2 clock enable with Intermediate Clock (ICLK) bus used as clock source for SDMMC 1&2 */
    RCC_PERIPH_SDMMC2_PLL1P       , /**< SDMMC2 clock enable with Phase Locked Loop 1 output P (PLL1P) used as clock source for SDMMC 1&2 */
#endif /* SDMMC2 */

#if defined(FMC_Bank1_R)
    RCC_PERIPH_FMC                , /**< Flexible Memory Controller (FMC) Clock Enable */
#endif /* FSMC */

#if defined(OCTOSPIM)
    RCC_PERIPH_OCTOSPIM           , /**< Octal SPI Manager (OCTOSPIM) Clock Enable */
#endif /* OCTOSPIM */
#if defined(OCTOSPI1)
    RCC_PERIPH_OCTOSPI1_SYSCLK    , /**< Octal SPI 1 (OCTOSPI) clock enable with system clock as clock source for QSPI 1&2 */
    RCC_PERIPH_OCTOSPI1_MSIK      , /**< Octal SPI 1 (OCTOSPI) clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for QSPI 1&2 */
    RCC_PERIPH_OCTOSPI1_PLL1Q     , /**< Octal SPI 1 (OCTOSPI) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source for QSPI 1&2 */
    RCC_PERIPH_OCTOSPI1_PLL2Q     , /**< Octal SPI 1 (OCTOSPI) clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source for QSPI 1&2 */
#endif /* OCTOSPI1 */
#if defined(OCTOSPI2)
    RCC_PERIPH_OCTOSPI2_SYSCLK    , /**< Octal SPI 2 (OCTOSPI) clock enable with system clock as clock source for QSPI 1&2 */
    RCC_PERIPH_OCTOSPI2_MSIK      , /**< Octal SPI 2 (OCTOSPI) clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for QSPI 1&2 */
    RCC_PERIPH_OCTOSPI2_PLL1Q     , /**< Octal SPI 2 (OCTOSPI) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source for QSPI 1&2 */
    RCC_PERIPH_OCTOSPI2_PLL2Q     , /**< Octal SPI 2 (OCTOSPI) clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source for QSPI 1&2 */
#endif /* OCTOSPI2 */

#if defined(HSPI1)
    RCC_PERIPH_HSPI1_SYSCLK       , /**< Hexa-deca SPI 1 (HSPI) clock enable with System Clock Enable as clock source */
    RCC_PERIPH_HSPI1_PLL1Q        , /**< Hexa-deca SPI 1 (HSPI) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_PERIPH_HSPI1_PLL2Q        , /**< Hexa-deca SPI 1 (HSPI) clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_HSPI1_PLL3R        , /**< Hexa-deca SPI 1 (HSPI) clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
#endif /* HSPI1 */

#if defined(USB_OTG_HS)
    RCC_PERIPH_USBPHY_HSE         , /**< USB High Speed (480Mbit) PHY clock enable with High Speed External (HSE) as clock source */
    RCC_PERIPH_USBPHY_HSE_DIV2    , /**< USB High Speed (480Mbit) PHY clock enable with High Speed External (HSE) divided by 2 as clock source */
    RCC_PERIPH_USBPHY_PLL1P       , /**< USB High Speed (480Mbit) PHY clock enable with Phase Locked Loop 1 output P (PLL1P) as clock source */
    RCC_PERIPH_USBPHY_PLL1P_DIV2  , /**< USB High Speed (480Mbit) PHY clock enable with Phase Locked Loop 1 output P (PLL1P) divided by 2 as clock source */

    RCC_PERIPH_USB                , /**< USB High Speed Clock Enable */
#endif /* USB_OTG_HS */

#if defined(UCPD1)
    RCC_PERIPH_UCPD1              , /**< USB Type-C power delivery 1 (UCPD) Clock Enable */
#endif /* UCPD1 */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(PSSI) || defined(DCMI)
    RCC_PERIPH_DCMI_PSSI          , /**< Digital Camera Media Interface (DCMI) and Parallel Synchronous Slave Interface (PSSI) Clock Enable */
#endif /* PSSI OR DCMI */

#if defined(DSI)
    RCC_PERIPH_DSIHOST            , /**< Display Serial Interface (DSI) host controller Clock Enable \note This functionality is configured separately. */
#endif /* DSI */

#if defined(GFXMMU)
    RCC_PERIPH_GFXMMU             , /**< Graphic Memory Management Unit (GFXMMU) Clock Enable */
#endif /* GFXMMU */

#if defined(JPEG)
    RCC_PERIPH_JPEG               , /**< JPEG Clock Enable */
#endif /* JPEG */

#if defined(LTDC)
    RCC_PERIPH_LTDC_PLL2R         , /**< LCD-TFT Display COntroller (LTDC) Clock Enable with PLL2 output R as clock source */
    RCC_PERIPH_LTDC_PLL3R         , /**< LCD-TFT Display COntroller (LTDC) Clock Enable with PLL3 output R as clock source */
#endif /* LTDC */

#if defined(GPU2D)
    RCC_PERIPH_GPU2D              , /**< Neo-Chrom graphic co-processor (GPU2D) Clock Enable */
#endif /* GPU2D */

#if defined(DMA2D)
    RCC_PERIPH_DMA2D              , /**< Chrom-Art Accelerator (DMA2D) Clock Enable */
#endif /* DMA2D */

#if defined(SAI1)
    RCC_PERIPH_SAI1_PLL2P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_PERIPH_SAI1_PLL3P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_PERIPH_SAI1_PLL1P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 P output as clock source */
    RCC_PERIPH_SAI1_CKIN          , /**< Serial Audio Interface 1 (SAI1) clock enable with clock input through pin as clock source */
    RCC_PERIPH_SAI1_HSI16         , /**< Serial Audio Interface 1 (SAI1) clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source*/
#endif /* SAI1 */
#if defined(SAI2)
    RCC_PERIPH_SAI2_PLL2P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_PERIPH_SAI2_PLL3P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_PERIPH_SAI2_PLL1P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 P output as clock source */
    RCC_PERIPH_SAI2_CKIN          , /**< Serial Audio Interface 2 (SAI2) clock enable with clock input through pin as clock source */
    RCC_PERIPH_SAI2_HSI16         , /**< Serial Audio Interface 2 (SAI2) clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source*/
#endif /* SAI2 */

    /*-------------------------------- Analog --------------------------------*/

#if defined(OPAMP)
    RCC_PERIPH_OPAMP              , /**< OPAMP Clock Enable */
#endif /* OPAMP */

#if defined(COMP1)
    RCC_PERIPH_COMP               , /**< COMP Clock Enable */
#endif /* COMP1 */

#if defined(VREFBUF)
    RCC_PERIPH_VREF               , /**< VREFBUF Clock Enable */
#endif /* VREFBUF */

#if defined(TSC)
    RCC_PERIPH_TSC                , /**< Touch Sensing Controller Clock Enable */
#endif /* TSC1 */

#if defined(ADC12_COMMON)
    RCC_PERIPH_ADC12_HCLK         , /**< ADC 1&2 clock enable with HCLK as clock source for ADC 1&2&4 and DAC. */
    RCC_PERIPH_ADC12_SYSCLK       , /**< ADC 1&2 clock enable with SysClk as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC12_PLL2R        , /**< ADC 1&2 clock enable with PLL2 output R as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC12_HSE          , /**< ADC 1&2 clock enable with High Speed External oscillator (HSE) as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC12_HSI16        , /**< ADC 1&2 clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC12_MSIK         , /**< ADC 1&2 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for ADC 1&2&4 and DAC */
#endif /* ADC1 */

#if defined(ADC4)
    RCC_PERIPH_ADC4_HCLK          , /**< ADC 4 clock enable with HCLK as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC4_SYSCLK        , /**< ADC 4 clock enable with SysClk as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC4_PLL2R         , /**< ADC 4 clock enable with PLL2 output R as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC4_HSE           , /**< ADC 4 clock enable with High Speed External oscillator (HSE) as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC4_HSI16         , /**< ADC 4 clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_ADC4_MSIK          , /**< ADC 4 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for ADC 1&2&4 and DAC */
#endif /* ADC4 */

#if defined(DAC1)
    RCC_PERIPH_DAC_HCLK           , /**< DAC clock enable with HCLK as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_DAC_SYSCLK         , /**< DAC clock enable with SysClk as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_DAC_PLL2R          , /**< DAC clock enable with PLL2 output R as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_DAC_HSE            , /**< DAC clock enable with High Speed External oscillator (HSE) as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_DAC_HSI16          , /**< DAC clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source for ADC 1&2&4 and DAC */
    RCC_PERIPH_DAC_MSIK           , /**< DAC clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for ADC 1&2&4 and DAC */
#endif /* DAC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(AES)
    RCC_PERIPH_AES                , /**< Advanced Encryption Standard (AES) HW accelerator Clock Enable */
#endif /* AES */

#if defined(GTZC_TZSC1)
    RCC_PERIPH_GTZC1              , /**< Global Trust Zone Controller 1 (GTZC) Clock Enable */
#endif /* GTZC1 */
#if defined(GTZC_TZSC2)
    RCC_PERIPH_GTZC2              , /**< Global Trust Zone Controller 2 (GTZC) Clock Enable */
#endif /* GTZC2 */

#if defined(HASH)
    RCC_PERIPH_HASH               , /**< HASH Clock Enable */
#endif /* HASH */

#if defined(OTFDEC1)
    RCC_PERIPH_OTFDEC1            , /**< On-the-fly Decryption Engine 1 (OTFDEC) Clock Enable */
#endif /* OTFDEC1 */
#if defined(OTFDEC2)
    RCC_PERIPH_OTFDEC2            , /**< On-the-fly Decryption Engine 2 (OTFDEC) Clock Enable */
#endif /* OTFDEC2 */

#if defined(PKA)
    RCC_PERIPH_PKA                , /**< Public Key Accelerator (PKA) Clock Enable */
#endif /* PKA */

#if defined(RNG)
    RCC_PERIPH_RNG_HSI48          , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
    RCC_PERIPH_RNG_HSI48_DIV2     , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) divided by 2 used as clock source */
    RCC_PERIPH_RNG_HSI16          , /**< Random Number Generator (RNG) clock enable with 16MHz High Speed Internal oscillator (HSI16) used as clock source */
#endif /* RNG */

#if defined(SAES)
    RCC_PERIPH_SAES_SHSI          , /**< Secure Advanced Encryption Standard (SAES) HW accelerator clock enable with Secure High Speed Internal (SHSI) oscillator used as clock source */
    RCC_PERIPH_SAES_SHSI_DIV2     , /**< Secure Advanced Encryption Standard (SAES) HW accelerator clock enable with Secure High Speed Internal (SHSI) oscillator divided by 2 used as clock source */
#endif /* SAES */

    /*------------------------------- Computing ------------------------------*/

#if defined(ADF1)
    RCC_PERIPH_ADF1_HCLK          , /**< Audio Digital Filter 1 (ADF) Clock Enable with HCLK as clock source */
    RCC_PERIPH_ADF1_PLL1P         , /**< Audio Digital Filter 1 (ADF) Clock Enable with PLL1 output P as clock source */
    RCC_PERIPH_ADF1_PLL3Q         , /**< Audio Digital Filter 1 (ADF) Clock Enable with PLL3 output Q as clock source */
    RCC_PERIPH_ADF1_CKIN          , /**< Audio Digital Filter 1 (ADF) Clock Enable with clock input through pin as clock source */
    RCC_PERIPH_ADF1_MSIK          , /**< Audio Digital Filter 1 (ADF) Clock Enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* ADF1 */

#if defined(CORDIC)
    RCC_PERIPH_CORDIC             , /**< CORDIC Clock Enable */
#endif /* CORDIC */

#if defined(CRC)
    RCC_PERIPH_CRC                , /**< CRC Clock Enable */
#endif /* CRC */

#if defined(FMAC)
    RCC_PERIPH_FMAC               , /**< Filter Math (FMAC) accelerator Clock Enable */
#endif /* FMAC */

#if defined(MDF1)
    RCC_PERIPH_MDF1_HCLK          , /**< Multi-function DIgital Filter 1 (MDF) clock enable with HCLK as clock source */
    RCC_PERIPH_MDF1_PLL1P         , /**< Multi-function DIgital Filter 1 (MDF) clock enable with PLL1 output P as clock source */
    RCC_PERIPH_MDF1_PLL3Q         , /**< Multi-function DIgital Filter 1 (MDF) clock enable with PLL3 output Q as clock source */
    RCC_PERIPH_MDF1_CKIN          , /**< Multi-function DIgital Filter 1 (MDF) clock enable with clock input through pin as clock source */
    RCC_PERIPH_MDF1_MSIK          , /**< Multi-function DIgital Filter 1 (MDF) clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* MDF1 */

    RCC_PERIPH_LIST_CNT
}   rcc_PeriphId_t;

/*------------------------ Clock sources configuration -----------------------*/

/** \brief Enumeration of High Speed External (HSE) input configuration */
typedef enum
{
    RCC_HSE_TYPE_NONE     = 0u , /**< No external clock connected to HSE pin */
    RCC_HSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_HSE_TYPE_SIG_ANALOG_IN , /**< External low voltage swing signal      */
    RCC_HSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_HseType_t;


/** \ brief Enumeration of Low Speed External (LSE) input configuration */
typedef enum
{
    RCC_LSE_TYPE_NONE     = 0u , /**< No external clock connected to LSE pin */
    RCC_LSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_LSE_TYPE_SIG_ANALOG_IN , /**< External low voltage swing signal      */
    RCC_LSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_LseType_t;


typedef enum
{
    RCC_MSI_CLK_48M = 0u, /**< 48MHz output from Multi-Speed Internal (MSI) Low Power oscillator    */
    RCC_MSI_CLK_24M,      /**< 24MHz output from Multi-Speed Internal (MSI) Low Power oscillator    */
    RCC_MSI_CLK_16M,      /**< 16MHz output from Multi-Speed Internal (MSI) Low Power oscillator    */
    RCC_MSI_CLK_12M,      /**< 12MHz output from Multi-Speed Internal (MSI) Low Power oscillator    */
    RCC_MSI_CLK_4M,       /**< 4MHz output from Multi-Speed Internal (MSI) Low Power oscillator     */
    RCC_MSI_CLK_3072K,    /**< 3.072MHz output from Multi-Speed Internal (MSI) Low Power oscillator */
    RCC_MSI_CLK_2000K,    /**< 2MHz output from Multi-Speed Internal (MSI) Low Power oscillator     */
    RCC_MSI_CLK_1536K,    /**< 1.536MHz output from Multi-Speed Internal (MSI) Low Power oscillator */
    RCC_MSI_CLK_1330K,    /**< 1.33MHz output from Multi-Speed Internal (MSI) Low Power oscillator  */
    RCC_MSI_CLK_1024K,    /**< 1.024MHz output from Multi-Speed Internal (MSI) Low Power oscillator */
    RCC_MSI_CLK_1000K,    /**< 1MHz output from Multi-Speed Internal (MSI) Low Power oscillator     */
    RCC_MSI_CLK_768K,     /**< 768kHz output from Multi-Speed Internal (MSI) Low Power oscillator   */
    RCC_MSI_CLK_400K,     /**< 400kHz output from Multi-Speed Internal (MSI) Low Power oscillator   */
    RCC_MSI_CLK_200K,     /**< 200kHz output from Multi-Speed Internal (MSI) Low Power oscillator   */
    RCC_MSI_CLK_133K,     /**< 133kHz output from Multi-Speed Internal (MSI) Low Power oscillator   */
    RCC_MSI_CLK_100K,     /**< 100kHz output from Multi-Speed Internal (MSI) Low Power oscillator   */
    RCC_MSI_CLK_CNT
}   rcc_MsisClkFreq_t;


typedef enum
{
    RCC_MSIS_OUT_K = 0u, /**< Multi-Speed Internal Oscillator output K (MSIK)  */
    RCC_MSIS_OUT_S,      /**< Multi-Speed Internal Oscillator output S (MSIS)  */
    RCC_MSIS_OUT_CNT     /**< Count of Multi-Speed Internal Oscillator outputs */
}   rcc_MsisOutId_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration */
typedef enum
{
    RCC_PLL_1 = 0u, /**< Phase Locked Loop 1 */
    RCC_PLL_2,      /**< Phase Locked Loop 2 */
    RCC_PLL_3,      /**< Phase Locked Loop 3 */
    RCC_PLL_CNT
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 * \note If the PLL is not used, select RCC_PLL_SRC_NONE. Otherwise will be
 *       PLL activated. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                             */
    RCC_PLL_SRC_MSIS     , /**< PLL will be clocked by MSIS oscillator      */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator       */
    RCC_PLL_SRC_HSI16    , /**< PLL will be clocked by 16MHz HSI oscillator */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                 */
}   rcc_PllClkSrc_t;


/** \brief Phase Locked Loop (PLL) M Divider value.
 * This is clock input divider for PLL.
 * Step size: 1
 * Range    : 1 - 63
 */
typedef uint32_t rcc_PllMDivider_t;


/** \brief Type used to signal values of PLL N multiplier
 * Phase Locked Loop (PLL) Feedback multiplier.
 * Step size: 1
 * Range    : 4 - 512
 */
typedef uint32_t rcc_PllNMultiplier_t;


/** \brief Phase Locked Loop (PLL) P output divider value.
 * Step size: 1
 * Range    : 1 - 128
 * \warning Because of ST's incompetence, the value for PLL1 odd division factors
 * are not allowed. Bravo ST, another one for the list. The other PLL's are
 * in range 1-128 in step size of 1.
 */
typedef uint32_t rcc_PllPDivider_t;


/** \brief Phase Locked Loop (PLL) Q output divider value.
 * Step size: 1
 * Range    : 1 - 128
 */
typedef uint32_t rcc_PllQDivider_t;


/** \brief Phase Locked Loop (PLL) R output divider value.
 * Step size: 1
 * Range    : 1 - 128
 */
typedef uint32_t rcc_PllRDivider_t;

/*--------------- Real Time Clock (RTC) clock source configuration -----------*/

typedef enum
{
    RCC_RTC_CLK_SOURCE_HSE_DIV = 0u, /**< Divided High Speed External (HSE) clock will be used as RTC clock source. */
    RCC_RTC_CLK_SOURCE_LSE,          /**< Low Speed External (LSE) will be used as RTC clock source                 */
    RCC_RTC_CLK_SOURCE_LSI,          /**< Low Speed Internal (LSI) will be used as RTC clock source                 */
}   rcc_Rtc_ClkSource_t;


/**
 * \brief Divider value of HSE clock source for RTC peripheral.
 * Step size: 1
 * Range    : 1-63
 */
typedef uint16_t rcc_Rtc_HseDiv_t;

/*------------------------- Clock outputs configuration ----------------------*/

/**
 * \brief Master Clock Output (MCO) clock source configuration enumeration.
 */
typedef enum
{
    RCC_MCO_CLOCK_SOURCE_NONE     = LL_RCC_MCO1SOURCE_NOCLOCK, /**< No clock source is selected. MCO is inactive.                        */
    RCC_MCO_CLOCK_SOURCE_LSE      = LL_RCC_MCO1SOURCE_LSE,     /**< Low Speed External (LSE) will be used as clock source                */
    RCC_MCO_CLOCK_SOURCE_LSI      = LL_RCC_MCO1SOURCE_LSI,     /**< Low Speed Internal (LSI) will be used as clock source                */
    RCC_MCO_CLOCK_SOURCE_HSE      = LL_RCC_MCO1SOURCE_HSE,     /**< High Speed External (HSE) will be used as clock source               */
    RCC_MCO_CLOCK_SOURCE_HSI16    = LL_RCC_MCO1SOURCE_HSI,     /**< 16MHz High Speed Internal (HSI) will be used as clock source         */
    RCC_MCO_CLOCK_SOURCE_HSI48    = LL_RCC_MCO1SOURCE_HSI48,   /**< 48MHz High Speed Internal (HSI) will be used as clock source         */
    RCC_MCO_CLOCK_SOURCE_PLL1RCLK = LL_RCC_MCO1SOURCE_PLLCLK,  /**< Phase Locked Loop 1 output R (PLL1R) will be used as clock source    */
    RCC_MCO_CLOCK_SOURCE_SYSCLK   = LL_RCC_MCO1SOURCE_SYSCLK,  /**< System clock (SYSCLK) will be used as clock source                   */
    RCC_MCO_CLOCK_SOURCE_MSIS     = LL_RCC_MCO1SOURCE_MSIS,    /**< Multi-Speed Internal Oscillator output S (MSIS) used as clock source */
}   rcc_Mco_ClkSource_t;


/**
 * \brief Master Clock Output (MCO) divider value type.
 */
typedef enum
{
    RCC_MCO_DIVIDER_1  = LL_RCC_MCO1_DIV_1, /**< MCO output clock is divided by 1  */
    RCC_MCO_DIVIDER_2  = LL_RCC_MCO1_DIV_2, /**< MCO output clock is divided by 2  */
    RCC_MCO_DIVIDER_4  = LL_RCC_MCO1_DIV_4, /**< MCO output clock is divided by 4  */
    RCC_MCO_DIVIDER_8  = LL_RCC_MCO1_DIV_8, /**< MCO output clock is divided by 8  */
    RCC_MCO_DIVIDER_16 = LL_RCC_MCO1_DIV_16,/**< MCO output clock is divided by 16 */
}   rcc_Mco_Divider_t;


/** \brief Low Speed Clock Output clock source configuration enumeration */
typedef enum
{
    RCC_LSCO_CLOCK_SOURCE_LSI = 0, /**< Low Speed Internal (LSI) will be used as clock source */
    RCC_LSCO_CLOCK_SOURCE_LSE = 1  /**< Low Speed External (LSE) will be used as clock source */
}   rcc_Lsco_ClkSource_t;

/*-------------------------- Clock buses configuration -----------------------*/

/** \brief System clock source multiplexer configuration list */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_MSIS = LL_RCC_SYS_CLKSOURCE_MSIS, /**< MSIS will be used as system clock source */
    RCC_SYSTEM_CLOCK_SOURCE_HSI  = LL_RCC_SYS_CLKSOURCE_HSI,  /**< HSI will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_HSE  = LL_RCC_SYS_CLKSOURCE_HSE,  /**< HSE will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_PLL  = LL_RCC_SYS_CLKSOURCE_PLL1, /**< PLL will be used as system clock source  */
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
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ===============================================================================================================
 * | Number of wait states | Programming delay |   VOS3 range   |   VOS2 range   |   VOS1 range   |   VOS0 range   |
 * |        (LATENCY)      |    (WRHIGHFREQ)   | 0.95 to 1.05 V | 1.05 to 1.15 V | 1.15 to 1.26 V | 1.30 to 1.40 V |
 * |=======================|===================|================|================|================|================|
 * |          0 WS         |                   | 0 to 20 MHz    | 0 to 30 MHz    | 0 to 34 MHz    | 0 to 42 MHz    |
 * | (1 FLASH clock cycle) |                   |                |                |                |                |
 * |-----------------------|        00         |----------------|----------------|----------------|----------------|
 * |          1 WS         |                   | 20 to 40 MHz   | 30 to 60 MHz   | 34 to 68 MHz   | 42 to 84 MHz   |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          2 WS         |                   | 40 to 60 MHz   | 60 to 90 MHz   | 68 to 102 MHz  | 84 to 126 MHz  |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        01         |----------------|----------------|----------------|----------------|
 * |          3 WS         |                   | 60 to 80 MHz   | 90 to 120 MHz  | 102 to 136 MHz | 126 to 168 MHz |
 * | (3 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          4 WS         |                   | 80 to 100 MHz  | 120 to 150 MHz | 136 to 170 MHz | 168 to 210 MHz |
 * | (4 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        10         |----------------|----------------|----------------|----------------|
 * |          5 WS         |                   | N/A            | N/A            | 170 to 200 MHz | 210 to 250 MHz |
 * | (5 FLASH clock cycles)|                   |                |                |                |                |
 *  ===============================================================================================================
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
 * \note Power voltage scaling has to be chosen according to desired performance.
 *
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ===============================================================================================================
 * | Number of wait states | Programming delay |   VOS3 range   |   VOS2 range   |   VOS1 range   |   VOS0 range   |
 * |        (LATENCY)      |    (WRHIGHFREQ)   | 0.95 to 1.05 V | 1.05 to 1.15 V | 1.15 to 1.26 V | 1.30 to 1.40 V |
 * |=======================|===================|================|================|================|================|
 * |          0 WS         |                   | 0 to 20 MHz    | 0 to 30 MHz    | 0 to 34 MHz    | 0 to 42 MHz    |
 * | (1 FLASH clock cycle) |                   |                |                |                |                |
 * |-----------------------|        00         |----------------|----------------|----------------|----------------|
 * |          1 WS         |                   | 20 to 40 MHz   | 30 to 60 MHz   | 34 to 68 MHz   | 42 to 84 MHz   |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          2 WS         |                   | 40 to 60 MHz   | 60 to 90 MHz   | 68 to 102 MHz  | 84 to 126 MHz  |
 * | (2 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        01         |----------------|----------------|----------------|----------------|
 * |          3 WS         |                   | 60 to 80 MHz   | 90 to 120 MHz  | 102 to 136 MHz | 126 to 168 MHz |
 * | (3 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|-------------------|----------------|----------------|----------------|----------------|
 * |          4 WS         |                   | 80 to 100 MHz  | 120 to 150 MHz | 136 to 170 MHz | 168 to 210 MHz |
 * | (4 FLASH clock cycles)|                   |                |                |                |                |
 * |-----------------------|        10         |----------------|----------------|----------------|----------------|
 * |          5 WS         |                   | N/A            | N/A            | 170 to 200 MHz | 210 to 250 MHz |
 * | (5 FLASH clock cycles)|                   |                |                |                |                |
 *  ===============================================================================================================
 * \endcond
 */
typedef enum
{
    RCC_PWR_VOLTAGE_SCALE_0 = LL_PWR_REGU_VOLTAGE_SCALE1, /**< Core voltage 1.3V  - 1.4V  */
    RCC_PWR_VOLTAGE_SCALE_1 = LL_PWR_REGU_VOLTAGE_SCALE2, /**< Core voltage 1.15V - 1.26V */
    RCC_PWR_VOLTAGE_SCALE_2 = LL_PWR_REGU_VOLTAGE_SCALE3, /**< Core voltage 1.05V - 1.15V */
    RCC_PWR_VOLTAGE_SCALE_3 = LL_PWR_REGU_VOLTAGE_SCALE4, /**< Core voltage 0.95V - 1.05V */
}   rcc_PwrVoltageScale_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL's */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - divider, used by both: PLL and PLLSAI1. */
    rcc_PllMDivider_t       M_Divider;

    /** Main PLLN prescaler - multiplier. Must be in range from 8 to 86 */
    rcc_PllNMultiplier_t    N_Multiplier;

    /** Main PLLP prescaler - divider. Configuration function sets the prescaler
    *  during startup, but the output must be enabled later by user.
    *  If not enabled properly, the SAI peripheral switches to HSI16 automatically. */
    rcc_PllPDivider_t       P_Divider;

    /** Main PLLQ prescaler - divider. Configuration function sets the prescaler
    *  during startup, but the output must be enabled later by user. */
    rcc_PllQDivider_t       Q_Divider;

    /** Main PLLR prescaler - divider. Configuration function sets the prescaler
    *  during startup and the output is enabled at the same time, if the PLL
    *  is used as the system cock source. */
    rcc_PllRDivider_t       R_Divider;

}   rcc_PllConfigStruct_t;


/** \brief Reset and Clock Control configuration structure */
typedef struct rcc_ConfigStruct_t
{
    /** Specifies HSE clock type. Ignored if HSE is not used */
    rcc_HseType_t           HSE_ClockType;

    /** Specified frequency of the HSE oscillator. Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Specifies frequencies for MSI's oscillators */
    rcc_MsisClkFreq_t       MsisConfig[RCC_MSIS_OUT_CNT];

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Specifies clock source of the main PLL */
    rcc_PllConfigStruct_t   Pll_Config[RCC_PLL_CNT];

    /**
    * @brief Enables or disables Clock Security System (CSS).
    *
    * If the CSS is enabled and a failure of HSE is detected, NMI_ISR function will
    * be executed. The NMI_ISR function sets HSI as a system clock source and can
    * call additional user callback.
    *
    * @warning When HSE failure occurs and the CSS is disabled, MCU switches the
    *          system clock source automatically! This event cant't be detected
    *          by software.
    * @warning When using the CSS, an user defined callback should be added using
    *          RCC_SetNMI_Handler. Otherwise the HSE failure can't be detected
    *          by your code and you will not be notified.
    * @warning Once enabled, the CSS can't be turned off by software on L4 MCU
    *          family (just by reset).
    * @note If HSE is set as the clock entry of a PLL, the PLL will be disabled
    *       by the CSS in case of HSE failure.
    * @note CSS also sends an event to break inputs of advanced-control timers
    *       in case of HSE failure.
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

    /** Flash latency - number of wait states. Must be set correctly with respect
    *  to the desired clock frequency and voltage scaling.
    */
    rcc_FlashLatency_t      FlashLatency;

    /**
    * Scaling of internal voltage supply
    */
    rcc_PwrVoltageScale_t   VoltageScaling;

    /**
    * Specify clock source for MCO output.
    * Pin PA8 has to be configure as alternate function 0 to enable MCO.
    * The selected clock output must not exceed maximal frequency of given pin.
    * Divider below may be used to meet the limit.
    * Note: you can configure also LSCO output later in your code
    * using RCC_SetLSCO_ClockSource function.
    */
    rcc_Mco_ClkSource_t     MCO_ClockSource;

    /** MCO clock output divider */
    rcc_Mco_Divider_t       MCO_ClockDivider;

    /** Value of time in ms [0.001s] between SysTicks */
    rcc_Time_ms_t           SysTickInterval;

}   rcc_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* RCC_RCC_TYPES_H */
