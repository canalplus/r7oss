/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplaytest/displaytestresulthandling.c
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Source file name : displaytestresulthandling.c
 * Description      : export infromation collected by the test module to
 *                    the user space
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
#include <displaytestresultshandling.h>
#include <vibe_debug.h>

extern struct stmcore_displaytest *pDisplayTest;

static void print_ovp_capability(struct ovpDisplayTest* pOvpTest)
{
    char bufferRelay[MAX_RELAYFS_STRING_LENGTH] = {0, };
    int32_t lenBufferRelay = 0;
    uint8_t i;

    /* to get capability of SOC General capability*/
    lenBufferRelay = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%sPTS,%sVsync,%sPictureID,%sPictureType,%sVTGEvent,\n",
                                                                                                    pOvpTest->curPipelineName,
                                                                                                    pOvpTest->curPipelineName,
                                                                                                    pOvpTest->curPipelineName,
                                                                                                    pOvpTest->curPipelineName,
                                                                                                    pOvpTest->curPipelineName);

    displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, lenBufferRelay, bufferRelay, pOvpTest->relayFSIndex);

    /* to get capability of SOC about CRC on output vsync */
    if(pOvpTest->curPipelineNo == MAIN_PIPELINE_NO)
    {
        lenBufferRelay = 0;
        i = 0;
        if(strlen(&pOvpTest->crcData.crcCapabilitiesStr[i][0]))
        {
            lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "%s(", &pOvpTest->crcData.crcCapabilitiesStr[i][0]);
            if(pOvpTest->crcData.crcParamsCounter > 0)
            {
                for(i = 1; i <= pOvpTest->crcData.crcParamsCounter; i++)
                {
                    lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "%s,", &pOvpTest->crcData.crcCapabilitiesStr[i][0]);
                }
            }
            lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "),\n");
            displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, lenBufferRelay, bufferRelay, pOvpTest->relayFSIndex);
        }
    }

    /* to get capability of SOC about MISR on output vsync */
    lenBufferRelay = 0;
    for(i = 0; i < MAX_MISR_CAP_ELEMENTS; i++)
    {
        if(strlen(&pOvpTest->misrData.misrCapabilitiesStr[i][0]) == 0)
        {
            break;
        }
        lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "%s,", &pOvpTest->misrData.misrCapabilitiesStr[i][0]);
    }
    lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "\n");
    displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, lenBufferRelay, bufferRelay, pOvpTest->relayFSIndex);
}

static void print_ivp_capability(struct ivpDisplayTest* pIvpTest)
{
    char bufferRelay[MAX_RELAYFS_STRING_LENGTH] = {0, };
    int32_t lenBufferRelay = 0;
    uint8_t i, j, sourceNo;

    sourceNo = pIvpTest->curSourceNo;
    lenBufferRelay = 0;
    lenBufferRelay = snprintf(bufferRelay, MAX_RELAYFS_STRING_LENGTH, "SOURCE%dPTS,SOURCE%dVsync,SOURCE%dPictureID,SOURCE%dPictureType,\n", sourceNo, sourceNo, sourceNo, sourceNo);
    displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, lenBufferRelay, bufferRelay, pIvpTest->relayFSIndex);
    lenBufferRelay = 0;
    for(i = 0; i < MAX_CRC_CAP_ELEMENTS; i++)
    {
        if(strlen(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0])==0)
            break;
        lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "%s(",&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]);
        i++;

        if ( i < MAX_CRC_CAP_ELEMENTS )
          {
            sscanf(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0], "%d", &pIvpTest->srcCrcData.crcParamsCounter);
            for(j = 0; j < pIvpTest->srcCrcData.crcParamsCounter; j++)
              {
                i++;

                if ( i < MAX_CRC_CAP_ELEMENTS )
                  {
                    lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "%s,", &pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]);
                  }
              }
            lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "),");
            displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, lenBufferRelay, bufferRelay, pIvpTest->relayFSIndex);
            lenBufferRelay = 0;
          }
    }
    lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH - lenBufferRelay, "\n");
    displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, lenBufferRelay, bufferRelay, pIvpTest->relayFSIndex);
}

