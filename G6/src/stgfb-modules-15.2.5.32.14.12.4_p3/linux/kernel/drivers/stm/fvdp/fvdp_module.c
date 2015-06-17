/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/fvdp/fvdp_module.c
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <vibe_os.h>

#include "fvdp_module_debug.h"
#include "fvdp_module.h"

#include <fvdp/fvdp.h>

#if FVDP_IOCTL

#if  defined(CONFIG_MPE42)

#include <fvdp/mpe42/fvdp_test.h>
#include <fvdp/mpe42/fvdp_debug.h>

#endif  // CONFIG_MPEXX

#include "fvdp_ioctl.h"

#define FVDP_NAME "fvdp" /* Name of character device */

#endif  // FVDP_IOCTL

/******************************************************************************
 *  Exported symbols
 */

EXPORT_SYMBOL(FVDP_InitDriver);
EXPORT_SYMBOL(FVDP_GetRequiredVideoMemorySize);
EXPORT_SYMBOL(FVDP_OpenChannel);
EXPORT_SYMBOL(FVDP_CloseChannel);
EXPORT_SYMBOL(FVDP_ConnectInput);
EXPORT_SYMBOL(FVDP_SetInputVideoInfo);
EXPORT_SYMBOL(FVDP_GetInputVideoInfo);
EXPORT_SYMBOL(FVDP_SetInputRasterInfo);
EXPORT_SYMBOL(FVDP_GetInputRasterInfo);
EXPORT_SYMBOL(FVDP_SetOutputVideoInfo);
EXPORT_SYMBOL(FVDP_GetOutputVideoInfo);
EXPORT_SYMBOL(FVDP_CommitUpdate);
EXPORT_SYMBOL(FVDP_InputUpdate);
EXPORT_SYMBOL(FVDP_OutputUpdate);
EXPORT_SYMBOL(FVDP_GetVersion);
EXPORT_SYMBOL(FVDP_SetPSIState);
EXPORT_SYMBOL(FVDP_GetPSIState);
EXPORT_SYMBOL(FVDP_SetPSITuningData);
EXPORT_SYMBOL(FVDP_GetPSITuningData);
EXPORT_SYMBOL(FVDP_SetCropWindow);
EXPORT_SYMBOL(FVDP_GetCropWindow);
EXPORT_SYMBOL(FVDP_SetPSIControl);
EXPORT_SYMBOL(FVDP_GetPSIControl);
EXPORT_SYMBOL(FVDP_GetPSIControlsRange);
EXPORT_SYMBOL(FVDP_QueueFlush);
EXPORT_SYMBOL(FVDP_SetCrcState);
EXPORT_SYMBOL(FVDP_GetCrcInfo);
EXPORT_SYMBOL(FVDP_SetOutputRasterInfo);
EXPORT_SYMBOL(FVDP_SetOutputWindow);
EXPORT_SYMBOL(FVDP_GetLatency);
EXPORT_SYMBOL(FVDP_SetLatencyType);
EXPORT_SYMBOL(FVDP_GetLatencyType);
EXPORT_SYMBOL(FVDP_ColorFill);
EXPORT_SYMBOL(FVDP_SetVideoProcType);

#if (ENC_HW_REV != REV_NONE)
EXPORT_SYMBOL(FVDP_SetBufferCallback);
EXPORT_SYMBOL(FVDP_QueueBuffer);
EXPORT_SYMBOL(FVDP_IntEndOfScaling);
#endif


/******************************************************************************
 *  Modularization
 */

#if FVDP_IOCTL

struct fvdp_device {
    struct cdev cdev;
};

static dev_t dev; /* contains the device number */
static struct class* fvdp_class; /* device class */
static struct fvdp_device fvdp_device; /* contains the character device */

