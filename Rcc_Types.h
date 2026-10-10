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
 * Peripherals with a kernel clock multiplexer have one ID for every kernel
 * clock source (RCC_PERIPH_<peripheral>_<source>), the other peripherals are
 * clocked by their bus clock. Peripherals not available on the selected
 * device are not defined.
 *
 * \note Some peripherals has common clock source multiplexer (e.g. SPI1 / SPI2 /
 *       SPI3, USART2 / USART3 / UART4 / UART5 / UART7 / UART8). If user tries to
 *       change clock source of an enabled group member, error will be returned.
 *       User has to deactivate the peripherals of the group first and then
 *       configure the clock source again. */
typedef enum rcc_PeriphId_t
{
    /*------------------------- System core (no clock enable) ----------------*/
    RCC_PERIPH_SYSTICK            = 0u, /**< System Tick timer clocked by CPU clock (processor clock or CPU clock / 8) */

    RCC_PERIPH_IWDG               , /**< Independent watchdog clocked by Low Speed Internal (LSI) oscillator (no clock enable) */

    RCC_PERIPH_RTC_HSE_DIV        , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by RTCPRE used as clock source */
    RCC_PERIPH_RTC_LSE            , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_PERIPH_RTC_LSI            , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

    RCC_PERIPH_LPCLK_HSI          , /**< Peripheral clock (per_ck, CKPER) from High Speed Internal (HSI) oscillator */
    RCC_PERIPH_LPCLK_CSI          , /**< Peripheral clock (per_ck, CKPER) from 4MHz Low Power Internal oscillator (CSI) */
    RCC_PERIPH_LPCLK_HSE          , /**< Peripheral clock (per_ck, CKPER) from High Speed External oscillator (HSE) */

    RCC_PERIPH_TRACE              , /**< Trace clock TRACECLKIN of the trace port / SWO (selected together with SYSCLK, no clock enable) */
    /*------------------------------ System core ------------------------------*/
#if defined(RCC_APB4ENR_SYSCFGEN)
    RCC_PERIPH_SYSCFG                 , /**< System configuration controller clock enable */
#endif
#if defined(RCC_APB4ENR_SBSEN)
    RCC_PERIPH_SBS                    , /**< System configuration, boot and security clock enable */
#endif
#if defined(RCC_AHB3ENR_FLASHEN)
    RCC_PERIPH_FLASH                  , /**< Flash memory interface clock enable */
#endif
#if defined(RCC_AHB1ENR_ARTEN)
    RCC_PERIPH_ART                    , /**< ART accelerator clock enable */
#endif
#if defined(RCC_AHB2ENR_HSEMEN) || \
    defined(RCC_AHB4ENR_HSEMEN)
    RCC_PERIPH_HSEM                   , /**< Hardware semaphore clock enable */
#endif
#if defined(RCC_AHB3ENR_IOMNGREN)
    RCC_PERIPH_IOMNGR                 , /**< OCTOSPI I/O manager clock enable */
#endif
    RCC_PERIPH_CRS                    , /**< Clock recovery system clock enable */
#if defined(RCC_APB3ENR_WWDG1EN)
    RCC_PERIPH_WWDG1                  , /**< Window watchdog 1 clock enable */
#endif
#if defined(RCC_APB1LENR_WWDG2EN)
    RCC_PERIPH_WWDG2                  , /**< Window watchdog 2 (Cortex-M4 core) clock enable */
#endif
#if defined(RCC_APB1ENR1_WWDGEN) || \
    defined(RCC_APB3ENR_WWDGEN)
    RCC_PERIPH_WWDG                   , /**< Window watchdog clock enable */
#endif
#if defined(RCC_APB4ENR_DTSEN)
    RCC_PERIPH_DTS                    , /**< Digital temperature sensor clock enable */
#endif
    RCC_PERIPH_BKPRAM                 , /**< Backup SRAM clock enable */
#if defined(RCC_AHB3ENR_AXISRAMEN)
    RCC_PERIPH_AXISRAM                , /**< AXI SRAM clock enable */
#endif
#if defined(RCC_AHB3ENR_ITCMEN)
    RCC_PERIPH_ITCM                   , /**< ITCM RAM clock enable */
#endif
#if defined(RCC_AHB3ENR_DTCM1EN)
    RCC_PERIPH_DTCM1                  , /**< DTCM 1 RAM clock enable */
#endif
#if defined(RCC_AHB3ENR_DTCM2EN)
    RCC_PERIPH_DTCM2                  , /**< DTCM 2 RAM clock enable */
#endif
#if defined(RCC_AHB2ENR_SRAM1EN)
    RCC_PERIPH_SRAM1                  , /**< AHB SRAM 1 (D2 domain) clock enable */
#endif
#if defined(RCC_AHB2ENR_SRAM2EN)
    RCC_PERIPH_SRAM2                  , /**< AHB SRAM 2 (D2 domain) clock enable */
#endif
#if defined(RCC_AHB2ENR_SRAM3EN)
    RCC_PERIPH_SRAM3                  , /**< D2 domain SRAM 3 clock enable */
#endif
#if defined(RCC_AHB2ENR_AHBSRAM1EN)
    RCC_PERIPH_AHBSRAM1               , /**< CD domain AHB SRAM 1 clock enable */
#endif
#if defined(RCC_AHB2ENR_AHBSRAM2EN)
    RCC_PERIPH_AHBSRAM2               , /**< CD domain AHB SRAM 2 clock enable */
#endif
#if defined(RCC_AHB4ENR_SRDSRAMEN)
    RCC_PERIPH_SRDSRAM                , /**< SRD domain SRAM clock enable */
#endif

    /*------------------------------ DMA --------------------------------------*/
#if defined(RCC_AHB1ENR_DMA1EN)
    RCC_PERIPH_DMA1                   , /**< DMA 1 clock enable */
#endif
#if defined(RCC_AHB1ENR_DMA2EN)
    RCC_PERIPH_DMA2                   , /**< DMA 2 clock enable */
#endif
#if defined(RCC_AHB4ENR_BDMAEN)
    RCC_PERIPH_BDMA                   , /**< Basic DMA (D3 domain) clock enable */
#endif
#if defined(RCC_AHB2ENR_BDMA1EN)
    RCC_PERIPH_BDMA1                  , /**< Basic DMA 1 (DFSDM) clock enable */
#endif
#if defined(RCC_AHB4ENR_BDMA2EN)
    RCC_PERIPH_BDMA2                  , /**< Basic DMA 2 (SRD domain) clock enable */
#endif
#if defined(RCC_AHB3ENR_MDMAEN)
    RCC_PERIPH_MDMA                   , /**< Master DMA clock enable */
#endif
#if defined(RCC_AHB1ENR_GPDMA1EN)
    RCC_PERIPH_GPDMA1                 , /**< General purpose DMA 1 clock enable */
#endif
#if defined(RCC_AHB5ENR_HPDMA1EN)
    RCC_PERIPH_HPDMA1                 , /**< High performance DMA 1 clock enable */
#endif
    RCC_PERIPH_DMA2D                  , /**< Chrom-ART accelerator clock enable */

    /*------------------------------ GPIO -------------------------------------*/
    RCC_PERIPH_GPIOA                  , /**< IO port A clock enable */
    RCC_PERIPH_GPIOB                  , /**< IO port B clock enable */
    RCC_PERIPH_GPIOC                  , /**< IO port C clock enable */
    RCC_PERIPH_GPIOD                  , /**< IO port D clock enable */
    RCC_PERIPH_GPIOE                  , /**< IO port E clock enable */
    RCC_PERIPH_GPIOF                  , /**< IO port F clock enable */
    RCC_PERIPH_GPIOG                  , /**< IO port G clock enable */
    RCC_PERIPH_GPIOH                  , /**< IO port H clock enable */
#if defined(RCC_AHB4ENR_GPIOIEN)
    RCC_PERIPH_GPIOI                  , /**< IO port I clock enable */
#endif
#if defined(RCC_AHB4ENR_GPIOJEN)
    RCC_PERIPH_GPIOJ                  , /**< IO port J clock enable */
#endif
#if defined(RCC_AHB4ENR_GPIOKEN)
    RCC_PERIPH_GPIOK                  , /**< IO port K clock enable */
#endif
#if defined(RCC_AHB4ENR_GPIOMEN)
    RCC_PERIPH_GPIOM                  , /**< IO port M clock enable */
#endif
#if defined(RCC_AHB4ENR_GPIONEN)
    RCC_PERIPH_GPION                  , /**< IO port N clock enable */
#endif
#if defined(RCC_AHB4ENR_GPIOOEN)
    RCC_PERIPH_GPIOO                  , /**< IO port O clock enable */
#endif
#if defined(RCC_AHB4ENR_GPIOPEN)
    RCC_PERIPH_GPIOP                  , /**< IO port P clock enable */
#endif

    /*------------------------------ Timers -----------------------------------*/
    RCC_PERIPH_TIM1                   , /**< Timer 1 clock enable */
    RCC_PERIPH_TIM2                   , /**< Timer 2 clock enable */
    RCC_PERIPH_TIM3                   , /**< Timer 3 clock enable */
    RCC_PERIPH_TIM4                   , /**< Timer 4 clock enable */
    RCC_PERIPH_TIM5                   , /**< Timer 5 clock enable */
    RCC_PERIPH_TIM6                   , /**< Timer 6 clock enable */
    RCC_PERIPH_TIM7                   , /**< Timer 7 clock enable */
#if defined(RCC_APB2ENR_TIM8EN)
    RCC_PERIPH_TIM8                   , /**< Timer 8 clock enable */
#endif
#if defined(RCC_APB2ENR_TIM9EN)
    RCC_PERIPH_TIM9                   , /**< Timer 9 clock enable */
#endif
    RCC_PERIPH_TIM12                  , /**< Timer 12 clock enable */
    RCC_PERIPH_TIM13                  , /**< Timer 13 clock enable */
    RCC_PERIPH_TIM14                  , /**< Timer 14 clock enable */
    RCC_PERIPH_TIM15                  , /**< Timer 15 clock enable */
    RCC_PERIPH_TIM16                  , /**< Timer 16 clock enable */
    RCC_PERIPH_TIM17                  , /**< Timer 17 clock enable */
#if defined(RCC_APB1HENR_TIM23EN)
    RCC_PERIPH_TIM23                  , /**< Timer 23 clock enable */
#endif
#if defined(RCC_APB1HENR_TIM24EN)
    RCC_PERIPH_TIM24                  , /**< Timer 24 clock enable */
#endif
#if defined(RCC_APB2ENR_HRTIMEN)
    RCC_PERIPH_HRTIM                  , /**< High resolution timer clock enable */
#endif
    RCC_PERIPH_LPTIM1_PCLK1           , /**< Low power Timer 1 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_LPTIM1_PLL2P           , /**< Low power Timer 1 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_LPTIM1_PLL3R           , /**< Low power Timer 1 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_LPTIM1_LSE             , /**< Low power Timer 1 clock enable with LSE as kernel clock source */
    RCC_PERIPH_LPTIM1_LSI             , /**< Low power Timer 1 clock enable with LSI as kernel clock source */
    RCC_PERIPH_LPTIM1_LPCLK           , /**< Low power Timer 1 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_LPTIM2_PCLK4           , /**< Low power Timer 2 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_LPTIM2_PLL2P           , /**< Low power Timer 2 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_LPTIM2_PLL3R           , /**< Low power Timer 2 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_LPTIM2_LSE             , /**< Low power Timer 2 clock enable with LSE as kernel clock source */
    RCC_PERIPH_LPTIM2_LSI             , /**< Low power Timer 2 clock enable with LSI as kernel clock source */
    RCC_PERIPH_LPTIM2_LPCLK           , /**< Low power Timer 2 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_LPTIM3_PCLK4           , /**< Low power Timer 3 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_LPTIM3_PLL2P           , /**< Low power Timer 3 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_LPTIM3_PLL3R           , /**< Low power Timer 3 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_LPTIM3_LSE             , /**< Low power Timer 3 clock enable with LSE as kernel clock source */
    RCC_PERIPH_LPTIM3_LSI             , /**< Low power Timer 3 clock enable with LSI as kernel clock source */
    RCC_PERIPH_LPTIM3_LPCLK           , /**< Low power Timer 3 clock enable with CLKP as kernel clock source */
#if defined(RCC_APB4ENR_LPTIM4EN)
    RCC_PERIPH_LPTIM4_PCLK4           , /**< Low power Timer 4 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_LPTIM4_PLL2P           , /**< Low power Timer 4 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_LPTIM4_PLL3R           , /**< Low power Timer 4 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_LPTIM4_LSE             , /**< Low power Timer 4 clock enable with LSE as kernel clock source */
    RCC_PERIPH_LPTIM4_LSI             , /**< Low power Timer 4 clock enable with LSI as kernel clock source */
    RCC_PERIPH_LPTIM4_LPCLK           , /**< Low power Timer 4 clock enable with CLKP as kernel clock source */
#endif
#if defined(RCC_APB4ENR_LPTIM5EN)
    RCC_PERIPH_LPTIM5_PCLK4           , /**< Low power Timer 5 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_LPTIM5_PLL2P           , /**< Low power Timer 5 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_LPTIM5_PLL3R           , /**< Low power Timer 5 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_LPTIM5_LSE             , /**< Low power Timer 5 clock enable with LSE as kernel clock source */
    RCC_PERIPH_LPTIM5_LSI             , /**< Low power Timer 5 clock enable with LSI as kernel clock source */
    RCC_PERIPH_LPTIM5_LPCLK           , /**< Low power Timer 5 clock enable with CLKP as kernel clock source */
#endif

    /*------------------------------ Connectivity -----------------------------*/
    RCC_PERIPH_SPI1_PLL1Q             , /**< SPI / I2S 1 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SPI1_PLL2P             , /**< SPI / I2S 1 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SPI1_PLL3P             , /**< SPI / I2S 1 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SPI1_PIN               , /**< SPI / I2S 1 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SPI1_LPCLK             , /**< SPI / I2S 1 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_SPI2_PLL1Q             , /**< SPI / I2S 2 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SPI2_PLL2P             , /**< SPI / I2S 2 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SPI2_PLL3P             , /**< SPI / I2S 2 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SPI2_PIN               , /**< SPI / I2S 2 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SPI2_LPCLK             , /**< SPI / I2S 2 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_SPI3_PLL1Q             , /**< SPI / I2S 3 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SPI3_PLL2P             , /**< SPI / I2S 3 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SPI3_PLL3P             , /**< SPI / I2S 3 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SPI3_PIN               , /**< SPI / I2S 3 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SPI3_LPCLK             , /**< SPI / I2S 3 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_SPI4_PCLK2             , /**< SPI / I2S 4 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_SPI4_PLL2Q             , /**< SPI / I2S 4 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_SPI4_PLL3Q             , /**< SPI / I2S 4 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_SPI4_HSI               , /**< SPI / I2S 4 clock enable with HSI as kernel clock source */
    RCC_PERIPH_SPI4_CSI               , /**< SPI / I2S 4 clock enable with CSI as kernel clock source */
    RCC_PERIPH_SPI4_HSE               , /**< SPI / I2S 4 clock enable with HSE as kernel clock source */
    RCC_PERIPH_SPI5_PCLK2             , /**< SPI / I2S 5 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_SPI5_PLL2Q             , /**< SPI / I2S 5 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_SPI5_PLL3Q             , /**< SPI / I2S 5 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_SPI5_HSI               , /**< SPI / I2S 5 clock enable with HSI as kernel clock source */
    RCC_PERIPH_SPI5_CSI               , /**< SPI / I2S 5 clock enable with CSI as kernel clock source */
    RCC_PERIPH_SPI5_HSE               , /**< SPI / I2S 5 clock enable with HSE as kernel clock source */
    RCC_PERIPH_SPI6_PCLK4             , /**< SPI / I2S 6 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_SPI6_PLL2Q             , /**< SPI / I2S 6 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_SPI6_PLL3Q             , /**< SPI / I2S 6 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_SPI6_HSI               , /**< SPI / I2S 6 clock enable with HSI as kernel clock source */
    RCC_PERIPH_SPI6_CSI               , /**< SPI / I2S 6 clock enable with CSI as kernel clock source */
    RCC_PERIPH_SPI6_HSE               , /**< SPI / I2S 6 clock enable with HSE as kernel clock source */
#if defined(LL_RCC_SPI6_CLKSOURCE_I2S_CKIN)
    RCC_PERIPH_SPI6_PIN               , /**< SPI / I2S 6 clock enable with I2S_CKIN as kernel clock source */
#endif
    RCC_PERIPH_I2C1_PCLK1             , /**< I2C 1 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_I2C1_PLL3R             , /**< I2C 1 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_I2C1_HSI               , /**< I2C 1 clock enable with HSI as kernel clock source */
    RCC_PERIPH_I2C1_CSI               , /**< I2C 1 clock enable with CSI as kernel clock source */
    RCC_PERIPH_I2C2_PCLK1             , /**< I2C 2 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_I2C2_PLL3R             , /**< I2C 2 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_I2C2_HSI               , /**< I2C 2 clock enable with HSI as kernel clock source */
    RCC_PERIPH_I2C2_CSI               , /**< I2C 2 clock enable with CSI as kernel clock source */
    RCC_PERIPH_I2C3_PCLK1             , /**< I2C 3 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_I2C3_PLL3R             , /**< I2C 3 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_I2C3_HSI               , /**< I2C 3 clock enable with HSI as kernel clock source */
    RCC_PERIPH_I2C3_CSI               , /**< I2C 3 clock enable with CSI as kernel clock source */
#if defined(RCC_APB4ENR_I2C4EN)
    RCC_PERIPH_I2C4_PCLK4             , /**< I2C 4 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_I2C4_PLL3R             , /**< I2C 4 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_I2C4_HSI               , /**< I2C 4 clock enable with HSI as kernel clock source */
    RCC_PERIPH_I2C4_CSI               , /**< I2C 4 clock enable with CSI as kernel clock source */
#endif
#if defined(RCC_APB1LENR_I2C5EN)
    RCC_PERIPH_I2C5_PCLK1             , /**< I2C 5 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_I2C5_PLL3R             , /**< I2C 5 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_I2C5_HSI               , /**< I2C 5 clock enable with HSI as kernel clock source */
    RCC_PERIPH_I2C5_CSI               , /**< I2C 5 clock enable with CSI as kernel clock source */
#endif
    RCC_PERIPH_USART1_PCLK2           , /**< USART 1 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_USART1_PLL2Q           , /**< USART 1 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_USART1_PLL3Q           , /**< USART 1 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USART1_HSI             , /**< USART 1 clock enable with HSI as kernel clock source */
    RCC_PERIPH_USART1_CSI             , /**< USART 1 clock enable with CSI as kernel clock source */
    RCC_PERIPH_USART1_LSE             , /**< USART 1 clock enable with LSE as kernel clock source */
    RCC_PERIPH_USART2_PCLK1           , /**< USART 2 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_USART2_PLL2Q           , /**< USART 2 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_USART2_PLL3Q           , /**< USART 2 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USART2_HSI             , /**< USART 2 clock enable with HSI as kernel clock source */
    RCC_PERIPH_USART2_CSI             , /**< USART 2 clock enable with CSI as kernel clock source */
    RCC_PERIPH_USART2_LSE             , /**< USART 2 clock enable with LSE as kernel clock source */
    RCC_PERIPH_USART3_PCLK1           , /**< USART 3 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_USART3_PLL2Q           , /**< USART 3 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_USART3_PLL3Q           , /**< USART 3 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USART3_HSI             , /**< USART 3 clock enable with HSI as kernel clock source */
    RCC_PERIPH_USART3_CSI             , /**< USART 3 clock enable with CSI as kernel clock source */
    RCC_PERIPH_USART3_LSE             , /**< USART 3 clock enable with LSE as kernel clock source */
    RCC_PERIPH_UART4_PCLK1            , /**< UART 4 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_UART4_PLL2Q            , /**< UART 4 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_UART4_PLL3Q            , /**< UART 4 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_UART4_HSI              , /**< UART 4 clock enable with HSI as kernel clock source */
    RCC_PERIPH_UART4_CSI              , /**< UART 4 clock enable with CSI as kernel clock source */
    RCC_PERIPH_UART4_LSE              , /**< UART 4 clock enable with LSE as kernel clock source */
    RCC_PERIPH_UART5_PCLK1            , /**< UART 5 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_UART5_PLL2Q            , /**< UART 5 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_UART5_PLL3Q            , /**< UART 5 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_UART5_HSI              , /**< UART 5 clock enable with HSI as kernel clock source */
    RCC_PERIPH_UART5_CSI              , /**< UART 5 clock enable with CSI as kernel clock source */
    RCC_PERIPH_UART5_LSE              , /**< UART 5 clock enable with LSE as kernel clock source */
#if defined(RCC_APB2ENR_USART6EN)
    RCC_PERIPH_USART6_PCLK2           , /**< USART 6 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_USART6_PLL2Q           , /**< USART 6 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_USART6_PLL3Q           , /**< USART 6 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USART6_HSI             , /**< USART 6 clock enable with HSI as kernel clock source */
    RCC_PERIPH_USART6_CSI             , /**< USART 6 clock enable with CSI as kernel clock source */
    RCC_PERIPH_USART6_LSE             , /**< USART 6 clock enable with LSE as kernel clock source */
#endif
    RCC_PERIPH_UART7_PCLK1            , /**< UART 7 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_UART7_PLL2Q            , /**< UART 7 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_UART7_PLL3Q            , /**< UART 7 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_UART7_HSI              , /**< UART 7 clock enable with HSI as kernel clock source */
    RCC_PERIPH_UART7_CSI              , /**< UART 7 clock enable with CSI as kernel clock source */
    RCC_PERIPH_UART7_LSE              , /**< UART 7 clock enable with LSE as kernel clock source */
    RCC_PERIPH_UART8_PCLK1            , /**< UART 8 clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_UART8_PLL2Q            , /**< UART 8 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_UART8_PLL3Q            , /**< UART 8 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_UART8_HSI              , /**< UART 8 clock enable with HSI as kernel clock source */
    RCC_PERIPH_UART8_CSI              , /**< UART 8 clock enable with CSI as kernel clock source */
    RCC_PERIPH_UART8_LSE              , /**< UART 8 clock enable with LSE as kernel clock source */
#if defined(RCC_APB2ENR_UART9EN)
    RCC_PERIPH_UART9_PCLK2            , /**< UART 9 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_UART9_PLL2Q            , /**< UART 9 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_UART9_PLL3Q            , /**< UART 9 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_UART9_HSI              , /**< UART 9 clock enable with HSI as kernel clock source */
    RCC_PERIPH_UART9_CSI              , /**< UART 9 clock enable with CSI as kernel clock source */
    RCC_PERIPH_UART9_LSE              , /**< UART 9 clock enable with LSE as kernel clock source */
#endif
#if defined(RCC_APB2ENR_USART10EN)
    RCC_PERIPH_USART10_PCLK2          , /**< USART 10 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_USART10_PLL2Q          , /**< USART 10 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_USART10_PLL3Q          , /**< USART 10 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USART10_HSI            , /**< USART 10 clock enable with HSI as kernel clock source */
    RCC_PERIPH_USART10_CSI            , /**< USART 10 clock enable with CSI as kernel clock source */
    RCC_PERIPH_USART10_LSE            , /**< USART 10 clock enable with LSE as kernel clock source */
#endif
    RCC_PERIPH_LPUART1_PCLK4          , /**< Low power UART 1 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_LPUART1_PLL2Q          , /**< Low power UART 1 clock enable with PLL2Q as kernel clock source */
    RCC_PERIPH_LPUART1_PLL3Q          , /**< Low power UART 1 clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_LPUART1_HSI            , /**< Low power UART 1 clock enable with HSI as kernel clock source */
    RCC_PERIPH_LPUART1_CSI            , /**< Low power UART 1 clock enable with CSI as kernel clock source */
    RCC_PERIPH_LPUART1_LSE            , /**< Low power UART 1 clock enable with LSE as kernel clock source */
    RCC_PERIPH_FDCAN_HSE              , /**< FDCAN clock enable with HSE as kernel clock source */
    RCC_PERIPH_FDCAN_PLL1Q            , /**< FDCAN clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_FDCAN_PLL2Q            , /**< FDCAN clock enable with PLL2Q as kernel clock source */
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL1Q)
    RCC_PERIPH_SDMMC1_PLL1Q           , /**< SDMMC 1 clock enable with PLL1Q as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2R)
    RCC_PERIPH_SDMMC1_PLL2R           , /**< SDMMC 1 clock enable with PLL2R as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2S)
    RCC_PERIPH_SDMMC1_PLL2S           , /**< SDMMC 1 clock enable with PLL2S as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2T)
    RCC_PERIPH_SDMMC1_PLL2T           , /**< SDMMC 1 clock enable with PLL2T as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL1Q)
    RCC_PERIPH_SDMMC2_PLL1Q           , /**< SDMMC 2 clock enable with PLL1Q as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2R)
    RCC_PERIPH_SDMMC2_PLL2R           , /**< SDMMC 2 clock enable with PLL2R as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2S)
    RCC_PERIPH_SDMMC2_PLL2S           , /**< SDMMC 2 clock enable with PLL2S as kernel clock source */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2T)
    RCC_PERIPH_SDMMC2_PLL2T           , /**< SDMMC 2 clock enable with PLL2T as kernel clock source */
