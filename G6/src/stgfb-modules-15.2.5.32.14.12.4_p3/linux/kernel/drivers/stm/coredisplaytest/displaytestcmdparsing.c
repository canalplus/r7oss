/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestcmdparsing.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : displaytestcmdparsing.c
 * Description      : handle commands which are sent to the test module
 *                    by the user space
 *
************************************************************************/
#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/fs.h>

#include <linux/string.h>
#include <linux/errno.h>

#include <asm/uaccess.h>
#include <asm/irq.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include <st_relay.h>

#include <circularbuffer.h>
#include <stmcoredisplaytest.h>
#include <displaytestdbgfs.h>
#include <displaytestcmdparsing.h>
#include <displaytestresultshandling.h>

#include <vibe_debug.h>

/* to store test commands */
#define MAX_LENGTH_CMD    30
#define MAX_NUM_CMD       50
#define KEY_CMD_POINTER    0
#define MAX_NUM_MISR_CONFIG_CMDS   5

typedef uint8_t cmdString_t[MAX_LENGTH_CMD];

static cmdString_t DisplayTestCmdsStr[MAX_NUM_CMD]; /* commands received from user space */
static uint32_t    nbrOfCmds = 0;                   /* Number of commands in DisplayTestCmdsStr[] */

extern struct stmcore_displaytest *pDisplayTest;

static void reset_pipeline_services(struct ovpDisplayTest *pOvpTest)
{
    TRC(TRC_ID_COREDISPLAYTEST, "pOvpTest = %p\n",pOvpTest);

    pOvpTest->infoReq.isVsync = false;
    pOvpTest->infoReq.isPicID = false;
    pOvpTest->infoReq.isPicType = false;
    pOvpTest->infoReq.isPTS = false;
    pOvpTest->infoReq.isVTGEvt = false;

    memset((void*)pOvpTest->crcData.requestedCrcStr, 0, sizeof(pOvpTest->crcData.requestedCrcStr));
    pOvpTest->crcData.isCrcRequested = false;

    memset((void*)&pOvpTest->misrData.requestedMisrStr, 0, sizeof(pOvpTest->misrData.requestedMisrStr));
    pOvpTest->misrData.isMisrRequested = false;
    pOvpTest->misrData.nbrOfRequestedMisr = 0;
}


static void reset_source_services(struct ivpDisplayTest *pIvpTest)
{
    TRC(TRC_ID_COREDISPLAYTEST, "pIvpTest = %p", pIvpTest);

    pIvpTest->infoReq.isVsync = false;
    pIvpTest->infoReq.isPicID = false;
    pIvpTest->infoReq.isPicType = false;
    pIvpTest->infoReq.isPTS = false;

    memset((void*)pIvpTest->srcCrcData.requestedCrcStr, 0, sizeof(pIvpTest->srcCrcData.requestedCrcStr));
    pIvpTest->srcCrcData.isCrcRequested = false;

    memset((void*)pIvpTest->planeCrcData.requestedCrcStr, 0, sizeof(pIvpTest->planeCrcData.requestedCrcStr));
    pIvpTest->planeCrcData.isCrcRequested = false;
}

static bool search_keyword_in_string_array(cmdString_t pCmd, uint8_t *stringArrayPtr, uint32_t nbrOfElements, uint32_t *storeIndex)
{
    uint32_t  i;
    uint32_t  offset = 0;
    uint8_t   strPtr[MAX_CAPABILITY_STRING_LENGTH];

    TRC(TRC_ID_COREDISPLAYTEST, "INFO: pCmd = %s, nbrOfElementsInArray = %d", pCmd, nbrOfElements);
    for(i = 0; i < nbrOfElements; i++)
    {
        offset = i*MAX_CAPABILITY_STRING_LENGTH;
        strncpy(strPtr, (stringArrayPtr + offset),MAX_CAPABILITY_STRING_LENGTH);
        strPtr[MAX_CAPABILITY_STRING_LENGTH - 1] = 0;
        if(strncmp(pCmd, strPtr, strlen(pCmd)) == 0)
        {
            if(storeIndex != NULL)
            {
                *storeIndex = i;
            }
            return true;
        }
    }
    TRC(TRC_ID_COREDISPLAYTEST, "%s is not in string array!", pCmd);
    return false;
}

