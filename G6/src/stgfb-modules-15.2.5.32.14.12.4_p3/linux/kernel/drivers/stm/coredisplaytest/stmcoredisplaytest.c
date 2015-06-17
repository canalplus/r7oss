/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/stmcoredisplaytest.c
 * Copyright (c) 2014 STMicroelectronics.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : stmcoredisplaytest.c
 * Description      : 1. register VSync_CallBack to collect CRC/MISR from CoreDisplay
 *                       module at each VSync interrupt.
 *                    2. export CRC/MISR to the user space by st_relay module
 *                    3. provide ioctl for user to control CRC/MISR test
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


#include <asm/uaccess.h>
#include <asm/irq.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include <st_relay.h>

#include <displaytestdbgfs.h>
#include <circularbuffer.h>
#include <stmcoredisplaytest.h>
#include <displaytestivp.h>
#include <displaytestovp.h>

#include <thread_vib_settings.h>

#include <vibe_debug.h>

/* Indicate which version the test module it is */
static char *InterfaceVersion = "2.1"; /* major. minor */
module_param(InterfaceVersion, charp, S_IRUGO);
MODULE_PARM_DESC(InterfaceVersion, "DisplayTest Module Version");

int thread_vib_testivp[2] = { THREAD_VIB_TESTIVP_POLICY, THREAD_VIB_TESTIVP_PRIORITY };
module_param_array_named(thread_VIB_TestIvp,thread_vib_testivp, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_TestIvp, "VIB-TestIvp thread:s(Mode),p(Priority)");

int thread_vib_testovp[2] = { THREAD_VIB_TESTOVP_POLICY, THREAD_VIB_TESTOVP_PRIORITY };
module_param_array_named(thread_VIB_TestOvp,thread_vib_testovp, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_TestOvp, "VIB-TestOvp thread:s(Mode),p(Priority)");

struct stmcore_displaytest *pDisplayTest = NULL;

