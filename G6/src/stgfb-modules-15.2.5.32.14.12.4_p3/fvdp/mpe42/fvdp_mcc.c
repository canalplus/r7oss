/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_mcc.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_mcc.h"
#include "fvdp_hostupdate.h"


/* Private Types ------------------------------------------------------------ */

typedef enum
{
    MCC_READ,
    MCC_WRITE
}MCCClientType_t;

typedef enum
{
    NO_ODD_SIGNAL,
    READ_ODD_SIGN,
    WRITE_ODD_SIG
}MCC_OddSignal_t;

typedef enum
{
    MCC_FE,
    MCC_BE,
    MCC_AUX,
    MCC_PIP,
    MCC_ENC,
    NUM_OF_MCC_MODULES
}MCC_Modules_t;

typedef enum
{
    NO_COMPRESSION,
    LUMA_BASE     = BIT0,
    AGGRESSIVE    = BIT1,
    RGB_565       = BIT2,
    UV_422_TO_420 = BIT3,
}MCC_ComprSupported_t;

typedef struct
{
    MCC_Modules_t        mcc_module;
    uint32_t             addr;
    MCC_OddSignal_t      oddsignal;
    MCCClientType_t      type;
    uint32_t             PixSizeMask;
    MCC_ComprSupported_t compression;
}MCC_ClientRegs_t;

typedef char Mcc_Clientname_t[14];

/* Private Constants -------------------------------------------------------- */
#define RD_CTRL_OFFS            4
#define RD_ADDR_CTRL_OFFS       8
#define RD_STRIDE_OFFS          12
#define RD_OFFSET_OFFS          16
#define RD_PART_PKT_OFFS        20
#define RD_ADDR_OFFS            24
#define RD_THROTTLE_CTRL_OFFS   28
#define RD_HOR_SIZE_OFFS        32

#define WR_ADDR_CTRL_OFFS       4
#define WR_STRIDE_OFFS          8
#define WR_ADDR_OFFS            12
#define WR_THROTTLE_CTRL_OFFS   16
#define WR_ODD_OFFS             20

#define RGB_UV_WR_CLIENT_CTRL   AUX_RGB_UV_WR_CLIENT_CTRL
#define Y_WR_CLIENT_CTRL        AUX_Y_WR_CLIENT_CTRL
#define RGB_UV_RD_CLIENT_CTRL   AUX_RGB_UV_RD_CLIENT_CTRL
#define Y_RD_CLIENT_CTRL        AUX_Y_RD_CLIENT_CTRL

#define SIZE_OF_MCC_WORD        8 /*in bytes*/
#define RD_420_TO_422_COEF_0    8
static const uint32_t MccBaseAddr[NUM_OF_MCC_MODULES] =
{
    FVDP_MCC_FE_BASE_ADDRESS,
    FVDP_MCC_BE_BASE_ADDRESS,
    FVDP_MCC_AUX_BASE_ADDRESS,
    FVDP_MCC_PIP_BASE_ADDRESS,
    FVDP_MCC_ENC_BASE_ADDRESS
};

#define MCC_REG32_Write(MccModule,Addr,Val)         REG32_Write(Addr+MccBaseAddr[MccModule],Val)
#define MCC_REG32_Read(MccModule,Addr)              REG32_Read(Addr+MccBaseAddr[MccModule])
#define MCC_REG32_Set(MccModule,Addr,BitsToSet)     REG32_Set(Addr+MccBaseAddr[MccModule],BitsToSet)
#define MCC_REG32_Clear(MccModule,Addr,BitsToClear) REG32_Clear(Addr+MccBaseAddr[MccModule],BitsToClear)
#define MCC_REG32_ClearAndSet(MccModule,Addr,BitsToClear,Offset,ValueToSet) \
                                                    REG32_ClearAndSet(Addr+MccBaseAddr[MccModule],BitsToClear,Offset,ValueToSet)
#define MCC_REG32_SetField(MccModule,Addr,Field,ValueToSet) \
                                                    REG32_SetField(Addr,Field,ValueToSet)(Addr+MccBaseAddr[MccModule],Field,ValueToSet)


#define CLIENT_EN_MASK          0x01
#define VER_SIZE_MASK           0x07ff0000
#define VER_SIZE_OFFSET         16
#define RGB_MODE_OFFSET         4
#define RD_565_EN_MASK          0x100
#define RD_565_EN_OFFSET        8
#define WR_565_EN_MASK          0x40
#define WR_565_EN_OFFSET        6
#define COMPRESSION_EN_MASK     0x100
#define AGGRESSIVE_ALGO_MASK    0x200
#define PIXSIZE_MASK            0x20
#define PIXSIZE_MASK_RGB        0x80
#define CONV_TO_420_EN_MASK     0x80
#define CONV_TO_422_EN_MASK     0x80
#define CONV_RGB_RD_TO_422_EN_MASK 0x200

