/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_datapath.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_datapath.h"


/* Private Macros ----------------------------------------------------------- */

// Specifies the maximum number of connections in a table.  Used for creating
// a for loop instead of an endless while loop (it's safer).
#define MAX_NUMBER_OF_CONNECTIONS  32

// This definition is used as an out of bound value to specify the end of the
// connections table.
#define NULL_CONNECTION            0xFFFFFFFF

// Initialization of union struct members requires different syntax between
// Linux GCC and Win VC++ compilers.  These macros take care of the syntax
// differences required for initialization of DFR_Connection_t.
#define FE_BLOCK_A(block_a)    {.FE_Block_A = block_a}
#define BE_BLOCK_A(block_a)    {.BE_Block_A = block_a}
#define LITE_BLOCK_A(block_a)  {.LITE_Block_A = block_a}
#define FE_BLOCK_B(block_b)    {.FE_Block_B = block_b}
#define BE_BLOCK_B(block_b)    {.BE_Block_B = block_b}
#define LITE_BLOCK_B(block_b)  {.LITE_Block_B = block_b}
#define END_OF_CONNECTIONS     {.Block_A = NULL_CONNECTION}, NULL_CONNECTION, {.Block_B = NULL_CONNECTION}, NULL_CONNECTION


/* Private Variables (static)------------------------------------------------ */

static const DFR_Connection_t DataPath_FE_Interlaced[] =
{
//  --------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                      ||
//  ||                    |           |             |           |             |           |                     ||
//  ||                    |           |             |   D F R   |             |           |                     ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                     ||
//  ||                    |           |             |  C O R E  |             |           |                     ||
//  ||                    |___________|             |___________|             |___________|                     ||
//  ||                                                    |                                                     ||
//  ||----------------------------------------------------|-----------------------------------------------------||
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_ELA_FIFO_INPUT)   , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_CCS_CURR_INPUT)   , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P2_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_CCS_P2_INPUT)     , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_CCS_RAW_RD)    , DFR_UV_ONLY  ,   FE_BLOCK_B(DFR_FE_CCS_P2_RAW_INPUT) , DFR_UV_ONLY },
    { FE_BLOCK_A(DFR_FE_CCS_CURR_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_CURR)      , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P1_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P1)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_CCS_P2_OUTPUT)     , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P2)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P3_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P3)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_CCS_RAW_OUTPUT)    , DFR_UV_ONLY  ,   FE_BLOCK_B(DFR_FE_MCC_CCS_RAW_WR)   , DFR_UV_ONLY },
    { FE_BLOCK_A(DFR_FE_MCMADI_TNR)        , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_C_WR)         , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCMADI_DEINT)      , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_YUV422  },

  { END_OF_CONNECTIONS                                                                                           }
//  --------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_FE_Interlaced_no_CCS [] =
{
//  --------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                      ||
//  ||                    |           |             |           |             |           |                     ||
//  ||                    |           |             |   D F R   |             |           |                     ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                     ||
//  ||                    |           |             |  C O R E  |             |           |                     ||
//  ||                    |___________|             |___________|             |___________|                     ||
//  ||                                                    |                                                     ||
//  ||----------------------------------------------------|-----------------------------------------------------||

    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_ELA_FIFO_INPUT),    DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_CURR)      , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P1_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P1)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P2_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P2)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P3_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P3)        , DFR_YUV422  },

    { FE_BLOCK_A(DFR_FE_MCMADI_TNR)        , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_C_WR)         , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCMADI_DEINT)      , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_YUV422  },
    { END_OF_CONNECTIONS                                                                                        }
//  --------------------------------------------------------------------------------------------------------------
};
static const DFR_Connection_t DataPath_FE_Interlaced_Spatial[] =
{
//  --------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                      ||
//  ||                    |           |             |           |             |           |                     ||
//  ||                    |           |             |   D F R   |             |           |                     ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                     ||
//  ||                    |           |             |  C O R E  |             |           |                     ||
//  ||                    |___________|             |___________|             |___________|                     ||
//  ||                                                    |                                                     ||
//  ||----------------------------------------------------|-----------------------------------------------------||

#if 0
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_ELA_FIFO_INPUT),    DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_C_WR)         , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P2_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_YUV422  },
    { END_OF_CONNECTIONS                                                                                        }
