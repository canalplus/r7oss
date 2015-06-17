/*****************************************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/stmcore_displaytestivp.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : stmcore_displaytestivp.c
 * Description      : 1. register source VSync_CallBack to collect CRC from CoreDisplay
 *                       module at each source VSync int32_t errupt
 *                    2. collect source CRCs in each source Vsync
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
#include <displaytestresultshandling.h>
#include <displaytestivp.h>

#include <vibe_debug.h>

extern struct stmcore_displaytest *pDisplayTest;

/*
 * This function handles two threads(one is for Source0, another is for Source1).
 * This kernel thread mainly exports test results to user space
 */
static int32_t  ivp_threads(void *data)
{
    struct ivpDisplayTest *pIvpTest = (struct ivpDisplayTest*)data;
    char     bufferRelay[MAX_RELAYFS_STRING_LENGTH];
    uint32_t lenBufferRelay;

    TRC(TRC_ID_COREDISPLAYTEST, "Starting Input Vsync DisplayTest Thread on SOURCE%d", pIvpTest->curSourceNo);

    set_freezable();

    while(1)
    {
        if(wait_event_interruptible_timeout(pIvpTest->status_wait_queue,
                                            ((pIvpTest->srcCrcData.isCrcFull != 0) || kthread_should_stop()), (HZ)))
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
            TRC(TRC_ID_COREDISPLAYTEST, "Source CRC Test Thread terminating for pIvpTest = %p", pIvpTest);
            mutex_unlock(&(pDisplayTest->displayTestMutex));
            return 0;
        }

        /* Print out CRC data on each source */
        print_ivp_test_results(pIvpTest);

        /* Print CRC circular buffer full error information if the thread is triggled by FULL */
        if(pIvpTest->srcCrcData.isCrcFull)
        {
            pIvpTest->srcCrcData.isCrcFull = 0;
            lenBufferRelay = sprintf (bufferRelay, "ERR::CRC circular buffer FULL on SOURCE%d!!!\n", pIvpTest->curSourceNo);
            displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, lenBufferRelay, bufferRelay, pIvpTest->relayFSIndex);
        }
        mutex_unlock(&(pDisplayTest->displayTestMutex));
    }
}

static void input_vsync_cb(stm_vsync_context_handle_t context, uint32_t timingevent)
{
    struct ivpDisplayTest   *pIvpTest = (struct ivpDisplayTest*)context;
    SetTuningInputData_t     input_list;
    SetTuningOutputData_t    output_list;
    uint32_t                 input_list_size;
    uint32_t                 output_list_size;
    int32_t                  ret;
    int                      res;

    input_list_size       = sizeof(SetTuningInputData_t);
    output_list_size      = sizeof(SetTuningOutputData_t);

    if(pDisplayTest->isTestStarted)
    {
        if(pIvpTest->srcCrcData.isCrcRequested)
        {
            /* Collect CRC value and general information */
            memset((void*)&output_list, 0, output_list_size);
            memset((void*)&input_list, 0, input_list_size);
            input_list.SourceId = pIvpTest->curSourceNo;

            res = stm_display_device_set_tuning(pIvpTest->device,
                                          MDTP_CRC_COLLECT,
                                          (void *)&input_list,
                                          input_list_size,
                                          (void *)&output_list,
                                          output_list_size);
            if(res<0)
            {
                TRC(TRC_ID_ERROR, "MDTP_CRC_COLLECT cmd failed with error %d", res);
            }

            ret = CircularBufferWrite(pIvpTest->srcCrcData.pCrcOutputCircularBuff, (void*)&output_list, output_list_size);
            if (ret == -CIRCULAR_BUFFER_FULL)
            {
                pIvpTest->srcCrcData.isCrcFull = true;
                wake_up_interruptible(&pIvpTest->status_wait_queue);
            }
        }
    }
}

static struct ivpDisplayTest * __init create_ivp_struct(void)
{
    struct  ivpDisplayTest *pIvpTestData = NULL;