static bool set_misr_control_value( struct ovpDisplayTest *pOvpTest, uint32_t storeIndex, MisrControl_t controlParams)
{
    SetTuningInputData_t misrInputList;
    int32_t setTuningRes = 0;

    /* to set MISR Control value */
    memcpy((void*)&misrInputList.MisrCtrlVal, (void*)&controlParams, sizeof(MisrControl_t));

    misrInputList.MisrStoreIndex = storeIndex;
    strncpy(misrInputList.ServiceStr, &pOvpTest->misrData.requestedMisrStr[storeIndex][0], MAX_CAPABILITY_STRING_LENGTH);
    misrInputList.ServiceStr[MAX_CAPABILITY_STRING_LENGTH - 1] = 0;
    misrInputList.OutputId = pOvpTest->main_output_id;


    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "%d : MISR_SET_CONTROL", pOvpTest->main_output_id);
    setTuningRes = stm_display_device_set_tuning(pOvpTest->device,
                                        MISR_SET_CONTROL,
                                        (void *)&misrInputList,
                                        sizeof(SetTuningInputData_t),
                                        NULL,
                                        0);

    if(setTuningRes < 0)
    {
        switch(setTuningRes)
        {
            case -EINTR: /*The call was interrupted while obtaining the device lock.*/
            {
                TRC(TRC_ID_ERROR, "The call was interrupted while obtaining the device lock!!!");
                break;
            }
            case -EINVAL:/*The device handle was invalid.*/
            {
                TRC(TRC_ID_ERROR, "The device handle was invalid!!!");
                break;
            }
            case -EFAULT: /*  The source output parameter pointer is an invalid address.*/
            {
                TRC(TRC_ID_ERROR, "The source output parameter pointer is an invalid address!!!");
                break;
            }
            case -EOPNOTSUPP: /* The service is not supported.*/
            {
                TRC(TRC_ID_ERROR, "The service is not supported!!!");
                break;
            }
            case -ERANGE: /*The tuning parameters are not valid.*/
            {
                TRC(TRC_ID_ERROR, "The tuning parameters are not valid!!!");
                break;
            }
            case -EAGAIN: /*The tuning parameters are not available.*/
            {
                TRC(TRC_ID_ERROR, "The tuning parameters are not available please try later!!!");
                break;
            }
            default:
                break;
        }
        TRC(TRC_ID_ERROR, "Failed in setting misr contrl value!!!");
        return false;
    }
    else
    {
        return true;
    }
}

static void reset_selection_cmds(void)
{
    uint32_t pipelineNo, sourceNo;
    struct ovpDisplayTest *pOvpTest = NULL;
    struct ivpDisplayTest *pIvpTest = NULL;

    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];
        if(pOvpTest)
        {
           reset_pipeline_services(pOvpTest);
        }
    }

    for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
    {
        pIvpTest = pDisplayTest->pIvpDisplayTest[sourceNo];
        if(pIvpTest)
        {
            reset_source_services(pIvpTest);
        }
    }
}

static void parse_error_cmd(void)
{
    TRC(TRC_ID_ERROR, "unexpected cmd is received!");
    pDisplayTest->errCmdStatus = true;
}


static void process_partial_reset_driver_cmd(void)
{
    int plane;
    static stm_display_device_h hDisplay  = 0;
    stm_display_plane_h hPlane;
    int output;
    stm_display_output_h hOutput;
    stm_rational_t DisplayAspectRatio;
    int ret;
    const char *name;
    bool applicable=false;
    uint32_t output_caps;

    if(stm_display_open_device(0, &hDisplay)!=0)
    {
        TRC(TRC_ID_ERROR, "Failed to open display device");
        return;
    }

    // for all plane set INPUT_MODE_AUTO
    for(plane=0; plane<STMCORE_MAX_PLANES; plane++)
    {
        if(stm_display_device_open_plane(hDisplay,plane,&hPlane) == 0)
        {
            if(stm_display_plane_get_name(hPlane, &name) == 0)
                TRC(TRC_ID_COREDISPLAYTEST, "PLANE: %s",name);

            ret = stm_display_plane_is_feature_applicable(hPlane,PLANE_FEAT_WINDOW_MODE,&applicable);
            // "-EOPNOTSUPP" is returned when the feature is not applicable. This is not an error
            if ((ret != 0) && (ret != -EOPNOTSUPP))
            {
                TRC(TRC_ID_ERROR, "stm_display_plane_is_feature_applicable failed with error %d", ret);
            }

            if (applicable==true)
            {
                TRC(TRC_ID_COREDISPLAYTEST, "WINDOW MODE supported by this PLANE: %s",name);

                ret = stm_display_plane_set_control(hPlane,PLANE_CTRL_INPUT_WINDOW_MODE, AUTO_MODE);
                if (ret < 0)
                {
                    TRC(TRC_ID_ERROR,"Failed to reset the IW mode on PLANE: %s",name);
                }

                ret = stm_display_plane_set_control(hPlane,PLANE_CTRL_OUTPUT_WINDOW_MODE, AUTO_MODE);
                if (ret < 0)
                {
                    TRC(TRC_ID_ERROR,"Failed to reset the OW mode on PLANE: %s",name);
                }

                ret = stm_display_plane_set_control(hPlane,PLANE_CTRL_ASPECT_RATIO_CONVERSION_MODE, ASPECT_RATIO_CONV_LETTER_BOX);
                if (ret < 0)
                {
                    TRC(TRC_ID_ERROR,"Failed to reset the AR conversion mode on PLANE: %s",name);
                }
            }
            else
            {
                TRC(TRC_ID_COREDISPLAYTEST,"WINDOW MODE not supported by this PLANE: %s",name);
            }
        }
        else
        {
            // There is no more plane
            break;
        }
        stm_display_plane_close(hPlane);
    }
    // for all output set
    DisplayAspectRatio.numerator = 16;
    DisplayAspectRatio.denominator = 9;
    for(output=0;output<5;output++)
    {
        if(stm_display_device_open_output(hDisplay,output,&hOutput) == 0)
        {
            /* OUTPUT_CTRL_DISPLAY_ASPECT_RATIO can be called only on outputs owning a mixer */
            ret = stm_display_output_get_capabilities(hOutput, &output_caps);
            if(ret == 0)
            {
                if (output_caps & OUTPUT_CAPS_PLANE_MIXER)
                {
                    ret = stm_display_output_set_compound_control(hOutput,OUTPUT_CTRL_DISPLAY_ASPECT_RATIO,&DisplayAspectRatio);
                    if (ret < 0)
                    {
                        TRC(TRC_ID_ERROR, "Failed to reset the Display AR on Output %d", output);
                    }
                }
            }
            else
            {
                TRC(TRC_ID_ERROR, "Failed to get output caps (%d)", ret);
            }
        }
        else
        {
            // There is no more output
            break;
        }
        stm_display_output_close(hOutput);
    }
    stm_display_device_close(hDisplay);
}

