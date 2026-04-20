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
 * \note Some peripherals has common clock source. If user tries to change clock
 *       source for this, error will be returned. User has to reset peripheral
 *       clock source and then configure it again. */
typedef enum rcc_PeriphId_t
{
    /*------------------------------ System core -----------------------------*/
    RCC_PERIPH_FLASH          = 0u, /**< Flash interface clock enable */

    RCC_PERIPH_SBS                , /**< System Backup and Security module clock enable */

    RCC_PERIPH_SYSTICK_HCLK_DIV8  , /**< System Tick timer active with HCLK divided by 8 */
    RCC_PERIPH_SYSTICK_LSI        , /**< System Tick timer active with Low Speed Internal (LSI) Oscillator as clock source */
    RCC_PERIPH_SYSTICK_LSE        , /**< System Tick timer active with Low Speed External (LSE) Oscillator as clock source */

    RCC_PERIPH_RTC_HSE_DIV32      , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source  */
    RCC_PERIPH_RTC_LSE            , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_PERIPH_RTC_LSI            , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */

    RCC_PERIPH_CRS                , /**< CRS Clock Enable */

    RCC_PERIPH_LPCLK_HSI          , /**< Low-Power Peripheral Domain Clock (PER) from 64MHz High Speed Internal (HSI48) oscillator Clock Enable */
    RCC_PERIPH_LPCLK_HSE          , /**< Low-Power Peripheral Domain Clock (PER) from PLL 1 Q output Clock Enable */
    RCC_PERIPH_LPCLK_CSI          , /**< Low-Power Peripheral Domain Clock (PER) from Multi-Speed Internal Oscillator output K (MSIK) Clock Enable */

#if defined(DTS)
    RCC_PERIPH_DTS                , /**< Digital Temperature Sensor Clock Enable */
#endif /* DTS */

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

#if defined(GPDMA1)
    RCC_PERIPH_GPDMA1             , /**< GPDMA1 Clock Enable */
#endif /* GPDMA1 */
#if defined(GPDMA2)
    RCC_PERIPH_GPDMA2             , /**< GPDMA2 Clock Enable */
#endif /* GPDMA2 */

#if defined(WWDG)
    RCC_PERIPH_WWDG               , /**< WWDG Clock Enable */
#endif /* WWDG */

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

    /*-------------------------------- Timers --------------------------------*/

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
#if defined(TIM12)
    RCC_PERIPH_TIM12              , /**< TIM12 Clock Enable */
#endif /* TIM12 */
#if defined(TIM13)
    RCC_PERIPH_TIM13              , /**< TIM13 Clock Enable */
#endif /* TIM13 */
#if defined(TIM14)
    RCC_PERIPH_TIM14              , /**< TIM14 Clock Enable */
#endif /* TIM14 */
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
    RCC_PERIPH_LPTIM1_PCLK3       , /**< Low Power Timer 1 clock enable with APB3 (PCLK3) as clock source. */
    RCC_PERIPH_LPTIM1_PLL2P       , /**< Low Power Timer 1 clock enable with PLL2 P output as clock source. */
    RCC_PERIPH_LPTIM1_PLL3R       , /**< Low Power Timer 1 clock enable with PLL3 R output as clock source. */
    RCC_PERIPH_LPTIM1_LSE         , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_PERIPH_LPTIM1_LSI         , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) as clock source. */
    RCC_PERIPH_LPTIM1_LPCLK       , /**< Low Power Timer 1 clock enable with Low-Power Clock Domain (PER) as clock source. */
#endif /* LPTIM1 */
#if defined(LPTIM2)
    RCC_PERIPH_LPTIM2_PCLK1       , /**< Low Power Timer 2 clock enable with APB1 (PCLK1) as clock source. */
    RCC_PERIPH_LPTIM2_PLL2P       , /**< Low Power Timer 2 clock enable with PLL2 P output as clock source. */
    RCC_PERIPH_LPTIM2_PLL3R       , /**< Low Power Timer 2 clock enable with PLL3 R output as clock source. */
    RCC_PERIPH_LPTIM2_LSE         , /**< Low Power Timer 2 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_PERIPH_LPTIM2_LSI         , /**< Low Power Timer 2 clock enable with Low Speed Internal (LSI) as clock source. */
    RCC_PERIPH_LPTIM2_LPCLK       , /**< Low Power Timer 2 clock enable with Low-Power Clock Domain (PER) as clock source. */
