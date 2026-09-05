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
    RCC_PERIPH_SYSCFG = 0u, /**< APB2 GRP1 bus */


#if defined(PWR)
    RCC_PERIPH_PWR,         /**< APB1 GRP1 bus */
#endif /* PWR */


#if defined(RCC_APB1ENR1_RTCAPBEN)
    RCC_PERIPH_RTC_HSE_DIV32, /**< RTC clock source HSE divided by 32 */
    RCC_PERIPH_RTC_LSE, /**< RTC clock source LSE */
    RCC_PERIPH_RTC_LSI, /**< RTC clock source LSI */
#endif /* RCC_PERIPH_APB1ENR1_RTCAPBEN */
#if defined(WWDG)
    RCC_PERIPH_WWDG,        /**< APB1 GRP1 bus */
#endif /* WWDG */


#if defined(DMA1)
    RCC_PERIPH_DMA1,        /**< AHB1 GRP1 bus */
#endif /* DMA1 */
#if defined(DMA2)
    RCC_PERIPH_DMA2,        /**< AHB1 GRP1 bus */
#endif /* DMA2 */
#if defined(DMAMUX1)
    RCC_PERIPH_DMAMUX1,     /**< AHB1 GRP1 bus */
#endif /* DMAMUX1 */


#if defined(FLASH)
    RCC_PERIPH_FLASH,       /**< AHB1 GRP1 bus */
#endif /* FLASH */
#if defined(FMC_R_BASE)
    RCC_PERIPH_FMC,         /**< AHB3 GRP1 bus */
#endif /* FMC_R_BASE */
#if defined(QUADSPI)
    /** System clock used as QSPI clock source (default) */
    RCC_PERIPH_QUADSPI_SYSCLK,
    /** PLL Q output used as QSPI clock source */
    RCC_PERIPH_QUADSPI_PLLQ,
    /** High Speed Internal 16MHz oscillator used as QSPI clock source */
    RCC_PERIPH_QUADSPI_HSI16, /**< AHB3 GRP1 bus */
#endif /* QUADSPI */


#if defined(ADC12_COMMON)
    /** System clock used as ADC 1&2 clock source (default) */
    RCC_PERIPH_ADC12_SYSCLK,
    /** PLL P output used as ADC 1&2 clock source */
    RCC_PERIPH_ADC12_PLLP,
#endif /* ADC12_COMMON */
#if defined(ADC345_COMMON)
    /** System clock used as ADC 3&4&5 clock source (default) */
    RCC_PERIPH_ADC345_SYSCLK,
    /** PLL P output used as ADC 3&4&5 clock source */
    RCC_PERIPH_ADC345_PLLP,
#endif /* ADC345_COMMON */

#if defined(DAC1)
    RCC_PERIPH_DAC1,        /**< AHB2 GRP1 bus */
#endif /* DAC1 */
#if defined(DAC2)
    RCC_PERIPH_DAC2,        /**< AHB2 GRP1 bus */
#endif /* DAC2 */
#if defined(DAC3)
    RCC_PERIPH_DAC3,        /**< AHB2 GRP1 bus */
#endif /* DAC3 */
#if defined(DAC4)
    RCC_PERIPH_DAC4,        /**< AHB2 GRP1 bus */
#endif /* DAC4 */


#if defined(CORDIC)
    RCC_PERIPH_CORDIC,      /**< AHB1 GRP1 bus */
#endif /* CORDIC */
#if defined(FMAC)
    RCC_PERIPH_FMAC,        /**< AHB1 GRP1 bus */
#endif /* FMAC */


#if defined(CRC)
    RCC_PERIPH_CRC,         /**< AHB1 GRP1 bus */
#endif /* CRC */
#if defined(AES)
    RCC_PERIPH_AES,         /**< AHB2 GRP1 bus */
