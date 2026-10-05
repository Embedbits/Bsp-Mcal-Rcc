/**
 * \author Mr.Nobody
 * \file Rcc_ClkOut.h
 * \ingroup Rcc
 * \brief Rcc module ClkOut component functionality.
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

/* ============================== TYPEDEFS ================================== */

/** \brief Clock output configuration (multiplexer field and output pin) */
typedef struct
{
    rcc_RegId_t   RegId;    /**< Register with source multiplexer field */
    uint32_t      SrcMask;  /**< Source multiplexer field mask          */
    gpio_PortId_t PortId;   /**< Output pin port                         */
    gpio_PinId_t  PinId;    /**< Output pin                              */
}   rcc_ClkOutConfig_t;


/** \brief Clock output source record */
typedef struct
{
    rcc_ClkOut_Id_t     OutId;     /**< Clock output                                          */
    rcc_ClkOut_Source_t ClkSource; /**< Clock output source                                   */
    uint32_t            RegValue;  /**< LL multiplexer value (masked by SrcMask of the output) */
}   rcc_ClkOutSrcConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** \brief Configuration of clock outputs, indexed by \ref rcc_ClkOut_Id_t */
static const rcc_ClkOutConfig_t rcc_ClkOutConfig[ RCC_CLK_OUT_CNT ] =
{
    [RCC_CLK_OUT_MCO1] = { .RegId = RCC_REG_CFGR1, .SrcMask = RCC_CFGR1_MCO1SEL_Msk, .PortId = GPIO_PORT_A, .PinId = GPIO_PIN_ID_8 },
    [RCC_CLK_OUT_MCO2] = { .RegId = RCC_REG_CFGR1, .SrcMask = RCC_CFGR1_MCO2SEL_Msk, .PortId = GPIO_PORT_C, .PinId = GPIO_PIN_ID_9 },
    [RCC_CLK_OUT_LSCO] = { .RegId = RCC_REG_BDCR,  .SrcMask = RCC_BDCR_LSCOSEL_Msk,  .PortId = GPIO_PORT_B, .PinId = GPIO_PIN_ID_2 },
};


/** \brief Selectable sources of clock outputs */
static const rcc_ClkOutSrcConfig_t rcc_ClkOutSrcConfig[ ] =
{
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI64 , .RegValue = LL_RCC_MCO1SOURCE_HSI     },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_LSE   , .RegValue = LL_RCC_MCO1SOURCE_LSE     },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSE   , .RegValue = LL_RCC_MCO1SOURCE_HSE     },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_PLL1Q , .RegValue = LL_RCC_MCO1SOURCE_PLL1Q   },
    { .OutId = RCC_CLK_OUT_MCO1, .ClkSource = RCC_CLK_SOURCE_MCO1_HSI48 , .RegValue = LL_RCC_MCO1SOURCE_HSI48   },
    { .OutId = RCC_CLK_OUT_MCO2, .ClkSource = RCC_CLK_SOURCE_MCO2_SYSCLK, .RegValue = LL_RCC_MCO2SOURCE_SYSCLK  },
    { .OutId = RCC_CLK_OUT_MCO2, .ClkSource = RCC_CLK_SOURCE_MCO2_PLL2P , .RegValue = LL_RCC_MCO2SOURCE_PLL2P   },
    { .OutId = RCC_CLK_OUT_MCO2, .ClkSource = RCC_CLK_SOURCE_MCO2_HSE   , .RegValue = LL_RCC_MCO2SOURCE_HSE     },
    { .OutId = RCC_CLK_OUT_MCO2, .ClkSource = RCC_CLK_SOURCE_MCO2_PLL1P , .RegValue = LL_RCC_MCO2SOURCE_PLL1P   },
    { .OutId = RCC_CLK_OUT_MCO2, .ClkSource = RCC_CLK_SOURCE_MCO2_CSI   , .RegValue = LL_RCC_MCO2SOURCE_CSI     },
    { .OutId = RCC_CLK_OUT_MCO2, .ClkSource = RCC_CLK_SOURCE_MCO2_LSI   , .RegValue = LL_RCC_MCO2SOURCE_LSI     },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSI   , .RegValue = LL_RCC_LSCO_CLKSOURCE_LSI },
    { .OutId = RCC_CLK_OUT_LSCO, .ClkSource = RCC_CLK_SOURCE_LSCO_LSE   , .RegValue = LL_RCC_LSCO_CLKSOURCE_LSE },
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
 * is rejected without any change.
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
                Rcc_Set_RegVal( outConfig->RegId, outConfig->SrcMask, rcc_ClkOutSrcConfig[ srcIdx ].RegValue );

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
            /* Source does not belong to the clock output - nothing is changed */
        }
    }

    return ( retState );
}


/**
 * \brief Function used to get clock output signal source.
 *
 * The source selected by the clock output multiplexer is returned (the multiplexer
 * always selects one source - \ref RCC_CLK_SOURCE_NONE is never returned).
 *
 * \param outId      [in]: Clock output identification
 * \param clkSource [out]: Pointer to MCO clock source
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
        const uint32_t                   regValue  = Rcc_Get_RegVal( outConfig->RegId, outConfig->SrcMask );

        for( uint32_t srcIdx = 0u; RCC_CLK_OUT_SRC_LIST_CNT > srcIdx; srcIdx++ )
        {
            if( ( outId    == rcc_ClkOutSrcConfig[ srcIdx ].OutId                               ) &&
                ( regValue == ( rcc_ClkOutSrcConfig[ srcIdx ].RegValue & outConfig->SrcMask ) )    )
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
 * \param clkDivider [in]: Clock output divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Set_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;
    uint32_t           regVal   = 0u;

    regVal = clkDivider;

    if( RCC_CLK_OUT_MCO1 == outId )
    {
        regVal = regVal << RCC_CFGR1_MCO1PRE_Pos;

        Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1PRE_Msk, regVal );
    }
    else if( RCC_CLK_OUT_MCO2 == outId )
    {
        regVal = regVal << RCC_CFGR1_MCO2PRE_Pos;

        Rcc_Set_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2PRE_Msk, regVal );
    }
    else
    {
        /* LSCO does not have prescaller */
    }

    return ( retState );
}


/**
 * \brief Function used to get MCO clock divider.
 *
 * \param outId      [in] : Clock output identification
 * \param clkDivider [out]: Pointer to MCO clock divider
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_ClkOut_Get_ClockDivider( rcc_ClkOut_Id_t outId, rcc_ClkOut_Div_t * const clkDivider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regVal   = 0u;

    if( ( RCC_NULL_PTR   != clkDivider ) &&
        ( RCC_CLK_OUT_CNT > outId      )    )
    {
        if( RCC_CLK_OUT_MCO1 == outId )
        {
            regVal = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO1PRE_Msk );

            regVal = regVal >> RCC_CFGR1_MCO1PRE_Pos;
        }
        else if( RCC_CLK_OUT_MCO2 == outId )
        {
            regVal = Rcc_Get_RegVal( RCC_REG_CFGR1, RCC_CFGR1_MCO2PRE_Msk );

            regVal = regVal >> RCC_CFGR1_MCO2PRE_Pos;
        }
        else
        {
            /* LSCO does not have prescaller */
            regVal = 0u;
        }

        *clkDivider = regVal;

        retState = RCC_REQUEST_OK;
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
