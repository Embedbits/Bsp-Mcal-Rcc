/**
 * \author Mr.Nobody
 * \file Rcc_ClkMux.h
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Peripheral's Clock Multiplexer (ClkMux)
 *        component functionality header file.
 *
 * This file contains the interface functions and types used by the RCC
 * module Clock Multiplexer (ClkMux) component.
 * List of available peripherals with configurable clock sources (STM32H7):
 * - RTC, peripheral clock per_ck (CKPER)
 * - LPTIM (LPTIM1, LPTIM2, LPTIM3 / LPTIM4 / LPTIM5)
 * - SPI (SPI1 / SPI2 / SPI3, SPI4 / SPI5, SPI6)
 * - I2C (I2C1 / I2C2 / I2C3 / I2C5, I2C4)
 * - USART (USART1 / USART6 / USART10 / UART9, USART2 / USART3 / UART4 / UART5 / UART7 / UART8)
 * - LPUART1, FDCAN, SDMMC, FMC, QUADSPI / OCTOSPI, SWPMI, HDMI-CEC, SPDIFRX
 * - SAI (SAI1, SAI2 / SAI3 or SAI2 A / B, SAI4 A / B), DFSDM1, DFSDM2, DSI
 * - USB OTG, RNG, ADC
 * STM32H7R / H7S (CCIPR1 - CCIPR4):
 * - RTC, peripheral clock per_ck (CKPER)
 * - LPTIM (LPTIM1, LPTIM2 / LPTIM3, LPTIM4 / LPTIM5), SPI (SPI1, SPI2 / SPI3,
 *   SPI4 / SPI5, SPI6), I2C (I2C1 / I3C1, I2C2 / I2C3)
 * - USART (USART1, USART2 / USART3 / UART4 / UART5 / UART7 / UART8), LPUART1
 * - FDCAN, SDMMC, FMC, XSPI1, XSPI2, HDMI-CEC, SPDIFRX, SAI1, SAI2, ADF1, PSSI
 * - USB OTG FS, USB HS PHY reference clock, ADC
 *
 * Multiplexers marked with "/" are shared by several peripherals.
 */

#ifndef RCC_CLKMUX_RCC_CLKMUX_H
#define RCC_CLKMUX_RCC_CLKMUX_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================= INCLUDES =================================== */
#include "Rcc_Types.h"                      /* Module types definition        */
/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ============================= TYPEDEFS =================================== */

/** \brief List of peripheral clock multiplexer inputs (one entry for every
 *  selectable clock source of every multiplexer, the default source of a
 *  multiplexer is its first entry). Multiplexers not available on the selected
 *  device are not defined. */
typedef enum
{
    /*------------------------------ System core -----------------------------*/
    RCC_CLK_MUX_RTC_NONE          = 0u, /**< Real Time Clock without clock source (default)                */
    RCC_CLK_MUX_RTC_HSE_DIV           , /**< Real Time Clock clocked by HSE divided by RTC prescaler       */
    RCC_CLK_MUX_RTC_LSE               , /**< Real Time Clock clocked by Low Speed External (LSE)           */
    RCC_CLK_MUX_RTC_LSI               , /**< Real Time Clock clocked by Low Speed Internal (LSI)           */
#if defined(LL_RCC_USART16910_CLKSOURCE_PCLK2)
    RCC_CLK_MUX_USART16910_PCLK2      , /**< USART16910 kernel clock source PCLK2 */
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_PLL2Q)
    RCC_CLK_MUX_USART16910_PLL2Q      , /**< USART16910 kernel clock source PLL2Q */
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_PLL3Q)
    RCC_CLK_MUX_USART16910_PLL3Q      , /**< USART16910 kernel clock source PLL3Q */
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_HSI)
    RCC_CLK_MUX_USART16910_HSI        , /**< USART16910 kernel clock source HSI */
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_CSI)
    RCC_CLK_MUX_USART16910_CSI        , /**< USART16910 kernel clock source CSI */
#endif
#if defined(LL_RCC_USART16910_CLKSOURCE_LSE)
    RCC_CLK_MUX_USART16910_LSE        , /**< USART16910 kernel clock source LSE */
