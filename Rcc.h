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

/** \brief RCC peripheral blocks IDs (clock enable, reset and sleep mode control
 *  of one peripheral). Blocks not available on the selected device are not
 *  defined. */
typedef enum rcc_BlockList_t
{
    /*------------------------- System core (no clock enable) ----------------*/
    RCC_BLOCK_SYSTICK       = 0u, /**< SysTick module (no clock enable)                     */
    RCC_BLOCK_IWDG              , /**< Independent watchdog (clocked by LSI, no clock enable) */
    RCC_BLOCK_CKPER             , /**< Peripheral clock per_ck (CKPER, no clock enable)      */
    RCC_BLOCK_TRACE             , /**< Trace clock TRACECLKIN (no clock enable)              */
    /*------------------------------ System core ------------------------------*/
#if defined(RCC_APB4ENR_SYSCFGEN)
    RCC_BLOCK_SYSCFG        , /**< System configuration controller */
#endif
#if defined(RCC_APB4ENR_SBSEN)
    RCC_BLOCK_SBS           , /**< System configuration, boot and security */
#endif
#if defined(RCC_AHB3ENR_FLASHEN)
    RCC_BLOCK_FLASH         , /**< Flash memory interface */
#endif
#if defined(RCC_AHB1ENR_ARTEN)
    RCC_BLOCK_ART           , /**< ART accelerator */
#endif
#if defined(RCC_AHB2ENR_HSEMEN) || \
    defined(RCC_AHB4ENR_HSEMEN)
    RCC_BLOCK_HSEM          , /**< Hardware semaphore */
#endif
#if defined(RCC_AHB3ENR_IOMNGREN)
    RCC_BLOCK_IOMNGR        , /**< OCTOSPI I/O manager */
#endif
    RCC_BLOCK_CRS           , /**< Clock recovery system */
    RCC_BLOCK_RTCAPB        , /**< RTC APB interface */
#if defined(RCC_APB3ENR_WWDG1EN)
    RCC_BLOCK_WWDG1         , /**< Window watchdog 1 */
#endif
#if defined(RCC_APB1LENR_WWDG2EN)
    RCC_BLOCK_WWDG2         , /**< Window watchdog 2 (Cortex-M4 core) */
#endif
#if defined(RCC_APB1ENR1_WWDGEN) || \
    defined(RCC_APB3ENR_WWDGEN)
    RCC_BLOCK_WWDG          , /**< Window watchdog */
#endif
#if defined(RCC_APB4ENR_DTSEN)
    RCC_BLOCK_DTS           , /**< Digital temperature sensor */
#endif
    RCC_BLOCK_BKPRAM        , /**< Backup SRAM */
#if defined(RCC_AHB3ENR_AXISRAMEN)
    RCC_BLOCK_AXISRAM       , /**< AXI SRAM */
#endif
#if defined(RCC_AHB3ENR_ITCMEN)
    RCC_BLOCK_ITCM          , /**< ITCM RAM */
#endif
#if defined(RCC_AHB3ENR_DTCM1EN)
    RCC_BLOCK_DTCM1         , /**< DTCM 1 RAM */
#endif
#if defined(RCC_AHB3ENR_DTCM2EN)
    RCC_BLOCK_DTCM2         , /**< DTCM 2 RAM */
#endif
#if defined(RCC_AHB2ENR_SRAM1EN)
    RCC_BLOCK_SRAM1         , /**< AHB SRAM 1 (D2 domain) */
#endif
#if defined(RCC_AHB2ENR_SRAM2EN)
    RCC_BLOCK_SRAM2         , /**< AHB SRAM 2 (D2 domain) */
#endif
#if defined(RCC_AHB2ENR_SRAM3EN)
    RCC_BLOCK_SRAM3         , /**< D2 domain SRAM 3 */
#endif
#if defined(RCC_AHB2ENR_AHBSRAM1EN)
    RCC_BLOCK_AHBSRAM1      , /**< CD domain AHB SRAM 1 */
#endif
#if defined(RCC_AHB2ENR_AHBSRAM2EN)
    RCC_BLOCK_AHBSRAM2      , /**< CD domain AHB SRAM 2 */
#endif
#if defined(RCC_AHB4ENR_SRDSRAMEN)
    RCC_BLOCK_SRDSRAM       , /**< SRD domain SRAM */
#endif

    /*------------------------------ DMA --------------------------------------*/
#if defined(RCC_AHB1ENR_DMA1EN)
    RCC_BLOCK_DMA1          , /**< DMA 1 */
#endif
#if defined(RCC_AHB1ENR_DMA2EN)
    RCC_BLOCK_DMA2          , /**< DMA 2 */
#endif
#if defined(RCC_AHB4ENR_BDMAEN)
    RCC_BLOCK_BDMA          , /**< Basic DMA (D3 domain) */
#endif
#if defined(RCC_AHB2ENR_BDMA1EN)
    RCC_BLOCK_BDMA1         , /**< Basic DMA 1 (DFSDM) */
#endif
#if defined(RCC_AHB4ENR_BDMA2EN)
    RCC_BLOCK_BDMA2         , /**< Basic DMA 2 (SRD domain) */
#endif
#if defined(RCC_AHB3ENR_MDMAEN)
    RCC_BLOCK_MDMA          , /**< Master DMA */
#endif
#if defined(RCC_AHB1ENR_GPDMA1EN)
    RCC_BLOCK_GPDMA1        , /**< General purpose DMA 1 */
#endif
#if defined(RCC_AHB5ENR_HPDMA1EN)
    RCC_BLOCK_HPDMA1        , /**< High performance DMA 1 */
#endif
    RCC_BLOCK_DMA2D         , /**< Chrom-ART accelerator */

    /*------------------------------ GPIO -------------------------------------*/
    RCC_BLOCK_GPIOA         , /**< IO port A */
    RCC_BLOCK_GPIOB         , /**< IO port B */
    RCC_BLOCK_GPIOC         , /**< IO port C */
    RCC_BLOCK_GPIOD         , /**< IO port D */
    RCC_BLOCK_GPIOE         , /**< IO port E */
    RCC_BLOCK_GPIOF         , /**< IO port F */
    RCC_BLOCK_GPIOG         , /**< IO port G */
    RCC_BLOCK_GPIOH         , /**< IO port H */
#if defined(RCC_AHB4ENR_GPIOIEN)
    RCC_BLOCK_GPIOI         , /**< IO port I */
#endif
#if defined(RCC_AHB4ENR_GPIOJEN)
    RCC_BLOCK_GPIOJ         , /**< IO port J */
#endif
#if defined(RCC_AHB4ENR_GPIOKEN)
    RCC_BLOCK_GPIOK         , /**< IO port K */
#endif
#if defined(RCC_AHB4ENR_GPIOMEN)
    RCC_BLOCK_GPIOM         , /**< IO port M */
#endif
#if defined(RCC_AHB4ENR_GPIONEN)
    RCC_BLOCK_GPION         , /**< IO port N */
#endif
#if defined(RCC_AHB4ENR_GPIOOEN)
    RCC_BLOCK_GPIOO         , /**< IO port O */
#endif
#if defined(RCC_AHB4ENR_GPIOPEN)
    RCC_BLOCK_GPIOP         , /**< IO port P */
#endif

    /*------------------------------ Timers -----------------------------------*/
    RCC_BLOCK_TIM1          , /**< Timer 1 */
    RCC_BLOCK_TIM2          , /**< Timer 2 */
    RCC_BLOCK_TIM3          , /**< Timer 3 */
    RCC_BLOCK_TIM4          , /**< Timer 4 */
    RCC_BLOCK_TIM5          , /**< Timer 5 */
    RCC_BLOCK_TIM6          , /**< Timer 6 */
    RCC_BLOCK_TIM7          , /**< Timer 7 */
#if defined(RCC_APB2ENR_TIM8EN)
    RCC_BLOCK_TIM8          , /**< Timer 8 */
#endif
#if defined(RCC_APB2ENR_TIM9EN)
    RCC_BLOCK_TIM9          , /**< Timer 9 */
#endif
    RCC_BLOCK_TIM12         , /**< Timer 12 */
    RCC_BLOCK_TIM13         , /**< Timer 13 */
    RCC_BLOCK_TIM14         , /**< Timer 14 */
    RCC_BLOCK_TIM15         , /**< Timer 15 */
    RCC_BLOCK_TIM16         , /**< Timer 16 */
    RCC_BLOCK_TIM17         , /**< Timer 17 */
#if defined(RCC_APB1HENR_TIM23EN)
    RCC_BLOCK_TIM23         , /**< Timer 23 */
#endif
#if defined(RCC_APB1HENR_TIM24EN)
    RCC_BLOCK_TIM24         , /**< Timer 24 */
#endif
#if defined(RCC_APB2ENR_HRTIMEN)
    RCC_BLOCK_HRTIM         , /**< High resolution timer */
#endif
    RCC_BLOCK_LPTIM1        , /**< Low power Timer 1 */
    RCC_BLOCK_LPTIM2        , /**< Low power Timer 2 */
    RCC_BLOCK_LPTIM3        , /**< Low power Timer 3 */
#if defined(RCC_APB4ENR_LPTIM4EN)
    RCC_BLOCK_LPTIM4        , /**< Low power Timer 4 */
#endif
#if defined(RCC_APB4ENR_LPTIM5EN)
    RCC_BLOCK_LPTIM5        , /**< Low power Timer 5 */
#endif

    /*------------------------------ Connectivity -----------------------------*/
    RCC_BLOCK_SPI1          , /**< SPI / I2S 1 */
    RCC_BLOCK_SPI2          , /**< SPI / I2S 2 */
    RCC_BLOCK_SPI3          , /**< SPI / I2S 3 */
    RCC_BLOCK_SPI4          , /**< SPI / I2S 4 */
    RCC_BLOCK_SPI5          , /**< SPI / I2S 5 */
    RCC_BLOCK_SPI6          , /**< SPI / I2S 6 */
    RCC_BLOCK_I2C1          , /**< I2C 1 */
    RCC_BLOCK_I2C2          , /**< I2C 2 */
    RCC_BLOCK_I2C3          , /**< I2C 3 */
#if defined(RCC_APB4ENR_I2C4EN)
    RCC_BLOCK_I2C4          , /**< I2C 4 */
#endif
#if defined(RCC_APB1LENR_I2C5EN)
    RCC_BLOCK_I2C5          , /**< I2C 5 */
#endif
    RCC_BLOCK_USART1        , /**< USART 1 */
    RCC_BLOCK_USART2        , /**< USART 2 */
    RCC_BLOCK_USART3        , /**< USART 3 */
    RCC_BLOCK_UART4         , /**< UART 4 */
    RCC_BLOCK_UART5         , /**< UART 5 */
#if defined(RCC_APB2ENR_USART6EN)
    RCC_BLOCK_USART6        , /**< USART 6 */
#endif
    RCC_BLOCK_UART7         , /**< UART 7 */
    RCC_BLOCK_UART8         , /**< UART 8 */
#if defined(RCC_APB2ENR_UART9EN)
    RCC_BLOCK_UART9         , /**< UART 9 */
#endif
#if defined(RCC_APB2ENR_USART10EN)
    RCC_BLOCK_USART10       , /**< USART 10 */
#endif
    RCC_BLOCK_LPUART1       , /**< Low power UART 1 */
    RCC_BLOCK_FDCAN         , /**< FDCAN */
    RCC_BLOCK_SDMMC1        , /**< SDMMC 1 */
    RCC_BLOCK_SDMMC2        , /**< SDMMC 2 */
    RCC_BLOCK_FMC           , /**< Flexible memory controller */
#if defined(RCC_AHB3ENR_QSPIEN)
    RCC_BLOCK_QSPI          , /**< Quad SPI */
#endif
#if defined(RCC_AHB3ENR_OSPI1EN)
    RCC_BLOCK_OSPI1         , /**< Octo SPI 1 */
#endif
#if defined(RCC_AHB3ENR_OSPI2EN)
    RCC_BLOCK_OSPI2         , /**< Octo SPI 2 */
#endif
#if defined(RCC_AHB3ENR_OTFDEC1EN)
    RCC_BLOCK_OTFDEC1       , /**< On-the-fly decryption 1 */
#endif
#if defined(RCC_AHB3ENR_OTFDEC2EN)
    RCC_BLOCK_OTFDEC2       , /**< On-the-fly decryption 2 */
#endif
#if defined(RCC_AHB5ENR_XSPI1EN)
    RCC_BLOCK_XSPI1         , /**< Extended SPI 1 */
#endif
#if defined(RCC_AHB5ENR_XSPI2EN)
    RCC_BLOCK_XSPI2         , /**< Extended SPI 2 */
#endif
#if defined(RCC_AHB5ENR_XSPIMEN)
    RCC_BLOCK_XSPIM         , /**< Extended SPI I/O manager */
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSEN)
    RCC_BLOCK_USB1OTGHS     , /**< USB 1 OTG HS */
