/*****************************************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestovp.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : displaytestovp.c
 * Description      : 1. Allocate and initialize ovp structure(s)
 *                    2. Register output vsync_callback to collect CRC/MISR from CoreDisplay
 *                       module at each output vsync interrupt
 *                    3. Create thread to export test results to the user space and receive
 *                       commands from the user space
 *
 *****************************************************************************************/
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


#include <asm/uaccess.h>
#include <asm/irq.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include <st_relay.h>

#include <displaytestdbgfs.h>
#include <circularbuffer.h>
#include <stmcoredisplaytest.h>
#include <displaytestcmdparsing.h>
#include <displaytestresultshandling.h>
#include <displaytestovp.h>

#include <vibe_debug.h>

extern struct stmcore_displaytest *pDisplayTest;

static int32_t ovp_threads(void *data)
{
    struct ovpDisplayTest *pOvpTest = (struct ovpDisplayTest*)data;
    char     bufferRelay[MAX_RELAYFS_STRING_LENGTH];
    uint32_t lenBufferRelay = 0;

    TRC(TRC_ID_COREDISPLAYTEST, "Starting Output Vsync DisplayTest Thread on %s", pOvpTest->curPipelineName);

    set_freezable();

    while(1)
    {
        if(wait_event_interruptible_timeout(pOvpTest->status_wait_queue,
                                            (((pOvpTest->crcData.isCrcFull != 0) || (pOvpTest->misrData.isMisrFull != 0))
                                            || kthread_should_stop()), (HZ)))
        {
            if (try_to_freeze())
            {
                /*
                 * Back around the loop, any pending work or kthread stop will get
                 * picked up again immediately.
                 */
                continue;
            }
        }

        mutex_lock(&(pDisplayTest->displayTestMutex));

        if(kthread_should_stop())
        {
            TRC(TRC_ID_COREDISPLAYTEST, "ovp DisplayTest Thread terminating for %sPipeline", pOvpTest->curPipelineName);
            mutex_unlock(&(pDisplayTest->displayTestMutex));
            return 0;
        }

        /* Print out CRC/MISR data on each pipeline */
        print_ovp_test_results(pOvpTest);

        /* Print CRC/MISR circular buffer full error information if the thread is triggled by FULL */
        if(pOvpTest->crcData.isCrcFull || pOvpTest->misrData.isMisrFull)
        {
            lenBufferRelay = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "ERR::%s%s"
                                      "circular buffer FULL on %s Pipeline!!!\n",
                                      pOvpTest->crcData.isCrcFull?"CRC ":"",
                                      pOvpTest->misrData.isMisrFull?"MISR ":"",
                                      pOvpTest->curPipelineName);
            if(pOvpTest->crcData.isCrcFull)
            {
                pOvpTest->crcData.isCrcFull = false;
            }
            if(pOvpTest->misrData.isMisrFull)
            {
                pOvpTest->misrData.isMisrFull = false;
            }
            displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, strlen(bufferRelay), bufferRelay, pOvpTest->relayFSIndex);
        }
        parse_cmds();
        mutex_unlock(&(pDisplayTest->displayTestMutex));
    }
}