#endif
    RCC_CLK_MUX_USART234578_PCLK1     , /**< USART234578 kernel clock source PCLK1 */
    RCC_CLK_MUX_USART234578_PLL2Q     , /**< USART234578 kernel clock source PLL2Q */
    RCC_CLK_MUX_USART234578_PLL3Q     , /**< USART234578 kernel clock source PLL3Q */
    RCC_CLK_MUX_USART234578_HSI       , /**< USART234578 kernel clock source HSI */
    RCC_CLK_MUX_USART234578_CSI       , /**< USART234578 kernel clock source CSI */
    RCC_CLK_MUX_USART234578_LSE       , /**< USART234578 kernel clock source LSE */
    RCC_CLK_MUX_LPUART1_PCLK4         , /**< LPUART1 kernel clock source PCLK4 */
    RCC_CLK_MUX_LPUART1_PLL2Q         , /**< LPUART1 kernel clock source PLL2Q */
    RCC_CLK_MUX_LPUART1_PLL3Q         , /**< LPUART1 kernel clock source PLL3Q */
    RCC_CLK_MUX_LPUART1_HSI           , /**< LPUART1 kernel clock source HSI */
    RCC_CLK_MUX_LPUART1_CSI           , /**< LPUART1 kernel clock source CSI */
    RCC_CLK_MUX_LPUART1_LSE           , /**< LPUART1 kernel clock source LSE */
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SPI123_PLL1Q          , /**< SPI123 kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SPI123_PLL2P          , /**< SPI123 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SPI123_PLL3P          , /**< SPI123 kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SPI123_PIN            , /**< SPI123 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SPI123_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SPI123_LPCLK          , /**< SPI123 kernel clock source CLKP */
#endif
    RCC_CLK_MUX_SPI45_PCLK2           , /**< SPI45 kernel clock source PCLK2 */
    RCC_CLK_MUX_SPI45_PLL2Q           , /**< SPI45 kernel clock source PLL2Q */
    RCC_CLK_MUX_SPI45_PLL3Q           , /**< SPI45 kernel clock source PLL3Q */
    RCC_CLK_MUX_SPI45_HSI             , /**< SPI45 kernel clock source HSI */
    RCC_CLK_MUX_SPI45_CSI             , /**< SPI45 kernel clock source CSI */
    RCC_CLK_MUX_SPI45_HSE             , /**< SPI45 kernel clock source HSE */
    RCC_CLK_MUX_SPI6_PCLK4            , /**< SPI6 kernel clock source PCLK4 */
    RCC_CLK_MUX_SPI6_PLL2Q            , /**< SPI6 kernel clock source PLL2Q */
    RCC_CLK_MUX_SPI6_PLL3Q            , /**< SPI6 kernel clock source PLL3Q */
    RCC_CLK_MUX_SPI6_HSI              , /**< SPI6 kernel clock source HSI */
    RCC_CLK_MUX_SPI6_CSI              , /**< SPI6 kernel clock source CSI */
    RCC_CLK_MUX_SPI6_HSE              , /**< SPI6 kernel clock source HSE */
#if defined(LL_RCC_SPI6_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SPI6_PIN              , /**< SPI6 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PCLK1)
    RCC_CLK_MUX_I2C123_PCLK1          , /**< I2C123 kernel clock source PCLK1 */
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_I2C123_PLL3R          , /**< I2C123 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_HSI)
    RCC_CLK_MUX_I2C123_HSI            , /**< I2C123 kernel clock source HSI */
#endif
#if defined(LL_RCC_I2C123_CLKSOURCE_CSI)
    RCC_CLK_MUX_I2C123_CSI            , /**< I2C123 kernel clock source CSI */
#endif
#if defined(LL_RCC_I2C4_CLKSOURCE_PCLK4)
    RCC_CLK_MUX_I2C4_PCLK4            , /**< I2C4 kernel clock source PCLK4 */
#endif
#if defined(LL_RCC_I2C4_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_I2C4_PLL3R            , /**< I2C4 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_I2C4_CLKSOURCE_HSI)
    RCC_CLK_MUX_I2C4_HSI              , /**< I2C4 kernel clock source HSI */