#endif /* AES */
#if defined(RNG)
    /** PLL Q output used as USB and RNG clock source (default) */
    RCC_PERIPH_RNG_PLLQ,
    /** High Speed Internal 48MHz oscillator used as USB and RNG clock source */
    RCC_PERIPH_RNG_HSI48,
#endif /* RNG */


#if defined(GPIOA)
    RCC_PERIPH_GPIOA        , /**< IO port A Clock Enable */
#endif /* GPIOA */
#if defined(GPIOB)
    RCC_PERIPH_GPIOB        , /**< IO port B Clock Enable */
#endif /* GPIOB */
#if defined(GPIOC)
    RCC_PERIPH_GPIOC        , /**< IO port C Clock Enable */
#endif /* GPIOC */
#if defined(GPIOD)
    RCC_PERIPH_GPIOD        , /**< IO port D Clock Enable */
#endif /* GPIOD */
#if defined(GPIOE)
    RCC_PERIPH_GPIOE        , /**< IO port E Clock Enable */
#endif /* GPIOE */
#if defined(GPIOF)
    RCC_PERIPH_GPIOF        , /**< IO port F Clock Enable */
#endif /* GPIOF */
#if defined(GPIOG)
    RCC_PERIPH_GPIOG        , /**< IO port G Clock Enable */
#endif /* GPIOG */


#if defined(TIM1)
    RCC_PERIPH_TIM1         , /**< TIM1 Clock Enable */
#endif /* TIM1 */
#if defined(TIM2)
    RCC_PERIPH_TIM2         , /**< TIM2 Clock Enable */
#endif /* TIM2 */
#if defined(TIM3)
    RCC_PERIPH_TIM3         , /**< TIM3 Clock Enable */
#endif /* TIM3 */
#if defined(TIM4)
    RCC_PERIPH_TIM4         , /**< TIM4 Clock Enable */
#endif /* TIM4 */
#if defined(TIM5)
    RCC_PERIPH_TIM5         , /**< TIM5 Clock Enable */
#endif /* TIM5 */
#if defined(TIM6)
    RCC_PERIPH_TIM6         , /**< TIM6 Clock Enable */
#endif /* TIM6 */
#if defined(TIM7)
    RCC_PERIPH_TIM7         , /**< TIM7 Clock Enable */
#endif /* TIM7 */
#if defined(TIM8)
    RCC_PERIPH_TIM8         , /**< TIM8 Clock Enable */
#endif /* TIM8 */
#if defined(TIM15)
    RCC_PERIPH_TIM15        , /**< TIM15 Clock Enable */
#endif /* TIM15 */
#if defined(TIM16)
    RCC_PERIPH_TIM16        , /**< TIM16 Clock Enable */
#endif /* TIM16 */
#if defined(TIM17)
    RCC_PERIPH_TIM17        , /**< TIM17 Clock Enable */
#endif /* TIM17 */
#if defined(TIM20)
    RCC_PERIPH_TIM20        , /**< APB2 GRP1 bus */
#endif /* TIM20 */
#if defined(HRTIM1)
    RCC_PERIPH_HRTIM        , /**< APB2 GRP1 bus */
#endif /* DSI */
#if defined(LPTIM1)
    /** APB1 (PCLK1) clock used as Low Power Timer 1 clock source (default) */
    RCC_PERIPH_LPTIM1_APB1,
    /** Low Speed Internal (32kHz) clock used as Low Power Timer 1 clock source */
    RCC_PERIPH_LPTIM1_LSI,
    /** High Speed Internal (16MHz) clock used as Low Power Timer 1 clock source */
    RCC_PERIPH_LPTIM1_HSI16,
    /** Low Speed External clock used as Low Power Timer 1 clock source */
    RCC_PERIPH_LPTIM1_LSE,
#endif /* LPTIM1 */


#if defined(CRS)
    RCC_PERIPH_CRS,     /**< APB1 GRP1 bus */
#endif /* CRS */


#if defined(SPI1)
    /** System clock used as SPI1 clock source */
    RCC_PERIPH_SPI1,
