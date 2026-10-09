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
    RCC_BLOCK_FLASH    = 0u, /**< Flash memory interface */
    RCC_BLOCK_SYSCFG       , /**< System configuration controller */
    RCC_BLOCK_PWR          , /**< Power interface */
    RCC_BLOCK_SYSTICK      , /**< SysTick module (no clock enable) */
    RCC_BLOCK_IWDG         , /**< Independent watchdog (clocked by LSI, no clock enable) */
    RCC_BLOCK_RTC          , /**< Real Time Clock (RTCEN in backup domain) */
    RCC_BLOCK_RTCAPB       , /**< Real Time Clock APB interface */
    RCC_BLOCK_SRAM1        , /**< SRAM1 (sleep mode clock only) */
#if defined(RCC_AHB2SMENR_SRAM2SMEN)
    RCC_BLOCK_SRAM2        , /**< SRAM2 (sleep mode clock only) */
#endif /* RCC_AHB2SMENR_SRAM2SMEN */
    RCC_BLOCK_CCMSRAM      , /**< CCM SRAM (sleep mode clock only) */
    RCC_BLOCK_DMA1         , /**< DMA1 */
    RCC_BLOCK_DMA2         , /**< DMA2 */
    RCC_BLOCK_DMAMUX1      , /**< DMA request multiplexer */
    RCC_BLOCK_WWDG         , /**< Window watchdog */
    RCC_BLOCK_CRS          , /**< Clock Recovery System */
    RCC_BLOCK_GPIOA        , /**< IO port A */
    RCC_BLOCK_GPIOB        , /**< IO port B */
    RCC_BLOCK_GPIOC        , /**< IO port C */
    RCC_BLOCK_GPIOD        , /**< IO port D */
    RCC_BLOCK_GPIOE        , /**< IO port E */
    RCC_BLOCK_GPIOF        , /**< IO port F */
    RCC_BLOCK_GPIOG        , /**< IO port G */

    /*-------------------------------- Timers --------------------------------*/

    RCC_BLOCK_TIM1         , /**< TIM1 */
    RCC_BLOCK_TIM2         , /**< TIM2 */
    RCC_BLOCK_TIM3         , /**< TIM3 */
    RCC_BLOCK_TIM4         , /**< TIM4 */
#if defined(RCC_APB1ENR1_TIM5EN)
    RCC_BLOCK_TIM5         , /**< TIM5 */
#endif /* RCC_APB1ENR1_TIM5EN */
    RCC_BLOCK_TIM6         , /**< TIM6 */
    RCC_BLOCK_TIM7         , /**< TIM7 */
    RCC_BLOCK_TIM8         , /**< TIM8 */
    RCC_BLOCK_TIM15        , /**< TIM15 */
    RCC_BLOCK_TIM16        , /**< TIM16 */
    RCC_BLOCK_TIM17        , /**< TIM17 */
#if defined(RCC_APB2ENR_TIM20EN)
    RCC_BLOCK_TIM20        , /**< TIM20 */
#endif /* RCC_APB2ENR_TIM20EN */
#if defined(RCC_APB2ENR_HRTIM1EN)
    RCC_BLOCK_HRTIM1       , /**< High Resolution Timer */
#endif /* RCC_APB2ENR_HRTIM1EN */
    RCC_BLOCK_LPTIM1       , /**< Low Power Timer 1 */

    /*----------------------------- Connectivity -----------------------------*/

    RCC_BLOCK_SPI1         , /**< SPI 1 */
    RCC_BLOCK_SPI2         , /**< SPI/I2S 2 */
#if defined(RCC_APB1ENR1_SPI3EN)
    RCC_BLOCK_SPI3         , /**< SPI/I2S 3 */
#endif /* RCC_APB1ENR1_SPI3EN */
#if defined(RCC_APB2ENR_SPI4EN)
    RCC_BLOCK_SPI4         , /**< SPI 4 */
#endif /* RCC_APB2ENR_SPI4EN */
#if defined(RCC_CCIPR_I2S23SEL)
    RCC_BLOCK_I2S23        , /**< I2S kernel clock of SPI2 / SPI3 (no clock enable) */
#endif /* RCC_CCIPR_I2S23SEL */
    RCC_BLOCK_I2C1         , /**< I2C 1 */
    RCC_BLOCK_I2C2         , /**< I2C 2 */
#if defined(RCC_APB1ENR1_I2C3EN)
    RCC_BLOCK_I2C3         , /**< I2C 3 */
#endif /* RCC_APB1ENR1_I2C3EN */
#if defined(RCC_APB1ENR2_I2C4EN)
    RCC_BLOCK_I2C4         , /**< I2C 4 */
#endif /* RCC_APB1ENR2_I2C4EN */
    RCC_BLOCK_USART1       , /**< USART 1 */
    RCC_BLOCK_USART2       , /**< USART 2 */
#if defined(RCC_APB1ENR1_USART3EN)
    RCC_BLOCK_USART3       , /**< USART 3 */