#endif
    RCC_PERIPH_FMC_HCLK               , /**< Flexible memory controller clock enable with HCLK as kernel clock source */
    RCC_PERIPH_FMC_PLL1Q              , /**< Flexible memory controller clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_FMC_PLL2R              , /**< Flexible memory controller clock enable with PLL2R as kernel clock source */
#if defined(LL_RCC_FMC_CLKSOURCE_CLKP)
    RCC_PERIPH_FMC_LPCLK              , /**< Flexible memory controller clock enable with CLKP as kernel clock source */
#endif
#if defined(LL_RCC_FMC_CLKSOURCE_HSI)
    RCC_PERIPH_FMC_HSI                , /**< Flexible memory controller clock enable with HSI as kernel clock source */
#endif
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_PERIPH_QSPI_HCLK              , /**< Quad SPI clock enable with HCLK as kernel clock source */
    RCC_PERIPH_QSPI_PLL1Q             , /**< Quad SPI clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_QSPI_PLL2R             , /**< Quad SPI clock enable with PLL2R as kernel clock source */
    RCC_PERIPH_QSPI_LPCLK             , /**< Quad SPI clock enable with CLKP as kernel clock source */
#endif
#if defined(RCC_AHB3ENR_OSPI1EN)
    RCC_PERIPH_OSPI1_HCLK             , /**< Octo SPI 1 clock enable with HCLK as kernel clock source */
    RCC_PERIPH_OSPI1_PLL1Q            , /**< Octo SPI 1 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_OSPI1_PLL2R            , /**< Octo SPI 1 clock enable with PLL2R as kernel clock source */
    RCC_PERIPH_OSPI1_LPCLK            , /**< Octo SPI 1 clock enable with CLKP as kernel clock source */
