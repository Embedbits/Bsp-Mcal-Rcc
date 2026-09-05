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

    RCC_BLOCK_SYSCFG = 0u,  /**< System configuration controller (SYSCFG) */


#if defined(PWR)
    RCC_BLOCK_PWR,          /**< Power control (PWR) */
#endif /* PWR */


#if defined(RCC_APB1ENR1_RTCAPBEN)
    RCC_BLOCK_RTC,          /**< Real-Time Clock (RTC) */
#endif /* RCC_BLOCK_APB1ENR1_RTCAPBEN */
#if defined(WWDG)
    RCC_BLOCK_WWDG,         /**< Window Watchdog (WWDG) */
#endif /* WWDG */


#if defined(DMA1)
    RCC_BLOCK_DMA1,         /**< Direct Memory Access controller 1 (DMA1) */
#endif /* DMA1 */
#if defined(DMA2)
    RCC_BLOCK_DMA2,         /**< Direct Memory Access controller 2 (DMA2) */
#endif /* DMA2 */
#if defined(DMAMUX1)
    RCC_BLOCK_DMAMUX1,      /**< DMA request multiplexer 1 (DMAMUX1) */
#endif /* DMAMUX1 */


#if defined(FLASH)
    RCC_BLOCK_FLASH,        /**< Flash memory interface (FLASH) */
#endif /* FLASH */
#if defined(FMC_R_BASE)
    RCC_BLOCK_FMC,          /**< Flexible Memory Controller (FMC) */
#endif /* FMC_R_BASE */
#if defined(QUADSPI)
    RCC_BLOCK_QUADSPI,      /**< Quad-SPI interface (QUADSPI) */
#endif /* QUADSPI */


#if defined(ADC12_COMMON)
    RCC_BLOCK_ADC12,        /**< Analog-to-Digital Converter common for ADC1/ADC2 (ADC12_COMMON) */
#endif /* ADC12_COMMON */
#if defined(ADC345_COMMON)
    RCC_BLOCK_ADC345,       /**< Analog-to-Digital Converter common for ADC3/ADC4/ADC5 (ADC345_COMMON) */
#endif /* ADC345_COMMON */

#if defined(DAC1)
    RCC_BLOCK_DAC1,         /**< Digital-to-Analog Converter 1 (DAC1) */
#endif /* DAC1 */
#if defined(DAC2)
    RCC_BLOCK_DAC2,         /**< Digital-to-Analog Converter 2 (DAC2) */
#endif /* DAC2 */
#if defined(DAC3)
    RCC_BLOCK_DAC3,         /**< Digital-to-Analog Converter 3 (DAC3) */
#endif /* DAC3 */
#if defined(DAC4)
    RCC_BLOCK_DAC4,         /**< Digital-to-Analog Converter 4 (DAC4) */
#endif /* DAC4 */


#if defined(CORDIC)
    RCC_BLOCK_CORDIC,       /**< Coordinate Rotation Digital Computer accelerator (CORDIC) */
#endif /* CORDIC */
#if defined(FMAC)
    RCC_BLOCK_FMAC,         /**< Filter Math Accelerator (FMAC) */
#endif /* FMAC */


#if defined(CRC)
    RCC_BLOCK_CRC,          /**< Cyclic Redundancy Check calculation unit (CRC) */
#endif /* CRC */
#if defined(AES)
    RCC_BLOCK_AES,          /**< Advanced Encryption Standard accelerator (AES) */
#endif /* AES */
#if defined(RNG)
    RCC_BLOCK_RNG,          /**< Random Number Generator (RNG) */
#endif /* RNG */


#if defined(GPIOA)
    RCC_BLOCK_GPIOA,        /**< General-purpose I/O port A (GPIOA) */
#endif /* GPIOA */
#if defined(GPIOB)
    RCC_BLOCK_GPIOB,        /**< General-purpose I/O port B (GPIOB) */
#endif /* GPIOB */
#if defined(GPIOC)
    RCC_BLOCK_GPIOC,        /**< General-purpose I/O port C (GPIOC) */
#endif /* GPIOC */
#if defined(GPIOD)
    RCC_BLOCK_GPIOD,        /**< General-purpose I/O port D (GPIOD) */
#endif /* GPIOD */
#if defined(GPIOE)
    RCC_BLOCK_GPIOE,        /**< General-purpose I/O port E (GPIOE) */
#endif /* GPIOE */
#if defined(GPIOF)
    RCC_BLOCK_GPIOF,        /**< General-purpose I/O port F (GPIOF) */
#endif /* GPIOF */
#if defined(GPIOG)
    RCC_BLOCK_GPIOG,        /**< General-purpose I/O port G (GPIOG) */
#endif /* GPIOG */


#if defined(TIM1)
    RCC_BLOCK_TIM1,         /**< Timer 1 (TIM1) */
#endif /* TIM1 */
#if defined(TIM2)
    RCC_BLOCK_TIM2,         /**< Timer 2 (TIM2) */
#endif /* TIM2 */
#if defined(TIM3)
    RCC_BLOCK_TIM3,         /**< Timer 3 (TIM3) */
