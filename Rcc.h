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
    /*------------------------------ System core -----------------------------*/
    RCC_BLOCK_FLASH    = 0u, /**< Flash interface (sleep mode clock only) */
    RCC_BLOCK_SYSCFG       , /**< System configuration controller */
    RCC_BLOCK_PWR          , /**< Power interface */
    RCC_BLOCK_SYSTICK      , /**< SysTick module (no clock enable) */
    RCC_BLOCK_IWDG         , /**< Independent watchdog (clocked by LSI, no clock enable) */
    RCC_BLOCK_RTC          , /**< Real Time Clock (RTCEN in backup domain) */

#if defined(RCC_APB1ENR_RTCAPBEN)
    RCC_BLOCK_RTCAPB       , /**< Real Time Clock APB interface */
#endif /* RCC_APB1ENR_RTCAPBEN */

#if defined(RCC_AHB1LPENR_SRAM1LPEN)
    RCC_BLOCK_SRAM1        , /**< SRAM1 (sleep mode clock only) */
#endif /* RCC_AHB1LPENR_SRAM1LPEN */
#if defined(RCC_AHB1LPENR_SRAM2LPEN)
    RCC_BLOCK_SRAM2        , /**< SRAM2 (sleep mode clock only) */
#endif /* RCC_AHB1LPENR_SRAM2LPEN */
#if defined(RCC_AHB1LPENR_SRAM3LPEN)
    RCC_BLOCK_SRAM3        , /**< SRAM3 (sleep mode clock only) */
#endif /* RCC_AHB1LPENR_SRAM3LPEN */

#if defined(RCC_AHB1ENR_BKPSRAMEN)
    RCC_BLOCK_BKPSRAM      , /**< Backup SRAM interface */
#endif /* RCC_AHB1ENR_BKPSRAMEN */
#if defined(RCC_AHB1ENR_CCMDATARAMEN)
    RCC_BLOCK_CCMDATARAM   , /**< CCM data RAM */
#endif /* RCC_AHB1ENR_CCMDATARAMEN */

    RCC_BLOCK_DMA1         , /**< DMA1 */
    RCC_BLOCK_DMA2         , /**< DMA2 */
#if defined(RCC_AHB1ENR_DMA2DEN)
    RCC_BLOCK_DMA2D        , /**< DMA2D */
#endif /* RCC_AHB1ENR_DMA2DEN */

    RCC_BLOCK_WWDG         , /**< Window watchdog */

#if defined(RCC_APB2ENR_EXTITEN)
    RCC_BLOCK_EXTIT        , /**< EXTI and external interrupt controller */
#endif /* RCC_APB2ENR_EXTITEN */

    RCC_BLOCK_GPIOA        , /**< IO port A */
    RCC_BLOCK_GPIOB        , /**< IO port B */
    RCC_BLOCK_GPIOC        , /**< IO port C */
#if defined(RCC_AHB1ENR_GPIODEN)
    RCC_BLOCK_GPIOD        , /**< IO port D */
#endif /* RCC_AHB1ENR_GPIODEN */
#if defined(RCC_AHB1ENR_GPIOEEN)
    RCC_BLOCK_GPIOE        , /**< IO port E */
#endif /* RCC_AHB1ENR_GPIOEEN */
#if defined(RCC_AHB1ENR_GPIOFEN)
    RCC_BLOCK_GPIOF        , /**< IO port F */
#endif /* RCC_AHB1ENR_GPIOFEN */
#if defined(RCC_AHB1ENR_GPIOGEN)
    RCC_BLOCK_GPIOG        , /**< IO port G */
#endif /* RCC_AHB1ENR_GPIOGEN */
    RCC_BLOCK_GPIOH        , /**< IO port H */
#if defined(RCC_AHB1ENR_GPIOIEN)
    RCC_BLOCK_GPIOI        , /**< IO port I */
#endif /* RCC_AHB1ENR_GPIOIEN */
#if defined(RCC_AHB1ENR_GPIOJEN)
    RCC_BLOCK_GPIOJ        , /**< IO port J */
#endif /* RCC_AHB1ENR_GPIOJEN */
#if defined(RCC_AHB1ENR_GPIOKEN)
    RCC_BLOCK_GPIOK        , /**< IO port K */
#endif /* RCC_AHB1ENR_GPIOKEN */

    /*-------------------------------- Timers --------------------------------*/

    RCC_BLOCK_TIM1         , /**< TIM1 */
