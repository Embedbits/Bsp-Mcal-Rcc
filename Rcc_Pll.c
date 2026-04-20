/**
 * \author Mr.Nobody
 * \file Rcc_Pll.h
 * \ingroup Rcc
 * \brief Reset & Clock Control (RCC) module Phase Locked Loop (PLL) handler
 *        functionality.
 */
/* ============================== INCLUDES ================================== */
#include "Rcc_Pll.h"                        /* Self include                   */
#include "Rcc_Port.h"                       /* Own port file include          */
#include "Rcc_Types.h"                      /* Module types definitions       */
#include "Rcc_Reg.h"                        /* Registry operations include    */
#include "Rcc_ClkSrc.h"                     /* Clock sources functionality    */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Maximal wait time for configuration request confirmation */
#define RCC_PLL_TIMEOUT_RAW             ( 0x84FCB )

/** Minimal value of internal PLL frequency in Hz (Actually 120 MHz) */
#define RCC_PLL_INT_FREQUENCY_MIN       ( 128000000u )
/** Maximal value of internal PLL frequency in Hz (Actually 560 MHz) */
#define RCC_PLL_INT_FREQUENCY_MAX       ( 560000000u )

/* ============================== TYPEDEFS ================================== */

typedef struct
{
    rcc_PllId_t       PllId;             /**< Phase Locked Loop (PLL) ID                     */

    rcc_RegId_t       StateRegId;        /**< PLL activation state register ID               */
    volatile uint32_t StateMask;         /**< PLL activation state mask                      */

    rcc_RegId_t       ClkSrcRegId;       /**< PLL clock source register ID                   */
    volatile uint32_t ClkSrcMask;        /**< PLL clock source mask                          */

    rcc_RegId_t       RdyFlagRegId;      /**< PLL ready flag register ID                     */
    volatile uint32_t RdyFlagMask;       /**< PLL ready flag mask                            */

    rcc_RegId_t       VcoRangeRegId;     /** VCO frequency range configuration register ID   */
    volatile uint32_t VcoRangeMask;      /** VCO frequency range configuration mask          */

    rcc_RegId_t       FreqInRangeRegId;  /** Input frequency range configuration register ID */
    volatile uint32_t FreqInRangeMask;   /** Input frequency range configuration mask        */

    /* ------------------- Input divider M configuration -------------------- */

    rcc_RegId_t       M_DivRegId;        /**< PLL M divider register ID                      */
    volatile uint32_t M_DivMask;         /**< PLL M divider mask                             */

    rcc_PllMDivider_t M_DivStepsize;     /**< PLL M divider step size                        */
    rcc_PllMDivider_t M_DivMaxValue;     /**< PLL M divider maximal value                    */
    rcc_PllMDivider_t M_DivMinValue;     /**< PLL M divider minimal value                    */

    /* ----------------- Internal multiplier N configuration ---------------- */

    rcc_RegId_t       N_MultRegId;       /**< PLL N multiplier register ID                   */
    volatile uint32_t N_MultMask;        /**< PLL N multiplier mask                          */

    rcc_PllNMult_t    N_MultStepsize;    /**< PLL N multiplier step size                     */
    rcc_PllNMult_t    N_MultMaxValue;    /**< PLL N multiplier maximal value                 */
    rcc_PllNMult_t    N_MultMinValue;    /**< PLL N multiplier minimal value                 */

    /* ----------------------- Output P configuration ----------------------- */

    rcc_RegId_t       Out_P_StateRegId;  /** PLL output P activation state register ID       */
    volatile uint32_t Out_P_StateMask;   /** PLL output P activation state mask              */

    rcc_RegId_t       Out_P_ConfRegId;   /**< PLL output P configuration register ID         */
    volatile uint32_t Out_P_ConfMask;    /**< PLL output P configuration mask                */

    rcc_PllPDivider_t Out_P_DivStepsize; /**< PLL Q divider step size                        */
    rcc_PllPDivider_t Out_P_DivMaxValue; /**< PLL Q divider maximal value                    */
    rcc_PllPDivider_t Out_P_DivMinValue; /**< PLL Q divider minimal value                    */

    /* ----------------------- Output Q configuration ----------------------- */

    rcc_RegId_t       Out_Q_StateRegId;  /** PLL output Q activation state register ID       */
    volatile uint32_t Out_Q_StateMask;   /** PLL output Q activation state mask              */

    rcc_RegId_t       Out_Q_ConfRegId;   /** PLL output Q configuration register ID          */
    volatile uint32_t Out_Q_ConfMask;    /** PLL output Q configuration mask                 */

    rcc_PllQDivider_t Out_Q_DivStepsize; /**< PLL Q divider step size                        */
    rcc_PllQDivider_t Out_Q_DivMaxValue; /**< PLL Q divider maximal value                    */
    rcc_PllQDivider_t Out_Q_DivMinValue; /**< PLL Q divider minimal value                    */

    /* ----------------------- Output R configuration ----------------------- */

    rcc_RegId_t       Out_R_StateRegId;  /** PLL output R activation state register ID       */
    volatile uint32_t Out_R_StateMask;   /** PLL output R activation state mask              */

    rcc_RegId_t       Out_R_ConfRegId;   /** PLL output R configuration register ID          */
    volatile uint32_t Out_R_ConfMask;    /** PLL output R configuration mask                 */

    rcc_PllRDivider_t Out_R_DivStepsize; /**< PLL R divider step size                        */
    rcc_PllRDivider_t Out_R_DivMaxValue; /**< PLL R divider maximal value                    */
    rcc_PllRDivider_t Out_R_DivMinValue; /**< PLL R divider minimal value                    */

}   rcc_PllConfig_t;

