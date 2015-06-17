/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description :  dwl common part
--
------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: dwl_linux.c,v $
--  $Revision: 1.54 $
--  $Date: 2011/01/17 13:18:22 $
--
------------------------------------------------------------------------------*/
/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.
************************************************************************/

#include "osdev_time.h"
#include "osdev_mem.h"

#include "dwl.h"
#include "vp8hard.h"

int hx170_check_chip_id()
{
	unsigned int *AsicBaseAdress_p = (unsigned int *)VP8_ASIC_BASE_ADDRESS;
	unsigned int regSize = VP8_REG_SIZE + 1;
	unsigned int *AsicBaseAdress_Cp;
	unsigned int           chip_id;
	int ret = 0;

	AsicBaseAdress_Cp = (unsigned int *)ioremap_nocache((unsigned int)AsicBaseAdress_p, regSize);

	chip_id = AsicBaseAdress_Cp[HX170_CHIP_ID_REG];

	switch (chip_id) {
	case HX170_IP_VERSION_V1:
	case HX170_IP_VERSION_V2:
	case HX170_IP_VERSION_V6:
		pr_info("%s: Chip ID: 0x%x\n", __func__, chip_id);
		break;

	default:
		pr_err("Error: %s Unknown chip ID: 0x%x\n", __func__, chip_id);
		ret = -(EINVAL);
		break;
	}

	iounmap((void *)AsicBaseAdress_Cp);
	return ret;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : unsigned int - the HW ID
------------------------------------------------------------------------------*/
unsigned int DWLReadAsicID()
{
	unsigned int *AsicBaseAdress_p = (unsigned int *)VP8_ASIC_BASE_ADDRESS;
	unsigned int regSize = VP8_REG_SIZE + 1;
	unsigned int *AsicBaseAdress_Cp;
	unsigned int id = 0xFFFFFFFF;

	AsicBaseAdress_Cp = (unsigned int *)ioremap_nocache((unsigned int)AsicBaseAdress_p, regSize);

	id = *AsicBaseAdress_Cp;

	iounmap((void *)AsicBaseAdress_Cp);
	return id;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicFuseStatus
    Description     : Read HW fuse configuration. Does not need a DWL instance to run

    Returns     : DWLHwFuseStatus_t * pHwFuseSts - structure with HW fuse configuration
------------------------------------------------------------------------------*/
void DWLReadAsicFuseStatus(DWLHwFuseStatus_t *pHwFuseSts)
{

	unsigned int *AsicBaseAdress_p = (unsigned int *)VP8_ASIC_BASE_ADDRESS;
	unsigned int regSize = VP8_REG_SIZE + 1;
	unsigned int *AsicBaseAdress_Cp;
	unsigned int configReg;
	unsigned int fuseReg;
	unsigned int fuseRegPp;

	memset(pHwFuseSts, 0, sizeof(*pHwFuseSts));

	AsicBaseAdress_Cp = (unsigned int *)ioremap_nocache((unsigned int)AsicBaseAdress_p, regSize);

	/* Decoder fuse configuration */
	fuseReg = AsicBaseAdress_Cp[HX170DEC_FUSE_CFG];

	pHwFuseSts->h264SupportFuse = (fuseReg >> DWL_H264_FUSE_E) & 0x01U;
	pHwFuseSts->mpeg4SupportFuse = (fuseReg >> DWL_MPEG4_FUSE_E) & 0x01U;
	pHwFuseSts->mpeg2SupportFuse = (fuseReg >> DWL_MPEG2_FUSE_E) & 0x01U;
	pHwFuseSts->sorensonSparkSupportFuse =
	        (fuseReg >> DWL_SORENSONSPARK_FUSE_E) & 0x01U;
	pHwFuseSts->jpegSupportFuse = (fuseReg >> DWL_JPEG_FUSE_E) & 0x01U;
	pHwFuseSts->vp6SupportFuse = (fuseReg >> DWL_VP6_FUSE_E) & 0x01U;
	pHwFuseSts->vc1SupportFuse = (fuseReg >> DWL_VC1_FUSE_E) & 0x01U;
	pHwFuseSts->jpegProgSupportFuse = (fuseReg >> DWL_PJPEG_FUSE_E) & 0x01U;
	pHwFuseSts->rvSupportFuse = (fuseReg >> DWL_RV_FUSE_E) & 0x01U;
	pHwFuseSts->avsSupportFuse = (fuseReg >> DWL_AVS_FUSE_E) & 0x01U;
	pHwFuseSts->vp7SupportFuse = (fuseReg >> DWL_VP7_FUSE_E) & 0x01U;
	pHwFuseSts->vp8SupportFuse = (fuseReg >> DWL_VP8_FUSE_E) & 0x01U;
	pHwFuseSts->customMpeg4SupportFuse = (fuseReg >> DWL_CUSTOM_MPEG4_FUSE_E) & 0x01U;
	pHwFuseSts->mvcSupportFuse = (fuseReg >> DWL_MVC_FUSE_E) & 0x01U;

	/* check max. decoder output width */
	if (fuseReg & 0x8000U) {
		pHwFuseSts->maxDecPicWidthFuse = 1920;
	} else if (fuseReg & 0x4000U) {
		pHwFuseSts->maxDecPicWidthFuse = 1280;
	} else if (fuseReg & 0x2000U) {
		pHwFuseSts->maxDecPicWidthFuse = 720;
	} else if (fuseReg & 0x1000U) {
		pHwFuseSts->maxDecPicWidthFuse = 352;
	}

	pHwFuseSts->refBufSupportFuse = (fuseReg >> DWL_REF_BUFF_FUSE_E) & 0x01U;

	/* Pp configuration */
	configReg = AsicBaseAdress_Cp[HX170PP_SYNTH_CFG];

	if ((configReg >> DWL_PP_E) & 0x01U) {
		/* Pp fuse configuration */
		fuseRegPp = AsicBaseAdress_Cp[HX170PP_FUSE_CFG];

		if ((fuseRegPp >> DWL_PP_FUSE_E) & 0x01U) {
			pHwFuseSts->ppSupportFuse = 1;

			/* check max. pp output width */
			if (fuseRegPp & 0x8000U) {
				pHwFuseSts->maxPpOutPicWidthFuse = 1920;
			} else if (fuseRegPp & 0x4000U) {
				pHwFuseSts->maxPpOutPicWidthFuse = 1280;
			} else if (fuseRegPp & 0x2000U) {
				pHwFuseSts->maxPpOutPicWidthFuse = 720;
			} else if (fuseRegPp & 0x1000U) {
				pHwFuseSts->maxPpOutPicWidthFuse = 352;
			}

			pHwFuseSts->ppConfigFuse = fuseRegPp;
		} else {
			pHwFuseSts->ppSupportFuse = 0;
			pHwFuseSts->maxPpOutPicWidthFuse = 0;
			pHwFuseSts->ppConfigFuse = 0;
		}
	}

	iounmap((void *)AsicBaseAdress_Cp);
}

/*--------------------------------------------------------------------------
    Function name   : DWLReadAsicConfig
    Description     : Read HW configuration.
----------------------------------------------------------------------------*/
void DWLReadAsicConfig(hX170dwl_config_t *pHwCfg)
{
	unsigned int *AsicBaseAdress_p = (unsigned int *)VP8_ASIC_BASE_ADDRESS;
	unsigned int regSize = VP8_REG_SIZE + 1;
	unsigned int *AsicBaseAdress_Cp;
	unsigned int configReg;
	unsigned int asicID;

	memset(pHwCfg, 0, sizeof(*pHwCfg));

	AsicBaseAdress_Cp = (unsigned int *)ioremap_nocache((unsigned int)AsicBaseAdress_p, regSize);


	/* Decoder configuration */
	configReg = AsicBaseAdress_Cp[HX170DEC_SYNTH_CFG];

	pHwCfg->h264Support = (configReg >> DWL_H264_E) & 0x3U;
	/* check jpeg */
	pHwCfg->jpegSupport = (configReg >> DWL_JPEG_E) & 0x01U;
	if (pHwCfg->jpegSupport && ((configReg >> DWL_PJPEG_E) & 0x01U)) {
		pHwCfg->jpegSupport = JPEG_PROGRESSIVE;
	}
	pHwCfg->mpeg4Support = (configReg >> DWL_MPEG4_E) & 0x3U;
	pHwCfg->vc1Support = (configReg >> DWL_VC1_E) & 0x3U;
	pHwCfg->mpeg2Support = (configReg >> DWL_MPEG2_E) & 0x01U;
	pHwCfg->sorensonSparkSupport = (configReg >> DWL_SORENSONSPARK_E) & 0x01U;
	pHwCfg->refBufSupport = (configReg >> DWL_REF_BUFF_E) & 0x01U;
	pHwCfg->vp6Support = (configReg >> DWL_VP6_E) & 0x01U;
#ifdef DEC_X170_APF_DISABLE
	if (DEC_X170_APF_DISABLE) {
		pHwCfg->tiledModeSupport = 0;
	}
#endif /* DEC_X170_APF_DISABLE */

	pHwCfg->maxDecPicWidth = configReg & 0x07FFU;

	/* 2nd Config register */
	configReg = AsicBaseAdress_Cp[HX170DEC_SYNTH_CFG_2];
	if (pHwCfg->refBufSupport) {
		if ((configReg >> DWL_REF_BUFF_ILACE_E) & 0x01U) {
			pHwCfg->refBufSupport |= 2;
		}
		if ((configReg >> DWL_REF_BUFF_DOUBLE_E) & 0x01U) {
			pHwCfg->refBufSupport |= 4;
		}
	}

	pHwCfg->customMpeg4Support = (configReg >> DWL_MPEG4_CUSTOM_E) & 0x01U;
	pHwCfg->vp7Support = (configReg >> DWL_VP7_E) & 0x01U;
	pHwCfg->vp8Support = (configReg >> DWL_VP8_E) & 0x01U;
	pHwCfg->avsSupport = (configReg >> DWL_AVS_E) & 0x01U;

	/* JPEG xtensions */
	asicID = DWLReadAsicID();
	if (((asicID >> 16) >= 0x8190U) ||
	    ((asicID >> 16) == 0x6731U)) {
		pHwCfg->jpegESupport = (configReg >> DWL_JPEG_EXT_E) & 0x01U;
	} else {
		pHwCfg->jpegESupport = JPEG_EXT_NOT_SUPPORTED;
	}

	if (((asicID >> 16) >= 0x9170U) ||
	    ((asicID >> 16) == 0x6731U)) {
		pHwCfg->rvSupport = (configReg >> DWL_RV_E) & 0x03U;
	} else {
		pHwCfg->rvSupport = RV_NOT_SUPPORTED;
	}

	pHwCfg->mvcSupport = (configReg >> DWL_MVC_E) & 0x03U;

	pHwCfg->webpSupport = (configReg >> DWL_WEBP_E) & 0x01U;
	pHwCfg->tiledModeSupport = (configReg >> DWL_DEC_TILED_L) & 0x03U;

	if (pHwCfg->refBufSupport &&
	    (asicID >> 16) == 0x6731U) {
		pHwCfg->refBufSupport |= 8; /* enable HW support for offset */
	}

	/* Pp configuration */
	configReg = AsicBaseAdress_Cp[HX170PP_SYNTH_CFG];

	if ((configReg >> DWL_PP_E) & 0x01U) {
		pHwCfg->ppSupport = 1;
		pHwCfg->maxPpOutPicWidth = configReg & 0x07FFU;
		/*pHwCfg->ppConfig = (configReg >> DWL_CFG_E) & 0x0FU; */
		pHwCfg->ppConfig = configReg;
	} else {
		pHwCfg->ppSupport = 0;
		pHwCfg->maxPpOutPicWidth = 0;
		pHwCfg->ppConfig = 0;
	}

	/* check the HW version */
	if (((asicID >> 16) >= 0x8190U) ||
	    ((asicID >> 16) == 0x6731U)) {
		unsigned int deInterlace;
		unsigned int alphaBlend;
		unsigned int deInterlaceFuse;
		unsigned int alphaBlendFuse;
		DWLHwFuseStatus_t hwFuseSts;

		/* check fuse status */
		DWLReadAsicFuseStatus(&hwFuseSts);

		/* Maximum decoding width supported by the HW */
		if (pHwCfg->maxDecPicWidth > hwFuseSts.maxDecPicWidthFuse) {
			pHwCfg->maxDecPicWidth = hwFuseSts.maxDecPicWidthFuse;
		}
		/* Maximum output width of Post-Processor */
		if (pHwCfg->maxPpOutPicWidth > hwFuseSts.maxPpOutPicWidthFuse) {
			pHwCfg->maxPpOutPicWidth = hwFuseSts.maxPpOutPicWidthFuse;
		}
		/* h264 */
		if (!hwFuseSts.h264SupportFuse) {
			pHwCfg->h264Support = H264_NOT_SUPPORTED;
		}
		/* mpeg-4 */
		if (!hwFuseSts.mpeg4SupportFuse) {
			pHwCfg->mpeg4Support = MPEG4_NOT_SUPPORTED;
		}
		/* custom mpeg-4 */
		if (!hwFuseSts.customMpeg4SupportFuse) {
			pHwCfg->customMpeg4Support = MPEG4_CUSTOM_NOT_SUPPORTED;
		}
		/* jpeg (baseline && progressive) */
		if (!hwFuseSts.jpegSupportFuse) {
			pHwCfg->jpegSupport = JPEG_NOT_SUPPORTED;
		}
		if ((pHwCfg->jpegSupport == JPEG_PROGRESSIVE) &&
		    !hwFuseSts.jpegProgSupportFuse) {
			pHwCfg->jpegSupport = JPEG_BASELINE;
		}
		/* mpeg-2 */
		if (!hwFuseSts.mpeg2SupportFuse) {
			pHwCfg->mpeg2Support = MPEG2_NOT_SUPPORTED;
		}
		/* vc-1 */
		if (!hwFuseSts.vc1SupportFuse) {
			pHwCfg->vc1Support = VC1_NOT_SUPPORTED;
		}
		/* vp6 */
		if (!hwFuseSts.vp6SupportFuse) {
			pHwCfg->vp6Support = VP6_NOT_SUPPORTED;
		}
		/* vp7 */
		if (!hwFuseSts.vp7SupportFuse) {
			pHwCfg->vp7Support = VP7_NOT_SUPPORTED;
		}
		/* vp8 */
		if (!hwFuseSts.vp8SupportFuse) {
			pHwCfg->vp8Support = VP8_NOT_SUPPORTED;
		}
		/* pp */
		if (!hwFuseSts.ppSupportFuse) {
			pHwCfg->ppSupport = PP_NOT_SUPPORTED;
		}
		/* check the pp config vs fuse status */
		if ((pHwCfg->ppConfig & 0xFC000000) &&
		    ((hwFuseSts.ppConfigFuse & 0xF0000000) >> 5)) {
			/* config */
			deInterlace = ((pHwCfg->ppConfig & PP_DEINTERLACING) >> 25);
			alphaBlend = ((pHwCfg->ppConfig & PP_ALPHA_BLENDING) >> 24);
			/* fuse */
			deInterlaceFuse =
			        (((hwFuseSts.ppConfigFuse >> 5) & PP_DEINTERLACING) >> 25);
			alphaBlendFuse =
			        (((hwFuseSts.ppConfigFuse >> 5) & PP_ALPHA_BLENDING) >> 24);

			/* check if */
			if (deInterlace && !deInterlaceFuse) {
				pHwCfg->ppConfig &= 0xFD000000;
			}
			if (alphaBlend && !alphaBlendFuse) {
				pHwCfg->ppConfig &= 0xFE000000;
			}
		}
		/* sorenson */
		if (!hwFuseSts.sorensonSparkSupportFuse) {
			pHwCfg->sorensonSparkSupport = SORENSON_SPARK_NOT_SUPPORTED;
		}
		/* ref. picture buffer */
		if (!hwFuseSts.refBufSupportFuse) {
			pHwCfg->refBufSupport = REF_BUF_NOT_SUPPORTED;
		}

		/* rv */
		if (!hwFuseSts.rvSupportFuse) {
			pHwCfg->rvSupport = RV_NOT_SUPPORTED;
		}
		/* avs */
		if (!hwFuseSts.avsSupportFuse) {
			pHwCfg->avsSupport = AVS_NOT_SUPPORTED;
		}
		/* mvc */
		if (!hwFuseSts.mvcSupportFuse) {
			pHwCfg->mvcSupport = MVC_NOT_SUPPORTED;
		}
	}

	pr_info("AsicFuse : vp8Support = %d;\n maxDecPicWidth = %d\n",
	        pHwCfg->vp8Support, pHwCfg->maxDecPicWidth);
	iounmap((void *)AsicBaseAdress_Cp);

}

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : DWLInitParam_t * param - initialization params
------------------------------------------------------------------------------*/
void *DWLInit(unsigned int clientType)
{
	hX170dwl_t *dec_dwl;
	unsigned long base;

	dec_dwl = (hX170dwl_t *) OSDEV_Malloc(sizeof(hX170dwl_t));

	if (dec_dwl == NULL) {
		pr_err("Init: failed to allocate an instance\n");
		return NULL;
	}

	dec_dwl->clientType = clientType;

	switch (dec_dwl->clientType) {
	case DWL_CLIENT_TYPE_H264_DEC:
	case DWL_CLIENT_TYPE_MPEG4_DEC:
	case DWL_CLIENT_TYPE_JPEG_DEC:
	case DWL_CLIENT_TYPE_PP:
	case DWL_CLIENT_TYPE_VC1_DEC:
	case DWL_CLIENT_TYPE_MPEG2_DEC:
	case DWL_CLIENT_TYPE_VP6_DEC:
	case DWL_CLIENT_TYPE_VP8_DEC:
	case DWL_CLIENT_TYPE_RV_DEC:
	case DWL_CLIENT_TYPE_AVS_DEC:
		break;
	default:
		pr_err("DWL: Unknown client type no. %d\n", dec_dwl->clientType);
		goto err;
	}

	base = VP8_REG_BASE_ADD;
	dec_dwl->regSize = VP8_REG_SIZE + 1;

	/* map the hw registers to user space */
	dec_dwl->pRegBase = (unsigned int *)ioremap_nocache((unsigned int) base, dec_dwl->regSize);
	pr_info("DWL: regs size %d bytes, virt 0x%08x\n", dec_dwl->regSize,
	        (unsigned int) dec_dwl->pRegBase);

	sema_init(&dec_dwl->semid, 1);
	sema_init(&dec_dwl->Dec_End_sem, 0);

	return dec_dwl;

err:
	DWLRelease(dec_dwl);
	return NULL;
}

void DWLClearSwRegister(void *instance)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	assert(dec_dwl != NULL);
	dec_dwl->pRegBase[1] = dec_dwl->pRegBase[1] & ~0x100; // Clear Bit 8 in swReg1
	up(&dec_dwl->Dec_End_sem);
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : int - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
int DWLRelease(const void *instance)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	assert(dec_dwl != NULL);

	if (dec_dwl == NULL) {
		return DWL_ERROR;
	}

	if (dec_dwl->pRegBase != NULL) {
		iounmap((void *)dec_dwl->pRegBase);
	}

	OSDEV_Free((void *)dec_dwl);
	dec_dwl = NULL;
	return (DWL_OK);
}


/*------------------------------------------------------------------------------
    Function name   : DWLReserveHw
    Description     :
    Return type     : signed int
    Argument        : const void *instance
------------------------------------------------------------------------------*/
signed int DWLReserveHw(const void *instance)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;
	if (dec_dwl->clientType == DWL_CLIENT_TYPE_PP) {
		pr_debug("DWL: PP locked\n");
	} else {
		pr_debug("DWL: Dec locked\n");
	}
	down(&dec_dwl->semid);

	pr_debug("DWL: success\n");
	return DWL_OK;
}

