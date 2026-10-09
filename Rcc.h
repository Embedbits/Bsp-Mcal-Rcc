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

/** \brief RCC peripheral blocks IDs */
typedef enum rcc_BlockList_t
{

    /*------------------------------ System core -------------------------------*/

    RCC_BLOCK_FLASH       , /**< Flash memory interface */
    RCC_BLOCK_SYSCFG      , /**< System configuration controller, COMP, VREFBUF */
    RCC_BLOCK_PWR         , /**< Power interface */
#if defined(RCC_APB2ENR_FWEN)
    RCC_BLOCK_FW          , /**< Firewall (enable only, cleared by system reset) */
#endif
    RCC_BLOCK_SYSTICK     , /**< SysTick (no clock enable) */
    RCC_BLOCK_IWDG        , /**< Independent watchdog (clocked by LSI, no clock enable) */
    RCC_BLOCK_RTC         , /**< Real Time Clock (RTCEN in backup domain) */
#if defined(RCC_APB1ENR1_RTCAPBEN)
    RCC_BLOCK_RTCAPB      , /**< Real Time Clock APB interface */
#endif
#if defined(RCC_AHB1SMENR_SRAM1SMEN)
    RCC_BLOCK_SRAM1       , /**< SRAM1 (sleep mode clock only) */
#endif
#if defined(RCC_AHB2SMENR_SRAM2SMEN)
    RCC_BLOCK_SRAM2       , /**< SRAM2 (sleep mode clock only) */
#endif
#if defined(RCC_AHB2SMENR_SRAM3SMEN)
    RCC_BLOCK_SRAM3       , /**< SRAM3 (sleep mode clock only) */
#endif
    RCC_BLOCK_DMA1        , /**< DMA1 */
    RCC_BLOCK_DMA2        , /**< DMA2 */
#if defined(RCC_AHB1ENR_DMAMUX1EN)
    RCC_BLOCK_DMAMUX1     , /**< DMA request multiplexer */
#endif
#if defined(RCC_AHB1ENR_DMA2DEN)
    RCC_BLOCK_DMA2D       , /**< DMA2D (Chrom-ART accelerator) */
#endif
#if defined(RCC_AHB1ENR_GFXMMUEN)
    RCC_BLOCK_GFXMMU      , /**< Graphic MMU */
#endif
#if defined(RCC_APB1ENR1_CRSEN)
    RCC_BLOCK_CRS         , /**< Clock recovery system */
#endif
    RCC_BLOCK_WWDG        , /**< Window watchdog */
    RCC_BLOCK_GPIOA       , /**< IO port A */
    RCC_BLOCK_GPIOB       , /**< IO port B */
    RCC_BLOCK_GPIOC       , /**< IO port C */
#if defined(RCC_AHB2ENR_GPIODEN)
    RCC_BLOCK_GPIOD       , /**< IO port D */
#endif
#if defined(RCC_AHB2ENR_GPIOEEN)
    RCC_BLOCK_GPIOE       , /**< IO port E */
#endif
#if defined(RCC_AHB2ENR_GPIOFEN)
    RCC_BLOCK_GPIOF       , /**< IO port F */
#endif
#if defined(RCC_AHB2ENR_GPIOGEN)
    RCC_BLOCK_GPIOG       , /**< IO port G */
#endif
    RCC_BLOCK_GPIOH       , /**< IO port H */
#if defined(RCC_AHB2ENR_GPIOIEN)
    RCC_BLOCK_GPIOI       , /**< IO port I */
#endif
    RCC_BLOCK_TSC         , /**< Touch sensing controller */

    /*--------------------------------- Timers ---------------------------------*/

    RCC_BLOCK_TIM1        , /**< TIM1 */
    RCC_BLOCK_TIM2        , /**< TIM2 */
#if defined(RCC_APB1ENR1_TIM3EN)
    RCC_BLOCK_TIM3        , /**< TIM3 */
#endif
#if defined(RCC_APB1ENR1_TIM4EN)
    RCC_BLOCK_TIM4        , /**< TIM4 */
#endif
#if defined(RCC_APB1ENR1_TIM5EN)
    RCC_BLOCK_TIM5        , /**< TIM5 */
#endif
    RCC_BLOCK_TIM6        , /**< TIM6 */
#if defined(RCC_APB1ENR1_TIM7EN)
    RCC_BLOCK_TIM7        , /**< TIM7 */
#endif
#if defined(RCC_APB2ENR_TIM8EN)
    RCC_BLOCK_TIM8        , /**< TIM8 */
#endif
    RCC_BLOCK_TIM15       , /**< TIM15 */
    RCC_BLOCK_TIM16       , /**< TIM16 */
#if defined(RCC_APB2ENR_TIM17EN)
    RCC_BLOCK_TIM17       , /**< TIM17 */
#endif
    RCC_BLOCK_LPTIM1      , /**< Low Power Timer 1 */
    RCC_BLOCK_LPTIM2      , /**< Low Power Timer 2 */

    /*------------------------------ Connectivity ------------------------------*/

    RCC_BLOCK_SPI1        , /**< SPI1 */
#if defined(RCC_APB1ENR1_SPI2EN)
    RCC_BLOCK_SPI2        , /**< SPI2 */
#endif
#if defined(RCC_APB1ENR1_SPI3EN)
    RCC_BLOCK_SPI3        , /**< SPI3 */
#endif
    RCC_BLOCK_I2C1        , /**< I2C1 */
#if defined(RCC_APB1ENR1_I2C2EN)
    RCC_BLOCK_I2C2        , /**< I2C2 */
#endif
    RCC_BLOCK_I2C3        , /**< I2C3 */
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_BLOCK_I2C4        , /**< I2C4 */
#endif
    RCC_BLOCK_USART1      , /**< USART1 */
    RCC_BLOCK_USART2      , /**< USART2 */
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_BLOCK_USART3      , /**< USART3 */
#endif
#if defined(RCC_APB1ENR1_UART4EN)
    RCC_BLOCK_UART4       , /**< UART4 */
#endif
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_BLOCK_UART5       , /**< UART5 */
#endif
    RCC_BLOCK_LPUART1     , /**< Low power UART 1 */
#if defined(RCC_APB1ENR1_CAN1EN)
    RCC_BLOCK_CAN1        , /**< CAN1 */
#endif
#if defined(RCC_APB1ENR1_CAN2EN)
    RCC_BLOCK_CAN2        , /**< CAN2 */
#endif
#if defined(RCC_TYPES_USB_SUPPORT)
    RCC_BLOCK_USB         , /**< USB full speed device (VDDUSB validated with the clock) */
#endif
#if defined(RCC_TYPES_SDMMC1_SUPPORT)
    RCC_BLOCK_SDMMC1      , /**< SD / MMC card interface 1 */
#endif
#if defined(RCC_AHB2ENR_SDMMC2EN)
    RCC_BLOCK_SDMMC2      , /**< SD / MMC card interface 2 */
#endif
#if defined(RCC_AHB3ENR_FMCEN)
    RCC_BLOCK_FMC         , /**< Flexible memory controller */
#endif
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_BLOCK_QSPI        , /**< Quad SPI memory interface */
#endif
#if defined(RCC_AHB3ENR_OSPI1EN)
    RCC_BLOCK_OSPI1       , /**< OctoSPI 1 */
#endif
#if defined(RCC_AHB3ENR_OSPI2EN)
    RCC_BLOCK_OSPI2       , /**< OctoSPI 2 */
#endif
#if defined(RCC_AHB2ENR_OSPIMEN)
    RCC_BLOCK_OSPIM       , /**< OctoSPI IO manager */
#endif
#if defined(RCC_APB1ENR2_SWPMI1EN)
    RCC_BLOCK_SWPMI1      , /**< Single wire protocol master interface */
#endif

    /*------------------------------- Multimedia -------------------------------*/

#if defined(RCC_AHB2ENR_DCMIEN)
    RCC_BLOCK_DCMI        , /**< Digital camera interface */
#endif
#if defined(RCC_APB2ENR_LTDCEN)
    RCC_BLOCK_LTDC        , /**< LCD-TFT display controller */
#endif
#if defined(RCC_APB2ENR_DSIEN)
    RCC_BLOCK_DSI         , /**< Display serial interface host */
#endif
#if defined(RCC_APB2ENR_SAI1EN)
    RCC_BLOCK_SAI1        , /**< Serial audio interface 1 */
#endif
#if defined(RCC_APB2ENR_SAI2EN)
    RCC_BLOCK_SAI2        , /**< Serial audio interface 2 */
#endif
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_BLOCK_DFSDM1      , /**< Digital filter for sigma-delta modulators */
#endif
#if defined(RCC_APB1ENR1_LCDEN)
    RCC_BLOCK_LCD         , /**< LCD controller */
#endif

    /*--------------------------------- Analog ---------------------------------*/

    RCC_BLOCK_ADC         , /**< ADC (common for all ADCs) */
#if defined(RCC_APB1ENR1_DAC1EN)
    RCC_BLOCK_DAC1        , /**< DAC1 */
#endif
    RCC_BLOCK_OPAMP       , /**< Operational amplifiers */

    /*-------------------------------- Security --------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
    RCC_BLOCK_AES         , /**< AES hardware accelerator */
#endif
#if defined(RCC_AHB2ENR_HASHEN)
    RCC_BLOCK_HASH        , /**< HASH processor */
#endif
#if defined(RCC_AHB2ENR_PKAEN)
    RCC_BLOCK_PKA         , /**< Public key accelerator */
#endif
    RCC_BLOCK_RNG         , /**< Random number generator */

    /*------------------------------- Computing --------------------------------*/

    RCC_BLOCK_CRC         , /**< CRC calculation unit */

    RCC_BLOCK_LIST_CNT
}   rcc_BlockList_t;