#endif /* TIM3 */
#if defined(TIM4)
    RCC_BLOCK_TIM4,         /**< Timer 4 (TIM4) */
#endif /* TIM4 */
#if defined(TIM5)
    RCC_BLOCK_TIM5,         /**< Timer 5 (TIM5) */
#endif /* TIM5 */
#if defined(TIM6)
    RCC_BLOCK_TIM6,         /**< Timer 6 (TIM6) */
#endif /* TIM6 */
#if defined(TIM7)
    RCC_BLOCK_TIM7,         /**< Timer 7 (TIM7) */
#endif /* TIM7 */
#if defined(TIM8)
    RCC_BLOCK_TIM8,         /**< Timer 8 (TIM8) */
#endif /* TIM8 */
#if defined(TIM15)
    RCC_BLOCK_TIM15,        /**< Timer 15 (TIM15) */
#endif /* TIM15 */
#if defined(TIM16)
    RCC_BLOCK_TIM16,        /**< Timer 16 (TIM16) */
#endif /* TIM16 */
#if defined(TIM17)
    RCC_BLOCK_TIM17,        /**< Timer 17 (TIM17) */
#endif /* TIM17 */
#if defined(TIM20)
    RCC_BLOCK_TIM20,        /**< Timer 20 (TIM20) */
#endif /* TIM20 */
#if defined(HRTIM1)
    RCC_BLOCK_HRTIM,        /**< High-Resolution Timer 1 (HRTIM1) */
#endif /* DSI */
#if defined(LPTIM1)
    RCC_BLOCK_LPTIM1,       /**< Low-Power Timer 1 (LPTIM1) */
#endif /* LPTIM1 */


#if defined(CRS)
    RCC_BLOCK_CRS,          /**< Clock Recovery System (CRS) */
#endif /* CRS */


#if defined(SPI1)
    RCC_BLOCK_SPI1,         /**< Serial Peripheral Interface 1 (SPI1) - system clock as source */
#endif /* SPI1 */
#if defined(SPI2)
    RCC_BLOCK_SPI2_I2S2,    /**< Serial Peripheral Interface 2 (SPI2) / I2S2 - system clock as source */
#endif /* SPI2 */
#if defined(SPI3)
    RCC_BLOCK_SPI3_I2S3,    /**< Serial Peripheral Interface 3 (SPI3) / I2S3 */
#endif /* SPI3 */
#if defined(SPI4)
    RCC_BLOCK_SPI4,         /**< Serial Peripheral Interface 4 (SPI4) */
#endif /* SPI4 */


#if defined(USART1)
    RCC_BLOCK_USART1,       /**< Universal Synchronous/Asynchronous Receiver Transmitter 1 (USART1) */
#endif /* USART1 */
#if defined(USART2)
    RCC_BLOCK_USART2,       /**< Universal Synchronous/Asynchronous Receiver Transmitter 2 (USART2) */
#endif /* USART2 */
#if defined(USART3)
    RCC_BLOCK_USART3,       /**< Universal Synchronous/Asynchronous Receiver Transmitter 3 (USART3) */
#endif /* USART3 */
#if defined(UART4)
    RCC_BLOCK_UART4,        /**< Universal Asynchronous Receiver Transmitter 4 (UART4) */
#endif /* UART4 */
#if defined(UART5)
    RCC_BLOCK_UART5,        /**< Universal Asynchronous Receiver Transmitter 5 (UART5) */
#endif /* USART5 */
#if defined(LPUART1)
    RCC_BLOCK_LPUART1,      /**< Low-Power Universal Asynchronous Receiver Transmitter 1 (LPUART1) */
#endif /* LPUART1 */


#if defined(I2C1)
    RCC_BLOCK_I2C1,         /**< Inter-Integrated Circuit 1 (I2C1) */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_BLOCK_I2C2,         /**< Inter-Integrated Circuit 2 (I2C2) */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_BLOCK_I2C3,         /**< Inter-Integrated Circuit 3 (I2C3) */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_BLOCK_I2C4,         /**< Inter-Integrated Circuit 4 (I2C4) */
#endif /* I2C4 */


#if defined(USB)
    RCC_BLOCK_USB,          /**< Universal Serial Bus peripheral (USB) */
#endif /* USB */

#if defined(FDCAN1)
    RCC_BLOCK_FDCAN1,       /**< Flexible Data-rate Controller Area Network 1 (FDCAN1) */
#endif /* FDCAN1 */

#if defined(SAI1)
    RCC_BLOCK_SAI1,         /**< Serial Audio Interface 1 (SAI1) */
#endif /* SAI1 */

    RCC_BLOCK_LIST_CNT      /**< Count of RCC peripheral block identifiers */
}   rcc_BlockList_t;

/* ========================= SYMBOLIC CONSTANTS ============================= */

/* ========================= EXPORTED MACROS ================================ */

/* ========================= EXPORTED VARIABLES ============================= */

/* ======================== EXPORTED FUNCTIONS ============================== */

#ifdef __cplusplus
}
#endif

#endif /* RCC_RCC_H */