#endif /* LPTIM2 */
#if defined(LPTIM3)
    RCC_PERIPH_LPTIM3_PCLK3       , /**< Low Power Timer 3 clock enable with APB3 (PCLK3) as clock source. */
    RCC_PERIPH_LPTIM3_PLL2P       , /**< Low Power Timer 3 clock enable with PLL2 P output as clock source. */
    RCC_PERIPH_LPTIM3_PLL3R       , /**< Low Power Timer 3 clock enable with PLL3 R output as clock source. */
    RCC_PERIPH_LPTIM3_LSE         , /**< Low Power Timer 3 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_PERIPH_LPTIM3_LSI         , /**< Low Power Timer 3 clock enable with Low Speed Internal (LSI) as clock source. */
    RCC_PERIPH_LPTIM3_LPCLK       , /**< Low Power Timer 3 clock enable with Low-Power Clock Domain (PER) as clock source. */
#endif /* LPTIM3 */
#if defined(LPTIM4)
    RCC_PERIPH_LPTIM4_PCLK3       , /**< Low Power Timer 4 clock enable with APB3 (PCLK3) as clock source. */
    RCC_PERIPH_LPTIM4_PLL2P       , /**< Low Power Timer 4 clock enable with PLL2 P output as clock source. */
    RCC_PERIPH_LPTIM4_PLL3R       , /**< Low Power Timer 4 clock enable with PLL3 R output as clock source. */
    RCC_PERIPH_LPTIM4_LSE         , /**< Low Power Timer 4 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_PERIPH_LPTIM4_LSI         , /**< Low Power Timer 4 clock enable with Low Speed Internal (LSI) as clock source. */
    RCC_PERIPH_LPTIM4_LPCLK       , /**< Low Power Timer 4 clock enable with Low-Power Clock Domain (PER) as clock source. */
#endif /* LPTIM4 */
#if defined(LPTIM5)
    RCC_PERIPH_LPTIM5_PCLK3       , /**< Low Power Timer 5 clock enable with APB3 (PCLK3) as clock source. */
    RCC_PERIPH_LPTIM5_PLL2P       , /**< Low Power Timer 5 clock enable with PLL2 P output as clock source. */
    RCC_PERIPH_LPTIM5_PLL3R       , /**< Low Power Timer 5 clock enable with PLL3 R output as clock source. */
    RCC_PERIPH_LPTIM5_LSE         , /**< Low Power Timer 5 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_PERIPH_LPTIM5_LSI         , /**< Low Power Timer 5 clock enable with Low Speed Internal (LSI) as clock source. */
    RCC_PERIPH_LPTIM5_LPCLK       , /**< Low Power Timer 5 clock enable with Low-Power Clock Domain (PER) as clock source. */
#endif /* LPTIM5 */
#if defined(LPTIM6)
    RCC_PERIPH_LPTIM6_PCLK3       , /**< Low Power Timer 6 clock enable with APB3 (PCLK3) as clock source. */
    RCC_PERIPH_LPTIM6_PLL2P       , /**< Low Power Timer 6 clock enable with PLL2 P output as clock source. */
    RCC_PERIPH_LPTIM6_PLL3R       , /**< Low Power Timer 6 clock enable with PLL3 R output as clock source. */
    RCC_PERIPH_LPTIM6_LSE         , /**< Low Power Timer 6 clock enable with Low Speed External (LSE) oscillator as clock source. */
    RCC_PERIPH_LPTIM6_LSI         , /**< Low Power Timer 6 clock enable with Low Speed Internal (LSI) as clock source. */
    RCC_PERIPH_LPTIM6_LPCLK       , /**< Low Power Timer 6 clock enable with Low-Power Clock Domain (PER) as clock source. */
#endif /* LPTIM6 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
    RCC_PERIPH_SPI1_PLL1Q         , /**< SPI/I2S 1 clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_PERIPH_SPI1_PLL2P         , /**< SPI/I2S 1 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source */
    RCC_PERIPH_SPI1_PLL3P         , /**< SPI/I2S 1 clock enable with Phase Locked Loop 3 output P (PLL3P) as clock source */
    RCC_PERIPH_SPI1_PIN           , /**< SPI/I2S 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_SPI1_LPCLK         , /**< SPI/I2S 1 clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* SPI1 */