static void output_vsync_cb(stm_vsync_context_handle_t context, uint32_t timingevent)
{
    struct ovpDisplayTest *pOvpTest = (struct ovpDisplayTest *)context;

    SetTuningInputData_t  input_list;
    SetTuningOutputData_t output_list;
    uint32_t              input_list_size  = sizeof(SetTuningInputData_t);
    uint32_t              output_list_size = sizeof(SetTuningOutputData_t);
    int32_t               ret;
    int                   res;

    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "%d isTestStarted=%d, isCrcRequested=%d, isMisrRequested=%d",
            pOvpTest->main_output_id,
            pDisplayTest->isTestStarted,
            pOvpTest->crcData.isCrcRequested,
            pOvpTest->misrData.isMisrRequested);

    if(pDisplayTest->isTestStarted)
    {
        if(pOvpTest->crcData.isCrcRequested)
        {
            /* to collect CRC value and general information */

            /* Prepare the command */
            memset((void*)&output_list, 0, output_list_size);
            memset((void*)&input_list, 0, input_list_size);
            input_list.PlaneId = pOvpTest->main_plane_id;

            /* Send the command to the driver */
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "%d : CRC_COLLECT", pOvpTest->main_output_id);
            res = stm_display_device_set_tuning(pOvpTest->device,
                                          CRC_COLLECT,
                                          (void *)&input_list,
                                          input_list_size,
                                          (void *)&output_list,
                                          output_list_size);
            // "-ENODATA" is returned when there is no CRC data to collect
            // In case of "-ENODATA", output_list contains only some null data but it should be put in the circular buffer anyway.
            if((res == 0) || (res == -ENODATA))
            {
                /* Store the results in the circular buffer */
                ret = CircularBufferWrite(pOvpTest->crcData.pCrcOutputCircularBuff, (void*)&output_list, output_list_size);
                if (ret == -CIRCULAR_BUFFER_FULL)
                {
                    pOvpTest->crcData.isCrcFull = true;
                    wake_up_interruptible(&pOvpTest->status_wait_queue);
                }
            }
            else
            {
                TRC(TRC_ID_ERROR, "Plane_id %d: CRC_COLLECT cmd failed with error %d", pOvpTest->main_plane_id, res);
            }
        }

        if(pOvpTest->misrData.isMisrRequested)
        {
            /* to collect MISR value */

            /* Prepare the command */
            memset((void*)&output_list, 0, output_list_size);
            memset((void*)&input_list, 0, input_list_size);
            input_list.OutputId = pOvpTest->main_output_id;

            /* Send the command to the driver */
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "%d : MISR_COLLECT", pOvpTest->main_output_id);
            res = stm_display_device_set_tuning(pOvpTest->device,
                                          MISR_COLLECT,
                                          (void *)&input_list,
                                          input_list_size,
                                          (void *)&output_list,
                                          output_list_size);
            // "-ENODATA" is returned when there is no MISR data to collect
            // In case of "-ENODATA", output_list contains only some null databut it should be put in the circular buffer anyway.
            if((res == 0) || (res == -ENODATA))
            {
                /* Store the results in the circular buffer */
                ret = CircularBufferWrite(pOvpTest->misrData.pMisrOutputCircularBuff, (void*)&output_list, output_list_size);
                if (ret == -CIRCULAR_BUFFER_FULL)
                {
                    pOvpTest->misrData.isMisrFull = true;
                    wake_up_interruptible(&pOvpTest->status_wait_queue);
                }
            }
            else
            {
                TRC(TRC_ID_ERROR, "Plane_id %d: MISR_COLLECT cmd failed with error %d", pOvpTest->main_plane_id, res);
            }
        }
    }
}

void ovp_exit(struct ovpDisplayTest *pOvpTest)
{
    if(pOvpTest)
    {
        /*
         * Terminate output vsync CoreDisplayTest management thread
         */
        if(pOvpTest->pthread != NULL)
        {
            kthread_stop(pOvpTest->pthread);
            pOvpTest->pthread = NULL;
        }

        /*
         * Unregister output vsync callback for the CoreDisplayTest module
         */
        stmcore_unregister_vsync_callback(pOvpTest->pdisplay_runtime, &pOvpTest->output_vsync_cb_info);
        /*
         * Release output circular buffers
         */
        CircularBufferRelease(pOvpTest->crcData.pCrcOutputCircularBuff);
        CircularBufferRelease(pOvpTest->misrData.pMisrOutputCircularBuff);
        /*
         * Free relayFSIndex
         */
        st_relayfs_freeindex(ST_RELAY_SOURCE_COREDISPLAY_TEST, pOvpTest->relayFSIndex);
        /*
         * Do not release the device handle, it was a copy of the platform data
         * which will get released elsewhere.
         */
        pOvpTest->device = NULL;
        kfree(pOvpTest);
    }
}


