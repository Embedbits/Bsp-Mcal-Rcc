/*
 *    Mr.Nobody, COPYRIGHT (c) 2021
 *    ALL RIGHTS RESERVED
 *
 */

/**
 * \file Rcc.h
 * \ingroup Rcc
 * \brief Rcc module common functionality header file.
 *
 * This file contains the common functionality used internally by the module,
 * and shall provide interface between the module and the application.
 *
 */

#ifndef RCC_RCC_H
#define RCC_RCC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Rcc_Types.h"                      /* Module types definition        */
/* ============================= TYPEDEFS =================================== */


typedef enum
{
    /*------------------------------ System core -----------------------------*/

    RCC_CLK_MUX_SYSTICK_HCLK_DIV8 = 0u, /**< System Tick timer active with HCLK divided by 8 */
    RCC_CLK_MUX_SYSTICK_LSI           , /**< System Tick timer active with Low Speed Internal (LSI) Oscillator as clock source */
    RCC_CLK_MUX_SYSTICK_LSE           , /**< System Tick timer active with Low Speed External (LSE) Oscillator as clock source */

    RCC_CLK_MUX_RTC_HSE_DIV32         , /**< Real Time Clock active with High Speed External oscillator (HSE) divided by 32 used as clock source  */
    RCC_CLK_MUX_RTC_LSE               , /**< Real Time Clock active with Low Speed External (LSE) used as clock source */
    RCC_CLK_MUX_RTC_LSI               , /**< Real Time Clock active with Low Speed Internal (LSI) used as clock source */


    RCC_CLK_MUX_ICLK_PLL2Q            , /**< Intermediate Clock (ICLK) from PLL 2 Q output Clock Enable */
    RCC_CLK_MUX_ICLK_PLL1Q            , /**< Intermediate Clock (ICLK) from PLL 1 Q output Clock Enable */
    RCC_CLK_MUX_ICLK_MSIK             , /**< Intermediate Clock (ICLK) from Multi-Speed Internal Oscillator output K (MSIK) Clock Enable */
    RCC_CLK_MUX_ICLK_HSI48            , /**< Intermediate Clock (ICLK) from 48MHz High Speed Internal (HSI48) oscillator Clock Enable */

    /*-------------------------------- Timers --------------------------------*/

#if defined(LPTIM1)
    RCC_CLK_MUX_LPTIM1_MSIK          , /**< Low Power Timer 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source for LPTIM 1. */
    RCC_CLK_MUX_LPTIM1_LSI           , /**< Low Power Timer 1 clock enable with Low Speed Internal (LSI) as clock source for LPTIM 1. */
    RCC_CLK_MUX_LPTIM1_HSI16         , /**< Low Power Timer 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 1. */
    RCC_CLK_MUX_LPTIM1_LSE           , /**< Low Power Timer 1 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 1. */
#endif /* LPTIM1 */
#if defined(LPTIM2)
    RCC_CLK_MUX_LPTIM2_PCLK1         , /**< Low Power Timer 2 clock enable with APB1 (PCLK1) as clock source for LPTIM 2. */
    RCC_CLK_MUX_LPTIM2_LSI           , /**< Low Power Timer 2 clock enable with Low Speed Internal (LSI) as clock source for LPTIM 2. */
    RCC_CLK_MUX_LPTIM2_HSI16         , /**< Low Power Timer 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source for LPTIM 2. */
    RCC_CLK_MUX_LPTIM2_LSE           , /**< Low Power Timer 2 clock enable with Low Speed External (LSE) oscillator as clock source for LPTIM 2. */
#endif /* LPTIM2 */
#if defined(LPTIM3) || defined(LPTIM4)
    RCC_CLK_MUX_LPTIM34_MSIK         , /**< Low Power Timer 3&4 Multi-Speed Internal Oscillator output K (MSIK) clock source for LPTIM 3&4. */
    RCC_CLK_MUX_LPTIM34_LSI          , /**< Low Power Timer 3&4 Low Speed External (LSE) clock source for LPTIM 3&4. */
    RCC_CLK_MUX_LPTIM34_HSI16        , /**< Low Power Timer 3&4 16MHz High Speed Internal (HSI) oscillator clock source for LPTIM 3&4. */
    RCC_CLK_MUX_LPTIM34_LSE          , /**< Low Power Timer 3&4 Low Speed External (LSE) oscillator clock source for LPTIM 3&4. */
#endif /* LPTIM3 || LPTIM4 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
    RCC_CLK_MUX_SPI1_PCLK2           , /**< SPI 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_SPI1_SYSCLK          , /**< SPI 1 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_SPI1_HSI             , /**< SPI 1 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_SPI1_MSIK            , /**< SPI 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* SPI1 */
#if defined(SPI2)
    RCC_CLK_MUX_SPI2_PCLK1           , /**< SPI 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_SPI2_SYSCLK          , /**< SPI 2 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_SPI2_HSI             , /**< SPI 2 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_SPI2_MSIK            , /**< SPI 2 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* SPI2 */
#if defined(SPI3)
    RCC_CLK_MUX_SPI3_PCLK3           , /**< SPI 3 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_SPI3_SYSCLK          , /**< SPI 3 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_SPI3_HSI             , /**< SPI 3 clock enable with 16MHz High Speed Internal (HSI) oscillator as clock source */
    RCC_CLK_MUX_SPI3_MSIK            , /**< SPI 3 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* SPI3 */

#if defined(I2C1)
    RCC_CLK_MUX_I2C1_PCLK1           , /**< I2C 1 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C1_SYSCLK          , /**< I2C 1 clock enable with SysClk (AHB) as clock source*/
    RCC_CLK_MUX_I2C1_HSI             , /**< I2C 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C1_MSIK            , /**< I2C 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_CLK_MUX_I2C2_PCLK1           , /**< I2C 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C2_SYSCLK          , /**< I2C 2 clock enable with SysClk (AHB) as clock source*/
    RCC_CLK_MUX_I2C2_HSI             , /**< I2C 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C2_MSIK            , /**< I2C 2 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_CLK_MUX_I2C3_PCLK3           , /**< I2C 3 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_I2C3_SYSCLK          , /**< I2C 3 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2C3_HSI             , /**< I2C 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C3_MSIK            , /**, I2C 3 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_CLK_MUX_I2C4_PCLK1           , /**< I2C 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C4_SYSCLK          , /**< I2C 4 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2C4_HSI             , /**< I2C 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C4_MSIK            , /**, I2C 4 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C4 */
#if defined(I2C5)
    RCC_CLK_MUX_I2C5_PCLK1           , /**< I2C 5 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C5_SYSCLK          , /**< I2C 5 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2C5_HSI             , /**< I2C 5 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C5_MSIK            , /**< I2C 5 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C5 */
#if defined(I2C6)
    RCC_CLK_MUX_I2C6_PCLK1           , /**< I2C 6 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_I2C6_SYSCLK          , /**< I2C 6 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_I2C6_HSI             , /**< I2C 6 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_I2C6_MSIK            , /**< I2C 6 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* I2C6 */

#if defined(USART1)
    RCC_CLK_MUX_USART1_APB2          , /**< USART 1 clock enable with APB2 (PCLK2) as clock source */
    RCC_CLK_MUX_USART1_SYSCLK        , /**< USART 1 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART1_HSI           , /**< USART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART1_LSE           , /**< USART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART1 */
#if defined(USART2)
    RCC_CLK_MUX_USART2_APB1          , /**< USART 2 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART2_SYSCLK        , /**< USART 2 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART2_HSI           , /**< USART 2 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART2_LSE           , /**< USART 2 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART2 */
#if defined(USART3)
    RCC_CLK_MUX_USART3_APB1          , /**< USART 3 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART3_SYSCLK        , /**< USART 3 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART3_HSI           , /**< USART 3 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART3_LSE           , /**< USART 3 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART3 */
#if defined(USART6)
    RCC_CLK_MUX_USART6_APB1          , /**< USART 6 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART6_SYSCLK        , /**< USART 6 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_USART6_HSI           , /**< USART 6 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_USART6_LSE           , /**< USART 6 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART6 */

#if defined(UART4)
    RCC_CLK_MUX_UART4_PCLK1          , /**< UART 4 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART4_SYSCLK        , /**< UART 4 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_UART4_HSI            , /**< UART 4 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART4_LSE            , /**< UART 4 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* UART4 */
#if defined(UART5)
    RCC_CLK_MUX_UART5_PCLK1          , /**< UART 5 clock enable with APB1 (PCLK1) as clock source */
    RCC_CLK_MUX_USART5_SYSCLK        , /**< UART 5 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_UART5_HSI16          , /**< UART 5 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_UART5_LSE            , /**< UART 5 clock enable with Low Speed External (LSE) oscillator output as clock source */
#endif /* USART5 */

#if defined(LPUART1)
    RCC_CLK_MUX_LPUART1_PCLK3        , /**< Low-Power UART 1 clock enable with APB3 (PCLK3) as clock source */
    RCC_CLK_MUX_LPUART1_SYSCLK       , /**< Low-Power UART 1 clock enable with SysClk (AHB) as clock source */
    RCC_CLK_MUX_LPUART1_HSI16        , /**< Low-Power UART 1 clock enable with 16MHz High Speed Internal (HSI) oscillator output as clock source */
    RCC_CLK_MUX_LPUART1_LSE          , /**< Low-Power UART 1 clock enable with Low Speed External (LSE) oscillator output as clock source */
    RCC_CLK_MUX_LPUART1_MSIK         , /**< Low-Power UART 1 clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* LPUART1 */

#if defined(FDCAN1)
    RCC_CLK_MUX_FDCAN_PLL1Q          , /**< FDCAN clock enable with Phase Locked Loop 1 output Q (PLL1Q) used as clock source */
    RCC_CLK_MUX_FDCAN_PLL2P          , /**< FDCAN clock enable with Phase Locked Loop 2 output P (PLL2P) used as clock source */
    RCC_CLK_MUX_FDCAN_HSE            , /**< FDCAN clock enable with High Speed External oscillator (HSE) used as clock source */
#endif /* FDCAN1 */

#if defined(SDMMC1)
    RCC_CLK_MUX_SDMMC_ICLK           , /**< SDMMC Intermediate Clock (ICLK) bus used as clock source for SDMMC 1&2 */
    RCC_CLK_MUX_SDMMC_PLL1P          , /**< SDMMC Phase Locked Loop 1 output P (PLL1P) used as clock source for SDMMC 1&2 */
#endif /* SDMMC1 */


#if defined(OCTOSPI1) || defined(OCTOSPI2)
    RCC_CLK_MUX_OCTOSPI_SYSCLK       , /**< Octal SPI (OCTOSPI) system clock clock source for QSPI 1&2 */
    RCC_CLK_MUX_OCTOSPI_MSIK         , /**< Octal SPI (OCTOSPI) Multi-Speed Internal Oscillator output K (MSIK) clock source for QSPI 1&2 */
    RCC_CLK_MUX_OCTOSPI_PLL1Q        , /**< Octal SPI (OCTOSPI) Phase Locked Loop 1 output Q (PLL1Q) clock source for QSPI 1&2 */
    RCC_CLK_MUX_OCTOSPI_PLL2Q        , /**< Octal SPI (OCTOSPI) Phase Locked Loop 2 output Q (PLL2Q) clock source for QSPI 1&2 */
#endif /* OCTOSPI1 */

#if defined(HSPI1)
    RCC_CLK_MUX_HSPI1_SYSCLK         , /**< Hexa-deca SPI 1 (HSPI) clock enable with System Clock Enable as clock source */
    RCC_CLK_MUX_HSPI1_PLL1Q          , /**< Hexa-deca SPI 1 (HSPI) clock enable with Phase Locked Loop 1 output Q (PLL1Q) as clock source */
    RCC_CLK_MUX_HSPI1_PLL2Q          , /**< Hexa-deca SPI 1 (HSPI) clock enable with Phase Locked Loop 2 output Q (PLL2Q) as clock source */
    RCC_CLK_MUX_HSPI1_PLL3R          , /**< Hexa-deca SPI 1 (HSPI) clock enable with Phase Locked Loop 3 output R (PLL3R) as clock source */
#endif /* HSPI1 */

#if defined(USB_OTG_HS)
    RCC_CLK_MUX_USBPHY_HSE           , /**< USB High Speed (480Mbit) PHY clock enable with High Speed External (HSE) as clock source */
    RCC_CLK_MUX_USBPHY_HSE_DIV2      , /**< USB High Speed (480Mbit) PHY clock enable with High Speed External (HSE) divided by 2 as clock source */
    RCC_CLK_MUX_USBPHY_PLL1P         , /**< USB High Speed (480Mbit) PHY clock enable with Phase Locked Loop 1 output P (PLL1P) as clock source */
    RCC_CLK_MUX_USBPHY_PLL1P_DIV2    , /**< USB High Speed (480Mbit) PHY clock enable with Phase Locked Loop 1 output P (PLL1P) divided by 2 as clock source */
#endif /* USB_OTG_HS */

    /*------------------------------ Multimedia ------------------------------*/
#if defined(LTDC)
    RCC_CLK_MUX_LTDC_PLL2R           , /**< LCD-TFT Display COntroller (LTDC) PLL2 output R clock source */
    RCC_CLK_MUX_LTDC_PLL3R           , /**< LCD-TFT Display COntroller (LTDC) PLL3 output R clock source */
#endif /* LTDC */

#if defined(SAI1)
    RCC_CLK_MUX_SAI1_PLL2P           , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_CLK_MUX_SAI1_PLL3P           , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_CLK_MUX_SAI1_PLL1P           , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 P output as clock source */
    RCC_CLK_MUX_SAI1_CKIN            , /**< Serial Audio Interface 1 (SAI1) clock enable with clock input through pin as clock source*/
    RCC_CLK_MUX_SAI1_HSI16           , /**< Serial Audio Interface 1 (SAI1) clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source*/
#endif /* SAI1 */
#if defined(SAI2)
    RCC_CLK_MUX_SAI2_PLL2P           , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 2 P output as clock source */
    RCC_CLK_MUX_SAI2_PLL3P           , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 3 P output as clock source */
    RCC_CLK_MUX_SAI2_PLL1P           , /**< Serial Audio Interface 1 (SAI1) clock enable with PLL 1 P output as clock source */
    RCC_CLK_MUX_SAI2_CKIN            , /**< Serial Audio Interface 2 (SAI2) clock enable with clock input through pin as clock source*/
    RCC_CLK_MUX_SAI2_HSI16           , /**< Serial Audio Interface 2 (SAI2) clock enable with 16MHz High Speed Internal oscillator (HSI) as clock source*/
#endif /* SAI2 */

    /*-------------------------------- Analog --------------------------------*/

#if defined(ADC12_COMMON) || defined(ADC4) || defined(DAC1)
    RCC_CLK_MUX_ADC_DAC_HCLK         , /**< ADC's & DAC HCLK clock source for ADC 1&2&4 and DAC. */
    RCC_CLK_MUX_ADC_DAC_SYSCLK       , /**< ADC's & DAC SysClk clock source for ADC 1&2&4 and DAC */
    RCC_CLK_MUX_ADC_DAC_PLL2R        , /**< ADC's & DAC PLL2 output R clock source for ADC 1&2&4 and DAC */
    RCC_CLK_MUX_ADC_DAC_HSE          , /**< ADC's & DAC High Speed External oscillator (HSE) clock source for ADC 1&2&4 and DAC */
    RCC_CLK_MUX_ADC_DAC_HSI16        , /**< ADC's & DAC 16MHz High Speed Internal oscillator (HSI) clock source for ADC 1&2&4 and DAC */
    RCC_CLK_MUX_ADC_DAC_MSIK         , /**< ADC's & DAC Multi-Speed Internal Oscillator output K (MSIK) clock source for ADC 1&2&4 and DAC */
#endif /* ADC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(RNG)
    RCC_CLK_MUX_RNG_HSI48            , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) used as clock source */
    RCC_CLK_MUX_RNG_HSI48_DIV2       , /**< Random Number Generator (RNG) clock enable with 48MHz High Speed Internal oscillator (HSI48) divided by 2 used as clock source */
    RCC_CLK_MUX_RNG_HSI16            , /**< Random Number Generator (RNG) clock enable with 16MHz High Speed Internal oscillator (HSI16) used as clock source */