#if defined(SPI2)
    RCC_PERIPH_SPI2_PLL1Q         , /**< SPI/I2S 2 clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_PERIPH_SPI2_PLL2P         , /**< SPI/I2S 2 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source */
    RCC_PERIPH_SPI2_PLL3P         , /**< SPI/I2S 2 clock enable with Phase Locked Loop 3 output P (PLL3P) as clock source */
    RCC_PERIPH_SPI2_PIN           , /**< SPI/I2S 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_SPI2_LPCLK         , /**< SPI/I2S 2 clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* SPI2 */
#if defined(SPI3)
    RCC_PERIPH_SPI3_PLL1Q         , /**< SPI/I2S 3 clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_PERIPH_SPI3_PLL2P         , /**< SPI/I2S 3 clock enable with Phase Locked Loop 2 output P (PLL2P) as clock source */
    RCC_PERIPH_SPI3_PLL3P         , /**< SPI/I2S 3 clock enable with Phase Locked Loop 3 output P (PLL3P) as clock source */
    RCC_PERIPH_SPI3_PIN           , /**< SPI/I2S 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_PERIPH_SPI3_LPCLK         , /**< SPI/I2S 3 clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* SPI3 */
#if defined(SPI4)
    RCC_PERIPH_SPI4_PCLK2         , /**< SPI 4 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_SPI4_PLL2Q         , /**< SPI 4 clock enable with Phase Locked Loop 2 output Q (PLL1Q) as clock source */
    RCC_PERIPH_SPI4_PLL3Q         , /**< SPI 4 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_SPI4_HSI64         , /**< SPI 4 clock enable with 64MHz High Speed Internal oscillator (HSI64) as clock source */
    RCC_PERIPH_SPI4_CSI           , /**< SPI 4 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_PERIPH_SPI4_HSE           , /**< SPI 4 clock enable with High Speed External clock (HSE) as clock source */
#endif /* SPI4 */
#if defined(SPI5)
    RCC_PERIPH_SPI5_PCLK3         , /**< SPI 5 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_SPI5_PLL2Q         , /**< SPI 5 clock enable with Phase Locked Loop 2 output Q (PLL1Q) as clock source */
    RCC_PERIPH_SPI5_PLL3Q         , /**< SPI 5 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_SPI5_HSI64         , /**< SPI 5 clock enable with 64MHz High Speed Internal oscillator (HSI64) as clock source */
    RCC_PERIPH_SPI5_CSI           , /**< SPI 5 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_PERIPH_SPI5_HSE           , /**< SPI 5 clock enable with High Speed External clock (HSE) as clock source */
#endif /* SPI5 */
#if defined(SPI6)
    RCC_PERIPH_SPI6_PCLK2         , /**< SPI 6 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_SPI6_PLL2Q         , /**< SPI 6 clock enable with Phase Locked Loop 2 output Q (PLL1Q) as clock source */
    RCC_PERIPH_SPI6_PLL3Q         , /**< SPI 6 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_SPI6_HSI64         , /**< SPI 6 clock enable with 64MHz High Speed Internal oscillator (HSI64) as clock source */
    RCC_PERIPH_SPI6_CSI           , /**< SPI 6 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
    RCC_PERIPH_SPI6_HSE           , /**< SPI 6 clock enable with High Speed External clock (HSE) as clock source */
#endif /* SPI6 */


#if defined(I2C1)
    RCC_PERIPH_I2C1_PCLK1         , /**< I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C1_PLL3R         , /**< I2C 1 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_PERIPH_I2C1_HSI           , /**< I2C 1 clock enable with 64MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C1_CSI           , /**< I2C 1 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_PERIPH_I2C2_PCLK1         , /**< I2C 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I2C2_PLL3R         , /**< I2C 2 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_PERIPH_I2C2_HSI           , /**< I2C 2 clock enable with 64MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C2_CSI           , /**< I2C 2 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_PERIPH_I2C3_PCLK3         , /**< I2C 3 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_I2C3_PLL3R         , /**< I2C 3 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_PERIPH_I2C3_HSI           , /**< I2C 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C3_CSI           , /**< I2C 3 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_PERIPH_I2C4_PCLK3         , /**< I2C 4 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_I2C4_PLL3R         , /**< I2C 4 clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_PERIPH_I2C4_HSI           , /**< I2C 4 clock enable with 64MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_I2C4_CSI           , /**< I2C 4 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* I2C4 */

#if defined(I3C1)
    RCC_PERIPH_I3C1_PCLK1          , /**< I3C 1 Clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_I3C1_PLL3R         , /**< I3C 1 Clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
    RCC_PERIPH_I3C1_HSI           , /**< I3C 1 Clock enable with 64MHz High Speed Internal (HSI) oscillator output as clock source */
