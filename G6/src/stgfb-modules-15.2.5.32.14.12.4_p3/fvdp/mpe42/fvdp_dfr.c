/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_dfr.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_regs.h"
#include "fvdp_common.h"
#include "fvdp_hostupdate.h"
#include "fvdp_dfr.h"


/* Private Macros ----------------------------------------------------------- */

// Indicates the MUX Channel is invalid.
// For example, MCMADI_CURR only has Y_G and U_UV_B channels, so V_R is not a valid channel.
#define DFR_INVALID_CH      0xFFFFFFFF

// Indicates that the MUX source port is invalid.
// For example, CCS_RAW_OUTPUT does not have a Y_G and V_R channels -- it only has a U_UV_B channel.
#define DFR_INVALID_PORT    0xFF

// Indicates the Block cannot be set to RGB_YUV444 connection type, since it does not support 3 channel.
#define DFR_NO_RGB_YUV444   0xFFFFFFFF

// Indicates the block cannot be set to Luma only since it is not supported
#define DFR_NO_LUMA_ONLY    0xFFFFFFFF

// Indicates the entry is not applicable.
// This is used for Color Control entries that are not invalid, just don't exist (eg: MCC blocks)
#define DFR_NOT_APPLICABLE  0xEEEEEEEE


/* Register Access Macros---------------------------------------------------- */

static const uint32_t DFR_BASE_ADDR[DFR_NUM_CORES] = {DFR_FE_BASE_ADDRESS,
                                                      DFR_BE_BASE_ADDRESS,
                                                      DFR_PIP_BASE_ADDRESS,
                                                      DFR_AUX_BASE_ADDRESS,
                                                      DFR_ENC_BASE_ADDRESS};

#define DFR_REG32_Write(DFR,Addr,Val)           REG32_Write(Addr+DFR_BASE_ADDR[DFR],Val)
#define DFR_REG32_Read(DFR,Addr)                REG32_Read(Addr+DFR_BASE_ADDR[DFR])
#define DFR_REG32_Set(DFR,Addr,BitsToSet)       REG32_Set(Addr+DFR_BASE_ADDR[DFR],BitsToSet)
#define DFR_REG32_Clear(DFR,Addr,BitsToClear)   REG32_Clear(Addr+DFR_BASE_ADDR[DFR],BitsToClear)
#define DFR_REG32_ClearAndSet(DFR,Addr,BitsToClear,Offset,ValueToSet) \
                                                REG32_ClearAndSet(Addr+DFR_BASE_ADDR[DFR],BitsToClear,Offset,ValueToSet)

/* Private Enum Definitions ------------------------------------------------- */

typedef enum
{
    DFR_Channel_Y_G = BIT0,
    DFR_Channel_U_UV_B = BIT1,
    DFR_Channel_V_R = BIT2
} DFR_Channel_t;


/* Private Struct Definitions ----------------------------------------------- */

typedef struct
{
    uint8_t             Y_G_ChannelNum;
    uint8_t             U_UV_B_ChannelNum;
    uint8_t             V_R_ChannelNum;
} DFR_MuxSource_t;

typedef struct
{
    uint32_t            EnRegAddr;
    uint32_t            EnBitMask;
    uint32_t            SelRegAddr;
    uint32_t            SelRegMask;
    uint32_t            SelRegOffset;
} DFR_MuxChannel_t;

typedef struct
{
    DFR_MuxChannel_t    Y_G_Channel;
    DFR_MuxChannel_t    U_UV_B_Channel;
    DFR_MuxChannel_t    V_R_Channel;
} DFR_Mux_t;

typedef struct
{
    uint32_t            ColorSpaceBitMask;
    uint32_t            LumaOnlyBitMask;
} DFR_ColorCtrl;

typedef struct
{
    uint32_t               NumMuxSources;
    uint32_t               NumOfMuxes;
    const DFR_MuxSource_t* MuxSourceList;
    const DFR_Mux_t*       MuxList;
    const DFR_ColorCtrl*   MuxSourceColorCtrlList;
    const DFR_ColorCtrl*   MuxColorCtrlList;
    const char**           MuxSourceNameList;
    const char**           MuxNameList;
    uint32_t               DFR_COLOUR_CTRL_RegAddr;
    uint32_t               DFR_CHL_CTRL_RegAddr;
} DFR_CoreInfo_t;

/* Static Function Declarations --------------------------------------------- */

static FVDP_Result_t        DFR_MuxSelect(DFR_Core_t DfrCore,
                                          const DFR_MuxChannel_t* MuxChannel,
                                          uint8_t MuxInputSelNum);
static FVDP_Result_t        DFR_ColorControl(DFR_Core_t DfrCore,
                                             DFR_ConnectionType_t ConnectionType,
                                             const DFR_ColorCtrl* ColorCtrl);
static bool                 DFR_IsMuxChannelEnabled(DFR_Core_t DfrCore,
                                                    const DFR_MuxChannel_t*  MuxChannel);
static uint32_t             DFR_GetMuxSource(DFR_Core_t DfrCore,
                                             const DFR_MuxChannel_t*  MuxChannel,
                                             uint8_t Ch);
static DFR_ConnectionType_t DFR_GetBlockColorCtrl(DFR_Core_t DfrCore,
                                                  const DFR_CoreInfo_t* Dfr_p,
                                                  DFR_ColorCtrl ColorCtrl);
static const char* DFR_GetConnectionTypeName(uint32_t Channel_Enabled,
                                             DFR_ConnectionType_t ConnectionType);


/* Private Variables (static const)------------------------------------------ */

static const char* DFR_ConnectionTypeError = "ERROR     ";

static const char* DFR_ConnectionTypeNameList[DFR_NUM_CONNECTION_TYPES] =
{
    "RGB_YUV444",
    "  YUV422  ",
    "  Y_ONLY  ",
    "  UV_ONLY "
};


static const char* DFR_FE_MuxSourceNameList[DFR_FE_NUM_MUX_SOURCES] =
{
    "VIDEO1_IN     ",
    "VIDEO2_IN     ",
    "VIDEO3_IN     ",
    "VIDEO4_IN     ",
    "HSCALE_LITE   ",
    "VSCALE_LITE   ",
    "CCS_CURR      ",
    "CCS_P2        ",
    "CCS_RAW       ",
    "MCMADI_DEINT  ",
    "MCMADI_TNR    ",
    "ELA_FIFO      ",
    "CHLSPLIT_CH_A ",
    "CHLSPLIT_CH_B ",
    "MCC_P1_RD     ",
    "MCC_P2_RD     ",
    "MCC_P3_RD     ",
    "MCC_CCS_RAW_RD"
};

static const char* DFR_FE_MuxNameList[DFR_FE_NUM_OF_MUXES] =
{
    "VIDEO1_OUT    ",
    "VIDEO2_OUT    ",
    "VIDEO3_OUT    ",
    "VIDEO4_OUT    ",
    "HSCALE_LITE   ",
    "VSCALE_LITE   ",
    "CCS_CURR      ",
    "CCS_P2        ",
    "CCS_P2_RAW    ",
    "MCMADI_CURR   ",
    "MCMADI_P1     ",
    "MCMADI_P2     ",
    "MCMADI_P3     ",
    "ELA_FIFO      ",
    "CHLSPLIT      ",
    "MCC_PROG_WR   ",
    "MCC_C_WR      ",
    "MCC_CCS_RAW_WR"
};

static const char* DFR_BE_MuxSourceNameList[DFR_BE_NUM_MUX_SOURCES] =
{
    "VIDEO1_IN     ",
    "VIDEO2_IN     ",
    "VIDEO3_IN     ",
    "VIDEO4_IN     ",
    "HQSCALER      ",
    "COLOR_CTRL    ",
    "BORDER_CROP   ",
    "R2DTO3D       ",
    "ELAFIFO       ",
    "MCC_RD        "
};

static const char* DFR_BE_MuxNameList[DFR_BE_NUM_OF_MUXES] =
{
    "VIDEO1_OUT    ",
    "VIDEO2_OUT    ",
    "VIDEO3_OUT    ",
    "VIDEO4_OUT    ",
    "HQSCALER      ",
    "COLOR_CTRL    ",
    "BORDER_CROP   ",
    "R2DTO3D       ",
    "ELAFIFO       ",
    "MCC_WR        "
};

static const char* DFR_LITE_MuxSourceNameList[DFR_LITE_NUM_MUX_SOURCES] =
{
    "VIDEO1_IN     ",
    "VIDEO2_IN     ",
    "VIDEO3_IN     ",
    "VIDEO4_IN     ",
    "TNR           ",
    "HSCALE_LITE   ",
    "VSCALE_LITE   ",
    "COLOR_CONTROL ",
    "BORDER_CROP   ",
    "ZOOMFIFO      ",
    "MCC_RD        ",
    "MCC_TNR_RD    "
};

static const char* DFR_LITE_MuxNameList[DFR_LITE_NUM_OF_MUXES] =
{
    "VIDEO1_OUT    ",
    "VIDEO2_OUT    ",
    "VIDEO3_OUT    ",
    "VIDEO4_OUT    ",
    "TNR_CURR      ",
    "TNR_PREV      ",
    "HSCALE_LITE   ",
    "VSCALE_LITE   ",
    "COLOR_CONTROL ",
    "BORDER_CROP   ",
    "ZOOMFIFO      ",
    "MCC_WR        "
};

static const DFR_MuxSource_t DFR_FE_MuxSourceList[DFR_FE_NUM_MUX_SOURCES] =
{
//  ----------------------------------------------------------------
//  |                         F V D P _ F E                        |
//  |--------------------------------------------------------------|
//  |  Y_G Channel Num  |  U_UV_B Channel Num  |  V_R Channel Num  |
//  |-------------------|----------------------|-------------------|
    {        0          ,            1         ,         2         },  // VIDEO1_IN
    {        3          ,            4         ,         5         },  // VIDEO2_IN
    {        6          ,            7         ,         8         },  // VIDEO3_IN
    {        9          ,           10         ,        11         },  // VIDEO4_IN
    {       12          ,           13         ,        14         },  // HSCALE_LITE_OUTPUT
    {       15          ,           16         ,        17         },  // VSCALE_LITE_OUTPUT
    {       18          ,           19         ,  DFR_INVALID_PORT },  // CCS_CURR_OUTPUT
    {       20          ,           21         ,  DFR_INVALID_PORT },  // CCS_P2_OUTPUT
    { DFR_INVALID_PORT  ,           22         ,  DFR_INVALID_PORT },  // CCS_RAW_OUTPUT
    {       25          ,           26         ,  DFR_INVALID_PORT },  // MCMADI_DEINT
    {       27          ,           28         ,  DFR_INVALID_PORT },  // MCMADI_TNR
    {       29          ,           30         ,  DFR_INVALID_PORT },  // ELA_FIFO_OUTPUT
    {       31          ,           32         ,        33         },  // CHLSPLIT_CH_A_OUTPUT
    {       34          ,           35         ,        36         },  // CHLSPLIT_CH_B_OUTPUT
    {       37          ,           38         ,  DFR_INVALID_PORT },  // MCC_P1_RD
    {       39          ,           40         ,  DFR_INVALID_PORT },  // MCC_P2_RD
    {       41          ,           42         ,  DFR_INVALID_PORT },  // MCC_P3_RD
    { DFR_INVALID_PORT  ,           43         ,  DFR_INVALID_PORT }   // MCC_CCS_RAW_RD
//  ----------------------------------------------------------------
};