#endif /* RCC_APB1ENR1_USART3EN */
    RCC_BLOCK_UART4        , /**< UART 4 */
#if defined(RCC_APB1ENR1_UART5EN)
    RCC_BLOCK_UART5        , /**< UART 5 */
#endif /* RCC_APB1ENR1_UART5EN */
    RCC_BLOCK_LPUART1      , /**< Low-Power UART 1 */
    RCC_BLOCK_FDCAN        , /**< FDCAN (all instances) */
#if defined(RCC_APB1ENR1_USBEN)
    RCC_BLOCK_USB          , /**< USB device */
#endif /* RCC_APB1ENR1_USBEN */
#if defined(RCC_APB1ENR2_UCPD1EN)
    RCC_BLOCK_UCPD1        , /**< USB Type-C / Power Delivery interface */
#endif /* RCC_APB1ENR2_UCPD1EN */
#if defined(RCC_AHB3ENR_FMCEN)
    RCC_BLOCK_FMC          , /**< Flexible Memory Controller */
#endif /* RCC_AHB3ENR_FMCEN */
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_BLOCK_QSPI         , /**< Quad SPI memory interface */
#endif /* RCC_AHB3ENR_QSPIEN */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(RCC_APB2ENR_SAI1EN)
    RCC_BLOCK_SAI1         , /**< Serial Audio Interface 1 */
#endif /* RCC_APB2ENR_SAI1EN */

    /*-------------------------------- Analog --------------------------------*/

    RCC_BLOCK_ADC12        , /**< ADC 1 and ADC 2 */
#if defined(RCC_AHB2ENR_ADC345EN)
    RCC_BLOCK_ADC345       , /**< ADC 3, ADC 4 and ADC 5 */
#endif /* RCC_AHB2ENR_ADC345EN */
    RCC_BLOCK_DAC1         , /**< DAC 1 */
#if defined(RCC_AHB2ENR_DAC2EN)
    RCC_BLOCK_DAC2         , /**< DAC 2 */
#endif /* RCC_AHB2ENR_DAC2EN */
    RCC_BLOCK_DAC3         , /**< DAC 3 */
#if defined(RCC_AHB2ENR_DAC4EN)
    RCC_BLOCK_DAC4         , /**< DAC 4 */
#endif /* RCC_AHB2ENR_DAC4EN */

    /*------------------------------- Security -------------------------------*/

#if defined(RCC_AHB2ENR_AESEN)
    RCC_BLOCK_AES          , /**< Advanced Encryption Standard HW accelerator */
#endif /* RCC_AHB2ENR_AESEN */
    RCC_BLOCK_RNG          , /**< Random Number Generator */

    /*------------------------------- Computing ------------------------------*/

    RCC_BLOCK_CORDIC       , /**< CORDIC co-processor */
    RCC_BLOCK_CRC          , /**< CRC */
    RCC_BLOCK_FMAC         , /**< Filter Math accelerator */

    RCC_BLOCK_LIST_CNT
}   rcc_BlockList_t;


/**
 * \brief List of possible clock sources used to read clock frequency.
 *
 */
typedef enum
{
    RCC_CLK_SRC_SYSCLK = 0u, /**< System clock source                                          */
    RCC_CLK_SRC_PLLPCLK,     /**< PLL P output clock source                                    */
    RCC_CLK_SRC_PLLQCLK,     /**< PLL Q output clock source                                    */
    RCC_CLK_SRC_PLLRCLK,     /**< PLL R output clock source                                    */
    RCC_CLK_SRC_AHBCLK,      /**< Advanced High-performance Bus clock source (HCLK)            */
    RCC_CLK_SRC_APB1CLK,     /**< Advanced Peripheral Bus 1 (APB1) clock source                */
    RCC_CLK_SRC_APB2CLK,     /**< Advanced Peripheral Bus 2 (APB2) clock source                */
    RCC_CLK_SRC_APB1TIMCLK,  /**< Timer kernel clock of APB1 timers (PCLK1 x1 / x2)            */
    RCC_CLK_SRC_APB2TIMCLK,  /**< Timer kernel clock of APB2 timers (PCLK2 x1 / x2)            */
    RCC_CLK_SRC_HSICLK,      /**< 16MHz High Speed Internal (HSI16) clock source               */
    RCC_CLK_SRC_HSI48CLK,    /**< 48MHz High Speed Internal (HSI48) clock source               */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock source                             */
    RCC_CLK_SRC_HSERTCCLK,   /**< High Speed External clock divided by 32 (RTC clock)          */
    RCC_CLK_SRC_LSICLK,      /**< 32kHz Low Speed Internal (LSI) oscillator clock source       */
    RCC_CLK_SRC_LSECLK,      /**< Low Speed External clock source                              */
    RCC_CLK_SRC_CNT          /**< Count of clock sources                                       */
}   rcc_ClkSrcId_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_H */