#endif /* I3C1 */

#if defined(USART1)
    RCC_PERIPH_USART1_PCLK2       , /**< USART 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART1_PLL2Q       , /**< USART 1 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_USART1_PLL3Q       , /**< USART 1 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USART1_HSI         , /**< USART 1 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART1_LSE         , /**< USART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_USART1_CSI         , /**< USART 1 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART1 */
#if defined(USART2)
    RCC_PERIPH_USART2_PCLK1       , /**< USART 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART2_PLL2Q       , /**< USART 2 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_USART2_PLL3Q       , /**< USART 2 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USART2_HSI         , /**< USART 2 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART2_LSE         , /**< USART 2 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_USART2_CSI         , /**< USART 2 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART2 */
#if defined(USART3)
    RCC_PERIPH_USART3_PCLK1       , /**< USART 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_PERIPH_USART3_PLL2Q       , /**< USART 3 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_USART3_PLL3Q       , /**< USART 3 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USART3_HSI         , /**< USART 3 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART3_LSE         , /**< USART 3 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_USART3_CSI         , /**< USART 3 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART3 */
#if defined(USART6)
    RCC_PERIPH_USART6_PCLK1       , /**< USART 6 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART6_PLL2Q       , /**< USART 6 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_USART6_PLL3Q       , /**< USART 6 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USART6_HSI         , /**< USART 6 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART6_LSE         , /**< USART 6 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_USART6_CSI         , /**< USART 6 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART6 */
#if defined(USART10)
    RCC_PERIPH_USART10_PCLK1      , /**< USART 10 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART10_PLL2Q      , /**< USART 10 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_USART10_PLL3Q      , /**< USART 10 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USART10_HSI        , /**< USART 10 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART10_LSE        , /**< USART 10 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_USART10_CSI        , /**< USART 10 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART10 */
#if defined(USART11)
    RCC_PERIPH_USART11_PCLK1      , /**< USART 11 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_USART11_PLL2Q      , /**< USART 11 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_USART11_PLL3Q      , /**< USART 11 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USART11_HSI        , /**< USART 11 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_USART11_LSE        , /**< USART 11 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_USART11_CSI        , /**< USART 11 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* USART11 */


#if defined(UART4)
    RCC_PERIPH_UART4_PCLK1        , /**< UART 4 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_UART4_PLL2Q        , /**< UART 4 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_UART4_PLL3Q        , /**< UART 4 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_UART4_HSI          , /**< UART 4 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART4_LSE          , /**< UART 4 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_UART4_CSI          , /**< UART 4 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART4 */
#if defined(UART5)
    RCC_PERIPH_UART5_PCLK1        , /**< UART 5 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_UART5_PLL2Q        , /**< UART 5 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_UART5_PLL3Q        , /**< UART 5 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_UART5_HSI          , /**< UART 5 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART5_LSE          , /**< UART 5 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_UART5_CSI          , /**< UART 5 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART5 */
#if defined(UART7)
    RCC_PERIPH_UART7_PCLK1        , /**< UART 7 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_UART7_PLL2Q        , /**< UART 7 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_UART7_PLL3Q        , /**< UART 7 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_UART7_HSI          , /**< UART 7 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART7_LSE          , /**< UART 7 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_UART7_CSI          , /**< UART 7 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART7 */
#if defined(UART8)
    RCC_PERIPH_UART8_PCLK1        , /**< UART 8 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_UART8_PLL2Q        , /**< UART 8 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_UART8_PLL3Q        , /**< UART 8 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_UART8_HSI          , /**< UART 8 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART8_LSE          , /**< UART 8 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_UART8_CSI          , /**< UART 8 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART8 */
#if defined(UART9)
    RCC_PERIPH_UART9_PCLK1        , /**< UART 9 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_UART9_PLL2Q        , /**< UART 9 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_UART9_PLL3Q        , /**< UART 9 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_UART9_HSI          , /**< UART 9 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART9_LSE          , /**< UART 9 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_UART9_CSI          , /**< UART 9 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART9 */