    pIvpTestData = kzalloc(sizeof(struct ivpDisplayTest), GFP_KERNEL);
    if(pIvpTestData != NULL)
    {
        init_waitqueue_head(&(pIvpTestData->status_wait_queue));

        /* create and init the circular buffer for each service of each pipeline */
        pIvpTestData->srcCrcData.pCrcOutputCircularBuff = NULL;
        CircularBufferInit(&pIvpTestData->srcCrcData.pCrcOutputCircularBuff, (uint32_t)OUTPUT_BUFFER_SIZE, sizeof(SetTuningOutputData_t));
        /* if the circular buffer can't be allocated, free pIvpTestData and returns NULL */
        if(pIvpTestData->srcCrcData.pCrcOutputCircularBuff == NULL)
        {
            kfree(pIvpTestData);
            pIvpTestData = NULL;
        }
    }
    return pIvpTestData;
}


static int __init init_ivp_struct(struct ivpDisplayTest *pIvpTest, uint32_t sourceNo, uint32_t pipelineNo, struct stmcore_display_pipeline_data *pPlatformData)
{
    char                  thread_name[14];
    int                   sched_policy;
    struct sched_param    sched_param;

    /*
     * Note that we reuse the device handle from the platform data.
     */
    pIvpTest->device        = pPlatformData->device;

    /* Set current sourceNo */
    pIvpTest->curSourceNo = sourceNo;

    /*
     * Init service cmd flags
     */
    pIvpTest->srcCrcData.isCrcRequested = false;
    pIvpTest->planeCrcData.isCrcRequested = false;

    pIvpTest->infoReq.isPicID = false;
    pIvpTest->infoReq.isPicType = false;
    pIvpTest->infoReq.isPTS = false;
    pIvpTest->infoReq.isVsync = false;

    pIvpTest->srcCrcData.isCrcFull   = false;
    pIvpTest->relayFSIndex = st_relayfs_getindex(ST_RELAY_SOURCE_COREDISPLAY_TEST);

   /*
    * Register input vsync callbacks
    */
    pIvpTest->psource_runtime = pPlatformData->sources[sourceNo].source_runtime;
    INIT_LIST_HEAD(&(pIvpTest->source_vsync_cb_info.node));
    pIvpTest->source_vsync_cb_info.owner = NULL;
    pIvpTest->source_vsync_cb_info.context = (void*)(pIvpTest);
    pIvpTest->source_vsync_cb_info.cb      = input_vsync_cb;
    if(stmcore_register_source_vsync_callback(pIvpTest->psource_runtime, &pIvpTest->source_vsync_cb_info)<0)
    {
        TRC(TRC_ID_ERROR, "Cannot register coredisplaytest input vsync callback on SOURCE%d",pIvpTest->curSourceNo);
        return -ENODEV;
    }
    /*
     * Create a thread for each source
     */
    snprintf(thread_name, sizeof(thread_name), "VIB-TestIvp/%d", sourceNo );
    pIvpTest->pthread = kthread_create(ivp_threads, pIvpTest, thread_name);
    if( pIvpTest->pthread == NULL)
    {
        TRC(TRC_ID_ERROR, "FAILED to create thread %s", thread_name);
        return -ENOMEM;
    }

    /* Set thread scheduling settings */
    sched_policy               = thread_vib_testivp[0];
    sched_param.sched_priority = thread_vib_testivp[1];
    if ( sched_setscheduler(pIvpTest->pthread, sched_policy, &sched_param) )
    {
      TRC(TRC_ID_ERROR, "FAILED to set thread scheduling parameters: name=%s, policy=%d, priority=%d", \
              thread_name, sched_policy, sched_param.sched_priority);
      return -ENODEV;
    }

    /* Wake up the thread */
    wake_up_process(pIvpTest->pthread);

    return 0;
}

struct ivpDisplayTest* __init create_ivp(struct stmcore_display_pipeline_data *pPlatformData, uint32_t sourceNo, uint32_t pipelineNo)
{
    struct ivpDisplayTest *pIvpTestData = NULL;

    if((pDisplayTest == NULL) || (pPlatformData == NULL))
    {
        return pIvpTestData;
    }

