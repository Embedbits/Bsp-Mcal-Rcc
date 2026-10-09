/**
 * \author Mr.Nobody
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

/** \brief RCC peripheral blocks IDs
 *
 * One block is one clock enable / sleep mode enable / reset bit triple in RCC
 * bus registers. More peripheral IDs (clock sources) can belong to one block.
 */
typedef enum rcc_BlockList_t
{
    /*------------------------------ System core -------------------------------*/

    RCC_BLOCK_FLASH = 0u  , /**< Flash memory interface */
    RCC_BLOCK_SYSCFG      , /**< System configuration controller (SYSCFG) */
    RCC_BLOCK_PWR         , /**< Power controller (PWR) */
    RCC_BLOCK_SYSTICK     , /**< System Tick timer (SysTick) external reference */
    RCC_BLOCK_IWDG        , /**< Independent watchdog (IWDG), clocked by LSI oscillator - no bus clock */
    RCC_BLOCK_RTC         , /**< Real Time Clock (RTC) APB interface */
    RCC_BLOCK_CRS         , /**< Clock recovery system (CRS) */
    RCC_BLOCK_WWDG        , /**< Window watchdog (WWDG) */
    RCC_BLOCK_RAMCFG      , /**< RAM configuration controller (RAMCFG) */
    RCC_BLOCK_BKPSRAM     , /**< Backup SRAM */
    RCC_BLOCK_SRAM1       , /**< SRAM1 */
    RCC_BLOCK_SRAM2       , /**< SRAM2 */
#if defined(RCC_AHB2ENR1_SRAM3EN)
    RCC_BLOCK_SRAM3       , /**< SRAM3 */
#endif /* SRAM3 */
    RCC_BLOCK_SRAM4       , /**< SRAM4 */
#if defined(RCC_AHB2ENR2_SRAM5EN)
    RCC_BLOCK_SRAM5       , /**< SRAM5 */
#endif /* SRAM5 */
#if defined(RCC_AHB2ENR2_SRAM6EN)
    RCC_BLOCK_SRAM6       , /**< SRAM6 */
#endif /* SRAM6 */
    RCC_BLOCK_DCACHE1     , /**< Data cache 1 (DCACHE1) */
#if defined(DCACHE2)
    RCC_BLOCK_DCACHE2     , /**< Data cache 2 (DCACHE2) */
#endif /* DCACHE2 */
    RCC_BLOCK_GTZC1       , /**< Global TrustZone controller 1 (GTZC1) */
    RCC_BLOCK_GTZC2       , /**< Global TrustZone controller 2 (GTZC2) */
    RCC_BLOCK_GPDMA1      , /**< General purpose DMA 1 (GPDMA1) */
    RCC_BLOCK_LPDMA1      , /**< Low power DMA 1 (LPDMA1) */
    RCC_BLOCK_GPIOA       , /**< IO port A */
    RCC_BLOCK_GPIOB       , /**< IO port B */
    RCC_BLOCK_GPIOC       , /**< IO port C */
    RCC_BLOCK_GPIOD       , /**< IO port D */
    RCC_BLOCK_GPIOE       , /**< IO port E */
#if defined(GPIOF)
    RCC_BLOCK_GPIOF       , /**< IO port F */
#endif /* GPIOF */
    RCC_BLOCK_GPIOG       , /**< IO port G */
    RCC_BLOCK_GPIOH       , /**< IO port H */
#if defined(GPIOI)
    RCC_BLOCK_GPIOI       , /**< IO port I */
#endif /* GPIOI */
#if defined(GPIOJ)
    RCC_BLOCK_GPIOJ       , /**< IO port J */
#endif /* GPIOJ */
    RCC_BLOCK_LPGPIO1     , /**< Low power general purpose IO (LPGPIO1) */

    /*--------------------------------- Timers ---------------------------------*/

    RCC_BLOCK_TIM1        , /**< Timer 1 (TIM1) */
    RCC_BLOCK_TIM2        , /**< Timer 2 (TIM2) */
    RCC_BLOCK_TIM3        , /**< Timer 3 (TIM3) */
    RCC_BLOCK_TIM4        , /**< Timer 4 (TIM4) */
    RCC_BLOCK_TIM5        , /**< Timer 5 (TIM5) */
    RCC_BLOCK_TIM6        , /**< Timer 6 (TIM6) */
    RCC_BLOCK_TIM7        , /**< Timer 7 (TIM7) */
    RCC_BLOCK_TIM8        , /**< Timer 8 (TIM8) */
    RCC_BLOCK_TIM15       , /**< Timer 15 (TIM15) */
    RCC_BLOCK_TIM16       , /**< Timer 16 (TIM16) */
    RCC_BLOCK_TIM17       , /**< Timer 17 (TIM17) */
    RCC_BLOCK_LPTIM1      , /**< Low power timer 1 (LPTIM1) */
    RCC_BLOCK_LPTIM2      , /**< Low power timer 2 (LPTIM2) */
    RCC_BLOCK_LPTIM3      , /**< Low power timer 3 (LPTIM3) */
    RCC_BLOCK_LPTIM4      , /**< Low power timer 4 (LPTIM4) */

    /*------------------------------ Connectivity ------------------------------*/

    RCC_BLOCK_SPI1        , /**< Serial peripheral interface 1 (SPI1) */
    RCC_BLOCK_SPI2        , /**< Serial peripheral interface 2 (SPI2) */
    RCC_BLOCK_SPI3        , /**< Serial peripheral interface 3 (SPI3) */
    RCC_BLOCK_I2C1        , /**< Inter-integrated circuit interface 1 (I2C1) */
    RCC_BLOCK_I2C2        , /**< Inter-integrated circuit interface 2 (I2C2) */
    RCC_BLOCK_I2C3        , /**< Inter-integrated circuit interface 3 (I2C3) */
    RCC_BLOCK_I2C4        , /**< Inter-integrated circuit interface 4 (I2C4) */
#if defined(I2C5)
    RCC_BLOCK_I2C5        , /**< Inter-integrated circuit interface 5 (I2C5) */
#endif /* I2C5 */
#if defined(I2C6)
    RCC_BLOCK_I2C6        , /**< Inter-integrated circuit interface 6 (I2C6) */
#endif /* I2C6 */
    RCC_BLOCK_USART1      , /**< Universal synchronous asynchronous receiver transmitter 1 (USART1) */
#if defined(USART2)
    RCC_BLOCK_USART2      , /**< Universal synchronous asynchronous receiver transmitter 2 (USART2) */
#endif /* USART2 */
    RCC_BLOCK_USART3      , /**< Universal synchronous asynchronous receiver transmitter 3 (USART3) */
    RCC_BLOCK_UART4       , /**< Universal asynchronous receiver transmitter 4 (UART4) */
    RCC_BLOCK_UART5       , /**< Universal asynchronous receiver transmitter 5 (UART5) */
#if defined(USART6)
    RCC_BLOCK_USART6      , /**< Universal synchronous asynchronous receiver transmitter 6 (USART6) */
#endif /* USART6 */
    RCC_BLOCK_LPUART1     , /**< Low power universal asynchronous receiver transmitter 1 (LPUART1) */
    RCC_BLOCK_FDCAN1      , /**< Controller area network with flexible data rate 1 (FDCAN1) */
#if defined(RCC_APB2ENR_USBEN)
    RCC_BLOCK_USB         , /**< USB full speed device (USB DRD FS) */
#endif /* USB */
#if defined(RCC_AHB2ENR1_OTGEN)
    RCC_BLOCK_OTG         , /**< USB on-the-go (OTG FS / OTG HS) */
#endif /* OTG */
#if defined(RCC_AHB2ENR1_USBPHYCEN)
    RCC_BLOCK_USBPHYC     , /**< USB OTG HS PHY controller (USBPHYC) */
#endif /* USBPHYC */
#if defined(UCPD1)
    RCC_BLOCK_UCPD1       , /**< USB Type-C power delivery 1 (UCPD1) */
#endif /* UCPD1 */
    RCC_BLOCK_OCTOSPI1    , /**< OctoSPI 1 (OCTOSPI1) */
#if defined(OCTOSPI2)
    RCC_BLOCK_OCTOSPI2    , /**< OctoSPI 2 (OCTOSPI2) */
#endif /* OCTOSPI2 */
#if defined(OCTOSPIM)
    RCC_BLOCK_OCTOSPIM    , /**< OctoSPI IO manager (OCTOSPIM) */
#endif /* OCTOSPIM */
#if defined(HSPI1)
    RCC_BLOCK_HSPI1       , /**< Hexadeca-SPI 1 (HSPI1) */
#endif /* HSPI1 */
#if defined(RCC_AHB2ENR2_FSMCEN)
    RCC_BLOCK_FMC         , /**< Flexible memory controller (FMC) */
#endif /* FMC */
    RCC_BLOCK_SDMMC1      , /**< SD / SDIO / MMC card host interface 1 (SDMMC1) */
#if defined(SDMMC2)
    RCC_BLOCK_SDMMC2      , /**< SD / SDIO / MMC card host interface 2 (SDMMC2) */
#endif /* SDMMC2 */

    /*------------------------------- Multimedia -------------------------------*/

    RCC_BLOCK_DCMI_PSSI   , /**< Digital camera interface (DCMI) and parallel synchronous slave interface (PSSI) */
    RCC_BLOCK_SAI1        , /**< Serial audio interface 1 (SAI1) */
#if defined(SAI2)
    RCC_BLOCK_SAI2        , /**< Serial audio interface 2 (SAI2) */
#endif /* SAI2 */
    RCC_BLOCK_MDF1        , /**< Multi-function digital filter 1 (MDF1) */
    RCC_BLOCK_ADF1        , /**< Audio digital filter 1 (ADF1) */
#if defined(DMA2D)
    RCC_BLOCK_DMA2D       , /**< Chrom-ART accelerator (DMA2D) */
#endif /* DMA2D */
#if defined(GPU2D)
    RCC_BLOCK_GPU2D       , /**< Neo-Chrom graphic processor (GPU2D) */
#endif /* GPU2D */
#if defined(GFXMMU)
    RCC_BLOCK_GFXMMU      , /**< Chrom-GRC graphic MMU (GFXMMU) */
#endif /* GFXMMU */
#if defined(GFXTIM)
    RCC_BLOCK_GFXTIM      , /**< Graphic timer (GFXTIM) */
#endif /* GFXTIM */
#if defined(JPEG)
    RCC_BLOCK_JPEG        , /**< JPEG codec (JPEG) */
#endif /* JPEG */
#if defined(LTDC)
    RCC_BLOCK_LTDC        , /**< LCD-TFT display controller (LTDC) */
#endif /* LTDC */
#if defined(DSI)
    RCC_BLOCK_DSIHOST     , /**< Display serial interface host (DSI) */
#endif /* DSIHOST */
    RCC_BLOCK_TSC         , /**< Touch sensing controller (TSC) */

    /*--------------------------------- Analog ---------------------------------*/

    RCC_BLOCK_ADC12       , /**< Analog-to-digital converters 1 and 2 (ADC1, ADC2) */
    RCC_BLOCK_ADC4        , /**< Analog-to-digital converter 4 (ADC4) */
    RCC_BLOCK_DAC1        , /**< Digital-to-analog converter 1 (DAC1) */
    RCC_BLOCK_COMP        , /**< Comparators (COMP) */
    RCC_BLOCK_OPAMP       , /**< Operational amplifiers (OPAMP) */
    RCC_BLOCK_VREF        , /**< Voltage reference buffer (VREFBUF) */

    /*-------------------------------- Security --------------------------------*/

#if defined(AES)
    RCC_BLOCK_AES         , /**< Advanced encryption standard hardware accelerator (AES) */
#endif /* AES */
#if defined(SAES)
    RCC_BLOCK_SAES        , /**< Secure advanced encryption standard hardware accelerator (SAES) */
#endif /* SAES */
    RCC_BLOCK_HASH        , /**< Hash processor (HASH) */
#if defined(PKA)
    RCC_BLOCK_PKA         , /**< Public key accelerator (PKA) */
#endif /* PKA */
#if defined(OTFDEC1)
    RCC_BLOCK_OTFDEC1     , /**< On-the-fly decryption engine 1 (OTFDEC1) */
#endif /* OTFDEC1 */
#if defined(OTFDEC2)
    RCC_BLOCK_OTFDEC2     , /**< On-the-fly decryption engine 2 (OTFDEC2) */
#endif /* OTFDEC2 */
    RCC_BLOCK_RNG         , /**< True random number generator (RNG) */

    /*------------------------------- Computing --------------------------------*/

    RCC_BLOCK_CORDIC      , /**< CORDIC co-processor (CORDIC) */
    RCC_BLOCK_CRC         , /**< Cyclic redundancy check calculation unit (CRC) */
    RCC_BLOCK_FMAC        , /**< Filter mathematical accelerator (FMAC) */

    RCC_BLOCK_LIST_CNT
}   rcc_BlockList_t;