#endif /* SPI1 */
#if defined(SPI2)
    /** System clock used as SPI2 and I2S clock source */
    RCC_PERIPH_SPI2_I2S2_SYSCLK,
    /** PLL Q output used as I2S clock source and I2S peripheral clock is activated.
     * \warning This is global setting for all I2S peripherals! */
    RCC_PERIPH_I2S2_PLLQ,
    /** High Speed Internal (16MHz) clock used as I2S clock source and I2S peripheral clock is activated.
     * \warning This is global setting for all I2S peripherals! */
    RCC_PERIPH_I2S2_HSI16,
    /** I2S Clock Input pin used as I2S clock source and I2S peripheral clock is activated.
     * \warning This is global setting for all I2S peripherals! */
    RCC_PERIPH_I2S2_CKIN,
#endif /* SPI2 */
#if defined(SPI3)
    /** System clock used as SPI3 and I2S clock source */
    RCC_PERIPH_SPI3_I2S3_SYSCLK,
    /** PLL Q output used as I2S clock source and I2S peripheral clock is activated.
     * \warning This is global setting for all I2S peripherals! */
    RCC_PERIPH_I2S3_PLLQ,
    /** High Speed Internal (16MHz) clock used as I2S clock source and I2S peripheral clock is activated.
     * \warning This is global setting for all I2S peripherals! */
    RCC_PERIPH_I2S3_HSI16,
    /** I2S Clock Input pin used as I2S clock source and I2S peripheral clock is activated.
     * \warning This is global setting for all I2S peripherals! */
    RCC_PERIPH_I2S3_CKIN,
#endif /* SPI3 */
#if defined(SPI4)
    /** System clock used as I2S clock source */
    RCC_PERIPH_SPI4,
#endif /* SPI4 */


#if defined(USART1)
    /** APB2 (PCLK2) clock used as USART 1 clock source (default) */
    RCC_PERIPH_USART1_APB1,
    /** System clock used as USART 1 clock source */
    RCC_PERIPH_USART1_SYSCLK,
    /** PLL Q output used as USART 1 clock source */
    RCC_PERIPH_USART1_HSI16,
    /** Low Speed External clock used as USART 1 clock source */
    RCC_PERIPH_USART1_LSE,
#endif /* USART1 */
#if defined(USART2)
    /** APB1 (PCLK1) clock used as USART 2 clock source (default) */
    RCC_PERIPH_USART2_APB1,
    /** System clock used as USART 2 clock source */
    RCC_PERIPH_USART2_SYSCLK,
    /** PLL Q output used as USART 2 clock source */
    RCC_PERIPH_USART2_HSI16,
    /** Low Speed External clock used as USART 2 clock source */
    RCC_PERIPH_USART2_LSE,
#endif /* USART2 */
#if defined(USART3)
    /** APB1 (PCLK1) clock used as USART 3 clock source (default) */
    RCC_PERIPH_USART3_APB1,
    /** System clock used as USART 3 clock source */
    RCC_PERIPH_USART3_SYSCLK,
    /** PLL Q output used as USART 3 clock source */
    RCC_PERIPH_USART3_HSI16,
    /** Low Speed External clock used as USART 3 clock source */
    RCC_PERIPH_USART3_LSE,
#endif /* USART3 */
#if defined(UART4)
    /** APB1 (PCLK1) clock used as UART 4 clock source (default) */
    RCC_PERIPH_UART4_APB1,
    /** System clock used as UART 4 clock source */
    RCC_PERIPH_UART4_SYSCLK,
    /** PLL Q output used as UART 4 clock source */
    RCC_PERIPH_UART4_HSI16,
    /** Low Speed External clock used as UART 4 clock source */
    RCC_PERIPH_UART4_LSE,