/* ======================== FORWARD DECLARATIONS ============================ */

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */


/* ------------------------- Peripherals arrays ----------------------------- */

/** \brief Phase Locked Loop's (PLL) configuration registry structure */
static const rcc_PllConfig_t            rcc_Pll_Config[ RCC_PLL_CNT ] =
{
  {
    .PllId             = RCC_PLL_1,

    .StateRegId        = RCC_REG_CR,
    .StateMask         = RCC_CR_PLL1ON,

    .ClkSrcRegId       = RCC_REG_PLL1CFGR,
    .ClkSrcMask        = RCC_PLL1CFGR_PLL1SRC,

    .RdyFlagRegId      = RCC_REG_CR,
    .RdyFlagMask       = RCC_CR_PLL1RDY,

    .VcoRangeRegId     = RCC_REG_PLL1CFGR,
    .VcoRangeMask      = RCC_PLL1CFGR_PLL1VCOSEL,

    .FreqInRangeRegId  = RCC_REG_PLL1CFGR,
    .FreqInRangeMask   = RCC_PLL1CFGR_PLL1RGE,

    /* ------------------- Input divider M configuration -------------------- */

    .M_DivRegId        = RCC_REG_PLL1CFGR,
    .M_DivMask         = RCC_PLL1CFGR_PLL1M,

    .M_DivStepsize     = 1u,  /**< PLL 1 input divider value step size. */
    .M_DivMaxValue     = 1u,  /**< PLL 1 input divider minimum value.   */
    .M_DivMinValue     = 63u, /**< PLL 1 input divider maximum value.   */

    /* ----------------- Internal multiplier N configuration ---------------- */

    .N_MultRegId       = RCC_REG_PLL1DIVR,
    .N_MultMask        = RCC_PLL1DIVR_PLL1N,

    .N_MultStepsize    = 1u,   /**< PLL 1 internal multiplier value step size. */
    .N_MultMinValue    = 4u,   /**< PLL 1 internal multiplier minimum value.   */
    .N_MultMaxValue    = 512u, /**< PLL 1 internal multiplier maximum value.   */

    /* ----------------------- Output P configuration ----------------------- */

    .Out_P_StateRegId  = RCC_REG_PLL1CFGR,
    .Out_P_StateMask   = RCC_PLL1CFGR_PLL1PEN,

    .Out_P_ConfRegId   = RCC_REG_PLL1DIVR,
    .Out_P_ConfMask    = RCC_PLL1DIVR_PLL1P,

    .Out_P_DivStepsize = 2u,   /**< PLL 1 Output P value step size. */
    .Out_P_DivMinValue = 2u,   /**< PLL 1 Output P minimum value.   */
    .Out_P_DivMaxValue = 128u, /**< PLL 1 Output P maximum value.   */

    /* ----------------------- Output Q configuration ----------------------- */

    .Out_Q_StateRegId  = RCC_REG_PLL1CFGR,
    .Out_Q_StateMask   = RCC_PLL1CFGR_PLL1QEN,

    .Out_Q_ConfRegId   = RCC_REG_PLL1DIVR,
    .Out_Q_ConfMask    = RCC_PLL1DIVR_PLL1Q,

    .Out_Q_DivStepsize = 1u,   /**< PLL 1 Output Q value step size. */
    .Out_Q_DivMinValue = 1u,   /**< PLL 1 Output Q minimum value.   */
    .Out_Q_DivMaxValue = 128u, /**< PLL 1 Output Q maximum value.   */

    /* ----------------------- Output R configuration ----------------------- */

    .Out_R_StateRegId  = RCC_REG_PLL1CFGR,
    .Out_R_StateMask   = RCC_PLL1CFGR_PLL1REN,

    .Out_R_ConfRegId   = RCC_REG_PLL1DIVR,
    .Out_R_ConfMask    = RCC_PLL1DIVR_PLL1R,

    .Out_R_DivStepsize = 1u,   /**< PLL 1 Output R value step size. */
    .Out_R_DivMinValue = 1u,   /**< PLL 1 Output R minimum value.   */
    .Out_R_DivMaxValue = 128u, /**< PLL 1 Output R maximum value.   */
  },
  {
    .PllId             = RCC_PLL_2,

    .StateRegId        = RCC_REG_CR,
    .StateMask         = RCC_CR_PLL2ON,

    .ClkSrcRegId       = RCC_REG_PLL2CFGR,
    .ClkSrcMask        = RCC_PLL2CFGR_PLL2SRC,

    .RdyFlagRegId      = RCC_REG_CR,
    .RdyFlagMask       = RCC_CR_PLL2RDY,

    .VcoRangeRegId     = RCC_REG_PLL2CFGR,
    .VcoRangeMask      = RCC_PLL2CFGR_PLL2VCOSEL,

    .FreqInRangeRegId  = RCC_REG_PLL2CFGR,
    .FreqInRangeMask   = RCC_PLL2CFGR_PLL2RGE,

    /* ------------------- Input divider M configuration -------------------- */

    .M_DivRegId        = RCC_REG_PLL2CFGR,
    .M_DivMask         = RCC_PLL2CFGR_PLL2M,

    .M_DivStepsize     = 1u,  /**< PLL 2 input divider value step size. */
    .M_DivMaxValue     = 1u,  /**< PLL 2 input divider minimum value.   */
    .M_DivMinValue     = 63u, /**< PLL 2 input divider maximum value.   */

    /* ----------------- Internal multiplier N configuration ---------------- */

    .N_MultRegId       = RCC_REG_PLL2DIVR,
    .N_MultMask        = RCC_PLL2DIVR_PLL2N,

    .N_MultStepsize    = 1u,   /**< PLL 2 internal multiplier value step size. */
    .N_MultMinValue    = 4u,   /**< PLL 2 internal multiplier minimum value.   */
    .N_MultMaxValue    = 512u, /**< PLL 2 internal multiplier maximum value.   */

    /* ----------------------- Output P configuration ----------------------- */

    .Out_P_StateRegId  = RCC_REG_PLL2CFGR,
    .Out_P_StateMask   = RCC_PLL2CFGR_PLL2PEN,

    .Out_P_ConfRegId   = RCC_REG_PLL2DIVR,
    .Out_P_ConfMask    = RCC_PLL2DIVR_PLL2P,

    .Out_P_DivStepsize = 1u,   /**< PLL 2 Output P value step size. */
    .Out_P_DivMinValue = 1u,   /**< PLL 2 Output P minimum value.   */
    .Out_P_DivMaxValue = 128u, /**< PLL 2 Output P maximum value.   */

    /* ----------------------- Output Q configuration ----------------------- */

    .Out_Q_StateRegId  = RCC_REG_PLL2CFGR,
    .Out_Q_StateMask   = RCC_PLL2CFGR_PLL2QEN,

    .Out_Q_ConfRegId   = RCC_REG_PLL2DIVR,
    .Out_Q_ConfMask    = RCC_PLL2DIVR_PLL2Q,

    .Out_Q_DivStepsize = 1u,   /**< PLL 2 Output Q value step size. */
    .Out_Q_DivMinValue = 1u,   /**< PLL 2 Output Q minimum value.   */
    .Out_Q_DivMaxValue = 128u, /**< PLL 2 Output Q maximum value.   */

    /* ----------------------- Output R configuration ----------------------- */

    .Out_R_StateRegId  = RCC_REG_PLL2CFGR,
    .Out_R_StateMask   = RCC_PLL2CFGR_PLL2REN,

    .Out_R_ConfRegId   = RCC_REG_PLL2DIVR,
    .Out_R_ConfMask    = RCC_PLL2DIVR_PLL2R,

    .Out_R_DivStepsize = 1u,   /**< PLL 2 Output R value step size. */
    .Out_R_DivMinValue = 1u,   /**< PLL 2 Output R minimum value.   */
    .Out_R_DivMaxValue = 128u, /**< PLL 2 Output R maximum value.   */
  },
  {
    .PllId             = RCC_PLL_3,

    .StateRegId        = RCC_REG_CR,
    .StateMask         = RCC_CR_PLL3ON,

    .ClkSrcRegId       = RCC_REG_PLL3CFGR,
    .ClkSrcMask        = RCC_PLL3CFGR_PLL3SRC,

    .RdyFlagRegId      = RCC_REG_CR,
    .RdyFlagMask       = RCC_CR_PLL3RDY,

    .VcoRangeRegId     = RCC_REG_PLL3CFGR,
    .VcoRangeMask      = RCC_PLL3CFGR_PLL3VCOSEL,

    .FreqInRangeRegId  = RCC_REG_PLL3CFGR,
    .FreqInRangeMask   = RCC_PLL3CFGR_PLL3RGE,

    /* ------------------- Input divider M configuration -------------------- */

    .M_DivRegId        = RCC_REG_PLL3CFGR,
    .M_DivMask         = RCC_PLL3CFGR_PLL3M,

    .M_DivStepsize     = 1u,  /**< PLL 3 input divider value step size. */
    .M_DivMaxValue     = 1u,  /**< PLL 3 input divider minimum value.   */
    .M_DivMinValue     = 63u, /**< PLL 3 input divider maximum value.   */

    /* ----------------- Internal multiplier N configuration ---------------- */

    .N_MultRegId       = RCC_REG_PLL3DIVR,
    .N_MultMask        = RCC_PLL3DIVR_PLL3N,

    .N_MultStepsize    = 1u,   /**< PLL 3 internal multiplier value step size. */
    .N_MultMinValue    = 4u,   /**< PLL 3 internal multiplier minimum value.   */
    .N_MultMaxValue    = 512u, /**< PLL 3 internal multiplier maximum value.   */

    /* ----------------------- Output P configuration ----------------------- */

    .Out_P_StateRegId  = RCC_REG_PLL3CFGR,
    .Out_P_StateMask   = RCC_PLL3CFGR_PLL3PEN,

    .Out_P_ConfRegId   = RCC_REG_PLL3DIVR,
    .Out_P_ConfMask    = RCC_PLL3DIVR_PLL3P,

    .Out_P_DivStepsize = 1u,   /**< PLL 2 Output P value step size. */
    .Out_P_DivMinValue = 1u,   /**< PLL 2 Output P minimum value.   */
    .Out_P_DivMaxValue = 128u, /**< PLL 2 Output P maximum value.   */

    /* ----------------------- Output Q configuration ----------------------- */

    .Out_Q_StateRegId  = RCC_REG_PLL3CFGR,
    .Out_Q_StateMask   = RCC_PLL3CFGR_PLL3QEN,

    .Out_Q_ConfRegId   = RCC_REG_PLL3DIVR,
    .Out_Q_ConfMask    = RCC_PLL3DIVR_PLL3Q,

    .Out_Q_DivStepsize = 1u,   /**< PLL 2 Output Q value step size. */
    .Out_Q_DivMinValue = 1u,   /**< PLL 2 Output Q minimum value.   */
    .Out_Q_DivMaxValue = 128u, /**< PLL 2 Output Q maximum value.   */

    /* ----------------------- Output R configuration ----------------------- */

    .Out_R_StateRegId  = RCC_REG_PLL3CFGR,
    .Out_R_StateMask   = RCC_PLL3CFGR_PLL3REN,

    .Out_R_ConfRegId   = RCC_REG_PLL3DIVR,
    .Out_R_ConfMask    = RCC_PLL3DIVR_PLL3R,

    .Out_R_DivStepsize = 1u,   /**< PLL 2 Output R value step size. */
    .Out_R_DivMinValue = 1u,   /**< PLL 2 Output R minimum value.   */
    .Out_R_DivMaxValue = 128u, /**< PLL 2 Output R maximum value.   */
  },
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Initializes Phase Locked Loop (PLL) handler module.
 *
 * During initialization process, module checks correctness of Phase Locked
 * Loop (PLL) configuration structure.
 */
rcc_RequestState_t Rcc_Pll_Init( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        if( pllId != rcc_Pll_Config[ pllId ].PllId )
        {
            retState = RCC_REQUEST_ERROR;
            break;
        }
    }

    return ( retState );
}