#endif
#if defined(RCC_AHB3ENR_OSPI2EN)
    RCC_PERIPH_OSPI2_HCLK             , /**< Octo SPI 2 clock enable with HCLK as kernel clock source */
    RCC_PERIPH_OSPI2_PLL1Q            , /**< Octo SPI 2 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_OSPI2_PLL2R            , /**< Octo SPI 2 clock enable with PLL2R as kernel clock source */
    RCC_PERIPH_OSPI2_LPCLK            , /**< Octo SPI 2 clock enable with CLKP as kernel clock source */
#endif
#if defined(RCC_AHB3ENR_OTFDEC1EN)
    RCC_PERIPH_OTFDEC1                , /**< On-the-fly decryption 1 clock enable */
#endif
#if defined(RCC_AHB3ENR_OTFDEC2EN)
    RCC_PERIPH_OTFDEC2                , /**< On-the-fly decryption 2 clock enable */
#endif
#if defined(RCC_AHB5ENR_XSPI1EN)
    RCC_PERIPH_XSPI1_HCLK             , /**< Extended SPI 1 clock enable with HCLK as kernel clock source */
    RCC_PERIPH_XSPI1_PLL2S            , /**< Extended SPI 1 clock enable with PLL2S as kernel clock source */
    RCC_PERIPH_XSPI1_PLL2T            , /**< Extended SPI 1 clock enable with PLL2T as kernel clock source */