#endif
#if defined(RCC_AHB1ENR_USB1OTGHSULPIEN)
    RCC_BLOCK_USB1OTGHSULPI , /**< USB 1 OTG HS ULPI */
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSEN)
    RCC_BLOCK_USB2OTGFS     , /**< USB 2 OTG FS */
#endif
#if defined(RCC_AHB1ENR_USB2OTGFSULPIEN)
    RCC_BLOCK_USB2OTGFSULPI , /**< USB 2 OTG FS ULPI */
#endif
#if defined(RCC_AHB1ENR_OTGHSEN)
    RCC_BLOCK_OTGHS         , /**< USB OTG HS */
#endif
#if defined(RCC_AHB1ENR_OTGFSEN)
    RCC_BLOCK_OTGFS         , /**< USB OTG FS */
#endif
#if defined(RCC_AHB1ENR_USBPHYCEN)
    RCC_BLOCK_USBPHYC       , /**< USB HS PHY controller */
#endif
#if defined(RCC_APB1ENR2_UCPD1EN)
    RCC_BLOCK_UCPD1         , /**< USB Type-C power delivery 1 */
#endif
#if defined(RCC_AHB1ENR_ETH1MACEN)
    RCC_BLOCK_ETH1MAC       , /**< Ethernet MAC */