#define RGB_MODE_MASK           0x10
#define END_ADDR_OFFSET         12
#define END_ADDR_MASK           0x00fff000

#define END_OFFSET_OFFSET       8
#define END_OFFSET_MASK         0x00007f00
#define START_OFFSET_MASK       0x0000007f

#define PKT_OFFSET_OFFSET       16
#define DROP_ODD_MASK           0x100
#define DROP_ODD_OFFSET         8
#define OUT_ODD_MASK            0x08
#define OUT_ODD_OFFSET          3
#define COMP_ALGO_MASK          0x7c00
#define COMP_ALGO_OFFSET        10
#define START_ADDR_MASK         0xfff

MCC_ClientRegs_t MCC_ClientsTbl[]=
{
    {MCC_FE,  CCS_WR_CLIENT_CTRL,       READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_FE,  CCS_RD_CLIENT_CTRL,       NO_ODD_SIGNAL,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_FE,  P1_Y_RD_CLIENT_CTRL,      WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_FE,  P2_Y_RD_CLIENT_CTRL,      WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_FE,  P3_Y_RD_CLIENT_CTRL,      WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_FE,  P1_UV_RD_CLIENT_CTRL,     NO_ODD_SIGNAL,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_FE,  P2_UV_RD_CLIENT_CTRL,     NO_ODD_SIGNAL,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_FE,  P3_UV_RD_CLIENT_CTRL,     NO_ODD_SIGNAL,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_FE,  D_Y_WR_CLIENT_CTRL,       READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_FE,  D_UV_WR_CLIENT_CTRL,      READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_FE,  MCDI_WR_CLIENT_CTRL,      READ_ODD_SIGN,  MCC_WRITE,            0,    NO_COMPRESSION},
    {MCC_FE,  MCDI_RD_CLIENT_CTRL,      NO_ODD_SIGNAL,  MCC_READ,             0,    NO_COMPRESSION},
    {MCC_FE,  C_Y_WR_CLIENT_CTRL,       READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_FE,  C_RGB_UV_WR_CLIENT_CTRL,  READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_BE,  OP_Y_RD_CLIENT_CTRL,      WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_BE,  OP_RGB_UV_RD_CLIENT_CTRL, WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK_RGB,LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_BE,  OP_Y_WR_CLIENT_CTRL,      READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_BE,  OP_UV_WR_CLIENT_CTRL,     READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420},
    {MCC_AUX, RGB_UV_WR_CLIENT_CTRL,    READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_AUX, Y_WR_CLIENT_CTRL,         READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_AUX, RGB_UV_RD_CLIENT_CTRL,    WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK_RGB,LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_AUX, Y_RD_CLIENT_CTRL,         WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_PIP, RGB_UV_WR_CLIENT_CTRL,    READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_PIP, Y_WR_CLIENT_CTRL,         READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_PIP, RGB_UV_RD_CLIENT_CTRL,    WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK_RGB,LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_PIP, Y_RD_CLIENT_CTRL,         WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_ENC, RGB_UV_WR_CLIENT_CTRL,    READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_ENC, Y_WR_CLIENT_CTRL,         READ_ODD_SIGN,  MCC_WRITE, PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_ENC, RGB_UV_RD_CLIENT_CTRL,    WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK_RGB,LUMA_BASE|RGB_565|UV_422_TO_420},
    {MCC_ENC, Y_RD_CLIENT_CTRL,         WRITE_ODD_SIG,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_ENC, AUX_TNR_Y_RD_CLIENT_CTRL, NO_ODD_SIGNAL,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|AGGRESSIVE},
    {MCC_ENC, AUX_TNR_UV_RD_CLIENT_CTRL,NO_ODD_SIGNAL,  MCC_READ,  PIXSIZE_MASK,    LUMA_BASE|UV_422_TO_420}
};