void export_capability(struct stmcore_displaytest *pDisplayTest)
{
    uint8_t pipelineNo, sourceNo;
    /* output vsync */
    if(pDisplayTest->pipelines_nbr)
    {
        for(pipelineNo = 0; pipelineNo < pDisplayTest->pipelines_nbr; pipelineNo++)
        {
            print_ovp_capability(pDisplayTest->pOvpDisplayTest[pipelineNo]);
        }
    }

    /* input vsync */
    if(pDisplayTest->sources_nbr)
    {
        for(sourceNo = 0; sourceNo < pDisplayTest->sources_nbr; sourceNo++)
        {
            print_ivp_capability(pDisplayTest->pIvpDisplayTest[sourceNo]);
        }
    }
}

static void store_ovp_general_header_to_relay_buffer(struct ovpDisplayTest *pOvpTest, int32_t *lenBufferRelay, char *bufferRelay)
{
    int32_t  bufferLength =  *lenBufferRelay;

    if(pOvpTest->infoReq.isVsync)
        bufferLength  += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%sVsync,", pOvpTest->curPipelineName);

    if(pOvpTest->infoReq.isPTS)
        bufferLength  += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%sPTS,", pOvpTest->curPipelineName);

    if(pOvpTest->infoReq.isPicID)
        bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%sPictureID,", pOvpTest->curPipelineName);

    if(pOvpTest->infoReq.isPicType)
        bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%sPictureTYPE,", pOvpTest->curPipelineName);

    *lenBufferRelay = bufferLength;
}