/**
 * \brief List of possible clock sources used to read clock frequency.
 *
 */
typedef enum
{
    RCC_CLK_SRC_SYSCLK = 0u,    /**< System clock source                                                     */
    RCC_CLK_SRC_PLL1RCLK,       /**< PLL 1 R output clock source                                             */
    RCC_CLK_SRC_PLL1QCLK,       /**< PLL 1 Q output clock source                                             */
    RCC_CLK_SRC_PLL1PCLK,       /**< PLL 1 P output clock source                                             */
    RCC_CLK_SRC_PLL2RCLK,       /**< PLL 2 R output clock source                                             */
    RCC_CLK_SRC_PLL2QCLK,       /**< PLL 2 Q output clock source                                             */
    RCC_CLK_SRC_PLL2PCLK,       /**< PLL 2 P output clock source                                             */
    RCC_CLK_SRC_PLL3RCLK,       /**< PLL 3 R output clock source                                             */
    RCC_CLK_SRC_PLL3QCLK,       /**< PLL 3 Q output clock source                                             */
    RCC_CLK_SRC_PLL3PCLK,       /**< PLL 3 P output clock source                                             */
    RCC_CLK_SRC_AHBCLK,         /**< Advanced High-performance Bus clock source (HCLK)                       */
    RCC_CLK_SRC_HCLKDIV8CLK,    /**< Advanced High-performance Bus clock divided by 8 (SysTick reference)    */
    RCC_CLK_SRC_APB1CLK,        /**< Advanced Peripheral Bus 1 (APB1) clock source                           */
    RCC_CLK_SRC_APB2CLK,        /**< Advanced Peripheral Bus 2 (APB2) clock source                           */
    RCC_CLK_SRC_APB3CLK,        /**< Advanced Peripheral Bus 3 (APB3) clock source                           */
    RCC_CLK_SRC_APB1TIMCLK,     /**< Timer kernel clock of APB1 timers (PCLK1 x1 / x2 by APB1 prescaler)     */
    RCC_CLK_SRC_APB2TIMCLK,     /**< Timer kernel clock of APB2 timers (PCLK2 x1 / x2 by APB2 prescaler)     */
    RCC_CLK_SRC_HSI16CLK,       /**< 16MHz High Speed Internal (HSI16) clock source                          */
    RCC_CLK_SRC_MSISCLK,        /**< Multi-Speed Internal oscillator system output (MSIS) clock source       */
    RCC_CLK_SRC_MSIKCLK,        /**< Multi-Speed Internal oscillator kernel output (MSIK) clock source       */
    RCC_CLK_SRC_HSI48CLK,       /**< 48MHz High Speed Internal (HSI48) oscillator clock source               */
    RCC_CLK_SRC_HSI48DIV2CLK,   /**< 48MHz High Speed Internal (HSI48) oscillator divided by 2               */
    RCC_CLK_SRC_HSECLK,         /**< High Speed External clock source                                        */
    RCC_CLK_SRC_HSEDIV32CLK,    /**< High Speed External clock source divided by 32 (RTC)                    */
    RCC_CLK_SRC_LSICLK,         /**< Low Speed Internal (LSI) oscillator clock source (after LSI prescaler)  */
    RCC_CLK_SRC_LSECLK,         /**< Low Speed External clock source                                         */
    RCC_CLK_SRC_PINCLK,         /**< External clock input pin - frequency is not known                       */
    RCC_CLK_SRC_CNT             /**< Count of clock sources                                                  */
}   rcc_ClkSrcId_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_H */