//  --------------------------------------------------------------------------------------------------------------

#else
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_ELA_FIFO_INPUT),    DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_C_WR)         , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P2_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_YUV422  },

    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_CURR)      , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P1_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P1)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P3_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P2)        , DFR_YUV422  },
//    { FE_BLOCK_A(DFR_FE_MCC_P3_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P3)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCMADI_DEINT)      , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VIDEO4_OUT), DFR_YUV422  },

//    { FE_BLOCK_A(DFR_FE_MCC_P2_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
//    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_CURR)      , DFR_YUV422  },
//    { FE_BLOCK_A(DFR_FE_MCMADI_TNR)        , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_C_WR)         , DFR_YUV422  },





//


    { END_OF_CONNECTIONS                                                                                        }

#endif
//  --------------------------------------------------------------------------------------------------------------


};

static const DFR_Connection_t DataPath_FE_ProgressiveSpatial[] =
{
//  --------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                      ||
//  ||                    |           |             |           |             |           |                     ||
//  ||                    |           |             |   D F R   |             |           |                     ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                     ||
//  ||                    |           |             |  C O R E  |             |           |                     ||
//  ||                    |___________|             |___________|             |___________|                     ||
//  ||                                                    |                                                     ||
//  ||----------------------------------------------------|-----------------------------------------------------||
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_ELA_FIFO_INPUT)   , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_YUV422  },
    { END_OF_CONNECTIONS                                                                                        }
//  --------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_FE_ProgressiveSpatial_RGB[] =
{
//  --------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                      ||
//  ||                    |           |             |           |             |           |                     ||
//  ||                    |           |             |   D F R   |             |           |                     ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                     ||
//  ||                    |           |             |  C O R E  |             |           |                     ||
//  ||                    |___________|             |___________|             |___________|                     ||
//  ||                                                    |                                                     ||
//  ||----------------------------------------------------|-----------------------------------------------------||
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_RGB_YUV444   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_RGB_YUV444},
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_RGB_YUV444   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_RGB_YUV444},
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_RGB_YUV444   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                        }
    //  --------------------------------------------------------------------------------------------------------------
    };


static const DFR_Connection_t DataPath_FE_ProgressiveTemporal [] =
{
//  --------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                      ||
//  ||                    |           |             |           |             |           |                     ||
//  ||                    |           |             |   D F R   |             |           |                     ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                     ||
//  ||                    |           |             |  C O R E  |             |           |                     ||
//  ||                    |___________|             |___________|             |___________|                     ||
//  ||                                                    |                                                     ||
//  ||----------------------------------------------------|-----------------------------------------------------||

    { FE_BLOCK_A(DFR_FE_VIDEO1_IN)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_ELA_FIFO_INPUT),    DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_ELA_FIFO_OUTPUT)   , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_CURR)      , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCC_P1_RD)         , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCMADI_P1)        , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCMADI_TNR)        , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_C_WR)         , DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_MCMADI_DEINT)      , DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_HSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_HSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VSCALE_LITE_INPUT), DFR_YUV422  },
    { FE_BLOCK_A(DFR_FE_VSCALE_LITE_OUTPUT), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR)      , DFR_YUV422  },
    { END_OF_CONNECTIONS                                                                                        }