#endif /* RNG */

#if defined(SAES)
    RCC_CLK_MUX_SAES_SHSI            , /**< Secure Advanced Encryption Standard (SAES) HW accelerator clock enable with Secure High Speed Internal (SHSI) oscillator used as clock source */
    RCC_CLK_MUX_SAES_SHSI_DIV2       , /**< Secure Advanced Encryption Standard (SAES) HW accelerator clock enable with Secure High Speed Internal (SHSI) oscillator divided by 2 used as clock source */
#endif /* SAES */

#if defined(ADF1)
    RCC_CLK_MUX_ADF1_HCLK            , /**< Audio Digital Filter 1 (ADF) HCLK clock source */
    RCC_CLK_MUX_ADF1_PLL1P           , /**< Audio Digital Filter 1 (ADF) PLL1 output P clock source */
    RCC_CLK_MUX_ADF1_PLL3Q           , /**< Audio Digital Filter 1 (ADF) PLL3 output Q clock source */
    RCC_CLK_MUX_ADF1_CKIN            , /**< Audio Digital Filter 1 (ADF) clock input through pin clock source */
    RCC_CLK_MUX_ADF1_MSIK            , /**< Audio Digital Filter 1 (ADF) Multi-Speed Internal Oscillator output K (MSIK) clock source */