#endif /* UART4 */
#if defined(UART5)
    /** APB1 (PCLK1) clock used as UART 5 clock source (default) */
    RCC_PERIPH_UART5_APB1,
    /** System clock used as UART 5 clock source */
    RCC_PERIPH_UART5_SYSCLK,
    /** PLL Q output used as UART 5 clock source */
    RCC_PERIPH_UART5_HSI16,
    /** Low Speed External clock used as UART 5 clock source */
    RCC_PERIPH_UART5_LSE,
#endif /* USART5 */
#if defined(LPUART1)
    /** APB1 (PCLK1) clock used as LPUART 1 clock source (default) */
    RCC_PERIPH_LPUART1_APB1,
    /** System clock used as LPUART 1 clock source */
    RCC_PERIPH_LPUART1_SYSCLK,
    /** PLL Q output used as LPUART 1 clock source */
    RCC_PERIPH_LPUART1_HSI16,
    /** Low Speed External clock used as LPUART 1 clock source */
    RCC_PERIPH_LPUART1_LSE,
#endif /* LPUART1 */


#if defined(I2C1)
    /** APB1 (PCLK1) clock used as I2C 1 clock source (default) */
    RCC_PERIPH_I2C1_APB1,
    /** System clock used as I2C 1 clock source */
    RCC_PERIPH_I2C1_SYSCLK,
    /** High Speed Internal (16MHz) clock used as I2C 1 clock source */
    RCC_PERIPH_I2C1_HSI16,
#endif /* I2C1 */
#if defined(I2C2)
    /** APB1 (PCLK1) clock used as I2C 2 clock source (default) */
    RCC_PERIPH_I2C2_APB1,
    /** System clock used as I2C 2 clock source */
    RCC_PERIPH_I2C2_SYSCLK,
    /** High Speed Internal (16MHz) clock used as I2C 2 clock source */
    RCC_PERIPH_I2C2_HSI16,
#endif /* I2C2 */
#if defined(I2C3)
    /** APB1 (PCLK1) clock used as I2C 3 clock source (default) */
    RCC_PERIPH_I2C3_APB1,
    /** System clock used as I2C 3 clock source */
    RCC_PERIPH_I2C3_SYSCLK,
    /** High Speed Internal (16MHz) clock used as I2C 3 clock source */
    RCC_PERIPH_I2C3_HSI16,
#endif /* I2C3 */
#if defined(I2C4)
    /** APB1 (PCLK1) clock used as I2C 4 clock source (default) */
    RCC_PERIPH_I2C4_APB1,
    /** System clock used as I2C 4 clock source */
    RCC_PERIPH_I2C4_SYSCLK,
    /** High Speed Internal (16MHz) clock used as I2C 4 clock source */
    RCC_PERIPH_I2C4_HSI16,
#endif /* I2C4 */


#if defined(USB)
    /** PLL P output used as USB and RNG clock source (default) */
    RCC_PERIPH_USB_PLLQ,
    /** High Speed Internal 48MHz oscillator used as USB and RNG clock source */
    RCC_PERIPH_USB_HSI48,
#endif /* USB */

#if defined(FDCAN1)
    /** APB1 (PCLK1) clock used as FDCAN 1 clock source (default) */
    RCC_PERIPH_FDCAN1_APB1,
    /** PLL Q output used as FDCAN 1 clock source */
    RCC_PERIPH_FDCAN1_PLLQ,
    /** High Speed External clock used as FDCAN 1 clock source */
    RCC_PERIPH_FDCAN1_HSE,
#endif /* FDCAN1 */

#if defined(SAI1)
    /** System clock used as SAI 1 clock source */
    RCC_PERIPH_SAI1_SYSCLK,
    /** PLL Q output used as SAI 1 clock source */
    RCC_PERIPH_SAI1_PLLQ,
    /** High Speed Internal (16MHz) clock used as SAI 1 clock source */
    RCC_PERIPH_SAI1_HSI16,
    /** SAI Clock Input pin used as SAI 1 clock source */
    RCC_PERIPH_SAI1_CKIN,
