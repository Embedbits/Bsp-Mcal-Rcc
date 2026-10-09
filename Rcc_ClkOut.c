/**
 * \author Mr.Nobody
 * \file Rcc_ClkOut.c
 * \ingroup Rcc
 * \brief Rcc module ClkOut component functionality.
 *
 * STM32G4 family clock outputs:
 * - MCO  (PA8, AF0)    - SYSCLK, HSI16, HSE, PLL output R, LSI, LSE or HSI48,
 *                        divider 1, 2, 4, 8, 16 (RCC_CFGR MCOSEL, MCOPRE)
 * - LSCO (PA2, analog) - LSI or LSE, no divider (RCC_BDCR LSCOEN, LSCOSEL,
 *                        backup domain)
 *
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkOut.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Gpio_Port.h"                      /* GPIO functionality include     */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Count of clock output source records */
#define RCC_CLK_OUT_SRC_LIST_CNT            ( sizeof( rcc_ClkOutSrcConfig ) / sizeof( rcc_ClkOutSrcConfig[ 0u ] ) )

/** Minimal value of clock output divider */
#define RCC_CLK_OUT_DIV_MIN                 ( 1u )

/** Maximal value of MCO prescaler field (division by 16) */
#define RCC_CLK_OUT_MCO_PRE_MAX             ( 4u )

/* ============================== TYPEDEFS ================================== */

/** \brief Clock output configuration (multiplexer field and output pin) */
typedef struct
{
    rcc_RegId_t      SrcRegId; /**< Register of the source multiplexer field       */
    uint32_t         SrcMask;  /**< Source multiplexer field mask (with enable)     */
    uint32_t         PreMask;  /**< Prescaler field mask (0 - no prescaler)         */
    uint32_t         PrePos;   /**< Prescaler field position                        */
    gpio_PortId_t    PortId;   /**< Output pin port                                 */
    gpio_PinId_t     PinId;    /**< Output pin                                      */
    gpio_PinMode_t   PinMode;  /**< Output pin mode (alternate function / analog)   */
}   rcc_ClkOutConfig_t;


/** \brief Clock output source record */
typedef struct
{
    rcc_ClkOut_Id_t     OutId;     /**< Clock output                                              */
    rcc_ClkOut_Source_t ClkSource; /**< Clock output source                                       */
    uint32_t            RegValue;  /**< Multiplexer field value (masked by SrcMask of the output) */
}   rcc_ClkOutSrcConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration of clock outputs, indexed by \ref rcc_ClkOut_Id_t */
static const rcc_ClkOutConfig_t rcc_ClkOutConfig[ RCC_CLK_OUT_CNT ] =
{
    [RCC_CLK_OUT_MCO1] = { .SrcRegId = RCC_REG_CFGR, .SrcMask = RCC_CFGR_MCOSEL                   , .PreMask = RCC_CFGR_MCOPRE, .PrePos = RCC_CFGR_MCOPRE_Pos, .PortId = GPIO_PORT_A, .PinId = GPIO_PIN_ID_8, .PinMode = GPIO_PIN_MODE_ALTERNATE },
    [RCC_CLK_OUT_LSCO] = { .SrcRegId = RCC_REG_BDCR, .SrcMask = RCC_BDCR_LSCOEN | RCC_BDCR_LSCOSEL, .PreMask = 0u             , .PrePos = 0u                 , .PortId = GPIO_PORT_A, .PinId = GPIO_PIN_ID_2, .PinMode = GPIO_PIN_MODE_ANALOG    },
};


/** \brief Selectable sources of clock outputs */
static const rcc_ClkOutSrcConfig_t rcc_ClkOutSrcConfig[ ] =
{
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_NONE       , .RegValue = LL_RCC_MCO1SOURCE_NOCLOCK                          },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_SYSCLK, .RegValue = LL_RCC_MCO1SOURCE_SYSCLK                           },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI   , .RegValue = LL_RCC_MCO1SOURCE_HSI                              },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSE   , .RegValue = LL_RCC_MCO1SOURCE_HSE                              },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_PLLR  , .RegValue = LL_RCC_MCO1SOURCE_PLLCLK                           },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSI   , .RegValue = LL_RCC_MCO1SOURCE_LSI                              },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSE   , .RegValue = LL_RCC_MCO1SOURCE_LSE                              },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI48 , .RegValue = LL_RCC_MCO1SOURCE_HSI48                            },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_NONE       , .RegValue = 0u                                                 },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSI   , .RegValue = RCC_BDCR_LSCOEN | LL_RCC_LSCO_CLKSOURCE_LSI        },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSE   , .RegValue = RCC_BDCR_LSCOEN | LL_RCC_LSCO_CLKSOURCE_LSE        },
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes clock output module
 *
 * \return State of request execution. Returns \ref RCC_REQUEST_OK if request was
 *         success, otherwise returns \ref RCC_REQUEST_ERROR.
 */