#if defined(UART12)
    RCC_PERIPH_UART12_PCLK1       , /**< UART 12 clock enable with APB2 (PCLK2) as clock source */
    RCC_PERIPH_UART12_PLL2Q       , /**< UART 12 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_UART12_PLL3Q       , /**< UART 12 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_UART12_HSI         , /**< UART 12 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_UART12_LSE         , /**< UART 12 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_UART12_CSI         , /**< UART 12 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* UART12 */


#if defined(LPUART1)
    RCC_PERIPH_LPUART1_PCLK3      , /**< Low-Power UART 1 clock enable with APB3 (PCLK3) as clock source */
    RCC_PERIPH_LPUART1_PLL2Q      , /**< Low-Power UART 1 clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_PERIPH_LPUART1_PLL3Q      , /**< Low-Power UART 1 clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_LPUART1_HSI        , /**< Low-Power UART 1 clock enable with High Speed Internal (HSI) oscillator output as clock source */
    RCC_PERIPH_LPUART1_LSE        , /**< Low-Power UART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_PERIPH_LPUART1_CSI        , /**< Low-Power UART 1 clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source */
#endif /* LPUART1 */

#if defined(FDCAN1)

    RCC_PERIPH_FDCAN_PLL1Q        , /**< FDCAN clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source */
    RCC_PERIPH_FDCAN_PLL2Q        , /**< FDCAN clock enable with Phase Locked Loop 2 output Q (PLL2Q) used as clock source */
    RCC_PERIPH_FDCAN_HSE          , /**< FDCAN clock enable with High Speed External oscillator (HSE) used as clock source */
#endif /* FDCAN1 */

#if defined(SDMMC1)
    RCC_PERIPH_SDMMC1_PLL1Q       , /**< SDMMC1 clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source for SDMMC 1 */
    RCC_PERIPH_SDMMC1_PLL2R       , /**< SDMMC1 clock enable with Phase Locked Loop 2 output R (PLL2R) used as clock source for SDMMC 1 */
#endif /* SDMMC1 */
#if defined(SDMMC2)
    RCC_PERIPH_SDMMC2_PLL1Q       , /**< SDMMC1 clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source for SDMMC 2 */
    RCC_PERIPH_SDMMC2_PLL2R       , /**< SDMMC1 clock enable with Phase Locked Loop 2 output R (PLL2R) used as clock source for SDMMC 2 */
#endif /* SDMMC2 */

#if defined(FMC_BANK1)
    RCC_PERIPH_FMC                , /**< Flexible Memory Controller (FMC) Clock Enable */
#endif /* FMC_BANK1 */

#if defined(OCTOSPI1)
    RCC_PERIPH_OCTOSPI1_HCLK      , /**< Octal SPI 1 (OCTOSPI) clock enable with HCLK clock as clock source for QSPI 1 */
    RCC_PERIPH_OCTOSPI1_PLL1Q     , /**< Octal SPI 1 (OCTOSPI) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source for QSPI 1 */
    RCC_PERIPH_OCTOSPI1_PLL2R     , /**< Octal SPI 1 (OCTOSPI) clock enable with Phase Locked Loop 2 output R (PLL2R) as clock source for QSPI 1 */
    RCC_PERIPH_OCTOSPI1_LPCLK     , /**< Octal SPI 1 (OCTOSPI) clock enable with Low-Power Clock Domain (PER) as clock source */
#endif /* OCTOSPI1 */


#if defined(USB_DRD_FS)
    RCC_PERIPH_USB_PLL1Q          , /**< USB Full Speed (12Mbps) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_PERIPH_USB_PLL3Q          , /**< USB Full Speed (12Mbps) clock enable with Phase Locked Loop 3 output Q (PLL3Q) as clock source */
    RCC_PERIPH_USB_HSI48          , /**< USB Full Speed (12Mbps) clock enable with 48MHz High Speed Internal (HSE48) as clock source */
#endif /* USB_OTG_HS */

#if defined(UCPD1)
    RCC_PERIPH_UCPD1              , /**< USB Type-C power delivery 1 (UCPD) Clock Enable */
#endif /* UCPD1 */

#if defined(ETH)
    RCC_PERIPH_ETH                , /**< Ethernet MAC clock enable */
    RCC_PERIPH_ETH_TX             , /**< Ethernet TX clock enable */
    RCC_PERIPH_ETH_RX             , /**< Ethernet RX clock enable */
#endif /* ETH */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(PSSI) || defined(DCMI)
    RCC_PERIPH_DCMI_PSSI          , /**< Digital Camera Media Interface (DCMI) and Parallel Synchronous Slave Interface (PSSI) Clock Enable */