static const DFR_MuxSource_t DFR_BE_MuxSourceList[DFR_BE_NUM_MUX_SOURCES] =
{
//  ----------------------------------------------------------------
//  |                         F V D P _ B E                        |
//  |--------------------------------------------------------------|
//  |  Y_G Channel Num  |  U_UV_B Channel Num  |  V_R Channel Num  |
//  |-------------------|----------------------|-------------------|
    {        0          ,            1         ,         2         },  // VIDEO1_IN
    {        3          ,            4         ,         5         },  // VIDEO2_IN
    {        6          ,            7         ,         8         },  // VIDEO3_IN
    {        9          ,           10         ,        11         },  // VIDEO4_IN
    {       12          ,           13         ,        14         },  // HQSCALER_OUTPUT
    {       15          ,           16         ,        17         },  // COLOR_CTRL_OUTPUT
    {       18          ,           19         ,        20         },  // BORDER_CROP_OUTPUT
    {       21          ,           22         ,        23         },  // R2DTO3D_OUTPUT
    {       24          ,           25         ,  DFR_INVALID_PORT },  // ELAFIFO_OUTPUT
    {       29          ,           30         ,        31         }   // MCC_RD
//  ----------------------------------------------------------------
};

static const DFR_MuxSource_t DFR_LITE_MuxSourceList[DFR_LITE_NUM_MUX_SOURCES] =
{
//  ----------------------------------------------------------------
//  |                       F V D P _ L I T E                      |
//  |--------------------------------------------------------------|
//  |  Y_G Channel Num  |  U_UV_B Channel Num  |  V_R Channel Num  |
//  |-------------------|----------------------|-------------------|
    {        0          ,            1         ,         2         },  // VIDEO1_IN
    {        3          ,            4         ,         5         },  // VIDEO2_IN
    {        6          ,            7         ,         8         },  // VIDEO3_IN
    {        9          ,           10         ,        11         },  // VIDEO4_IN
    {       12          ,           13         ,  DFR_INVALID_PORT },  // TNR_OUTPUT
    {       14          ,           15         ,        16         },  // HSCALE_LITE_OUTPUT
    {       17          ,           18         ,        19         },  // VSCALE_LITE_OUTPUT
    {       20          ,           21         ,        22         },  // COLOR_CONTROL_OUTPUT
    {       23          ,           24         ,        25         },  // BORDER_CROP_OUTPUT
    {       26          ,           27         ,        28         },  // ZOOMFIFO_OUTPUT
    {       29          ,           30         ,        31         },  // MCC_RD
    {       32          ,           33         ,  DFR_INVALID_PORT }   // MCC_TNR_RD
//  ----------------------------------------------------------------
};


static const DFR_Mux_t DFR_FE_MuxList[DFR_FE_NUM_OF_MUXES] =
{
//  -----------------------------------------------------------------------------------------------------------------------------------------------
//  ||                                                                                                                                           ||
//  ||                                                         F  V  D  P __ F  E                                                                ||
//  ||                                                                                                                                           ||
//  ||-------------------------------------------------------------------------------------------------------------------------------------------||
//  ||                                              ||                                                                                           ||
//  ||           M U X     E N A B L E              ||                                  M U X     S E L E C T                                    ||
//  ||                                              ||                                                                                           ||
//  ||----------------------------------------------||-------------------------------------------------------------------------------------------||
//  ||  Register Addr |          Bit Mask           ||  Register Addr   |        Register Mask              |         Register Offset            ||
//  ||----------------|-----------------------------||------------------|-----------------------------------|------------------------------------||
    {
    { DFR_FE_MUX_EN_0 , VIDEO1_OUT_Y_G_EN_MASK       , DFR_FE_MUX_SEL_0 , VIDEO1_OUT_Y_G_MUX_SEL_MASK       , VIDEO1_OUT_Y_G_MUX_SEL_OFFSET       }, // VIDEO1_OUT
    { DFR_FE_MUX_EN_0 , VIDEO1_OUT_U_UV_B_EN_MASK    , DFR_FE_MUX_SEL_0 , VIDEO1_OUT_U_UV_B_MUX_SEL_MASK    , VIDEO1_OUT_U_UV_B_MUX_SEL_OFFSET    },
    { DFR_FE_MUX_EN_0 , VIDEO1_OUT_V_R_EN_MASK       , DFR_FE_MUX_SEL_0 , VIDEO1_OUT_V_R_MUX_SEL_MASK       , VIDEO1_OUT_V_R_MUX_SEL_OFFSET       }
    },
    {
    { DFR_FE_MUX_EN_0 , VIDEO2_OUT_Y_G_EN_MASK       , DFR_FE_MUX_SEL_0 , VIDEO2_OUT_Y_G_MUX_SEL_MASK       , VIDEO2_OUT_Y_G_MUX_SEL_OFFSET       }, // VIDEO2_OUT
    { DFR_FE_MUX_EN_0 , VIDEO2_OUT_U_UV_B_EN_MASK    , DFR_FE_MUX_SEL_1 , VIDEO2_OUT_U_UV_B_MUX_SEL_MASK    , VIDEO2_OUT_U_UV_B_MUX_SEL_OFFSET    },
    { DFR_FE_MUX_EN_0 , VIDEO2_OUT_V_R_EN_MASK       , DFR_FE_MUX_SEL_1 , VIDEO2_OUT_V_R_MUX_SEL_MASK       , VIDEO2_OUT_V_R_MUX_SEL_OFFSET       }
    },
    {
    { DFR_FE_MUX_EN_0 , VIDEO3_OUT_Y_G_EN_MASK       , DFR_FE_MUX_SEL_1 , VIDEO3_OUT_Y_G_MUX_SEL_MASK       , VIDEO3_OUT_Y_G_MUX_SEL_OFFSET       }, // VIDEO3_OUT
    { DFR_FE_MUX_EN_0 , VIDEO3_OUT_U_UV_B_EN_MASK    , DFR_FE_MUX_SEL_1 , VIDEO3_OUT_U_UV_B_MUX_SEL_MASK    , VIDEO3_OUT_U_UV_B_MUX_SEL_OFFSET    },
    { DFR_FE_MUX_EN_0 , VIDEO3_OUT_V_R_EN_MASK       , DFR_FE_MUX_SEL_2 , VIDEO3_OUT_V_R_MUX_SEL_MASK       , VIDEO3_OUT_V_R_MUX_SEL_OFFSET       }
    },
    {
    { DFR_FE_MUX_EN_0 , VIDEO4_OUT_Y_G_EN_MASK       , DFR_FE_MUX_SEL_2 , VIDEO4_OUT_Y_G_MUX_SEL_MASK       , VIDEO4_OUT_Y_G_MUX_SEL_OFFSET       }, // VIDEO4_OUT
    { DFR_FE_MUX_EN_0 , VIDEO4_OUT_U_UV_B_EN_MASK    , DFR_FE_MUX_SEL_2 , VIDEO4_OUT_U_UV_B_MUX_SEL_MASK    , VIDEO4_OUT_U_UV_B_MUX_SEL_OFFSET    },
    { DFR_FE_MUX_EN_0 , VIDEO4_OUT_V_R_EN_MASK       , DFR_FE_MUX_SEL_2 , VIDEO4_OUT_V_R_MUX_SEL_MASK       , VIDEO4_OUT_V_R_MUX_SEL_OFFSET       }
    },
    {
    { DFR_FE_MUX_EN_0 , HFLITE_Y_G_EN_MASK           , DFR_FE_MUX_SEL_3 , HFLITE_Y_G_MUX_SEL_MASK           , HFLITE_Y_G_MUX_SEL_OFFSET           }, // HSCALE_LITE_INPUT
    { DFR_FE_MUX_EN_0 , HFLITE_U_UV_B_EN_MASK        , DFR_FE_MUX_SEL_3 , HFLITE_U_UV_B_MUX_SEL_MASK        , HFLITE_U_UV_B_MUX_SEL_OFFSET        },
    { DFR_FE_MUX_EN_0 , HFLITE_V_R_EN_MASK           , DFR_FE_MUX_SEL_3 , HFLITE_V_R_MUX_SEL_MASK           , HFLITE_V_R_MUX_SEL_OFFSET           }
    },
    {
    { DFR_FE_MUX_EN_0 , VFLITE_Y_G_EN_MASK           , DFR_FE_MUX_SEL_3 , VFLITE_Y_G_MUX_SEL_MASK           , VFLITE_Y_G_MUX_SEL_OFFSET           }, // VSCALE_LITE_INPUT
    { DFR_FE_MUX_EN_0 , VFLITE_U_UV_B_EN_MASK        , DFR_FE_MUX_SEL_4 , VFLITE_U_UV_B_MUX_SEL_MASK        , VFLITE_U_UV_B_MUX_SEL_OFFSET        },
    { DFR_FE_MUX_EN_0 , VFLITE_V_R_EN_MASK           , DFR_FE_MUX_SEL_4 , VFLITE_V_R_MUX_SEL_MASK           , VFLITE_V_R_MUX_SEL_OFFSET           }
    },
    {
    { DFR_FE_MUX_EN_0 , CCS_CURR_Y_G_EN_MASK         , DFR_FE_MUX_SEL_4 , CCS_CURR_Y_G_MUX_SEL_MASK         , CCS_CURR_Y_G_MUX_SEL_OFFSET         }, // CCS_CURR_INPUT
    { DFR_FE_MUX_EN_0 , CCS_CURR_U_UV_B_EN_MASK      , DFR_FE_MUX_SEL_4 , CCS_CURR_U_UV_B_MUX_SEL_MASK      , CCS_CURR_U_UV_B_MUX_SEL_OFFSET      },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_0 , CCS_P2_Y_G_EN_MASK           , DFR_FE_MUX_SEL_5 , CCS_P2_Y_G_MUX_SEL_MASK           , CCS_P2_Y_G_MUX_SEL_OFFSET           }, // CCS_P2_INPUT
    { DFR_FE_MUX_EN_0 , CCS_P2_U_UV_B_EN_MASK        , DFR_FE_MUX_SEL_5 , CCS_P2_U_UV_B_MUX_SEL_MASK        , CCS_P2_U_UV_B_MUX_SEL_OFFSET        },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }, // CCS_P2_RAW_INPUT
    { DFR_FE_MUX_EN_0 , CCS_P2_RAW_U_UV_B_EN_MASK    , DFR_FE_MUX_SEL_5 , CCS_P2_RAW_U_UV_B_MUX_SEL_MASK    , CCS_P2_RAW_U_UV_B_MUX_SEL_OFFSET    },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_0 , MCMADI_CURR_Y_G_EN_MASK      , DFR_FE_MUX_SEL_6 , MCMADI_CURR_Y_G_MUX_SEL_MASK      , MCMADI_CURR_Y_G_MUX_SEL_OFFSET      }, // MCMADI_CURR
    { DFR_FE_MUX_EN_0 , MCMADI_CURR_U_UV_B_EN_MASK   , DFR_FE_MUX_SEL_6 , MCMADI_CURR_U_UV_B_MUX_SEL_MASK   , MCMADI_CURR_U_UV_B_MUX_SEL_OFFSET   },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_0 , MCMADI_P1_Y_G_EN_MASK        , DFR_FE_MUX_SEL_6 , MCMADI_P1_Y_G_MUX_SEL_MASK        , MCMADI_P1_Y_G_MUX_SEL_OFFSET        }, // MCMADI_P1
    { DFR_FE_MUX_EN_0 , MCMADI_P1_U_UV_B_EN_MASK     , DFR_FE_MUX_SEL_7 , MCMADI_P1_U_UV_B_MUX_SEL_MASK     , MCMADI_P1_U_UV_B_MUX_SEL_OFFSET     },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_0 , MCMADI_P2_Y_G_EN_MASK        , DFR_FE_MUX_SEL_7 , MCMADI_P2_Y_G_MUX_SEL_MASK        , MCMADI_P2_Y_G_MUX_SEL_OFFSET        }, // MCMADI_P2
    { DFR_FE_MUX_EN_0 , MCMADI_P2_U_UV_B_EN_MASK     , DFR_FE_MUX_SEL_7 , MCMADI_P2_U_UV_B_MUX_SEL_MASK     , MCMADI_P2_U_UV_B_MUX_SEL_OFFSET     },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_0 , MCMADI_P3_Y_G_EN_MASK        , DFR_FE_MUX_SEL_7 , MCMADI_P3_Y_G_MUX_SEL_MASK        , MCMADI_P3_Y_G_MUX_SEL_OFFSET        }, // MCMADI_P3
    { DFR_FE_MUX_EN_1 , MCMADI_P3_U_UV_B_EN_MASK     , DFR_FE_MUX_SEL_8 , MCMADI_P3_U_UV_B_MUX_SEL_MASK     , MCMADI_P3_U_UV_B_MUX_SEL_OFFSET     },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_1 , ELAFIFO_Y_G_EN_MASK          , DFR_FE_MUX_SEL_8 , ELAFIFO_Y_G_MUX_SEL_MASK          , ELAFIFO_Y_G_MUX_SEL_OFFSET          }, // ELA_FIFO_INPUT
    { DFR_FE_MUX_EN_1 , ELAFIFO_U_UV_B_EN_MASK       , DFR_FE_MUX_SEL_8 , ELAFIFO_U_UV_B_MUX_SEL_MASK       , ELAFIFO_U_UV_B_MUX_SEL_OFFSET       },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_FE_MUX_EN_1 , CHLSPLIT_Y_G_EN_MASK         , DFR_FE_MUX_SEL_8 , CHLSPLIT_Y_G_MUX_SEL_MASK         , CHLSPLIT_Y_G_MUX_SEL_OFFSET         }, // CHLSPLIT_INPUT
    { DFR_FE_MUX_EN_1 , CHLSPLIT_U_UV_B_EN_MASK      , DFR_FE_MUX_SEL_9 , CHLSPLIT_U_UV_B_MUX_SEL_MASK      , CHLSPLIT_U_UV_B_MUX_SEL_OFFSET      },
    { DFR_FE_MUX_EN_1 , CHLSPLIT_V_R_EN_MASK         , DFR_FE_MUX_SEL_9 , CHLSPLIT_V_R_MUX_SEL_MASK         , CHLSPLIT_V_R_MUX_SEL_OFFSET         }
    },
    {
    { DFR_FE_MUX_EN_1 , MCC_PROG_WR_Y_G_EN_MASK      , DFR_FE_MUX_SEL_9 , MCC_PROG_WR_Y_G_MUX_SEL_MASK      , MCC_PROG_WR_Y_G_MUX_SEL_OFFSET      }, // MCC_PROG_WR
    { DFR_FE_MUX_EN_1 , MCC_PROG_WR_U_UV_B_EN_MASK   , DFR_FE_MUX_SEL_9 , MCC_PROG_WR_U_UV_B_MUX_SEL_MASK   , MCC_PROG_WR_U_UV_B_MUX_SEL_OFFSET   },
    { DFR_FE_MUX_EN_1 , MCC_PROG_WR_V_R_EN_MASK      , DFR_FE_MUX_SEL_10, MCC_PROG_WR_V_R_MUX_SEL_MASK      , MCC_PROG_WR_V_R_MUX_SEL_OFFSET      }
    },
    {
    { DFR_FE_MUX_EN_1 , MCC_C_WR_Y_G_EN_MASK         , DFR_FE_MUX_SEL_10, MCC_C_WR_Y_G_MUX_SEL_MASK         , MCC_C_WR_Y_G_MUX_SEL_OFFSET         }, // MCC_C_WR
    { DFR_FE_MUX_EN_1 , MCC_C_WR_U_UV_B_EN_MASK      , DFR_FE_MUX_SEL_10, MCC_C_WR_U_UV_B_MUX_SEL_MASK      , MCC_C_WR_U_UV_B_MUX_SEL_OFFSET      },
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    },
    {
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }, // MCC_CCS_RAW_WR
    { DFR_FE_MUX_EN_1 , MCC_CCS_RAW_WR_U_UV_B_EN_MASK, DFR_FE_MUX_SEL_10, MCC_CCS_RAW_WR_U_UV_B_MUX_SEL_MASK, MCC_CCS_RAW_WR_U_UV_B_MUX_SEL_OFFSET},
    { DFR_INVALID_CH  , DFR_INVALID_CH               , DFR_INVALID_CH   , DFR_INVALID_CH                    , DFR_INVALID_CH                      }
    }