static void process_start_cmd(void)
{
    uint8_t pipelineNo, sourceNo;
    struct ovpDisplayTest *pOvpTest = NULL;
    struct ivpDisplayTest *pIvpTest = NULL;
    MisrControl_t controlParams;

    if(pDisplayTest->errCmdStatus)
    {
        TRC(TRC_ID_ERROR, "The test can't be started because an unexpected command has been required!!!");
        return;
    }

    if(pDisplayTest->isTestStarted)
    {
        TRC(TRC_ID_ERROR, "Test already started!");
        return;
    }

    /* isTestStarted will be set to TRUE only if a valid service is requested by the application */

    /* Check CRCs/MISRs requested on each pipeline */
    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];
        if(pOvpTest)
        {
            if((pOvpTest->crcData.isCrcRequested) || (pOvpTest->misrData.isMisrRequested))
            {
                if(pOvpTest->misrData.isMisrRequested && (pOvpTest->misrData.nbrOfRequestedMisr==0))
                {
                    /* VTGEvt is only collected after MISR_CONTROL is called. Usually, MISR_CONTROL is called when MISR type(s) is required.
                     * Here, MISR_CONTROL is called to start VTGEvt collection when none of MISR types is required.
                     */
                    strncpy(&pOvpTest->misrData.requestedMisrStr[0][0], &pOvpTest->misrData.misrCapabilitiesStr[0][0], MAX_CAPABILITY_STRING_LENGTH);
                    TRC(TRC_ID_COREDISPLAYTEST, "pCmd = %s pOvpTest->misrData.requestedMisrStr[%d][0] = %s",
                             &pOvpTest->misrData.misrCapabilitiesStr[0][0],
                             pOvpTest->misrData.nbrOfRequestedMisr,
                             &pOvpTest->misrData.requestedMisrStr[pOvpTest->misrData.nbrOfRequestedMisr][0]);
                    memset((void*)&controlParams, 0, sizeof(controlParams));
                    if (set_misr_control_value(pOvpTest, pOvpTest->misrData.nbrOfRequestedMisr, controlParams))
                    {
                        pOvpTest->misrData.nbrOfRequestedMisr++;
                        TRC(TRC_ID_COREDISPLAYTEST, "pOvpTest = %p, pOvpTest->misrData.nbrOfRequestedMisr = %d", pOvpTest, pOvpTest->misrData.nbrOfRequestedMisr);
                    }
                }
                goto start_test;
            }
        }
    }

    /* Check CRCs requested on each Source */
    for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
    {
        pIvpTest = pDisplayTest->pIvpDisplayTest[sourceNo];
        if(pIvpTest)
        {
            if( (pIvpTest->srcCrcData.isCrcRequested) || (pIvpTest->planeCrcData.isCrcRequested) )
            {
                goto start_test;
            }
        }
    }

    TRC(TRC_ID_ERROR, "Start cmd issued while no CRC/MISR requested!");
    return;

start_test:
    pDisplayTest->isTestStarted = true;
    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "Test STARTED\n");
}