static void stmcore_displaytest_exit(void)
{
    int32_t sourceNo, pipelineNo;

    if(!pDisplayTest)
        return;

    if(pDisplayTest != NULL)
    {
        for (pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
        {

            if(pDisplayTest->pOvpDisplayTest[pipelineNo]== NULL)
            {
                break;
            }
            /* exit ovp displaytest on each pipeline */
            ovp_exit(pDisplayTest->pOvpDisplayTest[pipelineNo]);
            pDisplayTest->pOvpDisplayTest[pipelineNo] = NULL;
        }
        for (sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
        {
            if(pDisplayTest->pIvpDisplayTest[sourceNo]== NULL)
            {
                break;
            }
            /* exit ivp displaytest on each source if it exists */
            ivp_exit(pDisplayTest->pIvpDisplayTest[sourceNo]);
            pDisplayTest->pIvpDisplayTest[sourceNo] = NULL;
        }
    }
    kfree(pDisplayTest);
    displaytest_dbgfs_close();
}


static struct stmcore_displaytest * __init stmcore_displaytest_create_dev_struct(void)
{
    struct  stmcore_displaytest *pTestData = NULL;
    int8_t  pipelineNo, sourceNo;

    pTestData = kzalloc(sizeof(struct stmcore_displaytest), GFP_KERNEL);
    if(pTestData != NULL)
    {
        mutex_init(&(pTestData->displayTestMutex));

        for(pipelineNo = 0; pipelineNo< MAX_PIPELINES; pipelineNo++)
        {
            pTestData->pOvpDisplayTest[pipelineNo] = NULL;
        }

        for(sourceNo = 0; sourceNo < STMCORE_MAX_SOURCES; sourceNo++)
        {
            pTestData->pIvpDisplayTest[sourceNo] = NULL;
        }
    }
    return pTestData;
}

static void stmcore_displaytest_getCapability(void)
{
    if(!pDisplayTest)
    {
        TRC(TRC_ID_ERROR, "pDisplayTest is null!");
        return;
    }

    if(pDisplayTest->pipelines_nbr)
    {
        get_ovp_capability();
    }
    if(pDisplayTest->sources_nbr)
    {
        get_ivp_capability();
    }
}

static int __init stmcore_displaytest_create(void)
{
    struct stmcore_display_pipeline_data *pTestPlatformData[MAX_PIPELINES];
    int32_t pipelineNo, sourceNo;
    int32_t pipelines_nbr = 0;
    int retval = 0;
    int32_t videoSourcNo = 0;

    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO,"Start installing CoreDisplayTest module...\n");

    pDisplayTest = NULL;


    /*
     * copy the platform data from CoreDisplay module
     */
    for(pipelineNo = 0; pipelineNo < MAX_PIPELINES; pipelineNo++)
    {
        pTestPlatformData[pipelineNo] = NULL;
    }

    for(pipelineNo = 0; pipelineNo < MAX_PIPELINES; pipelineNo++)
    {
        pTestPlatformData[pipelineNo] = kzalloc(sizeof(struct stmcore_display_pipeline_data), GFP_KERNEL);
        if(!pTestPlatformData[pipelineNo])
        {
            TRC(TRC_ID_ERROR, "Can't allocate memory to store Platform data for all pipelines!\n");
            retval = -ENOMEM;
            goto exit_create;
        }

        if(stmcore_get_display_pipeline(pipelineNo, pTestPlatformData[pipelineNo])== 0)
        {
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO,"INFO: pipelines %d Device %p, Name = %s\n", pipelineNo, pTestPlatformData[pipelineNo]->device, pTestPlatformData[pipelineNo]->name);
        }
        else
        {
            break;
        }
    }

    pipelines_nbr = pipelineNo;

    /* coredisplay general test structure */
    if((pDisplayTest = stmcore_displaytest_create_dev_struct())==NULL)
    {
        stmcore_displaytest_exit();
        TRC(TRC_ID_ERROR, "Can't allocate memory to install coredisplay test module!\n");
        retval = -ENOMEM;
        goto exit_create;
    }

    pDisplayTest->pipelines_nbr = pipelines_nbr;
    pDisplayTest->isTestStarted = false;
    pDisplayTest->errCmdStatus  = false;

    for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
    {
        if(!pTestPlatformData[pipelineNo])
        {
            TRC(TRC_ID_ERROR, "Platform data pointer is NULL!\n");
            retval = -EINVAL;
            goto exit_create;
        }
        /* create the task to collect crc/misr on output vsync interrupt */
        if((pDisplayTest->pOvpDisplayTest[pipelineNo] = create_ovp(pTestPlatformData[pipelineNo], pipelineNo))== NULL)
        {
            TRC(TRC_ID_ERROR, "Cannot create coredisplay ovp crc/misr test on Pipeline%d\n",pipelineNo);
            stmcore_displaytest_exit();
            retval = -ENODEV;
            goto exit_create;
        }
    }

    /* Create one IVP per source owning a timing generator */
    /* Such sources are listed in pTestPlatformData[MAIN_PIPELINE_NO] */

    /* store the number of sources in pDisplayTest structure */
    pDisplayTest->sources_nbr = pTestPlatformData[MAIN_PIPELINE_NO]->sources_nbr;

    /* create the task to collect crc/misr on input vsync interrupt if it exists */
    if(pDisplayTest->sources_nbr >0)
    {
        for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
        {
            stm_display_source_interface_params_t interfaceParams;
            stm_display_source_queue_h queueInterfaceHandle=0;

            interfaceParams.interface_type = STM_SOURCE_QUEUE_IFACE;
            if(!stm_display_source_get_interface(pTestPlatformData[MAIN_PIPELINE_NO]->sources[sourceNo].source_handle, interfaceParams, (void**)&(queueInterfaceHandle)))
            {
                if((pDisplayTest->pIvpDisplayTest[sourceNo] = create_ivp(pTestPlatformData[MAIN_PIPELINE_NO], sourceNo, MAIN_PIPELINE_NO))==NULL)
                {
                    TRC(TRC_ID_ERROR, "Cannot create coredisplay ivp crc test on source%d\n", sourceNo);
                    stmcore_displaytest_exit();
                    retval = -ENODEV;
                    stm_display_source_queue_release(queueInterfaceHandle);
                    goto exit_create;
                }
                videoSourcNo++;
                stm_display_source_queue_release(queueInterfaceHandle);
            }
            TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO,"%s INFO: sourceNo = %d, videoSourcNo = %d\n", __FUNCTION__, sourceNo, videoSourcNo);
        }
        /* sources_nbr will be set to total available video source number */
        pDisplayTest->sources_nbr = videoSourcNo;
    }

    /*
     * Get the CRC/MISR capability
     */
    stmcore_displaytest_getCapability();

    displaytest_dbgfs_open();

    TRC(TRC_ID_COREDISPLAYTEST_MAIN_INFO,"CoreDisplayTest module was installed.\n");

exit_create:
    /* Free memories allocated locally */
    for(pipelineNo = 0; pipelineNo < MAX_PIPELINES; pipelineNo++)
    {
        if(pTestPlatformData[pipelineNo])
        {
            kfree(pTestPlatformData[pipelineNo]);
        }
    }

    if(retval < 0)
    {
        TRC(TRC_ID_ERROR, "Error Code = %d\n", retval);
    }
    return retval;
}



/******************************************************************************
 *  Modularization
 */

#ifdef MODULE
module_init(stmcore_displaytest_create);
module_exit(stmcore_displaytest_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CoreDipslay Test Module to access CRC/MISR.");
MODULE_AUTHOR("CHENG SERCY");
#endif /* MODULE */