#endif
#if defined(LL_RCC_I2C4_CLKSOURCE_CSI)
    RCC_CLK_MUX_I2C4_CSI              , /**< I2C4 kernel clock source CSI */
#endif
    RCC_CLK_MUX_LPTIM1_PCLK1          , /**< LPTIM1 kernel clock source PCLK1 */
    RCC_CLK_MUX_LPTIM1_PLL2P          , /**< LPTIM1 kernel clock source PLL2P */
    RCC_CLK_MUX_LPTIM1_PLL3R          , /**< LPTIM1 kernel clock source PLL3R */
    RCC_CLK_MUX_LPTIM1_LSE            , /**< LPTIM1 kernel clock source LSE */
    RCC_CLK_MUX_LPTIM1_LSI            , /**< LPTIM1 kernel clock source LSI */
    RCC_CLK_MUX_LPTIM1_LPCLK          , /**< LPTIM1 kernel clock source CLKP */
#if defined(LL_RCC_LPTIM2_CLKSOURCE_PCLK4)
    RCC_CLK_MUX_LPTIM2_PCLK4          , /**< LPTIM2 kernel clock source PCLK4 */
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_LPTIM2_PLL2P          , /**< LPTIM2 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_LPTIM2_PLL3R          , /**< LPTIM2 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_LSE)
    RCC_CLK_MUX_LPTIM2_LSE            , /**< LPTIM2 kernel clock source LSE */
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_LSI)
    RCC_CLK_MUX_LPTIM2_LSI            , /**< LPTIM2 kernel clock source LSI */
#endif
#if defined(LL_RCC_LPTIM2_CLKSOURCE_CLKP)
    RCC_CLK_MUX_LPTIM2_LPCLK          , /**< LPTIM2 kernel clock source CLKP */
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PCLK4)
    RCC_CLK_MUX_LPTIM345_PCLK4        , /**< LPTIM345 kernel clock source PCLK4 */
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_LPTIM345_PLL2P        , /**< LPTIM345 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_LPTIM345_PLL3R        , /**< LPTIM345 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSE)
    RCC_CLK_MUX_LPTIM345_LSE          , /**< LPTIM345 kernel clock source LSE */
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_LSI)
    RCC_CLK_MUX_LPTIM345_LSI          , /**< LPTIM345 kernel clock source LSI */
#endif
#if defined(LL_RCC_LPTIM345_CLKSOURCE_CLKP)
    RCC_CLK_MUX_LPTIM345_LPCLK        , /**< LPTIM345 kernel clock source CLKP */
#endif
    RCC_CLK_MUX_SAI1_PLL1Q            , /**< SAI1 kernel clock source PLL1Q */
    RCC_CLK_MUX_SAI1_PLL2P            , /**< SAI1 kernel clock source PLL2P */
    RCC_CLK_MUX_SAI1_PLL3P            , /**< SAI1 kernel clock source PLL3P */
    RCC_CLK_MUX_SAI1_PIN              , /**< SAI1 kernel clock source I2S_CKIN */
    RCC_CLK_MUX_SAI1_LPCLK            , /**< SAI1 kernel clock source CLKP */
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SAI23_PLL1Q           , /**< SAI23 kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SAI23_PLL2P           , /**< SAI23 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SAI23_PLL3P           , /**< SAI23 kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SAI23_PIN             , /**< SAI23 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SAI23_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SAI23_LPCLK           , /**< SAI23 kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SAI2A_PLL1Q           , /**< SAI2A kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SAI2A_PLL2P           , /**< SAI2A kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SAI2A_PLL3P           , /**< SAI2A kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SAI2A_PIN             , /**< SAI2A kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SAI2A_LPCLK           , /**< SAI2A kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI2A_CLKSOURCE_SPDIF)
    RCC_CLK_MUX_SAI2A_SPDIF           , /**< SAI2A kernel clock source SPDIF */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SAI2B_PLL1Q           , /**< SAI2B kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SAI2B_PLL2P           , /**< SAI2B kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SAI2B_PLL3P           , /**< SAI2B kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SAI2B_PIN             , /**< SAI2B kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SAI2B_LPCLK           , /**< SAI2B kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI2B_CLKSOURCE_SPDIF)
    RCC_CLK_MUX_SAI2B_SPDIF           , /**< SAI2B kernel clock source SPDIF */