#endif /* PSSI OR DCMI */

#if defined(SAI1)
    RCC_PERIPH_SAI1_PLL2P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_PERIPH_SAI1_PLL3P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_PERIPH_SAI1_PLL1Q         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 Q output as clock source */
    RCC_PERIPH_SAI1_CKIN          , /**< Serial Audio Interface 1 (SAI1) clock enable with clock input through pin as clock source */
    RCC_PERIPH_SAI1_LPCLK         , /**< Serial Audio Interface 1 (SAI1) clock enable with Low-Power Clock Domain (PER) as clock source*/
#endif /* SAI1 */
#if defined(SAI2)
    RCC_PERIPH_SAI2_PLL2P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_PERIPH_SAI2_PLL3P         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_PERIPH_SAI2_PLL1Q         , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 Q output as clock source */
    RCC_PERIPH_SAI2_CKIN          , /**< Serial Audio Interface 2 (SAI2) clock enable with clock input through pin as clock source */
    RCC_PERIPH_SAI2_LPCLK         , /**< Serial Audio Interface 2 (SAI2) clock enable with Low-Power Clock Domain (PER) as clock source*/
#endif /* SAI2 */

#if defined(CEC)
    RCC_PERIPH_CEC_LSE            , /**< Consumer Electronics Control (CEC) clock enable with Low Speed External oscillator (LSE) as clock source */
    RCC_PERIPH_CEC_CSI_DIV        , /**< Consumer Electronics Control (CEC) clock enable with 4MHz Low Power Internal oscillator (CSI) divided by 122 as clock source */
    RCC_PERIPH_CEC_LSI            , /**< Consumer Electronics Control (CEC) clock enable with Low Speed Internal oscillator (LSI) as clock source */
#endif /* CEC */

    /*-------------------------------- Analog --------------------------------*/

#if defined(VREFBUF)
    RCC_PERIPH_VREF               , /**< VREFBUF Clock Enable */
#endif /* VREFBUF */

#if defined(ADC1) || defined(ADC2)
    RCC_PERIPH_ADC_HCLK           , /**< ADC clock enable with HCLK as clock source for ADC's and DAC. */
    RCC_PERIPH_ADC_SYSCLK         , /**< ADC clock enable with SysClk as clock source for ADC's and DAC */
    RCC_PERIPH_ADC_PLL2R          , /**< ADC clock enable with PLL2 output R as clock source for ADC's and DAC */
    RCC_PERIPH_ADC_HSE            , /**< ADC clock enable with High Speed External oscillator (HSE) as clock source for ADC's and DAC */
    RCC_PERIPH_ADC_HSI            , /**< ADC clock enable with 64MHz High Speed Internal oscillator (HSI) as clock source for ADC's and DAC */
    RCC_PERIPH_ADC_CSI            , /**< ADC clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source for ADC's and DAC */
#endif /* ADC1 */

#if defined(DAC1)
    RCC_PERIPH_DAC_HCLK           , /**< DAC clock enable with HCLK as clock source for ADC's and DAC */
    RCC_PERIPH_DAC_SYSCLK         , /**< DAC clock enable with SysClk as clock source for ADC's and DAC */
    RCC_PERIPH_DAC_PLL2R          , /**< DAC clock enable with PLL2 output R as clock source for ADC's and DAC */
    RCC_PERIPH_DAC_HSE            , /**< DAC clock enable with High Speed External oscillator (HSE) as clock source for ADC's and DAC */
    RCC_PERIPH_DAC_HSI            , /**< DAC clock enable with 64MHz High Speed Internal oscillator (HSI) as clock source for ADC's and DAC */
    RCC_PERIPH_DAC_CSI            , /**< DAC clock enable with 4MHz Low Power Internal oscillator (CSI) as clock source for ADC's and DAC */

    RCC_PERIPH_DAC_SAH_LSE        , /**< DAC Sample & Hold clock enable with Low Speed External oscillator (LSE) as clock source for DAC */
    RCC_PERIPH_DAC_SAH_LSI        , /**< DAC Sample & Hold clock enable with Low Speed Internal oscillator (LSI) as clock source for DAC */
#endif /* DAC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(AES)
    RCC_PERIPH_AES                , /**< Advanced Encryption Standard (AES) HW accelerator Clock Enable */
#endif /* AES */

#if defined(GTZC_TZSC1)
    RCC_PERIPH_GTZC1              , /**< Global Trust Zone Controller 1 (GTZC) Clock Enable */