//  -----------------------------------------------------------------------------------------------------------------------------------------------
};

static const DFR_Mux_t DFR_BE_MuxList[DFR_BE_NUM_OF_MUXES] =
{
//  -----------------------------------------------------------------------------------------------------------------------------------------------------------
//  ||                                                                                                                                                       ||
//  ||                                                             F  V  D  P __ B  E                                                                        ||
//  ||                                                                                                                                                       ||
//  ||-------------------------------------------------------------------------------------------------------------------------------------------------------||
//  ||                                                  ||                                                                                                   ||
//  ||           M U X     E N A B L E                  ||                                      M U X     S E L E C T                                        ||
//  ||                                                  ||                                                                                                   ||
//  ||--------------------------------------------------||---------------------------------------------------------------------------------------------------||
//  ||  Register Addr |          Bit Mask               ||  Register Addr   |            Register Mask              |           Register Offset              ||
//  ||----------------|---------------------------------||------------------|---------------------------------------|----------------------------------------||
    {
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO1_OUT_Y_G_EN_MASK    , DFR_BE_MUX_SEL_0 , DFR_BE_VIDEO1_OUT_Y_G_MUX_SEL_MASK    , DFR_BE_VIDEO1_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO1_OUT
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO1_OUT_U_UV_B_EN_MASK , DFR_BE_MUX_SEL_0 , DFR_BE_VIDEO1_OUT_U_UV_B_MUX_SEL_MASK , DFR_BE_VIDEO1_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO1_OUT_V_R_EN_MASK    , DFR_BE_MUX_SEL_0 , DFR_BE_VIDEO1_OUT_V_R_MUX_SEL_MASK    , DFR_BE_VIDEO1_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO2_OUT_Y_G_EN_MASK    , DFR_BE_MUX_SEL_0 , DFR_BE_VIDEO2_OUT_Y_G_MUX_SEL_MASK    , DFR_BE_VIDEO2_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO2_OUT
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO2_OUT_U_UV_B_EN_MASK , DFR_BE_MUX_SEL_1 , DFR_BE_VIDEO2_OUT_U_UV_B_MUX_SEL_MASK , DFR_BE_VIDEO2_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO2_OUT_V_R_EN_MASK    , DFR_BE_MUX_SEL_1 , DFR_BE_VIDEO2_OUT_V_R_MUX_SEL_MASK    , DFR_BE_VIDEO2_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO3_OUT_Y_G_EN_MASK    , DFR_BE_MUX_SEL_1 , DFR_BE_VIDEO3_OUT_Y_G_MUX_SEL_MASK    , DFR_BE_VIDEO3_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO3_OUT
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO3_OUT_U_UV_B_EN_MASK , DFR_BE_MUX_SEL_1 , DFR_BE_VIDEO3_OUT_U_UV_B_MUX_SEL_MASK , DFR_BE_VIDEO3_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO3_OUT_V_R_EN_MASK    , DFR_BE_MUX_SEL_2 , DFR_BE_VIDEO3_OUT_V_R_MUX_SEL_MASK    , DFR_BE_VIDEO3_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO4_OUT_Y_G_EN_MASK    , DFR_BE_MUX_SEL_2 , DFR_BE_VIDEO4_OUT_Y_G_MUX_SEL_MASK    , DFR_BE_VIDEO4_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO4_OUT
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO4_OUT_U_UV_B_EN_MASK , DFR_BE_MUX_SEL_2 , DFR_BE_VIDEO4_OUT_U_UV_B_MUX_SEL_MASK , DFR_BE_VIDEO4_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_BE_MUX_EN_0 , DFR_BE_VIDEO4_OUT_V_R_EN_MASK    , DFR_BE_MUX_SEL_2 , DFR_BE_VIDEO4_OUT_V_R_MUX_SEL_MASK    , DFR_BE_VIDEO4_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_BE_MUX_EN_0 , HQSCALER_Y_G_EN_MASK             , DFR_BE_MUX_SEL_3 , HQSCALER_Y_G_MUX_SEL_MASK             , HQSCALER_Y_G_MUX_SEL_OFFSET             }, // HQSCALER_INPUT
    { DFR_BE_MUX_EN_0 , HQSCALER_U_UV_B_EN_MASK          , DFR_BE_MUX_SEL_3 , HQSCALER_U_UV_B_MUX_SEL_MASK          , HQSCALER_U_UV_B_MUX_SEL_OFFSET          },
    { DFR_BE_MUX_EN_0 , HQSCALER_V_R_EN_MASK             , DFR_BE_MUX_SEL_3 , HQSCALER_V_R_MUX_SEL_MASK             , HQSCALER_V_R_MUX_SEL_OFFSET             }
    },
    {
    { DFR_BE_MUX_EN_0 , CCTRL_Y_G_EN_MASK                , DFR_BE_MUX_SEL_3 , CCTRL_Y_G_MUX_SEL_MASK                , CCTRL_Y_G_MUX_SEL_OFFSET                }, // COLOR_CTRL_INPUT
    { DFR_BE_MUX_EN_0 , CCTRL_U_UV_B_EN_MASK             , DFR_BE_MUX_SEL_4 , CCTRL_U_UV_B_MUX_SEL_MASK             , CCTRL_U_UV_B_MUX_SEL_OFFSET             },
    { DFR_BE_MUX_EN_0 , CCTRL_V_R_EN_MASK                , DFR_BE_MUX_SEL_4 , CCTRL_V_R_MUX_SEL_MASK                , CCTRL_V_R_MUX_SEL_OFFSET                }
    },
    {
    { DFR_BE_MUX_EN_0 , BDR_Y_G_EN_MASK                  , DFR_BE_MUX_SEL_4 , BDR_Y_G_MUX_SEL_MASK                  , BDR_Y_G_MUX_SEL_OFFSET                  }, // BORDER_CROP_INPUT
    { DFR_BE_MUX_EN_0 , BDR_U_UV_B_EN_MASK               , DFR_BE_MUX_SEL_4 , BDR_U_UV_B_MUX_SEL_MASK               , BDR_U_UV_B_MUX_SEL_OFFSET               },
    { DFR_BE_MUX_EN_0 , BDR_V_R_EN_MASK                  , DFR_BE_MUX_SEL_5 , BDR_V_R_MUX_SEL_MASK                  , BDR_V_R_MUX_SEL_OFFSET                  }
    },
    {
    { DFR_BE_MUX_EN_0 , R2DTO3D_Y_G_EN_MASK              , DFR_BE_MUX_SEL_5 , R2DTO3D_Y_G_MUX_SEL_MASK              , R2DTO3D_Y_G_MUX_SEL_OFFSET              }, // R2DTO3D_INPUT
    { DFR_BE_MUX_EN_0 , R2DTO3D_U_UV_B_EN_MASK           , DFR_BE_MUX_SEL_5 , R2DTO3D_U_UV_B_MUX_SEL_MASK           , R2DTO3D_U_UV_B_MUX_SEL_OFFSET           },
    { DFR_BE_MUX_EN_0 , R2DTO3D_V_R_EN_MASK              , DFR_BE_MUX_SEL_5 , R2DTO3D_V_R_MUX_SEL_MASK              , R2DTO3D_V_R_MUX_SEL_OFFSET              }
    },
    {
    { DFR_BE_MUX_EN_0 , DFR_BE_ELAFIFO_Y_G_EN_MASK       , DFR_BE_MUX_SEL_6 , DFR_BE_ELAFIFO_Y_G_MUX_SEL_MASK       , DFR_BE_ELAFIFO_Y_G_MUX_SEL_OFFSET       }, // ELAFIFO_INPUT
    { DFR_BE_MUX_EN_0 , DFR_BE_ELAFIFO_U_UV_B_EN_MASK    , DFR_BE_MUX_SEL_6 , DFR_BE_ELAFIFO_U_UV_B_MUX_SEL_MASK    , DFR_BE_ELAFIFO_U_UV_B_MUX_SEL_OFFSET    },
    { DFR_INVALID_CH  , DFR_INVALID_CH                   , DFR_INVALID_CH   , DFR_INVALID_CH                        , DFR_INVALID_CH                          }
    },
    {
    { DFR_BE_MUX_EN_0 , MCC_WR_Y_G_EN_MASK               , DFR_BE_MUX_SEL_7 , MCC_WR_Y_G_MUX_SEL_MASK               , MCC_WR_Y_G_MUX_SEL_OFFSET               }, // MCC_WR
    { DFR_BE_MUX_EN_0 , MCC_WR_U_UV_B_EN_MASK            , DFR_BE_MUX_SEL_7 , MCC_WR_U_UV_B_MUX_SEL_MASK            , MCC_WR_U_UV_B_MUX_SEL_OFFSET            },
    { DFR_INVALID_CH  , DFR_INVALID_CH                   , DFR_INVALID_CH   , DFR_INVALID_CH                        , DFR_INVALID_CH                          }
    }