//  --------------------------------------------------------------------------------------------------------------
};
static const DFR_Connection_t DataPath_BE_Normal[] =
{
//  ---------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                       ||
//  ||                    |           |             |           |             |           |                      ||
//  ||                    |           |             |   D F R   |             |           |                      ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                      ||
//  ||                    |           |             |  C O R E  |             |           |                      ||
//  ||                    |___________|             |___________|             |___________|                      ||
//  ||                                                    |                                                      ||
//  ||----------------------------------------------------|------------------------------------------------------||
    { BE_BLOCK_A(DFR_BE_MCC_RD)           , DFR_YUV422    ,   BE_BLOCK_B(DFR_BE_HQSCALER_INPUT)  , DFR_YUV422    },
    { BE_BLOCK_A(DFR_BE_HQSCALER_OUTPUT)  , DFR_YUV422    ,   BE_BLOCK_B(DFR_BE_ELAFIFO_INPUT)   , DFR_YUV422    },
    { BE_BLOCK_A(DFR_BE_ELAFIFO_OUTPUT)   , DFR_YUV422    ,   BE_BLOCK_B(DFR_BE_COLOR_CTRL_INPUT), DFR_YUV422    },
    { BE_BLOCK_A(DFR_BE_COLOR_CTRL_OUTPUT), DFR_RGB_YUV444,   BE_BLOCK_B(DFR_BE_VIDEO1_OUT)      , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                         }
//  ---------------------------------------------------------------------------------------------------------------
};


static const DFR_Connection_t DataPath_BE_Normal_RGB[] =
{
//  ---------------------------------------------------------------------------------------------------------------
//  ||                     ___________               ___________               ___________                       ||
//  ||                    |           |             |           |             |           |                      ||
//  ||                    |           |             |   D F R   |             |           |                      ||
//  ||                    |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                      ||
//  ||                    |           |             |  C O R E  |             |           |                      ||
//  ||                    |___________|             |___________|             |___________|                      ||
//  ||                                                    |                                                      ||
//  ||----------------------------------------------------|------------------------------------------------------||
{ BE_BLOCK_A(DFR_BE_MCC_RD)           , DFR_RGB_YUV444    ,   BE_BLOCK_B(DFR_BE_HQSCALER_INPUT)  , DFR_RGB_YUV444},
{ BE_BLOCK_A(DFR_BE_HQSCALER_OUTPUT)  , DFR_RGB_YUV444    ,   BE_BLOCK_B(DFR_BE_COLOR_CTRL_INPUT), DFR_RGB_YUV444},
{ BE_BLOCK_A(DFR_BE_COLOR_CTRL_OUTPUT), DFR_RGB_YUV444    ,   BE_BLOCK_B(DFR_BE_VIDEO1_OUT)      , DFR_RGB_YUV444},
{ END_OF_CONNECTIONS                                                                                         }
//  ---------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_BE_Mem_CCtrl[] =
{
    { BE_BLOCK_A(DFR_BE_MCC_RD)           , DFR_YUV422    ,   BE_BLOCK_B(DFR_BE_COLOR_CTRL_INPUT), DFR_YUV422    },
    { BE_BLOCK_A(DFR_BE_COLOR_CTRL_OUTPUT), DFR_RGB_YUV444,   BE_BLOCK_B(DFR_BE_VIDEO1_OUT)      , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                         }
};

static const DFR_Connection_t DataPath_FE_BypassAlgoBypassMem[] =
{
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_VIDEO2_OUT) , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                       }
};

static const DFR_Connection_t DataPath_FE_BypassAlgo[] =
{
    { FE_BLOCK_A(DFR_FE_VIDEO1_IN), DFR_YUV422   ,   FE_BLOCK_B(DFR_FE_MCC_PROG_WR), DFR_YUV422},
    { END_OF_CONNECTIONS                                                                       }
};

static const DFR_Connection_t DataPath_BE_BypassAlgoBypassMem[] =
{
    { BE_BLOCK_A(DFR_BE_VIDEO2_IN), DFR_YUV422   ,   BE_BLOCK_B(DFR_BE_VIDEO1_OUT) , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                       }
};

static const DFR_Connection_t DataPath_BE_BypassAlgo[] =
{
    { BE_BLOCK_A(DFR_BE_MCC_RD)   , DFR_YUV422   ,   BE_BLOCK_B(DFR_BE_VIDEO1_OUT) , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                       }
};