static int init_ovp_struct(struct ovpDisplayTest *pOvpTest, uint32_t pipelineNo, struct stmcore_display_pipeline_data *pPlatformData)
{
    stm_plane_capabilities_t plane_caps_value, plane_caps_mask;
    char                     thread_name[14];
    int                      sched_policy;
    struct sched_param       sched_param;
    int                      res;

    /*
     * Note that we reuse the device handle from the platform data.
     */
    pOvpTest->device        = pPlatformData->device;

    /* Set current pipelineNo */
    pOvpTest->curPipelineNo = pipelineNo;

    /* Set current pipeline name */
    if(pOvpTest->curPipelineNo == 0)
    {
        strcpy(pOvpTest->curPipelineName, "MAIN");
    }
    else if(pOvpTest->curPipelineNo == 1)
    {
        strcpy(pOvpTest->curPipelineName, "AUX");
    }

    /*
     * Get preferred video plane
     */
    if(pipelineNo == MAIN_PIPELINE_NO)
    {
        plane_caps_value = (stm_plane_capabilities_t) (PLANE_CAPS_VIDEO | PLANE_CAPS_VIDEO_BEST_QUALITY |
                                                       PLANE_CAPS_PRIMARY_PLANE | PLANE_CAPS_PRIMARY_OUTPUT);
        plane_caps_mask = plane_caps_value;
    }
    else
    {
        plane_caps_value = (stm_plane_capabilities_t) (PLANE_CAPS_VIDEO | PLANE_CAPS_SECONDARY_OUTPUT);
        plane_caps_mask = plane_caps_value;
    }
    res = stm_display_device_find_planes_with_capabilities(pOvpTest->device, plane_caps_value, plane_caps_mask, &pOvpTest->main_plane_id, 1);
    if(res<0)
    {
        TRC(TRC_ID_ERROR, "Failed to find planes with capabilities! (%d)", res);
    }
    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO, "pOvpTest->main_plane_id= %d", pOvpTest->main_plane_id);

    /*
     * Copy video output ids, source id...
     */
    pOvpTest->main_output_id = pPlatformData->main_output_id;
    pOvpTest->hdmi_output_id = pPlatformData->hdmi_output_id;
    pOvpTest->dvo_output_id  = pPlatformData->dvo_output_id;

    /*
     * Init service cmd flags
     */
    pOvpTest->crcData.isCrcRequested = false;

    pOvpTest->misrData.isMisrRequested = false;
    pOvpTest->infoReq.isPicID = false;
    pOvpTest->infoReq.isPicType = false;
    pOvpTest->infoReq.isPTS = false;
    pOvpTest->infoReq.isVsync = false;
    pOvpTest->infoReq.isVTGEvt = false;
    pOvpTest->crcData.isCrcFull   = false;
    pOvpTest->misrData.isMisrFull = false;
    pOvpTest->relayFSIndex = st_relayfs_getindex(ST_RELAY_SOURCE_COREDISPLAY_TEST);

    /*
     * Copy the display runtime pointer for the vsync callback handling.
     */
    pOvpTest->pdisplay_runtime = pPlatformData->display_runtime;

    /*
     * Register output vsync callback
     */
    INIT_LIST_HEAD(&(pOvpTest->output_vsync_cb_info.node));
    pOvpTest->output_vsync_cb_info.owner = NULL;
    pOvpTest->output_vsync_cb_info.context = (void*)pOvpTest;
    pOvpTest->output_vsync_cb_info.cb      = output_vsync_cb;
    if(stmcore_register_vsync_callback(pOvpTest->pdisplay_runtime, &pOvpTest->output_vsync_cb_info)<0)
    {
        TRC(TRC_ID_ERROR, "Cannot register coredisplaytest output vsync callback on %sPipeline", pOvpTest->curPipelineName);
        return -ENODEV;
    }

    /*
     * Create a thread for each pipeline
     */
    snprintf(thread_name, sizeof(thread_name), "VIB-TestOvp/%d", pipelineNo );
    pOvpTest->pthread = kthread_create(ovp_threads, pOvpTest, thread_name);
    if(pOvpTest->pthread == NULL)
    {
        TRC(TRC_ID_ERROR, "FAILED to create thread %s", thread_name);
        return -ENOMEM;
    }

    /* Set thread scheduling settings */
    sched_policy               = thread_vib_testovp[0];
    sched_param.sched_priority = thread_vib_testovp[1];
    if ( sched_setscheduler(pOvpTest->pthread, sched_policy, &sched_param) )
    {
        TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
            thread_name, sched_policy, sched_param.sched_priority);
        return -ENODEV;
    }

    /* Wake up the thread */
    wake_up_process(pOvpTest->pthread);

    return 0;
}

static struct ovpDisplayTest * __init create_ovp_struct(void)
{
    struct  ovpDisplayTest *pOvpTestData = NULL;

    pOvpTestData = kzalloc(sizeof(struct ovpDisplayTest), GFP_KERNEL);
    if(pOvpTestData != NULL)
    {
        init_waitqueue_head(&(pOvpTestData->status_wait_queue));

        /* create and init CRC circular buffer */
        pOvpTestData->crcData.pCrcOutputCircularBuff = NULL;
        CircularBufferInit(&(pOvpTestData->crcData.pCrcOutputCircularBuff), (uint32_t)OUTPUT_BUFFER_SIZE, sizeof(SetTuningOutputData_t));
        /* if the circular buffer can't be allocated, free pOvpTestData and returns NULL */
        if(pOvpTestData->crcData.pCrcOutputCircularBuff == NULL)
        {
            kfree(pOvpTestData);
            pOvpTestData = NULL;
            return pOvpTestData;
        }

        /* create and init MISR circular buffer */
        pOvpTestData->misrData.pMisrOutputCircularBuff = NULL;
        CircularBufferInit(&(pOvpTestData->misrData.pMisrOutputCircularBuff), (uint32_t)OUTPUT_BUFFER_SIZE, sizeof(SetTuningOutputData_t));
        /* if the circular buffer can't be allocated, free CRC circular buffer, pOvpTestData and returns NULL */
        if(pOvpTestData->misrData.pMisrOutputCircularBuff == NULL)
        {
            CircularBufferRelease(pOvpTestData->crcData.pCrcOutputCircularBuff);
            kfree(pOvpTestData);
            pOvpTestData = NULL;
            return pOvpTestData;
        }
    }
    return pOvpTestData;
}