Mcc_Clientname_t MCC_ClientName_tbl[MCC_NUM_OF_CLIENTS] =
{
    "C_UV_RAW_WR  ",//CCS_WR",
    "P2_UV_RAW_RD ",//CCS_RD",
    "P1_Y_RD      ",//P1_Y_RD",
    "P2_Y_RD      ",//P2_Y_RD",
    "P3_Y_RD      ",//P3_Y_RD",
    "P1_UV_RD     ",//P1_UV_RD",
    "P2_UV_RD     ",//P2_UV_RD",
    "P3_UV_RD     ",//P3_UV_RD",
    "C_Y_WR       ",//D_Y_WR",
    "C_UV_WR      ",//D_UV_WR",
    "MCDI_SB_WR   ",//MCDI_WR",
    "MCDI_SB_RD   ",//MCDI_RD",
    "PRO_Y_G_WR   ",//C_Y_WR",
    "PRO_U_UV_B_WR",//C_RGB_UV_WR",
    "OP_Y_RD      ",//OP_Y_RD",
    "OP_RGB_UV_RD ",//OP_RGB_UV_RD",
    "OP_Y_WR      ",//OP_Y_WR",
    "OP_UV_WR     ",//OP_UV_WR",
    "AUX_RGB_UV_WR",//AUX_RGB_UV_WR",
    "AUX_Y_WR     ",//AUX_Y_WR",
    "AUX_RGB_UV_RD",//AUX_RGB_UV_RD",
    "AUX_Y_RD     ",//AUX_Y_RD",
    "PIP_RGB_UV_WR",//PIP_RGB_UV_WR",
    "PIP_Y_WR     ",//PIP_Y_WR",
    "PIP_RGB_UV_RD",//PIP_RGB_UV_RD",
    "PIP_Y_RD     ",//PIP_Y_RD",
    "ENC_RGB_UV_WR",//ENC_RGB_UV_WR",
    "ENC_Y_WR     ",//ENC_Y_WR",
    "ENC_RGB_UV_RD",//ENC_RGB_UV_RD",
    "ENC_Y_RD     ",//ENC_Y_RD",
    "ENC_TNR_Y_RD ",//ENC_TNR_Y_RD",
    "ENC_TNR_UV_RD" //ENC_TNR_UV_RD"
};

/* Private Macros ----------------------------------------------------------- */

#define TrMCC(level,...)  TraceModuleOn(level,MccDebugLevel,__VA_ARGS__)

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */
static FVDP_Result_t MCC_SetCropWindow(MCC_Client_t client, MCC_CropWindow_t crop, uint16_t hsize, uint16_t bpp);
static FVDP_Result_t MCC_HostUpdate(MCC_Modules_t MccModule, MCCClientType_t MccClientType);