#endif /* GTZC1 */

#if defined(HASH)
    RCC_PERIPH_HASH               , /**< HASH Clock Enable */
#endif /* HASH */

#if defined(OTFDEC1)
    RCC_PERIPH_OTFDEC1            , /**< On-the-fly Decryption Engine 1 (OTFDEC) Clock Enable */
#endif /* OTFDEC1 */

#if defined(PKA)
    RCC_PERIPH_PKA                , /**< Public Key Accelerator (PKA) Clock Enable */
#endif /* PKA */

#if defined(RNG)
    RCC_PERIPH_RNG_HSI48          , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
    RCC_PERIPH_RNG_PLL1Q          , /**< Random Number Generator (RNG) clock enable with PLL 1 Q output used as clock source */
    RCC_PERIPH_RNG_LSE            , /**< Random Number Generator (RNG) clock enable with Low Speed External oscillator (LSE) used as clock source */
    RCC_PERIPH_RNG_LSI            , /**< Random Number Generator (RNG) clock enable with Low Speed Internal oscillator (LSI) used as clock source */
#endif /* RNG */

#if defined(SAES)
    RCC_PERIPH_SAES               , /**< Secure Advanced Encryption Standard (SAES) HW accelerator clock enable. */
#endif /* SAES */

    /*------------------------------- Computing ------------------------------*/

#if defined(CORDIC)
    RCC_PERIPH_CORDIC             , /**< CORDIC Clock Enable */
#endif /* CORDIC */

#if defined(CRC)
    RCC_PERIPH_CRC                , /**< CRC Clock Enable */
#endif /* CRC */

#if defined(FMAC)
    RCC_PERIPH_FMAC               , /**< Filter Math (FMAC) accelerator Clock Enable */
#endif /* FMAC */

    RCC_PERIPH_ID_CNT
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
    RCC_OSC_LSI,        /**< 32.768 kHz Low Speed Internal (LSI) oscillator. */
    RCC_OSC_CNT         /**< Count of available internal oscillator */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition. */
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
 *       PLL activated. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                               */
    RCC_PLL_SRC_CSI      , /**< PLL will be clocked by 4MHz CSI RC oscillator */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator         */
    RCC_PLL_SRC_HSI      , /**< PLL will be clocked by 64MHz HSI oscillator   */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                   */
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
typedef uint32_t rcc_PllNMult_t;


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
 * \brief Clock Output identification enumeration.
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

    RCC_CLK_SOURCE_MCO1_LSE   , /**< Low Speed External (LSE) will be used as Master Clock Output (MCO) clock source             */
    RCC_CLK_SOURCE_MCO1_HSE   , /**< High Speed External (HSE) will be used as Master Clock Output (MCO) clock source            */
    RCC_CLK_SOURCE_MCO1_HSI64 , /**< 64MHz High Speed Internal (HSI) will be used as Master Clock Output (MCO) clock source      */
    RCC_CLK_SOURCE_MCO1_HSI48 , /**< 48MHz High Speed Internal (HSI) will be used as Master Clock Output (MCO) clock source      */
    RCC_CLK_SOURCE_MCO1_PLL1Q , /**< Phase Locked Loop 1 output R (PLL1R) will be used as Master Clock Output (MCO) clock source */

    RCC_CLK_SOURCE_MCO2_LSI    , /**< Low Speed Internal (LSI) will be used as Master Clock Output 2 (MCO2) clock source                    */
    RCC_CLK_SOURCE_MCO2_HSE    , /**< High Speed External (HSE) will be used as Master Clock Output 2 (MCO2) clock source                   */
    RCC_CLK_SOURCE_MCO2_CSI    , /**< 4MHz Low power internal RC oscillator (CSI) will be used as Master Clock Output 2 (MCO2) clock source */
    RCC_CLK_SOURCE_MCO2_PLL1P  , /**< Phase Locked Loop 1 output P (PLL1P) will be used as Master Clock Output 2 (MCO2) clock source        */
    RCC_CLK_SOURCE_MCO2_PLL2P  , /**< Phase Locked Loop 2 output P (PLL2P) will be used as Master Clock Output 2 (MCO2) clock source        */
    RCC_CLK_SOURCE_MCO2_SYSCLK , /**< System clock (SYSCLK) will be used as Master Clock Output 2 (MCO2) clock source                       */

    RCC_CLK_SOURCE_LSCO_LSI    , /**< Low Speed Internal (LSI) will be used as Low Speed Clock Output (LSCO) clock source */
    RCC_CLK_SOURCE_LSCO_LSE    , /**< Low Speed External (LSE) will be used as Low Speed Clock Output (LSCO) clock source */
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