static void process_stop_cmd(void)
{
    uint32_t pipelineNo;
    SetTuningInputData_t inputList;
    struct ovpDisplayTest *pOvpTest = NULL;
    int res;

    pDisplayTest->isTestStarted = false;

    /* Send the STOP command to the driver */
    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];

        inputList.OutputId = pOvpTest->main_output_id;

        res = stm_display_device_set_tuning(pOvpTest->device,
                                      MISR_STOP,
                                      (void *)&inputList,
                                      sizeof(SetTuningInputData_t),
                                      NULL,
                                      0);
        if(res<0)
        {
            TRC(TRC_ID_ERROR, "MISR_STOP cmd failed with error %d", res);
        }
    }
    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "Test STOPPED!");
}

static void process_capability_cmd(void)
{
    if (pDisplayTest->isTestStarted)
    {
        TRC(TRC_ID_ERROR, "Can't get capabilities when an acquisition is on going!");
    }
    else
    {
        export_capability(pDisplayTest);
    }
    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "Return Test CAPABILITY");
}


static void process_clear_error_cmd(void)
{
    reset_selection_cmds();
    pDisplayTest->errCmdStatus = false;
}

static bool process_general_cmd(cmdString_t pCmd)
{
    struct ovpDisplayTest *pOvpTest = NULL;
    struct ivpDisplayTest *pIvpTest = NULL;
    uint32_t pipelineNo, sourceNo;
    uint8_t expectedKeyword[MAX_LENGTH_CMD];

    /* Check if the cmd match the hard coded keywords */
    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];
        if(strncmp(pCmd, pOvpTest->curPipelineName, strlen(pOvpTest->curPipelineName)) == 0)
        {
            snprintf(expectedKeyword,MAX_LENGTH_CMD,"%sPTS",pOvpTest->curPipelineName);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {

                pOvpTest->infoReq.isPTS = true;
                goto general_test;
            }

            snprintf(expectedKeyword,MAX_LENGTH_CMD,"%sVsync",pOvpTest->curPipelineName);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pOvpTest->infoReq.isVsync = true;
                goto general_test;
            }

            snprintf(expectedKeyword,MAX_LENGTH_CMD,"%sPictureID",pOvpTest->curPipelineName);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pOvpTest->infoReq.isPicID = true;
                goto general_test;
            }

            snprintf(expectedKeyword,MAX_LENGTH_CMD,"%sPictureType",pOvpTest->curPipelineName);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pOvpTest->infoReq.isPicType = true;
                goto general_test;
            }

            snprintf(expectedKeyword,MAX_LENGTH_CMD,"%sVTGEvent",pOvpTest->curPipelineName);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pOvpTest->infoReq.isVTGEvt = true;
                goto general_test;
            }
        }
    }

    for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
    {
        pIvpTest = pDisplayTest->pIvpDisplayTest[sourceNo];

        snprintf(expectedKeyword,MAX_LENGTH_CMD,"SOURCE%d",pIvpTest->curSourceNo);
        if(strncmp(pCmd, expectedKeyword, strlen(expectedKeyword)) == 0)
        {
            snprintf(expectedKeyword,MAX_LENGTH_CMD,"SOURCE%dPTS",pIvpTest->curSourceNo);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pIvpTest->infoReq.isPTS = true;
                goto general_test;
            }

            snprintf(expectedKeyword,MAX_LENGTH_CMD,"SOURCE%dVsync",pIvpTest->curSourceNo);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pIvpTest->infoReq.isVsync = true;
                goto general_test;
            }
            snprintf(expectedKeyword,MAX_LENGTH_CMD,"SOURCE%dPictureID",pIvpTest->curSourceNo);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pIvpTest->infoReq.isPicID = true;
                goto general_test;
            }

            snprintf(expectedKeyword,MAX_LENGTH_CMD,"SOURCE%dPictureType",pIvpTest->curSourceNo);
            if (strncmp(pCmd,expectedKeyword, strlen(expectedKeyword)) == 0)
            {
                pIvpTest->infoReq.isPicType = true;
                goto general_test;
            }
        }
    }
    /* This command is NOT a General Cmd */
    return false;

general_test:
    /* general information to be collected on each pipeline */
    if(pOvpTest != NULL)
    {
        if(pOvpTest->infoReq.isPTS|| pOvpTest->infoReq.isVsync||pOvpTest->infoReq.isPicID||pOvpTest->infoReq.isPicType)
        {
            pOvpTest->crcData.isCrcRequested = true;
        }

        if(pOvpTest->infoReq.isVTGEvt)
        {
            pOvpTest->misrData.isMisrRequested = true;
        }
    }

    /* general information to be collected on each source */
    if(pIvpTest != NULL)
    {
        if(pIvpTest->infoReq.isPTS|| pIvpTest->infoReq.isVsync||pIvpTest->infoReq.isPicID||pIvpTest->infoReq.isPicType)
        {
            pIvpTest->srcCrcData.isCrcRequested = true;
        }
    }
    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "GeneralCmd = %s\n", pCmd);
    return true;
}