#endif
#if defined(LL_RCC_SAI4A_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SAI4A_PLL1Q           , /**< SAI4A kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SAI4A_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SAI4A_PLL2P           , /**< SAI4A kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SAI4A_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SAI4A_PLL3P           , /**< SAI4A kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SAI4A_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SAI4A_PIN             , /**< SAI4A kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SAI4A_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SAI4A_LPCLK           , /**< SAI4A kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI4A_CLKSOURCE_SPDIF)
    RCC_CLK_MUX_SAI4A_SPDIF           , /**< SAI4A kernel clock source SPDIF */
#endif
#if defined(LL_RCC_SAI4B_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SAI4B_PLL1Q           , /**< SAI4B kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SAI4B_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SAI4B_PLL2P           , /**< SAI4B kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SAI4B_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SAI4B_PLL3P           , /**< SAI4B kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SAI4B_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SAI4B_PIN             , /**< SAI4B kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SAI4B_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SAI4B_LPCLK           , /**< SAI4B kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI4B_CLKSOURCE_SPDIF)
    RCC_CLK_MUX_SAI4B_SPDIF           , /**< SAI4B kernel clock source SPDIF */
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SPDIF_PLL1Q           , /**< SPDIF kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_PLL2R)
    RCC_CLK_MUX_SPDIF_PLL2R           , /**< SPDIF kernel clock source PLL2R */
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_SPDIF_PLL3R           , /**< SPDIF kernel clock source PLL3R */
#endif
#if defined(LL_RCC_SPDIF_CLKSOURCE_HSI)
    RCC_CLK_MUX_SPDIF_HSI             , /**< SPDIF kernel clock source HSI */
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_HSI48)
    RCC_CLK_MUX_RNG_HSI48             , /**< RNG kernel clock source HSI48 */
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_RNG_PLL1Q             , /**< RNG kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_LSE)
    RCC_CLK_MUX_RNG_LSE               , /**< RNG kernel clock source LSE */
#endif
#if defined(LL_RCC_RNG_CLKSOURCE_LSI)
    RCC_CLK_MUX_RNG_LSI               , /**< RNG kernel clock source LSI */
#endif
#if defined(LL_RCC_USB_CLKSOURCE_DISABLE)
    RCC_CLK_MUX_USB_DISABLE           , /**< USB kernel clock source DISABLE */
#endif
#if defined(LL_RCC_USB_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_USB_PLL1Q             , /**< USB kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_USB_CLKSOURCE_PLL3Q)
    RCC_CLK_MUX_USB_PLL3Q             , /**< USB kernel clock source PLL3Q */
#endif
#if defined(LL_RCC_USB_CLKSOURCE_HSI48)
    RCC_CLK_MUX_USB_HSI48             , /**< USB kernel clock source HSI48 */
#endif
    RCC_CLK_MUX_FDCAN_HSE             , /**< FDCAN kernel clock source HSE */
    RCC_CLK_MUX_FDCAN_PLL1Q           , /**< FDCAN kernel clock source PLL1Q */
    RCC_CLK_MUX_FDCAN_PLL2Q           , /**< FDCAN kernel clock source PLL2Q */
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SDMMC_PLL1Q           , /**< SDMMC kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2R)
    RCC_CLK_MUX_SDMMC_PLL2R           , /**< SDMMC kernel clock source PLL2R */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2S)
    RCC_CLK_MUX_SDMMC_PLL2S           , /**< SDMMC kernel clock source PLL2S */
#endif
#if defined(LL_RCC_SDMMC_CLKSOURCE_PLL2T)
    RCC_CLK_MUX_SDMMC_PLL2T           , /**< SDMMC kernel clock source PLL2T */
#endif
    RCC_CLK_MUX_FMC_HCLK              , /**< FMC kernel clock source HCLK */
    RCC_CLK_MUX_FMC_PLL1Q             , /**< FMC kernel clock source PLL1Q */
    RCC_CLK_MUX_FMC_PLL2R             , /**< FMC kernel clock source PLL2R */
#if defined(LL_RCC_FMC_CLKSOURCE_CLKP)
    RCC_CLK_MUX_FMC_LPCLK             , /**< FMC kernel clock source CLKP */