#if defined(RCC_APB1ENR_TIM2EN)
    RCC_BLOCK_TIM2         , /**< TIM2 */
#endif /* RCC_APB1ENR_TIM2EN */
#if defined(RCC_APB1ENR_TIM3EN)
    RCC_BLOCK_TIM3         , /**< TIM3 */
#endif /* RCC_APB1ENR_TIM3EN */
#if defined(RCC_APB1ENR_TIM4EN)
    RCC_BLOCK_TIM4         , /**< TIM4 */
#endif /* RCC_APB1ENR_TIM4EN */
    RCC_BLOCK_TIM5         , /**< TIM5 */
#if defined(RCC_APB1ENR_TIM6EN)
    RCC_BLOCK_TIM6         , /**< TIM6 */
#endif /* RCC_APB1ENR_TIM6EN */
#if defined(RCC_APB1ENR_TIM7EN)
    RCC_BLOCK_TIM7         , /**< TIM7 */
#endif /* RCC_APB1ENR_TIM7EN */
#if defined(RCC_APB2ENR_TIM8EN)
    RCC_BLOCK_TIM8         , /**< TIM8 */
#endif /* RCC_APB2ENR_TIM8EN */
    RCC_BLOCK_TIM9         , /**< TIM9 */
#if defined(RCC_APB2ENR_TIM10EN)
    RCC_BLOCK_TIM10        , /**< TIM10 */
#endif /* RCC_APB2ENR_TIM10EN */
    RCC_BLOCK_TIM11        , /**< TIM11 */
#if defined(RCC_APB1ENR_TIM12EN)
    RCC_BLOCK_TIM12        , /**< TIM12 */
#endif /* RCC_APB1ENR_TIM12EN */
#if defined(RCC_APB1ENR_TIM13EN)
    RCC_BLOCK_TIM13        , /**< TIM13 */
#endif /* RCC_APB1ENR_TIM13EN */
#if defined(RCC_APB1ENR_TIM14EN)
    RCC_BLOCK_TIM14        , /**< TIM14 */
#endif /* RCC_APB1ENR_TIM14EN */

#if defined(RCC_APB1ENR_LPTIM1EN)
    RCC_BLOCK_LPTIM1       , /**< Low Power Timer 1 */
#endif /* RCC_APB1ENR_LPTIM1EN */

    /*----------------------------- Connectivity -----------------------------*/

    RCC_BLOCK_SPI1         , /**< SPI/I2S 1 */
#if defined(RCC_APB1ENR_SPI2EN)
    RCC_BLOCK_SPI2         , /**< SPI/I2S 2 */
#endif /* RCC_APB1ENR_SPI2EN */
#if defined(RCC_APB1ENR_SPI3EN)
    RCC_BLOCK_SPI3         , /**< SPI/I2S 3 */
#endif /* RCC_APB1ENR_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
    RCC_BLOCK_SPI4         , /**< SPI/I2S 4 */
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_APB2ENR_SPI5EN)
    RCC_BLOCK_SPI5         , /**< SPI/I2S 5 */
#endif /* RCC_APB2ENR_SPI5EN */
#if defined(RCC_APB2ENR_SPI6EN)
    RCC_BLOCK_SPI6         , /**< SPI 6 */
#endif /* RCC_APB2ENR_SPI6EN */

    RCC_BLOCK_I2C1         , /**< I2C 1 */
    RCC_BLOCK_I2C2         , /**< I2C 2 */
#if defined(RCC_APB1ENR_I2C3EN)
    RCC_BLOCK_I2C3         , /**< I2C 3 */
#endif /* RCC_APB1ENR_I2C3EN */
#if defined(RCC_APB1ENR_FMPI2C1EN)
    RCC_BLOCK_FMPI2C1      , /**< Fast-mode Plus I2C 1 */
#endif /* RCC_APB1ENR_FMPI2C1EN */

    RCC_BLOCK_USART1       , /**< USART 1 */
    RCC_BLOCK_USART2       , /**< USART 2 */
#if defined(RCC_APB1ENR_USART3EN)
    RCC_BLOCK_USART3       , /**< USART 3 */
#endif /* RCC_APB1ENR_USART3EN */
#if defined(RCC_APB1ENR_UART4EN)
    RCC_BLOCK_UART4        , /**< UART 4 */
#endif /* RCC_APB1ENR_UART4EN */
#if defined(RCC_APB1ENR_UART5EN)
    RCC_BLOCK_UART5        , /**< UART 5 */