#endif /* SAI1 */

    RCC_PERIPH_LIST_CNT
}   rcc_PeriphId_t;

/*------------------------ Clock sources configuration -----------------------*/

/** \brief Enumeration of High Speed External (HSE) input configuration */
typedef enum
{
    RCC_HSE_TYPE_NONE     = 0u , /**< No external clock connected to HSE pin */
    RCC_HSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_HSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_HseType_t;


/** \brief Enumeration of Low Speed External (LSE) input configuration */
typedef enum
{
    RCC_LSE_TYPE_NONE     = 0u , /**< No external clock connected to LSE pin */
    RCC_LSE_TYPE_CRYSTAL       , /**< External crystal/ceramic resonator     */
    RCC_LSE_TYPE_SIG_DIGITAL_IN, /**< External high voltage swing signal     */
}   rcc_LseType_t;


/** \brief List of all available internal oscillators */
typedef enum
{
    RCC_OSC_HSE = 0u,   /**< High Speed External (HSE) oscillator. */
    RCC_OSC_LSE,        /**< Low Speed External (LSE) oscillator. */
    RCC_OSC_HSI16,      /**< 16 MHz High Speed Internal (HSI) oscillator. */
    RCC_OSC_HSI48,      /**< 48 MHz High Speed Internal (HSI) oscillator. */
    RCC_OSC_LSI,        /**< 32.768 kHz Low Speed Internal (LSI) oscillator. */
    RCC_OSC_CNT         /**< Count of available internal oscillator */
}   rcc_OscId_t;


/** \brief Oscillator divider value type definition. */
typedef uint32_t rcc_OscDiv_t;

/*------------------- Phase Locked Loop's (PLL) configuration ----------------*/

/** \brief Phase Locked Loop identification enumeration */
typedef enum
{
    RCC_PLL_1 = 0u, /**< Phase Locked Loop 1 */
    RCC_PLL_CNT
}   rcc_PllId_t;


/** \brief Phase Locked Loop (PLL) clock source multiplexer configuration list
 * \note If the PLL is not used, select RCC_PLL_SRC_NONE. Otherwise will be
 *       PLL activated. */
typedef enum
{
    RCC_PLL_SRC_NONE = 0u, /**< PLL is inactive                             */
    RCC_PLL_SRC_HSE      , /**< PLL will be clocked by HSE oscillator       */
    RCC_PLL_SRC_HSI16    , /**< PLL will be clocked by 16MHz HSI oscillator */
    RCC_PLL_SRC_CNT        /**< Count of PLL source options                 */
}   rcc_PllClkSrc_t;


/** \brief Phase Locked Loop (PLL) M Divider value.
 * This is clock input divider for PLL.
 * Step size: 1
 * Range    : 1 - 63
 */
typedef uint32_t rcc_PllMDiv_t;


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
typedef uint32_t rcc_PllPDiv_t;


/** \brief Phase Locked Loop (PLL) Q output divider value.
 * Step size: 1
 * Range    : 1 - 128
 */
typedef uint32_t rcc_PllQDiv_t;


/** \brief Phase Locked Loop (PLL) R output divider value.
 * Step size: 1
 * Range    : 1 - 128
 */
typedef uint32_t rcc_PllRDiv_t;

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
    RCC_CLK_OUT_LSCO,      /**< Low Speed Clock Output (LSCO) */
    RCC_CLK_OUT_CNT        /**< Count of Clock Outputs        */
}   rcc_ClkOut_Id_t;


/**
 * \brief Clock Output's source configuration enumeration.
 */