/** \brief List of all available clock buses */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u, /**< Advanced High-performance Bus 1   */
    RCC_CLK_BUS_AHB2,      /**< Advanced High-performance Bus 2   */
    RCC_CLK_BUS_AHB4,      /**< Advanced High-performance Bus 4   */
    RCC_CLK_BUS_APB1_1,    /**< Advanced Peripheral Bus 1 group 1 */
    RCC_CLK_BUS_APB1_2,    /**< Advanced Peripheral Bus 1 group 2 */
    RCC_CLK_BUS_APB2,      /**< Advanced Peripheral Bus 2         */
    RCC_CLK_BUS_APB3,      /**< Advanced Peripheral Bus 3         */
    RCC_CLK_BUS_CNT        /**< Count of available clock buses    */
}   rcc_ClkBusId_t;


/** \brief Clock bus divider value type definition.
 * Used for AHB, APB1, APB2 and APB3 clock bus dividers (all available clock
 * busses in \ref rcc_ClkBusId_t ) */
typedef uint32_t rcc_ClkBusDiv_t;


/** \brief System clock source multiplexer configuration list */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_HSI = 0u, /**< HSI will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_CSI     , /**< CSI will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_HSE     , /**< HSE will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_PLL     , /**< PLL will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_CNT       /**< Count of available system clock sources  */
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
    RCC_PWR_VOLTAGE_SCALE_0 = LL_PWR_REGU_VOLTAGE_SCALE0, /**< Core voltage 1.3V  - 1.4V  */
    RCC_PWR_VOLTAGE_SCALE_1 = LL_PWR_REGU_VOLTAGE_SCALE1, /**< Core voltage 1.15V - 1.26V */
    RCC_PWR_VOLTAGE_SCALE_2 = LL_PWR_REGU_VOLTAGE_SCALE2, /**< Core voltage 1.05V - 1.15V */
    RCC_PWR_VOLTAGE_SCALE_3 = LL_PWR_REGU_VOLTAGE_SCALE3, /**< Core voltage 0.95V - 1.05V */
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
    rcc_PllNMult_t    N_Multiplier;

    /** Output P prescaler - divider. Configuration function sets the prescaler
    *  during startup, but the output must be enabled later by user.
    *  If not enabled properly, the SAI peripheral switches to HSI16 automatically. */
    rcc_PllPDivider_t       P_Divider;

    /** Output Q prescaler - divider. Configuration function sets the prescaler
    *  during startup, but the output must be enabled later by user. */
    rcc_PllQDivider_t       Q_Divider;

    /** Output R prescaler - divider. Configuration function sets the prescaler
    *  during startup and the output is enabled at the same time, if the PLL
    *  is used as the system cock source. */
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

    /** Specified frequency of the HSE oscillator. Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Specifies clock source of the whole system */
    rcc_SystemClkSrc_t      SystemClockSource;

    /** Specifies clock source of the main PLL */
    rcc_PllConfigStruct_t   Pll_Config[ RCC_PLL_CNT ];

    /**
    * \brief Enables or disables Clock Security System (CSS).
    *
    * If the CSS is enabled and a failure of HSE is detected, NMI_ISR function will
    * be executed. The NMI_ISR function sets HSI as a system clock source and can
    * call additional user callback.
    *
    * \warning When HSE failure occurs and the CSS is disabled, MCU switches the
    *          system clock source automatically! This event cant't be detected
    *          by software.
    * \warning When using the CSS, an user defined callback should be added using
    *          RCC_SetNMI_Handler. Otherwise the HSE failure can't be detected
    *          by your code and you will not be notified.
    * \warning Once enabled, the CSS can't be turned off by software on L4 MCU
    *          family (just by reset).
    * \note If HSE is set as the clock entry of a PLL, the PLL will be disabled
    *       by the CSS in case of HSE failure.
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
    /** APB3 prescaler - divider */
    rcc_APB3_Div_t          APB3_Divider;

    /** Value of time in ms [0.001s] between SysTicks */
    rcc_Time_ms_t           SysTickInterval;

    /** Flash latency - number of wait states. Must be set correctly with respect
    *  to the desired clock frequency and voltage scaling.
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