static bool process_crc_misr_cmd(cmdString_t pCmd)
{
    struct ovpDisplayTest *pOvpTest = NULL;
    struct ivpDisplayTest *pIvpTest = NULL;
    uint32_t  pipelineNo, sourceNo;
    uint32_t  misrIndex;
    MisrControl_t controlParams;

    /* Check if the cmd match Crc/Misr capability */
    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];

        /* search CRC capability */
        if(strncmp(pCmd, "CRC", strlen("CRC")) == 0)
        {
            if(search_keyword_in_string_array(pCmd, (uint8_t*)pOvpTest->crcData.crcCapabilitiesStr, MAX_CRC_CAP_ELEMENTS, NULL))
            {
                /* This cmd match a CRC capability for this pipeline */
                strncpy(pOvpTest->crcData.requestedCrcStr, pCmd, sizeof(pOvpTest->crcData.requestedCrcStr)-1);
                pOvpTest->crcData.isCrcRequested = true;
                TRC(TRC_ID_COREDISPLAYTEST, "pipelineName = %s, pCmd = %s pOvpTest->crcData.requestedCrcStr = %s", pOvpTest->curPipelineName, pCmd, pOvpTest->crcData.requestedCrcStr);
                goto crc_misr_test;
            }
        }

        /* search MISR capability */
        if(strncmp(pCmd, "MISR", strlen("MISR")) == 0)
        {
            if(search_keyword_in_string_array(pCmd, (uint8_t*)&pOvpTest->misrData.misrCapabilitiesStr, MAX_MISR_CAP_ELEMENTS, NULL))
            {
                /* This cmd match a MISR capability for this pipeline: First check if this MISR can be applied */
                misrIndex = pOvpTest->misrData.nbrOfRequestedMisr;
                strncpy(&pOvpTest->misrData.requestedMisrStr[misrIndex][0], pCmd, sizeof(pOvpTest->misrData.requestedMisrStr[misrIndex]));
                TRC(TRC_ID_COREDISPLAYTEST, "pCmd = %s pOvpTest->misrData.requestedMisrStr[%d][0] = %s", pCmd, misrIndex, &pOvpTest->misrData.requestedMisrStr[misrIndex][0]);
                memset((void*)&controlParams, 0, sizeof(controlParams));
                if (set_misr_control_value(pOvpTest, misrIndex, controlParams))
                {
                    pOvpTest->misrData.isMisrRequested = true;
                    pOvpTest->misrData.nbrOfRequestedMisr++;
                    TRC(TRC_ID_COREDISPLAYTEST, "pOvpTest = %p, pOvpTest->misrData.nbrOfRequestedMisr = %d", pOvpTest, pOvpTest->misrData.nbrOfRequestedMisr);
                }
                goto crc_misr_test;
            }
        }
    }
    /* search source CRC capability */
    for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
    {
        if(strncmp(pCmd, "CRC", strlen("CRC")) == 0)
        {
            pIvpTest = pDisplayTest->pIvpDisplayTest[sourceNo];
            if(search_keyword_in_string_array(pCmd, (uint8_t*)pIvpTest->srcCrcData.crcCapabilitiesStr, MAX_CRC_CAP_ELEMENTS,NULL))
            {
                pIvpTest->srcCrcData.isCrcRequested = true;
                strncpy(pIvpTest->srcCrcData.requestedCrcStr, pCmd, sizeof(pIvpTest->srcCrcData.requestedCrcStr)-1);
                TRC(TRC_ID_COREDISPLAYTEST, "sourceNo = %d, pIvpTest->srcCrcData.requestedCrcStr = %s", pIvpTest->curSourceNo, pIvpTest->srcCrcData.requestedCrcStr);
                goto crc_misr_test;
            }
        }
    }

    /* This command is NOT a CRC or MISR Cmd */
    return false;

crc_misr_test:
    TRC(TRC_ID_COREDISPLAYTEST, "%s is collected", pCmd);
    return true;
}


static bool process_selection_cmd(cmdString_t pCmd)
{
    /* Check if this command match one of the following:
        - A GeneralCmd (coming from a hard coded list)
        - A CRC or MISR available in the capabilities
    */
    if (process_general_cmd(pCmd))
    {
        /* This was a General Cmd and it has been processed */
        return true;
    }
    if(process_crc_misr_cmd(pCmd))
    {
        /* This was a CRC/MISR Cmd and it has been processed */
        return true;
    }
    TRC(TRC_ID_ERROR, "Unknown Selection cmd! (%s)", pCmd);
    return false;
}


static void process_all_selection_cmds(void)
{
    uint32_t i;

    /* Reset every command previously set */
    reset_selection_cmds();

    /* Go through each command of the selection command */
    for(i = 1; i < nbrOfCmds; i++)
    {
        TRC(TRC_ID_COREDISPLAYTEST, "DisplayTestCmdsStr[%d] = %s", i, DisplayTestCmdsStr[i]);
        if(!process_selection_cmd(DisplayTestCmdsStr[i]))
        {
            parse_error_cmd();
            break;
        }
    }
}