typedef enum
{
    RCC_CLKOUT_SOURCE_NONE   = 0u, /**< No clock source selected                                                                    */

    RCC_CLKOUT_SOURCE_MCO1_LSE   , /**< Low Speed External (LSE) will be used as Master Clock Output (MCO) clock source             */
    RCC_CLKOUT_SOURCE_MCO1_LSI   , /**< Low Speed Internal (LSI) will be used as Master Clock Output (MCO) clock source             */
    RCC_CLKOUT_SOURCE_MCO1_HSE   , /**< High Speed External (HSE) will be used as Master Clock Output (MCO) clock source            */
    RCC_CLKOUT_SOURCE_MCO1_HSI16 , /**< 16MHz High Speed Internal (HSI) will be used as Master Clock Output (MCO) clock source      */
    RCC_CLKOUT_SOURCE_MCO1_HSI48 , /**< 48MHz High Speed Internal (HSI) will be used as Master Clock Output (MCO) clock source      */
    RCC_CLKOUT_SOURCE_MCO1_PLL1R , /**< Phase Locked Loop 1 output R (PLL1R) will be used as Master Clock Output (MCO) clock source */
    RCC_CLKOUT_SOURCE_MCO1_SYSCLK, /**< System clock (SYSCLK) will be used as Master Clock Output (MCO) clock source                */

    RCC_CLKOUT_SOURCE_LSCO_LSI   , /**< Low Speed Internal (LSI) will be used as Low Speed Clock Output (LSCO) clock source         */
    RCC_CLKOUT_SOURCE_LSCO_LSE   , /**< Low Speed External (LSE) will be used as Low Speed Clock Output (LSCO) clock source         */

    RCC_CLKOUT_SOURCE_CNT          /**< Count of clock output sources                                                               */
}   rcc_ClkOut_Source_t;


/**
 * \brief Clock Output divider value type.
 *
 * Output clock divider value for Clock Output's.
 * Step size: 1
 * Range of values: 1 - 15
 */
typedef uint32_t rcc_ClkOut_Div_t;


/** \brief List of available MCO outputs */
typedef enum
{
    RCC_CLKOUT_PIN_MCO1_PA8 = 0u, /**< Pin PA8 used as MCO output  */
#if defined(GPIOG)
    RCC_CLKOUT_PIN_MCO1_PG10,     /**< Pin PG10 used as MCO output */
#endif
    RCC_CLKOUT_PIN_LSCO_PA2,      /**< Pin PA2 used as LSCO output */
    RCC_CLKOUT_PIN_CNT
}   rcc_ClkOut_Pin_t;

/*-------------------------- Clock buses configuration -----------------------*/

/** \brief List of all available clock buses */
typedef enum
{
    RCC_CLK_BUS_AHB1 = 0u,/**< AHB1 Peripheral bus         */
    RCC_CLK_BUS_AHB2,     /**< AHB2 group 1 Peripheral bus */
    RCC_CLK_BUS_AHB3,     /**< AHB3 Peripheral bus         */
    RCC_CLK_BUS_APB1_1,   /**< APB2 group 1 Peripheral bus */
    RCC_CLK_BUS_APB1_2,   /**< APB2 group 2 Peripheral bus */
    RCC_CLK_BUS_APB2,     /**< APB2 Peripheral bus         */
    RCC_CLK_BUS_CNT       /**< Count of clock buses        */
}   rcc_ClkBusId_t;


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
    RCC_CLK_SRC_AHBCLK,      /**< Advanced High-performance Bus clock source                          */
    RCC_CLK_SRC_APB1CLK,     /**< Advanced Peripheral Bus 1 (APB1) clock source                       */
    RCC_CLK_SRC_APB2CLK,     /**< Advanced Peripheral Bus 2 (APB2) clock source                       */
    RCC_CLK_SRC_HSI16CLK,    /**< 64MHz High Speed Internal (HSI) clock source                        */
    RCC_CLK_SRC_HSI48CLK,    /**< 48MHz High Speed Internal (HSI) oscillator clock source             */
    RCC_CLK_SRC_HSECLK,      /**< High Speed External clock source                                    */
    RCC_CLK_SRC_LSICLK,      /**< 32kHz Low Speed Internal (HSI) oscillator clock source              */
    RCC_CLK_SRC_LSECLK,      /**< Low Speed External clock source                                     */
    RCC_CLK_SRC_CNT          /**< Count of clock sources                                              */
}   rcc_ClkSrcId_t;


