/**
 * \author Mr.Nobody
 * \file Rcc_ClkOut.c
 * \ingroup Rcc
 * \brief Rcc module ClkOut component functionality.
 *
 * STM32L4 / STM32L4+ family clock outputs:
 * - MCO  (PA8, AF0)    - SYSCLK, MSI, HSI16, HSE, main PLL output R, LSI, LSE,
 *                        HSI48 (selected MCUs), divider 1, 2, 4, 8, 16
 * - LSCO (PA2, analog) - LSI or LSE (backup domain, divider 1 only)
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkOut.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Gpio_Port.h"                      /* GPIO functionality include     */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_CLK_OUT_TIMEOUT_RAW             ( 0x84FCB )

/** Count of clock output source records */
#define RCC_CLK_OUT_SRC_LIST_CNT            ( sizeof( rcc_ClkOutSrcConfig ) / sizeof( rcc_ClkOutSrcConfig[ 0u ] ) )

/** Minimal value of clock output divider */
#define RCC_CLK_OUT_DIV_MIN                 ( 1u )

/** Count of MCO prescaler values (divider 1, 2, 4, 8, 16 - prescaler field = log2 of divider) */
#define RCC_CLK_OUT_MCO_PRE_CNT             ( 5u )

/* ============================== TYPEDEFS ================================== */

/** \brief Clock output configuration (multiplexer field and output pin) */
typedef struct
{
    rcc_RegId_t    SrcRegId; /**< Register of source multiplexer and enable bits      */
    uint32_t       SrcMask;  /**< Source multiplexer field mask (incl. enable bit)    */
    uint32_t       PreMask;  /**< Prescaler field mask (RCC_CFGR, 0 - no prescaler)   */
    uint32_t       PrePos;   /**< Prescaler field position                            */
    gpio_PortId_t  PortId;   /**< Output pin port                                     */
    gpio_PinId_t   PinId;    /**< Output pin                                          */
    gpio_PinMode_t PinMode;  /**< Output pin mode (alternate function / analog)       */
}   rcc_ClkOutConfig_t;


/** \brief Clock output source record */
typedef struct
{
    rcc_ClkOut_Id_t     OutId;     /**< Clock output                                          */
    rcc_ClkOut_Source_t ClkSource; /**< Clock output source                                   */
    uint32_t            RegValue;  /**< Multiplexer field value (masked by SrcMask of the output) */
}   rcc_ClkOutSrcConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static rcc_RequestState_t Rcc_ClkOut_Wait_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal );

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration of clock outputs, indexed by \ref rcc_ClkOut_Id_t */
static const rcc_ClkOutConfig_t rcc_ClkOutConfig[ RCC_CLK_OUT_CNT ] =
{
    [RCC_CLK_OUT_MCO1] = { .SrcRegId = RCC_REG_CFGR, .SrcMask = RCC_CFGR_MCOSEL                  , .PreMask = RCC_CFGR_MCOPRE, .PrePos = RCC_CFGR_MCOPRE_Pos,
                           .PortId   = GPIO_PORT_A , .PinId   = GPIO_PIN_ID_8, .PinMode = GPIO_PIN_MODE_ALTERNATE },
    [RCC_CLK_OUT_LSCO] = { .SrcRegId = RCC_REG_BDCR, .SrcMask = RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL, .PreMask = 0u             , .PrePos = 0u                 ,
                           .PortId   = GPIO_PORT_A , .PinId   = GPIO_PIN_ID_2, .PinMode = GPIO_PIN_MODE_ANALOG    },
};