#endif /* ADF1 */

#if defined(MDF1)
    RCC_CLK_MUX_MDF1_HCLK            , /**< Multi-function DIgital Filter 1 (MDF) clock enable with HCLK as clock source */
    RCC_CLK_MUX_MDF1_PLL1P           , /**< Multi-function DIgital Filter 1 (MDF) clock enable with PLL1 output P as clock source */
    RCC_CLK_MUX_MDF1_PLL3Q           , /**< Multi-function DIgital Filter 1 (MDF) clock enable with PLL3 output Q as clock source */
    RCC_CLK_MUX_MDF1_CKIN            , /**< Multi-function DIgital Filter 1 (MDF) clock enable with clock input through pin as clock source */
    RCC_CLK_MUX_MDF1_MSIK            , /**< Multi-function DIgital Filter 1 (MDF) clock enable with Multi-Speed Internal Oscillator output K (MSIK) as clock source */
#endif /* MDF1 */

    RCC_CLK_MUX_LIST_CNT
}   rcc_ClkMuxId_t;

/**
 * \brief List of possible clock sources used to read clock frequency.
 *
 */
typedef enum
{
    RCC_CLK_SRC_SYSCLK = 0u, /**< System clock source                                                 */
    RCC_CLK_SRC_PLL1RCLK,    /**< PLL 1 R output clock source                                         */
    RCC_CLK_SRC_PLL1QCLK,    /**< PLL 1 Q output clock source                                         */
    RCC_CLK_SRC_PLL1PCLK,    /**< PLL 1 P output clock source                                         */
    RCC_CLK_SRC_PLL2RCLK,    /**< PLL 2 R output clock source                                         */
    RCC_CLK_SRC_PLL2QCLK,    /**< PLL 2 Q output clock source                                         */
    RCC_CLK_SRC_PLL2PCLK,    /**< PLL 2 P output clock source                                         */
    RCC_CLK_SRC_PLL3RCLK,    /**< PLL 3 R output clock source                                         */
    RCC_CLK_SRC_PLL3QCLK,    /**< PLL 3 Q output clock source                                         */
    RCC_CLK_SRC_PLL3PCLK,    /**< PLL 3 P output clock source                                         */
    RCC_CLK_SRC_AHBCLK,      /**< Advanced High-performance Bus clock source                          */
    RCC_CLK_SRC_APB1CLK,     /**< Advanced Peripheral Bus 1 (APB1) clock source                       */
    RCC_CLK_SRC_APB2CLK,     /**< Advanced Peripheral Bus 2 (APB2) clock source                       */
    RCC_CLK_SRC_APB3CLK,     /**< Advanced Peripheral Bus 3 (APB3) clock source                       */
    RCC_CLK_SRC_HSI16CLK,    /**< 64MHz High Speed Internal (HSI) clock source                        */
    RCC_CLK_SRC_HSI48CLK,    /**< 48MHz High Speed Internal (HSI) oscillator clock source             */
    RCC_CLK_SRC_SHSI48CLK,   /**< 48MHz High Speed Internal (HSI) oscillator clock source             */
    RCC_CLK_SRC_MSICLK_K,    /**< Intermediate Clock Internal (MSIS) oscillator output K clock source */
    RCC_CLK_SRC_MSICLK_S,    /**< Intermediate Clock Internal (MSIS) oscillator output S clock source */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock source                                    */
    RCC_CLK_SRC_LSICLK,      /**< 32kHz Low Speed Internal (HSI) oscillator clock source              */
    RCC_CLK_SRC_LSECLK,      /**< Low Speed External clock source                                     */
//    RCC_CLK_SRC_ICCLK,       /**< Independent clock source                                            */
    RCC_CLK_SRC_CNT          /**< Count of clock sources                                              */
}   rcc_ClkSrcId_t;