//  -----------------------------------------------------------------------------------------------------------------------------------------------------------
};

static const DFR_Mux_t DFR_LITE_MuxList[DFR_LITE_NUM_OF_MUXES] =
{
//  -------------------------------------------------------------------------------------------------------------------------------------------------------------
//  ||                                                                                                                                                         ||
//  ||                                                          F  V  D  P __ L  I  T  E                                                                       ||
//  ||                                                                                                                                                         ||
//  ||---------------------------------------------------------------------------------------------------------------------------------------------------------||
//  ||                                                   ||                                                                                                    ||
//  ||              M U X     E N A B L E                ||                                      M U X     S E L E C T                                         ||
//  ||                                                   ||                                                                                                    ||
//  ||---------------------------------------------------||----------------------------------------------------------------------------------------------------||
//  ||  Register Addr |          Bit Mask                ||  Register Addr  |            Register Mask               |           Register Offset               ||
//  ||----------------|----------------------------------||-----------------|----------------------------------------|-----------------------------------------||
    {
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO1_OUT_Y_G_EN_MASK    , DFR_MUX_SEL_0   , DFR_PIP_VIDEO1_OUT_Y_G_MUX_SEL_MASK    , DFR_PIP_VIDEO1_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO1_OUT
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO1_OUT_U_UV_B_EN_MASK , DFR_MUX_SEL_0   , DFR_PIP_VIDEO1_OUT_U_UV_B_MUX_SEL_MASK , DFR_PIP_VIDEO1_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO1_OUT_V_R_EN_MASK    , DFR_MUX_SEL_0   , DFR_PIP_VIDEO1_OUT_V_R_MUX_SEL_MASK    , DFR_PIP_VIDEO1_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO2_OUT_Y_G_EN_MASK    , DFR_MUX_SEL_0   , DFR_PIP_VIDEO2_OUT_Y_G_MUX_SEL_MASK    , DFR_PIP_VIDEO2_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO2_OUT
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO2_OUT_U_UV_B_EN_MASK , DFR_MUX_SEL_1   , DFR_PIP_VIDEO2_OUT_U_UV_B_MUX_SEL_MASK , DFR_PIP_VIDEO2_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO2_OUT_V_R_EN_MASK    , DFR_MUX_SEL_1   , DFR_PIP_VIDEO2_OUT_V_R_MUX_SEL_MASK    , DFR_PIP_VIDEO2_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO3_OUT_Y_G_EN_MASK    , DFR_MUX_SEL_1   , DFR_PIP_VIDEO3_OUT_Y_G_MUX_SEL_MASK    , DFR_PIP_VIDEO3_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO3_OUT
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO3_OUT_U_UV_B_EN_MASK , DFR_MUX_SEL_1   , DFR_PIP_VIDEO3_OUT_U_UV_B_MUX_SEL_MASK , DFR_PIP_VIDEO3_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO3_OUT_V_R_EN_MASK    , DFR_MUX_SEL_2   , DFR_PIP_VIDEO3_OUT_V_R_MUX_SEL_MASK    , DFR_PIP_VIDEO3_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO4_OUT_Y_G_EN_MASK    , DFR_MUX_SEL_2   , DFR_PIP_VIDEO4_OUT_Y_G_MUX_SEL_MASK    , DFR_PIP_VIDEO4_OUT_Y_G_MUX_SEL_OFFSET    }, // VIDEO4_OUT
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO4_OUT_U_UV_B_EN_MASK , DFR_MUX_SEL_2   , DFR_PIP_VIDEO4_OUT_U_UV_B_MUX_SEL_MASK , DFR_PIP_VIDEO4_OUT_U_UV_B_MUX_SEL_OFFSET },
    { DFR_MUX_EN_0    , DFR_PIP_VIDEO4_OUT_V_R_EN_MASK    , DFR_MUX_SEL_2   , DFR_PIP_VIDEO4_OUT_V_R_MUX_SEL_MASK    , DFR_PIP_VIDEO4_OUT_V_R_MUX_SEL_OFFSET    }
    },
    {
    { DFR_MUX_EN_0    , TNR_CURR_Y_G_EN_MASK              , DFR_MUX_SEL_3   , TNR_CURR_Y_G_MUX_SEL_MASK              , TNR_CURR_Y_G_MUX_SEL_OFFSET              }, // TNR_CURR
    { DFR_MUX_EN_0    , TNR_CURR_U_UV_B_EN_MASK           , DFR_MUX_SEL_3   , TNR_CURR_U_UV_B_MUX_SEL_MASK           , TNR_CURR_U_UV_B_MUX_SEL_OFFSET           },
    { DFR_INVALID_CH  , DFR_INVALID_CH                    , DFR_INVALID_CH  , DFR_INVALID_CH                         , DFR_INVALID_CH                           }
    },
    {
    { DFR_MUX_EN_0    , TNR_PREV_Y_G_EN_MASK              , DFR_MUX_SEL_3   , TNR_PREV_Y_G_MUX_SEL_MASK              , TNR_PREV_Y_G_MUX_SEL_OFFSET              }, // TNR_PREV
    { DFR_MUX_EN_0    , TNR_PREV_U_UV_B_EN_MASK           , DFR_MUX_SEL_3   , TNR_PREV_U_UV_B_MUX_SEL_MASK           , TNR_PREV_U_UV_B_MUX_SEL_OFFSET           },
    { DFR_INVALID_CH  , DFR_INVALID_CH                    , DFR_INVALID_CH  , DFR_INVALID_CH                         , DFR_INVALID_CH                           }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_HFLITE_Y_G_EN_MASK        , DFR_MUX_SEL_4   , DFR_PIP_HFLITE_Y_G_MUX_SEL_MASK        , DFR_PIP_HFLITE_Y_G_MUX_SEL_OFFSET        }, // HSCALE_LITE_INTPUT
    { DFR_MUX_EN_0    , DFR_PIP_HFLITE_U_UV_B_EN_MASK     , DFR_MUX_SEL_4   , DFR_PIP_HFLITE_U_UV_B_MUX_SEL_MASK     , DFR_PIP_HFLITE_U_UV_B_MUX_SEL_OFFSET     },
    { DFR_MUX_EN_0    , DFR_PIP_HFLITE_V_R_EN_MASK        , DFR_MUX_SEL_4   , DFR_PIP_HFLITE_V_R_MUX_SEL_MASK        , DFR_PIP_HFLITE_V_R_MUX_SEL_OFFSET        }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_VFLITE_Y_G_EN_MASK        , DFR_MUX_SEL_4   , DFR_PIP_VFLITE_Y_G_MUX_SEL_MASK        , DFR_PIP_VFLITE_Y_G_MUX_SEL_OFFSET        }, // VSCALE_LITE_INPUT
    { DFR_MUX_EN_0    , DFR_PIP_VFLITE_U_UV_B_EN_MASK     , DFR_MUX_SEL_5   , DFR_PIP_VFLITE_U_UV_B_MUX_SEL_MASK     , DFR_PIP_VFLITE_U_UV_B_MUX_SEL_OFFSET     },
    { DFR_MUX_EN_0    , DFR_PIP_VFLITE_V_R_EN_MASK        , DFR_MUX_SEL_5   , DFR_PIP_VFLITE_V_R_MUX_SEL_MASK        , DFR_PIP_VFLITE_V_R_MUX_SEL_OFFSET        }
    },
    {
    { DFR_MUX_EN_0    , CCTRL_LITE_Y_G_EN_MASK            , DFR_MUX_SEL_5   , CCTRL_LITE_Y_G_MUX_SEL_MASK            , CCTRL_LITE_Y_G_MUX_SEL_OFFSET            }, // COLOR_CONTROL_INPUT
    { DFR_MUX_EN_0    , CCTRL_LITE_U_UV_B_EN_MASK         , DFR_MUX_SEL_5   , CCTRL_LITE_U_UV_B_MUX_SEL_MASK         , CCTRL_LITE_U_UV_B_MUX_SEL_OFFSET         },
    { DFR_MUX_EN_0    , CCTRL_LITE_V_R_EN_MASK            , DFR_MUX_SEL_6   , CCTRL_LITE_V_R_MUX_SEL_MASK            , CCTRL_LITE_V_R_MUX_SEL_OFFSET            }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_BDR_Y_G_EN_MASK           , DFR_MUX_SEL_6   , DFR_PIP_BDR_Y_G_MUX_SEL_MASK           , DFR_PIP_BDR_Y_G_MUX_SEL_OFFSET           }, // BORDER_CROP_INPUT
    { DFR_MUX_EN_0    , DFR_PIP_BDR_U_UV_B_EN_MASK        , DFR_MUX_SEL_6   , DFR_PIP_BDR_U_UV_B_MUX_SEL_MASK        , DFR_PIP_BDR_U_UV_B_MUX_SEL_OFFSET        },
    { DFR_MUX_EN_0    , DFR_PIP_BDR_V_R_EN_MASK           , DFR_MUX_SEL_6   , DFR_PIP_BDR_V_R_MUX_SEL_MASK           , DFR_PIP_BDR_V_R_MUX_SEL_OFFSET           }
    },
    {
    { DFR_MUX_EN_0    , ZOOMFIFO_Y_G_EN_MASK              , DFR_MUX_SEL_7   , ZOOMFIFO_Y_G_MUX_SEL_MASK              , ZOOMFIFO_Y_G_MUX_SEL_OFFSET              }, // ZOOMFIFO_INPUT
    { DFR_MUX_EN_0    , ZOOMFIFO_U_UV_B_EN_MASK           , DFR_MUX_SEL_7   , ZOOMFIFO_U_UV_B_MUX_SEL_MASK           , ZOOMFIFO_U_UV_B_MUX_SEL_OFFSET           },
    { DFR_MUX_EN_0    , ZOOMFIFO_V_R_EN_MASK              , DFR_MUX_SEL_7   , ZOOMFIFO_V_R_MUX_SEL_MASK              , ZOOMFIFO_V_R_MUX_SEL_OFFSET              }
    },
    {
    { DFR_MUX_EN_0    , DFR_PIP_MCC_WR_Y_G_EN_MASK        , DFR_MUX_SEL_7   , DFR_PIP_MCC_WR_Y_G_MUX_SEL_MASK        , DFR_PIP_MCC_WR_Y_G_MUX_SEL_OFFSET        }, // MCC_WR
    { DFR_MUX_EN_1    , DFR_PIP_MCC_WR_U_UV_B_EN_MASK     , DFR_MUX_SEL_8   , DFR_PIP_MCC_WR_U_UV_B_MUX_SEL_MASK     , DFR_PIP_MCC_WR_U_UV_B_MUX_SEL_OFFSET     },
    { DFR_MUX_EN_1    , MCC_WR_V_R_EN_MASK                , DFR_MUX_SEL_8   , MCC_WR_V_R_MUX_SEL_MASK                , MCC_WR_V_R_MUX_SEL_OFFSET                }
    }