#endif
#if defined(RCC_AHB5ENR_XSPI2EN)
    RCC_PERIPH_XSPI2_HCLK             , /**< Extended SPI 2 clock enable with HCLK as kernel clock source */
    RCC_PERIPH_XSPI2_PLL2S            , /**< Extended SPI 2 clock enable with PLL2S as kernel clock source */
    RCC_PERIPH_XSPI2_PLL2T            , /**< Extended SPI 2 clock enable with PLL2T as kernel clock source */
#endif
#if defined(RCC_AHB5ENR_XSPIMEN)
    RCC_PERIPH_XSPIM                  , /**< Extended SPI I/O manager clock enable */
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSEN)
    RCC_PERIPH_USB1OTGHS_PLL1Q        , /**< USB 1 OTG HS clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_USB1OTGHS_PLL3Q        , /**< USB 1 OTG HS clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USB1OTGHS_HSI48        , /**< USB 1 OTG HS clock enable with HSI48 as kernel clock source */
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSULPIEN)
    RCC_PERIPH_USB1OTGHSULPI          , /**< USB 1 OTG HS ULPI clock enable */
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSEN)
    RCC_PERIPH_USB2OTGFS_PLL1Q        , /**< USB 2 OTG FS clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_USB2OTGFS_PLL3Q        , /**< USB 2 OTG FS clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_USB2OTGFS_HSI48        , /**< USB 2 OTG FS clock enable with HSI48 as kernel clock source */
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSULPIEN)
    RCC_PERIPH_USB2OTGFSULPI          , /**< USB 2 OTG FS ULPI clock enable */
#endif
#if defined(RCC_AHB1ENR_OTGHSEN)
    RCC_PERIPH_OTGHS                  , /**< USB OTG HS clock enable */
#endif
#if defined(RCC_AHB1ENR_OTGFSEN)
    RCC_PERIPH_OTGFS_HSI48            , /**< USB OTG FS clock enable with HSI48 as kernel clock source */
    RCC_PERIPH_OTGFS_PLL3Q            , /**< USB OTG FS clock enable with PLL3Q as kernel clock source */
    RCC_PERIPH_OTGFS_HSE              , /**< USB OTG FS clock enable with HSE as kernel clock source */
    RCC_PERIPH_OTGFS_CLK48            , /**< USB OTG FS clock enable with CLK48 as kernel clock source */
#endif
#if defined(RCC_AHB1ENR_USBPHYCEN)
    RCC_PERIPH_USBPHYC_HSE            , /**< USB HS PHY controller clock enable with HSE as kernel clock source */
    RCC_PERIPH_USBPHYC_HSE_DIV2       , /**< USB HS PHY controller clock enable with HSE_DIV_2 as kernel clock source */
    RCC_PERIPH_USBPHYC_PLL3Q          , /**< USB HS PHY controller clock enable with PLL3Q as kernel clock source */
#endif
#if defined(RCC_APB1ENR2_UCPD1EN)
    RCC_PERIPH_UCPD1                  , /**< USB Type-C power delivery 1 clock enable */
#endif
#if defined(RCC_AHB1ENR_ETH1MACEN)
    RCC_PERIPH_ETH1MAC                , /**< Ethernet MAC clock enable */
#endif
#if defined(RCC_AHB1ENR_ETH1TXEN)
    RCC_PERIPH_ETH1TX                 , /**< Ethernet transmission clock enable */
#endif
#if defined(RCC_AHB1ENR_ETH1RXEN)
    RCC_PERIPH_ETH1RX                 , /**< Ethernet reception clock enable */
#endif
    RCC_PERIPH_MDIOS                  , /**< MDIO slave clock enable */
#if defined(RCC_APB1HENR_SWPMIEN)
    RCC_PERIPH_SWPMI_PCLK1            , /**< Single wire protocol master interface clock enable with PCLK1 as kernel clock source */
    RCC_PERIPH_SWPMI_HSI              , /**< Single wire protocol master interface clock enable with HSI as kernel clock source */
#endif
    RCC_PERIPH_CEC_LSE                , /**< HDMI-CEC clock enable with LSE as kernel clock source */
    RCC_PERIPH_CEC_LSI                , /**< HDMI-CEC clock enable with LSI as kernel clock source */
    RCC_PERIPH_CEC_CSI_DIV122         , /**< HDMI-CEC clock enable with CSI_DIV122 as kernel clock source */
    RCC_PERIPH_SPDIFRX_PLL1Q          , /**< SPDIF receiver clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SPDIFRX_PLL2R          , /**< SPDIF receiver clock enable with PLL2R as kernel clock source */
    RCC_PERIPH_SPDIFRX_PLL3R          , /**< SPDIF receiver clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_SPDIFRX_HSI            , /**< SPDIF receiver clock enable with HSI as kernel clock source */
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_PERIPH_DFSDM1_PCLK2           , /**< DFSDM 1 clock enable with PCLK2 as kernel clock source */
    RCC_PERIPH_DFSDM1_SYSCLK          , /**< DFSDM 1 clock enable with SYSCLK as kernel clock source */
#endif
#if defined(RCC_APB4ENR_DFSDM2EN)
    RCC_PERIPH_DFSDM2_PCLK4           , /**< DFSDM 2 clock enable with PCLK4 as kernel clock source */
    RCC_PERIPH_DFSDM2_SYSCLK          , /**< DFSDM 2 clock enable with SYSCLK as kernel clock source */
#endif

    /*------------------------------ Multimedia -------------------------------*/
    RCC_PERIPH_SAI1_PLL1Q             , /**< Serial audio interface 1 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SAI1_PLL2P             , /**< Serial audio interface 1 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SAI1_PLL3P             , /**< Serial audio interface 1 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SAI1_PIN               , /**< Serial audio interface 1 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SAI1_LPCLK             , /**< Serial audio interface 1 clock enable with CLKP as kernel clock source */
#if defined(RCC_APB2ENR_SAI2EN)
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL1Q) || \
    defined(LL_RCC_SAI2_CLKSOURCE_PLL1Q)
    RCC_PERIPH_SAI2_PLL1Q             , /**< Serial audio interface 2 clock enable with PLL1Q as kernel clock source */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL2P) || \
    defined(LL_RCC_SAI2_CLKSOURCE_PLL2P)
    RCC_PERIPH_SAI2_PLL2P             , /**< Serial audio interface 2 clock enable with PLL2P as kernel clock source */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL3P) || \
    defined(LL_RCC_SAI2_CLKSOURCE_PLL3P)
    RCC_PERIPH_SAI2_PLL3P             , /**< Serial audio interface 2 clock enable with PLL3P as kernel clock source */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_I2S_CKIN) || \
    defined(LL_RCC_SAI2_CLKSOURCE_I2S_CKIN)
    RCC_PERIPH_SAI2_PIN               , /**< Serial audio interface 2 clock enable with I2S_CKIN as kernel clock source */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_CLKP) || \
    defined(LL_RCC_SAI2_CLKSOURCE_CLKP)
    RCC_PERIPH_SAI2_LPCLK             , /**< Serial audio interface 2 clock enable with CLKP as kernel clock source */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL1Q)
    RCC_PERIPH_SAI2A_PLL1Q            , /**< Serial audio interface 2 clock enable with PLL1Q as kernel clock source */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL2P)
    RCC_PERIPH_SAI2A_PLL2P            , /**< Serial audio interface 2 clock enable with PLL2P as kernel clock source */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL3P)
    RCC_PERIPH_SAI2A_PLL3P            , /**< Serial audio interface 2 clock enable with PLL3P as kernel clock source */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_I2S_CKIN)
    RCC_PERIPH_SAI2A_PIN              , /**< Serial audio interface 2 clock enable with I2S_CKIN as kernel clock source */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_CLKP)
    RCC_PERIPH_SAI2A_LPCLK            , /**< Serial audio interface 2 clock enable with CLKP as kernel clock source */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_SPDIF)
    RCC_PERIPH_SAI2A_SPDIF            , /**< Serial audio interface 2 clock enable with SPDIF as kernel clock source */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL1Q)
    RCC_PERIPH_SAI2B_PLL1Q            , /**< Serial audio interface 2 clock enable with PLL1Q as kernel clock source */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL2P)
    RCC_PERIPH_SAI2B_PLL2P            , /**< Serial audio interface 2 clock enable with PLL2P as kernel clock source */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL3P)
    RCC_PERIPH_SAI2B_PLL3P            , /**< Serial audio interface 2 clock enable with PLL3P as kernel clock source */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_I2S_CKIN)
    RCC_PERIPH_SAI2B_PIN              , /**< Serial audio interface 2 clock enable with I2S_CKIN as kernel clock source */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_CLKP)
    RCC_PERIPH_SAI2B_LPCLK            , /**< Serial audio interface 2 clock enable with CLKP as kernel clock source */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_SPDIF)
    RCC_PERIPH_SAI2B_SPDIF            , /**< Serial audio interface 2 clock enable with SPDIF as kernel clock source */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_SPDIFRX)
    RCC_PERIPH_SAI2_SPDIF             , /**< Serial audio interface 2 clock enable with SPDIFRX as kernel clock source */