#endif
#if defined(RCC_AHB1ENR_ETH1TXEN)
    RCC_BLOCK_ETH1TX        , /**< Ethernet transmission */
#endif
#if defined(RCC_AHB1ENR_ETH1RXEN)
    RCC_BLOCK_ETH1RX        , /**< Ethernet reception */
#endif
    RCC_BLOCK_MDIOS         , /**< MDIO slave */
#if defined(RCC_APB1HENR_SWPMIEN)
    RCC_BLOCK_SWPMI         , /**< Single wire protocol master interface */
#endif
    RCC_BLOCK_CEC           , /**< HDMI-CEC */
    RCC_BLOCK_SPDIFRX       , /**< SPDIF receiver */
#if defined(RCC_APB2ENR_DFSDM1EN)
    RCC_BLOCK_DFSDM1        , /**< DFSDM 1 */
#endif
#if defined(RCC_APB4ENR_DFSDM2EN)
    RCC_BLOCK_DFSDM2        , /**< DFSDM 2 */
#endif

    /*------------------------------ Multimedia -------------------------------*/
    RCC_BLOCK_SAI1          , /**< Serial audio interface 1 */
#if defined(RCC_APB2ENR_SAI2EN)
    RCC_BLOCK_SAI2          , /**< Serial audio interface 2 */
