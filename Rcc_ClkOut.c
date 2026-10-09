/**
 * \author Mr.Nobody
 * \file Rcc_ClkOut.c
 * \ingroup Rcc
 * \brief Rcc module ClkOut component functionality.
 *
 * Clock outputs of STM32U5:
 * - MCO  (master clock output, PA8 alternate function 0) - source MCOSEL and
 *   divider MCOPRE (1, 2, 4, 8, 16) in RCC CFGR1,
 * - LSCO (low speed clock output, PA2 additional function - no GPIO
 *   configuration) - source LSCOSEL and enable LSCOEN in RCC BDCR (backup
 *   domain, write protection released automatically).
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_ClkOut.h"                     /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registers operations include   */
#include "Rcc_ClkSrc.h"                     /* Backup domain access           */
#include "Gpio_Port.h"                      /* GPIO functionality include     */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Count of clock output source records */
#define RCC_CLK_OUT_SRC_LIST_CNT            ( sizeof( rcc_ClkOutSrcConfig ) / sizeof( rcc_ClkOutSrcConfig[ 0u ] ) )

/** Count of MCO divider records */
#define RCC_CLK_OUT_DIV_LIST_CNT            ( sizeof( rcc_ClkOutDivConfig ) / sizeof( rcc_ClkOutDivConfig[ 0u ] ) )

/** Divider of clock output without prescaler */
#define RCC_CLK_OUT_DIV_NONE                ( 1u )

/* ============================== TYPEDEFS ================================== */

/** \brief Clock output configuration (multiplexer field and output pin) */
typedef struct
{
    rcc_RegId_t         RegId;      /**< Register with source multiplexer field             */
    uint32_t            SrcMask;    /**< Source multiplexer field mask                      */
    uint32_t            EnableMask; /**< Output enable bit (0 - output has no enable bit)   */
    uint32_t            DivMask;    /**< Divider field mask (0 - output has no divider)     */
    rcc_FunctionState_t PinConfig;  /**< Output pin is configured as alternate function     */
    gpio_PortId_t       PortId;     /**< Output pin port                                    */
    gpio_PinId_t        PinId;      /**< Output pin                                         */
}   rcc_ClkOutConfig_t;


/** \brief Clock output source record */
typedef struct
{
    rcc_ClkOut_Id_t     OutId;     /**< Clock output                                          */
    rcc_ClkOut_Source_t ClkSource; /**< Clock output source                                   */
    uint32_t            RegValue;  /**< LL multiplexer value (masked by SrcMask of the output) */
}   rcc_ClkOutSrcConfig_t;


/** \brief MCO divider record */
typedef struct
{
    rcc_ClkOut_Div_t    Divider;   /**< Divider value             */
    uint32_t            RegValue;  /**< MCOPRE field value        */
}   rcc_ClkOutDivConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration of clock outputs, indexed by \ref rcc_ClkOut_Id_t */
static const rcc_ClkOutConfig_t rcc_ClkOutConfig[ RCC_CLK_OUT_CNT ] =
{
    [RCC_CLK_OUT_MCO1] = { .RegId = RCC_REG_CFGR1, .SrcMask = RCC_CFGR1_MCOSEL_Msk,  .EnableMask = 0u,                .DivMask = RCC_CFGR1_MCOPRE_Msk,
                           .PinConfig = RCC_FUNCTION_ACTIVE,   .PortId = GPIO_PORT_A, .PinId = GPIO_PIN_ID_8 },
    [RCC_CLK_OUT_LSCO] = { .RegId = RCC_REG_BDCR,  .SrcMask = RCC_BDCR_LSCOSEL_Msk,  .EnableMask = RCC_BDCR_LSCOEN,   .DivMask = 0u,
                           .PinConfig = RCC_FUNCTION_INACTIVE, .PortId = GPIO_PORT_A, .PinId = GPIO_PIN_ID_2 },
};


/** \brief Selectable sources of clock outputs */
static const rcc_ClkOutSrcConfig_t rcc_ClkOutSrcConfig[ ] =
{
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_NONE       , .RegValue = LL_RCC_MCO1SOURCE_NOCLOCK  },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_SYSCLK, .RegValue = LL_RCC_MCO1SOURCE_SYSCLK   },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_MSIS  , .RegValue = LL_RCC_MCO1SOURCE_MSIS     },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI   , .RegValue = LL_RCC_MCO1SOURCE_HSI      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSE   , .RegValue = LL_RCC_MCO1SOURCE_HSE      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_PLL1R , .RegValue = LL_RCC_MCO1SOURCE_PLLCLK   },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSI   , .RegValue = LL_RCC_MCO1SOURCE_LSI      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSE   , .RegValue = LL_RCC_MCO1SOURCE_LSE      },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI48 , .RegValue = LL_RCC_MCO1SOURCE_HSI48    },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_MSIK  , .RegValue = LL_RCC_MCO1SOURCE_MSIK     },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSI   , .RegValue = LL_RCC_LSCO_CLKSOURCE_LSI  },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSE   , .RegValue = LL_RCC_LSCO_CLKSOURCE_LSE  },
};