static void store_ovp_crc_header_to_relay_buffer(struct ovpDisplayTest *pOvpTest, int32_t *lenBufferRelay, char *bufferRelay)
{
    int32_t  bufferLength =  *lenBufferRelay;
    uint8_t  i;

    if(strlen(pOvpTest->crcData.requestedCrcStr))
    {
        i = 0;
        bufferLength += snprintf(bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%s(", &pOvpTest->crcData.crcCapabilitiesStr[i][0]);
        for(i = 1; i <= pOvpTest->crcData.crcParamsCounter; i++)
        {
            bufferLength += snprintf(bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%s,", &pOvpTest->crcData.crcCapabilitiesStr[i][0]);
        }
        bufferLength += snprintf(bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "),");
    }
    *lenBufferRelay = bufferLength;
}

static void store_ovp_misr_header_to_relaybuffer(struct ovpDisplayTest *pOvpTest, int32_t *lenBufferRelay, char *bufferRelay)
{
    int32_t  bufferLength =  *lenBufferRelay;
    uint8_t  i;

    if(pOvpTest->infoReq.isVsync)
        bufferLength  += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "MISR%sVsync,", pOvpTest->curPipelineName);

    if(pOvpTest->infoReq.isVTGEvt)
        bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%sVTGEvent,", pOvpTest->curPipelineName);

    for(i = 0; i < pOvpTest->misrData.nbrOfRequestedMisr; i++)
    {
        if(strlen(&pOvpTest->misrData.requestedMisrStr[i][0]))
        {
            bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%s(R1,R2,R3),", &pOvpTest->misrData.requestedMisrStr[i][0]);
        }
    }
    *lenBufferRelay = bufferLength;
}

static void print_ovp_test_header(int32_t isCrcEmpty, int32_t isMisrEmpty, struct ovpDisplayTest *pOvpTest, char *bufferRelay)
{
    int32_t  lenBufferRelay = 0;

    if(isCrcEmpty == CIRCULAR_BUFFER_OK)
    {
        store_ovp_general_header_to_relay_buffer(pOvpTest, &lenBufferRelay, bufferRelay);
        store_ovp_crc_header_to_relay_buffer(pOvpTest, &lenBufferRelay, bufferRelay);
    }

    if(isMisrEmpty == CIRCULAR_BUFFER_OK)
    {
        store_ovp_misr_header_to_relaybuffer(pOvpTest, &lenBufferRelay, bufferRelay);
    }
    lenBufferRelay += snprintf (bufferRelay  + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "\n");
    displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, lenBufferRelay, bufferRelay, pOvpTest->relayFSIndex);
}

static void store_ovp_general_data_to_relay_buffer(struct ovpDisplayTest *pOvpTest, int32_t *lenBufferRelay, char *bufferRelay, SetTuningOutputData_t *crcData)
{
    int32_t  bufferLength = *lenBufferRelay;

    if(pOvpTest->infoReq.isVsync )
        bufferLength = snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%16llx,", crcData->Data.Crc.LastVsyncTime);

    if(pOvpTest->infoReq.isPTS )
        bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%16llx,", crcData->Data.Crc.PTS);

    if(pOvpTest->infoReq.isPicID)
        bufferLength += snprintf (bufferRelay + bufferLength , MAX_RELAYFS_STRING_LENGTH, "%8d,", crcData->Data.Crc.PictureID);

    if(pOvpTest->infoReq.isPicType)
    {
        /* required by test frame work */
        if((crcData->Data.Crc.PictureType != 'T')&&(crcData->Data.Crc.PictureType != 'B')&&(crcData->Data.Crc.PictureType != 'F'))
        {
            crcData->Data.Crc.PictureType = 'U';
        }
        bufferLength += snprintf (bufferRelay + bufferLength , MAX_RELAYFS_STRING_LENGTH, "%c,", crcData->Data.Crc.PictureType);
    }
    *lenBufferRelay = bufferLength;
}

static void store_ovp_crc_data_to_relay_buffer(struct ovpDisplayTest *pOvpTest, int32_t *lenBufferRelay, char *bufferRelay, SetTuningOutputData_t *crcData)
{
    int32_t  bufferLength = *lenBufferRelay;
    uint8_t  i;

    if(strlen(pOvpTest->crcData.requestedCrcStr))
    {
        for(i = 0; i < pOvpTest->crcData.crcParamsCounter; i++)
        {
            bufferLength += snprintf (bufferRelay  + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%8x,", crcData->Data.Crc.CrcValue[i]);
        }
    }
    *lenBufferRelay = bufferLength;
}

static void store_ovp_misr_data_to_relay_buffer(struct ovpDisplayTest *pOvpTest, int32_t *lenBufferRelay, char *bufferRelay, SetTuningOutputData_t *misrData)
{
    int32_t  bufferLength = *lenBufferRelay;
    uint8_t  i;
    char     errBufferRelay[MAX_RELAYFS_STRING_LENGTH];
    uint32_t lenErrBuffer = 0;


    if(pOvpTest->infoReq.isVsync)
        bufferLength += snprintf (bufferRelay  + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%16llx,", misrData->Data.Misr.LastVsyncTime);

    if(pOvpTest->infoReq.isVTGEvt)
    {
        if(misrData->Data.Misr.VTGEvt & STM_TIMING_EVENT_FRAME)
        {
            /* If STM_TIMING_EVENT_FRAME and STM_TIMING_EVENT_TOP_FIELD are set: This is a Top Field */
            if(misrData->Data.Misr.VTGEvt & STM_TIMING_EVENT_TOP_FIELD)
            {
                bufferLength += snprintf (bufferRelay  + bufferLength, MAX_RELAYFS_STRING_LENGTH, "T,");
            }
            /* If only STM_TIMING_EVENT_FRAME is set: This is a Frame */
            else
            {
                bufferLength += snprintf (bufferRelay  + bufferLength, MAX_RELAYFS_STRING_LENGTH, "F,");
            }
        }
        /*If only STM_TIMING_EVENT_BOTTOM_FIELD is set: This is a bottom field.*/
        if(misrData->Data.Misr.VTGEvt & STM_TIMING_EVENT_BOTTOM_FIELD)
        {
                bufferLength += snprintf (bufferRelay  + bufferLength, MAX_RELAYFS_STRING_LENGTH, "B,");
        }
    }

    for( i = 0; i < pOvpTest->misrData.nbrOfRequestedMisr; i++)
    {
        if(strlen(&pOvpTest->misrData.requestedMisrStr[i][0]))
        {
            if(misrData->Data.Misr.MisrValue[i].LostCnt)
            {
                lenErrBuffer = snprintf (errBufferRelay, MAX_RELAYFS_STRING_LENGTH, "ERR::%s new signatures lost since they were read after the time interval defined!!!\n",&pOvpTest->misrData.requestedMisrStr[i][0]);
                displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, lenErrBuffer, errBufferRelay, pOvpTest->relayFSIndex);
            }
            if(misrData->Data.Misr.MisrValue[i].Valid)
            {
                bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%8x,%8x,%8x,", misrData->Data.Misr.MisrValue[i].Reg1, misrData->Data.Misr.MisrValue[i].Reg2, misrData->Data.Misr.MisrValue[i].Reg3);
            }
            else
            {
                bufferLength += snprintf (bufferRelay + bufferLength, MAX_RELAYFS_STRING_LENGTH, "%8s,%8s,%8s,", "NONE","NONE","NONE");
            }
            *lenBufferRelay = bufferLength;
        }
    }
}

static void print_ovp_test_data(int32_t isCrcEmpty, int32_t isMisrEmpty, struct ovpDisplayTest *pOvpTest, char *bufferRelay, SetTuningOutputData_t *crcData, SetTuningOutputData_t *misrData)
{
    int32_t  lenBufferRelay = 0;

    if(isCrcEmpty == CIRCULAR_BUFFER_OK)
    {
        store_ovp_general_data_to_relay_buffer(pOvpTest, &lenBufferRelay, bufferRelay, crcData);
        store_ovp_crc_data_to_relay_buffer(pOvpTest, &lenBufferRelay, bufferRelay, crcData);
    }

    if(isMisrEmpty == CIRCULAR_BUFFER_OK)
    {
        store_ovp_misr_data_to_relay_buffer(pOvpTest, &lenBufferRelay, bufferRelay, misrData);
    }

    if((isCrcEmpty == CIRCULAR_BUFFER_OK) || (isMisrEmpty == CIRCULAR_BUFFER_OK))
    {
        lenBufferRelay += snprintf (bufferRelay  + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "\n");
        displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, lenBufferRelay, bufferRelay, pOvpTest->relayFSIndex);
    }
}

static int32_t store_ivp_test_header_to_relay_buffer(struct ivpDisplayTest *pIvpTest, int32_t bufferStartPtr, char *bufferRelay, int *crcParamsCnt)
{
    int32_t  lenBufferRelay = bufferStartPtr;
    char     *pCrcServiceStrCmd = NULL;
    int      paramsCounter;
    uint8_t  i, j;

    if(pIvpTest->infoReq.isVsync)
        lenBufferRelay  += snprintf (bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "SOURCE%dVsync,", pIvpTest->curSourceNo);

    if(pIvpTest->infoReq.isPTS)
        lenBufferRelay  += snprintf (bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "SOURCE%dPTS,", pIvpTest->curSourceNo);

    if(pIvpTest->infoReq.isPicID)
        lenBufferRelay += snprintf (bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "SOURCE%dPictureID,", pIvpTest->curSourceNo);

    if(pIvpTest->infoReq.isPicType)
        lenBufferRelay += snprintf (bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "SOURCE%dPictureType,", pIvpTest->curSourceNo);

    if(strlen(pIvpTest->srcCrcData.requestedCrcStr))
    {
        for(i = 0; i < MAX_CRC_CAP_ELEMENTS; i++)
        {
            pCrcServiceStrCmd = strstr(&(pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]), pIvpTest->srcCrcData.requestedCrcStr);
            if(pCrcServiceStrCmd != NULL)
            {
                lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "%s(", &pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]);
                i++;

                if ( i < MAX_CRC_CAP_ELEMENTS )
                  {
                    sscanf(&pIvpTest->srcCrcData.crcCapabilitiesStr[i][0], "%d", &paramsCounter);
                    *crcParamsCnt = paramsCounter;
                    for(j = 0; j < paramsCounter; j++)
                      {
                        i++;

                        if ( i < MAX_CRC_CAP_ELEMENTS )
                          {
                            lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "%s,", &(pIvpTest->srcCrcData.crcCapabilitiesStr[i][0]));
                          }
                      }
                    lenBufferRelay += snprintf(bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "),");
                  }
            }
        }
    }
    return lenBufferRelay;
}