#endif
#if defined(LL_RCC_FMC_CLKSOURCE_HSI)
    RCC_CLK_MUX_FMC_HSI               , /**< FMC kernel clock source HSI */
#endif
#if defined(LL_RCC_QSPI_CLKSOURCE_HCLK)
    RCC_CLK_MUX_QSPI_HCLK             , /**< QSPI kernel clock source HCLK */
#endif
#if defined(LL_RCC_QSPI_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_QSPI_PLL1Q            , /**< QSPI kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_QSPI_CLKSOURCE_PLL2R)
    RCC_CLK_MUX_QSPI_PLL2R            , /**< QSPI kernel clock source PLL2R */
#endif
#if defined(LL_RCC_QSPI_CLKSOURCE_CLKP)
    RCC_CLK_MUX_QSPI_LPCLK            , /**< QSPI kernel clock source CLKP */
#endif
#if defined(LL_RCC_OSPI_CLKSOURCE_HCLK)
    RCC_CLK_MUX_OSPI_HCLK             , /**< OSPI kernel clock source HCLK */
#endif
#if defined(LL_RCC_OSPI_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_OSPI_PLL1Q            , /**< OSPI kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_OSPI_CLKSOURCE_PLL2R)
    RCC_CLK_MUX_OSPI_PLL2R            , /**< OSPI kernel clock source PLL2R */
#endif
#if defined(LL_RCC_OSPI_CLKSOURCE_CLKP)
    RCC_CLK_MUX_OSPI_LPCLK            , /**< OSPI kernel clock source CLKP */
#endif
    RCC_CLK_MUX_ADC_PLL2P             , /**< ADC kernel clock source PLL2P */
    RCC_CLK_MUX_ADC_PLL3R             , /**< ADC kernel clock source PLL3R */
    RCC_CLK_MUX_ADC_LPCLK             , /**< ADC kernel clock source CLKP */
    RCC_CLK_MUX_CEC_LSE               , /**< CEC kernel clock source LSE */
    RCC_CLK_MUX_CEC_LSI               , /**< CEC kernel clock source LSI */
    RCC_CLK_MUX_CEC_CSI_DIV122        , /**< CEC kernel clock source CSI_DIV122 */
#if defined(LL_RCC_DFSDM1_CLKSOURCE_PCLK2)
    RCC_CLK_MUX_DFSDM1_PCLK2          , /**< DFSDM1 kernel clock source PCLK2 */
#endif
#if defined(LL_RCC_DFSDM1_CLKSOURCE_SYSCLK)
    RCC_CLK_MUX_DFSDM1_SYSCLK         , /**< DFSDM1 kernel clock source SYSCLK */
#endif
#if defined(LL_RCC_DFSDM2_CLKSOURCE_PCLK4)
    RCC_CLK_MUX_DFSDM2_PCLK4          , /**< DFSDM2 kernel clock source PCLK4 */
#endif
#if defined(LL_RCC_DFSDM2_CLKSOURCE_SYSCLK)
    RCC_CLK_MUX_DFSDM2_SYSCLK         , /**< DFSDM2 kernel clock source SYSCLK */
#endif
#if defined(LL_RCC_SWP_CLKSOURCE_PCLK1)
    RCC_CLK_MUX_SWP_PCLK1             , /**< SWP kernel clock source PCLK1 */
#endif
#if defined(LL_RCC_SWP_CLKSOURCE_HSI)
    RCC_CLK_MUX_SWP_HSI               , /**< SWP kernel clock source HSI */
#endif
#if defined(LL_RCC_DSI_CLKSOURCE_PHY)
    RCC_CLK_MUX_DSI_PHY               , /**< DSI kernel clock source PHY */
#endif
#if defined(LL_RCC_DSI_CLKSOURCE_PLL2Q)
    RCC_CLK_MUX_DSI_PLL2Q             , /**< DSI kernel clock source PLL2Q */
#endif
#if defined(LL_RCC_USART1_CLKSOURCE_PCLK2)
    RCC_CLK_MUX_USART1_PCLK2          , /**< USART1 kernel clock source PCLK2 */