rcc_RequestState_t Rcc_ClkOut_Init( void )
{
    rcc_RequestState_t returnState = RCC_REQUEST_OK;

    return ( returnState );
}


/**
 * \brief De-initializes clock output module
 */
void Rcc_ClkOut_Deinit( void )
{
    return;
}


/**
 * \brief Main task of clock output module
 */
void Rcc_ClkOut_Task( void )
{
    return;
}

/*----------------------- Clock outputs configuration ------------------------*/

/**
 * \brief Function used to set clock output signal source.
 *
 * \ref RCC_CLK_SOURCE_NONE marks unused clock output - nothing is configured (the
 * multiplexer and the output pin are not changed). Source of another clock output
 * is rejected without any change. The output pin is configured when a source is
 * selected (MCO - PA8 alternate function 0, LSCO - PA2 analog mode).
 *
 * \note LSCO is located in backup domain - write protection is released
 *       automatically. The oscillator of the selected source must be running.
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
                Rcc_Set_RegVal( outConfig->SrcRegId, outConfig->SrcMask, rcc_ClkOutSrcConfig[ srcIdx ].RegValue );

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
            gpio_Config_t gpioConfig;

            gpioConfig.PortId           = outConfig->PortId;
            gpioConfig.PinId            = outConfig->PinId;
            gpioConfig.PinMode          = outConfig->PinMode;
            gpioConfig.PinPull          = GPIO_PIN_PULL_NONE;
            gpioConfig.PinSpeed         = GPIO_PIN_SPEED_VERY_HIGH;
            gpioConfig.PinOutType       = GPIO_PIN_OUTPUT_PUSHPULL;
            gpioConfig.PinAltFunction   = GPIO_ALT_FUNC_0;
            gpioConfig.PinActiveLevel   = GPIO_PIN_LEVEL_HIGH;

            (void)Gpio_Init( &gpioConfig );
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
 * \ref RCC_CLK_SOURCE_NONE is returned for a disabled clock output.
 *
 * \param outId      [in]: Clock output identification
 * \param clkSource [out]: Pointer to clock output source
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Get_ClockSource( rcc_ClkOut_Id_t outId, rcc_ClkOut_Source_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_CLK_OUT_CNT  > outId     ) &&
        ( RCC_NULL_PTR    != clkSource )    )
    {
        const rcc_ClkOutConfig_t * const outConfig = &rcc_ClkOutConfig[ outId ];
        uint32_t                         regValue  = Rcc_Get_RegVal( outConfig->SrcRegId, outConfig->SrcMask );

        if( ( RCC_CLK_OUT_LSCO == outId                       ) &&
            ( 0u               == ( regValue & RCC_BDCR_LSCOEN ) )    )
        {
            /* Disabled LSCO - source selection is not relevant */
            regValue = 0u;
        }
        else
        {
            /* Selection is used as read */
        }

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
 * \param clkDivider [in]: Clock output divider - 1, 2, 4, 8 or 16 for MCO,
 *                         1 for LSCO (no divider)
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
        /* Output without divider - only "not divided" is accepted */
        retState = ( RCC_CLK_OUT_DIV_MIN == clkDivider ) ? RCC_REQUEST_OK : RCC_REQUEST_ERROR;
    }
    else
    {
        /* MCOPRE: 0 - /1, 1 - /2, 2 - /4, 3 - /8, 4 - /16 */
        for( uint32_t preVal = 0u; RCC_CLK_OUT_MCO_PRE_MAX >= preVal; preVal++ )
        {
            if( ( RCC_CLK_OUT_DIV_MIN << preVal ) == clkDivider )
            {
                Rcc_Set_RegVal( RCC_REG_CFGR, rcc_ClkOutConfig[ outId ].PreMask, preVal << rcc_ClkOutConfig[ outId ].PrePos );

                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Divider not supported yet, keep error */
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
        if( 0u == rcc_ClkOutConfig[ outId ].PreMask )
        {
            /* Output without divider */
            *clkDivider = RCC_CLK_OUT_DIV_MIN;

            retState = RCC_REQUEST_OK;
        }
        else
        {
            const uint32_t preVal = Rcc_Get_RegVal( RCC_REG_CFGR, rcc_ClkOutConfig[ outId ].PreMask ) >> rcc_ClkOutConfig[ outId ].PrePos;

            if( RCC_CLK_OUT_MCO_PRE_MAX >= preVal )
            {
                *clkDivider = RCC_CLK_OUT_DIV_MIN << preVal;

                retState = RCC_REQUEST_OK;
            }
            else
            {
                /* Reserved prescaler value */
                retState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/* =========================== INTERRUPT HANDLERS =========================== */

/* ================================ TASKS =================================== */