static int32_t store_ivp_data_to_relay_buffer(struct ivpDisplayTest *pIvpTest, int32_t bufferStartPtr, char *bufferRelay, SetTuningOutputData_t *crc, int crcParamsCnt)
{
    int32_t  lenBufferRelay = bufferStartPtr;
    int      paramsCounter = crcParamsCnt;
    uint8_t  i;

    if(pIvpTest->infoReq.isVsync)
        lenBufferRelay = snprintf (bufferRelay, MAX_RELAYFS_STRING_LENGTH, "%16llx,", crc->Data.Crc.LastVsyncTime);

    if(pIvpTest->infoReq.isPTS)
        lenBufferRelay += snprintf (bufferRelay + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "%16llx,",crc->Data.Crc.PTS);

    if(pIvpTest->infoReq.isPicID)
        lenBufferRelay += snprintf (bufferRelay + lenBufferRelay , MAX_RELAYFS_STRING_LENGTH, "%8d,", crc->Data.Crc.PictureID);

    if(pIvpTest->infoReq.isPicType)
    {
        /* required by test frame work */
        if((crc->Data.Crc.PictureType != 'T')&&(crc->Data.Crc.PictureType != 'B')&&(crc->Data.Crc.PictureType != 'F'))
        {
            crc->Data.Crc.PictureType = 'U';
        }
        lenBufferRelay += snprintf (bufferRelay + lenBufferRelay , MAX_RELAYFS_STRING_LENGTH, "%c,", crc->Data.Crc.PictureType);
    }

    if(strlen(pIvpTest->srcCrcData.requestedCrcStr))
    {
        for(i = 0; i < paramsCounter; i++)
        {
            lenBufferRelay += snprintf (bufferRelay  + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "%8x,", crc->Data.Crc.CrcValue[i]);
        }
    }
    return lenBufferRelay;

}