void get_ovp_capability(void)
{
    char    capability[CAPABILITY_STRING_MAX_LEN];
    char   *pCapability = NULL;
    char  **bp = NULL;
    char   *tok = NULL;
    uint8_t i, pipelineNo;
    SetTuningInputData_t inputList;
    int     result;

    struct ovpDisplayTest *pOvpTest = NULL;

    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        pOvpTest = pDisplayTest->pOvpDisplayTest[pipelineNo];

        if(!pOvpTest)
            return;

        if(pipelineNo == MAIN_PIPELINE_NO)
        {
            memset(capability, 0, CAPABILITY_STRING_MAX_LEN);
            /* Get capability of SOC about CRC because it only exists on main display */
            inputList.PlaneId  = pOvpTest->main_plane_id;

            result = stm_display_device_set_tuning( pOvpTest->device,
                                              CRC_CAPABILITY,
                                              (void *)&inputList,
                                              sizeof(SetTuningInputData_t),
                                              (void*)capability,
                                              CAPABILITY_STRING_MAX_LEN);
            if(result == 0)
            {
                pOvpTest->crcData.crcParamsCounter=0;
                pCapability = capability;
                bp = &pCapability;
                i = 0;
                tok = strsep(bp, ",");
                if(tok != NULL)
                {
                    strcpy(& pOvpTest->crcData.crcCapabilitiesStr[i][0], "CRC_MAIN_");
                    strcat(& pOvpTest->crcData.crcCapabilitiesStr[i][0], tok);
                    TRC(TRC_ID_COREDISPLAYTEST, "pOvpTest->crcData.crcCapabilitiesStr[%d][0]= %s", i, &pOvpTest->crcData.crcCapabilitiesStr[i][0]);
                }
                while((tok = strsep(bp, ",")))
                {
                    TRC(TRC_ID_COREDISPLAYTEST, "tok[%s]", tok);
                    i++;
                    strcpy(& pOvpTest->crcData.crcCapabilitiesStr[i][0], tok);
                    pOvpTest->crcData.crcParamsCounter++;
                }
                TRC(TRC_ID_COREDISPLAYTEST, "pOvpTest->crcData.crcParamsCounter = %d", pOvpTest->crcData.crcParamsCounter);
            }
            else
            {
                TRC(TRC_ID_ERROR, "Failed to get CRC_CAPABILITY with error %d", result);
            }
        }

        /* Get capability of SOC about MISR */
        memset(capability, 0, CAPABILITY_STRING_MAX_LEN);
        inputList.OutputId = pOvpTest->main_output_id;

        result = stm_display_device_set_tuning(pOvpTest->device,
                                         MISR_CAPABILITY,
                                         (void *)&inputList,
                                         sizeof(SetTuningInputData_t),
                                         (void*)capability,
                                         CAPABILITY_STRING_MAX_LEN);
        if(result == 0)
        {
            pCapability = capability;
            bp = &pCapability;
            i = 0;
            while((tok = strsep(bp, ",")))
            {
                TRC(TRC_ID_COREDISPLAYTEST, "tok[%s]", tok);
                strcpy(&pOvpTest->misrData.misrCapabilitiesStr[i][0], tok);
                i++;
            }
        }
        else
        {
            TRC(TRC_ID_ERROR, "Failed to get MISR_CAPABILITY with error %d", result);
        }
    }
}

struct ovpDisplayTest * __init create_ovp(struct stmcore_display_pipeline_data *pPlatformData, uint32_t pipelineNo)
{
    struct ovpDisplayTest *pOvpTestData = NULL;

    if((pDisplayTest == NULL) || (pPlatformData == NULL))
    {
        return pOvpTestData;
    }

    /* create structure for ovp */
    if((pOvpTestData = create_ovp_struct()) ==NULL)
    {
        return pOvpTestData;
    }

    /* init structure for ovp */
    if(init_ovp_struct(pOvpTestData, pipelineNo, pPlatformData)<0)
    {
        TRC(TRC_ID_ERROR, "Cannot create coredisplay ovp crc/misr test on %s", pOvpTestData->curPipelineName);
        ovp_exit(pOvpTestData);
        pOvpTestData = NULL;
    }
    return pOvpTestData;
}