    /* create structure for ivp */
    if((pIvpTestData = create_ivp_struct())==NULL)
    {
        return pIvpTestData;
    }

    /* init structure for ivp */
    if(init_ivp_struct(pIvpTestData, sourceNo, pipelineNo, pPlatformData)<0)
    {
        TRC(TRC_ID_ERROR, "Cannot create coredisplay ivp crc test on source%d", sourceNo);
        ivp_exit(pIvpTestData);
        pIvpTestData = NULL;
    }
    return pIvpTestData;
}

void ivp_exit(struct ivpDisplayTest *pIvpTest)
{
    if(pIvpTest)
    {
        /*
         * Terminate input vsync CoreDisplayTest management thread
         */
        if(pIvpTest->pthread != NULL)
        {
            kthread_stop(pIvpTest->pthread);
            pIvpTest->pthread = NULL;
        }

        /*
         * Unregister input vsync callback for the CoreDisplayTest module
         * release output circular buffers
         */
        stmcore_unregister_source_vsync_callback(pIvpTest->psource_runtime, &pIvpTest->source_vsync_cb_info);
        CircularBufferRelease(pIvpTest->srcCrcData.pCrcOutputCircularBuff);
        st_relayfs_freeindex(ST_RELAY_SOURCE_COREDISPLAY_TEST, pIvpTest->relayFSIndex);
        /*
         * Do not release the device handle, it was a copy of the platform data
         * which will get released elsewhere.
         */
        pIvpTest->device = NULL;
        kfree(pIvpTest);
    }
    return;
}

void get_ivp_capability(void)
{
    char    capability[CAPABILITY_STRING_MAX_LEN];
    char   *pCapability = NULL;
    char  **bp = NULL;
    char   *tok = NULL;
    uint8_t i, j;
    struct ivpDisplayTest *pIvpTest = NULL;
    uint8_t sourceNo;
    SetTuningInputData_t inputList;
    int res;

    for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
    {
        pIvpTest = pDisplayTest->pIvpDisplayTest[sourceNo];
        if(!pIvpTest)
            return;

        memset(capability, 0, CAPABILITY_STRING_MAX_LEN);
        inputList.SourceId = sourceNo;

        res = stm_display_device_set_tuning(pIvpTest->device,
                                         MDTP_CRC_CAPABILITY,
                                         (void *)&inputList,
                                         sizeof(SetTuningInputData_t),
                                         (void*)capability,
                                         CAPABILITY_STRING_MAX_LEN);
        if(res == 0)
        {
            i = 0;
            TRC(TRC_ID_COREDISPLAYTEST, "capability = %s", capability);
            pCapability = capability;
            bp = &pCapability;
            while((tok = strsep(bp, ",")))
            {
                snprintf(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0], MAX_CAPABILITY_STRING_LENGTH,"CRC_SOURCE%d_%s", sourceNo, tok);
                TRC(TRC_ID_COREDISPLAYTEST, "pIvpTest->srcCrcData.crcCapabilitiesStr[%d]= %s", i, &pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]);
                tok = strsep(bp, ",");
                if(tok != NULL)
                {
                    i++;
                    strcpy(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0], tok);
                    TRC(TRC_ID_COREDISPLAYTEST, "pIvpTest->srcCrcData.crcCapabilitiesStr[%d]= %s", i, &pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]);
                    sscanf(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0],"%d",&pIvpTest->srcCrcData.crcParamsCounter);
                }
                for(j = 0; j <  pIvpTest->srcCrcData.crcParamsCounter; j++)
                {
                    tok = strsep(bp, ",");
                    if(tok != NULL)
                    {
                        i++;
                        strcpy(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0], tok);
                        TRC(TRC_ID_COREDISPLAYTEST, "pIvpTest->srcCrcData.crcCapabilitiesStr[%d]= %s", i, &pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]);
                    }
                }
                i++;
            }
        }
        else
        {
            TRC(TRC_ID_ERROR, "MDTP_CRC_CAPABILITY cmd failed with error %d", res);
        }
    }
}