#endif
#endif
#if defined(RCC_APB2ENR_SAI3EN)
    RCC_PERIPH_SAI3_PLL1Q             , /**< Serial audio interface 3 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SAI3_PLL2P             , /**< Serial audio interface 3 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SAI3_PLL3P             , /**< Serial audio interface 3 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SAI3_PIN               , /**< Serial audio interface 3 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SAI3_LPCLK             , /**< Serial audio interface 3 clock enable with CLKP as kernel clock source */
#endif
#if defined(RCC_APB4ENR_SAI4EN)
    RCC_PERIPH_SAI4A_PLL1Q            , /**< Serial audio interface 4 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SAI4A_PLL2P            , /**< Serial audio interface 4 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SAI4A_PLL3P            , /**< Serial audio interface 4 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SAI4A_PIN              , /**< Serial audio interface 4 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SAI4A_LPCLK            , /**< Serial audio interface 4 clock enable with CLKP as kernel clock source */
#if defined(LL_RCC_SAI4A_CLKSOURCE_SPDIF)
    RCC_PERIPH_SAI4A_SPDIF            , /**< Serial audio interface 4 clock enable with SPDIF as kernel clock source */
#endif
    RCC_PERIPH_SAI4B_PLL1Q            , /**< Serial audio interface 4 clock enable with PLL1Q as kernel clock source */
    RCC_PERIPH_SAI4B_PLL2P            , /**< Serial audio interface 4 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_SAI4B_PLL3P            , /**< Serial audio interface 4 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_SAI4B_PIN              , /**< Serial audio interface 4 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_SAI4B_LPCLK            , /**< Serial audio interface 4 clock enable with CLKP as kernel clock source */
#if defined(LL_RCC_SAI4B_CLKSOURCE_SPDIF)
    RCC_PERIPH_SAI4B_SPDIF            , /**< Serial audio interface 4 clock enable with SPDIF as kernel clock source */
#endif
#endif
#if defined(RCC_AHB1ENR_ADF1EN)
    RCC_PERIPH_ADF1_HCLK              , /**< Audio digital filter 1 clock enable with HCLK as kernel clock source */
    RCC_PERIPH_ADF1_PLL2P             , /**< Audio digital filter 1 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_ADF1_PLL3P             , /**< Audio digital filter 1 clock enable with PLL3P as kernel clock source */
    RCC_PERIPH_ADF1_PIN               , /**< Audio digital filter 1 clock enable with I2S_CKIN as kernel clock source */
    RCC_PERIPH_ADF1_CSI               , /**< Audio digital filter 1 clock enable with CSI as kernel clock source */
    RCC_PERIPH_ADF1_HSI               , /**< Audio digital filter 1 clock enable with HSI as kernel clock source */
#endif
#if defined(RCC_AHB2ENR_DCMIEN)
    RCC_PERIPH_DCMI                   , /**< Digital camera interface clock enable */
#endif
#if defined(RCC_AHB2ENR_DCMI_PSSIEN)
    RCC_PERIPH_DCMI_PSSI              , /**< Digital camera interface / PSSI clock enable */
#endif
#if defined(RCC_APB5ENR_DCMIPPEN)
    RCC_PERIPH_DCMIPP                 , /**< Digital camera interface pixel pipeline clock enable */
#endif
#if defined(RCC_AHB2ENR_PSSIEN)
    RCC_PERIPH_PSSI_PLL3R             , /**< Parallel synchronous slave interface clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_PSSI_LPCLK             , /**< Parallel synchronous slave interface clock enable with CLKP as kernel clock source */
#endif
#if defined(RCC_APB3ENR_LTDCEN) || \
    defined(RCC_APB5ENR_LTDCEN)
    RCC_PERIPH_LTDC                   , /**< LCD-TFT controller clock enable */
#endif
#if defined(RCC_APB3ENR_DSIEN)
    RCC_PERIPH_DSI_PHY                , /**< DSI host clock enable with PHY as kernel clock source */
    RCC_PERIPH_DSI_PLL2Q              , /**< DSI host clock enable with PLL2Q as kernel clock source */
#endif
#if defined(RCC_AHB3ENR_JPGDECEN)
    RCC_PERIPH_JPGDEC                 , /**< JPEG codec clock enable */
#endif
#if defined(RCC_AHB5ENR_JPEGEN)
    RCC_PERIPH_JPEG                   , /**< JPEG codec clock enable */
#endif
#if defined(RCC_AHB3ENR_GFXMMUEN) || \
    defined(RCC_AHB5ENR_GFXMMUEN)
    RCC_PERIPH_GFXMMU                 , /**< Graphic MMU clock enable */
#endif
#if defined(RCC_APB5ENR_GFXTIMEN)
    RCC_PERIPH_GFXTIM                 , /**< Graphic timer clock enable */
#endif
#if defined(RCC_AHB5ENR_GPU2DEN)
    RCC_PERIPH_GPU2D                  , /**< Graphic processing unit 2D clock enable */
#endif

    /*------------------------------ Analog -----------------------------------*/
    RCC_PERIPH_ADC12_PLL2P            , /**< ADC 1 / 2 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_ADC12_PLL3R            , /**< ADC 1 / 2 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_ADC12_LPCLK            , /**< ADC 1 / 2 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_ADC12_HCLK             , /**< ADC 1 / 2 clock enable, synchronous clock HCLK (CKMODE) */
#if defined(RCC_AHB4ENR_ADC3EN)
    RCC_PERIPH_ADC3_PLL2P             , /**< ADC 3 clock enable with PLL2P as kernel clock source */
    RCC_PERIPH_ADC3_PLL3R             , /**< ADC 3 clock enable with PLL3R as kernel clock source */
    RCC_PERIPH_ADC3_LPCLK             , /**< ADC 3 clock enable with CLKP as kernel clock source */
    RCC_PERIPH_ADC3_HCLK              , /**< ADC 3 clock enable, synchronous clock HCLK (CKMODE) */
#endif
#if defined(RCC_APB1LENR_DAC12EN)
    RCC_PERIPH_DAC12                  , /**< DAC 1 clock enable */
#endif
#if defined(RCC_APB4ENR_DAC2EN)
    RCC_PERIPH_DAC2                   , /**< DAC 2 clock enable */
#endif
#if defined(RCC_APB4ENR_COMP12EN)
    RCC_PERIPH_COMP12                 , /**< Comparators 1 / 2 clock enable */
#endif
#if defined(RCC_APB1HENR_OPAMPEN)
    RCC_PERIPH_OPAMP                  , /**< Operational amplifiers clock enable */
#endif
    RCC_PERIPH_VREF                   , /**< Voltage reference buffer clock enable */

    /*------------------------------ Security ---------------------------------*/
#if defined(RCC_AHB2ENR_CRYPEN) || \
    defined(RCC_AHB3ENR_CRYPEN)
    RCC_PERIPH_CRYP                   , /**< Cryptographic processor clock enable */
#endif
#if defined(RCC_AHB2ENR_HASHEN) || \
    defined(RCC_AHB3ENR_HASHEN)
    RCC_PERIPH_HASH                   , /**< Hash processor clock enable */
#endif
    RCC_PERIPH_RNG_HSI48              , /**< Random number generator clock enable with HSI48 as kernel clock source */
#if defined(LL_RCC_RNG_CLKSOURCE_PLL1Q)
    RCC_PERIPH_RNG_PLL1Q              , /**< Random number generator clock enable with PLL1Q as kernel clock source */
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_LSE)
    RCC_PERIPH_RNG_LSE                , /**< Random number generator clock enable with LSE as kernel clock source */
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_LSI)
    RCC_PERIPH_RNG_LSI                , /**< Random number generator clock enable with LSI as kernel clock source */
#endif
#if defined(RCC_AHB3ENR_PKAEN)
    RCC_PERIPH_PKA                    , /**< Public key accelerator clock enable */
