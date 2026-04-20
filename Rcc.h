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
    RCC_BLOCK_FLASH    = 0u, /**< Flash interface clock enable */
    RCC_BLOCK_SBS          , /**< System Backup and Security module */
    RCC_BLOCK_RTC          , /**< Real Time Clock */
    RCC_BLOCK_CRS          , /**< CRS Clock Enable */

#if defined(DTS)
    RCC_BLOCK_DTS          , /**< Digital Temperature Sensor */
#endif /* DTS */

#if defined(DCACHE1)
    RCC_BLOCK_DCACHE1      , /**< DCACHE1 Clock Enable */
#endif /* DCACHE1 */

    RCC_BLOCK_RAMCFG       , /**< RAMCFG Clock Enable */

#if defined(RAMCFG_BKPRAM)
    RCC_BLOCK_BKPSRAM      , /**< BKPSRAM Clock Enable */
#endif /* RAMCFG_BKPRAM */

#if defined(RAMCFG_SRAM1)
    RCC_BLOCK_SRAM1        , /**< SRAM1 Clock Enable */
#endif /* RAMCFG_SRAM1 */
#if defined(RAMCFG_SRAM2)
    RCC_BLOCK_SRAM2        , /**< SRAM2 Clock Enable */
#endif /* RAMCFG_SRAM2 */
#if defined(RAMCFG_SRAM3)
    RCC_BLOCK_SRAM3        , /**< SRAM3 Clock Enable */
#endif /* RAMCFG_SRAM3 */

#if defined(GPDMA1)
    RCC_BLOCK_GPDMA1       , /**< GPDMA1 Clock Enable */
#endif /* GPDMA1 */
#if defined(GPDMA2)
    RCC_BLOCK_GPDMA2       , /**< GPDMA2 Clock Enable */
#endif /* GPDMA2 */

#if defined(WWDG)
    RCC_BLOCK_WWDG         , /**< WWDG Clock Enable */
#endif /* WWDG */

#if defined(GPIOA)
    RCC_BLOCK_GPIOA        , /**< IO port A Clock Enable */
#endif /* GPIOA */
#if defined(GPIOB)
    RCC_BLOCK_GPIOB        , /**< IO port B Clock Enable */
#endif /* GPIOB */
#if defined(GPIOC)
    RCC_BLOCK_GPIOC        , /**< IO port C Clock Enable */
#endif /* GPIOC */
#if defined(GPIOD)
    RCC_BLOCK_GPIOD        , /**< IO port D Clock Enable */
#endif /* GPIOD */
#if defined(GPIOE)
    RCC_BLOCK_GPIOE        , /**< IO port E Clock Enable */
#endif /* GPIOE */
#if defined(GPIOF)
    RCC_BLOCK_GPIOF        , /**< IO port F Clock Enable */
#endif /* GPIOF */
#if defined(GPIOG)
    RCC_BLOCK_GPIOG        , /**< IO port G Clock Enable */
#endif /* GPIOG */
#if defined(GPIOH)
    RCC_BLOCK_GPIOH        , /**< IO port H Clock Enable */
#endif /* GPIOG */
#if defined(GPIOI)
    RCC_BLOCK_GPIOI        , /**< IO port I Clock Enable */
#endif /* GPIOG */

    /*-------------------------------- Timers --------------------------------*/

#if defined(TIM1)
    RCC_BLOCK_TIM1         , /**< TIM1 Clock Enable */
#endif /* TIM1 */
#if defined(TIM2)
    RCC_BLOCK_TIM2         , /**< TIM2 Clock Enable */
#endif /* TIM2 */
#if defined(TIM3)
    RCC_BLOCK_TIM3         , /**< TIM3 Clock Enable */
#endif /* TIM3 */
#if defined(TIM4)
    RCC_BLOCK_TIM4         , /**< TIM4 Clock Enable */
#endif /* TIM4 */
#if defined(TIM5)
    RCC_BLOCK_TIM5         , /**< TIM5 Clock Enable */
#endif /* TIM5 */
#if defined(TIM6)
    RCC_BLOCK_TIM6         , /**< TIM6 Clock Enable */
#endif /* TIM6 */
#if defined(TIM7)
    RCC_BLOCK_TIM7         , /**< TIM7 Clock Enable */
#endif /* TIM7 */
#if defined(TIM8)
    RCC_BLOCK_TIM8         , /**< TIM8 Clock Enable */
#endif /* TIM8 */
#if defined(TIM12)
    RCC_BLOCK_TIM12        , /**< TIM12 Clock Enable */
#endif /* TIM12 */
#if defined(TIM13)
    RCC_BLOCK_TIM13        , /**< TIM13 Clock Enable */
#endif /* TIM13 */
#if defined(TIM14)
    RCC_BLOCK_TIM14        , /**< TIM14 Clock Enable */