/**
 * \brief Reset and clock control peripheral BUS list
 */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u,/**< AHB1 Peripheral bus         */
    RCC_CLK_BUS_AHB2_1,   /**< AHB2 group 1 Peripheral bus */
    RCC_CLK_BUS_AHB2_2,   /**< AHB2 group 2 Peripheral bus */
    RCC_CLK_BUS_AHB3,     /**< AHB3 Peripheral bus         */
    RCC_CLK_BUS_APB1_1,   /**< APB2 group 1 Peripheral bus */
    RCC_CLK_BUS_APB1_2,   /**< APB2 group 2 Peripheral bus */
    RCC_CLK_BUS_APB2,     /**< APB2 Peripheral bus         */
    RCC_CLK_BUS_APB3,     /**< APB3 Peripheral bus         */
    RCC_CLK_BUS_CNT       /**< Count of clock buses        */
}   rcc_ClkBusId_t;


/** \brief List of RCC registers */
typedef enum
{
    RCC_REG_CR = 0u    , /**< RCC clock control register                                       */
    RCC_REG_ICSCR1     , /**< RCC internal clock sources calibration register 1                */
    RCC_REG_ICSCR2     , /**< RCC internal clock sources calibration register 2                */
    RCC_REG_ICSCR3     , /**< RCC internal clock sources calibration register 3                */
    RCC_REG_CRRCR      , /**< RCC Clock Recovery RC Register                                   */
    RCC_REG_CFGR1      , /**< RCC clock configuration register 1                               */
    RCC_REG_CFGR2      , /**< RCC clock configuration register 2                               */
    RCC_REG_CFGR3      , /**< RCC clock configuration register 3                               */
    RCC_REG_PLL1CFGR   , /**< PLL1 Configuration Register                                      */
    RCC_REG_PLL2CFGR   , /**< PLL2 Configuration Register                                      */
    RCC_REG_PLL3CFGR   , /**< PLL3 Configuration Register                                      */
    RCC_REG_PLL1DIVR   , /**< PLL1 Dividers Configuration Register                             */
    RCC_REG_PLL1FRACR  , /**< PLL1 Fractional Divider Configuration Register                   */
    RCC_REG_PLL2DIVR   , /**< PLL2 Dividers Configuration Register                             */
    RCC_REG_PLL2FRACR  , /**< PLL2 Fractional Divider Configuration Register                   */
    RCC_REG_PLL3DIVR   , /**< PLL3 Dividers Configuration Register                             */
    RCC_REG_PLL3FRACR  , /**< PLL3 Fractional Divider Configuration Register                   */
    RCC_REG_CIER       , /**< Clock Interrupt Enable Register                                  */
    RCC_REG_CIFR       , /**< Clock Interrupt Flag Register                                    */
    RCC_REG_CICR       , /**< Clock Interrupt Clear Register                                   */
    RCC_REG_AHB1RSTR   , /**< AHB1 Peripherals Reset Register                                  */
    RCC_REG_AHB2RSTR1  , /**< AHB2 Peripherals Reset Register 1                                */
    RCC_REG_AHB2RSTR2  , /**< AHB2 Peripherals Reset Register 2                                */
    RCC_REG_AHB3RSTR   , /**< AHB3 Peripherals Reset Register                                  */
    RCC_REG_APB1RSTR1  , /**< APB1 Peripherals Reset Register 1                                */
    RCC_REG_APB1RSTR2  , /**< APB1 Peripherals Reset Register 2                                */
    RCC_REG_APB2RSTR   , /**< APB2 Peripherals Reset Register                                  */
    RCC_REG_APB3RSTR   , /**< APB3 Peripherals Reset Register                                  */
    RCC_REG_AHB1ENR    , /**< AHB1 Peripherals Clock Enable Register                           */
    RCC_REG_AHB2ENR1   , /**< AHB2 Peripherals Clock Enable Register 1                         */
    RCC_REG_AHB2ENR2   , /**< AHB2 Peripherals Clock Enable Register 2                         */
    RCC_REG_AHB3ENR    , /**< AHB3 Peripherals Clock Enable Register                           */
    RCC_REG_APB1ENR1   , /**< APB1 Peripherals Clock Enable Register 1                         */
    RCC_REG_APB1ENR2   , /**< APB1 Peripherals Clock Enable Register 2                         */
    RCC_REG_APB2ENR    , /**< APB2 Peripherals Clock Enable Register                           */
    RCC_REG_APB3ENR    , /**< APB3 Peripherals Clock Enable Register                           */
    RCC_REG_AHB1SMENR  , /**< AHB1 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    RCC_REG_AHB2SMENR1 , /**< AHB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    RCC_REG_AHB2SMENR2 , /**< AHB2 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    RCC_REG_AHB3SMENR  , /**< AHB3 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    RCC_REG_APB1SMENR1 , /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    RCC_REG_APB1SMENR2 , /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    RCC_REG_APB2SMENR  , /**< APB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    RCC_REG_APB3SMENR  , /**< APB3 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    RCC_REG_SRDAMR     , /**< SRD Autonomous Mode Register                                     */
    RCC_REG_CCIPR1     , /**< IPs Clocks Configuration Register 1                              */
    RCC_REG_CCIPR2     , /**< IPs Clocks Configuration Register 2                              */
    RCC_REG_CCIPR3     , /**< IPs Clocks Configuration Register 3                              */
    RCC_REG_BDCR       , /**< Backup Domain Control Register                                   */
    RCC_REG_CSR        , /**< V33 Clock Control & Status Register                              */
    RCC_REG_SECCFGR    , /**< RCC secure configuration register                                */
    RCC_REG_PRIVCFGR   , /**< RCC privilege configuration register                             */
    RCC_REG_FLASH_ACR  , /**< Flash configuration register                                     */
    RCC_REG_CNT          /**< Count of available RCC registers                                 */
}   rcc_RegId_t;