#endif
#if defined(RCC_AHB3ENR_SAESEN)
    RCC_PERIPH_SAES                   , /**< Secure AES coprocessor clock enable */
#endif

    /*------------------------------ Computing --------------------------------*/
    RCC_PERIPH_CRC                    , /**< CRC calculation unit clock enable */
#if defined(RCC_AHB2ENR_CORDICEN)
    RCC_PERIPH_CORDIC                 , /**< CORDIC co-processor clock enable */
#endif
#if defined(RCC_AHB2ENR_FMACEN)
    RCC_PERIPH_FMAC                   , /**< Filter math accelerator clock enable */
#endif

    RCC_PERIPH_ID_CNT
}   rcc_PeriphId_t;

/*---------------------------- Reset source flags ----------------------------*/

/** \brief List of reset sources stored in RCC reset status register (RSR) */
typedef enum
{
    RCC_RESET_SRC_PIN   = 0u, /**< Reset from NRST pin                                  */
    RCC_RESET_SRC_BOR       , /**< Brown-out reset (BOR), also set after power-on reset */
    RCC_RESET_SRC_SW        , /**< System reset requested by software                   */
    RCC_RESET_SRC_IWDG      , /**< Independent watchdog 1 reset                         */
    RCC_RESET_SRC_WWDG      , /**< Window watchdog 1 reset                              */
    RCC_RESET_SRC_LPWR      , /**< Illegal Stop / Standby mode entry reset              */
    RCC_RESET_SRC_CNT         /**< Number of reset sources                              */
}   rcc_ResetSrc_t;

/*------------------------ Clock sources configuration -----------------------*/