#endif
#if defined(RCC_APB2ENR_SAI3EN)
    RCC_BLOCK_SAI3          , /**< Serial audio interface 3 */
#endif
#if defined(RCC_APB4ENR_SAI4EN)
    RCC_BLOCK_SAI4          , /**< Serial audio interface 4 */
#endif
#if defined(RCC_AHB1ENR_ADF1EN)
    RCC_BLOCK_ADF1          , /**< Audio digital filter 1 */
#endif
#if defined(RCC_AHB2ENR_DCMIEN)
    RCC_BLOCK_DCMI          , /**< Digital camera interface */
#endif
#if defined(RCC_AHB2ENR_DCMI_PSSIEN)
    RCC_BLOCK_DCMI_PSSI     , /**< Digital camera interface / PSSI */
#endif
#if defined(RCC_APB5ENR_DCMIPPEN)
    RCC_BLOCK_DCMIPP        , /**< Digital camera interface pixel pipeline */
#endif
#if defined(RCC_AHB2ENR_PSSIEN)
    RCC_BLOCK_PSSI          , /**< Parallel synchronous slave interface */
#endif
#if defined(RCC_APB3ENR_LTDCEN) || \
    defined(RCC_APB5ENR_LTDCEN)
    RCC_BLOCK_LTDC          , /**< LCD-TFT controller */