//  -------------------------------------------------------------------------------------------------------------------------------------------------------------
};

static const DFR_ColorCtrl DFR_FE_MuxSourceColorCtrlList[DFR_FE_NUM_MUX_SOURCES] =
{
//  ------------------------------------------------------------------
//  |                          F V D P _ F E                         |
//  |----------------------------------------------------------------|
//  |   DFR_COLOUR_CTRL Bit Mask   |      DFR_CHL_CTRL Bit Mask      |
//  |------------------------------|---------------------------------|
    { VIDEO1_IN_YUV_RGBN_MASK      , VIDEO1_IN_LUMA_ONLY_MASK        },  // VIDEO1_IN
    { VIDEO2_IN_YUV_RGBN_MASK      , VIDEO2_IN_LUMA_ONLY_MASK        },  // VIDEO2_IN
    { VIDEO3_IN_YUV_RGBN_MASK      , VIDEO3_IN_LUMA_ONLY_MASK        },  // VIDEO3_IN
    { VIDEO4_IN_YUV_RGBN_MASK      , VIDEO4_IN_LUMA_ONLY_MASK        },  // VIDEO4_IN
    { HFLITE_OUT_YUV_RGBN_MASK     , HFLITE_OUT_LUMA_ONLY_MASK       },  // HSCALE_LITE_OUTPUT
    { VFLITE_OUT_YUV_RGBN_MASK     , VFLITE_OUT_LUMA_ONLY_MASK       },  // VSCALE_LITE_OUTPUT
    { DFR_NO_RGB_YUV444            , CCS_CURR_OUT_LUMA_ONLY_MASK     },  // CCS_CURR_OUTPUT
    { DFR_NO_RGB_YUV444            , CCS_P2_OUT_LUMA_ONLY_MASK       },  // CCS_P2_OUTPUT
    { DFR_NO_RGB_YUV444            , DFR_NO_LUMA_ONLY                },  // CCS_RAW_OUTPUT
    { DFR_NO_RGB_YUV444            , MCMADI_DEINT_OUT_LUMA_ONLY_MASK },  // MCMADI_DEINT
    { DFR_NO_RGB_YUV444            , MCMADI_TNR_OUT_LUMA_ONLY_MASK   },  // MCMADI_TNR
    { DFR_NO_RGB_YUV444            , ELAFIFO_OUT_LUMA_ONLY_MASK      },  // ELA_FIFO_OUTPUT
    { CHLSPLIT_A_OUT_YUV_RGBN_MASK , CHLSPLIT_A_OUT_LUMA_ONLY_MASK   },  // CHLSPLIT_CH_A_OUTPUT
    { CHLSPLIT_B_OUT_YUV_RGBN_MASK , CHLSPLIT_B_OUT_LUMA_ONLY_MASK   },  // CHLSPLIT_CH_B_OUTPUT
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              },  // MCC_P1_RD
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              },  // MCC_P2_RD
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              },  // MCC_P3_RD
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              }   // MCC_CCS_RAW_RD
//  ------------------------------------------------------------------
};

static const DFR_ColorCtrl DFR_FE_MuxColorCtrlList[DFR_FE_NUM_OF_MUXES] =
{
//  ------------------------------------------------------------------
//  |                          F V D P _ F E                         |
//  |----------------------------------------------------------------|
//  |   DFR_COLOUR_CTRL Bit Mask   |      DFR_CHL_CTRL Bit Mask      |
//  |------------------------------|---------------------------------|
    { VIDEO1_OUT_YUV_RGBN_MASK     , VIDEO1_OUT_LUMA_ONLY_MASK       },  // VIDEO1_OUT
    { VIDEO2_OUT_YUV_RGBN_MASK     , VIDEO2_OUT_LUMA_ONLY_MASK       },  // VIDEO2_OUT
    { VIDEO3_OUT_YUV_RGBN_MASK     , VIDEO3_OUT_LUMA_ONLY_MASK       },  // VIDEO3_OUT
    { VIDEO4_OUT_YUV_RGBN_MASK     , VIDEO4_OUT_LUMA_ONLY_MASK       },  // VIDEO4_OUT
    { HFLITE_IN_YUV_RGBN_MASK      , HFLITE_IN_LUMA_ONLY_MASK        },  // HSCALE_LITE_INPUT
    { VFLITE_IN_YUV_RGBN_MASK      , VFLITE_IN_LUMA_ONLY_MASK        },  // VSCALE_LITE_INPUT
    { DFR_NO_RGB_YUV444            , CCS_CURR_IN_LUMA_ONLY_MASK      },  // CCS_CURR_INPUT
    { DFR_NO_RGB_YUV444            , CCS_P2_IN_LUMA_ONLY_MASK        },  // CCS_P2_INPUT
    { DFR_NO_RGB_YUV444            , DFR_NO_LUMA_ONLY                },  // CCS_P2_RAW_INPUT
    { DFR_NO_RGB_YUV444            , MCMADI_CURR_IN_LUMA_ONLY_MASK   },  // MCMADI_CURR
    { DFR_NO_RGB_YUV444            , MCMADI_P1_IN_LUMA_ONLY_MASK     },  // MCMADI_P1
    { DFR_NO_RGB_YUV444            , MCMADI_P2_IN_LUMA_ONLY_MASK     },  // MCMADI_P2
    { DFR_NO_RGB_YUV444            , MCMADI_P3_IN_LUMA_ONLY_MASK     },  // MCMADI_P3
    { DFR_NO_RGB_YUV444            , ELAFIFO_IN_LUMA_ONLY_MASK       },  // ELA_FIFO_INPUT
    { CHLSPLIT_IN_YUV_RGBN_MASK    , CHLSPLIT_IN_LUMA_ONLY_MASK      },  // CHLSPLIT_INPUT
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              },  // MCC_PROG_WR
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              },  // MCC_C_WR
    { DFR_NOT_APPLICABLE           , DFR_NOT_APPLICABLE              }   // MCC_CCS_RAW_WR
//  ------------------------------------------------------------------
};