/** \brief Clock bus divider value type definition.
 * Used for AHB, APB1, APB2 and APB3 clock bus dividers (all available clock
 * busses in \ref rcc_ClkBusId_t ) */
typedef uint32_t rcc_ClkBusDiv_t;


/** \brief System clock source multiplexer configuration list */
typedef enum
{
    RCC_SYSTEM_CLOCK_SOURCE_HSI = 0u, /**< HSI will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_HSE,      /**< HSE will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_PLL,      /**< PLL will be used as system clock source  */
    RCC_SYSTEM_CLOCK_SOURCE_CNT       /**< Count of system clock sources            */
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
    RCC_APB3_DIVIDER_NONE = 0u /**< No division, APB3 is not used */
}   rcc_APB3_Div_t;

/*------------------------ Flash and power configuration ---------------------*/

/**
 * \brief Defines number of wait states for Flash memory access.
 *
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ================================================================
 * |  Wait states   | Vcore Range 1 | Vcore Range 1 | Vcore Range 2 |
 * |   (LATENCY)    |  boost mode   |  normal mode  |               |
 * |================|===============|===============|===============|
 * |      0 WS      |  =< 34 MHz    |  =< 30 MHz    |  =< 12 MHz    |
 * | (1 CPU cycle)  |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      1 WS      |  =< 68 MHz    |  =< 60 MHz    |  =< 24 MHz    |
 * | (2 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      2 WS      |  =< 102 MHz   |  =< 90 MHz    |  =< 26 MHz    |
 * | (3 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      3 WS      |  =< 136 MHz   |  =< 120 MHz   |      ---      |
 * | (4 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      4 WS      |  =< 170 MHz   |  =< 150 MHz   |      ---      |
 * | (5 CPU cycles) |               |               |               |
 *  ================================================================
 * \endcond
 */
typedef enum
{
    RCC_FLASH_LATENCY_0_WS  = LL_FLASH_LATENCY_0,  /**< 1 CPU cycle wait for reading from FLASH */
    RCC_FLASH_LATENCY_1_WS  = LL_FLASH_LATENCY_1,  /**< 2 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_2_WS  = LL_FLASH_LATENCY_2,  /**< 3 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_3_WS  = LL_FLASH_LATENCY_3,  /**< 4 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_4_WS  = LL_FLASH_LATENCY_4,  /**< 5 CPU cycles wait for reading from FLASH */
#if defined(FLASH_ACR_LATENCY_5WS)
    RCC_FLASH_LATENCY_5_WS  = LL_FLASH_LATENCY_5,  /**< 6 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_6_WS  = LL_FLASH_LATENCY_6,  /**< 7 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_7_WS  = LL_FLASH_LATENCY_7,  /**< 8 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_8_WS  = LL_FLASH_LATENCY_8,  /**< 9 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_9_WS  = LL_FLASH_LATENCY_9,  /**< 10 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_10_WS = LL_FLASH_LATENCY_10, /**< 11 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_11_WS = LL_FLASH_LATENCY_11, /**< 12 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_12_WS = LL_FLASH_LATENCY_12, /**< 13 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_13_WS = LL_FLASH_LATENCY_13, /**< 14 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_14_WS = LL_FLASH_LATENCY_14, /**< 15 CPU cycles wait for reading from FLASH */
    RCC_FLASH_LATENCY_15_WS = LL_FLASH_LATENCY_15, /**< 16 CPU cycles wait for reading from FLASH */
#endif
}   rcc_FlashLatency_t;