#endif
#if defined(RCC_APB3ENR_DSIEN)
    RCC_BLOCK_DSI           , /**< DSI host */
#endif
#if defined(RCC_AHB3ENR_JPGDECEN)
    RCC_BLOCK_JPGDEC        , /**< JPEG codec */
#endif
#if defined(RCC_AHB5ENR_JPEGEN)
    RCC_BLOCK_JPEG          , /**< JPEG codec */
#endif
#if defined(RCC_AHB3ENR_GFXMMUEN) || \
    defined(RCC_AHB5ENR_GFXMMUEN)
    RCC_BLOCK_GFXMMU        , /**< Graphic MMU */
#endif
#if defined(RCC_APB5ENR_GFXTIMEN)
    RCC_BLOCK_GFXTIM        , /**< Graphic timer */
#endif
#if defined(RCC_AHB5ENR_GPU2DEN)
    RCC_BLOCK_GPU2D         , /**< Graphic processing unit 2D */
#endif

    /*------------------------------ Analog -----------------------------------*/
    RCC_BLOCK_ADC12         , /**< ADC 1 / 2 */
#if defined(RCC_AHB4ENR_ADC3EN)
    RCC_BLOCK_ADC3          , /**< ADC 3 */
#endif
#if defined(RCC_APB1LENR_DAC12EN)
    RCC_BLOCK_DAC12         , /**< DAC 1 */
#endif
#if defined(RCC_APB4ENR_DAC2EN)
    RCC_BLOCK_DAC2          , /**< DAC 2 */
#endif
#if defined(RCC_APB4ENR_COMP12EN)
    RCC_BLOCK_COMP12        , /**< Comparators 1 / 2 */
#endif
#if defined(RCC_APB1HENR_OPAMPEN)
    RCC_BLOCK_OPAMP         , /**< Operational amplifiers */
#endif
    RCC_BLOCK_VREF          , /**< Voltage reference buffer */

    /*------------------------------ Security ---------------------------------*/
#if defined(RCC_AHB2ENR_CRYPEN) || \
    defined(RCC_AHB3ENR_CRYPEN)
    RCC_BLOCK_CRYP          , /**< Cryptographic processor */
#endif
#if defined(RCC_AHB2ENR_HASHEN) || \
    defined(RCC_AHB3ENR_HASHEN)
    RCC_BLOCK_HASH          , /**< Hash processor */
#endif
    RCC_BLOCK_RNG           , /**< Random number generator */
#if defined(RCC_AHB3ENR_PKAEN)
    RCC_BLOCK_PKA           , /**< Public key accelerator */
#endif
#if defined(RCC_AHB3ENR_SAESEN)
    RCC_BLOCK_SAES          , /**< Secure AES coprocessor */
#endif

    /*------------------------------ Computing --------------------------------*/
    RCC_BLOCK_CRC           , /**< CRC calculation unit */
#if defined(RCC_AHB2ENR_CORDICEN)
    RCC_BLOCK_CORDIC        , /**< CORDIC co-processor */
#endif
#if defined(RCC_AHB2ENR_FMACEN)
    RCC_BLOCK_FMAC          , /**< Filter math accelerator */
#endif

    RCC_BLOCK_LIST_CNT
}   rcc_BlockList_t;


/**
 * \brief List of possible clock sources used to read clock frequency.
 *
 */