void print_ovp_test_results(struct ovpDisplayTest *pOvpTest)
{
    int32_t isCrcEmpty = CIRCULAR_BUFFER_OK;
    int32_t isMisrEmpty = CIRCULAR_BUFFER_OK;
    SetTuningOutputData_t dataCrcOutput;
    SetTuningOutputData_t dataMisrOutput;
    uint32_t size = sizeof(SetTuningOutputData_t);

    char     *bufferRelay = NULL;
    int32_t  lenBufferRelay = 0;


    if(pOvpTest == NULL)
    {
        TRC(TRC_ID_ERROR, "ovpDisplayTest is NULL pointer!!!");
        return;
    }

    isCrcEmpty  = CircularBufferRead(pOvpTest->crcData.pCrcOutputCircularBuff, &dataCrcOutput, size);
    isMisrEmpty = CircularBufferRead(pOvpTest->misrData.pMisrOutputCircularBuff, &dataMisrOutput, size);

    if((isCrcEmpty != CIRCULAR_BUFFER_OK) && (isMisrEmpty != CIRCULAR_BUFFER_OK))
    {
        TRC(TRC_ID_COREDISPLAYTEST, "INFO: %s No CRC and MISR are collected.", pOvpTest->curPipelineName);
        return;
    }

    lenBufferRelay = 0;
    bufferRelay = kzalloc(MAX_RELAYFS_STRING_LENGTH, GFP_KERNEL);
    if(bufferRelay == NULL)
    {
        TRC(TRC_ID_ERROR, "Can't allocate memory to export data by strelay!!!");
        return;
    }

    if((isCrcEmpty == CIRCULAR_BUFFER_OK) || (isMisrEmpty == CIRCULAR_BUFFER_OK))
    {
        print_ovp_test_header(isCrcEmpty, isMisrEmpty, pOvpTest, bufferRelay);
    }

    while((isCrcEmpty == CIRCULAR_BUFFER_OK) || (isMisrEmpty == CIRCULAR_BUFFER_OK))
    {
        print_ovp_test_data(isCrcEmpty, isMisrEmpty, pOvpTest, bufferRelay, &dataCrcOutput, &dataMisrOutput);
        isCrcEmpty  = CircularBufferRead(pOvpTest->crcData.pCrcOutputCircularBuff, &dataCrcOutput, size);
        isMisrEmpty = CircularBufferRead(pOvpTest->misrData.pMisrOutputCircularBuff, &dataMisrOutput, size);
    }

    kfree(bufferRelay);
}