#endif /* TIM14 */
#if defined(TIM15)
    RCC_BLOCK_TIM15        , /**< TIM15 Clock Enable */
#endif /* TIM15 */
#if defined(TIM16)
    RCC_BLOCK_TIM16        , /**< TIM16 Clock Enable */
#endif /* TIM16 */
#if defined(TIM17)
    RCC_BLOCK_TIM17        , /**< TIM17 Clock Enable */
#endif /* TIM17 */


#if defined(LPTIM1)
    RCC_BLOCK_LPTIM1       , /**< Low Power Timer 1. */
#endif /* LPTIM1 */
#if defined(LPTIM2)
    RCC_BLOCK_LPTIM2       , /**< Low Power Timer 2. */
#endif /* LPTIM2 */
#if defined(LPTIM3)
    RCC_BLOCK_LPTIM3       , /**< Low Power Timer 3. */
#endif /* LPTIM3 */
#if defined(LPTIM4)
    RCC_BLOCK_LPTIM4       , /**< Low Power Timer 4 clock enable with APB3 (PCLK3) as clock source. */
#endif /* LPTIM4 */
#if defined(LPTIM5)
    RCC_BLOCK_LPTIM5       , /**< Low Power Timer 5 clock enable with APB3 (PCLK3) as clock source. */
#endif /* LPTIM5 */
#if defined(LPTIM6)
    RCC_BLOCK_LPTIM6       , /**< Low Power Timer 6 clock enable with APB3 (PCLK3) as clock source. */
#endif /* LPTIM6 */

    /*----------------------------- Connectivity -----------------------------*/

#if defined(SPI1)
    RCC_BLOCK_SPI1         , /**< SPI/I2S 1 */
#endif /* SPI1 */
#if defined(SPI2)
    RCC_BLOCK_SPI2         , /**< SPI/I2S 2 */
#endif /* SPI2 */
#if defined(SPI3)
    RCC_BLOCK_SPI3         , /**< SPI/I2S 3 */
#endif /* SPI3 */
#if defined(SPI4)
    RCC_BLOCK_SPI4         , /**< SPI 4 */
#endif /* SPI4 */
#if defined(SPI5)
    RCC_BLOCK_SPI5         , /**< SPI 5 */
#endif /* SPI5 */
#if defined(SPI6)
    RCC_BLOCK_SPI6         , /**< SPI 6 */
#endif /* SPI6 */


#if defined(I2C1)
    RCC_BLOCK_I2C1         , /**< I2C 1 */
#endif /* I2C1 */
#if defined(I2C2)
    RCC_BLOCK_I2C2         , /**< I2C 2 */
#endif /* I2C2 */
#if defined(I2C3)
    RCC_BLOCK_I2C3         , /**< I2C 3 */
#endif /* I2C3 */
#if defined(I2C4)
    RCC_BLOCK_I2C4         , /**< I2C 4 */
#endif /* I2C4 */


#if defined(I3C1)
    RCC_BLOCK_I3C1         , /**< I3C 1 */
#endif /* I3C1 */


#if defined(USART1)
    RCC_BLOCK_USART1       , /**< USART 1 */
#endif /* USART1 */
#if defined(USART2)
    RCC_BLOCK_USART2       , /**< USART 2 */
#endif /* USART2 */
#if defined(USART3)
    RCC_BLOCK_USART3       , /**< USART 3 */
#endif /* USART3 */
#if defined(USART6)
    RCC_BLOCK_USART6       , /**< USART 6 */
#endif /* USART6 */
#if defined(USART10)
    RCC_BLOCK_USART10      , /**< USART 10 */
#endif /* USART10 */
#if defined(USART11)
    RCC_BLOCK_USART11      , /**< USART 11 */
#endif /* USART11 */


#if defined(UART4)
    RCC_BLOCK_UART4        , /**< UART 4 */
#endif /* UART4 */
#if defined(UART5)
    RCC_BLOCK_UART5        , /**< UART 5 */
#endif /* UART5 */
#if defined(UART7)
    RCC_BLOCK_UART7        , /**< UART 7 */
#endif /* UART7 */
#if defined(UART8)
    RCC_BLOCK_UART8        , /**< UART 8 */
#endif /* UART8 */
#if defined(UART9)
    RCC_BLOCK_UART9        , /**< UART 9 */
#endif /* UART9 */
#if defined(UART12)
    RCC_BLOCK_UART12       , /**< UART 12 */
#endif /* UART12 */


#if defined(LPUART1)
    RCC_BLOCK_LPUART1      , /**< Low-Power UART 1  */
#endif /* LPUART1 */

#if defined(FDCAN1)
    RCC_BLOCK_FDCAN        , /**< FD CAN */
#endif /* FDCAN1 */