static struct ovpDisplayTest *search_misr_to_config(cmdString_t pCmd,  uint32_t *misrIndex)
{
    struct ovpDisplayTest *pOvpTest = NULL;
    uint32_t  pipelineNo;

    TRC(TRC_ID_COREDISPLAYTEST, "pCmd = %s", pCmd);
    /* Check if the cmd match Misr requested */
    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];

        if(strncmp(pCmd, "MISR", strlen("MISR")) == 0)
        {
            if(search_keyword_in_string_array(pCmd, (uint8_t*)&pOvpTest->misrData.requestedMisrStr, pOvpTest->misrData.nbrOfRequestedMisr, misrIndex))
            {
                /* This cmd match a MISR requested for this pipeline:*/
                TRC(TRC_ID_COREDISPLAYTEST, "pCmd = %s pOvpTest->misrData.requestedMisrStr[%d][0] = %s", pCmd,  *misrIndex, &pOvpTest->misrData.requestedMisrStr[ *misrIndex][0]);
                return pOvpTest;
            }
        }
    }
    TRC(TRC_ID_ERROR, "%s isn't in MISR TYPE requested list!", pCmd);
    return NULL;
}

static void process_misr_config_cmds(void)
{
    uint32_t cmdIndex = 0;
    uint32_t configKeyCmdIndex = 0;
    MisrControl_t controlParams;
    struct ovpDisplayTest *pOvpTest = NULL;
    int32_t misrIndex = 0;

    /*
     * example of config cmd:
     * 1. config one MISR:
     * "MISR_CONFIG MISR_MAIN_PF VSYNC 0 COUNTER 0"
     * "MISR_CONFIG MISR_MAIN_PF VSYNC 0 COUNTER 1 MAXVIEWPORT 200 MINVIEWPORT 100"
     *
     * 2. config two MISRs:
     * "MISR_CONFIG MISR_MAIN_PF VSYNC 0 COUNTER 0 MISR_HD_OUT VSYNC 0 COUNTER 0"
     * "MISR_CONFIG MISR_MAIN_PF VSYNC 0 COUNTER 1 MAXVIEWPORT xxx MINVIEWPORT yyy MISR_HD_OUT VSYNC 0 COUNTER 1 MAXVIEWPORT xxx MINVIEWPORT yyy"
     *
     * more than two, just add it behind.
     *
     * Note:
     * when COUNTER == 0, we don't specifiy MAXVIEWPORT and MINVIEWPORT
     * otherwise, MAXVIEWPORT and MINVIEWPORT to be set.
     *
     * Warning:
     * Any config command is not in the order above, all of test commands have to be set again
     *
     */

    cmdIndex = 1;
    while(cmdIndex < nbrOfCmds)
    {
        TRC(TRC_ID_COREDISPLAYTEST, "DisplayTestCmdsStr[%d] = %s", cmdIndex, DisplayTestCmdsStr[cmdIndex]);
        /*
         * Firstly, check the misr type to be configured whether exists in requested misr list
         */
        if(pOvpTest == NULL)
        {
            pOvpTest = search_misr_to_config(DisplayTestCmdsStr[cmdIndex], &misrIndex);
            if(pOvpTest != NULL)
            {
                memset((void*)&controlParams, 0, sizeof(controlParams));
            }
            else
            {
                goto misr_config_err;
            }
        }
        /* If yes, Go through each misr config command */
        else
        {
            configKeyCmdIndex = 0;
            while ((configKeyCmdIndex < MAX_NUM_MISR_CONFIG_CMDS) && (cmdIndex < nbrOfCmds))
            {
                if(strcmp(DisplayTestCmdsStr[cmdIndex],"VSYNC") == 0)
                {
                    cmdIndex++;
                    sscanf(DisplayTestCmdsStr[cmdIndex], "%d", (int*)&controlParams.VsyncVal);
                    /* VsyncVal indicates to collect MISR at which vsync.
                     * 00: every vsync; 01: every two vsyncs; 10: every 4 vsyncs
                     */
                    /* if VsyncVal is greater than 2, it's wrong!!! */
                    if(controlParams.VsyncVal >= NUMBER_OF_VSYNC_TYPES)
                    {
                        goto misr_config_err;
                    }
                }
                else if(strcmp(DisplayTestCmdsStr[cmdIndex], "COUNTER") == 0)
                {
                    cmdIndex++;
                    sscanf(DisplayTestCmdsStr[cmdIndex], "%d", (int*)&controlParams.CounterFlag);
                    /* CounterFlag decides whether maxviewport and minviewport are set by user or not
                     * 0: default viewport; 1: user defined viewport
                     */
                    if(controlParams.CounterFlag == MISR_NON_COUNTER)
                    {
                        /* here, all of config valus are collected when counter is set to "0" */
                        break;
                    }
                    if((controlParams.CounterFlag != MISR_NON_COUNTER)&&(controlParams.CounterFlag != MISR_COUNTER))
                    {
                        goto misr_config_err;
                    }
                }
                else if(strcmp(DisplayTestCmdsStr[cmdIndex], "MAXVIEWPORT") == 0)
                {
                    /* viewport shoud be set only when config_counter is set to "1" */
                    if(controlParams.CounterFlag == MISR_COUNTER)
                    {

                        cmdIndex++;
                        sscanf(DisplayTestCmdsStr[cmdIndex], "%i", (int*)&controlParams.VportMaxPixel);
                        cmdIndex++;
                        sscanf(DisplayTestCmdsStr[cmdIndex], "%i", (int*)&controlParams.VportMaxLine);
                    }
                    else
                    {
                        /* viewport shouldn't be set when counter is "0" */
                        goto misr_config_err;
                    }
                }
                else if(strcmp(DisplayTestCmdsStr[cmdIndex], "MINVIEWPORT") == 0)
                {
                    if(controlParams.CounterFlag == MISR_COUNTER)
                    {
                        cmdIndex++;
                        sscanf(DisplayTestCmdsStr[cmdIndex], "%i", (int*)&controlParams.VportMinPixel);
                        cmdIndex++;
                        sscanf(DisplayTestCmdsStr[cmdIndex], "%i", (int*)&controlParams.VportMinLine);
                        /* here, all of config valus are collected when counter is set to "1" */
                        break;
                    }
                    else
                    {
                        /* viewport shouldn't be set when counter is "0" */
                        goto misr_config_err;
                    }
                }
                else
                {
                        goto misr_config_err;
                }
                cmdIndex++;
                configKeyCmdIndex++;
            }

            if (set_misr_control_value(pOvpTest, misrIndex, controlParams))
            {
                pOvpTest = NULL;
            }
        }
        cmdIndex++;
    }
    return;
misr_config_err:
    parse_error_cmd();
    return;
}

