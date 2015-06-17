/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/stmcoredisplaytest.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Header file name : stmcoredisplaytest.h
 *
 * Access to all platform specific display test information etc
 *
 ************************************************************************/
#ifndef _STMCOREDISPLAYTEST_H
#define _STMCOREDISPLAYTEST_H

#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/stm/stmcoredisplay.h>

#include <stm_display_test_tuning.h>
#include <asm/types.h>

/* Pipeline */
#define MAX_PIPELINES                  4
#define MAIN_PIPELINE_NO               0
#define AUX_PIPELINE_NO                1
#define PIPENAME_STRING_MAX_LEN        5     /* "MAIN", "AUX",*/

/* Crc and Misr*/
#define MAX_CRC_CAP_ELEMENTS           26    /* Value Names:24, Type name:1, Extra:1 */
#define MAX_MISR_CAP_ELEMENTS          9     /* Main MISR type names: 8, Extra:1 */

/* Circular Buffer Size */
#define OUTPUT_BUFFER_SIZE             200   /* Now the maximum number of CRC should be around 60 per second maxi number of MISR (to be defined)*/

/* ST RelayFS */
#define MAX_RELAYFS_STRING_LENGTH      560   /* maximum number of characters per line in the output file */
#define CAPABILITY_STRING_MAX_LEN      320

struct generalInfoReq{
    bool                          isPTS;
    bool                          isVsync;
    bool                          isPicID;
    bool                          isPicType;
    bool                          isVTGEvt;
};

struct crcData{
    CircularBuffer_t             *pCrcOutputCircularBuff;
    bool                          isCrcRequested;                                                               /* Is CRC or GeneralInfo requested by the application? */
    bool                          isCrcFull;
    uint8_t                       requestedCrcStr[MAX_CAPABILITY_STRING_LENGTH];                                /* CRC requested by the application */
    uint8_t                       crcCapabilitiesStr[MAX_CRC_CAP_ELEMENTS][MAX_CAPABILITY_STRING_LENGTH];       /* CRC available on this SoC */
    int                           crcParamsCounter;
};

struct misrData{
    CircularBuffer_t             *pMisrOutputCircularBuff;
    bool                          isMisrRequested;                                                              /* Is some MISRs requested by the application? */
    bool                          isMisrFull;

    uint8_t                       requestedMisrStr[MAX_MISR_COUNTER][MAX_CAPABILITY_STRING_LENGTH];             /* MISR requested by the application */
    uint32_t                      nbrOfRequestedMisr;                                                           /* Number of elements in requestedMisrStr[] */

    uint8_t                       misrCapabilitiesStr[MAX_MISR_CAP_ELEMENTS][MAX_CAPABILITY_STRING_LENGTH];     /* MISR available on this SoC */
};

struct ivpDisplayTest {
    stm_display_device_h          device;
    wait_queue_head_t             status_wait_queue;
    struct task_struct           *pthread;
    uint32_t                      relayFSIndex;
    int32_t                       main_plane_id;           /* Used in the future for FVDP CRC */
    struct generalInfoReq         infoReq;
    uint8_t                       curSourceNo;
    stm_display_source_h          curSourceHandle;
    char                          curSourceName[7];
    struct stmcore_source        *psource_runtime;
    struct stmcore_vsync_cb       source_vsync_cb_info;
    struct crcData                srcCrcData;
    struct crcData                planeCrcData;
};

struct ovpDisplayTest {

    stm_display_device_h          device;
    wait_queue_head_t             status_wait_queue;
    struct task_struct           *pthread;
    uint32_t                      relayFSIndex;
    int32_t                       main_plane_id;
    int32_t                       main_output_id;
    int32_t                       hdmi_output_id;
    int32_t                       dvo_output_id;
    struct generalInfoReq         infoReq;
    struct stmcore_display       *pdisplay_runtime;
    struct stmcore_vsync_cb       output_vsync_cb_info;
    uint8_t                       curPipelineNo;
    char                          curPipelineName[PIPENAME_STRING_MAX_LEN];
    struct misrData               misrData;
    struct crcData                crcData;
};

struct stmcore_displaytest {
    uint32_t                      pipelines_nbr;
    struct ovpDisplayTest        *pOvpDisplayTest[MAX_PIPELINES];
    uint32_t                      sources_nbr;
    struct ivpDisplayTest        *pIvpDisplayTest[STMCORE_MAX_SOURCES];
    bool                          isTestStarted;
    struct mutex                  displayTestMutex;
    bool                          errCmdStatus;
};

extern int thread_vib_testivp[];
extern int thread_vib_testovp[];

#endif /*_STMCOREDISPLAYTEST_H */