/** \brief Enumeration of High Speed External (HSE) input configuration */
typedef enum
{
    RCC_HSE_TYPE_NONE     = 0u , /**< No external clock connected to HSE pin */
    RCC_HSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_HSE_TYPE_SIG_ANALOG_IN , /**< External low voltage swing signal      */
    RCC_HSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_HseType_t;


/** \brief Enumeration of Low Speed External (LSE) input configuration */
typedef enum
{
    RCC_LSE_TYPE_NONE     = 0u , /**< No external clock connected to LSE pin */
    RCC_LSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_LSE_TYPE_SIG_ANALOG_IN , /**< External low voltage swing signal      */
    RCC_LSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_LseType_t;

/** \brief List of all available internal oscillators */
typedef enum
{
    RCC_OSC_HSI64 = 0u, /**< 64 MHz High Speed Internal (HSI) oscillator. */
    RCC_OSC_HSI48,      /**< 48 MHz High Speed Internal (HSI) oscillator. */
    RCC_OSC_CSI,        /**< 4 MHz Low Power RC (CSI) oscillator. */
    RCC_OSC_LSI,        /**< 32 kHz Low Speed Internal (LSI) oscillator. */
    RCC_OSC_CNT         /**< Count of available internal oscillator */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition.
 * Only the HSI oscillator has a divider (HSIDIV): 1, 2, 4 or 8. */
typedef uint32_t rcc_OscDiv_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration */
typedef enum
{
    RCC_PLL_1 = 0u, /**< Phase Locked Loop 1     */
    RCC_PLL_2,      /**< Phase Locked Loop 2     */
    RCC_PLL_3,      /**< Phase Locked Loop 3     */
    RCC_PLL_CNT     /**< Count of available PLLs */
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 * \note If the PLL is not used, select RCC_PLL_SRC_NONE. Otherwise will be
 *       PLL activated.
 * \note STM32H7 has one clock source multiplexer common for all PLLs - all
 *       active PLLs must use the same clock source. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                               */
    RCC_PLL_SRC_CSI      , /**< PLL will be clocked by 4MHz CSI RC oscillator */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator         */
    RCC_PLL_SRC_HSI      , /**< PLL will be clocked by 64MHz HSI oscillator (after HSI divider) */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                   */
}   rcc_PllClkSrc_t;


/** \brief Phase Locked Loop (PLL) M Divider value.
 * This is clock input divider for PLL (DIVMx).
 * Step size: 1
 * Range    : 1 - 63
 */
typedef uint32_t rcc_PllMDivider_t;


/** \brief Type used to signal values of PLL N multiplier
 * Phase Locked Loop (PLL) Feedback multiplier (DIVNx).
 * Step size: 1
 * Range    : 4 - 512 (8 - 420 on STM32H7A3 / H7B0 / H7B3 and STM32H7R / H7S)
 */
typedef uint32_t rcc_PllNMult_t;


/** \brief Phase Locked Loop (PLL) P output divider value.
 * Step size: 1
 * Range    : 1 - 128
 * \warning PLL1 P output supports even division factors only (2 - 128), the
 *          division factor 1 is supported on STM32H72x/73x and STM32H7A3 / H7B0 /
 *          H7B3 lines only. STM32H7R / H7S lines support all factors 1 - 128.
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

#if defined(STM32H7RS)
/** \brief Phase Locked Loop (PLL) S output divider value (STM32H7R / H7S).
 * Step size: 1
 * Range    : 1 - 8
 */
typedef uint32_t rcc_PllSDivider_t;


/** \brief Phase Locked Loop (PLL) T output divider value (STM32H7R / H7S, PLL2
 * only).
 * Step size: 1
 * Range    : 1 - 8
 */
typedef uint32_t rcc_PllTDivider_t;
#endif

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
 * Step size: 1
 * Range    : 2-63
 */
typedef uint16_t rcc_Rtc_HseDiv_t;

/*------------------------- Clock outputs configuration ----------------------*/

/**
 * \brief Clock Output identification enumeration.
 *
 * \note Low Speed Clock Output (LSCO) is not available on STM32H7 - requests
 *       return error.
 */
typedef enum
{
    RCC_CLK_OUT_MCO1 = 0u, /**< Master Clock Output 1 (MCO1)  */
    RCC_CLK_OUT_MCO2,      /**< Master Clock Output 2 (MCO2)  */
    RCC_CLK_OUT_LSCO,      /**< Low Speed Clock Output (LSCO) */
    RCC_CLK_OUT_CNT        /**< Count of Clock Outputs        */
}   rcc_ClkOut_Id_t;


/**
 * \brief Clock Output's source configuration enumeration.
 */
typedef enum
{
    RCC_CLK_SOURCE_NONE   = 0u, /**< No clock source selected */

    RCC_CLK_SOURCE_MCO1_LSE   , /**< Low Speed External (LSE) will be used as Master Clock Output 1 (MCO1) clock source            */
    RCC_CLK_SOURCE_MCO1_HSE   , /**< High Speed External (HSE) will be used as Master Clock Output 1 (MCO1) clock source           */
    RCC_CLK_SOURCE_MCO1_HSI64 , /**< High Speed Internal (HSI) will be used as Master Clock Output 1 (MCO1) clock source           */
    RCC_CLK_SOURCE_MCO1_HSI48 , /**< 48MHz High Speed Internal (HSI48) will be used as Master Clock Output 1 (MCO1) clock source   */
    RCC_CLK_SOURCE_MCO1_PLL1Q , /**< Phase Locked Loop 1 output Q (PLL1Q) will be used as Master Clock Output 1 (MCO1) clock source */

    RCC_CLK_SOURCE_MCO2_LSI    , /**< Low Speed Internal (LSI) will be used as Master Clock Output 2 (MCO2) clock source                    */
    RCC_CLK_SOURCE_MCO2_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output 2 (MCO2) clock source                   */
    RCC_CLK_SOURCE_MCO2_CSI    , /**< 4MHz Low power internal RC oscillator (CSI) will be used as Master Clock Output 2 (MCO2) clock source */
    RCC_CLK_SOURCE_MCO2_PLL1P  , /**< Phase Locked Loop 1 output P (PLL1P) will be used as Master Clock Output 2 (MCO2) clock source        */
    RCC_CLK_SOURCE_MCO2_PLL2P  , /**< Phase Locked Loop 2 output P (PLL2P) will be used as Master Clock Output 2 (MCO2) clock source        */
    RCC_CLK_SOURCE_MCO2_SYSCLK , /**< System clock (SYSCLK) will be used as Master Clock Output 2 (MCO2) clock source                       */

    RCC_CLK_SOURCE_LSCO_LSI    , /**< Low Speed Internal (LSI) will be used as Low Speed Clock Output (LSCO) clock source (not available on STM32H7) */
    RCC_CLK_SOURCE_LSCO_LSE    , /**< Low Speed External (LSE) will be used as Low Speed Clock Output (LSCO) clock source (not available on STM32H7) */
    RCC_CLK_SOURCE_CNT           /**< Count of clock output sources                                                       */
}   rcc_ClkOut_Source_t;


/**
 * \brief Master Clock Output (MCO) divider value type.
 *
 * Output clock divider value for Clock Output's.
 * Step size: 1
 * Range of values: 1 - 15
 */
typedef uint32_t rcc_ClkOut_Div_t;

/*-------------------------- Clock buses configuration -----------------------*/

/** \brief List of all available clock buses
 *
 * AHB1 - AHB4 are clocked by HCLK (AHB prescaler), APB3 (D1 / CD domain) by
 * APB3 prescaler, APB1 and APB2 (D2 / CD domain) by APB1 / APB2 prescalers and
 * APB4 (D3 / SRD domain) by APB4 prescaler.
 *
 * STM32H7R / H7S: AHB1 - AHB5 are clocked by HCLK (bus matrix prescaler BMPRE),
 * APB1, APB2, APB4 and APB5 by APB prescalers PPRE1 / PPRE2 / PPRE4 / PPRE5,
 * there is no APB3 bus.
 */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u, /**< Advanced High-performance Bus 1                */
    RCC_CLK_BUS_AHB2,      /**< Advanced High-performance Bus 2                */
    RCC_CLK_BUS_AHB3,      /**< Advanced High-performance Bus 3 (D1 / CD)      */
    RCC_CLK_BUS_AHB4,      /**< Advanced High-performance Bus 4 (D3 / SRD)     */
#if defined(STM32H7RS)
    RCC_CLK_BUS_AHB5,      /**< Advanced High-performance Bus 5 (STM32H7R / H7S) */
#endif
    RCC_CLK_BUS_APB1_1,    /**< Advanced Peripheral Bus 1 group 1 (APB1L)      */
    RCC_CLK_BUS_APB1_2,    /**< Advanced Peripheral Bus 1 group 2 (APB1H)      */
    RCC_CLK_BUS_APB2,      /**< Advanced Peripheral Bus 2                      */
#if defined(STM32H7RS)
    RCC_CLK_BUS_APB4,      /**< Advanced Peripheral Bus 4                      */
    RCC_CLK_BUS_APB5,      /**< Advanced Peripheral Bus 5 (STM32H7R / H7S)     */
#else
    RCC_CLK_BUS_APB3,      /**< Advanced Peripheral Bus 3 (D1 / CD)            */
    RCC_CLK_BUS_APB4,      /**< Advanced Peripheral Bus 4 (D3 / SRD)           */
#endif
    RCC_CLK_BUS_CNT        /**< Count of available clock buses                 */
}   rcc_ClkBusId_t;


/** \brief Clock bus divider value type definition.
 * Used for AHB, APB1, APB2, APB3 (APB5 on STM32H7R / H7S) and APB4 clock bus
 * dividers (all available clock busses in \ref rcc_ClkBusId_t ) */
typedef uint32_t rcc_ClkBusDiv_t;


/** \brief System clock source multiplexer configuration list */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_HSI = 0u, /**< HSI will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_CSI     , /**< CSI will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_HSE     , /**< HSE will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_PLL     , /**< PLL1 P output will be used as system clock source */
    RCC_SYSTEM_CLOCK_SOURCE_CNT       /**< Count of available system clock sources  */
}   rcc_SystemClkSrc_t;


/** \brief Type representing numerical value of system clock divider (D1CPRE /
 * CDCPRE, CPRE on STM32H7R / H7S). The CPU clock is divided from SYSCLK. */
typedef enum
{
    RCC_SYS_DIVIDER_1   = LL_RCC_SYSCLK_DIV_1  ,
    RCC_SYS_DIVIDER_2   = LL_RCC_SYSCLK_DIV_2  ,
    RCC_SYS_DIVIDER_4   = LL_RCC_SYSCLK_DIV_4  ,
    RCC_SYS_DIVIDER_8   = LL_RCC_SYSCLK_DIV_8  ,
    RCC_SYS_DIVIDER_16  = LL_RCC_SYSCLK_DIV_16 ,
    RCC_SYS_DIVIDER_64  = LL_RCC_SYSCLK_DIV_64 ,
    RCC_SYS_DIVIDER_128 = LL_RCC_SYSCLK_DIV_128,
    RCC_SYS_DIVIDER_256 = LL_RCC_SYSCLK_DIV_256,
    RCC_SYS_DIVIDER_512 = LL_RCC_SYSCLK_DIV_512
}   rcc_SYS_Div_t;


/** \brief Type representing numerical value of AHB divider (HPRE, bus matrix
 * prescaler BMPRE on STM32H7R / H7S).
 * The value of AHB (HCLK) is divided from CPU clock */
typedef enum
{
    RCC_AHB_DIVIDER_1   = LL_RCC_AHB_DIV_1  ,
    RCC_AHB_DIVIDER_2   = LL_RCC_AHB_DIV_2  ,
    RCC_AHB_DIVIDER_4   = LL_RCC_AHB_DIV_4  ,
    RCC_AHB_DIVIDER_8   = LL_RCC_AHB_DIV_8  ,
    RCC_AHB_DIVIDER_16  = LL_RCC_AHB_DIV_16 ,
    RCC_AHB_DIVIDER_64  = LL_RCC_AHB_DIV_64 ,
    RCC_AHB_DIVIDER_128 = LL_RCC_AHB_DIV_128,
    RCC_AHB_DIVIDER_256 = LL_RCC_AHB_DIV_256,
    RCC_AHB_DIVIDER_512 = LL_RCC_AHB_DIV_512
}   rcc_AHB_Div_t;


/** \brief Type representing numerical value of APB1 divider (D2PPRE1 / CDPPRE1).
 * The value of APB1 is divided from HCLK */
typedef enum
{
    RCC_APB1_DIVIDER_1  = LL_RCC_APB1_DIV_1 ,
    RCC_APB1_DIVIDER_2  = LL_RCC_APB1_DIV_2 ,
    RCC_APB1_DIVIDER_4  = LL_RCC_APB1_DIV_4 ,
    RCC_APB1_DIVIDER_8  = LL_RCC_APB1_DIV_8 ,
    RCC_APB1_DIVIDER_16 = LL_RCC_APB1_DIV_16,
}   rcc_APB1_Div_t;


/** \brief Type representing numerical value of APB2 divider (D2PPRE2 / CDPPRE2).
 * The value of APB2 is divided from HCLK */
typedef enum
{
    RCC_APB2_DIVIDER_1  = LL_RCC_APB2_DIV_1 ,
    RCC_APB2_DIVIDER_2  = LL_RCC_APB2_DIV_2 ,
    RCC_APB2_DIVIDER_4  = LL_RCC_APB2_DIV_4 ,
    RCC_APB2_DIVIDER_8  = LL_RCC_APB2_DIV_8 ,
    RCC_APB2_DIVIDER_16 = LL_RCC_APB2_DIV_16,
}   rcc_APB2_Div_t;


#if defined(STM32H7RS)
/** \brief Type representing numerical value of APB5 divider (PPRE5, STM32H7R /
 * H7S). The value of APB5 is divided from HCLK */
typedef enum
{
    RCC_APB5_DIVIDER_1  = LL_RCC_APB5_DIV_1 ,
    RCC_APB5_DIVIDER_2  = LL_RCC_APB5_DIV_2 ,
    RCC_APB5_DIVIDER_4  = LL_RCC_APB5_DIV_4 ,
    RCC_APB5_DIVIDER_8  = LL_RCC_APB5_DIV_8 ,
    RCC_APB5_DIVIDER_16 = LL_RCC_APB5_DIV_16,
}   rcc_APB5_Div_t;
#else
/** \brief Type representing numerical value of APB3 divider (D1PPRE / CDPPRE).
 * The value of APB3 is divided from HCLK */
typedef enum
{
    RCC_APB3_DIVIDER_1  = LL_RCC_APB3_DIV_1 ,
    RCC_APB3_DIVIDER_2  = LL_RCC_APB3_DIV_2 ,
    RCC_APB3_DIVIDER_4  = LL_RCC_APB3_DIV_4 ,
    RCC_APB3_DIVIDER_8  = LL_RCC_APB3_DIV_8 ,
    RCC_APB3_DIVIDER_16 = LL_RCC_APB3_DIV_16,
}   rcc_APB3_Div_t;
#endif


/** \brief Type representing numerical value of APB4 divider (D3PPRE / SRDPPRE,
 * PPRE4 on STM32H7R / H7S). The value of APB4 is divided from HCLK */
typedef enum
{
    RCC_APB4_DIVIDER_1  = LL_RCC_APB4_DIV_1 ,
    RCC_APB4_DIVIDER_2  = LL_RCC_APB4_DIV_2 ,
    RCC_APB4_DIVIDER_4  = LL_RCC_APB4_DIV_4 ,
    RCC_APB4_DIVIDER_8  = LL_RCC_APB4_DIV_8 ,
    RCC_APB4_DIVIDER_16 = LL_RCC_APB4_DIV_16,
}   rcc_APB4_Div_t;

/*------------------------ Flash and power configuration ---------------------*/

/**
 * \brief Defines number of wait states for Flash memory access.
 *
 * \note \ref Rcc_Init sets the maximal number of wait states during the clock
 *       switch and the number of wait states required by the AXI clock (HCLK)
 *       in the active voltage scale afterwards - the value of the configuration
 *       structure is not used (kept for interface compatibility).
 */
typedef enum
{
#if defined(STM32H7RS)
    RCC_FLASH_LATENCY_0_WS  = ( 0u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_1_WS  = ( 1u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_2_WS  = ( 2u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_3_WS  = ( 3u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_4_WS  = ( 4u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_5_WS  = ( 5u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_6_WS  = ( 6u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_7_WS  = ( 7u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_8_WS  = ( 8u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_9_WS  = ( 9u  << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_10_WS = ( 10u << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_11_WS = ( 11u << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_12_WS = ( 12u << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_13_WS = ( 13u << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_14_WS = ( 14u << FLASH_ACR_LATENCY_Pos ),
    RCC_FLASH_LATENCY_15_WS = ( 15u << FLASH_ACR_LATENCY_Pos ),
#else
    RCC_FLASH_LATENCY_0_WS  = FLASH_ACR_LATENCY_0WS,
    RCC_FLASH_LATENCY_1_WS  = FLASH_ACR_LATENCY_1WS,
    RCC_FLASH_LATENCY_2_WS  = FLASH_ACR_LATENCY_2WS,
    RCC_FLASH_LATENCY_3_WS  = FLASH_ACR_LATENCY_3WS,
    RCC_FLASH_LATENCY_4_WS  = FLASH_ACR_LATENCY_4WS,
    RCC_FLASH_LATENCY_5_WS  = FLASH_ACR_LATENCY_5WS,
    RCC_FLASH_LATENCY_6_WS  = FLASH_ACR_LATENCY_6WS,
    RCC_FLASH_LATENCY_7_WS  = FLASH_ACR_LATENCY_7WS,
    RCC_FLASH_LATENCY_8_WS  = FLASH_ACR_LATENCY_8WS,
    RCC_FLASH_LATENCY_9_WS  = FLASH_ACR_LATENCY_9WS,
    RCC_FLASH_LATENCY_10_WS = FLASH_ACR_LATENCY_10WS,
    RCC_FLASH_LATENCY_11_WS = FLASH_ACR_LATENCY_11WS,
    RCC_FLASH_LATENCY_12_WS = FLASH_ACR_LATENCY_12WS,
    RCC_FLASH_LATENCY_13_WS = FLASH_ACR_LATENCY_13WS,
    RCC_FLASH_LATENCY_14_WS = FLASH_ACR_LATENCY_14WS,
    RCC_FLASH_LATENCY_15_WS = FLASH_ACR_LATENCY_15WS,
#endif
}   rcc_FlashLatency_t;


/**
 * \brief PWR Voltage scaling configuration (VOS) of the core domain.
 *
 * \note Power voltage scaling has to be chosen according to desired
 *       performance (maximal frequencies of each scale are given by the device
 *       datasheet). VOS0 is reached through VOS1 (on STM32H742 / H743 / H745 /
 *       H747 / H750 / H753 / H755 / H757 by the SYSCFG overdrive ODEN).
 * \note Voltage scale other than \ref RCC_PWR_VOLTAGE_SCALE_3 needs the supply
 *       configuration (\ref rcc_PwrSupply_t) written after power-on reset.
 * \note STM32H7R / H7S have two voltage scales only: \ref RCC_PWR_VOLTAGE_SCALE_0
 *       (VOS high) and \ref RCC_PWR_VOLTAGE_SCALE_1 (VOS low, reset value),
 *       requests of the other scales return error.
 */
typedef enum
{
    RCC_PWR_VOLTAGE_SCALE_0 = 0u, /**< VOS0 - highest performance           */
    RCC_PWR_VOLTAGE_SCALE_1,      /**< VOS1                                 */
    RCC_PWR_VOLTAGE_SCALE_2,      /**< VOS2                                 */
    RCC_PWR_VOLTAGE_SCALE_3,      /**< VOS3 - lowest power (reset value)    */
    RCC_PWR_VOLTAGE_SCALE_CNT     /**< Count of voltage scales              */
}   rcc_PwrVoltageScale_t;


/**
 * \brief Power supply configuration of the core domain (PWR_CR3).
 *
 * \warning The supply configuration can be written only once after power-on
 *          reset and must match the supply circuit of the board (LDO / SMPS
 *          wiring). Wrong configuration can stop the device - it has to be
 *          recovered by power cycle and connection under reset.
 *          \ref RCC_PWR_SUPPLY_DEFAULT keeps the supply configuration of the
 *          reset (nothing is written) - safe for every board, but only voltage
 *          scale \ref RCC_PWR_VOLTAGE_SCALE_3 is available then.
 * \note  SMPS configurations are available on devices with SMPS only
 *        (STM32H725 / H735 / H730Q, H745 / H747 / H755 / H757, H7A3Q / H7B0Q /
 *        H7B3Q, all STM32H7R / H7S), requests on other devices return error.
 *        STM32H7R / H7S have no SMPS 2.5 V configurations (requests return error).
 */
typedef enum
{
    RCC_PWR_SUPPLY_DEFAULT = 0u,       /**< Supply configuration of the reset is kept (not written)                 */
    RCC_PWR_SUPPLY_LDO,                /**< Core domain supplied from LDO                                           */
    RCC_PWR_SUPPLY_DIRECT_SMPS,        /**< Core domain supplied from SMPS directly                                 */
    RCC_PWR_SUPPLY_SMPS_1V8_LDO,       /**< SMPS 1.8 V output supplies LDO which supplies core domain               */
    RCC_PWR_SUPPLY_SMPS_2V5_LDO,       /**< SMPS 2.5 V output supplies LDO which supplies core domain               */
    RCC_PWR_SUPPLY_SMPS_1V8_EXT_LDO,   /**< SMPS 1.8 V output supplies external circuits and LDO (core from LDO)     */
    RCC_PWR_SUPPLY_SMPS_2V5_EXT_LDO,   /**< SMPS 2.5 V output supplies external circuits and LDO (core from LDO)     */
    RCC_PWR_SUPPLY_SMPS_1V8_EXT,       /**< SMPS 1.8 V output supplies external source which supplies core domain   */
    RCC_PWR_SUPPLY_SMPS_2V5_EXT,       /**< SMPS 2.5 V output supplies external source which supplies core domain   */
    RCC_PWR_SUPPLY_EXTERNAL_SOURCE,    /**< LDO and SMPS bypassed - core domain supplied from external source       */
    RCC_PWR_SUPPLY_CNT                 /**< Count of supply configurations                                          */
}   rcc_PwrSupply_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL's (common for all PLLs on STM32H7) */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - input divider of the PLL (1 - 63). The reference
     *  frequency (input / M) must be within 1 - 16 MHz. */
    rcc_PllMDivider_t       M_Divider;

    /** N multiplier - VCO = reference frequency x N */
    rcc_PllNMult_t          N_Multiplier;

    /** Output P prescaler - divider. 0 - output not used (disabled). */
    rcc_PllPDivider_t       P_Divider;

    /** Output Q prescaler - divider. 0 - output not used (disabled). */
    rcc_PllQDivider_t       Q_Divider;

    /** Output R prescaler - divider. 0 - output not used (disabled). */
    rcc_PllRDivider_t       R_Divider;

#if defined(STM32H7RS)
    /** Output S prescaler - divider (STM32H7R / H7S). 0 - output not used (disabled). */
    rcc_PllSDivider_t       S_Divider;

    /** Output T prescaler - divider (STM32H7R / H7S, PLL2 only - must be 0 on
     *  PLL1 / PLL3). 0 - output not used (disabled). */
    rcc_PllTDivider_t       T_Divider;
#endif

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
    /** Specifies power supply configuration of the core domain (STM32H7 specific). */
    rcc_PwrSupply_t         PowerSupply;

    /** Specifies HSE clock type. Ignored if HSE is not used */
    rcc_HseType_t           HSE_ClockType;

    /** Specified frequency of the HSE oscillator. Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Specifies configuration of the PLLs */
    rcc_PllConfigStruct_t   Pll_Config[ RCC_PLL_CNT ];

    /**
    * \brief Enables or disables Clock Security System (CSS) of HSE.
    *
    * If the CSS is enabled and a failure of HSE is detected, NMI is generated
    * and the system clock is switched to HSI by hardware.
    *
    * \warning Once enabled, the CSS can't be turned off by software (just by
    *          reset).
    */
    rcc_FunctionState_t     CSS_Enable;

    /** System clock prescaler - divider of the CPU clock (STM32H7 specific) */
    rcc_SYS_Div_t           SYS_Divider;
    /** AHB prescaler - divider */
    rcc_AHB_Div_t           AHB_Divider;
    /** APB1 prescaler - divider */
    rcc_APB1_Div_t          APB1_Divider;
    /** APB2 prescaler - divider */
    rcc_APB2_Div_t          APB2_Divider;
#if defined(STM32H7RS)
    /** APB4 prescaler - divider (STM32H7 specific) */
    rcc_APB4_Div_t          APB4_Divider;
    /** APB5 prescaler - divider (STM32H7R / H7S specific) */
    rcc_APB5_Div_t          APB5_Divider;
#else
    /** APB3 prescaler - divider */
    rcc_APB3_Div_t          APB3_Divider;
    /** APB4 prescaler - divider (STM32H7 specific) */
    rcc_APB4_Div_t          APB4_Divider;
#endif

    /** Value of time in ms [0.001s] between SysTicks */
    rcc_Time_ms_t           SysTickInterval;

    /** Flash latency - not used on STM32H7, \ref Rcc_Init sets the latency
    *  according to the AXI clock and the voltage scaling.
    */
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