#endif
#if defined(LL_RCC_USART1_CLKSOURCE_PLL2Q)
    RCC_CLK_MUX_USART1_PLL2Q          , /**< USART1 kernel clock source PLL2Q */
#endif
#if defined(LL_RCC_USART1_CLKSOURCE_PLL3Q)
    RCC_CLK_MUX_USART1_PLL3Q          , /**< USART1 kernel clock source PLL3Q */
#endif
#if defined(LL_RCC_USART1_CLKSOURCE_HSI)
    RCC_CLK_MUX_USART1_HSI            , /**< USART1 kernel clock source HSI */
#endif
#if defined(LL_RCC_USART1_CLKSOURCE_CSI)
    RCC_CLK_MUX_USART1_CSI            , /**< USART1 kernel clock source CSI */
#endif
#if defined(LL_RCC_USART1_CLKSOURCE_LSE)
    RCC_CLK_MUX_USART1_LSE            , /**< USART1 kernel clock source LSE */
#endif
#if defined(LL_RCC_SPI1_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SPI1_PLL1Q            , /**< SPI1 kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SPI1_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SPI1_PLL2P            , /**< SPI1 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SPI1_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SPI1_PLL3P            , /**< SPI1 kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SPI1_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SPI1_PIN              , /**< SPI1 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SPI1_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SPI1_LPCLK            , /**< SPI1 kernel clock source CLKP */
#endif
#if defined(LL_RCC_SPI23_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SPI23_PLL1Q           , /**< SPI23 kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SPI23_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SPI23_PLL2P           , /**< SPI23 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SPI23_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SPI23_PLL3P           , /**< SPI23 kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SPI23_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SPI23_PIN             , /**< SPI23 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SPI23_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SPI23_LPCLK           , /**< SPI23 kernel clock source CLKP */
#endif
#if defined(LL_RCC_I2C1_CLKSOURCE_PCLK1)
    RCC_CLK_MUX_I2C1_PCLK1            , /**< I2C1 kernel clock source PCLK1 */
#endif
#if defined(LL_RCC_I2C1_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_I2C1_PLL3R            , /**< I2C1 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_I2C1_CLKSOURCE_HSI)
    RCC_CLK_MUX_I2C1_HSI              , /**< I2C1 kernel clock source HSI */
#endif
#if defined(LL_RCC_I2C1_CLKSOURCE_CSI)
    RCC_CLK_MUX_I2C1_CSI              , /**< I2C1 kernel clock source CSI */
#endif
#if defined(LL_RCC_I2C23_CLKSOURCE_PCLK1)
    RCC_CLK_MUX_I2C23_PCLK1           , /**< I2C23 kernel clock source PCLK1 */
#endif
#if defined(LL_RCC_I2C23_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_I2C23_PLL3R           , /**< I2C23 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_I2C23_CLKSOURCE_HSI)
    RCC_CLK_MUX_I2C23_HSI             , /**< I2C23 kernel clock source HSI */
#endif
#if defined(LL_RCC_I2C23_CLKSOURCE_CSI)
    RCC_CLK_MUX_I2C23_CSI             , /**< I2C23 kernel clock source CSI */
#endif
#if defined(LL_RCC_LPTIM23_CLKSOURCE_PCLK4)
    RCC_CLK_MUX_LPTIM23_PCLK4         , /**< LPTIM23 kernel clock source PCLK4 */
#endif
#if defined(LL_RCC_LPTIM23_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_LPTIM23_PLL2P         , /**< LPTIM23 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_LPTIM23_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_LPTIM23_PLL3R         , /**< LPTIM23 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_LPTIM23_CLKSOURCE_LSE)
    RCC_CLK_MUX_LPTIM23_LSE           , /**< LPTIM23 kernel clock source LSE */
#endif
#if defined(LL_RCC_LPTIM23_CLKSOURCE_LSI)
    RCC_CLK_MUX_LPTIM23_LSI           , /**< LPTIM23 kernel clock source LSI */
#endif
#if defined(LL_RCC_LPTIM23_CLKSOURCE_CLKP)
    RCC_CLK_MUX_LPTIM23_LPCLK         , /**< LPTIM23 kernel clock source CLKP */