/**
 * \brief List of possible clock sources used to read clock frequency.
 *
 */
typedef enum
{
    RCC_CLK_SRC_SYSCLK = 0u, /**< System clock source                                                   */
    RCC_CLK_SRC_AHBCLK,      /**< Advanced High-performance Bus clock (HCLK)                            */
    RCC_CLK_SRC_APB1CLK,     /**< Advanced Peripheral Bus 1 (APB1) clock (PCLK1)                        */
    RCC_CLK_SRC_APB2CLK,     /**< Advanced Peripheral Bus 2 (APB2) clock (PCLK2)                        */
    RCC_CLK_SRC_APB1TIMCLK,  /**< Timer kernel clock of APB1 timers (PCLK1 x1 / x2 by APB1 prescaler)    */
    RCC_CLK_SRC_APB2TIMCLK,  /**< Timer kernel clock of APB2 timers (PCLK2 x1 / x2 by APB2 prescaler)    */
    RCC_CLK_SRC_HSICLK,      /**< 16 MHz High Speed Internal (HSI16) clock                              */
    RCC_CLK_SRC_MSICLK,      /**< Multi Speed Internal (MSI) clock                                      */
#if defined(RCC_CRRCR_HSI48ON)
    RCC_CLK_SRC_HSI48CLK,    /**< 48 MHz High Speed Internal (HSI48) clock                              */
#endif /* RCC_CRRCR_HSI48ON */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock                                             */
    RCC_CLK_SRC_HSERTCCLK,   /**< High Speed External clock divided by 32 (RTC)                         */
    RCC_CLK_SRC_LSICLK,      /**< 32 kHz Low Speed Internal (LSI) oscillator clock                      */
    RCC_CLK_SRC_LSECLK,      /**< Low Speed External clock                                              */
    RCC_CLK_SRC_PLLQCLK,     /**< Main PLL output Q clock                                               */
#if defined(RCC_CR_PLLSAI1ON)
    RCC_CLK_SRC_PLLSAI1QCLK, /**< PLLSAI1 output Q clock                                                */
    RCC_CLK_SRC_PLLSAI1RCLK, /**< PLLSAI1 output R clock                                                */
#endif /* RCC_CR_PLLSAI1ON */
#if defined(RCC_CR_PLLSAI2ON)
    RCC_CLK_SRC_PLLSAI2RCLK, /**< PLLSAI2 output R clock                                                */
#endif /* RCC_CR_PLLSAI2ON */
    RCC_CLK_SRC_SDMMCCLK,    /**< SDMMC kernel clock - 48 MHz clock (CLK48) or main PLL output P (L4+)   */
    RCC_CLK_SRC_CNT          /**< Count of clock sources                                                */
}   rcc_ClkSrcId_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_H */