/** \brief Structure type used to store physical address of RCC registers.
 * This array is used to reduce size of configuration array. With this style
 * the registers are referenced through enumeration.
 *
 */
typedef struct
{
    rcc_RegId_t         RegId;   /**< Peripheral register ID      */
    volatile uint32_t * RegAddr; /**< Peripheral register address */
}   rcc_RegList_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/** \brief RCC registers configuration structure
 *
 * This structure is used to store addresses of RCC registers.
 * It is used to reduce code size while referring to RCC registers.
 */
const rcc_RegList_t                     rcc_RegList[ RCC_REG_CNT ] =
{
    { .RegId = RCC_REG_CR         , .RegAddr = &(RCC->CR        ) }, /**< RCC clock control register                                       */
    { .RegId = RCC_REG_ICSCR1     , .RegAddr = &(RCC->ICSCR1    ) }, /**< RCC internal clock sources calibration register 1                */
    { .RegId = RCC_REG_ICSCR2     , .RegAddr = &(RCC->ICSCR2    ) }, /**< RCC internal clock sources calibration register 2                */
    { .RegId = RCC_REG_ICSCR3     , .RegAddr = &(RCC->ICSCR3    ) }, /**< RCC internal clock sources calibration register 3                */
    { .RegId = RCC_REG_CRRCR      , .RegAddr = &(RCC->CRRCR     ) }, /**< RCC Clock Recovery RC Register                                   */
    { .RegId = RCC_REG_CFGR1      , .RegAddr = &(RCC->CFGR1     ) }, /**< RCC clock configuration register 1                               */
    { .RegId = RCC_REG_CFGR2      , .RegAddr = &(RCC->CFGR2     ) }, /**< RCC clock configuration register 2                               */
    { .RegId = RCC_REG_CFGR3      , .RegAddr = &(RCC->CFGR3     ) }, /**< RCC clock configuration register 3                               */
    { .RegId = RCC_REG_PLL1CFGR   , .RegAddr = &(RCC->PLL1CFGR  ) }, /**< PLL1 Configuration Register                                      */
    { .RegId = RCC_REG_PLL2CFGR   , .RegAddr = &(RCC->PLL2CFGR  ) }, /**< PLL2 Configuration Register                                      */
    { .RegId = RCC_REG_PLL3CFGR   , .RegAddr = &(RCC->PLL3CFGR  ) }, /**< PLL3 Configuration Register                                      */
    { .RegId = RCC_REG_PLL1DIVR   , .RegAddr = &(RCC->PLL1DIVR  ) }, /**< PLL1 Dividers Configuration Register                             */
    { .RegId = RCC_REG_PLL1FRACR  , .RegAddr = &(RCC->PLL1FRACR ) }, /**< PLL1 Fractional Divider Configuration Register                   */
    { .RegId = RCC_REG_PLL2DIVR   , .RegAddr = &(RCC->PLL2DIVR  ) }, /**< PLL2 Dividers Configuration Register                             */
    { .RegId = RCC_REG_PLL2FRACR  , .RegAddr = &(RCC->PLL2FRACR ) }, /**< PLL2 Fractional Divider Configuration Register                   */
    { .RegId = RCC_REG_PLL3DIVR   , .RegAddr = &(RCC->PLL3DIVR  ) }, /**< PLL3 Dividers Configuration Register                             */
    { .RegId = RCC_REG_PLL3FRACR  , .RegAddr = &(RCC->PLL3FRACR ) }, /**< PLL3 Fractional Divider Configuration Register                   */
    { .RegId = RCC_REG_CIER       , .RegAddr = &(RCC->CIER      ) }, /**< Clock Interrupt Enable Register                                  */
    { .RegId = RCC_REG_CIFR       , .RegAddr = &(RCC->CIFR      ) }, /**< Clock Interrupt Flag Register                                    */
    { .RegId = RCC_REG_CICR       , .RegAddr = &(RCC->CICR      ) }, /**< Clock Interrupt Clear Register                                   */
    { .RegId = RCC_REG_AHB1RSTR   , .RegAddr = &(RCC->AHB1RSTR  ) }, /**< AHB1 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_AHB2RSTR1  , .RegAddr = &(RCC->AHB2RSTR1 ) }, /**< AHB2 Peripherals Reset Register 1                                */
    { .RegId = RCC_REG_AHB2RSTR2  , .RegAddr = &(RCC->AHB2RSTR2 ) }, /**< AHB2 Peripherals Reset Register 2                                */
    { .RegId = RCC_REG_AHB3RSTR   , .RegAddr = &(RCC->AHB3RSTR  ) }, /**< AHB3 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_APB1RSTR1  , .RegAddr = &(RCC->APB1RSTR1 ) }, /**< APB1 Peripherals Reset Register 1                                */
    { .RegId = RCC_REG_APB1RSTR2  , .RegAddr = &(RCC->APB1RSTR2 ) }, /**< APB1 Peripherals Reset Register 2                                */
    { .RegId = RCC_REG_APB2RSTR   , .RegAddr = &(RCC->APB2RSTR  ) }, /**< APB2 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_APB3RSTR   , .RegAddr = &(RCC->APB3RSTR  ) }, /**< APB3 Peripherals Reset Register                                  */
    { .RegId = RCC_REG_AHB1ENR    , .RegAddr = &(RCC->AHB1ENR   ) }, /**< AHB1 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_AHB2ENR1   , .RegAddr = &(RCC->AHB2ENR1  ) }, /**< AHB2 Peripherals Clock Enable Register 1                         */
    { .RegId = RCC_REG_AHB2ENR2   , .RegAddr = &(RCC->AHB2ENR2  ) }, /**< AHB2 Peripherals Clock Enable Register 2                         */
    { .RegId = RCC_REG_AHB3ENR    , .RegAddr = &(RCC->AHB3ENR   ) }, /**< AHB3 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_APB1ENR1   , .RegAddr = &(RCC->APB1ENR1  ) }, /**< APB1 Peripherals Clock Enable Register 1                         */
    { .RegId = RCC_REG_APB1ENR2   , .RegAddr = &(RCC->APB1ENR2  ) }, /**< APB1 Peripherals Clock Enable Register 2                         */
    { .RegId = RCC_REG_APB2ENR    , .RegAddr = &(RCC->APB2ENR   ) }, /**< APB2 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_APB3ENR    , .RegAddr = &(RCC->APB3ENR   ) }, /**< APB3 Peripherals Clock Enable Register                           */
    { .RegId = RCC_REG_AHB1SMENR  , .RegAddr = &(RCC->AHB1SMENR ) }, /**< AHB1 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    { .RegId = RCC_REG_AHB2SMENR1 , .RegAddr = &(RCC->AHB2SMENR1) }, /**< AHB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    { .RegId = RCC_REG_AHB2SMENR2 , .RegAddr = &(RCC->AHB2SMENR2) }, /**< AHB2 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    { .RegId = RCC_REG_AHB3SMENR  , .RegAddr = &(RCC->AHB3SMENR ) }, /**< AHB3 Peripherals Clock Enable in Sleep and Stop Modes Register   */
    { .RegId = RCC_REG_APB1SMENR1 , .RegAddr = &(RCC->APB1SMENR1) }, /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    { .RegId = RCC_REG_APB1SMENR2 , .RegAddr = &(RCC->APB1SMENR2) }, /**< APB1 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    { .RegId = RCC_REG_APB2SMENR  , .RegAddr = &(RCC->APB2SMENR ) }, /**< APB2 Peripherals Clock Enable in Sleep and Stop Modes Register 1 */
    { .RegId = RCC_REG_APB3SMENR  , .RegAddr = &(RCC->APB3SMENR ) }, /**< APB3 Peripherals Clock Enable in Sleep and Stop Modes Register 2 */
    { .RegId = RCC_REG_SRDAMR     , .RegAddr = &(RCC->SRDAMR    ) }, /**< SRD Autonomous Mode Register                                     */
    { .RegId = RCC_REG_CCIPR1     , .RegAddr = &(RCC->CCIPR1    ) }, /**< IPs Clocks Configuration Register 1                              */
    { .RegId = RCC_REG_CCIPR2     , .RegAddr = &(RCC->CCIPR2    ) }, /**< IPs Clocks Configuration Register 2                              */
    { .RegId = RCC_REG_CCIPR3     , .RegAddr = &(RCC->CCIPR3    ) }, /**< IPs Clocks Configuration Register 3                              */
    { .RegId = RCC_REG_BDCR       , .RegAddr = &(RCC->BDCR      ) }, /**< Backup Domain Control Register                                   */
    { .RegId = RCC_REG_CSR        , .RegAddr = &(RCC->CSR       ) }, /**< V33 Clock Control & Status Register                              */
    { .RegId = RCC_REG_SECCFGR    , .RegAddr = &(RCC->SECCFGR   ) }, /**< RCC secure configuration register                                */
    { .RegId = RCC_REG_PRIVCFGR   , .RegAddr = &(RCC->PRIVCFGR  ) }, /**< RCC privilege configuration register                             */
    { .RegId = RCC_REG_FLASH_ACR  , .RegAddr = &(FLASH->ACR     ) }  /**< Flash configuration register                                     */

};
/* ======================== EXPORTED FUNCTIONS ============================== */