static const DFR_Connection_t DataPath_LITE_HZoom_VZoom[] =
{
//  --------------------------------------------------------------------------------------------------------------------------
//  ||                            ___________               ___________               ___________                            ||
//  ||                           |           |             |           |             |           |                           ||
//  ||                           |           |             |   D F R   |             |           |                           ||
//  ||                           |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                           ||
//  ||                           |           |             |  C O R E  |             |           |                           ||
//  ||                           |___________|             |___________|             |___________|                           ||
//  ||                                                           |                                                           ||
//  ||-----------------------------------------------------------|-----------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_VIDEO1_IN)           , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_COLOR_CONTROL_INPUT), DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_COLOR_CONTROL_OUTPUT), DFR_RGB_YUV444, LITE_BLOCK_B(DFR_LITE_VIDEO1_OUT)         , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                                     }
//  --------------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_HShrink_VZoom[] =
{
//  --------------------------------------------------------------------------------------------------------------------------
//  ||                            ___________               ___________               ___________                            ||
//  ||                           |           |             |           |             |           |                           ||
//  ||                           |           |             |   D F R   |             |           |                           ||
//  ||                           |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                           ||
//  ||                           |           |             |  C O R E  |             |           |                           ||
//  ||                           |___________|             |___________|             |___________|                           ||
//  ||                                                           |                                                           ||
//  ||-----------------------------------------------------------|-----------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_VIDEO1_IN)           , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_COLOR_CONTROL_INPUT), DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_COLOR_CONTROL_OUTPUT), DFR_RGB_YUV444, LITE_BLOCK_B(DFR_LITE_VIDEO1_OUT)         , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                                     }
//  ---------------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_HZoom_VShrink[] =
{
//  --------------------------------------------------------------------------------------------------------------------------
//  ||                            ___________               ___________               ___________                            ||
//  ||                           |           |             |           |             |           |                           ||
//  ||                           |           |             |   D F R   |             |           |                           ||
//  ||                           |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                           ||
//  ||                           |           |             |  C O R E  |             |           |                           ||
//  ||                           |___________|             |___________|             |___________|                           ||
//  ||                                                           |                                                           ||
//  ||-----------------------------------------------------------|-----------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_VIDEO1_IN)           , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_COLOR_CONTROL_INPUT), DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_COLOR_CONTROL_OUTPUT), DFR_RGB_YUV444, LITE_BLOCK_B(DFR_LITE_VIDEO1_OUT)         , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                                     }
//  --------------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_HShrink_VShrink[] =
{
//  --------------------------------------------------------------------------------------------------------------------------
//  ||                            ___________               ___________               ___________                            ||
//  ||                           |           |             |           |             |           |                           ||
//  ||                           |           |             |   D F R   |             |           |                           ||
//  ||                           |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                           ||
//  ||                           |           |             |  C O R E  |             |           |                           ||
//  ||                           |___________|             |___________|             |___________|                           ||
//  ||                                                           |                                                           ||
//  ||-----------------------------------------------------------|-----------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_VIDEO1_IN)           , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_ZOOMFIFO_INPUT)     , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_ZOOMFIFO_OUTPUT)     , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_MCC_RD   )           , DFR_YUV422    , LITE_BLOCK_B(DFR_LITE_COLOR_CONTROL_INPUT), DFR_YUV422    },
    { LITE_BLOCK_A(DFR_LITE_COLOR_CONTROL_OUTPUT), DFR_RGB_YUV444, LITE_BLOCK_B(DFR_LITE_VIDEO1_OUT)         , DFR_RGB_YUV444},
    { END_OF_CONNECTIONS                                                                                                     }
//  --------------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_MemToMem_Spatial[] =
{
//  ----------------------------------------------------------------------------------------------------------------------
//  ||                          ___________               ___________               ___________                         ||
//  ||                         |           |             |           |             |           |                        ||
//  ||                         |           |             |   D F R   |             |           |                        ||
//  ||                         |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                        ||
//  ||                         |           |             |  C O R E  |             |           |                        ||
//  ||                         |___________|             |___________|             |___________|                        ||
//  ||                                                         |                                                        ||
//  ||---------------------------------------------------------|--------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_BORDER_CROP_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_BORDER_CROP_OUTPUT)  , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                                                }
//  ----------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_MemToMem_Temporal[] =
{
//  ----------------------------------------------------------------------------------------------------------------------
//  ||                          ___________               ___________               ___________                         ||
//  ||                         |           |             |           |             |           |                        ||
//  ||                         |           |             |   D F R   |             |           |                        ||
//  ||                         |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                        ||
//  ||                         |           |             |  C O R E  |             |           |                        ||
//  ||                         |___________|             |___________|             |___________|                        ||
//  ||                                                         |                                                        ||
//  ||---------------------------------------------------------|--------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_BORDER_CROP_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_BORDER_CROP_OUTPUT)  , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_TNR_CURR)           , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_MCC_TNR_RD)          , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_TNR_PREV)           , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_TNR_OUTPUT)          , DFR_YUV422  ,  LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                                                }
//  ----------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_MemToMem_Spatial_444[] =
{
//  ----------------------------------------------------------------------------------------------------------------------
//  ||                          ___________               ___________               ___________                         ||
//  ||                         |           |             |           |             |           |                        ||
//  ||                         |           |             |   D F R   |             |           |                        ||
//  ||                         |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                        ||
//  ||                         |           |             |  C O R E  |             |           |                        ||
//  ||                         |___________|             |___________|             |___________|                        ||
//  ||                                                         |                                                        ||
//  ||---------------------------------------------------------|--------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_RGB_YUV444,  LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_RGB_YUV444},
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_RGB_YUV444,  LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_RGB_YUV444},
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_RGB_YUV444,  LITE_BLOCK_B(DFR_LITE_COLOR_CONTROL_INPUT), DFR_RGB_YUV444},
    { LITE_BLOCK_A(DFR_LITE_COLOR_CONTROL_OUTPUT), DFR_YUV422    ,  LITE_BLOCK_B(DFR_LITE_BORDER_CROP_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_BORDER_CROP_OUTPUT)  , DFR_YUV422    ,  LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                                                }
//  ----------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t DataPath_LITE_MemToMem_Temporal_444[] =
{
//  ----------------------------------------------------------------------------------------------------------------------
//  ||                          ___________               ___________               ___________                         ||
//  ||                         |           |             |           |             |           |                        ||
//  ||                         |           |             |   D F R   |             |           |                        ||
//  ||                         |  BLOCK A  | ==========> |           | ==========> |  BLOCK B  |                        ||
//  ||                         |           |             |  C O R E  |             |           |                        ||
//  ||                         |___________|             |___________|             |___________|                        ||
//  ||                                                         |                                                        ||
//  ||---------------------------------------------------------|--------------------------------------------------------||
    { LITE_BLOCK_A(DFR_LITE_MCC_RD)              , DFR_RGB_YUV444,  LITE_BLOCK_B(DFR_LITE_HSCALE_LITE_INPUT)  , DFR_RGB_YUV444},
    { LITE_BLOCK_A(DFR_LITE_HSCALE_LITE_OUTPUT)  , DFR_RGB_YUV444,  LITE_BLOCK_B(DFR_LITE_VSCALE_LITE_INPUT)  , DFR_RGB_YUV444},
    { LITE_BLOCK_A(DFR_LITE_VSCALE_LITE_OUTPUT)  , DFR_RGB_YUV444,  LITE_BLOCK_B(DFR_LITE_COLOR_CONTROL_INPUT), DFR_RGB_YUV444},
    { LITE_BLOCK_A(DFR_LITE_COLOR_CONTROL_OUTPUT), DFR_YUV422    ,  LITE_BLOCK_B(DFR_LITE_BORDER_CROP_INPUT)  , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_BORDER_CROP_OUTPUT)  , DFR_YUV422    ,  LITE_BLOCK_B(DFR_LITE_TNR_CURR)           , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_MCC_TNR_RD)          , DFR_YUV422    ,  LITE_BLOCK_B(DFR_LITE_TNR_PREV)           , DFR_YUV422},
    { LITE_BLOCK_A(DFR_LITE_TNR_OUTPUT)          , DFR_YUV422    ,  LITE_BLOCK_B(DFR_LITE_MCC_WR)             , DFR_YUV422},
    { END_OF_CONNECTIONS                                                                                                }
//  ----------------------------------------------------------------------------------------------------------------------
};

static const DFR_Connection_t* DataPath_FE_Table[DATAPATH_FE_NUM] =
{
    DataPath_FE_BypassAlgoBypassMem,    // DATAPATH_FE_BYPASS_ALGO_BYPASS_MEM
    DataPath_FE_BypassAlgo,             // DATAPATH_FE_BYPASS_ALGO
    DataPath_FE_ProgressiveSpatial,     // DATAPATH_FE_PROGRESSIVE_SPATIAL
    DataPath_FE_Interlaced,             // DATAPATH_FE_INTERLACED
    DataPath_FE_Interlaced_no_CCS,      // DATAPATH_FE_INTERLACED_NO_CCS
    DataPath_FE_ProgressiveTemporal,    // DATAPATH_FE_PROGRESSIVE_TEMPORAL
    DataPath_FE_Interlaced_Spatial,     // DATAPATH_FE_INTERLACED_SPATIAL
    DataPath_FE_ProgressiveSpatial_RGB  // DATAPATH_FE_PROGRESSIVE_SPATIAL_RGB
};

static const DFR_Connection_t* DataPath_BE_Table[DATAPATH_BE_NUM] =
{
    DataPath_BE_BypassAlgoBypassMem,    // DATAPATH_BE_BYPASS_ALGO_BYPASS_MEM
    DataPath_BE_BypassAlgo,             // DATAPATH_BE_BYPASS_ALGO
    DataPath_BE_Mem_CCtrl,              // DATAPATH_BE_MEM_CCTRL
    DataPath_BE_Normal,                 // DATAPATH_BE_NORMAL
    DataPath_BE_Normal_RGB              // DATAPATH_BE_NORMAL_RGB
};

static const DFR_Connection_t* DataPath_LITE_Table[DATAPATH_LITE_NUM] =
{
    DataPath_LITE_HZoom_VZoom,          // DATAPATH_LITE_HZOOM_VZOOM
    DataPath_LITE_HShrink_VZoom,        // DATAPATH_LITE_HSHRINK_VZOOM
    DataPath_LITE_HZoom_VShrink,        // DATAPATH_LITE_HZOOM_VSHRINK
    DataPath_LITE_HShrink_VShrink,      // DATAPATH_LITE_HSHRINK_VSHRINK
    DataPath_LITE_MemToMem_Spatial,     // DATAPATH_LITE_MEMTOMEM_SPATIAL
    DataPath_LITE_MemToMem_Temporal,    // DATAPATH_LITE_MEMTOMEM_TEMPORAL
    DataPath_LITE_MemToMem_Spatial_444, // DATAPATH_LITE_MEMTOMEM_SPATIAL_444
    DataPath_LITE_MemToMem_Temporal_444 // DATAPATH_LITE_MEMTOMEM_TEMPORAL_444
};


/* Static Function Declarations --------------------------------------------- */

static FVDP_Result_t DATAPATH_SetDfrConnections(DFR_Core_t DfrCore, const DFR_Connection_t* Connections_p);


/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : DATAPATH_FE_Connect
Description : Connects the Datapath for FVDP_FE
Parameters  : [in] Datapath - specifies which datapath should be connected

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
FVDP_Result_t DATAPATH_FE_Connect(DATAPATH_FE_t Datapath)
{
    FVDP_Result_t result;

    // Error check that Datapath parameter is in bounds
    if (Datapath >= DATAPATH_FE_NUM)
        return FVDP_ERROR;

    // Reset the DFR FE block (disables all muxes)
    result = DFR_Reset(DFR_FE);
    if (result != FVDP_OK)
        return result;

    // Set the datapath connections for the selected datapath table
    result = DATAPATH_SetDfrConnections(DFR_FE, DataPath_FE_Table[Datapath]);
    if (result != FVDP_OK)
        return result;

#if (DATAPATH_DEBUG)
    DFR_PrintConnections(DFR_FE);
#endif

    return FVDP_OK;
}

/*******************************************************************************
Name        : DATAPATH_BE_Connect
Description : Connects the Datapath for FVDP_BE
Parameters  : [in] Datapath - specifies which datapath should be connected

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
FVDP_Result_t DATAPATH_BE_Connect(DATAPATH_BE_t Datapath)
{
    FVDP_Result_t result;

    // Error check that Datapath parameter is in bounds
    if (Datapath >= DATAPATH_BE_NUM)
        return FVDP_ERROR;

    // Reset the DFR BE block (disables all muxes)
    result = DFR_Reset(DFR_BE);
    if (result != FVDP_OK)
        return result;

    // Set the datapath connections for the selected datapath table
    result = DATAPATH_SetDfrConnections(DFR_BE, DataPath_BE_Table[Datapath]);
    if (result != FVDP_OK)
        return result;

#if (DATAPATH_DEBUG)
    DFR_PrintConnections(DFR_BE);
#endif

    return FVDP_OK;
}

/*******************************************************************************
Name        : DATAPATH_LITE_Connect
Description : Connects the Datapath for FVDP_PIP/AUX/ENC
Parameters  : [in] Datapath - specifies which datapath should be connected

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
FVDP_Result_t DATAPATH_LITE_Connect(FVDP_CH_t CH, DATAPATH_LITE_t Datapath)
{
    FVDP_Result_t result;
    DFR_Core_t DfrCore;

    // Error check that Datapath parameter is in bounds
    if (Datapath >= DATAPATH_LITE_NUM)
        return FVDP_ERROR;

     // Error check that Datapath parameter is in bounds
     switch (CH)
     {
         case FVDP_PIP:
             DfrCore = DFR_PIP;
             break;

         case FVDP_AUX:
             DfrCore = DFR_AUX;
             break;

         case FVDP_ENC:
             DfrCore = DFR_ENC;
             break;

         case FVDP_MAIN:
         default:
             return FVDP_ERROR;
     }

    // Reset the DFR LITE block (disables all muxes)
    result = DFR_Reset(DfrCore);
    if (result != FVDP_OK)
        return result;

    // Set the datapath connections for the selected datapath table
    result = DATAPATH_SetDfrConnections(DfrCore, DataPath_LITE_Table[Datapath]);
    if (result != FVDP_OK)
        return result;

#if (DATAPATH_DEBUG)
    DFR_PrintConnections(DfrCore);
#endif

    return FVDP_OK;
}


/*******************************************************************************
Name        : DATAPATH_SetDfrConnections
Description : Sets the DFR connections for the datapath
Parameters  : [in] DfrCore       - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] Connections_p - Pointer to the datapath connections table

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t DATAPATH_SetDfrConnections(DFR_Core_t DfrCore, const DFR_Connection_t* Connections_p)
{
    FVDP_Result_t result;
    uint8_t       i;

    // Loop through the table and call DFR_Connect for each connection
    for (i = 0; i < MAX_NUMBER_OF_CONNECTIONS; i++)
    {
        // Check if we are at the end of the connections table
        if (Connections_p[i].Block_A == NULL_CONNECTION)
            break;

        // Set the DFR connection
        result = DFR_Connect(DfrCore, Connections_p[i]);
        if (result != FVDP_OK)
        {
            TRC( TRC_ID_MAIN_INFO, "ERROR:  DFR_Connect(%d)", i );
            return result;
        }
    }

    return FVDP_OK;
}

/* end of file */