#endif
#if defined(LL_RCC_LPTIM45_CLKSOURCE_PCLK4)
    RCC_CLK_MUX_LPTIM45_PCLK4         , /**< LPTIM45 kernel clock source PCLK4 */
#endif
#if defined(LL_RCC_LPTIM45_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_LPTIM45_PLL2P         , /**< LPTIM45 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_LPTIM45_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_LPTIM45_PLL3R         , /**< LPTIM45 kernel clock source PLL3R */
#endif
#if defined(LL_RCC_LPTIM45_CLKSOURCE_LSE)
    RCC_CLK_MUX_LPTIM45_LSE           , /**< LPTIM45 kernel clock source LSE */
#endif
#if defined(LL_RCC_LPTIM45_CLKSOURCE_LSI)
    RCC_CLK_MUX_LPTIM45_LSI           , /**< LPTIM45 kernel clock source LSI */
#endif
#if defined(LL_RCC_LPTIM45_CLKSOURCE_CLKP)
    RCC_CLK_MUX_LPTIM45_LPCLK         , /**< LPTIM45 kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SAI2_PLL1Q            , /**< SAI2 kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_SAI2_PLL2P            , /**< SAI2 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_SAI2_PLL3P            , /**< SAI2 kernel clock source PLL3P */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_SAI2_PIN              , /**< SAI2 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_CLKP)
    RCC_CLK_MUX_SAI2_LPCLK            , /**< SAI2 kernel clock source CLKP */
#endif
#if defined(LL_RCC_SAI2_CLKSOURCE_SPDIFRX)
    RCC_CLK_MUX_SAI2_SPDIF            , /**< SAI2 kernel clock source SPDIFRX */
#endif
#if defined(LL_RCC_SPDIFRX_CLKSOURCE_PLL1Q)
    RCC_CLK_MUX_SPDIFRX_PLL1Q         , /**< SPDIFRX kernel clock source PLL1Q */
#endif
#if defined(LL_RCC_SPDIFRX_CLKSOURCE_PLL2R)
    RCC_CLK_MUX_SPDIFRX_PLL2R         , /**< SPDIFRX kernel clock source PLL2R */
#endif
#if defined(LL_RCC_SPDIFRX_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_SPDIFRX_PLL3R         , /**< SPDIFRX kernel clock source PLL3R */
#endif
#if defined(LL_RCC_SPDIFRX_CLKSOURCE_HSI)
    RCC_CLK_MUX_SPDIFRX_HSI           , /**< SPDIFRX kernel clock source HSI */
#endif
#if defined(LL_RCC_XSPI1_CLKSOURCE_HCLK)
    RCC_CLK_MUX_XSPI1_HCLK            , /**< XSPI1 kernel clock source HCLK */
#endif
#if defined(LL_RCC_XSPI1_CLKSOURCE_PLL2S)
    RCC_CLK_MUX_XSPI1_PLL2S           , /**< XSPI1 kernel clock source PLL2S */
#endif
#if defined(LL_RCC_XSPI1_CLKSOURCE_PLL2T)
    RCC_CLK_MUX_XSPI1_PLL2T           , /**< XSPI1 kernel clock source PLL2T */
#endif
#if defined(LL_RCC_XSPI2_CLKSOURCE_HCLK)
    RCC_CLK_MUX_XSPI2_HCLK            , /**< XSPI2 kernel clock source HCLK */
#endif
#if defined(LL_RCC_XSPI2_CLKSOURCE_PLL2S)
    RCC_CLK_MUX_XSPI2_PLL2S           , /**< XSPI2 kernel clock source PLL2S */
#endif
#if defined(LL_RCC_XSPI2_CLKSOURCE_PLL2T)
    RCC_CLK_MUX_XSPI2_PLL2T           , /**< XSPI2 kernel clock source PLL2T */
#endif
#if defined(LL_RCC_ADF1_CLKSOURCE_HCLK)
    RCC_CLK_MUX_ADF1_HCLK             , /**< ADF1 kernel clock source HCLK */
#endif
#if defined(LL_RCC_ADF1_CLKSOURCE_PLL2P)
    RCC_CLK_MUX_ADF1_PLL2P            , /**< ADF1 kernel clock source PLL2P */