/* ---------------------------------------------------- */

unsigned int GetDecRegister(const unsigned int *regBase, unsigned int id)
{
	unsigned int tmp;

	assert(id < HWIF_LAST_REG);

	tmp = regBase[hwDecRegSpec[id][0]];
	tmp = tmp >> hwDecRegSpec[id][2];
	tmp &= regMask[hwDecRegSpec[id][1]];
	return (tmp);
}


/* ---------------------------------------------------- */

void SetDecRegister(unsigned int *regBase, unsigned int id, unsigned int value)
{
	unsigned int tmp;

	assert(id < HWIF_LAST_REG);

	tmp = regBase[hwDecRegSpec[id][0]];
	tmp &= ~(regMask[hwDecRegSpec[id][1]] << hwDecRegSpec[id][2]);
	tmp |= (value & regMask[hwDecRegSpec[id][1]]) << hwDecRegSpec[id][2];
	regBase[hwDecRegSpec[id][0]] = tmp;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteReg
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : unsigned int offset - byte offset of the register to be written
    Argument        : unsigned int value - value to be written out
------------------------------------------------------------------------------*/
void DWLWriteReg(const void *instance, unsigned int offset, unsigned int value)
{
	/*SE : I keep the original definition of this function, there is no need to change it
	       Rq : The Register zone should be mapped before calling this function             */
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;
	assert(dec_dwl != NULL);

	pr_debug("DWL: Write reg %d at offset 0x%02X --> %08X\n", offset / 4,
	         offset, value);

	assert((dec_dwl->clientType != DWL_CLIENT_TYPE_PP &&
	        offset < HX170PP_REG_START) ||
	       (dec_dwl->clientType == DWL_CLIENT_TYPE_PP &&
	        offset >= HX170PP_REG_START));

	assert(offset < dec_dwl->regSize);

	offset = offset / 4;
	*(dec_dwl->pRegBase + offset) = value;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitHwReady
    Description     : Wait until hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.

    Return type     : signed int - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
signed int DWLWaitHwReady(const void *instance, unsigned int timeout)
{
	const hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	signed int ret;

	assert(dec_dwl);

	switch (dec_dwl->clientType) {
	case DWL_CLIENT_TYPE_H264_DEC:
	case DWL_CLIENT_TYPE_MPEG4_DEC:
	case DWL_CLIENT_TYPE_JPEG_DEC:
	case DWL_CLIENT_TYPE_VC1_DEC:
	case DWL_CLIENT_TYPE_MPEG2_DEC:
	case DWL_CLIENT_TYPE_RV_DEC:
	case DWL_CLIENT_TYPE_VP6_DEC:
	case DWL_CLIENT_TYPE_VP8_DEC:
	case DWL_CLIENT_TYPE_AVS_DEC: {
		ret = DWLWaitDecHwReady(dec_dwl, timeout);
		break;
	}
	case DWL_CLIENT_TYPE_PP: {
		ret = DWLWaitPpHwReady(dec_dwl, timeout);
		break;
	}
	default: {
		assert(0);  /* should not happen */
		ret = DWL_HW_WAIT_ERROR;
	}
	}

	return ret;
}

signed int DWLWaitDecHwReady(const void *instance, unsigned int timeout)
{
	return DWLWaitPpHwIt(instance, timeout);
}

signed int DWLWaitPpHwIt(const void *instance, unsigned int timeout)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	assert(dec_dwl != NULL);

	down(&dec_dwl->Dec_End_sem);
	return DWL_HW_WAIT_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitHwReady
    Description     : Wait until hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait can succeed or timeout.

    Return type     : signed int - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT

    Argument        : const void * instance - DWL instance
                      unsigned int timeout - timeout period for the wait specified in
                                milliseconds; 0 will perform a poll of the
                                hardware status and -1 means an infinit wait
------------------------------------------------------------------------------*/

signed int DWLWaitPpHwReady(const void *instance, unsigned int timeout)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;
	volatile unsigned int irq_stats;
	unsigned int irqRegOffset;

	assert(dec_dwl != NULL);

	if (dec_dwl->clientType == DWL_CLIENT_TYPE_PP) {
		irqRegOffset = HX170PP_REG_START;        /* pp ctrl reg offset */
	} else {
		irqRegOffset = HX170DEC_REG_START;        /* decoder ctrl reg offset */
	}

	/* wait for decoder */
#ifdef _READ_DEBUG_REGS
	(void) DWLReadReg(dec_dwl, 40 * 4);
	(void) DWLReadReg(dec_dwl, 41 * 4);
#endif
	irq_stats = DWLReadReg(dec_dwl, irqRegOffset);
	irq_stats = (irq_stats >> 12) & 0xFF;

	if (irq_stats != 0) {
		return DWL_HW_WAIT_OK;
	} else if (timeout) {
		unsigned int sleep_time = 0;
		int forever = 0;
		int loop = 1;
		signed int ret = DWL_HW_WAIT_TIMEOUT;
		int polling_interval_milli_sec = 1;   /* 1ms polling interval */

		if (timeout == (unsigned int)(-1)) {
			forever = 1;    /* wait forever */
		}

		do {
			OSDEV_SleepMilliSeconds(polling_interval_milli_sec);


#ifdef _READ_DEBUG_REGS
			(void) DWLReadReg(dec_dwl, 40 * 4);
			(void) DWLReadReg(dec_dwl, 41 * 4);
			(void) DWLReadReg(dec_dwl, 42 * 4);
			(void) DWLReadReg(dec_dwl, 43 * 4);
#endif

			irq_stats = DWLReadReg(dec_dwl, irqRegOffset);
			irq_stats = (irq_stats >> 12) & 0xFF;

			if (irq_stats != 0) {
				ret = DWL_HW_WAIT_OK;
				loop = 0;   /* end the polling loop */
			}
			/* if not a forever wait */
			else if (!forever) {
				sleep_time += polling_interval_milli_sec;

				if (sleep_time >= timeout) {
					ret = DWL_HW_WAIT_TIMEOUT;
					loop = 0;
				}
			}
			pr_debug("Loop for HW timeout. Total sleep: %dms\n", sleep_time);
		} while (loop);

		return ret;
	} else {
		return DWL_HW_WAIT_TIMEOUT;
		/*Application don't want to wait, just read the register and return OK or TIMEOUT*/
	}
}
/*------------------------------------------------------------------------------
    Function name   : DWLReadReg
    Description     : Read the value of a hardware IO register

    Return type     : unsigned int - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : unsigned int offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
unsigned int DWLReadReg(const void *instance, unsigned int offset)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;
	unsigned int val;

	assert(dec_dwl != NULL);
	assert((dec_dwl->clientType != DWL_CLIENT_TYPE_PP &&
	        offset < HX170PP_REG_START) ||
	       (dec_dwl->clientType == DWL_CLIENT_TYPE_PP &&
	        offset >= HX170PP_REG_START) || (offset == 0) ||
	       (offset == HX170PP_SYNTH_CFG));

	assert(offset < dec_dwl->regSize);

	offset = offset / 4;
	val = *(dec_dwl->pRegBase + offset);

	pr_debug("DWL: Read reg %d at offset 0x%02X --> %08X\n", offset,
	         offset * 4, val);

	return val;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
void DWLReleaseHw(const void *instance)
{
	hX170dwl_t *dec_dwl = (hX170dwl_t *) instance;

	/* dec_dwl->clientType -- identifies the client type */
	up(&dec_dwl->semid);
	pr_debug("DWL: HW released\n");
}