static long fvdp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    /* Error checking */
    int result = 0;
    struct fvdp_device *dev = NULL;

    if (_IOC_TYPE(cmd) != FVDPIO_MAGIC) return -EINVAL;

    if (_IOC_DIR(cmd) & _IOC_READ)
        result = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        result =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

    if (result)
        return -EFAULT;

    dev = (struct fvdp_device*)f->private_data;

    /* IOCTL Handler */
    switch(cmd)
    {
        /* MISC IOCTLs */
        case FVDPIO_READ_REG:
        {
            FVDPIO_Reg_t reg;
            void *p_reg = &reg; //to remove coverity tainted data warning

            if(copy_from_user(p_reg, (FVDPIO_Reg_t*)arg, sizeof(FVDPIO_Reg_t)))
                return -EFAULT;

            FVDP_ReadRegister(reg.address, &reg.value);

            if(copy_to_user((FVDPIO_Reg_t*)arg, &reg, sizeof(FVDPIO_Reg_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_WRITE_REG:
        {
            FVDPIO_Reg_t reg;
            void *p_reg = &reg; //to remove coverity tainted data warning

            if(copy_from_user(p_reg, (FVDPIO_Reg_t*)arg, sizeof(FVDPIO_Reg_t)))
                return -EFAULT;

            FVDP_WriteRegister(reg.address, reg.value);
        }
        break;

        case FVDPIO_GET_DATA_INFO:
        {
            uint8_t log_source;
            FVDPIO_Data_t container;

            if (copy_from_user(&container, (FVDPIO_Data_t*)arg, sizeof(FVDPIO_Data_t)))
                return -EFAULT;

            switch (container.type)
            {
                case FVDPIO_TYPE_VCD_LOG:
                    log_source = container.param0 & 0x0F;
                    FVDP_VCDLog_GetInfo(log_source, &container.kaddr, &container.size);
                    break;

                default:
                    result = -ENOTTY;
                    break;
            }

            // 0 if valid container.type case
            if (result == 0)
            {
                if(copy_to_user((FVDPIO_Data_t*)arg, &container, sizeof(FVDPIO_Data_t)))
                    return -EFAULT;
            }
        }
        break;

        case FVDPIO_COPY_DATA_TO_USER:
        {
            FVDPIO_Data_t   container;
            void *p_container = &container; //to remove coverity tainted data warning

            if (copy_from_user(p_container, (FVDPIO_Data_t*)arg, sizeof(FVDPIO_Data_t)))
                return -EFAULT;

            if (result == 0 && container.uaddr && container.kaddr && container.size)
            {
                if (copy_to_user(container.uaddr, container.kaddr, container.size))
                    return -EFAULT;
            }
        }
        break;

        /* FVDP API IOCTLs */
        case FVDPIO_INIT_DRIVER:
        {
            FVDPIO_InitDriver_t Param;

            if(copy_from_user(&Param, (FVDPIO_InitDriver_t*)arg, sizeof(FVDPIO_InitDriver_t)))
                return -EFAULT;

            Param.Result = FVDP_InitDriver(Param.InitParams);

            if(copy_to_user((FVDPIO_InitDriver_t*)arg, &Param, sizeof(FVDPIO_InitDriver_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_REQ_VID_MEM_SIZE:
        {
            FVDPIO_GetRequiredVideoMemorySize_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetRequiredVideoMemorySize_t *)arg, sizeof(FVDPIO_GetRequiredVideoMemorySize_t )))
                return -EFAULT;

            Param.Result = FVDP_GetRequiredVideoMemorySize(Param.CH, Param.Context, &Param.MemSize);

            if(copy_to_user((FVDPIO_GetRequiredVideoMemorySize_t*)arg, &Param, sizeof(FVDPIO_GetRequiredVideoMemorySize_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_OPEN_CHANNEL:
        {
            FVDPIO_OpenChannel_t Param;

            if(copy_from_user(&Param, (FVDPIO_OpenChannel_t*)arg, sizeof(FVDPIO_OpenChannel_t)))
                return -EFAULT;

            Param.Result = FVDP_OpenChannel(&Param.FvdpChHandle, Param.CH, Param.MemBaseAddress, Param.MemSize, Param.Context);

            if(copy_to_user((FVDPIO_OpenChannel_t*)arg, &Param, sizeof(FVDPIO_OpenChannel_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_CLOSE_CHANNEL:
        {
            FVDPIO_CloseChannel_t Param;

            if(copy_from_user(&Param, (FVDPIO_CloseChannel_t*)arg, sizeof(FVDPIO_CloseChannel_t)))
                return -EFAULT;

            Param.Result = FVDP_CloseChannel(Param.FvdpChHandle);

            if(copy_to_user((FVDPIO_CloseChannel_t*)arg, &Param, sizeof(FVDPIO_CloseChannel_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_CONNECT_INPUT:
        {
            FVDPIO_ConnectInput_t Param;

            if(copy_from_user(&Param, (FVDPIO_ConnectInput_t *)arg, sizeof(FVDPIO_ConnectInput_t )))
                return -EFAULT;

            Param.Result = FVDP_ConnectInput(Param.FvdpChHandle, Param.Input);

            if(copy_to_user((FVDPIO_ConnectInput_t*)arg, &Param, sizeof(FVDPIO_ConnectInput_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_DISCONNECT_INPUT:
        {
            FVDPIO_DisconnectInput_t Param;

            if(copy_from_user(&Param, (FVDPIO_DisconnectInput_t*)arg, sizeof(FVDPIO_DisconnectInput_t)))
                return -EFAULT;

            Param.Result = FVDP_DisconnectInput(Param.FvdpChHandle);

            if(copy_to_user((FVDPIO_DisconnectInput_t*)arg, &Param, sizeof(FVDPIO_DisconnectInput_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_INPUT_VIDEO_INFO:
        {
            FVDPIO_SetInputVideoInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetInputVideoInfo_t*)arg, sizeof(FVDPIO_SetInputVideoInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_SetInputVideoInfo(Param.FvdpChHandle, Param.InputVideoInfo);

            if(copy_to_user((FVDPIO_SetInputVideoInfo_t*)arg, &Param, sizeof(FVDPIO_SetInputVideoInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_INPUT_VIDEO_INFO:
        {
            FVDPIO_GetInputVideoInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetInputVideoInfo_t*)arg, sizeof(FVDPIO_GetInputVideoInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_GetInputVideoInfo(Param.FvdpChHandle, &Param.InputVideoInfo);

            if(copy_to_user((FVDPIO_GetInputVideoInfo_t*)arg, &Param, sizeof(FVDPIO_GetInputVideoInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_OUTPUT_VIDEO_INFO:
        {
            FVDPIO_SetOutputVideoInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetOutputVideoInfo_t*)arg, sizeof(FVDPIO_SetOutputVideoInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_SetOutputVideoInfo(Param.FvdpChHandle, Param.OutputVideoInfo);

            if(copy_to_user((FVDPIO_SetOutputVideoInfo_t*)arg, &Param, sizeof(FVDPIO_SetOutputVideoInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_OUTPUT_VIDEO_INFO:
        {
            FVDPIO_GetOutputVideoInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetOutputVideoInfo_t*)arg, sizeof(FVDPIO_GetOutputVideoInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_GetOutputVideoInfo(Param.FvdpChHandle, &Param.OutputVideoInfo);

            if(copy_to_user((FVDPIO_GetOutputVideoInfo_t*)arg, &Param, sizeof(FVDPIO_GetOutputVideoInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_INPUT_RASTER_INFO:
        {
            FVDPIO_SetInputRasterInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetInputRasterInfo_t*)arg, sizeof(FVDPIO_SetInputRasterInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_SetInputRasterInfo(Param.FvdpChHandle, Param.InputRasterInfo);

            if(copy_to_user((FVDPIO_SetInputRasterInfo_t*)arg, &Param, sizeof(FVDPIO_SetInputRasterInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_INPUT_RASTER_INFO:
        {
            FVDPIO_GetInputRasterInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetInputRasterInfo_t*)arg, sizeof(FVDPIO_GetInputRasterInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_GetInputRasterInfo(Param.FvdpChHandle, &Param.InputRasterInfo);

            if(copy_to_user((FVDPIO_GetInputRasterInfo_t*)arg, &Param, sizeof(FVDPIO_GetInputRasterInfo_t)))
                return -EFAULT;
        }
        break;

        /*case FVDPIO_SET_STATE:
        {
            FVDPIO_SetState_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetState_t*)arg, sizeof(FVDPIO_SetState_t)))
                return -EFAULT;

            Param.Result = FVDP_SetState(Param.FvdpChHandle, Param.State);

            if(copy_to_user((FVDPIO_SetState_t*)arg, &Param, sizeof(FVDPIO_SetState_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_STATE:
        {
            FVDPIO_GetState_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetState_t*)arg, sizeof(FVDPIO_GetState_t)))
                return -EFAULT;

            Param.Result = FVDP_GetState(Param.FvdpChHandle, &Param.State);

            if(copy_to_user((FVDPIO_GetState_t*)arg, &Param, sizeof(FVDPIO_GetState_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_LATENCY:
        {
            FVDPIO_SetLatency_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetLatency_t*)arg, sizeof(FVDPIO_SetLatency_t)))
                return -EFAULT;

            Param.Result = FVDP_SetLatency(Param.FvdpChHandle, Param.FrameLatency, Param.LineLatency);

            if(copy_to_user((FVDPIO_SetLatency_t*)arg, &Param, sizeof(FVDPIO_SetLatency_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_LATENCY:
        {
            FVDPIO_GetLatency_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetLatency_t*)arg, sizeof(FVDPIO_GetLatency_t)))
                return -EFAULT;

            Param.Result = FVDP_GetLatency(Param.FvdpChHandle, &Param.FrameLatency, &Param.LineLatency);

            if(copy_to_user((FVDPIO_GetLatency_t*)arg, &Param, sizeof(FVDPIO_GetLatency_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_BW_OPTIONS:
        {
            FVDPIO_SetBWOptions_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetBWOptions_t*)arg, sizeof(FVDPIO_SetBWOptions_t)))
                return -EFAULT;

            Param.Result = FVDP_SetBWOptions(Param.FvdpChHandle, Param.BWOptions);

            if(copy_to_user((FVDPIO_SetBWOptions_t*)arg, &Param, sizeof(FVDPIO_SetBWOptions_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_BW_OPTIONS:
        {
            FVDPIO_GetBWOptions_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetBWOptions_t*)arg, sizeof(FVDPIO_GetBWOptions_t)))
                return -EFAULT;

            Param.Result = FVDP_GetBWOptions(Param.FvdpChHandle, &Param.BWOptions);

            if(copy_to_user((FVDPIO_GetBWOptions_t*)arg, &Param, sizeof(FVDPIO_GetBWOptions_t)))
                return -EFAULT;
        }
        break;*/

        case FVDPIO_SET_LATENCY:
        {
            FVDPIO_SetLatencyType_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetLatencyType_t*)arg, sizeof(FVDPIO_SetLatencyType_t)))
                return -EFAULT;

            Param.Result = FVDP_SetLatencyType(Param.FvdpChHandle, Param.LatencyType);

            if(copy_to_user((FVDPIO_SetLatencyType_t*)arg, &Param, sizeof(FVDPIO_SetLatencyType_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_LATENCY:
        {
            FVDPIO_GetLatencyType_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetLatencyType_t*)arg, sizeof(FVDPIO_GetLatencyType_t)))
                return -EFAULT;

            Param.Result = FVDP_GetLatencyType(Param.FvdpChHandle, &Param.LatencyType);

            if(copy_to_user((FVDPIO_GetLatencyType_t*)arg, &Param, sizeof(FVDPIO_GetLatencyType_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_PROCESSING:
        {
            FVDPIO_SetProcessing_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetProcessing_t*)arg, sizeof(FVDPIO_SetProcessing_t)))
                return -EFAULT;

            Param.Result = FVDP_SetVideoProcType(Param.FvdpChHandle, Param.ProcessingType);

            if(copy_to_user((FVDPIO_SetProcessing_t*)arg, &Param, sizeof(FVDPIO_SetProcessing_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_CROP_WINDOW:
        {
            FVDPIO_SetCropWindow_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetCropWindow_t*)arg, sizeof(FVDPIO_SetCropWindow_t)))
                return -EFAULT;

            Param.Result = FVDP_SetCropWindow(Param.FvdpChHandle, Param.CropWindow);

            if(copy_to_user((FVDPIO_SetCropWindow_t*)arg, &Param, sizeof(FVDPIO_SetCropWindow_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_CROP_WINDOW:
        {
            FVDPIO_GetCropWindow_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetCropWindow_t*)arg, sizeof(FVDPIO_GetCropWindow_t)))
                return -EFAULT;

            Param.Result = FVDP_GetCropWindow(Param.FvdpChHandle, &Param.CropWindow);

            if(copy_to_user((FVDPIO_GetCropWindow_t*)arg, &Param, sizeof(FVDPIO_GetCropWindow_t)))
                return -EFAULT;
        }
        break;

        /*case FVDPIO_SET_BLANKING_COLOR:
        {
            FVDPIO_SetBlankingColor_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetBlankingColor_t*)arg, sizeof(FVDPIO_SetBlankingColor_t)))
                return -EFAULT;

            Param.Result = FVDP_SetBlankingColor(Param.FvdpChHandle, Param.Red, Param.Green, Param.Blue);

            if(copy_to_user((FVDPIO_SetBlankingColor_t*)arg, &Param, sizeof(FVDPIO_SetBlankingColor_t)))
                return -EFAULT;
        }
        break;*/

        /*case FVDPIO_GET_BLANKING_COLOR:
        {
            FVDPIO_GetBlankingColor_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetBlankingColor_t*)arg, sizeof(FVDPIO_GetBlankingColor_t)))
                return -EFAULT;

            Param.Result = FVDP_GetBlankingColor(Param.FvdpChHandle, &Param.Red, &Param.Green, &Param.Blue);

            if(copy_to_user((FVDPIO_GetBlankingColor_t*)arg, &Param, sizeof(FVDPIO_GetBlankingColor_t)))
                return -EFAULT;
        }
        break;*/

        case FVDPIO_SET_PSI_CONTROL:
        {
            FVDPIO_SetPSIControl_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetPSIControl_t*)arg, sizeof(FVDPIO_SetPSIControl_t)))
                return -EFAULT;

            Param.Result = FVDP_SetPSIControl(Param.FvdpChHandle, Param.PSIControl, Param.Value);

            if(copy_to_user((FVDPIO_SetPSIControl_t*)arg, &Param, sizeof(FVDPIO_SetPSIControl_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_PSI_CONTROL:
        {
            FVDPIO_GetPSIControl_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetPSIControl_t*)arg, sizeof(FVDPIO_GetPSIControl_t)))
                return -EFAULT;

            Param.Result = FVDP_GetPSIControl(Param.FvdpChHandle, Param.PSIControl, &Param.Value);

            if(copy_to_user((FVDPIO_GetPSIControl_t*)arg, &Param, sizeof(FVDPIO_GetPSIControl_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_PSI_STATE:
        {
            FVDPIO_SetPSIState_t Param;
            void *p_Param = &Param; //to remove coverity tainted data warning

            if(copy_from_user(p_Param, (FVDPIO_SetPSIState_t*)arg, sizeof(FVDPIO_SetPSIState_t)))
                return -EFAULT;

            Param.Result = FVDP_SetPSIState(Param.FvdpChHandle, Param.PSIFeature, Param.PSIState);

            if(copy_to_user((FVDPIO_SetPSIState_t*)arg, &Param, sizeof(FVDPIO_SetPSIState_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_PSI_STATE:
        {
            FVDPIO_GetPSIState_t Param;
            void *p_Param = &Param; //to remove coverity tainted data warning

            if(copy_from_user(p_Param, (FVDPIO_GetPSIState_t*)arg, sizeof(FVDPIO_GetPSIState_t)))
                return -EFAULT;

            Param.Result = FVDP_GetPSIState(Param.FvdpChHandle, Param.PSIFeature, &Param.PSIState);

            if(copy_to_user((FVDPIO_GetPSIState_t*)arg, &Param, sizeof(FVDPIO_GetPSIState_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_PSI_TUNING_DATA:
        {
            FVDPIO_SetPSITuningData_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetPSITuningData_t*)arg, sizeof(FVDPIO_SetPSITuningData_t)))
                return -EFAULT;

            Param.Result = FVDP_SetPSITuningData(Param.FvdpChHandle, Param.PSIFeature, &Param.PSITuningData);

            if(copy_to_user((FVDPIO_SetPSITuningData_t*)arg, &Param, sizeof(FVDPIO_SetPSITuningData_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_PSI_TUNING_DATA:
        {
            FVDPIO_GetPSITuningData_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetPSITuningData_t*)arg, sizeof(FVDPIO_GetPSITuningData_t)))
                return -EFAULT;

            Param.Result = FVDP_GetPSITuningData(Param.FvdpChHandle, Param.PSIFeature, &Param.PSITuningData);

            if(copy_to_user((FVDPIO_GetPSITuningData_t*)arg, &Param, sizeof(FVDPIO_GetPSITuningData_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_VERSION:
        {
            FVDPIO_GetVersion_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetVersion_t*)arg, sizeof(FVDPIO_GetVersion_t)))
                return -EFAULT;

            Param.Result = FVDP_GetVersion(&Param.Version);

            if(copy_to_user((FVDPIO_GetVersion_t*)arg, &Param, sizeof(FVDPIO_GetVersion_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_PSI_CONTROLS_RANGE:
        {
            FVDPIO_GetPSIControlsRange_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetPSIControlsRange_t*)arg, sizeof(FVDPIO_GetPSIControlsRange_t)))
                return -EFAULT;

            Param.Result = FVDP_GetPSIControlsRange(Param.FvdpChHandle, Param.PSIControlsSelect, \
                                    &Param.MinValue, &Param.MaxValue, &Param.DefaultValue, &Param.StepValue);

            if(copy_to_user((FVDPIO_GetPSIControlsRange_t*)arg, &Param, sizeof(FVDPIO_GetPSIControlsRange_t)))
                return -EFAULT;
        }
        break;

        /*case FVDPIO_SET_EXTERNAL_INFO:
        {
            FVDPIO_SetExternalInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetExternalInfo_t*)arg, sizeof(FVDPIO_SetExternalInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_SetExternalInfo(Param.FvdpChHandle, Param.ExternalInfo, Param.Value);

            if(copy_to_user((FVDPIO_SetExternalInfo_t*)arg, &Param, sizeof(FVDPIO_SetExternalInfo_t)))
                return -EFAULT;
        }
        break;*/

        /*case FVDPIO_GET_EXTERNAL_INFO:
        {
            FVDPIO_GetExternalInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetExternalInfo_t*)arg, sizeof(FVDPIO_GetExternalInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_GetExternalInfo(Param.FvdpChHandle, Param.ExternalInfo, &Param.Value);

            if(copy_to_user((FVDPIO_GetExternalInfo_t*)arg, &Param, sizeof(FVDPIO_GetExternalInfo_t)))
                return -EFAULT;
        }
        break;*/

        case FVDPIO_INPUT_UPDATE:
        {
            FVDPIO_InputUpdate_t Param;

            if(copy_from_user(&Param, (FVDPIO_InputUpdate_t*)arg, sizeof(FVDPIO_InputUpdate_t)))
                return -EFAULT;

            Param.Result = FVDP_InputUpdate(Param.FvdpChHandle);

            if(copy_to_user((FVDPIO_InputUpdate_t*)arg, &Param, sizeof(FVDPIO_InputUpdate_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_OUTPUT_UPDATE:
        {
            FVDPIO_OutputUpdate_t Param;

            if(copy_from_user(&Param, (FVDPIO_OutputUpdate_t*)arg, sizeof(FVDPIO_OutputUpdate_t)))
                return -EFAULT;

            Param.Result = FVDP_OutputUpdate(Param.FvdpChHandle);

            if(copy_to_user((FVDPIO_OutputUpdate_t*)arg, &Param, sizeof(FVDPIO_OutputUpdate_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_COMMIT_UPDATE:
        {
            FVDPIO_CommitUpdate_t Param;

            if(copy_from_user(&Param, (FVDPIO_CommitUpdate_t*)arg, sizeof(FVDPIO_CommitUpdate_t)))
                return -EFAULT;

            Param.Result = FVDP_CommitUpdate(Param.FvdpChHandle);

            if(copy_to_user((FVDPIO_CommitUpdate_t*)arg, &Param, sizeof(FVDPIO_CommitUpdate_t)))
                return -EFAULT;
        }
        break;

        /*case FVDPIO_SET_BUFFER_CALLBACK:
        {
            FVDPIO_SetBufferCallback_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetBufferCallback_t*)arg, sizeof(FVDPIO_SetBufferCallback_t)))
                return -EFAULT;

            Param.Result = FVDP_SetBufferCallback(Param.FvdpChHandle, (void*)Param.AddrScalingCompleted, (void*)Param.AddrBufferDone);

            if(copy_to_user((FVDPIO_SetBufferCallback_t*)arg, &Param, sizeof(FVDPIO_SetBufferCallback_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_BUFFER_CALLBACK:
        {
            FVDPIO_GetBufferCallback_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetBufferCallback_t*)arg, sizeof(FVDPIO_GetBufferCallback_t)))
                return -EFAULT;

            Param.Result = FVDP_GetBufferCallback(&Param.AddrScalingCompleted, &Param.AddrBufferDone);

            if(copy_to_user((FVDPIO_GetBufferCallback_t*)arg, &Param, sizeof(FVDPIO_GetBufferCallback_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_QUEUE_BUFFER:
        {
            FVDPIO_QueueBuffer_t Param;

            if(copy_from_user(&Param, (FVDPIO_QueueBuffer_t*)arg, sizeof(FVDPIO_QueueBuffer_t)))
                return -EFAULT;

            Param.Result = FVDP_QueueBuffer(Param.FvdpChHandle,
                               Param.FrameID,
                               Param.SrcAddr1,
                               Param.SrcAddr2,
                               Param.DstAddr1,
                               Param.DstAddr2);

            if(copy_to_user((FVDPIO_QueueBuffer_t*)arg, &Param, sizeof(FVDPIO_QueueBuffer_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_QUEUE_GET_STATUS:
        {
            FVDPIO_QueueGetStatus_t Param;

            if(copy_from_user(&Param, (FVDPIO_QueueGetStatus_t*)arg, sizeof(FVDPIO_QueueGetStatus_t)))
                return -EFAULT;

            Param.Result = FVDP_QueueGetStatus(Param.FvdpChHandle, &Param.QueueStatus);

            if(copy_to_user((FVDPIO_QueueGetStatus_t*)arg, &Param, sizeof(FVDPIO_QueueGetStatus_t)))
                return -EFAULT;
        }
        break;*/

        case FVDPIO_QUEUE_FLUSH:
        {
            FVDPIO_QueueFlush_t Param;

            if(copy_from_user(&Param, (FVDPIO_QueueFlush_t*)arg, sizeof(FVDPIO_QueueFlush_t)))
                return -EFAULT;

            Param.Result = FVDP_QueueFlush(Param.FvdpChHandle, Param.bFlushCurrentNode);

            if(copy_to_user((FVDPIO_QueueFlush_t*)arg, &Param, sizeof(FVDPIO_QueueFlush_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_CRC_STATE:
        {
            FVDPIO_SetCrcState_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetCrcState_t*)arg, sizeof(FVDPIO_SetCrcState_t)))
                return -EFAULT;

            Param.Result = FVDP_SetCrcState(Param.FvdpChHandle, Param.Feature, Param.State);

            if(copy_to_user((FVDPIO_SetCrcState_t*)arg, &Param, sizeof(FVDPIO_SetCrcState_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_GET_CRC_INFO:
        {
            FVDPIO_GetCrcInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_GetCrcInfo_t*)arg, sizeof(FVDPIO_GetCrcInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_GetCrcInfo(Param.FvdpChHandle, Param.Feature, &Param.Info);

            if(copy_to_user((FVDPIO_GetCrcInfo_t*)arg, &Param, sizeof(FVDPIO_GetCrcInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_SET_OUTPUT_RASTER_INFO:
        {
            FVDPIO_SetOutputRasterInfo_t Param;

            if(copy_from_user(&Param, (FVDPIO_SetOutputRasterInfo_t*)arg, sizeof(FVDPIO_SetOutputRasterInfo_t)))
                return -EFAULT;

            Param.Result = FVDP_SetOutputRasterInfo(Param.FvdpChHandle, Param.OutputRasterInfo);

            if(copy_to_user((FVDPIO_SetOutputRasterInfo_t*)arg, &Param, sizeof(FVDPIO_SetOutputRasterInfo_t)))
                return -EFAULT;
        }
        break;

        case FVDPIO_APPTEST:
        {
            FVDPIO_Apptest_t Param;

            if(copy_from_user(&Param, (FVDPIO_Apptest_t*) arg, sizeof(FVDPIO_Apptest_t)))
                return -EFAULT;

            switch(Param.action_id)
            {
            case APPTEST_VCDLOG_GET_CONFIG:
                {
                    vcdlog_config_t config;
                    void      * uaddr1   = (void*) Param.param[0];

                    FVDP_VCDLog_GetConfig(&config);

                    if (copy_to_user(uaddr1, &config, sizeof(vcdlog_config_t)) != 0)
                        return -EFAULT;
                    break;
                }

            case APPTEST_VCDLOG_SET_SOURCE:
                FVDP_VCDLog_SetSource(Param.param[0]);
                break;

            case APPTEST_VCDLOG_SET_MODE:
                FVDP_VCDLog_SetMode(Param.param[0], Param.param[1]);
                break;

            case APPTEST_REGLOG_GET_CONFIG:
                {
                    reglog_t  * local    = FVDP_RegLog_GetPtr();
                    void      * uaddr1   = (void*) Param.param[0];

                    if (!local)
                        return -EFAULT;

                    if (copy_to_user(uaddr1, local, sizeof(reglog_t)) != 0)
                        return -EFAULT;
                    break;
                }
            case APPTEST_REGLOG_SET_CONFIG:
                {
                    reglog_t  * reglog  = FVDP_RegLog_GetPtr();
                    uint8_t   * data    = reglog->data_p;
                    void      * uaddr   = (void*) Param.param[0];

                    if (copy_from_user(reglog, uaddr, sizeof(reglog_t)) != 0)
                        return -EFAULT;

                    reglog->data_p = data;
                    break;
                }
            case APPTEST_REGLOG_START_CAPTURE:
                if (!FVDP_RegLog_Start())
                    return -EFAULT;
                break;
            case APPTEST_REGLOG_STOP_CAPTURE:
                if (!FVDP_RegLog_Stop())
                    return -EFAULT;
                break;
            case APPTEST_REGLOG_GET_LOG_DATA:
                {
                    reglog_t  * local    = FVDP_RegLog_GetPtr();
                    uint32_t    log_size;
                    void      * uaddr1   = (void*) Param.param[0];
                    void      * uaddr2   = (void*) Param.param[1];

                    if (!local)
                        return -EFAULT;

                    log_size = local->row_size * local->rows_recorded;

                    if (copy_to_user(uaddr1, local, sizeof(reglog_t)) != 0)
                        return -EFAULT;

                    if (copy_to_user(uaddr2, local->data_p, log_size) != 0)
                        return -EFAULT;

                    break;
                }

            default:
                result = -ENOTTY;
                break;
            }
        }
        break;

        default:
            result = -ENOTTY;
            break;

    }

    return result;
}

static int fvdp_open(struct inode *inode, struct file *filp)
{
    struct fvdp_device *dev = container_of(inode->i_cdev, struct fvdp_device, cdev);
    filp->private_data = dev;
    return 0;
}

static struct file_operations fvdp_fops =
{
    .owner = THIS_MODULE,
    .open = fvdp_open,
    .unlocked_ioctl = fvdp_ioctl
};

/******************************************************************************
 *  Modularization
 */

#ifdef CONFIG_PM
/* With Power Management */
static int fvdp_set_power_state(struct platform_device *pdev, pm_message_t state)
{
  int retval = -ENODEV;

  switch(state.event) {
    case PM_EVENT_RESTORE: {
        FVDP_Result_t ErrorCode = FVDP_OK;

        stm_fvdp_printd(">%s: RESTORE event!\n", __func__);

        /* Claim sysconf registers */
        if(fvdp_claim_sysconf(pdev))
        {
          stm_fvdp_printe("Failed, fvdp_claim_sysconf\n");
          goto exit_callback;
        }

        /* Power Down FVDP chip */
        fvdp_hw_power_down(pdev);

        /* Power On FVDP chip */
        fvdp_hw_power_up(pdev);

        /* Initialize FVDP clocks */
        if(fvdp_clocks_initialize(pdev))
        {
            stm_fvdp_printe("Failed, fvdp_clocks_initialize\n");
            goto exit_callback;
        }

        /* Switch on FVDP clocks */
        if(fvdp_clocks_enable(pdev, true))
        {
            stm_fvdp_printe("Failed, to switch on FVDP clocks\n");
            goto exit_callback;
        }

        /* Bring up FVDP driver */
        if((ErrorCode = FVDP_PowerControl(FVDP_RESUME)) != FVDP_OK)
        {
          stm_fvdp_printe("Failed, to resume FVDP driver (err=%d)\n", ErrorCode);
          goto exit_callback;
        }

        /* Configure gpio pads */
        if(fvdp_configure_pads(pdev, true))
        {
          stm_fvdp_printe("Failed, to configure gpio pads\n");
          goto exit_callback;
        }

        retval = 0;

        break;
    }

    case PM_EVENT_RESUME: {
        stm_fvdp_printd(">%s: RESUME event!\n", __func__);

        /* Claim sysconf registers */
        if(fvdp_claim_sysconf(pdev))
        {
          stm_fvdp_printe("Failed, fvdp_claim_sysconf\n");
          goto exit_callback;
        }

        /* Power Down FVDP chip */
        fvdp_hw_power_down(pdev);

        /* Power On FVDP chip */
        fvdp_hw_power_up(pdev);

        /* Switch on FVDP clocks */
        if(fvdp_clocks_enable(pdev, true))
        {
          stm_fvdp_printe("Failed, to switch on FVDP clocks\n");
          goto exit_callback;
        }

        /* Configure gpio pads */
        if(fvdp_configure_pads(pdev, true))
        {
          stm_fvdp_printe("Failed, to configure gpio pads\n");
          goto exit_callback;
        }

        retval = 0;
        break;
    }

    case PM_EVENT_SUSPEND: {
        stm_fvdp_printd(">%s: SUSPEND event!\n", __func__);

        /* Unconfigure gpio pads */
        if(fvdp_configure_pads(pdev, false))
        {
          stm_fvdp_printe("Failed, to reset gpio pads\n");
          goto exit_callback;
        }

        /* Switch off FVDP clocks */
        if(fvdp_clocks_enable(pdev, false))
        {
          stm_fvdp_printe("Failed, to switch off FVDP clocks\n");
          goto exit_callback;
        }

        /* Power Down FVDP chip */
        fvdp_hw_power_down(pdev);

        /* Release sysconf registers */
        fvdp_release_sysconf(pdev);

        retval = 0;
        break;
    }

    case PM_EVENT_FREEZE: {
        FVDP_Result_t ErrorCode = FVDP_OK;

        stm_fvdp_printd(">%s: FREEZE event!\n", __func__);

        /* Bring down FVDP driver */
        if((ErrorCode = FVDP_PowerControl(FVDP_SUSPEND)) != FVDP_OK)
        {
          stm_fvdp_printe("Failed, to suspend FVDP driver (err=%d)\n", ErrorCode);
          goto exit_callback;
        }

        /* Unconfigure gpio pads */
        if(fvdp_configure_pads(pdev, false))
        {
          stm_fvdp_printe("Failed, to reset gpio pads\n");
          goto exit_callback;
        }

        /* Switch off FVDP clocks */
        if(fvdp_clocks_enable(pdev, false))
        {
          stm_fvdp_printe("Failed, to switch off FVDP clocks\n");
          goto exit_callback;
        }
        fvdp_clocks_uninitialize(pdev);

        /* Power Down FVDP chip */
        fvdp_hw_power_down(pdev);

        /* Release sysconf registers */
        fvdp_release_sysconf(pdev);

        retval = 0;
        break;
    }

    case PM_EVENT_ON:
    case PM_EVENT_THAW:
    default : {
        stm_fvdp_printe(">%s: Unsupported PM event!\n", __func__);
        retval = -EINVAL;
        break;
    }
  }

exit_callback:
  if(retval < 0)
    stm_fvdp_printe("Failed %s \n", __func__);
  return retval;
}

static int fvdp_driver_suspend(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);

    return fvdp_set_power_state(pdev, PMSG_SUSPEND);
}
static int fvdp_driver_resume(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);

    return fvdp_set_power_state(pdev, PMSG_RESUME);
}
static int fvdp_driver_freeze(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);

    return fvdp_set_power_state(pdev, PMSG_FREEZE);
}
static int fvdp_driver_restore(struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);

    return fvdp_set_power_state(pdev, PMSG_RESTORE);
}

const struct dev_pm_ops fvdp_pm_ops = {
  .suspend         = fvdp_driver_suspend,
  .resume          = fvdp_driver_resume,
  .freeze          = fvdp_driver_freeze,
  .thaw            = fvdp_driver_resume,
  .poweroff        = fvdp_driver_suspend,
  .restore         = fvdp_driver_restore,
  .runtime_suspend = fvdp_driver_suspend,
  .runtime_resume  = fvdp_driver_resume,
  .runtime_idle    = NULL,
};

#define FVDP_PM_OPS   (&fvdp_pm_ops)

#else

/* Without Power Management */
#define FVDP_PM_OPS   NULL

#endif

static struct of_device_id stm_fvdp_match[] = {
    {
        .compatible = "st,fvdp",
    },
    {},
};

static struct platform_driver fvdp_driver = {
        .driver = {
                .name   = "stm-fvdp",
                .owner  = THIS_MODULE,
                .pm     = FVDP_PM_OPS,
                .of_match_table = of_match_ptr(stm_fvdp_match),
        },
        .probe  = fvdp_driver_probe,
        .remove = __exit_p (fvdp_driver_remove),
};

static int __init fvdp_module_init(void)
{
    struct device_node *np = NULL;
    const void * status = NULL;
    struct device* class_device;
    int result = 0;
    stm_fvdp_printd(">%s\n", __func__);

    /* Check if we do have an fvdp DT node available */
    np = of_find_matching_node(NULL, stm_fvdp_match);
    if (np) {
        /*
         * Check the "status" property of the DT node :
         *
         * If the "status" property is present and equal to "okay" then this
         * means that the DT framework did registred an FVDP platform device for
         * us.
         */
        status = of_get_property(np, "status", NULL);
        if(status)
        {
           /*
           * If the "status" property is present and is disabled then this means
           * that the DT framework didn't loaded the FVDP device for us so we
           * have to register our platform device (Non DT support).
           */
            if (strcmp(status, "okay")!=0)
            {
                /* Invalidate the status pointer */
                status = NULL;
            }
        }
    }

    /*
     * If the node is not present or the status is invalid then we should unload
     * our platform device before exiting.
     */
    if (!np || !status)
    {
        if(fvdp_platform_device_register())
            return -ENODEV;
    }

    if(platform_driver_register(&fvdp_driver))
    {
        printk(KERN_ERR "%s: Unable to register fvdp platform driver", fvdp_driver_name);
        if (!np || !status)
        {
            fvdp_platform_device_unregister();
        }
        return -ENODEV;
    }

    stm_fvdp_printd("<%s\n", __func__);

    /* Registers 1 device file with the name FVDP_NAME. Free major number is found dynamically. */
    result = alloc_chrdev_region(&dev, 0, 1, FVDP_NAME);
    if(result)
    {
      printk(KERN_ERR "%s: unable to allocate device numbers\n",__FUNCTION__);
      return result;
    }

    /* Class and device create allow the udev Linux deamon to automatically pick up information and create device file. */
    fvdp_class = class_create(THIS_MODULE, FVDP_NAME);
    if(IS_ERR(fvdp_class))
    {
        result = PTR_ERR(fvdp_class);
        printk(KERN_ERR "%s: unable to create device class, errno = %d\n",__FUNCTION__, result);
        unregister_chrdev_region(dev, 1);
        return result;
    }

    class_device = device_create(fvdp_class, NULL, dev, NULL, FVDP_NAME);
    if(IS_ERR(class_device))
    {
        result = PTR_ERR(class_device);
        printk(KERN_ERR "%s: unable to populate device numbers for device class, errno = %d\n",__FUNCTION__, result);
        class_destroy(fvdp_class);
        unregister_chrdev_region(dev, 1);
        return result;
    }

    /* Cdev init and add are used to hand the file operations structure to the virtual file system which deals with the device file. */
    cdev_init(&fvdp_device.cdev, &fvdp_fops);
    result = cdev_add(&fvdp_device.cdev, dev, 1);
    if (result)
    {
        printk(KERN_ERR "%s: unable to add character device\n",__FUNCTION__);
        device_destroy(fvdp_class, dev);
        class_destroy(fvdp_class);
        unregister_chrdev_region(dev, 1);
    }

    return result;
}

static void __exit fvdp_module_exit(void)
{
    struct device_node *np = NULL;
    const void * status = NULL;

    cdev_del(&fvdp_device.cdev);
    device_destroy(fvdp_class, dev);
    class_destroy(fvdp_class);
    unregister_chrdev_region(dev, 1);
    stm_fvdp_printd(">%s\n", __func__);

    platform_driver_unregister(&fvdp_driver);

    /* Check if we do have an fvdp DT node available */
    np = of_find_matching_node(NULL, stm_fvdp_match);
    if (np) {
        /*
         * Check the "status" property of the DT node :
         *
         * If the "status" property is present and equal to "okay" then this
         * means that the DT framework did registred an FVDP platform device for
         * us.
         */
        status = of_get_property(np, "status", NULL);
        if(status)
        {
           /*
           * If the "status" property is present and is disabled then this means
           * that the DT framework didn't loaded the FVDP device for us and we
           * were registering our platform device (Non DT support) which we
           * should unload before exiting.
           */
            if (strcmp(status, "okay")!=0)
            {
                /* Invalidate the status pointer */
                status = NULL;
            }
        }
    }

    /*
     * If the node is not present or the status is invalid then we should unload
     * our platform device before exiting.
     */
    if (!np || !status)
    {
        fvdp_platform_device_unregister();
    }

    stm_fvdp_printd("<%s\n", __func__);
}
#endif // FVDP_IOCTL

module_init(fvdp_module_init);
module_exit(fvdp_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("The fvdp driver");
MODULE_VERSION(KBUILD_VERSION);