#endif /* RCC_APB1ENR_UART5EN */
#if defined(RCC_APB2ENR_USART6EN)
    RCC_BLOCK_USART6       , /**< USART 6 */
#endif /* RCC_APB2ENR_USART6EN */
#if defined(RCC_APB1ENR_UART7EN)
    RCC_BLOCK_UART7        , /**< UART 7 */
#endif /* RCC_APB1ENR_UART7EN */
#if defined(RCC_APB1ENR_UART8EN)
    RCC_BLOCK_UART8        , /**< UART 8 */
#endif /* RCC_APB1ENR_UART8EN */
#if defined(RCC_APB2ENR_UART9EN)
    RCC_BLOCK_UART9        , /**< UART 9 */
#endif /* RCC_APB2ENR_UART9EN */
#if defined(RCC_APB2ENR_UART10EN)
    RCC_BLOCK_UART10       , /**< UART 10 */
#endif /* RCC_APB2ENR_UART10EN */

#if defined(RCC_APB1ENR_CAN1EN)
    RCC_BLOCK_CAN1         , /**< CAN 1 */
#endif /* RCC_APB1ENR_CAN1EN */
#if defined(RCC_APB1ENR_CAN2EN)
    RCC_BLOCK_CAN2         , /**< CAN 2 */
#endif /* RCC_APB1ENR_CAN2EN */
#if defined(RCC_APB1ENR_CAN3EN)
    RCC_BLOCK_CAN3         , /**< CAN 3 */
#endif /* RCC_APB1ENR_CAN3EN */

#if defined(RCC_APB2ENR_SDIOEN)
    RCC_BLOCK_SDIO         , /**< SDIO */
#endif /* RCC_APB2ENR_SDIOEN */

#if defined(RCC_AHB3ENR_FSMCEN)
    RCC_BLOCK_FSMC         , /**< Flexible Static Memory Controller */
#endif /* RCC_AHB3ENR_FSMCEN */
#if defined(RCC_AHB3ENR_FMCEN)
    RCC_BLOCK_FMC          , /**< Flexible Memory Controller */
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_BLOCK_QSPI         , /**< Quad SPI memory interface */
#endif /* RCC_AHB3ENR_QSPIEN */

#if defined(RCC_AHB2ENR_OTGFSEN)
    RCC_BLOCK_USB_OTG_FS   , /**< USB On-The-Go Full Speed */
#endif /* RCC_AHB2ENR_OTGFSEN */
#if defined(RCC_AHB1ENR_OTGHSEN)
    RCC_BLOCK_USB_OTG_HS   , /**< USB On-The-Go High Speed */
    RCC_BLOCK_USB_OTG_HS_ULPI, /**< USB On-The-Go High Speed ULPI */
#endif /* RCC_AHB1ENR_OTGHSEN */

#if defined(RCC_AHB1ENR_ETHMACEN)
    RCC_BLOCK_ETH          , /**< Ethernet MAC */
    RCC_BLOCK_ETH_TX       , /**< Ethernet TX */
    RCC_BLOCK_ETH_RX       , /**< Ethernet RX */
    RCC_BLOCK_ETH_PTP      , /**< Ethernet PTP */
#endif /* RCC_AHB1ENR_ETHMACEN */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(RCC_AHB2ENR_DCMIEN)
    RCC_BLOCK_DCMI         , /**< Digital Camera Media Interface */
#endif /* RCC_AHB2ENR_DCMIEN */
#if defined(RCC_APB2ENR_LTDCEN)
    RCC_BLOCK_LTDC         , /**< LCD-TFT Display Controller */
#endif /* RCC_APB2ENR_LTDCEN */
#if defined(RCC_APB2ENR_DSIEN)
    RCC_BLOCK_DSI          , /**< Display Serial Interface Host */
#endif /* RCC_APB2ENR_DSIEN */
#if defined(RCC_APB2ENR_SAI1EN)
    RCC_BLOCK_SAI1         , /**< Serial Audio Interface 1 */
#endif /* RCC_APB2ENR_SAI1EN */
#if defined(RCC_APB2ENR_SAI2EN)
    RCC_BLOCK_SAI2         , /**< Serial Audio Interface 2 */
#endif /* RCC_APB2ENR_SAI2EN */
#if defined(RCC_APB1ENR_CECEN)
    RCC_BLOCK_CEC          , /**< Consumer Electronics Control */
#endif /* RCC_APB1ENR_CECEN */
#if defined(RCC_APB1ENR_SPDIFRXEN)
    RCC_BLOCK_SPDIFRX      , /**< SPDIF receiver */