void print_ivp_test_results(struct ivpDisplayTest *pIvpTest)
{
    int32_t isCrcEmpty = CIRCULAR_BUFFER_OK;
    SetTuningOutputData_t dataCrcOutput;
    uint32_t size = sizeof(SetTuningOutputData_t);
    char     *bufferRelay = NULL;
    int32_t  lenBufferRelay = 0;
    int      crcParamsCnt = 0;

    if(pIvpTest == NULL)
    {
        TRC(TRC_ID_ERROR, "ivpDisplayTest is NULL pointer!!!");
        return;
    }

    isCrcEmpty = CircularBufferRead(pIvpTest->srcCrcData.pCrcOutputCircularBuff,  &dataCrcOutput, size);
    if(isCrcEmpty != CIRCULAR_BUFFER_OK)
    {
        TRC(TRC_ID_COREDISPLAYTEST, "INFO: No CRC is collected.");
        return;
    }

    lenBufferRelay = 0;
    bufferRelay = kzalloc(MAX_RELAYFS_STRING_LENGTH, GFP_KERNEL);
    if(bufferRelay == NULL)
    {
        TRC(TRC_ID_ERROR, "Can't allocate memories to export data by strelay!!!");
        return;
    }

    /*
     * print out the header of a group of crc data
     */
    lenBufferRelay = store_ivp_test_header_to_relay_buffer(pIvpTest, lenBufferRelay, bufferRelay, &crcParamsCnt);
    lenBufferRelay += snprintf (bufferRelay  + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "\n");
    displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, lenBufferRelay, bufferRelay, pIvpTest->relayFSIndex);

    /*
     * print out one group of crc data
     */
    while(isCrcEmpty == CIRCULAR_BUFFER_OK)
    {
        lenBufferRelay = 0;

        lenBufferRelay = store_ivp_data_to_relay_buffer(pIvpTest, lenBufferRelay, bufferRelay, &dataCrcOutput, crcParamsCnt);
        lenBufferRelay += snprintf (bufferRelay  + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "\n");
        displaytest_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, lenBufferRelay, bufferRelay, pIvpTest->relayFSIndex);

        isCrcEmpty  = CircularBufferRead(pIvpTest->srcCrcData.pCrcOutputCircularBuff,  &dataCrcOutput, size);
    }

    kfree(bufferRelay);
}

void displaytest_relayfs_write(int relayType, int32_t length, char *string, uint32_t index)
{
    char*    bufferRelay    = string;
    int32_t  lenBufferRelay = length;
    uint32_t relayFSIndex   = index;

    if(lenBufferRelay > MAX_RELAYFS_STRING_LENGTH)
    {
        TRC(TRC_ID_COREDISPLAYTEST, "Warning: The string length is %d above maximum length defined for relayfs %d!!!", lenBufferRelay, MAX_RELAYFS_STRING_LENGTH);
        TRC(TRC_ID_COREDISPLAYTEST, "Warning: The string above MAX_RELAYFS_STRING_LENGTH %d will be truncated!!!", MAX_RELAYFS_STRING_LENGTH);
        lenBufferRelay = MAX_RELAYFS_STRING_LENGTH -1;
        lenBufferRelay += snprintf (bufferRelay  + lenBufferRelay, MAX_RELAYFS_STRING_LENGTH, "\n");
    }
    switch(relayType)
    {
        case ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY:
        {
            st_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_TEST_CAPABILITY, ST_RELAY_SOURCE_COREDISPLAY_TEST + relayFSIndex, bufferRelay, lenBufferRelay, 0);
            break;
        }
        case ST_RELAY_TYPE_COREDISPLAY_CRC_MISR:
        {
            st_relayfs_write(ST_RELAY_TYPE_COREDISPLAY_CRC_MISR, ST_RELAY_SOURCE_COREDISPLAY_TEST + relayFSIndex, bufferRelay, lenBufferRelay, 0);
            break;
        }
        default:
            break;
    }
}