/** \brief Selectable sources of clock outputs (\ref RCC_CLK_SOURCE_NONE - output disabled) */
static const rcc_ClkOutSrcConfig_t rcc_ClkOutSrcConfig[ ] =
{
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_NONE        , .RegValue = LL_RCC_MCO1SOURCE_NOCLOCK                  },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_SYSCLK , .RegValue = LL_RCC_MCO1SOURCE_SYSCLK                   },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_MSI    , .RegValue = LL_RCC_MCO1SOURCE_MSI                      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI    , .RegValue = LL_RCC_MCO1SOURCE_HSI                      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSE    , .RegValue = LL_RCC_MCO1SOURCE_HSE                      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_PLLR   , .RegValue = LL_RCC_MCO1SOURCE_PLLCLK                   },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSI    , .RegValue = LL_RCC_MCO1SOURCE_LSI                      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSE    , .RegValue = LL_RCC_MCO1SOURCE_LSE                      },
#if defined(RCC_CRRCR_HSI48ON)
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI48  , .RegValue = LL_RCC_MCO1SOURCE_HSI48                    },
#endif /* RCC_CRRCR_HSI48ON */
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_NONE        , .RegValue = 0u                                         },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSI    , .RegValue = RCC_BDCR_LSCOEN                            },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSE    , .RegValue = RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL         },
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/*----------------------- Clock outputs configuration ------------------------*/

/**
 * \brief Function used to set clock output signal source.
 *
 * The output pin is configured (MCO - alternate function 0, LSCO - analog) and
 * the source is selected. \ref RCC_CLK_SOURCE_NONE marks unused clock output -
 * nothing is configured (the multiplexer and the output pin are not changed).
 * Source of another clock output is rejected without any change.
 *
 * \param outId     [in]: Clock output identification
 * \param clkSource [in]: Clock output signal source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue = 0u;

    if( RCC_CLK_OUT_CNT <= outId )
    {
        /* Incorrect output ID */
        retState = RCC_REQUEST_ERROR;
    }
    else if( RCC_CLK_SOURCE_NONE == clkSource )
    {
        /* Clock output is not used */
        retState = RCC_REQUEST_OK;
    }
    else
    {
        const rcc_ClkOutConfig_t * const outConfig = &rcc_ClkOutConfig[ outId ];

        for( uint32_t srcIdx = 0u; RCC_CLK_OUT_SRC_LIST_CNT > srcIdx; srcIdx++ )
        {
            if( ( outId     == rcc_ClkOutSrcConfig[ srcIdx ].OutId     ) &&
                ( clkSource == rcc_ClkOutSrcConfig[ srcIdx ].ClkSource )    )
            {
                regValue = rcc_ClkOutSrcConfig[ srcIdx ].RegValue;
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next record */
            }
        }

        if( RCC_REQUEST_OK == retState )
        {
            gpio_Config_t       gpioConfig;
            gpio_RequestState_t gpioState = GPIO_REQUEST_ERROR;

            gpioConfig.PortId           = outConfig->PortId;
            gpioConfig.PinId            = outConfig->PinId;
            gpioConfig.PinMode          = outConfig->PinMode;
            gpioConfig.PinPull          = GPIO_PIN_PULL_NONE;
            gpioConfig.PinSpeed         = GPIO_PIN_SPEED_VERY_HIGH;
            gpioConfig.PinOutType       = GPIO_PIN_OUTPUT_PUSHPULL;
            gpioConfig.PinAltFunction   = GPIO_ALT_FUNC_0;
            gpioConfig.PinActiveLevel   = GPIO_PIN_LEVEL_HIGH;

            gpioState = Gpio_Init( &gpioConfig );

            if( GPIO_REQUEST_OK == gpioState )
            {
                Rcc_Set_RegVal( outConfig->SrcRegId, outConfig->SrcMask, regValue );

                retState = Rcc_ClkOut_Wait_RegVal( outConfig->SrcRegId, outConfig->SrcMask, regValue );
            }
            else
            {
                /* Output pin could not be configured - source is not selected */
                retState = RCC_REQUEST_ERROR;
            }
        }
        else
        {
            /* Source does not belong to the clock output - nothing is changed */
        }
    }

    return ( retState );
}


/**
 * \brief Function used to get clock output signal source.
 *
 * \param outId      [in]: Clock output identification
 * \param clkSource [out]: Pointer to clock output source (\ref RCC_CLK_SOURCE_NONE - output disabled)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error (also for reserved multiplexer value).
 */