#endif /* RCC_APB1ENR_SPDIFRXEN */
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_BLOCK_DFSDM1       , /**< Digital Filter for Sigma-Delta Modulators 1 */
#endif /* RCC_APB2ENR_DFSDM1EN */
#if defined(RCC_APB2ENR_DFSDM2EN)
    RCC_BLOCK_DFSDM2       , /**< Digital Filter for Sigma-Delta Modulators 2 */
#endif /* RCC_APB2ENR_DFSDM2EN */

    /*-------------------------------- Analog --------------------------------*/

    RCC_BLOCK_ADC1         , /**< ADC 1 */
#if defined(RCC_APB2ENR_ADC2EN)
    RCC_BLOCK_ADC2         , /**< ADC 2 */
#endif /* RCC_APB2ENR_ADC2EN */
#if defined(RCC_APB2ENR_ADC3EN)
    RCC_BLOCK_ADC3         , /**< ADC 3 */
#endif /* RCC_APB2ENR_ADC3EN */
#if defined(RCC_APB1ENR_DACEN)
    RCC_BLOCK_DAC          , /**< DAC */
#endif /* RCC_APB1ENR_DACEN */

    /*------------------------------- Security -------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
    RCC_BLOCK_AES          , /**< Advanced Encryption Standard HW accelerator */
#endif /* RCC_AHB2ENR_AESEN */
#if defined(RCC_AHB2ENR_CRYPEN)
    RCC_BLOCK_CRYP         , /**< Cryptographic processor */
#endif /* RCC_AHB2ENR_CRYPEN */
#if defined(RCC_AHB2ENR_HASHEN)
    RCC_BLOCK_HASH         , /**< HASH processor */
#endif /* RCC_AHB2ENR_HASHEN */
#if defined(RCC_AHB1ENR_RNGEN) || defined(RCC_AHB2ENR_RNGEN)
    RCC_BLOCK_RNG          , /**< Random Number Generator */
#endif /* RCC_AHB1ENR_RNGEN OR RCC_AHB2ENR_RNGEN */

    /*------------------------------- Computing ------------------------------*/

    RCC_BLOCK_CRC          , /**< CRC */

    RCC_BLOCK_LIST_CNT
}   rcc_BlockList_t;


/**
 * \brief List of possible clock sources used to read clock frequency.
 *
 */
typedef enum
{
    RCC_CLK_SRC_SYSCLK = 0u, /**< System clock source                                                 */
    RCC_CLK_SRC_PLLPCLK,     /**< Main PLL P output clock source                                      */
    RCC_CLK_SRC_PLLQCLK,     /**< Main PLL Q output clock source                                      */
    RCC_CLK_SRC_PLLRCLK,     /**< Main PLL R output clock source (selected MCUs only)                 */
    RCC_CLK_SRC_PLL48CLK,    /**< 48 MHz clock (USB OTG FS, RNG, SDIO) - main PLL Q or CK48 mux output */
    RCC_CLK_SRC_SDIOCLK,     /**< SDIO kernel clock - PLL48CLK or SYSCLK (SDIO mux output)            */
    RCC_CLK_SRC_AHBCLK,      /**< Advanced High-performance Bus clock source                          */
    RCC_CLK_SRC_APB1CLK,     /**< Advanced Peripheral Bus 1 (APB1) clock source                       */
    RCC_CLK_SRC_APB2CLK,     /**< Advanced Peripheral Bus 2 (APB2) clock source                       */
    RCC_CLK_SRC_APB1TIMCLK,  /**< Timer kernel clock of APB1 timers (PCLK1 x1 / x2 / x4 by APB1 prescaler and TIMPRE) */
    RCC_CLK_SRC_APB2TIMCLK,  /**< Timer kernel clock of APB2 timers (PCLK2 x1 / x2 / x4 by APB2 prescaler and TIMPRE) */
    RCC_CLK_SRC_HSICLK,      /**< 16MHz High Speed Internal (HSI) clock source                        */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock source                                    */
    RCC_CLK_SRC_HSERTCCLK,   /**< High Speed External clock divided by RTC prescaler (RTCPRE)          */
    RCC_CLK_SRC_LSICLK,      /**< 32kHz Low Speed Internal (LSI) oscillator clock source              */
    RCC_CLK_SRC_LSECLK,      /**< Low Speed External clock source                                     */
    RCC_CLK_SRC_CNT          /**< Count of clock sources                                              */
}   rcc_ClkSrcId_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_H */