#endif
#if defined(LL_RCC_ADF1_CLKSOURCE_PLL3P)
    RCC_CLK_MUX_ADF1_PLL3P            , /**< ADF1 kernel clock source PLL3P */
#endif
#if defined(LL_RCC_ADF1_CLKSOURCE_I2S_CKIN)
    RCC_CLK_MUX_ADF1_PIN              , /**< ADF1 kernel clock source I2S_CKIN */
#endif
#if defined(LL_RCC_ADF1_CLKSOURCE_CSI)
    RCC_CLK_MUX_ADF1_CSI              , /**< ADF1 kernel clock source CSI */
#endif
#if defined(LL_RCC_ADF1_CLKSOURCE_HSI)
    RCC_CLK_MUX_ADF1_HSI              , /**< ADF1 kernel clock source HSI */
#endif
#if defined(LL_RCC_OTGFS_CLKSOURCE_HSI48)
    RCC_CLK_MUX_OTGFS_HSI48           , /**< OTGFS kernel clock source HSI48 */
#endif
#if defined(LL_RCC_OTGFS_CLKSOURCE_PLL3Q)
    RCC_CLK_MUX_OTGFS_PLL3Q           , /**< OTGFS kernel clock source PLL3Q */
#endif
#if defined(LL_RCC_OTGFS_CLKSOURCE_HSE)
    RCC_CLK_MUX_OTGFS_HSE             , /**< OTGFS kernel clock source HSE */
#endif
#if defined(LL_RCC_OTGFS_CLKSOURCE_CLK48)
    RCC_CLK_MUX_OTGFS_CLK48           , /**< OTGFS kernel clock source CLK48 */
#endif
#if defined(LL_RCC_USBPHYC_CLKSOURCE_HSE)
    RCC_CLK_MUX_USBPHYC_HSE           , /**< USBPHYC kernel clock source HSE */
#endif
#if defined(LL_RCC_USBPHYC_CLKSOURCE_HSE_DIV_2)
    RCC_CLK_MUX_USBPHYC_HSE_DIV2      , /**< USBPHYC kernel clock source HSE_DIV_2 */
#endif
#if defined(LL_RCC_USBPHYC_CLKSOURCE_PLL3Q)
    RCC_CLK_MUX_USBPHYC_PLL3Q         , /**< USBPHYC kernel clock source PLL3Q */
#endif
#if defined(LL_RCC_USBPHYC_CLKSOURCE_DISABLE)
    RCC_CLK_MUX_USBPHYC_DISABLE       , /**< USBPHYC kernel clock source DISABLE */
#endif
#if defined(LL_RCC_PSSI_CLKSOURCE_PLL3R)
    RCC_CLK_MUX_PSSI_PLL3R            , /**< PSSI kernel clock source PLL3R */
#endif
#if defined(LL_RCC_PSSI_CLKSOURCE_CLKP)
    RCC_CLK_MUX_PSSI_LPCLK            , /**< PSSI kernel clock source CLKP */
#endif
    RCC_CLK_MUX_CLKP_HSI              , /**< CLKP kernel clock source HSI */
    RCC_CLK_MUX_CLKP_CSI              , /**< CLKP kernel clock source CSI */
    RCC_CLK_MUX_CLKP_HSE              , /**< CLKP kernel clock source HSE */

    RCC_CLK_MUX_LIST_CNT
}   rcc_ClkMuxId_t;

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

rcc_RequestState_t          Rcc_ClkMux_Init             ( void );
void                        Rcc_ClkMux_Deinit           ( void );
void                        Rcc_ClkMux_Task             ( void );

rcc_RequestState_t          Rcc_ClkMux_Set_ClkActive    ( rcc_ClkMuxId_t clkMuxId  );
rcc_RequestState_t          Rcc_ClkMux_Set_ClkInactive  ( rcc_ClkMuxId_t clkMuxId  );

rcc_RequestState_t          Rcc_ClkMux_Get_ClkSrc       ( rcc_ClkMuxId_t clkMuxIdIn, rcc_ClkMuxId_t * const clkMuxId );

#ifdef __cplusplus
}
#endif

#endif /* RCC_CLKMUX_RCC_CLKMUX_H */