#if defined(SDMMC1)
    RCC_BLOCK_SDMMC1       , /**< SDMMC1 */
#endif /* SDMMC1 */
#if defined(SDMMC2)
    RCC_BLOCK_SDMMC2       , /**< SDMMC2 */
#endif /* SDMMC2 */

#if defined(FMC_BANK1)
    RCC_BLOCK_FMC          , /**< Flexible Memory Controller (FMC) */
#endif /* FMC_BANK1 */

#if defined(OCTOSPI1)
    RCC_BLOCK_OCTOSPI1     , /**< Octal SPI 1 (OCTOSPI) */
#endif /* OCTOSPI1 */


#if defined(USB_DRD_FS)
    RCC_BLOCK_USB          , /**< USB Full Speed (12Mbps) */
#endif /* USB_OTG_HS */

#if defined(UCPD1)
    RCC_BLOCK_UCPD1        , /**< USB Type-C power delivery 1 (UCPD) */
#endif /* UCPD1 */

#if defined(ETH)
    RCC_BLOCK_ETH          , /**< Ethernet MAC */
    RCC_BLOCK_ETH_TX       , /**< Ethernet TX */
    RCC_BLOCK_ETH_RX       , /**< Ethernet RX */
#endif /* ETH */

    /*------------------------------ Multimedia ------------------------------*/

#if defined(PSSI) || defined(DCMI)
    RCC_BLOCK_DCMI_PSSI    , /**< Digital Camera Media Interface (DCMI) and Parallel Synchronous Slave Interface (PSSI) */
#endif /* PSSI OR DCMI */

#if defined(SAI1)
    RCC_BLOCK_SAI1         , /**< Serial Audio Interface 1 (SAI1) */
#endif /* SAI1 */
#if defined(SAI2)
    RCC_BLOCK_SAI2         , /**< Serial Audio Interface 1 (SAI1) */
#endif /* SAI2 */

#if defined(CEC)
    RCC_BLOCK_CEC          , /**< Consumer Electronics Control (CEC) */
#endif /* CEC */

    /*-------------------------------- Analog --------------------------------*/

#if defined(VREFBUF)
    RCC_BLOCK_VREF         , /**< VREFBUF */
#endif /* VREFBUF */

#if defined(ADC1) || defined(ADC2)
    RCC_BLOCK_ADC          , /**< ADC */
#endif /* ADC1 */

#if defined(DAC1)
    RCC_BLOCK_DAC          , /**< DAC */
#endif /* DAC1 */

    /*------------------------------- Security -------------------------------*/

#if defined(AES)
    RCC_BLOCK_AES          , /**< Advanced Encryption Standard (AES) HW accelerator */
#endif /* AES */

#if defined(GTZC_TZSC1)
    RCC_BLOCK_GTZC1        , /**< Global Trust Zone Controller 1 (GTZC) */
#endif /* GTZC1 */

#if defined(HASH)
    RCC_BLOCK_HASH         , /**< HASH */
#endif /* HASH */

#if defined(OTFDEC1)
    RCC_BLOCK_OTFDEC1      , /**< On-the-fly Decryption Engine 1 (OTFDEC) Clock Enable */
#endif /* OTFDEC1 */

#if defined(PKA)
    RCC_BLOCK_PKA          , /**< Public Key Accelerator (PKA) Clock Enable */
#endif /* PKA */

#if defined(RNG)
    RCC_BLOCK_RNG          , /**< Random Number Generator (RNG) */
#endif /* RNG */

#if defined(SAES)
    RCC_BLOCK_SAES         , /**< Secure Advanced Encryption Standard (SAES) HW accelerator */
#endif /* SAES */

    /*------------------------------- Computing ------------------------------*/

#if defined(CORDIC)
    RCC_BLOCK_CORDIC       , /**< CORDIC Clock Enable */
#endif /* CORDIC */

#if defined(CRC)
    RCC_BLOCK_CRC          , /**< CRC Clock Enable */
#endif /* CRC */

#if defined(FMAC)
    RCC_BLOCK_FMAC         , /**< Filter Math (FMAC) accelerator Clock Enable */
#endif /* FMAC */

    RCC_BLOCK_LIST_CNT
}   rcc_BlockList_t;


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
    RCC_CLK_SRC_HSI64CLK,    /**< 64MHz High Speed Internal (HSI) clock source                        */
    RCC_CLK_SRC_CSI4CLK,     /**< 4MHz Low Power internal RC oscillator (CSI) clock source            */
    RCC_CLK_SRC_HSI48CLK,    /**< 48MHz High Speed Internal (HSI) oscillator clock source             */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock source                                    */
    RCC_CLK_SRC_LSICLK,      /**< 32kHz Low Speed Internal (HSI) oscillator clock source              */
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