typedef enum
{
    RCC_CLK_SRC_SYSCLK = 0u, /**< System clock source                                                 */
    RCC_CLK_SRC_CPUCLK,      /**< CPU clock (SYSCLK divided by system clock prescaler D1CPRE / CDCPRE) */
    RCC_CLK_SRC_PLL1RCLK,    /**< PLL 1 R output clock source                                         */
    RCC_CLK_SRC_PLL1QCLK,    /**< PLL 1 Q output clock source                                         */
    RCC_CLK_SRC_PLL1PCLK,    /**< PLL 1 P output clock source                                         */
    RCC_CLK_SRC_PLL2RCLK,    /**< PLL 2 R output clock source                                         */
    RCC_CLK_SRC_PLL2QCLK,    /**< PLL 2 Q output clock source                                         */
    RCC_CLK_SRC_PLL2PCLK,    /**< PLL 2 P output clock source                                         */
    RCC_CLK_SRC_PLL3RCLK,    /**< PLL 3 R output clock source                                         */
    RCC_CLK_SRC_PLL3QCLK,    /**< PLL 3 Q output clock source                                         */
    RCC_CLK_SRC_PLL3PCLK,    /**< PLL 3 P output clock source                                         */
#if defined(STM32H7RS)
    RCC_CLK_SRC_PLL1SCLK,    /**< PLL 1 S output clock source (STM32H7R / H7S)                        */
    RCC_CLK_SRC_PLL2SCLK,    /**< PLL 2 S output clock source (STM32H7R / H7S)                        */
    RCC_CLK_SRC_PLL2TCLK,    /**< PLL 2 T output clock source (STM32H7R / H7S)                        */
    RCC_CLK_SRC_PLL3SCLK,    /**< PLL 3 S output clock source (STM32H7R / H7S)                        */
#endif
    RCC_CLK_SRC_AHBCLK,      /**< Advanced High-performance Bus clock source (HCLK)                   */
    RCC_CLK_SRC_APB1CLK,     /**< Advanced Peripheral Bus 1 (APB1) clock source                       */
    RCC_CLK_SRC_APB2CLK,     /**< Advanced Peripheral Bus 2 (APB2) clock source                       */
#if defined(STM32H7RS)
    RCC_CLK_SRC_APB4CLK,     /**< Advanced Peripheral Bus 4 (APB4) clock source                       */
    RCC_CLK_SRC_APB5CLK,     /**< Advanced Peripheral Bus 5 (APB5) clock source (STM32H7R / H7S)      */
#else
    RCC_CLK_SRC_APB3CLK,     /**< Advanced Peripheral Bus 3 (APB3) clock source                       */
    RCC_CLK_SRC_APB4CLK,     /**< Advanced Peripheral Bus 4 (APB4) clock source                       */
#endif
    RCC_CLK_SRC_APB1TIMCLK,  /**< Timer kernel clock of APB1 timers (by APB1 prescaler and TIMPRE)    */
    RCC_CLK_SRC_APB2TIMCLK,  /**< Timer kernel clock of APB2 timers (by APB2 prescaler and TIMPRE)    */
    RCC_CLK_SRC_HSI64CLK,    /**< High Speed Internal (HSI) clock source (64 MHz / HSIDIV)            */
    RCC_CLK_SRC_CSI4CLK,     /**< 4MHz Low Power internal RC oscillator (CSI) clock source            */
    RCC_CLK_SRC_CSIDIV122CLK,/**< CSI clock divided by 122 (HDMI-CEC kernel clock)                    */
    RCC_CLK_SRC_HSI48CLK,    /**< 48MHz High Speed Internal (HSI) oscillator clock source             */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock source                                    */
#if defined(STM32H7RS)
    RCC_CLK_SRC_HSEDIV2CLK,  /**< High Speed External clock divided by 2 (USB PHY reference, STM32H7R / H7S) */
#endif
    RCC_CLK_SRC_LSICLK,      /**< 32kHz Low Speed Internal (LSI) oscillator clock source              */
    RCC_CLK_SRC_LSECLK,      /**< Low Speed External clock source                                     */
    RCC_CLK_SRC_PERCLK,      /**< Peripheral clock per_ck (CKPER: HSI / CSI / HSE)                    */
    RCC_CLK_SRC_RTCHSECLK,   /**< HSE divided by RTC prescaler (RTCPRE)                               */
    RCC_CLK_SRC_PINCLK,      /**< External clock input (I2S_CKIN, SPDIF symbol clock, DSI PHY, USB PHY 48 MHz) - frequency unknown */
    RCC_CLK_SRC_TRACECLK,    /**< Trace clock TRACECLKIN (PLL1 R output with PLL1 system clock, otherwise SYSCLK) */
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