rcc_RequestState_t Rcc_ClkOut_Get_ClockSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_CLK_OUT_CNT  > outId     ) &&
        ( RCC_NULL_PTR    != clkSource )    )
    {
        const rcc_ClkOutConfig_t * const outConfig = &rcc_ClkOutConfig[ outId ];
        const uint32_t                   regValue  = Rcc_Get_RegVal( outConfig->SrcRegId, outConfig->SrcMask );

        for( uint32_t srcIdx = 0u; RCC_CLK_OUT_SRC_LIST_CNT > srcIdx; srcIdx++ )
        {
            if( ( outId    == rcc_ClkOutSrcConfig[ srcIdx ].OutId    ) &&
                ( regValue == rcc_ClkOutSrcConfig[ srcIdx ].RegValue )    )
            {
                *clkSource = rcc_ClkOutSrcConfig[ srcIdx ].ClkSource;
                retState   = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next record */
            }
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Function used to set clock output divider.
 *
 * \param outId      [in]: Clock output identification
 * \param clkDivider [in]: Clock output divider - 1, 2, 4, 8, 16 (MCO), 1 (LSCO)
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_CLK_OUT_CNT <= outId )
    {
        retState = RCC_REQUEST_ERROR;
    }
    else if( 0u == rcc_ClkOutConfig[ outId ].PreMask )
    {
        /* Output without prescaler - divider 1 only */
        if( RCC_CLK_OUT_DIV_MIN == clkDivider )
        {
            retState = RCC_REQUEST_OK;
        }
        else
        {
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* MCOPRE: 000 - /1, 001 - /2, 010 - /4, 011 - /8, 100 - /16 */
        for( uint32_t preVal = 0u; RCC_CLK_OUT_MCO_PRE_CNT > preVal; preVal++ )
        {
            if( ( RCC_CLK_OUT_DIV_MIN << preVal ) == clkDivider )
            {
                const uint32_t preRegVal = preVal << rcc_ClkOutConfig[ outId ].PrePos;

                Rcc_Set_RegVal( RCC_REG_CFGR, rcc_ClkOutConfig[ outId ].PreMask, preRegVal );

                retState = Rcc_ClkOut_Wait_RegVal( RCC_REG_CFGR, rcc_ClkOutConfig[ outId ].PreMask, preRegVal );
                break;
            }
            else
            {
                /* Continue with next prescaler value */
                retState = RCC_REQUEST_ERROR;
            }
        }
    }

    return ( retState );
}


/**
 * \brief Function used to get clock output divider.
 *
 * \param outId      [in] : Clock output identification
 * \param clkDivider [out]: Pointer to clock output divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Get_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR   != clkDivider ) &&
        ( RCC_CLK_OUT_CNT > outId      )    )
    {
        const uint32_t preMask = rcc_ClkOutConfig[ outId ].PreMask;
        const uint32_t preVal  = Rcc_Get_RegVal( RCC_REG_CFGR, preMask ) >> rcc_ClkOutConfig[ outId ].PrePos;

        if( ( 0u                      != preMask ) &&
            ( RCC_CLK_OUT_MCO_PRE_CNT >  preVal  )    )
        {
            *clkDivider = RCC_CLK_OUT_DIV_MIN << preVal;
            retState    = RCC_REQUEST_OK;
        }
        else if( 0u == preMask )
        {
            /* Output without prescaler */
            *clkDivider = RCC_CLK_OUT_DIV_MIN;
            retState    = RCC_REQUEST_OK;
        }
        else
        {
            /* Reserved prescaler value */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Waits until register field reaches expected value.
 *
 * \param regId       [in]: Register identification
 * \param regMask     [in]: Mask of the checked field
 * \param expectedVal [in]: Expected value of the masked field
 *
 * \return Returns "OK" if the field reached expected value in time, otherwise
 *         returns error.
 */
static rcc_RequestState_t Rcc_ClkOut_Wait_RegVal( rcc_RegId_t regId, uint32_t regMask, uint32_t expectedVal )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    for( uint32_t iterationCnt = 0u; RCC_CLK_OUT_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        const uint32_t regValue = Rcc_Get_RegVal( regId, regMask );

        if( expectedVal == regValue )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Register has not reached expected value yet, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return ( retState );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