/**
 * \brief Sets the register bit with given mask.
 *
 * \param regId   [in]: RCC register ID
 * \param bitMask [in]: Mask to apply to the register
 */
static inline void Rcc_Set_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        *rcc_RegList[regId].RegAddr |= bitMask;
    }
}


/**
 * \brief Resets the register bit with given mask.
 *
 * \param regId   [in]: RCC register ID
 * \param bitMask [in]: Mask to apply to the register
 */
static inline void Rcc_Reset_RegBit( rcc_RegId_t regId, uint32_t bitMask )
{
    if( RCC_REG_CNT > regId )
    {
        *rcc_RegList[regId].RegAddr &= ~bitMask;
    }
}


/**
 * \brief Gets the register bit value with given mask.
 *
 * \param regId   [in]: RCC register ID
 * \param bitMask [in]: Mask to apply to the register
 * \return Register bit value with applied mask
 */
static inline uint32_t Rcc_Get_RegBit(rcc_RegId_t regId, uint32_t bitMask)
{
    if( RCC_REG_CNT > regId )
    {
        return ( *rcc_RegList[regId].RegAddr & bitMask );
    }
    else
    {
        return 0u;
    }
}


/**
 * \brief Updates the register value with given mask and value.
 *
 * \param regId   [in]: RCC register ID
 * \param regMask [in]: Mask to apply to the register
 * \param regValue Value to set in the register
 */
static inline void Rcc_Set_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t regValue )
{
    if( RCC_REG_CNT > regId )
    {
        /* Set the register value with mask */
        *rcc_RegList[regId].RegAddr = ( ( *rcc_RegList[regId].RegAddr & ~regMask ) |
                                        ( regValue & regMask ) );
    }
}


/**
 * \brief Gets the register value with given mask.
 *
 * \param regId   [in]: RCC register ID.
 * \param regMask [in]: Mask to apply to the register
 * \return Register value with applied mask
 */
static inline uint32_t Rcc_Get_RegVal( rcc_RegId_t regId, uint32_t regMask )
{
    if( RCC_REG_CNT > regId )
    {
        /* Get the register value with mask */
        return ( *rcc_RegList[regId].RegAddr & regMask );
    }
    else
    {
        return 0u;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_H */