static const DFR_ColorCtrl DFR_BE_MuxSourceColorCtrlList[DFR_BE_NUM_MUX_SOURCES] =
{
//  -----------------------------------------------------------------------
//  |                            F V D P _ B E                            |
//  |---------------------------------------------------------------------|
//  |   DFR_COLOUR_CTRL Bit Mask      |       DFR_CHL_CTRL Bit Mask       |
//  |---------------------------------|-----------------------------------|
    { DFR_BE_VIDEO1_IN_YUV_RGBN_MASK  , DFR_BE_VIDEO1_IN_LUMA_ONLY_MASK   },  // VIDEO1_IN
    { DFR_BE_VIDEO2_IN_YUV_RGBN_MASK  , DFR_BE_VIDEO2_IN_LUMA_ONLY_MASK   },  // VIDEO2_IN
    { DFR_BE_VIDEO3_IN_YUV_RGBN_MASK  , DFR_BE_VIDEO3_IN_LUMA_ONLY_MASK   },  // VIDEO3_IN
    { DFR_BE_VIDEO4_IN_YUV_RGBN_MASK  , DFR_BE_VIDEO4_IN_LUMA_ONLY_MASK   },  // VIDEO4_IN
    { HQSCALER_OUT_YUV_RGBN_MASK      , HQSCALER_OUT_LUMA_ONLY_MASK       },  // HQSCALER_OUTPUT
    { CCTRL_OUT_YUV_RGBN_MASK         , CCTRL_OUT_LUMA_ONLY_MASK          },  // COLOR_CTRL_OUTPUT
    { BDR_OUT_YUV_RGBN_MASK           , BDR_OUT_LUMA_ONLY_MASK            },  // BORDER_CROP_OUTPUT
    { R2DTO3D_OUT_YUV_RGBN_MASK       , R2DTO3D_OUT_LUMA_ONLY_MASK        },  // R2DTO3D_OUTPUT
    { DFR_NO_RGB_YUV444               , DFR_BE_ELAFIFO_OUT_LUMA_ONLY_MASK },  // ELAFIFO_OUTPUT
    { DFR_NOT_APPLICABLE              , DFR_NOT_APPLICABLE                }   // MCC_RD
//  -----------------------------------------------------------------------
};

static const DFR_ColorCtrl DFR_BE_MuxColorCtrlList[DFR_BE_NUM_OF_MUXES] =
{
//  -----------------------------------------------------------------------
//  |                            F V D P _ B E                            |
//  |---------------------------------------------------------------------|
//  |   DFR_COLOUR_CTRL Bit Mask      |       DFR_CHL_CTRL Bit Mask       |
//  |---------------------------------|-----------------------------------|
    { DFR_BE_VIDEO1_OUT_YUV_RGBN_MASK , DFR_BE_VIDEO1_OUT_LUMA_ONLY_MASK  },  // VIDEO1_OUT
    { DFR_BE_VIDEO2_OUT_YUV_RGBN_MASK , DFR_BE_VIDEO2_OUT_LUMA_ONLY_MASK  },  // VIDEO2_OUT
    { DFR_BE_VIDEO3_OUT_YUV_RGBN_MASK , DFR_BE_VIDEO3_OUT_LUMA_ONLY_MASK  },  // VIDEO3_OUT
    { DFR_BE_VIDEO4_OUT_YUV_RGBN_MASK , DFR_BE_VIDEO4_OUT_LUMA_ONLY_MASK  },  // VIDEO4_OUT
    { HQSCALER_IN_YUV_RGBN_MASK       , HQSCALER_IN_LUMA_ONLY_MASK        },  // HQSCALER_INPUT
    { CCTRL_IN_YUV_RGBN_MASK          , CCTRL_IN_LUMA_ONLY_MASK           },  // COLOR_CTRL_INPUT
    { BDR_IN_YUV_RGBN_MASK            , BDR_IN_LUMA_ONLY_MASK             },  // BORDER_CROP_INPUT
    { R2DTO3D_IN_YUV_RGBN_MASK        , R2DTO3D_IN_LUMA_ONLY_MASK         },  // R2DTO3D_INPUT
    { DFR_NO_RGB_YUV444               , DFR_BE_ELAFIFO_IN_LUMA_ONLY_MASK  },  // ELAFIFO_INPUT
    { DFR_NOT_APPLICABLE              , DFR_NOT_APPLICABLE                }   // MCC_WR
//  -----------------------------------------------------------------------
};

static const DFR_ColorCtrl DFR_LITE_MuxSourceColorCtrlList[DFR_LITE_NUM_MUX_SOURCES] =
{
//  ------------------------------------------------------------------------
//  |                          F V D P _ L I T E                           |
//  |----------------------------------------------------------------------|
//  |    DFR_COLOUR_CTRL Bit Mask      |       DFR_CHL_CTRL Bit Mask       |
//  |----------------------------------|-----------------------------------|
    { DFR_PIP_VIDEO1_IN_YUV_RGBN_MASK  , DFR_PIP_VIDEO1_IN_LUMA_ONLY_MASK  },  // VIDEO1_IN
    { DFR_PIP_VIDEO2_IN_YUV_RGBN_MASK  , DFR_PIP_VIDEO2_IN_LUMA_ONLY_MASK  },  // VIDEO2_IN
    { DFR_PIP_VIDEO3_IN_YUV_RGBN_MASK  , DFR_PIP_VIDEO3_IN_LUMA_ONLY_MASK  },  // VIDEO3_IN
    { DFR_PIP_VIDEO4_IN_YUV_RGBN_MASK  , DFR_PIP_VIDEO4_IN_LUMA_ONLY_MASK  },  // VIDEO4_IN
    { DFR_NO_RGB_YUV444                , TNR_OUT_LUMA_ONLY_MASK            },  // TNR_OUTPUT
    { DFR_PIP_HFLITE_OUT_YUV_RGBN_MASK , DFR_PIP_HFLITE_OUT_LUMA_ONLY_MASK },  // HSCALE_LITE_OUTPUT
    { DFR_PIP_VFLITE_OUT_YUV_RGBN_MASK , DFR_PIP_VFLITE_OUT_LUMA_ONLY_MASK },  // VSCALE_LITE_OUTPUT
    { CCTRL_LITE_OUT_YUV_RGBN_MASK     , CCTRL_LITE_OUT_LUMA_ONLY_MASK     },  // COLOR_CONTROL_OUTPUT
    { DFR_PIP_BDR_OUT_YUV_RGBN_MASK    , DFR_PIP_BDR_OUT_LUMA_ONLY_MASK    },  // BORDER_CROP_OUTPUT
    { ZOOMFIFO_OUT_YUV_RGBN_MASK       , ZOOMFIFO_OUT_LUMA_ONLY_MASK       },  // ZOOMFIFO_OUTPUT
    { DFR_NOT_APPLICABLE               , DFR_NOT_APPLICABLE                },  // MCC_RD
    { DFR_NOT_APPLICABLE               , DFR_NOT_APPLICABLE                }   // MCC_TNR_RD
//  ------------------------------------------------------------------------
};

static const DFR_ColorCtrl DFR_LITE_MuxColorCtrlList[DFR_LITE_NUM_OF_MUXES] =
{
//  ------------------------------------------------------------------------
//  |                          F V D P _ L I T E                           |
//  |----------------------------------------------------------------------|
//  |    DFR_COLOUR_CTRL Bit Mask      |       DFR_CHL_CTRL Bit Mask       |
//  |----------------------------------|-----------------------------------|
    { DFR_PIP_VIDEO1_OUT_YUV_RGBN_MASK , DFR_PIP_VIDEO1_OUT_LUMA_ONLY_MASK },  // VIDEO1_OUT
    { DFR_PIP_VIDEO2_OUT_YUV_RGBN_MASK , DFR_PIP_VIDEO2_OUT_LUMA_ONLY_MASK },  // VIDEO2_OUT
    { DFR_PIP_VIDEO3_OUT_YUV_RGBN_MASK , DFR_PIP_VIDEO3_OUT_LUMA_ONLY_MASK },  // VIDEO3_OUT
    { DFR_PIP_VIDEO4_OUT_YUV_RGBN_MASK , DFR_PIP_VIDEO4_OUT_LUMA_ONLY_MASK },  // VIDEO4_OUT
    { DFR_NO_RGB_YUV444                , TNR_CURR_IN_LUMA_ONLY_MASK        },  // TNR_CURR
    { DFR_NO_RGB_YUV444                , TNR_PREV_IN_LUMA_ONLY_MASK        },  // TNR_PREV
    { DFR_PIP_HFLITE_IN_YUV_RGBN_MASK  , DFR_PIP_HFLITE_IN_LUMA_ONLY_MASK  },  // HSCALE_LITE_INTPUT
    { DFR_PIP_VFLITE_IN_YUV_RGBN_MASK  , DFR_PIP_VFLITE_IN_LUMA_ONLY_MASK  },  // VSCALE_LITE_INPUT
    { CCTRL_LITE_IN_YUV_RGBN_MASK      , CCTRL_LITE_IN_LUMA_ONLY_MASK      },  // COLOR_CONTROL_INPUT
    { DFR_PIP_BDR_IN_YUV_RGBN_MASK     , DFR_PIP_BDR_IN_LUMA_ONLY_MASK     },  // BORDER_CROP_INPUT
    { ZOOMFIFO_IN_YUV_RGBN_MASK        , ZOOMFIFO_IN_LUMA_ONLY_MASK        },  // ZOOMFIFO_INPUT
    { DFR_NOT_APPLICABLE               , DFR_NOT_APPLICABLE                }   // MCC_WR
//  ------------------------------------------------------------------------
};