/* Global Variables --------------------------------------------------------- */

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : MCC_Init
Description : Initialize MCC registers for the specified Channel
Parameters  : CH - The FVDP Channel
Assumptions : sinc update must be requested after this function execution
Limitations : NO
Returns     : FVDP_OK or FVDP_ERROR
*******************************************************************************/
FVDP_Result_t MCC_Init(FVDP_CH_t CH)
{
    int indx;
    MCC_ClientRegs_t client;

    if (CH == FVDP_MAIN)
    {
        MCC_REG32_Write(MCC_FE, FVDP_FE_PLUG_CTRL, 0x1);
        MCC_REG32_Write(MCC_FE, FVDP_FE_0_RD_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_FE, FVDP_FE_1_RD_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_FE, FVDP_FE_2_RD_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_FE, FVDP_FE_0_WR_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_FE, FVDP_FE_1_WR_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_FE, FVDP_FE_MCC_BYPASS, 0x0);
        for(indx = 0; indx < AUX_RGB_UV_WR; indx++)
        {
            client = MCC_ClientsTbl[indx];
            MCC_REG32_Write(client.mcc_module, client.addr, 0);
            if(client.type == MCC_WRITE) {
                MCC_REG32_Write(client.mcc_module, client.addr + WR_THROTTLE_CTRL_OFFS, 0);
            } else if(client.type == MCC_READ) {
                MCC_REG32_Write(client.mcc_module, client.addr + RD_THROTTLE_CTRL_OFFS, 0);
            } else
                return FVDP_ERROR;
        }
        MCC_REG32_Write(MCC_FE, D_Y_WR_CLIENT_CTRL, 0x1F << COMP_ALGO_OFFSET);
        MCC_REG32_Write(MCC_FE, C_Y_WR_CLIENT_CTRL, 0x1F << COMP_ALGO_OFFSET);
        MCC_REG32_Clear(MCC_FE, P1_Y_RD_CLIENT_CTRL, P1_Y_RD_UNCOMP_BANDING_EN_MASK);
        MCC_REG32_Clear(MCC_FE, P2_Y_RD_CLIENT_CTRL, P2_Y_RD_UNCOMP_BANDING_EN_MASK);
        MCC_REG32_Clear(MCC_FE, P3_Y_RD_CLIENT_CTRL, P3_Y_RD_UNCOMP_BANDING_EN_MASK);
        MCC_REG32_Clear(MCC_FE, FVDP_FE_MCC_BYPASS, 0);

        MCC_REG32_Write(MCC_BE, FVDP_BE_PLUG_CTRL, 0x1);
        MCC_REG32_Write(MCC_BE, FVDP_BE_RD_0_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_BE, FVDP_BE_RD_1_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(MCC_BE, FVDP_BE_WR_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Clear(MCC_BE, FVDP_BE_MCC_BYPASS, 0);

        MCC_REG32_Write(MCC_BE, OP_RGB_UV_RD_420_COEF, RD_420_TO_422_COEF_0);

        // Sync Update for FVDP_FE and FVDP_BE MCCs
        MCC_HostUpdate(MCC_FE, MCC_WRITE);
        MCC_HostUpdate(MCC_BE, MCC_READ);
    }
    else
    {
        MCC_Client_t  client_start;
        MCC_Client_t  client_end;
        MCC_Modules_t mcc_module;

        if (CH == FVDP_PIP)
        {
            client_start = PIP_RGB_UV_WR;
            client_end   = PIP_Y_RD;
            mcc_module   = MCC_PIP;
        }
        else if (CH == FVDP_AUX)
        {
            client_start = AUX_RGB_UV_WR;
            client_end   = AUX_Y_RD;
            mcc_module   = MCC_AUX;
        }
        else // (CH == FVDP_ENC)
        {
            client_start = ENC_RGB_UV_WR;
            client_end   = ENC_TNR_UV_RD;
            mcc_module   = MCC_ENC;
        }

        MCC_REG32_Write(mcc_module, AUX_VDP_PLUG_CTRL,    0x1);
        MCC_REG32_Write(mcc_module, AUX_RD_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(mcc_module, AUX_WR_PLUG_TRF_CTRL, 0x5E2);
        MCC_REG32_Write(mcc_module, AUX_VDP_MCC_BYPASS,   0);

        for(indx = client_start; indx <= client_end; indx++)
        {
            client = MCC_ClientsTbl[indx];
            MCC_REG32_Write(client.mcc_module, client.addr, 0);
            if(client.type == MCC_WRITE) {
                MCC_REG32_Write(client.mcc_module, client.addr + WR_THROTTLE_CTRL_OFFS, 0);
            } else if(client.type == MCC_READ) {
                MCC_REG32_Write(client.mcc_module, client.addr + RD_THROTTLE_CTRL_OFFS, 0);
            } else
                return FVDP_ERROR;
        }

        // Sync Update for the MCC registers
        MCC_HostUpdate(mcc_module, MCC_READ);
        MCC_HostUpdate(mcc_module, MCC_WRITE);
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : MCC_IsEnabled
Description : Checks to see if MCC client is enabled
Parameters  : client - MCC client id enumerator
Assumptions :
Limitations : NO
Returns     : TRUE if client has been enabled
*******************************************************************************/
bool MCC_IsEnabled(MCC_Client_t client)
{
    int32_t       ctrl_reg_addr = MCC_ClientsTbl[client].addr;
    MCC_Modules_t mcc_module    = MCC_ClientsTbl[client].mcc_module;

    return (MCC_REG32_Read(mcc_module, ctrl_reg_addr) & CLIENT_EN_MASK) != 0;
}

/*******************************************************************************
Name        : MCC_Enable
Description : Enables one of the MCC clients
Parameters  : client - MCC client id enumerator
Assumptions : sinc update must be requested after this function execution
Limitations : NO
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t MCC_Enable(MCC_Client_t client)
{
    int32_t       ctrl_reg_addr = MCC_ClientsTbl[client].addr;
    MCC_Modules_t mcc_module    = MCC_ClientsTbl[client].mcc_module;

    MCC_REG32_Set(mcc_module, ctrl_reg_addr, CLIENT_EN_MASK);
    MCC_HostUpdate(mcc_module, MCC_ClientsTbl[client].type);
    return FVDP_OK;
}

/*******************************************************************************
Name        : MCC_Disable
Description : Disables one of the MCC clients
Parameters  : client - MCC client id enumerator
Assumptions : sinc update must be requested after this function execution
Limitations : NO
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t MCC_Disable(MCC_Client_t client)
{
    int32_t       ctrl_reg_addr = MCC_ClientsTbl[client].addr;
    MCC_Modules_t mcc_module    = MCC_ClientsTbl[client].mcc_module;

    MCC_REG32_Clear(mcc_module, ctrl_reg_addr, CLIENT_EN_MASK);
    MCC_HostUpdate(mcc_module, MCC_ClientsTbl[client].type);
    return FVDP_OK;
}


/*******************************************************************************
Name        : MCC_GetRequiredStrideValue
Description : Calculates minimal stride value required by MCC clients
Parameters  :   h_size 	- image active line size in pixels (horizontal resolution)
				bpp    	-  bits per pixel, supposed to be 8 or 10
				rgb_mode - TRUE - image is RGB, FALSE - image is YUV
Limitations : NO
Returns     : stride value in bytes
*******************************************************************************/
uint32_t MCC_GetRequiredStrideValue(uint32_t h_size, uint32_t bpp, bool rgb_mode)
{
    uint32_t stride_value;

    if(rgb_mode)
        stride_value = 1 + (3 * h_size * bpp - 1)/64;
    else
        stride_value = 1 + (h_size * bpp - 1)/64;

    return (stride_value * 8); /*return stride value in bytes*/
}

/*******************************************************************************
Name        : MCC_Config
Description : Configurate selected MCC client.
Parameters  :
                client - client id enumerator
                config - a structure configuration data (see MCC_Config_t)
Assumptions : sinc update must be requested after this function execution
Limitations : NO
Returns     : FVDP_OK, FVDP_ERROR
*******************************************************************************/
FVDP_Result_t MCC_Config(MCC_Client_t client, MCC_Config_t config)
{
    FVDP_Result_t result = FVDP_OK;
    MCC_ClientRegs_t memclient = MCC_ClientsTbl[client];
    uint32_t addr0 = memclient.addr;
    MCC_Modules_t mcc_module = memclient.mcc_module;
    uint32_t RegValue = MCC_REG32_Read(mcc_module,addr0) & (CLIENT_EN_MASK | COMP_ALGO_MASK);
    MCC_Compression_t comp = config.compression;
    uint32_t Power_Factor = 1;

    if(config.stride % 8)
    {
        TRC( TRC_ID_MAIN_INFO, "FVDP MCC_Config: client %d MCC_Config error - the stride is not divisible by 8", client );
        return FVDP_ERROR;
    }

    RegValue |= ((config.bpp == 10) ||(config.bpp == 30)) ? memclient.PixSizeMask : 0;

    /* enable RGB YUV mode */
    RegValue |= (config.rgb_mode) ? RGB_MODE_MASK :0;

    if(config.Alternated_Line_Read == TRUE)
    {
        Power_Factor = 2;
        config.stride *= 2;
    }

    /* set compression mode */
    switch(comp)
    {
        case MCC_COMP_LUMA_BASE:
            RegValue |= (memclient.compression & LUMA_BASE)  ? COMPRESSION_EN_MASK  :0;
            break;
        case MCC_COMP_LUMA_AGGRESSIVE:
            RegValue |= (memclient.compression & AGGRESSIVE) ? AGGRESSIVE_ALGO_MASK :0;
            break;
        case MCC_COMP_UV_422_TO_420:
            /* is it RD_UV_RGB client? */
            if((memclient.type == MCC_READ) && (memclient.compression & RGB_565))
                RegValue |= (memclient.compression & UV_422_TO_420)? CONV_RGB_RD_TO_422_EN_MASK :0;
            else
                RegValue |= (memclient.compression & UV_422_TO_420)? CONV_TO_420_EN_MASK :0;

            break;
        case MCC_COMP_RGB_565:
            if(memclient.compression & RGB_565)
                RegValue |= (memclient.type == MCC_READ) ? RD_565_EN_MASK : WR_565_EN_MASK;
            break;
        default:
            break;
    }

    /* Set Vertical and Horisontal buffer sizes */
    RegValue |= ((config.buffer_v_size / Power_Factor) << VER_SIZE_OFFSET) & VER_SIZE_MASK;

    MCC_REG32_Write(mcc_module,addr0,RegValue);

    {
        uint32_t addr_ctrl_value, addr_offset;

        if(config.rgb_mode)
            addr_ctrl_value = (3 * config.buffer_h_size * config.bpp - 1)/64;
        else
            addr_ctrl_value = (config.buffer_h_size * config.bpp - 1)/64;

        /* program start and end addresses */
        addr_offset = (memclient.type == MCC_WRITE) ? WR_ADDR_CTRL_OFFS : RD_ADDR_CTRL_OFFS;
        MCC_REG32_Write(mcc_module,addr0 + addr_offset, addr_ctrl_value << END_ADDR_OFFSET);


        /* program stride value */
        addr_offset = (memclient.type == MCC_WRITE) ? WR_STRIDE_OFFS : RD_STRIDE_OFFS;
        MCC_REG32_Write(mcc_module,addr0 + addr_offset, config.stride/8);

    }
    if(memclient.type == MCC_READ)
    {
        uint32_t start_offset = 0;
        uint32_t end_offset;
        uint32_t part_pkt = 0;
        uint32_t part_pkt_offset = 64;

        if(config.rgb_mode)
            end_offset = (config.buffer_h_size * 3 * config.bpp - 1) % 64 + 1;
        else
            end_offset = (config.buffer_h_size * config.bpp - 1) % 64 + 1;

        /* program sart and end offset */
        MCC_REG32_Write(mcc_module,addr0 + RD_OFFSET_OFFS, start_offset + (end_offset << END_OFFSET_OFFSET));

        /* program part packet */
        MCC_REG32_Write(mcc_module,addr0 + RD_PART_PKT_OFFS, part_pkt + (part_pkt_offset << PKT_OFFSET_OFFSET));

        /* program horizontal size register */
        if(memclient.compression & UV_422_TO_420)
        {
            uint32_t horsize_reg_addr = addr0 + RD_HOR_SIZE_OFFS;

            if(client == OP_RGB_UV_RD)
                horsize_reg_addr = OP_RGB_UV_RD_HOR_SIZE;

            MCC_REG32_Write(mcc_module, horsize_reg_addr, config.buffer_h_size);
        }

        /* cropping */
        if(config.crop_enable)
        {
            config.crop_window.crop_v_size /= Power_Factor;
            result = MCC_SetCropWindow(client, config.crop_window, config.buffer_h_size, config.bpp);
        }

    }

    // Host Update for MCC HW
    MCC_HostUpdate(mcc_module, memclient.type);

    return result;
}
/*******************************************************************************
Name        : MCC_SetCropWindow
Description : The static function is called by MCC_Config() when cropping is requested
Parameters  :
                client - client id enumerator
                crop - a structure with cropping parameters
                hsize  horizontal buffer size in pixels
                bpp - bit per pixels of the client
Assumptions : sinc update must be requested after this function execution
              for verical cropping must be followed by MCC_SetBaseAddr()
Limitations :
Returns     : FVDP_OK, FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t MCC_SetCropWindow(MCC_Client_t client, MCC_CropWindow_t crop, uint16_t hsize, uint16_t bpp)
{
    MCC_ClientRegs_t memclient = MCC_ClientsTbl[client];
    MCC_Modules_t mcc_module = memclient.mcc_module;
    uint32_t addr0 = memclient.addr;
    uint32_t start_addr, end_addr, start_offset, end_offset;


    /*according programming model croping is applied only  for read MCC clients*/
    if(memclient.type == MCC_READ)
    {
        /* set vertical size cropping */
        MCC_REG32_ClearAndSet(mcc_module,addr0,VER_SIZE_MASK,VER_SIZE_OFFSET, crop.crop_v_size);

        /* program start and end addresses - vertical cropping*/
        start_addr = (crop.crop_h_start * bpp)/64;
        end_addr = ((crop.crop_h_start + crop.crop_h_size) * bpp -1);
        end_addr /= 64;

        MCC_REG32_Write(mcc_module,addr0 + RD_ADDR_CTRL_OFFS, start_addr + (end_addr << END_ADDR_OFFSET));

        /* program start and end offset  - horisontal cropping*/
        start_offset = (crop.crop_h_start * bpp) % 64;
        end_offset = ((crop.crop_h_start + crop.crop_h_size) * bpp-1) % 64 + 1;
        MCC_REG32_Write(mcc_module,addr0 + RD_OFFSET_OFFS, start_offset + (end_offset << END_OFFSET_OFFSET));
    }
    else
        return FVDP_ERROR;

    // Host Update for MCC HW
    MCC_HostUpdate(mcc_module, memclient.type);

    return FVDP_OK;
}
/*******************************************************************************
Name        : MCC_SetBaseAddr
Description : set MCC client base address in SDRAM
Parameters  :
                client - client id enumerator
                addr - MCC client base address
                config - a structure configuration data (see MCC_Config_t)
Assumptions : sinc update must be requested after this function execution
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t MCC_SetBaseAddr(MCC_Client_t client, uint32_t addr, MCC_Config_t Config)
{
    MCC_ClientRegs_t memclient = MCC_ClientsTbl[client];
    MCC_Modules_t mcc_module = memclient.mcc_module;

    /*specify register for writing client memory base address */
    uint32_t reg_addr_offset = (memclient.type == MCC_WRITE) ? WR_ADDR_OFFS : RD_ADDR_OFFS;
    uint32_t base_addr_reg = memclient.addr + reg_addr_offset;

    if(Config.stride % 8)
    {
        TRC( TRC_ID_MAIN_INFO, "FVDP MCC_SetBaseAddr: client %d MCC_Config error - the stride is not divisible by 8", client );
        return FVDP_ERROR;
    }

    /*modify base address if vertical croping is enabled*/
    if((Config.crop_enable) &&(MCC_ClientsTbl[client].type == MCC_READ))
    {
        uint32_t stride = Config.stride/8;
        addr += (stride * Config.crop_window.crop_v_start * SIZE_OF_MCC_WORD);
    }

    /*program base address*/
    MCC_REG32_Write(mcc_module,base_addr_reg,addr);

    // Host Update for MCC HW
    MCC_HostUpdate(mcc_module, memclient.type);

    return FVDP_OK;
}

/*******************************************************************************
Name        : MCC_SetOddSignal
Description : Sets value of the ODD signal for a read MCC client
Parameters  :
                client - client id enumerator
                odd - value for ODD signal to be set
Assumptions : sinc update must be requested after this function execution
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t MCC_SetOddSignal(MCC_Client_t client, bool odd)
{
    MCC_ClientRegs_t memclient = MCC_ClientsTbl[client];
    if((memclient.type == MCC_READ) && (memclient.oddsignal & WRITE_ODD_SIG))
        MCC_REG32_ClearAndSet(memclient.mcc_module, memclient.addr, OUT_ODD_MASK, OUT_ODD_OFFSET, odd);

    // Host Update for MCC HW
    MCC_HostUpdate(memclient.mcc_module, memclient.type);

    return FVDP_OK;
}


/*******************************************************************************
Name        : MCC_GetOddSignal
Description : Gets value of the ODD signal from a write MCC client
Parameters  :
                client - client id enumerator
Assumptions : sinc update must be requested after this function execution
Limitations :
Returns     : ODD signal value
*******************************************************************************/
bool          MCC_GetOddSignal(MCC_Client_t client)
{
    MCC_ClientRegs_t memclient = MCC_ClientsTbl[client];
    if((memclient.type == MCC_WRITE) && (memclient.oddsignal & READ_ODD_SIGN))
        return (bool)(MCC_REG32_Read(memclient.mcc_module, memclient.addr + WR_ODD_OFFS) & OUT_ODD_MASK);
    return FALSE;
}

/*******************************************************************************
Name        : MCC_HostUpdate
Description : Call the HostUpdate module perform sync/force updates to MCC HW
Parameters  : MccModule     - The MCC Module
              MccClientType - The MCC client type - Read MCC or Write MCC
Assumptions :
Limitations :
Returns     : FVDP_OK or FVDP_ERROR
*******************************************************************************/
static FVDP_Result_t MCC_HostUpdate(MCC_Modules_t MccModule, MCCClientType_t MccClientType)
{
    if (MccModule == MCC_FE)
    {
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_MCC_IP, SYNC_UPDATE);
    }
    else if (MccModule == MCC_BE)
    {
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_MCC_OP, SYNC_UPDATE);
    }
    else if (MccModule == MCC_AUX)
    {
        if (MccClientType == MCC_WRITE)
        {
            HostUpdate_SetUpdate_LITE(FVDP_AUX, LITE_HOST_UPDATE_MCC_IP, SYNC_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(FVDP_AUX, LITE_HOST_UPDATE_MCC_OP, SYNC_UPDATE);
        }
    }
    else if (MccModule == MCC_PIP)
    {
        if (MccClientType == MCC_WRITE)
        {
            HostUpdate_SetUpdate_LITE(FVDP_PIP, LITE_HOST_UPDATE_MCC_IP, SYNC_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(FVDP_PIP, LITE_HOST_UPDATE_MCC_OP, SYNC_UPDATE);
        }
    }
    else if (MccModule == MCC_ENC)
    {
        if (MccClientType == MCC_WRITE)
        {
            HostUpdate_SetUpdate_LITE(FVDP_ENC, LITE_HOST_UPDATE_MCC_IP, FORCE_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(FVDP_ENC, LITE_HOST_UPDATE_MCC_OP, FORCE_UPDATE);
        }
    }
    else
    {
        return FVDP_ERROR;
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : MCC_PrintConfiguration(FVDP_CH_t CH)
Description : Printout MCC cofiguration for selected channel
Parameters  : CH - channel ID

Assumptions :
Limitations :
Returns     : FVDP_OK or FVDP_ERROR
*******************************************************************************/
FVDP_Result_t MCC_PrintConfiguration(FVDP_CH_t CH)
{

    MCC_Client_t client_id;
    MCC_Client_t last_client;
    TRC( TRC_ID_MAIN_INFO, "-=-=-=-=-=-=- MCC_PrintConfiguration -=-=-=-=-=-=-" );
    TRC( TRC_ID_MAIN_INFO, "\nClient\t\tON/OFF\tBASE_ADDR\tEND_ADDR\tBPP\tH_SIZE\tV_SIZE" );

    client_id =     (CH == FVDP_MAIN) ? CCS_WR        :
                    (CH == FVDP_AUX ) ? AUX_RGB_UV_WR :
                    (CH == FVDP_PIP ) ? PIP_RGB_UV_WR :
                    (CH == FVDP_ENC ) ? ENC_RGB_UV_WR :
                    MCC_NUM_OF_CLIENTS;

    last_client =   (CH == FVDP_MAIN) ? OP_UV_WR   :
                    (CH == FVDP_AUX ) ? AUX_Y_RD      :
                    (CH == FVDP_PIP ) ? PIP_Y_RD      :
                    (CH == FVDP_ENC ) ? ENC_TNR_UV_RD :
                    MCC_NUM_OF_CLIENTS;

    if(client_id == MCC_NUM_OF_CLIENTS)
        return FVDP_ERROR;


    for(;client_id <= last_client; client_id++)
    {
        uint32_t regval;
        uint32_t baseaddr,bpp;
        MCC_ClientRegs_t client = MCC_ClientsTbl[client_id];
        MCC_Modules_t mcc_module =  client.mcc_module;
        bool readclient = (bool)(client.type == MCC_READ);
        uint32_t stride = MCC_REG32_Read(mcc_module,client.addr + (readclient ? RD_STRIDE_OFFS : WR_STRIDE_OFFS));
        uint32_t versize = (MCC_REG32_Read(mcc_module,client.addr)& VER_SIZE_MASK) >> VER_SIZE_OFFSET;

        /*print client name*/
        TRC( TRC_ID_MAIN_INFO, "%s\t", MCC_ClientName_tbl[client_id] );

        /*print out client state - ON or OFF*/
        if(MCC_REG32_Read(mcc_module,client.addr) & CLIENT_EN_MASK)
            TRC( TRC_ID_MAIN_INFO, "ON\t" );
        else
        {
            TRC( TRC_ID_MAIN_INFO, "OFF" );
            /* client is OFF, got to the next one*/
            continue;
        }

        /*printout the base address*/
        baseaddr = MCC_REG32_Read(mcc_module,client.addr + (readclient ? RD_ADDR_OFFS : WR_ADDR_OFFS));
        TRC( TRC_ID_MAIN_INFO, "0x%08X\t", baseaddr );

        /*calculate and printout the end address*/
        /* End_address = base_address + verical_size * stride * 8 - 1 */
        regval = baseaddr + versize*stride*SIZE_OF_MCC_WORD-1;
        TRC( TRC_ID_MAIN_INFO, "0x%08X\t", regval );

        /*print out bpp value*/
        if(client.PixSizeMask == 0)
            bpp = 4;
        else
        {
            bool is_pix_size_bit_set = ((MCC_REG32_Read(mcc_module, client.addr) & client.PixSizeMask)!=0);
            bpp = is_pix_size_bit_set ? 10 : 8;
        }

        TRC( TRC_ID_MAIN_INFO, "%2d\t", bpp );

        /*print Horizontal size*/
        regval  = MCC_REG32_Read(mcc_module,client.addr + (readclient ? RD_ADDR_CTRL_OFFS : WR_ADDR_CTRL_OFFS));
        {
            uint32_t endoffs;
            uint32_t startoffs;
            uint32_t startaddr  = regval & START_ADDR_MASK;
            uint32_t endaddr = (regval & END_ADDR_MASK) >> END_ADDR_OFFSET;
            if(readclient)
            {
                regval    = MCC_REG32_Read(mcc_module,client.addr + RD_OFFSET_OFFS);
                startoffs = regval & START_OFFSET_MASK;
                endoffs   = (regval & END_OFFSET_MASK) >> END_OFFSET_OFFSET;
            }
            else
            {
                startoffs = 0;
                endoffs   = 64;
            }
            TRC( TRC_ID_MAIN_INFO, "%4d\t", ((endaddr - startaddr)*64 + endoffs - startoffs)/bpp );
        }
        /*print Vertical size*/
        TRC( TRC_ID_MAIN_INFO, "%4d", versize );
    }
    return FVDP_OK;
}