void reset_display_statistics(void)
{
    SetTuningInputData_t resetInputList;
    int setTuningRes = 0;
    uint8_t pipelineNo, sourceNo;
    struct ovpDisplayTest *pOvpTest = NULL;
    struct ivpDisplayTest *pIvpTest = NULL;

    if(pDisplayTest->isTestStarted)
    {
        TRC(TRC_ID_ERROR, "The statistics can't be reset after the test is started!");
        return;
    }

    /* reset statistics on each pipeline */
    memset((void*)&resetInputList, 0x00, sizeof(resetInputList));
    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];
        if(pOvpTest)
        {
            resetInputList.PlaneId = pOvpTest->main_plane_id;
            setTuningRes = stm_display_device_set_tuning(pOvpTest->device,
                                                        RESET_STATISTICS,
                                                        (void *)&resetInputList,
                                                        sizeof(SetTuningInputData_t),
                                                        NULL,
                                                        0);
            if(setTuningRes<0)
            {
                TRC(TRC_ID_ERROR, "RESET_STATISTICS cmd failed with error %d", setTuningRes);
            }
        }
    }
    memset((void*)&resetInputList, 0x00, sizeof(resetInputList));
    /* reset statistics on each Source */
    for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
    {
        pIvpTest = pDisplayTest->pIvpDisplayTest[sourceNo];
        if(pIvpTest)
        {
            resetInputList.SourceId = pIvpTest->curSourceNo;;
            setTuningRes = stm_display_device_set_tuning(pIvpTest->device,
                                                        RESET_STATISTICS,
                                                        (void *)&resetInputList,
                                                        sizeof(SetTuningInputData_t),
                                                        NULL,
                                                        0);
            if(setTuningRes<0)
            {
                TRC(TRC_ID_ERROR, "RESET_STATISTICS cmd failed with error %d", setTuningRes);
            }
        }
    }
    displaytest_clear_reset_flag();
}