static const DFR_CoreInfo_t DFR_CoreInfoList[DFR_NUM_CORES] =
{
    {   // DFR_FE
        DFR_FE_NUM_MUX_SOURCES,
        DFR_FE_NUM_OF_MUXES,
        DFR_FE_MuxSourceList,
        DFR_FE_MuxList,
        DFR_FE_MuxSourceColorCtrlList,
        DFR_FE_MuxColorCtrlList,
        DFR_FE_MuxSourceNameList,
        DFR_FE_MuxNameList,
        DFR_FE_COLOUR_CTRL,
        DFR_FE_CHL_CTRL
    },
    {   // DFR_BE
        DFR_BE_NUM_MUX_SOURCES,
        DFR_BE_NUM_OF_MUXES,
        DFR_BE_MuxSourceList,
        DFR_BE_MuxList,
        DFR_BE_MuxSourceColorCtrlList,
        DFR_BE_MuxColorCtrlList,
        DFR_BE_MuxSourceNameList,
        DFR_BE_MuxNameList,
        DFR_BE_COLOUR_CTRL,
        DFR_BE_CHL_CTRL
    },
    {   // DFR_PIP
        DFR_LITE_NUM_MUX_SOURCES,
        DFR_LITE_NUM_OF_MUXES,
        DFR_LITE_MuxSourceList,
        DFR_LITE_MuxList,
        DFR_LITE_MuxSourceColorCtrlList,
        DFR_LITE_MuxColorCtrlList,
        DFR_LITE_MuxSourceNameList,
        DFR_LITE_MuxNameList,
        DFR_COLOUR_CTRL,
        DFR_CHL_CTRL
    },
    {   // DFR_AUX
        DFR_LITE_NUM_MUX_SOURCES,
        DFR_LITE_NUM_OF_MUXES,
        DFR_LITE_MuxSourceList,
        DFR_LITE_MuxList,
        DFR_LITE_MuxSourceColorCtrlList,
        DFR_LITE_MuxColorCtrlList,
        DFR_LITE_MuxSourceNameList,
        DFR_LITE_MuxNameList,
        DFR_COLOUR_CTRL,
        DFR_CHL_CTRL
    },
    {   // DFR_ENC
        DFR_LITE_NUM_MUX_SOURCES,
        DFR_LITE_NUM_OF_MUXES,
        DFR_LITE_MuxSourceList,
        DFR_LITE_MuxList,
        DFR_LITE_MuxSourceColorCtrlList,
        DFR_LITE_MuxColorCtrlList,
        DFR_LITE_MuxSourceNameList,
        DFR_LITE_MuxNameList,
        DFR_COLOUR_CTRL,
        DFR_CHL_CTRL
    }
};

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
Name        : DFR_Reset
Description : Resets the Data Flow Router HW by disabling all MUX outputs
Parameters  : [in] DfrCore - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
FVDP_Result_t DFR_Reset(DFR_Core_t DfrCore)
{
    if (DfrCore >= DFR_NUM_CORES)
    {
        return FVDP_ERROR;
    }

    if (DfrCore == DFR_FE)
    {
        DFR_REG32_Write(DfrCore, DFR_FE_MUX_EN_0, 0);
        DFR_REG32_Write(DfrCore, DFR_FE_MUX_EN_1, 0);

#if 0    // ISSUE:  DFR registers are R/W !!!
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
#endif
    }
    else if (DfrCore == DFR_BE)
    {
        DFR_REG32_Write(DfrCore, DFR_BE_MUX_EN_0, 0);
#if 0    // ISSUE:  DFR registers are R/W !!!
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
#endif
    }
    else
    {
        DFR_REG32_Write(DfrCore, DFR_MUX_EN_0, 0);
        DFR_REG32_Write(DfrCore, DFR_MUX_EN_1, 0);
#if 0    // ISSUE:  DFR registers are R/W !!!
        if (DfrCore == DFR_PIP)
            HostUpdate_SetUpdate_LITE(FVDP_PIP, LITE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
        else if (DfrCore == DFR_AUX)
            HostUpdate_SetUpdate_LITE(FVDP_AUX, LITE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
        else
            HostUpdate_SetUpdate_LITE(FVDP_ENC, LITE_HOST_UPDATE_COMMCTRL, FORCE_UPDATE);
#endif
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : DFR_Connect
Description : Creates a DFR connection between the output of Block A and the
              input to Block B.  Blcok A is the mux source and the mux output is
              the input to Block B.
Parameters  : [in] DfrCore    - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] Connection - Specifies the connecion parameters (Block A, Block B)

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
FVDP_Result_t DFR_Connect(DFR_Core_t DfrCore, DFR_Connection_t Connection)
{
    const DFR_CoreInfo_t* Dfr_p;
    uint32_t              Enable_Channel = 0;
    FVDP_Result_t         Result = FVDP_OK;

    // Error checking that the DFR_Core_t input parameter is valid
    if (DfrCore >= DFR_NUM_CORES)
    {
        return FVDP_ERROR;
    }

    // Point to the DFR Core Info structure that is associated with the DFR_Core_t input parameter
    Dfr_p = &(DFR_CoreInfoList[DfrCore]);

    // Error checking that the input parameters are valid
    if ( (Connection.Block_A >= Dfr_p->NumMuxSources) || (Connection.Block_A_Type >= DFR_NUM_CONNECTION_TYPES) ||
         (Connection.Block_B >= Dfr_p->NumOfMuxes)    || (Connection.Block_B_Type >= DFR_NUM_CONNECTION_TYPES) )
    {
        return FVDP_ERROR;
    }

    // Decide which MUX channels should be enabled
    if (Connection.Block_B_Type == DFR_Y_ONLY)
    {
        Enable_Channel = DFR_Channel_Y_G;
    }
    else if (Connection.Block_B_Type == DFR_UV_ONLY)
    {
        Enable_Channel = DFR_Channel_U_UV_B;
    }
    else if (Connection.Block_B_Type == DFR_YUV422)
    {
        Enable_Channel = (DFR_Channel_Y_G | DFR_Channel_U_UV_B);
    }
    else    // DFR_RGB_YUV444
    {
        Enable_Channel = (DFR_Channel_Y_G | DFR_Channel_U_UV_B | DFR_Channel_V_R);
    }

    // Program the DFR Color Control for Block_A and Block_B
    Result |= DFR_ColorControl(DfrCore, Connection.Block_A_Type, &(Dfr_p->MuxSourceColorCtrlList[Connection.Block_A]));
    Result |= DFR_ColorControl(DfrCore, Connection.Block_B_Type, &(Dfr_p->MuxColorCtrlList[Connection.Block_B]));

    // Select and Enable the Muxes
    if (Enable_Channel & DFR_Channel_Y_G)
    {
        Result |= DFR_MuxSelect(DfrCore, &(Dfr_p->MuxList[Connection.Block_B].Y_G_Channel),
                                Dfr_p->MuxSourceList[Connection.Block_A].Y_G_ChannelNum);
    }
    if (Enable_Channel & DFR_Channel_U_UV_B)
    {
        Result |= DFR_MuxSelect(DfrCore, &(Dfr_p->MuxList[Connection.Block_B].U_UV_B_Channel),
                                Dfr_p->MuxSourceList[Connection.Block_A].U_UV_B_ChannelNum);
    }
    if (Enable_Channel & DFR_Channel_V_R)
    {
        Result |= DFR_MuxSelect(DfrCore, &(Dfr_p->MuxList[Connection.Block_B].V_R_Channel),
                                Dfr_p->MuxSourceList[Connection.Block_A].V_R_ChannelNum);
    }

    // ISSUE:  DFR registers are R/W !!!
#if 0
    // Set the Host Update for the DFR Block
    if (DfrCore == DFR_FE)
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
    else if (DfrCore == DFR_BE)
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
    else if (DfrCore == DFR_PIP)
        HostUpdate_SetUpdate_LITE(FVDP_PIP, LITE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
    else if (DfrCore == DFR_AUX)
        HostUpdate_SetUpdate_LITE(FVDP_AUX, LITE_HOST_UPDATE_COMMCTRL, SYNC_UPDATE);
    else
        HostUpdate_SetUpdate_LITE(FVDP_ENC, LITE_HOST_UPDATE_COMMCTRL, FORCE_UPDATE);
#endif

    return Result;
}

/*******************************************************************************
Name        : DFR_MuxSelect
Description : Enables the mux and selects the source
Parameters  : [in] DfrCore    - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] MuxChannel - Register information for the mux
              [in] MuxInputSelNum - The mux source number

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t DFR_MuxSelect(DFR_Core_t DfrCore,
                                   const DFR_MuxChannel_t* MuxChannel,
                                   uint8_t MuxInputSelNum)
{
    // Check that the Mux Channel is valid and that the input selection is valid
    if ((MuxChannel->EnRegAddr == DFR_INVALID_CH) || (MuxInputSelNum == DFR_INVALID_PORT))
    {
        return FVDP_ERROR;
    }

    // Program the registers to enable the mux and select the input
    DFR_REG32_Set(DfrCore, MuxChannel->EnRegAddr, MuxChannel->EnBitMask);
    DFR_REG32_ClearAndSet(DfrCore, MuxChannel->SelRegAddr, MuxChannel->SelRegMask, MuxChannel->SelRegOffset, MuxInputSelNum);

    return FVDP_OK;
}

/*******************************************************************************
Name        : DFR_ColorControl
Description : Sets the color control registers (the number of channels) for the
              mux based on the given connection type
Parameters  : [in] DfrCore    - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] ConnectionType - The connection type (i.e. number of channel)
              [in] MuxInputSelNum - The color control register bit masks

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t DFR_ColorControl(DFR_Core_t DfrCore,
                                      DFR_ConnectionType_t ConnectionType,
                                      const DFR_ColorCtrl* ColorCtrl)
{
    const DFR_CoreInfo_t* Dfr_p;

    // Point to the DFR Core Info structure that is associated with the DFR_Core_t input parameter
    Dfr_p = &(DFR_CoreInfoList[DfrCore]);

    // Some Blocks (MCC) do not have Color Control selection so there is nothing to do, just return.
    if (ColorCtrl->ColorSpaceBitMask == DFR_NOT_APPLICABLE)
    {
        return FVDP_OK;
    }

    // Check that the Connection Type is supported by this Block
    if (ConnectionType == DFR_RGB_YUV444)
    {
        if (ColorCtrl->ColorSpaceBitMask == DFR_NO_RGB_YUV444)
        {
            // The Block does not support RGB_YUV444 color space.
            return FVDP_ERROR;
        }
    }
    if (ConnectionType == DFR_Y_ONLY)
    {
        if (ColorCtrl->LumaOnlyBitMask == DFR_NO_LUMA_ONLY)
        {
            // The Block does not support Y_ONLY color space.
            return FVDP_ERROR;
        }
    }

    // Program the Color Control registers
    if (ConnectionType == DFR_RGB_YUV444)
    {
        // Configure for RGB_YUV444 (3 channels)
        DFR_REG32_Clear(DfrCore, Dfr_p->DFR_COLOUR_CTRL_RegAddr, ColorCtrl->ColorSpaceBitMask);
    }
    else
    {
        // Check that this Block contains a COLOR_CTRL entry.
        // If not, it's always in YUV_422 and there's no register bit to program
        if (ColorCtrl->ColorSpaceBitMask != DFR_NO_RGB_YUV444)
        {
            // Configure for YUV_422 (2 channels)
            DFR_REG32_Set(DfrCore, Dfr_p->DFR_COLOUR_CTRL_RegAddr, ColorCtrl->ColorSpaceBitMask);
        }

        if (ConnectionType == DFR_Y_ONLY)
        {
            DFR_REG32_Set(DfrCore, Dfr_p->DFR_CHL_CTRL_RegAddr, ColorCtrl->LumaOnlyBitMask);
        }
        else
        {
            // Check if this block supports LUMA_ONLY, otherwise there is no bit to clear.
            if (ColorCtrl->LumaOnlyBitMask != DFR_NO_LUMA_ONLY)
            {
                DFR_REG32_Clear(DfrCore, Dfr_p->DFR_CHL_CTRL_RegAddr, ColorCtrl->LumaOnlyBitMask);
            }
        }
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : DFR_PrintConnections
Description : Prints all the connections that are currently enabled in the DFR
Parameters  : [in] DfrCore - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
FVDP_Result_t DFR_PrintConnections(DFR_Core_t DfrCore)
{
    const DFR_CoreInfo_t*  Dfr_p;
    const DFR_Mux_t*       mux_p;
    uint32_t               mux_num;
    uint32_t               Channel_Enabled = 0;
    uint32_t               mux_input_Y_G = 0;
    uint32_t               mux_input_U_UV_B = 0;
    uint32_t               mux_input_V_R = 0;
    uint32_t               mux_input_block = 0;
    DFR_ConnectionType_t   block_A_connection_type;
    DFR_ConnectionType_t   block_B_connection_type;
    bool                   error_flag;

    // Error checking that the DFR_Core_t input parameter is valid
    if (DfrCore >= DFR_NUM_CORES)
    {
        return FVDP_ERROR;
    }

    // Point to the DFR Core Info structure that is associated with the DFR_Core_t input parameter
    Dfr_p = &(DFR_CoreInfoList[DfrCore]);

    // Loop through each of the muxes
    for (mux_num = 0; mux_num < Dfr_p->NumOfMuxes; mux_num++)
    {
        error_flag = FALSE;
        Channel_Enabled = 0;
        mux_p = &(Dfr_p->MuxList[mux_num]);

        // Check which of the MUX channels are enabled
        if (DFR_IsMuxChannelEnabled(DfrCore, &(mux_p->Y_G_Channel)))
            Channel_Enabled |= DFR_Channel_Y_G;
        if (DFR_IsMuxChannelEnabled(DfrCore, &(mux_p->U_UV_B_Channel)))
            Channel_Enabled |= DFR_Channel_U_UV_B;
        if (DFR_IsMuxChannelEnabled(DfrCore, &(mux_p->V_R_Channel)))
            Channel_Enabled |= DFR_Channel_V_R;

        // Get the Mux source logical block number
        if (Channel_Enabled & DFR_Channel_Y_G)
        {
            mux_input_Y_G = DFR_GetMuxSource(DfrCore, &(mux_p->Y_G_Channel), 0);
            if (mux_input_Y_G == DFR_INVALID_PORT)
                error_flag = TRUE;
            else
                mux_input_block = mux_input_Y_G;
        }
        if (Channel_Enabled & DFR_Channel_U_UV_B)
        {
            mux_input_U_UV_B = DFR_GetMuxSource(DfrCore, &(mux_p->U_UV_B_Channel), 1);
            if (mux_input_U_UV_B == DFR_INVALID_PORT)
                error_flag = TRUE;
            else if (Channel_Enabled & DFR_Channel_Y_G)
            {
                if (mux_input_U_UV_B != mux_input_Y_G)
                    error_flag = TRUE;
            }
            else
                mux_input_block = mux_input_U_UV_B;
        }
        if (Channel_Enabled & DFR_Channel_V_R)
        {
            mux_input_V_R = DFR_GetMuxSource(DfrCore, &(mux_p->V_R_Channel), 2);
            if (mux_input_V_R == DFR_INVALID_PORT)
                error_flag = TRUE;
            else
            {
                if (Channel_Enabled & (DFR_Channel_Y_G | DFR_Channel_U_UV_B))
                {
                    if (mux_input_V_R != mux_input_block)
                        error_flag = TRUE;
                }
                else
                    mux_input_block = mux_input_V_R;
            }
        }

        if (Channel_Enabled != 0)
        {

            block_A_connection_type = DFR_GetBlockColorCtrl(DfrCore, Dfr_p, Dfr_p->MuxSourceColorCtrlList[mux_input_block]);
            block_B_connection_type = DFR_GetBlockColorCtrl(DfrCore, Dfr_p, Dfr_p->MuxColorCtrlList[mux_num]);

            if (error_flag)
            {
                TRC( TRC_ID_MAIN_INFO, "   ERROR      " );
            }
            else
            {
                TRC( TRC_ID_MAIN_INFO, "%s (%s) ", Dfr_p->MuxSourceNameList[mux_input_block], DFR_GetConnectionTypeName(Channel_Enabled, block_A_connection_type) );
            }

            TRC( TRC_ID_MAIN_INFO, "------> %s (%s)", Dfr_p->MuxNameList[mux_num], DFR_GetConnectionTypeName(Channel_Enabled, block_B_connection_type) );
        }
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : DFR_IsMuxChannelEnabled
Description : Returns whether the mux channel is enabled or not.
Parameters  : [in] DfrCore - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] MuxChannel - Register information for the mux

Assumptions : None
Limitations : None
Returns     : bool - TRUE if the mux channel is enabled, else FALSE
*******************************************************************************/
static bool DFR_IsMuxChannelEnabled(DFR_Core_t DfrCore, const DFR_MuxChannel_t*  MuxChannel)
{
    if (MuxChannel->EnRegAddr == DFR_INVALID_CH)
        return FALSE;

    if (DFR_REG32_Read(DfrCore, MuxChannel->EnRegAddr) & MuxChannel->EnBitMask)
        return TRUE;
    else
        return FALSE;
}

/*******************************************************************************
Name        : DFR_GetMuxSource
Description : Returns the mux source number for a given mux channel
Parameters  : [in] DfrCore - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] MuxChannel - Register information for the mux
              [in] Ch - The physical mux channel (0 = Y_G; 1 = U_UV_B; 2 = V_R)

Assumptions : None
Limitations : None
Returns     : uint32 - returns the mux source number
*******************************************************************************/
static uint32_t DFR_GetMuxSource(DFR_Core_t DfrCore, const DFR_MuxChannel_t*  MuxChannel, uint8_t Ch)
{
    const DFR_CoreInfo_t*  Dfr_p;
    uint32_t               mux_input_source;
    uint32_t               mux_input_block;

    // Point to the DFR Core Info structure that is associated with the DFR_Core_t input parameter
    Dfr_p = &(DFR_CoreInfoList[DfrCore]);

    // Get the Mux input source channel number
    mux_input_source = (DFR_REG32_Read(DfrCore, MuxChannel->SelRegAddr) & MuxChannel->SelRegMask) >> MuxChannel->SelRegOffset;

    // Go through the logical mux input blocks, to find which one has the mux input number
    for (mux_input_block = 0; mux_input_block < Dfr_p->NumMuxSources; mux_input_block++)
    {
        if (Ch == 0)
        {
            if (Dfr_p->MuxSourceList[mux_input_block].Y_G_ChannelNum == mux_input_source)
            {
                return mux_input_block;
            }
        }
        else if  (Ch == 1)
        {
            if (Dfr_p->MuxSourceList[mux_input_block].U_UV_B_ChannelNum == mux_input_source)
            {
                return mux_input_block;
            }
        }
        else
        {
            if (Dfr_p->MuxSourceList[mux_input_block].V_R_ChannelNum == mux_input_source)
            {
                return mux_input_block;
            }
        }
    }

    // Did not find the channel source number in the table
    return (uint32_t)DFR_INVALID_PORT;
}

/*******************************************************************************
Name        : DFR_GetBlockColorCtrl
Description : Return the Connection Type (i.e. color control info) for a given
              mux channel.
Parameters  : [in] DfrCore   - The DFR Core to reset (FE, BE, PIP, AUX, or ENC)
              [in] Dfr_p     - The DFR Core information
              [in] ColorCtrl - Color Control register bit mask information

Assumptions : None
Limitations : None
Returns     : FVDP_Result_t - FVDP_OK if no errors, else FVDP_ERROR
*******************************************************************************/
static DFR_ConnectionType_t DFR_GetBlockColorCtrl(DFR_Core_t DfrCore,
                                                  const DFR_CoreInfo_t* Dfr_p,
                                                  DFR_ColorCtrl ColorCtrl)
{
    if (ColorCtrl.ColorSpaceBitMask == DFR_NOT_APPLICABLE)
        return DFR_NOT_APPLICABLE;

    if (ColorCtrl.ColorSpaceBitMask != DFR_NO_RGB_YUV444)
    {
        if ((DFR_REG32_Read(DfrCore, Dfr_p->DFR_COLOUR_CTRL_RegAddr) & ColorCtrl.ColorSpaceBitMask) == 0)
        {
            return DFR_RGB_YUV444;
        }
    }

    if (ColorCtrl.LumaOnlyBitMask == DFR_NO_LUMA_ONLY)
        return DFR_YUV422;

    if (DFR_REG32_Read(DfrCore, Dfr_p->DFR_CHL_CTRL_RegAddr) & ColorCtrl.LumaOnlyBitMask)
        return DFR_Y_ONLY;
    else
        return DFR_YUV422;
}

/*******************************************************************************
Name        : DFR_GetConnectionTypeName
Description : Return a character string of the connection type
Parameters  : [in] Channel_Enabled - Specifies which mux channels are enabled
              [in] ConnectionType  - The connection type for this mux

Assumptions : None
Limitations : None
Returns     : const char* - connection type name string
*******************************************************************************/
static const char* DFR_GetConnectionTypeName(uint32_t Channel_Enabled, DFR_ConnectionType_t ConnectionType)
{
    switch (Channel_Enabled)
    {
        case (DFR_Channel_Y_G | DFR_Channel_U_UV_B | DFR_Channel_V_R):
        {
            if ((ConnectionType == DFR_NOT_APPLICABLE) || (ConnectionType == DFR_RGB_YUV444))
                return DFR_ConnectionTypeNameList[DFR_RGB_YUV444];
            break;
        }
        case (DFR_Channel_Y_G | DFR_Channel_U_UV_B):
        {
            if ((ConnectionType == DFR_NOT_APPLICABLE) || (ConnectionType == DFR_YUV422))
                return DFR_ConnectionTypeNameList[DFR_YUV422];
            break;
        }
        case (DFR_Channel_Y_G):
        {
            if ((ConnectionType == DFR_NOT_APPLICABLE) || (ConnectionType == DFR_Y_ONLY))
                return DFR_ConnectionTypeNameList[DFR_Y_ONLY];
            break;
        }
        case (DFR_Channel_U_UV_B):
        {
            if ((ConnectionType == DFR_NOT_APPLICABLE) || (ConnectionType == DFR_YUV422))
                return DFR_ConnectionTypeNameList[DFR_UV_ONLY];
        }
        default:
            break;
    }

    return DFR_ConnectionTypeError;
}