/** \brief Dividers of MCO output */
static const rcc_ClkOutDivConfig_t rcc_ClkOutDivConfig[ ] =
{
    { .Divider =  1u, .RegValue = LL_RCC_MCO1_DIV_1  },
    { .Divider =  2u, .RegValue = LL_RCC_MCO1_DIV_2  },
    { .Divider =  4u, .RegValue = LL_RCC_MCO1_DIV_4  },
    { .Divider =  8u, .RegValue = LL_RCC_MCO1_DIV_8  },
    { .Divider = 16u, .RegValue = LL_RCC_MCO1_DIV_16 },
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
 * is rejected without any change. MCO pin (PA8) is configured as alternate
 * function, LSCO is enabled in backup domain.
 *
 * \param outId     [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkSource [in]: Clock output signal source, value from \ref rcc_ClkOut_Source_t
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
        uint32_t                         regValue  = 0u;

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

        if( ( RCC_REQUEST_OK == retState      ) &&
            ( RCC_REG_BDCR   == outConfig->RegId )    )
        {
            /* Output located in backup domain */
            retState = Rcc_ClkSrc_Set_BkUpAccess();
        }
        else
        {
            /* Source does not belong to the clock output or output outside of backup domain */
        }

        if( RCC_REQUEST_OK == retState )
        {
            Rcc_Set_RegVal( outConfig->RegId, outConfig->SrcMask | outConfig->EnableMask, regValue | outConfig->EnableMask );
        }
        else
        {
            /* Source does not belong to the clock output - nothing is changed */
        }

        if( ( RCC_REQUEST_OK      == retState             ) &&
            ( RCC_FUNCTION_ACTIVE == outConfig->PinConfig )    )
        {
            gpio_Config_t gpioConfig;

            gpioConfig.PortId           = outConfig->PortId;
            gpioConfig.PinId            = outConfig->PinId;
            gpioConfig.PinMode          = GPIO_PIN_MODE_ALTERNATE;
            gpioConfig.PinPull          = GPIO_PIN_PULL_NONE;
            gpioConfig.PinSpeed         = GPIO_PIN_SPEED_VERY_HIGH;
            gpioConfig.PinOutType       = GPIO_PIN_OUTPUT_PUSHPULL;
            gpioConfig.PinAltFunction   = GPIO_ALT_FUNC_0;
            gpioConfig.PinActiveLevel   = GPIO_PIN_LEVEL_HIGH;

            (void)Gpio_Init( &gpioConfig );
        }
        else
        {
            /* Output pin is driven without GPIO configuration or source was rejected */
        }
    }

    return ( retState );
}


/**
 * \brief Function used to get clock output signal source.
 *
 * \ref RCC_CLK_SOURCE_NONE is returned for MCO without clock (MCOSEL = 0) and for
 * disabled LSCO (LSCOEN = 0).
 *
 * \param outId      [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkSource [out]: Pointer to store clock output source. Must not be NULL.
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
        const rcc_ClkOutConfig_t * const outConfig   = &rcc_ClkOutConfig[ outId ];
        const uint32_t                   regValue    = Rcc_Get_RegVal( outConfig->RegId, outConfig->SrcMask );
        const uint32_t                   enableValue = Rcc_Get_RegVal( outConfig->RegId, outConfig->EnableMask );

        if( ( 0u != outConfig->EnableMask ) &&
            ( 0u == enableValue           )    )
        {
            /* Output with enable bit is disabled - the selected source is not output */
            *clkSource = RCC_CLK_SOURCE_NONE;
            retState   = RCC_REQUEST_OK;
        }
        else
        {
            for( uint32_t srcIdx = 0u; RCC_CLK_OUT_SRC_LIST_CNT > srcIdx; srcIdx++ )
            {
                const uint32_t srcValue = rcc_ClkOutSrcConfig[ srcIdx ].RegValue & outConfig->SrcMask;

                if( ( outId    == rcc_ClkOutSrcConfig[ srcIdx ].OutId ) &&
                    ( regValue == srcValue                             )    )
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
 * \param outId      [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkDivider [in]: Clock output divider - MCO: 1, 2, 4, 8, 16; LSCO: 1 only
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_CLK_OUT_CNT <= outId )
    {
        /* Incorrect output ID */
        retState = RCC_REQUEST_ERROR;
    }
    else if( 0u == rcc_ClkOutConfig[ outId ].DivMask )
    {
        /* Output without prescaler accepts divider 1 only */
        if( RCC_CLK_OUT_DIV_NONE == clkDivider )
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
        for( uint32_t divIdx = 0u; RCC_CLK_OUT_DIV_LIST_CNT > divIdx; divIdx++ )
        {
            if( clkDivider == rcc_ClkOutDivConfig[ divIdx ].Divider )
            {
                Rcc_Set_RegVal( rcc_ClkOutConfig[ outId ].RegId, rcc_ClkOutConfig[ outId ].DivMask, rcc_ClkOutDivConfig[ divIdx ].RegValue );

                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Continue with next divider */
            }
        }
    }

    return ( retState );
}


/**
 * \brief Function used to get clock output divider.
 *
 * \param outId      [in]: Clock output identification, value from \ref rcc_ClkOut_Id_t
 * \param clkDivider [out]: Pointer to store clock output divider (1 for LSCO). Must not be NULL.
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
        if( 0u == rcc_ClkOutConfig[ outId ].DivMask )
        {
            /* Output without prescaler */
            *clkDivider = RCC_CLK_OUT_DIV_NONE;
            retState    = RCC_REQUEST_OK;
        }
        else
        {
            const uint32_t regVal = Rcc_Get_RegVal( rcc_ClkOutConfig[ outId ].RegId, rcc_ClkOutConfig[ outId ].DivMask );

            for( uint32_t divIdx = 0u; RCC_CLK_OUT_DIV_LIST_CNT > divIdx; divIdx++ )
            {
                if( regVal == rcc_ClkOutDivConfig[ divIdx ].RegValue )
                {
                    *clkDivider = rcc_ClkOutDivConfig[ divIdx ].Divider;
                    retState    = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Continue with next divider */
                }
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