/**
 * \brief PWR Voltage scaling configuration.
 * \note Power voltage scaling has to be chosen according to desired performance.
 *
 * \cond INTERNAL
 * Recommended number of wait states and programming delay
 *  ================================================================
 * |  Wait states   | Vcore Range 1 | Vcore Range 1 | Vcore Range 2 |
 * |   (LATENCY)    |  boost mode   |  normal mode  |               |
 * |================|===============|===============|===============|
 * |      0 WS      |  =< 34 MHz    |  =< 30 MHz    |  =< 12 MHz    |
 * | (1 CPU cycle)  |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      1 WS      |  =< 68 MHz    |  =< 60 MHz    |  =< 24 MHz    |
 * | (2 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      2 WS      |  =< 102 MHz   |  =< 90 MHz    |  =< 26 MHz    |
 * | (3 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      3 WS      |  =< 136 MHz   |  =< 120 MHz   |      ---      |
 * | (4 CPU cycles) |               |               |               |
 * |----------------|---------------|---------------|---------------|
 * |      4 WS      |  =< 170 MHz   |  =< 150 MHz   |      ---      |
 * | (5 CPU cycles) |               |               |               |
 *  ================================================================
 * \endcond
 */
typedef enum
{
    RCC_PWR_VOLTAGE_SCALE_0 = 0u, /**< Vcore Range 1 & boost mode  */
    RCC_PWR_VOLTAGE_SCALE_1,      /**< Vcore Range 1 & normal mode */
    RCC_PWR_VOLTAGE_SCALE_2,      /**< Vcore Range 2               */
    RCC_PWR_VOLTAGE_SCALE_CNT     /**< Power voltage scale counts  */
}   rcc_PwrVoltageScale_t;

/*--------------------------- Configuration structures -----------------------*/

/** \brief Phase Locked Loop (PLL) Configuration structure type */
typedef struct rcc_PllConfigStruct_t
{
    /** Specifies clock source of the PLL's */
    rcc_PllClkSrc_t         Pll_Source;

    /** M prescaler - divider, used by both: PLL and PLLSAI1. */
    rcc_PllMDiv_t           M_Divider;

    /** Main PLLN prescaler - multiplier. Must be in range from 8 to 86 */
    rcc_PllNMult_t          N_Multiplier;

    /** Output P prescaler - divider. Configuration function sets the prescaler
    *  during startup, but the output must be enabled later by user.
    *  If not enabled properly, the SAI peripheral switches to HSI16 automatically. */
    rcc_PllPDiv_t           P_Divider;

    /** Output Q prescaler - divider. Configuration function sets the prescaler
    *  during startup, but the output must be enabled later by user. */
    rcc_PllQDiv_t           Q_Divider;

    /** Output R prescaler - divider. Configuration function sets the prescaler
    *  during startup and the output is enabled at the same time, if the PLL
    *  is used as the system cock source. */
    rcc_PllRDiv_t           R_Divider;

}   rcc_PllConfigStruct_t;


/** \brief Clock outputs configuration structure */
typedef struct
{
    rcc_ClkOut_Source_t ClockSource;  /**< Clock source identification for clock output */
    rcc_ClkOut_Div_t    ClockDivider; /**< Clock divider value for clock output         */
    rcc_ClkOut_Pin_t    ClockOutPin;  /**< Output pin identification for clock output   */
}   rcc_ClkOutConfigStruct_t;


/** \brief Reset and Clock Control configuration structure */
typedef struct rcc_ConfigStruct_t
{
    /** Specifies HSE clock type. Ignored if HSE is not used */
    rcc_HseType_t           HSE_ClockType;

    /** Specified frequency of the HSE oscillator. Ignored if HSE is not used */
    rcc_FreqHz_t            HSE_Frequency_Hz;

    /** Specifies LSE clock type. Ignored if LSE is not used */
    rcc_LseType_t           LSE_ClockType;

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

    /**
     * Scaling of internal voltage supply
     */
    rcc_PwrVoltageScale_t    VoltageScaling;

    rcc_ClkOutConfigStruct_t McoConfig[ RCC_CLK_OUT_CNT ];
}   rcc_ConfigStruct_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* RCC_RCC_TYPES_H */