void parse_cmds(void)
{
    char    *DisplayTestCtrlCmd;
    char    **bp = NULL;
    char    *tok = NULL;

    TRC(TRC_ID_COREDISPLAYTEST, "coredisplay cmd : entering parse_cmds");
    DisplayTestCtrlCmd = displaytest_get_cmd_str();

    if(!(strlen(DisplayTestCtrlCmd)))
    {
        /* The test command is empty */
        /* to check whether reset display statistics is required*/
        if(displaytest_get_reset_flag())
            reset_display_statistics();
        TRC(TRC_ID_COREDISPLAYTEST, "coredisplay cmd : parse_cmds return case 1 : strlen(DisplayTestCtrlCmd)");
        return;
    }

    if(!pDisplayTest->pipelines_nbr && !pDisplayTest->sources_nbr)
    {
        /* No pipeline, no source, this should not happen... */
        TRC(TRC_ID_ERROR, "No pipeline, no source, this should not happen!");
        return;
    }

    //TRC(TRC_ID_COREDISPLAYTEST, "coredisplay cmd : parse_cmds calling displaytest_reset_result_str\n");
    //displaytest_reset_result_str();

    /* reset commands stored locally */
    memset((void*)DisplayTestCmdsStr, 0x00, sizeof(DisplayTestCmdsStr));

    /* Parse the command string and cut it in individual commands */
    nbrOfCmds = 0;

    bp = &DisplayTestCtrlCmd;
    while((tok = strsep(bp, ", '\n'")) && (nbrOfCmds < MAX_NUM_CMD) )
    {
        if(strcmp(tok, "\0"))
        {
            if(strlen(tok) < MAX_LENGTH_CMD)
            {
                strncpy(DisplayTestCmdsStr[nbrOfCmds], tok, sizeof(DisplayTestCmdsStr[0]));
                DisplayTestCmdsStr[nbrOfCmds][sizeof(DisplayTestCmdsStr[0]) - 1] = 0;
                TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "DisplayTestCmdsStr[%d] = %s",nbrOfCmds, DisplayTestCmdsStr[nbrOfCmds]);
                nbrOfCmds++;
            }
            else
            {
                /* The length of command is too long! */
                TRC(TRC_ID_ERROR, "The length of command is too long");
                goto err_exit;
            }
        }
    }

    if(pDisplayTest->errCmdStatus)
    {
        TRC(TRC_ID_COREDISPLAYTEST, "case errCmdStatus");
        // only supported command when bad state machine is CLEAR_ERROR_STATUS
        if(strcmp(DisplayTestCmdsStr[0], "CLEAR_ERROR_STATUS") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST, "CLEAR_ERROR_STATUS %d", __LINE__);
            process_clear_error_cmd();
            goto done;
        }
        else
        {
            TRC(TRC_ID_COREDISPLAYTEST, "don t process cmd as not CLEAR_ERROR_STATUS");
            goto done;
        }
    }
    else
    {
        /* The first KEYWORD is containing the command and can be one of the following:
            - "START"                   : Start data acquisition
            - "STOP"                    : Stop data acquisition
            - "CAPABILITY"              : Get SoC capabilities
            - "CLEAR_ERROR_STATUS"      :
            - "SELECTION"               : Select CRC/MISR/GeneralCmd needed to acquire (based on the SoC capabilities)
            - "OPTIONAL_MISR_CONFIG"    : Configure some optional parameters like MISR_Viewport and VSync selection
        */
        if(strcmp(DisplayTestCmdsStr[0], "PARTIAL_RST_DRIVER") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "PARTIAL_RST_DRIVER");
            TRC(TRC_ID_COREDISPLAYTEST, "displaytestcmdparsing DisplayTestLastCmd=PARTIAL_RST_DRIVER");
            process_partial_reset_driver_cmd();
            goto done;
        }

        if(strcmp(DisplayTestCmdsStr[0], "START") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "START ");
            process_start_cmd();
            goto done;
        }

        if(strcmp(DisplayTestCmdsStr[0], "STOP") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "STOP ");
            process_stop_cmd();
            goto done;
        }

        if(strcmp(DisplayTestCmdsStr[0], "CAPABILITY") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "CAPABILITY %d", __LINE__);
            process_capability_cmd();
            goto done;
        }

        if(strcmp(DisplayTestCmdsStr[0], "CLEAR_ERROR_STATUS") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "CLEAR_ERROR_STATUS %d", __LINE__);
            process_clear_error_cmd();
            goto done;
        }

        if(strcmp(DisplayTestCmdsStr[0], "SELECTION") == 0)
        {

            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "SELECTION ");
            process_all_selection_cmds();
            goto done;
        }

        if(strcmp(DisplayTestCmdsStr[0], "MISR_CONFIG") == 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "MISR_CONFIG ");
            process_misr_config_cmds();
            goto done;
        }
    }

err_exit:
    TRC(TRC_ID_ERROR, "Set result as FAILED as cmd not recognized");
    displaytest_set_result_str("FAILED");
    parse_error_cmd();
    displaytest_reset_cmd_str();
    return;
done:
    if(pDisplayTest->errCmdStatus)
    {
        // bad state machine
        TRC(TRC_ID_ERROR, "Set result FAILED (due to bad state machine, need to ask CLEAR_ERROR_STATUS before)");
        displaytest_set_result_str("FAILED");
    }
    else
    {
        // good state machine
        TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "coredisplay cmd : set result SUCCEEDED");
        displaytest_set_result_str("SUCCEEDED");
    }

    displaytest_reset_cmd_str();
}