/**
 * \brief De-initializes Phase Locked Loop (PLL) handler module.
 *
 * De-initialization process disables all Phase Locked Loop (PLL) blocks.
 */
rcc_RequestState_t Rcc_Pll_Deinit( void )
{
    rcc_RequestState_t retState = RCC_REQUEST_OK;

    for( rcc_PllId_t pllId = RCC_PLL_1; RCC_PLL_CNT > pllId; pllId++ )
    {
        retState = Rcc_Pll_Set_Inactive( pllId );

        if( retState != RCC_REQUEST_OK )
        {
            break;
        }
    }

    return ( retState );
}


/**
 * \brief Main task of module Rcc_Pll
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's 
 * requirements.
 */
void Rcc_Pll_Task( void )
{
    return;
}

/*---------------------------- Pll's configuration ---------------------------*/

/**
 * \brief Configuration of main Phase Locked Loop (PLL)
 *
 * User can configure dividers and multipliers of main PLL through PLL
 * configuration structure.
 *
 * \warning User must configure clock source before PLL configuration!
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 * \param configStruct [in]: PLL configuration structure
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Config( rcc_PllId_t pllId, rcc_PllConfigStruct_t * const configStruct )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != configStruct ) &&
        ( RCC_PLL_CNT   >  pllId       )    )
    {
        if( RCC_PLL_SRC_NONE == configStruct->Pll_Source )
        {
            Rcc_Pll_Set_Inactive( pllId );
        }
        else
        {
            /* PLL must be inactive during configuration */
            retState = Rcc_Pll_Set_Inactive( pllId );

            if( RCC_REQUEST_OK == retState )
            {
                retState = Rcc_Pll_Set_Source( pllId, configStruct->Pll_Source );
            }

            /*------------- Configure PLL internal clock ---------------------*/

            if( ( rcc_Pll_Config[ pllId ].M_DivMinValue  <= configStruct->M_Divider    ) &&
                ( rcc_Pll_Config[ pllId ].M_DivMaxValue  >= configStruct->M_Divider    ) &&
                ( rcc_Pll_Config[ pllId ].N_MultMinValue <= configStruct->N_Multiplier ) &&
                ( rcc_Pll_Config[ pllId ].N_MultMaxValue >= configStruct->N_Multiplier )    )
            {
                /* -------- Configure PLL M divider and N multiplier -------- */

                if( RCC_REQUEST_ERROR != retState )
                {
                    Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].M_DivRegId,
                                    rcc_Pll_Config[ pllId ].M_DivMask,
                                    ( configStruct->M_Divider - 1u ) << RCC_PLL1CFGR_PLL1M_Pos );

                    Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].N_MultRegId,
                                    rcc_Pll_Config[ pllId ].N_MultMask,
                                    configStruct->N_Multiplier << RCC_PLL1DIVR_PLL1N_Pos );
                }

                /* ---------- Configure PLL input frequency range ----------- */

                rcc_FreqHz_t freqInHz = 0u;
                uint32_t     regValue = 0u;

                if( RCC_REQUEST_OK != retState )
                {
                    if( RCC_PLL_SRC_CSI == configStruct->Pll_Source )
                    {
                        retState = Rcc_ClkSrc_Get_CsiClk( &freqInHz );
                    }
                    else if( RCC_PLL_SRC_HSE == configStruct->Pll_Source )
                    {
                        retState = Rcc_ClkSrc_Get_HseClk( &freqInHz );
                    }
                    else if( RCC_PLL_SRC_HSI == configStruct->Pll_Source )
                    {
                        retState = Rcc_ClkSrc_Get_Hsi64Clk( &freqInHz );
                    }
                    else
                    {
                        /* Unsupported PLL source */
                        retState = RCC_REQUEST_ERROR;
                    }

                    if( RCC_REQUEST_ERROR != retState )
                    {
                        if( ( 1000000u <= freqInHz ) &&
                            ( 2000000u >= freqInHz )    )
                        {
                            /* Input frequency range 1: 1 MHz - 2 MHz */
                            regValue = 0u;
                        }
                        else if( ( 2000000u <  freqInHz ) &&
                                 ( 4000000u >= freqInHz )    )
                        {
                            /* Input frequency range 2: 2 MHz - 4 MHz */
                            regValue = 1u;
                        }
                        else if( ( 4000000u <  freqInHz ) &&
                                 ( 8000000u >= freqInHz )    )
                        {
                            /* Input frequency range 3: 4 MHz - 8 MHz */
                            regValue = 2u;
                        }
                        else if( ( 8000000u  <  freqInHz ) &&
                                 ( 16000000u >= freqInHz )    )
                        {
                            /* Input frequency range 4: 8 MHz - 16 MHz */
                            regValue = 3u;
                        }
                        else
                        {
                            /* Incorrect input frequency */
                            retState = RCC_REQUEST_ERROR;
                        }
                    }

                    if( RCC_REQUEST_ERROR != retState )
                    {
                        Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].FreqInRangeRegId,
                                        rcc_Pll_Config[ pllId ].FreqInRangeMask,
                                        regValue );
                    }
                }

                /* ------------- Configure VCO frequency range -------------- */

                rcc_FreqHz_t      vcoFreqHz = 0u;
                rcc_PllMDivider_t inputDiv  = configStruct->M_Divider;

                if( 0u == inputDiv )
                {
                    inputDiv = 1u;
                }
                else
                {
                    /* Input divider is not bypassed (value 0) */
                }

                vcoFreqHz = ( freqInHz / inputDiv ) * configStruct->N_Multiplier;

                if( RCC_REQUEST_ERROR != retState )
                {
                    if( ( 150000000u <= vcoFreqHz ) &&
                        ( 420000000u >= vcoFreqHz )    )
                    {
                        regValue = 1u;
                    }
                    else
                    {
                        /* VCO frequency is out of medium VCO range, thus wide
                         * range has to be activated. */
                        regValue = 0u;
                    }

                    Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].VcoRangeRegId,
                                    rcc_Pll_Config[ pllId ].VcoRangeMask,
                                    regValue );
                }
            }
            else
            {
                /* Incorrect PLL internal configuration */
                retState = RCC_REQUEST_ERROR;
            }


            /*-------------------- Configure PLL P output --------------------*/

            if( ( 0u             != configStruct->P_Divider ) &&
                ( RCC_REQUEST_OK == retState                )    )
            {
                retState = Rcc_Pll_Set_OutP( pllId, configStruct->P_Divider );
            }
            else
            {
                /* PLL output P is not used. */
            }

            /*-------------------- Configure PLL Q output --------------------*/

            if( ( 0u             != configStruct->Q_Divider ) &&
                ( RCC_REQUEST_OK == retState                )    )
            {
                retState = Rcc_Pll_Set_OutQ( pllId, configStruct->Q_Divider );
            }
            else
            {
                /* PLL output Q is not used. */
            }

            /*-------------------- Configure PLL R output --------------------*/

            if( ( 0u             != configStruct->R_Divider ) &&
                ( RCC_REQUEST_OK == retState                )    )
            {
                retState = Rcc_Pll_Set_OutR( pllId, configStruct->R_Divider );
            }
            else
            {
                /* PLL output Q is not used. */
            }

            /* ------------- Activate PLL if no error occurred -------------- */

            if( RCC_REQUEST_ERROR != retState )
            {
                /* Activate PLL. This must be the last step! */
                Rcc_Pll_Set_Active( pllId );
            }
            else
            {
                /* Some error occurred during PLL configuration. Do not activate
                 * the PLL. */
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
 * \brief Returns PLL internal frequency
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \param busClk [out]: Pointer to PLL internal frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *        otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_InternalClk( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState     = RCC_REQUEST_ERROR;
    rcc_PllClkSrc_t    pllClkSource = 0u;
    rcc_FreqHz_t       pllIntFreq   = 0u;
    uint32_t           pllNMult     = 0u;
    uint32_t           pllMDiv      = 0u;
    uint32_t           inputClkFreq = 0u;

    retState = Rcc_Pll_Get_Source( pllId, &pllClkSource );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        rcc_PllClkSrc_t pllClkSource = RCC_PLL_SRC_NONE;

        Rcc_Pll_Get_Source( pllId, &pllClkSource );

        if( RCC_PLL_SRC_HSE == pllClkSource )
        {
            retState = Rcc_ClkSrc_Get_HseClk( &inputClkFreq );
        }
        if( RCC_PLL_SRC_CSI == pllClkSource )
        {
            retState = Rcc_ClkSrc_Get_CsiClk( &inputClkFreq );
        }
        else if( RCC_PLL_SRC_HSI == pllClkSource )
        {
            retState = Rcc_ClkSrc_Get_Hsi64Clk( &inputClkFreq );
        }
        else
        {
            retState     = RCC_REQUEST_ERROR;
            inputClkFreq = 0u;
        }

        uint32_t mDivRegValue  = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].M_DivRegId , rcc_Pll_Config[ pllId ].M_DivMask  );
        uint32_t nMultRegValue = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].N_MultRegId, rcc_Pll_Config[ pllId ].N_MultMask );

        pllNMult = ( nMultRegValue >> RCC_PLL1DIVR_PLL1N_Pos );
        pllMDiv  = ( mDivRegValue  >> RCC_PLL1CFGR_PLL1M_Pos );


        if( 0u != pllMDiv )
        {
            pllIntFreq = ( ( inputClkFreq / pllMDiv ) * pllNMult );
        }
        else
        {
            pllIntFreq = 0u;
        }

        if( RCC_REQUEST_ERROR != retState )
        {
            *pllClk = pllIntFreq;

            retState = RCC_REQUEST_OK;
        }
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Activation of Phase Locked Loop (LSI) block
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Active( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Set_RegBit( rcc_Pll_Config[ pllId ].StateRegId, rcc_Pll_Config[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( rcc_Pll_Config[ pllId ].StateRegId, rcc_Pll_Config[ pllId ].RdyFlagMask );

            if( 0u != regValue )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
                retState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief De-activation of Phase Locked Loop (LSI) block
 *
 * \param pllId [in]: Required Phase Locked Loop (PLL) identification.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Inactive( rcc_PllId_t pllId )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;
    uint32_t           regValue    = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        Rcc_Reset_RegBit( rcc_Pll_Config[ pllId ].StateRegId, rcc_Pll_Config[ pllId ].StateMask );

        for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
        {
            regValue = Rcc_Get_RegBit( rcc_Pll_Config[ pllId ].StateRegId, rcc_Pll_Config[ pllId ].StateMask );

            if( 0u == regValue )
            {
                retState = RCC_REQUEST_OK;
                break;
            }
            else
            {
                /* Clock source has not yet been changed, keep return state as error */
                retState = RCC_REQUEST_ERROR;
            }
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading status of Phase Locked Loop (PLL) block
 *
 * \param retState [out]: Pointer to actual status value
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_State( rcc_PllId_t pllId, rcc_FunctionState_t * const state )
{
    rcc_RequestState_t retState       = RCC_REQUEST_ERROR;
    uint32_t           readyRegVal    = 0u;
    uint32_t           pllStateRegVal = 0u;

    if( ( RCC_NULL_PTR != state ) &&
        ( RCC_PLL_CNT   > pllId )    )
    {
        readyRegVal = Rcc_Get_RegBit( rcc_Pll_Config[ pllId ].RdyFlagRegId, rcc_Pll_Config[ pllId ].RdyFlagMask );

        pllStateRegVal = Rcc_Get_RegBit( rcc_Pll_Config[ pllId ].StateRegId, rcc_Pll_Config[ pllId ].StateMask );

        if ( ( 0u != readyRegVal    ) &&
             ( 0u != pllStateRegVal )    )
        {
            *state = RCC_FUNCTION_ACTIVE;
        }
        else
        {
            *state = RCC_FUNCTION_INACTIVE;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Selection of clock source for Phase Locked Loop's multiplexer
 *
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_NONE : PLL is inactive
 *  - \ref RCC_PLL_SRC_CSI  : PLL will be clocked by CSI oscillator
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_Source( rcc_PllId_t pllId, rcc_PllClkSrc_t clkSource )
{
    rcc_RequestState_t retState  = RCC_REQUEST_ERROR;
    uint32_t           targetRegVal = 0u;

    if( RCC_PLL_CNT > pllId )
    {
        if( RCC_PLL_SRC_CSI == clkSource )
        {
            targetRegVal = LL_RCC_PLL1SOURCE_CSI;

            retState = Rcc_ClkSrc_Set_CsiActive();
        }
        else if( RCC_PLL_SRC_HSE == clkSource )
        {
            targetRegVal = LL_RCC_PLL1SOURCE_HSE;

            retState = RCC_REQUEST_OK;
        }
        else if( RCC_PLL_SRC_HSI  == clkSource )
        {
            targetRegVal = LL_RCC_PLL1SOURCE_HSI;

            retState = Rcc_ClkSrc_Set_Hsi64Active();
        }
        else
        {
            retState = RCC_REQUEST_ERROR;

            targetRegVal = 0u;
        }

        if( RCC_REQUEST_ERROR != retState )
        {
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].ClkSrcRegId,
                            rcc_Pll_Config[ pllId ].ClkSrcMask,
                            targetRegVal );

            for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
            {
                uint32_t regValue = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].ClkSrcRegId,
                                                    rcc_Pll_Config[ pllId ].ClkSrcMask );

                regValue = ( regValue & RCC_PLL1CFGR_PLL1SRC ) >> RCC_PLL1CFGR_PLL1SRC_Pos;

                if( regValue == targetRegVal )
                {
                    retState = RCC_REQUEST_OK;
                    break;
                }
                else
                {
                    /* Clock source has not yet been changed, keep return state as error */
                    retState = RCC_REQUEST_ERROR;
                }
            }
        }
        else
        {
            /* Incorrect clock source was requested */
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Selection of clock source for Phase Locked Loop's multiplexer
 *
 * \param clkSource [in]: Phase Locked Loop's clock source ID. Can be one of enumeration:
 *  - \ref RCC_PLL_SRC_NONE : PLL is inactive
 *  - \ref RCC_PLL_SRC_CSI  : PLL will be clocked by CSI oscillator
 *  - \ref RCC_PLL_SRC_HSE  : PLL will be clocked by HSE oscillator
 *  - \ref RCC_PLL_SRC_HSI  : PLL will be clocked by HSI oscillator
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Source( rcc_PllId_t pllId, rcc_PllClkSrc_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( ( RCC_NULL_PTR != clkSource ) &&
        ( RCC_PLL_CNT   > pllId     )    )
    {
        uint32_t regValue = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].ClkSrcRegId,
                                            rcc_Pll_Config[ pllId ].ClkSrcMask );

        regValue = ( regValue & rcc_Pll_Config[ pllId ].ClkSrcMask ) >> RCC_PLL1CFGR_PLL1SRC_Pos;

        if( LL_RCC_PLL1SOURCE_CSI == regValue )
        {
            *clkSource = RCC_PLL_SRC_CSI;
        }
        else if( LL_RCC_PLL1SOURCE_HSI == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSI;
        }
        else if( LL_RCC_PLL1SOURCE_HSE == regValue )
        {
            *clkSource = RCC_PLL_SRC_HSE;
        }
        else
        {
            *clkSource = RCC_PLL_SRC_NONE;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Phase Locked Loop (PLL) output P divider configuration.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output P divider value.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutP( rcc_PllId_t pllId, rcc_PllPDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        uint32_t modulo = divider % rcc_Pll_Config[ pllId ].Out_P_DivStepsize;

        if( ( rcc_Pll_Config[ pllId ].Out_P_DivMinValue <= divider ) &&
            ( rcc_Pll_Config[ pllId ].Out_P_DivMaxValue >= divider ) &&
            ( 0u                                        == modulo  )    )
        {
            uint32_t regValue = divider - 1u;

            /* Bit shift "RCC_PLL1DIVR_PLL1P_Pos" is used for all PLL's.
                                 * At least this ST did not screw up. */
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].Out_P_ConfRegId,
                            rcc_Pll_Config[ pllId ].Out_P_ConfMask,
                            regValue << RCC_PLL1DIVR_PLL1P_Pos );

            /* Activate output P */
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].Out_P_StateRegId,
                            rcc_Pll_Config[ pllId ].Out_P_StateMask,
                            1u << RCC_PLL1CFGR_PLL1PEN_Pos );

            retState = RCC_REQUEST_OK;
        }
        else
        {
            /* Incorrect PLL output P divider value */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency from main PLL from output P
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutP( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        uint32_t regVal = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].Out_P_ConfRegId,
                                          rcc_Pll_Config[ pllId ].Out_P_ConfMask );

        regVal = ( ( regVal & rcc_Pll_Config[ pllId ].Out_P_ConfMask ) >> RCC_PLL1DIVR_PLL1P_Pos ) + 1u;

        if( 0u != regVal )
        {
            *pllClk = ( pllIntFreq / regVal );
        }
        else
        {
            *pllClk = 0u;
        }

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Phase Locked Loop (PLL) output Q divider configuration.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output Q divider value.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutQ( rcc_PllId_t pllId, rcc_PllQDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        uint32_t modulo = divider % rcc_Pll_Config[ pllId ].Out_Q_DivStepsize;

        if( ( rcc_Pll_Config[ pllId ].Out_Q_DivMinValue <= divider ) &&
            ( rcc_Pll_Config[ pllId ].Out_Q_DivMaxValue >= divider ) &&
            ( 0u                                        == modulo  )    )
        {
            uint32_t regValue = divider - 1u;

            /* Bit shift "RCC_PLL1DIVR_PLL1Q_Pos" is used for all PLL's.
                                 * At least this ST did not screw up. */
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].Out_Q_ConfRegId,
                            rcc_Pll_Config[ pllId ].Out_Q_ConfMask,
                            regValue << RCC_PLL1DIVR_PLL1Q_Pos );

            /* Activate output Q */
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].Out_Q_StateRegId,
                            rcc_Pll_Config[ pllId ].Out_Q_StateMask,
                            1u << RCC_PLL1CFGR_PLL1QEN_Pos );

            retState = RCC_REQUEST_OK;
        }
        else
        {
            /* Incorrect PLL output Q divider value */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency from main PLL from output Q
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutQ( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

    if ((RCC_REQUEST_ERROR != retState) && ( RCC_NULL_PTR != pllClk))
    {
        uint32_t regVal = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].Out_Q_ConfRegId,
                                          rcc_Pll_Config[ pllId ].Out_Q_ConfMask );

        regVal = ( ( regVal & rcc_Pll_Config[ pllId ].Out_Q_ConfMask ) >> RCC_PLL1DIVR_PLL1Q_Pos ) + 1u;

        pllIntFreq = ( pllIntFreq / regVal );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return (retState);
}


/**
 * \brief Phase Locked Loop (PLL) output R divider configuration.
 *
 * \param pllId   [in]: Required Phase Locked Loop (PLL) identification.
 * \param divider [in]: Required PLL output R divider value.
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_OutR( rcc_PllId_t pllId, rcc_PllRDivider_t divider )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_PLL_CNT > pllId )
    {
        uint32_t modulo = divider % rcc_Pll_Config[ pllId ].Out_R_DivStepsize;

        if( ( rcc_Pll_Config[ pllId ].Out_R_DivMinValue <= divider ) &&
            ( rcc_Pll_Config[ pllId ].Out_R_DivMaxValue >= divider ) &&
            ( 0u                                        == modulo  )    )
        {
            uint32_t regValue = divider - 1u;

            /* Bit shift "RCC_PLL1DIVR_PLL1P_Pos" is used for all PLL's.
                                 * At least this ST did not screw up. */
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].Out_R_ConfRegId,
                            rcc_Pll_Config[ pllId ].Out_R_ConfMask,
                            regValue << RCC_PLL1DIVR_PLL1R_Pos );

            /* Activate output R */
            Rcc_Set_RegVal( rcc_Pll_Config[ pllId ].Out_R_StateRegId,
                            rcc_Pll_Config[ pllId ].Out_R_StateMask,
                            1u << RCC_PLL1CFGR_PLL1REN_Pos );

            retState = RCC_REQUEST_OK;
        }
        else
        {
            /* Incorrect PLL output R divider value */
            retState = RCC_REQUEST_ERROR;
        }
    }
    else
    {
        /* Incorrect PLL Id */
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reading of output frequency from main PLL from output R
 *
 * \note This function reads real values from registers and calculate real
 *       frequency.
 *
 * \param pllClk [out]: Pointer to PLL clock output frequency in Hz
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_Clk_OutR( rcc_PllId_t pllId, rcc_FreqHz_t * const pllClk )
{
    rcc_RequestState_t retState   = RCC_REQUEST_ERROR;
    rcc_FreqHz_t       pllIntFreq = 0u;

    retState = Rcc_Pll_Get_InternalClk( pllId, &pllIntFreq );

    if( ( RCC_REQUEST_ERROR != retState ) &&
        ( RCC_NULL_PTR      != pllClk   )    )
    {
        uint32_t regVal = Rcc_Get_RegVal( rcc_Pll_Config[ pllId ].Out_R_ConfRegId,
                                          rcc_Pll_Config[ pllId ].Out_R_ConfMask );

        regVal = ( ( regVal & rcc_Pll_Config[ pllId ].Out_R_ConfMask ) >> RCC_PLL1DIVR_PLL1R_Pos ) + 1u;

        pllIntFreq = ( pllIntFreq / regVal );

        retState = RCC_REQUEST_OK;
    }
    else
    {
        retState = RCC_REQUEST_ERROR;
    }

    return ( retState );
}

/*----------------------- Low Speed Clock configuration ----------------------*/

/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * \param clkSource [in]: RTC clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Set_RtcClkSource( rcc_Rtc_ClkSource_t clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    LL_RCC_SetRTCClockSource( clkSource );

    for( uint32_t iterationCnt = 0u; RCC_PLL_TIMEOUT_RAW > iterationCnt; iterationCnt ++ )
    {
        uint32_t registerValue = LL_RCC_GetRTCClockSource();

        if( registerValue == clkSource )
        {
            retState = RCC_REQUEST_OK;
            break;
        }
        else
        {
            /* Clock source has not yet been changed, keep return state as error */
            retState = RCC_REQUEST_ERROR;
        }
    }

    return ( retState );
}


/**
 * \brief Selection of clock source for Real Time Clock (RTC) multiplexer
 *
 * \param clkSource [in]: RTC clock source ID
 *
 * \return State of request execution. Returns "OK" if request was success,
 *         otherwise return error.
 */
rcc_RequestState_t Rcc_Pll_Get_RtcClkSource( rcc_Rtc_ClkSource_t * const clkSource )
{
    rcc_RequestState_t retState = RCC_REQUEST_ERROR;

    if( RCC_NULL_PTR != clkSource )
    {
        *clkSource = LL_RCC_GetRTCClockSource();

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
