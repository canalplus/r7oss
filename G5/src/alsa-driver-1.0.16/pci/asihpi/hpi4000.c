/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  Hardware Programming Interface (HPI) for AudioScience ASI4500 and 4100
  series adapters. These PCI bus adapters are based on the Motorola DSP56301
  DSP with on-chip PCI I/F.

  Exported functions:
  void HPI_4000(struct hpi_message *phm, struct hpi_response *phr)
******************************************************************************/
/* #define DEBUG */
/*#define USE_ASM_DATA */
/*#define USE_ASM_MSG */

/* writes to I/O port 0x182! */
/*#define TIMING_DEBUG */

#define SOURCEFILE_NAME "hpi4000.c"
#include "hpi.h"
#include "hpidebug.h"
#include "hpicmn.h"
#include "hpi4000.h"
#include "hpipci.h"
#include "hpidspcd.h"

#ifdef ASI_DRV_DEBUG
#include "asidrv.h"
#endif

#ifndef __linux__
#pragma hdrstop			/* allow headers above here to be precompiled */
#endif

#define DPI_ERROR		900	/* non-specific error */
#define DPI_ERROR_SEND		910
#define DPI_ERROR_GET		950
#define DPI_ERROR_DOWNLOAD	930

struct hpi_hw_obj {
	__iomem u32 *pMemBase;
	u32 dwHCTR;
	u32 dwHCVR;
};

static void SubSysCreateAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void SubSysDeleteAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
);
static short CreateAdapterObj(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
);

static short CheckAdapterPresent(
	struct hpi_hw_obj *pio
);
static short AdapterBootLoadDsp(
	struct hpi_adapter_obj *pao,
	struct hpi_hw_obj *pio,
	u32 *pdwOsErrorCode
);
static void MessageResponseSequence(
	struct hpi_hw_obj *pio,
	struct hpi_message *phm,
	struct hpi_response *phr
);

/** DSP56301 Pci Boot code (multi-segment)
 * The following code came from boot4000.asm, via makeboot.bat
 */
static u32 adwDspCode_Boot4000a[] = {

/**** P memory	****/
/*	Length	*/ 0x0000008B,
/*	Address */ 0x00000F60,
/*Type:P,X,Y*/ 0x00000004,
/* 00000F60 */ 0x00240000, 0x0004C4BB, 0x0004C4B1, 0x000003F8,
	0x0061F400,
	0x00000048, 0x0054F400, 0x000BF080, 0x0007598C, 0x0054F400,
/* 00000F6A */ 0x00000FDE, 0x0007618C, 0x0008F4B9, 0x00100739,
	0x0008F4B7,
	0x00800839, 0x0008F4B6, 0x0040080A, 0x0008F4B8, 0x00200739,
/* 00000F74 */ 0x0008F4BB, 0x001FFFE2, 0x0046F400, 0x00123456,
	0x0060F400,
	0x00100000, 0x0061F400, 0x00110000, 0x00000000, 0x00000000,
/* 00000F7E */ 0x00446100, 0x00466000, 0x000200C4, 0x0056E000,
	0x00200055,
	0x000D104A, 0x00000005, 0x002C0300, 0x000D10C0, 0x0000001F,
/* 00000F88 */ 0x0056E100, 0x00200055, 0x000D1042, 0x00000005,
	0x002C0700,
	0x000D10C0, 0x00000018, 0x00200013, 0x0020001B, 0x00209500,
/* 00000F92 */ 0x0007F08D, 0x00000102, 0x000140CD, 0x00004300,
	0x000D104A,
	0x0000000B, 0x000140CD, 0x00004600, 0x000D104A, 0x00000004,
/* 00000F9C */ 0x000D10C0, 0x00000008, 0x002C0600, 0x000D10C0,
	0x00000006,
	0x002C0200, 0x000D10C0, 0x00000003, 0x002C0400, 0x000C1E86,
/* 00000FA6 */ 0x00000000, 0x00084405, 0x00200042, 0x0008CC05,
	0x00200013,
	0x000A8982, 0x00000FAB, 0x00084F0B, 0x000140CD, 0x00FFFFFF,
/* 00000FB0 */ 0x000AF0AA, 0x00000FD8, 0x000A8982, 0x00000FB2,
	0x0008500B,
	0x00221100, 0x000A8982, 0x00000FB6, 0x00084E0B, 0x0006CD00,
/* 00000FBA */ 0x00000FD6, 0x000A8982, 0x00000FBB, 0x00014485,
	0x000AF0A2,
	0x00000FC3, 0x0008584B, 0x000AF080, 0x00000FD6, 0x00014185,
/* 00000FC4 */ 0x000AF0A2, 0x00000FC9, 0x0008588B, 0x000AF080,
	0x00000FD6,
	0x00014285, 0x000AF0A2, 0x00000FCD, 0x000858CB, 0x00014385,
/* 00000FCE */ 0x000AF0A2, 0x00000FD6, 0x000CFFA0, 0x00000005,
	0x0008608B,
	0x000D10C0, 0x00000003, 0x000858CB, 0x00000000, 0x000C0FAA,
/* 00000FD8 */ 0x00084E05, 0x000140C6, 0x00FFFFC7, 0x0008CC05,
	0x000000B9,
	0x000C0000, 0x0008F498, 0x00000000, 0x00000000, 0x00000000,
/* 00000FE2 */ 0x0008F485, 0x00000000, 0x00000000, 0x00000000,
	0x00000084,
	0x00000000, 0x00000000, 0x000AF080, 0x00FF0000,
/* End-Of-Array */ 0xFFFFFFFF
};

static inline void HW_Message(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HpiOs_Dsplock_Lock(pao);
	MessageResponseSequence((struct hpi_hw_obj *)pao->priv, phm, phr);
	HpiOs_Dsplock_UnLock(pao);
	/* errors fatal to this interface module */
	if ((phr->wError >= DPI_ERROR) && (phr->wError <= 1000))
		HPI_DEBUG_LOG(ERROR, DBG_TEXT("HPI Response - error# %d\n"),
			phr->wError);

}

static void SubSysMessage(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wFunction) {
	case HPI_SUBSYS_OPEN:
	case HPI_SUBSYS_CLOSE:
	case HPI_SUBSYS_GET_INFO:
	case HPI_SUBSYS_DRIVER_UNLOAD:
	case HPI_SUBSYS_DRIVER_LOAD:
	case HPI_SUBSYS_FIND_ADAPTERS:
		/* messages that should not get here */
		phr->wError = HPI_ERROR_UNIMPLEMENTED;
		break;
	case HPI_SUBSYS_CREATE_ADAPTER:
		SubSysCreateAdapter(phm, phr);
		break;
	case HPI_SUBSYS_DELETE_ADAPTER:
		SubSysDeleteAdapter(phm, phr);
		break;
	default:
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	}
}

static void AdapterMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wFunction) {
	case HPI_ADAPTER_GET_INFO:
		HW_Message(pao, phm, phr);
		break;
	case HPI_ADAPTER_GET_ASSERT:
		HW_Message(pao, phm, phr);
		phr->u.a.wAdapterType = 0;
		break;
	case HPI_ADAPTER_OPEN:
	case HPI_ADAPTER_CLOSE:
	case HPI_ADAPTER_TEST_ASSERT:
	case HPI_ADAPTER_SELFTEST:
	case HPI_ADAPTER_GET_MODE:
	case HPI_ADAPTER_SET_MODE:
		HW_Message(pao, phm, phr);
		break;
	case HPI_ADAPTER_FIND_OBJECT:
		HPI_InitResponse(phr, HPI_OBJ_ADAPTER,
			HPI_ADAPTER_FIND_OBJECT, 0);
		/* really DSP index in this context */
		phr->u.a.wAdapterIndex = 0;
		break;
	default:
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	}
}

static void MixerMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wFunction) {
	case HPI_MIXER_OPEN:
		/* experiment with delay to allow settling of D/As on adapter */
		/* before enabling mixer and so outputs */
		HpiOs_DelayMicroSeconds(500000L);	/*500ms */
		HW_Message(pao, phm, phr);
		return;
	default:
		HW_Message(pao, phm, phr);
		return;
	}
}

static void OStreamMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wFunction) {
	case HPI_OSTREAM_HOSTBUFFER_ALLOC:
	case HPI_OSTREAM_HOSTBUFFER_FREE:
		/* Don't let these messages go to the HW function because
		 * they're called without taking the spinlock.
		 * For the HPI4000 adapters the HW would return
		 * HPI_ERROR_INVALID_FUNC anyway.
		 */
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	default:
		HW_Message(pao, phm, phr);
		return;
	}
}

static void IStreamMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	switch (phm->wFunction) {
	case HPI_ISTREAM_HOSTBUFFER_ALLOC:
	case HPI_ISTREAM_HOSTBUFFER_FREE:
		/* Don't let these messages go to the HW function because
		 * they're called without taking the spinlock.
		 * For the HPI4000 adapters the HW would return
		 * HPI_ERROR_INVALID_FUNC anyway.
		 */
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	default:
		HW_Message(pao, phm, phr);
		return;
	}
}

/** Entry point
 * All calls to the HPI start here
 */
void HPI_4000(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_adapter_obj *pao = NULL;

	HPI_DEBUG_LOG(DEBUG, "O %d,F %d\n", phm->wObject, phm->wFunction);

	if (phm->wObject != HPI_OBJ_SUBSYSTEM) {
		pao = HpiFindAdapter(phm->wAdapterIndex);
		if (!pao) {
			HPI_DEBUG_LOG(DEBUG,
				" %d,%d refused, for another HPI?\n",
				phm->wObject, phm->wFunction);
			return;
		}
		if (pao->wDspCrashed) {
			/* then don't communicate with it any more */
			HPI_InitResponse(phr, phm->wObject, phm->wFunction,
				HPI_ERROR_DSP_HARDWARE);
			HPI_DEBUG_LOG(DEBUG, " %d,%d dsp crashed.\n",
				phm->wObject, phm->wFunction);
			return;
		}
	}
	/* Init default response */
	if (phm->wFunction != HPI_SUBSYS_CREATE_ADAPTER)
		HPI_InitResponse(phr, phm->wObject, phm->wFunction,
			HPI_ERROR_PROCESSING_MESSAGE);

	HPI_DEBUG_LOG(VERBOSE, "start of switch\n");
	switch (phm->wType) {
	case HPI_TYPE_MESSAGE:
		switch (phm->wObject) {
		case HPI_OBJ_SUBSYSTEM:
			SubSysMessage(phm, phr);
			break;

		case HPI_OBJ_ADAPTER:
			phr->wSize = sizeof(struct hpi_response_header) +
				sizeof(struct hpi_adapter_res);
			AdapterMessage(pao, phm, phr);
			break;

		case HPI_OBJ_MIXER:
			MixerMessage(pao, phm, phr);
			break;

		case HPI_OBJ_OSTREAM:
			OStreamMessage(pao, phm, phr);
			break;

		case HPI_OBJ_ISTREAM:
			IStreamMessage(pao, phm, phr);
			break;

		default:
			HW_Message(pao, phm, phr);
			break;
		}
		break;

	default:
		phr->wError = HPI_ERROR_INVALID_TYPE;
		break;
	}
}

/* SUBSYSTEM */

/** create an adapter object and initialise it based on resource information
 * passed in in the message
 **** NOTE - you cannot use this function AND the FindAdapters function at the
 * same time, the application must use only one of them to get the adapters
 */
static void SubSysCreateAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	/* create temp adapter obj, because we don't know what index yet */
	struct hpi_adapter_obj ao;
	u32 dwOsErrorCode;
	short nError = 0;

	HPI_DEBUG_LOG(VERBOSE, "SubSysCreateAdapter\n");

	memset(&ao, 0, sizeof(struct hpi_adapter_obj));

	if (phm->u.s.Resource.wBusType != HPI_BUS_PCI)
		return;
	if (phm->u.s.Resource.r.Pci->wVendorId != HPI_PCI_VENDOR_ID_MOTOROLA)
		return;

	ao.priv = kmalloc(sizeof(struct hpi_hw_obj), GFP_KERNEL);
	memset(ao.priv, 0, sizeof(struct hpi_hw_obj));
	/* create the adapter object based on the resource information */
	/*? memcpy(&ao.Pci,&phm->u.s.Resource.r.Pci,sizeof(ao.Pci)); */
	ao.Pci = *phm->u.s.Resource.r.Pci;

	((struct hpi_hw_obj *)ao.priv)->pMemBase = ao.Pci.apMemBase[0];

	nError = CreateAdapterObj(&ao, &dwOsErrorCode);
	if (!nError)
		nError = HpiAddAdapter(&ao);

	if (nError) {
		phr->u.s.dwData = dwOsErrorCode;
		kfree(ao.priv);
		phr->wError = nError;
		return;
	}

	phr->u.s.awAdapterList[ao.wIndex] = ao.wAdapterType;
	phr->u.s.wAdapterIndex = ao.wIndex;
	phr->u.s.wNumAdapters++;
	phr->wError = 0;
}

static void SubSysDeleteAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_adapter_obj *pao = NULL;
	void *priv;

	pao = HpiFindAdapter(phm->wAdapterIndex);
	if (!pao)
		return;

	priv = pao->priv;
	HpiDeleteAdapter(pao);
	kfree(priv);
	phr->wError = 0;
}

/** called from either SubSysFindAdapter or SubSysCreateAdapter */
static short CreateAdapterObj(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
)
{
	short nBootError = 0;

	pao->wOpen = 0;

	/* Is it really a 301 chip? */
	if (CheckAdapterPresent((struct hpi_hw_obj *)pao->priv))
		return (HPI_ERROR_BAD_ADAPTER);	/*error */

	HPI_DEBUG_LOG(VERBOSE, "CreateAdapterObj - Adapter present OK\n");

	nBootError =
		AdapterBootLoadDsp(pao, (struct hpi_hw_obj *)pao->priv,
		pdwOsErrorCode);
	if (nBootError)
		return (nBootError);	/*error */

	HPI_DEBUG_LOG(INFO, "Bootload DSP OK\n");

	/* Get type and index from adapter */
	{
		struct hpi_message hM;
		struct hpi_response hR;

		HPI_DEBUG_LOG(VERBOSE,
			"CreateAdapterObj - Send ADAPTER_GET_INFO\n");
		memset(&hM, 0, sizeof(struct hpi_message));
		hM.wType = HPI_TYPE_MESSAGE;
		hM.wSize = sizeof(struct hpi_message);
		hM.wObject = HPI_OBJ_ADAPTER;
		hM.wFunction = HPI_ADAPTER_GET_INFO;
		hM.wAdapterIndex = 0;
		memset(&hR, 0, sizeof(struct hpi_response));
		hR.wSize = sizeof(struct hpi_response);

		MessageResponseSequence((struct hpi_hw_obj *)pao->priv, &hM,
			&hR);

		if (hR.wError) {
			HPI_DEBUG_LOG(VERBOSE, "HPI4000.C - message error\n");
			return (hR.wError);	/*error */
		}

		pao->wAdapterType = hR.u.a.wAdapterType;
		pao->wIndex = hR.u.a.wAdapterIndex;
	}
	HPI_DEBUG_LOG(VERBOSE, "CreateAdapterObj - Get adapter info OK\n");
	return (0);		/*success! */
}

/************************************************************************
HPI56301 LOWLEVEL CODE

Errata Notes
------------

ED46
Description (added 12/10/2001):

The following sequence gives erroneous results:
1) A different slave on the bus terminates a transaction (for
example, assertion of "stop").
2) Immediately afterwards (no more than one PCI clock), the chips
memory space control/status register at PCI address ADDR is read
in a single-word transaction. In this transaction, the chip drives to
the bus the data corresponding to the register at PCI address
ADDR+4, instead of the requested ADDR.

NOTE: ADDR is the PCI address of one of the following registers:
HCTR (ADDR=$10) , HSTR (ADDR=$14), or HCVR (ADDR=$18),
and not the data register.
Workaround:
The user should find a way to set/clear at least one bit in the control/status
registers to clearly differentiate between them. For example, you can set
HNMI in the HCVR, as this bit will always be 0 in the HSTR. If NMI
cannot be used, then HCVR{HV4,HV3,HV2} and
HSTR{HF5,HF4,HF3} can be set in any combinations that distinguish
between HCVR and HSTR data reads.
Pertains to:
DSP56301 Users Manual: Put this errata text as a note in the description
of the HCTR (p.
6-48), the HSTR (p. 6-57), and the HCVR (p. 6-59). These page numbers
are for Revision 3 of the manual.
DSP56305 Users Manual: Put this errata text as a note in the description
of the HCTR (p. 6-54), the HSTR (p. 6-68), and the HCVR (p. 6-72).
These page numbers are for Revision 1of the manual.

AudioScience workaround
-----------------------

1.	Save a copy of HCTR so it never needs to be read.

2.	Whenever we write HCVR set the CVR_NMI bit. See Dpi_SafeWriteHCVR().
	Whenever we read HSTR we check that CVR_NMI from HCVR is not set.
	See Dpi_SafeReadHSTR().

3.	Whenever we read HCVR we make sure it matches the last value written
	(except possibly the CVR_INT bit). See Dpi_SafeReadHCVR().

(C) Copyright AudioScience Inc. 1997-2006
************************************************************************/

#define LogRegisterRead(a, b, c)
#define LogRegisterWrite(a, b)
#define DumpRegisterLog()

#define MessageBuffer_Sprintf(pfmt, ...)

/* DSP56301 PCI HOST registers (these are the offset from the base address) */
/* pMemBase is pointer to u32 */
#define REG56301_HCTR	   (0x0010/sizeof(u32))	/* HI32 control reg */

#define HCTR_HTF0		0x0100
#define HCTR_HTF1		0x0200
#define HCTR_HRF0		0x0800
#define HCTR_HRF1		0x1000
#define HCTR_XFER_FORMAT	(HCTR_HTF0|HCTR_HRF0)
#define HCTR_XFER_FORMAT_MASK (HCTR_HTF0|HCTR_HTF1|HCTR_HRF0|HCTR_HRF1)

#define REG56301_HSTR	(0x0014/sizeof(u32))	/* HI32 status reg */
#define HSTR_TRDY	0x0001	/* tx fifo empty */
#define HSTR_HTRQ	0x0002	/* txfifo not full */
#define HSTR_HRRQ	0x0004	/* rx fifo not empty */
#define HSTR_HF4	0x0010

/* HI32 command vector reg */
#define REG56301_HCVR	(0x0018/sizeof(u32))
/* Host Transmit data reg (FIFO) */
#define REG56301_HTXR	(0x001C/sizeof(u32))
/* Host Receive data reg (FIFO) */
#define REG56301_HRXS	(0x001C/sizeof(u32))

#define TIMEOUT		100000	/* # of times to retry operations */

#define DPI_DATA_SIZE	16

/**************************** LOCAL PROTOTYPES **************************/
static u32 Dpi_SafeReadHSTR(
	struct hpi_hw_obj *pio
);
static u32 Dpi_SafeReadHCVR(
	struct hpi_hw_obj *pio
);
static void Dpi_SafeWriteHCVR(
	struct hpi_hw_obj *pio,
	u32 dwHCVR
);

static short Dpi_Command(
	struct hpi_hw_obj *pio,
	u32 Cmd
);
static void Dpi_SetFlags(
	struct hpi_hw_obj *pio,
	short Flagval
);
static short Dpi_WaitFlags(
	struct hpi_hw_obj *pio,
	short Flagval
);
static short Dpi_GetFlags(
	struct hpi_hw_obj *pio
);
static short DpiData_Read16(
	struct hpi_hw_obj *pio,
	u16 *pwData
);
static short DpiData_Read32(
	struct hpi_hw_obj *pio,
	u32 *pdwData
);
static short DpiData_Write32(
	struct hpi_hw_obj *pio,
	u32 *pdwData
);
static short DpiData_WriteBlock16(
	struct hpi_hw_obj *pio,
	u16 *pwData,
	u32 dwLength
);
static short DpiData_Write24(
	struct hpi_hw_obj *pio,
	u32 *pdwData
);

/**************************** EXPORTED FUNCTIONS ************************/
/************************************************************************/
/** just check that we have the correct adapter (in this case
 * the 56301 chip) at the indicated memory address
 */
static short CheckAdapterPresent(
	struct hpi_hw_obj *pio
)
{
	u32 dwData = 0;
	/*! Don't set TWSD */
	iowrite32(0xFFF7FFFFL, pio->pMemBase + REG56301_HCTR);
	dwData = ioread32(pio->pMemBase + REG56301_HCTR);
	if (dwData != 0x1DBFEL) {
		HPI_DEBUG_LOG(DEBUG, "Read should be 1DBFE, but is %X\n",
			dwData);
		return (1);	/*error */
	} else {
		u32 dwHSTR = 0, dwHCVR = 0;
		dwHSTR = ioread32(pio->pMemBase + REG56301_HSTR);
		dwHCVR = ioread32(pio->pMemBase + REG56301_HCVR);
		HPI_DEBUG_LOG(VERBOSE,
			"PCI registers, HCTR=%08X HSTR=%08X HCVR=%08X\n",
			dwData, dwHSTR, dwHCVR);
		return (0);	/*success */
	}
}

/************************************************************************/
static short AdapterBootLoadDsp(
	struct hpi_adapter_obj *pao,
	struct hpi_hw_obj *pio,
	u32 *pdwOsErrorCode
)
{
	__iomem u32 *pMemBase = pio->pMemBase;
	struct dsp_code DspCode;
	short nError = 0;

	u32 dwDspCodeLength = adwDspCode_Boot4000a[0];
	u32 dwDspCodeAddr = adwDspCode_Boot4000a[1];
	u32 dwDspCodeType = 0;
	/*u32  * adwDspCodeArray; */
	/*	 short nArrayNum; */
	short bFlags;
	u32 dwHSTR;
	u32 dwData;
	/*	  u32 dwOffset=0; */
	u32 i = 0;
	u32 dwCount;
	u16 wAdapterId = 0;
	short k = 0;
	u16 nLoadFamily;

	/* Number of words per dot during download 102 = 1K/10 */
#define DOTRATE 102

	Dpi_SetFlags(pio, HF_DSP_STATE_RESET);

#ifdef JTAGLOAD

	/*  Use this if not bootloading */
	Dpi_Command(pio, CVR_RESTART + CVR_NMI);
	dwHSTR = 0;
	iowrite32(dwHSTR, pMemBase + REG56301_HCTR);
	LogRegisterWrite(REG56301_HCTR, dwHSTR);
	HPI_DEBUG_LOG(VERBOSE, "reset\n");
	return (0);
#endif

	HPI_DEBUG_LOG(VERBOSE, "Boot-");
	/* If board isn't reset, try to reset it now */
	i = 0;
	do {
		dwHSTR = ioread32(pMemBase + REG56301_HSTR);
		bFlags = (short)(dwHSTR & HF_MASK);
		if (bFlags) {
			HPI_DEBUG_LOG(VERBOSE, "reset,");
			Dpi_Command(pio, CVR_NMIRESET);
			HpiOs_DelayMicroSeconds(100);
		}
	}
	while ((bFlags != 0) && (++i < 5));

	if (bFlags) {
		HPI_DEBUG_LOG(VERBOSE,
			"ERROR: Bootload - Cannot soft reset DSP\n");
		return (DPI_ERROR_DOWNLOAD + 1);
	}
	/* download boot-loader */
	HPI_DEBUG_LOG(VERBOSE, "Dbl,");

	/* Set up the transmit and receive FIFO for 24bit words */
	/* in lower portion of 32 bit word */
	/* Also set HS[2-0] to 111b to distinguish it from HSTR */
	pio->dwHCTR = HCTR_XFER_FORMAT | 0x1C000;
	iowrite32(pio->dwHCTR, pMemBase + REG56301_HCTR);
	LogRegisterWrite(REG56301_HCTR, pio->dwHCTR);

	HpiOs_DelayMicroSeconds(100);

	Dpi_SafeWriteHCVR(pio, 0);

	/* write length and starting address */
	if (DpiData_Write24(pio, &dwDspCodeLength))
		return (DPI_ERROR_DOWNLOAD + 2);
	if (DpiData_Write24(pio, &dwDspCodeAddr))
		return (DPI_ERROR_DOWNLOAD + 3);

	/* and then actual code */
	/* Skip array[2] which contains type */
	for (i = 3; i < dwDspCodeLength + 3; i++) {
		dwData = (u32)adwDspCode_Boot4000a[(u16)i];
		if (DpiData_Write24(pio, &dwData))
			return (DPI_ERROR_DOWNLOAD + 4);
	}

	/* Get adapter ID from bootloader through HF3,4,5, wait until !=0 */
	HPI_DEBUG_LOG(VERBOSE, "Id,");
	for (k = 10000; k != 0; k--) {
		dwHSTR = ioread32(pMemBase + REG56301_HSTR);
		LogRegisterRead(REG56301_HSTR, dwHSTR, 0);
		if (dwHSTR & 0x0038)
			break;
	}
	if (k == 0)
		return (DPI_ERROR_DOWNLOAD + 5);	/* 935 */

	wAdapterId = (unsigned short)(dwHSTR & 0x38) >> 3;

	/* assign DSP code based on adapter type returned */
	switch (wAdapterId) {
	case 4:
		nLoadFamily = HPI_ADAPTER_FAMILY_ASI4100;
		break;
	case 2:
	default:
		nLoadFamily = HPI_ADAPTER_FAMILY_ASI4300;
		break;
	}

	HPI_DEBUG_LOG(VERBOSE, "(Family = %x) \n", nLoadFamily);

	DspCode.psDev = pao->Pci.pOsData;
	nError = HpiDspCode_Open(nLoadFamily, &DspCode, pdwOsErrorCode);
	if (nError)
		goto exit;

	/* download multi-segment, multi-array/file DSP code (P,X,Y) */
	HPI_DEBUG_LOG(VERBOSE, "Dms,");	/* *************** debug */
	dwCount = 0;
	while (1) {
		nError = HpiDspCode_ReadWord(&DspCode, &dwDspCodeLength);
		if (nError)
			goto exit;
		/* write length, starting address and segment type (P,X or Y) */
		if (DpiData_Write24(pio, &dwDspCodeLength)) {
			nError = (DPI_ERROR_DOWNLOAD + 6);
			goto exit;
		}
		/*end of code, still wrote to DSP to signal end of load */
		if (dwDspCodeLength == 0xFFFFFFFFL)
			break;

		nError = HpiDspCode_ReadWord(&DspCode, &dwDspCodeAddr);
		if (nError)
			goto exit;
		nError = HpiDspCode_ReadWord(&DspCode, &dwDspCodeType);
		if (nError)
			goto exit;

		if (DpiData_Write24(pio, &dwDspCodeAddr)) {
			nError = (DPI_ERROR_DOWNLOAD + 7);
			goto exit;
		}
		if (DpiData_Write24(pio, &dwDspCodeType)) {
			nError = (DPI_ERROR_DOWNLOAD + 8);
			goto exit;
		}
		dwCount += 3;

		/* and then actual code segment */
		for (i = 0; i < dwDspCodeLength; i++) {
			/*if ((i % DOTRATE)==0)
			   HPI_DEBUG_LOG(VERBOSE, "."); */
			if ((nError = HpiDspCode_ReadWord(&DspCode,
						&dwData)) != 0)
				goto exit;
			if (DpiData_Write24(pio, &dwData)) {
				nError = (DPI_ERROR_DOWNLOAD + 9);
				goto exit;
			}
			dwCount++;
		}
		/*HPI_DEBUG_LOG(VERBOSE, "."); */
	}
	/* ======== END OF MAIN DOWNLOAD ======== */

	/* Wait for DSP to be ready to reset PCI */
	/* Use custom WaitFlags code that has an extra long timeout */
	{
		u32 dwTimeout = 1000000;
		u16 wFlags = 0;
		do {
			wFlags = Dpi_GetFlags(pio);
		}
		while ((wFlags != HF_DSP_STATE_IDLE) && (--dwTimeout));
		if (!dwTimeout) {
			/* Never got to idle */
			nError = (DPI_ERROR_DOWNLOAD + 11);
			goto exit;
		}
	}
	Dpi_SetFlags(pio, CMD_NONE);

	HPI_DEBUG_LOG(VERBOSE, "!\n");

exit:
	HpiDspCode_Close(&DspCode);
	return (nError);

}

/************************************************************************/
static u16 Hpi56301_Resync(
	struct hpi_hw_obj *pio
)
{
	u16 wError;
	/*?  u32 dwData; */
	u32 dwTimeout;

	Dpi_SetFlags(pio, CMD_NONE);
	Dpi_Command(pio, CVR_CMDRESET);
	wError = Dpi_WaitFlags(pio, HF_DSP_STATE_IDLE);
	dwTimeout = TIMEOUT;
	while (dwTimeout
		&& (ioread32(pio->pMemBase + REG56301_HSTR) & HSTR_HRRQ))
		dwTimeout--;

	if (dwTimeout == 0)
		wError = HPI_ERROR_BAD_ADAPTER;
	return wError;

}

/************************************************************************/
static short Hpi56301_Send(
	struct hpi_hw_obj *pio,
	u16 *pData,
	u32 dwLengthInWords,
	short wDataType
)
{
	u16 *pwData = pData;
	u16 nErrorIndex = 0;
	u16 wError;
	/*u32 dwCount; */
	u32 dwLength;
	u32 dwTotal;

	HPI_DEBUG_LOG(VERBOSE, "Send, ptr=%p, len=%d, cmd=%d\n",
		pData, dwLengthInWords, wDataType);
	HPI_DEBUG_DATA(pData, dwLengthInWords);

	wError = Dpi_WaitFlags(pio, HF_DSP_STATE_IDLE);
	if (wError) {
#ifdef ASI_DRV_DEBUG
		/*DBGPRINTF1(TEXT("!Dpi_WaitFlags %d!"),wError); */
#endif
		nErrorIndex = 1;
		goto ErrorExit;	/* error 911 */
	}
	if (!(Dpi_SafeReadHSTR(pio) & 1)) {
		/* XMIT FIFO NOT EMPTY */
		DumpRegisterLog();
		HPI_DEBUG_LOG(ERROR,
			"Xmit FIFO not empty in Hpi56301_Send()\n");
		Hpi56301_Resync(pio);

		if (!(Dpi_SafeReadHSTR(pio) & 1) || wDataType != CMD_SEND_MSG) {
			nErrorIndex = 13;
			goto ErrorExit;
		}
	}
	HPI_DEBUG_LOG(VERBOSE, "flags = %d\n", Dpi_GetFlags(pio));
	Dpi_SetFlags(pio, wDataType);
	HPI_DEBUG_LOG(VERBOSE, "flags = %d\n", Dpi_GetFlags(pio));
	if (DpiData_Write32(pio, &dwLengthInWords)) {
		nErrorIndex = 2;
		goto ErrorExit;
	}			/* error 912 */
	HPI_DEBUG_LOG(VERBOSE, "flags = %d\n", Dpi_GetFlags(pio));
	if (Dpi_Command(pio, CVR_CMD)) {
		nErrorIndex = 3;
		goto ErrorExit;
	}			/* error 913 */
	HPI_DEBUG_LOG(VERBOSE, "flags = %d\n", Dpi_GetFlags(pio));
	if (Dpi_WaitFlags(pio, HF_DSP_STATE_CMD_OK)) {
		nErrorIndex = 4;
		goto ErrorExit;
	}
	/* error 914 */
	/* read back length before wrap */
	if (DpiData_Read32(pio, &dwLength)) {
		HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error line\n");
		nErrorIndex = 5;
		goto ErrorExit;
	}

	/* error 915 */
	/* round up limit.
	   Buffer must have extra word if odd length is requested.
	   I.e buffers must be allocated in 32bit multiples
	 */
 /*----------------------------------- SGT test transfer time, start */
	/*outportb(0x378,0); */
	/*	dwTotal = ((dwLength+1)/2); */
	if (dwLength > dwLengthInWords) {
		/*ASIWDM_LogMessage( */
		/*"Returned length longer than" */
		/*" requested in Hpi56301_Send().\n"); */
		DumpRegisterLog();
		dwLength = dwLengthInWords;
	}
	dwTotal = dwLength;

	if (DpiData_WriteBlock16(pio, pwData, dwTotal)) {
		nErrorIndex = 6;	/* error 916 */
		goto ErrorExit;
	}

	if (dwLength < dwLengthInWords) {
		if (Dpi_Command(pio, CVR_DMASTOP)) {
			nErrorIndex = 7;
			goto ErrorExit;
		}
		/* error 917 */
		/* Wait for DMA to be reprogrammed for second block */
		if (Dpi_WaitFlags(pio, HF_DSP_STATE_WRAP_READY)) {
			nErrorIndex = 8;
			goto ErrorExit;
		}
		/* error 918 */
		/*		pdwData=(u32  *)(pData+ dwLength); */
		pwData = (pData + dwLength);
		/* round up limit. Buffer must have extra word if odd length is
		   requested. Buffers must be allocated in 32bit multiples
		   Should this code try to use aligned dwords where possible?
		   i.e if pData starts odd, do word xfer,loop dword xfer,
		   word xfer */

		/* remaining words after wrap */
		dwTotal = (dwLengthInWords - dwLength);

		if (DpiData_WriteBlock16(pio, pwData, dwTotal)) {
			nErrorIndex = 9;	/* error 919 */
			goto ErrorExit;
		}
	}
	/* ---------------------------------- SGT test transfer time, end */
	/*outportb(0x378,0); */

	if (Dpi_Command(pio, CVR_DMASTOP)) {
		nErrorIndex = 10;
		goto ErrorExit;
	}
	/* error 920 */
	if (wDataType == CMD_SEND_MSG) {
		if (Dpi_WaitFlags(pio, HF_DSP_STATE_RESP_READY)) {
			nErrorIndex = 11;
			goto ErrorExit;
		}		/* error 921 */
	} else {
		if (Dpi_WaitFlags(pio, HF_DSP_STATE_IDLE)) {
			nErrorIndex = 12;
			goto ErrorExit;
		}		/* error 922 */
	}
	return (0);

ErrorExit:
	/* When there is an error, we automatically try to resync.
	 * If the error persists, bump the error count up by 1000.
	 * Fatal errors will now return 19xx (decimal), and errors that "should"
	 * be recoverable will now return 9xx.
	 */
	if (Hpi56301_Resync(pio))
		nErrorIndex += 1000;
	return (DPI_ERROR_SEND + nErrorIndex);
}

/************************************************************************/
static short Hpi56301_Get(
	struct hpi_hw_obj *pio,
	u16 *pData,
	u32 *pdwLengthInWords,
	short wDataType,
	u32 dwMaxLengthInWords
)
{
	u32 dwLength1, dwLength2;
	u16 *pwData = pData;
	u16 nErrorIndex = 0;
	u32 dwCount;
	u32 dwBeforeWrap, dwAfterWrap;
	u32 dwJunk;
	u32 *pdwJunk = &dwJunk;

	HPI_DEBUG_LOG(VERBOSE, "Get, ptr=%p, cmd=%d\n", pData, wDataType);
	MessageBuffer_Sprintf("Get, ptr =0x%lx, len = 0x%x, cmd = %d\n",
		pData, dwMaxLengthInWords * 2, wDataType);

	/* Wait for indication that previous command has been processed */
	if (wDataType == CMD_GET_RESP) {
		if (Dpi_WaitFlags(pio, HF_DSP_STATE_RESP_READY)) {
			nErrorIndex = 1;
			goto ErrorExit;
		}		/* error 951 */
	} else {
		if (Dpi_WaitFlags(pio, HF_DSP_STATE_IDLE)) {
			nErrorIndex = 2;
			goto ErrorExit;
		}		/* error 952 */
	}
	/* Tell DSP what to do next. */
	Dpi_SetFlags(pio, wDataType);
	if (Dpi_Command(pio, CVR_CMD)) {
		nErrorIndex = 3;
		goto ErrorExit;
	}
	/* error 953 */
	if (Dpi_WaitFlags(pio, HF_DSP_STATE_CMD_OK)) {
		nErrorIndex = 4;
		goto ErrorExit;
	}

	/* error 954 */
	/* returned length in 16 bit words */
	if (DpiData_Read32(pio, &dwLength1)) {
		HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
		nErrorIndex = 5;
		goto ErrorExit;
	}
	/* error 955 */
	/* dwBeforeWrap = ((dwLength1+1)/2); */
	dwBeforeWrap = dwLength1;
	/* length of second part of data, may be 0 */
	if (DpiData_Read32(pio, &dwLength2)) {
		HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
		nErrorIndex = 5;
		goto ErrorExit;
	}			/* error 955 */
	dwAfterWrap = dwLength2;
	*pdwLengthInWords = dwLength1 + dwLength2;	/* AGE Feb 19 98 */

	if (dwBeforeWrap + dwAfterWrap > dwMaxLengthInWords) {
		/* XMIT FIFO NOT EMPTY */
		DumpRegisterLog();
		if (dwBeforeWrap > dwMaxLengthInWords) {
			dwBeforeWrap = dwMaxLengthInWords;
			dwAfterWrap = 0;
		} else {
			dwAfterWrap = dwMaxLengthInWords - dwBeforeWrap;
		}
	}
	/* read the first part of data */
	for (dwCount = 0; dwCount < dwBeforeWrap; dwCount++) {
		if (DpiData_Read16(pio, pwData)) {
			nErrorIndex = 6;
			goto ErrorExit;
		}
		pwData++;
	}			/* error 956 */

	/* check for a bad value of dwAfterWrap */
	if (dwAfterWrap && (wDataType == CMD_GET_RESP)) {
		HPI_DEBUG_LOG(DEBUG,
			"Unexpected data wrap for response, got %u\n",
			dwAfterWrap);
		/* this may fix the error -
		   it depends if the DSP is in error or not. */
		dwAfterWrap = 0;
	}

	if (dwAfterWrap) {
		/* Handshake, then read second block */
		if (Dpi_Command(pio, CVR_DMASTOP)) {
			HPI_DEBUG_LOG(DEBUG, "Dpi_Command\n");
			nErrorIndex = 7;
			goto ErrorExit;
		}		/* error 957 */
		if (Dpi_WaitFlags(pio, HF_DSP_STATE_WRAP_READY)) {
			HPI_DEBUG_LOG(DEBUG, "Dpi_WaitFlags\n");
			nErrorIndex = 8;
			goto ErrorExit;
		}
		/* error 958 */
		/* Clean the extra data out of the FIFO */
		if (DpiData_Read32(pio, pdwJunk)) {
			HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
			nErrorIndex = 9;
			goto ErrorExit;
		}		/* error 959 */
		if (DpiData_Read32(pio, pdwJunk)) {
			HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
			nErrorIndex = 9;
			goto ErrorExit;
		}		/* error 959 */
		if (DpiData_Read32(pio, pdwJunk)) {
			HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
			nErrorIndex = 9;
			goto ErrorExit;
		}		/* error 959 */
		for (dwCount = 0; dwCount < dwAfterWrap; dwCount++) {
			if (DpiData_Read16(pio, pwData)) {
				nErrorIndex = 10;
				goto ErrorExit;
			}
			pwData++;
		}		/* error 960 */
	}

	if (Dpi_Command(pio, CVR_DMASTOP)) {
		nErrorIndex = 11;
		goto ErrorExit;
	}			/* error 961 */
	if (Dpi_WaitFlags(pio, HF_DSP_STATE_IDLE)) {
		nErrorIndex = 12;
		goto ErrorExit;
	}

	/* error 962 */
	/* Clean the extra data out of the FIFO */
	if (DpiData_Read32(pio, pdwJunk)) {
		HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
		nErrorIndex = 13;
		goto ErrorExit;
	}			/* error 963 */
	if (DpiData_Read32(pio, pdwJunk)) {
		HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
		nErrorIndex = 14;
		goto ErrorExit;
	}			/* error 964 */
	if (DpiData_Read32(pio, pdwJunk)) {
		HPI_DEBUG_LOG(DEBUG, "DpiData_Read32 error\n");
		nErrorIndex = 15;
		goto ErrorExit;
	}			/* error 965 */
	HPI_DEBUG_DATA(pData, *pdwLengthInWords);
	return (0);

ErrorExit:
	/* When there is an error, we automatically try to resync.
	 * If the error persists, bump the error count up by 1000.
	 * Fatal errors will now return 19xx (decimal), and errors that "should"
	 * be recoverable will now return 9xx.
	 */
	if (Hpi56301_Resync(pio))
		nErrorIndex += 1000;
	return (DPI_ERROR_GET + nErrorIndex);
}

/************************************************************************/
static void MessageResponseSequence(
	struct hpi_hw_obj *pio,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	short nError;
	u32 dwSize;

	HPI_DEBUG_LOG(VERBOSE, "A");

	/* Need initialise in case hardware fails to return a response */
	HPI_InitResponse(phr, 0, 0, HPI_ERROR_INVALID_RESPONSE);

	if ((phm->wObject == 0) || (phm->wFunction == 0)) {
		phr->wError = DPI_ERROR;
		return;
	}

	/* error is for data transport. If no data transport error, there
	   still may be an error in the message, so dont assign directly */

	nError = Hpi56301_Send(pio,
		(u16 *)phm,
		(phm->wSize / sizeof(u32)) * sizeof(u16), CMD_SEND_MSG);
	if (nError) {
		phr->wError = nError;
		return;
	}

	HPI_DEBUG_LOG(VERBOSE, "B");
	nError = Hpi56301_Get(pio,
		(u16 *)phr,
		&dwSize,
		CMD_GET_RESP, sizeof(struct hpi_response) / sizeof(u16));
	if (nError) {
		phr->wError = nError;
		return;
	}

	/* maybe an error response to WRITE or READ message */
	if (phr->wError)
		return;

	if (phm->wFunction == HPI_OSTREAM_WRITE) {
		HPI_DEBUG_LOG(VERBOSE, "C");
		/* note - this transfers the entire packet in one go! */
		nError = Hpi56301_Send(pio,
			(u16 *)phm->u.d.u.Data.
			pbData,
			(phm->u.d.u.Data.dwDataSize /
				sizeof(u16)), CMD_SEND_DATA);
		if (nError) {
			phr->wError = nError;
			return;
		}
	}

	if (phm->wFunction == HPI_ISTREAM_READ) {
		HPI_DEBUG_LOG(VERBOSE, "D");
		/* note - this transfers the entire packet in one go! */
		nError = Hpi56301_Get(pio,
			(u16 *)phm->u.d.u.Data.pbData,
			&dwSize,
			CMD_GET_DATA,
			phm->u.d.u.Data.dwDataSize / sizeof(u16));
		if (nError) {
			phr->wError = nError;
			return;
		}

		if (phm->u.d.u.Data.dwDataSize != (dwSize * 2))
			/* mismatch in requested and received data size */
			phr->wError = HPI_ERROR_INVALID_DATA_TRANSFER;

	}

	HPI_DEBUG_LOG(VERBOSE, "G");
	return;
}

/**************************** LOCAL FUNCTIONS ***************************/
/************************************************************************/
static u32 Dpi_SafeReadHSTR(
	struct hpi_hw_obj *pio
)
{
	u32 dwHSTR;
	u32 dwTimeout = 5;

	do {
		dwTimeout--;
		dwHSTR = ioread32(pio->pMemBase + REG56301_HSTR);
		if (dwHSTR & CVR_NMI) {
			LogRegisterRead(REG56301_HSTR, dwHSTR, 1);
			HPI_DEBUG_LOG(DEBUG,
				"Dpi_SafeReadHSTR() bad read. "
				"Value:0x%08x\n", dwHSTR);
		} else
			break;
	}
	while (dwTimeout);

	if (dwTimeout) {
		LogRegisterRead(REG56301_HSTR, dwHSTR, 0);
	} else {
		HPI_DEBUG_LOG(ERROR,
			"Dpi_SafeReadHSTR() failed.  Value:0x%08x\n", dwHSTR);
	}
	return dwHSTR;
}

static u32 Dpi_SafeReadHCVR(
	struct hpi_hw_obj *pio
)
{
	u32 dwHCVR;
	u32 dwTimeout = 5;

	do {
		dwTimeout--;
		dwHCVR = ioread32(pio->pMemBase + REG56301_HCVR);
		if ((dwHCVR | CVR_INT) != pio->dwHCVR) {
			LogRegisterRead(REG56301_HCVR, dwHCVR, 1);
			HPI_DEBUG_LOG(DEBUG,
				"Dpi_SafeReadHCVR() bad read. "
				"Value:0x%08x\n", dwHCVR);
		} else
			break;
	}
	while (dwTimeout);

	if (dwTimeout) {
		LogRegisterRead(REG56301_HCVR, dwHCVR, 0);
	} else {
		HPI_DEBUG_LOG(ERROR,
			"Dpi_SafeReadHCVR() failed.  Value:0x%08x\n", dwHCVR);
	}
	return dwHCVR;
}

static void Dpi_SafeWriteHCVR(
	struct hpi_hw_obj *pio,
	u32 dwHCVR
)
{
	iowrite32(dwHCVR | CVR_NMI, pio->pMemBase + REG56301_HCVR);
	LogRegisterWrite(REG56301_HCVR, dwHCVR | CVR_NMI);
	pio->dwHCVR = dwHCVR | CVR_NMI | CVR_INT;
}

static void Dpi_SetFlags(
	struct hpi_hw_obj *pio,
	short Flagval
)
{
	pio->dwHCTR = (pio->dwHCTR & HF_CLEAR) | Flagval;

	iowrite32(pio->dwHCTR, pio->pMemBase + REG56301_HCTR);
	LogRegisterWrite(REG56301_HCTR, pio->dwHCTR);
}

/************************************************************************/
/** Wait for specified flag value to appear in the HSTR
 * Return 0 if it does, DPI_ERROR if we time out waiting
 */
static short Dpi_WaitFlags(
	struct hpi_hw_obj *pio,
	short Flagval
)
{
/*	u32 dwHSTR; */
	u16 wFlags;
	u32 dwTimeout = TIMEOUT;

	/* HPI_DEBUG_LOG(VERBOSE, "Wf,"); */

	do {
		wFlags = Dpi_GetFlags(pio);
		if (wFlags == Flagval)
			break;
	}
	while (--dwTimeout);
	HPI_DEBUG_LOG(VERBOSE, "Expecting %d, got %d%s\r\n",
		Flagval, wFlags, !dwTimeout ? " timeout" : "");
	if (!dwTimeout) {
		HPI_DEBUG_LOG(DEBUG,
			"Dpi_WaitFlags()  Expecting %d, got %d\r\n",
			Flagval, wFlags);
		return (DPI_ERROR + wFlags);
	} else
		return (0);
}

/************************************************************************/
static short Dpi_GetFlags(
	struct hpi_hw_obj *pio
)
{
	u32 dwHSTR;

	dwHSTR = Dpi_SafeReadHSTR(pio);
	return ((short)(dwHSTR & HF_MASK));
}

/************************************************************************/
/** Wait for CVR_INT bits to be set to 0 */
static u32 Dpi_Command_Handshake(
	struct hpi_hw_obj *pio,
	u32 Cmd
)
{
	u32 dwTimeout;
	u32 dwHCVR;
/*	u32 dwHCTR; */

	/* HPI_DEBUG_LOG(VERBOSE, "Dc,"); */

	/* make sure HCVR-HC is zero before writing to HCVR */
	dwTimeout = TIMEOUT;
	do {
		dwTimeout--;
		dwHCVR = Dpi_SafeReadHCVR(pio);
	}
	while ((dwHCVR & CVR_INT) && dwTimeout);
	HPI_DEBUG_LOG(VERBOSE, "(cmd = %d) attempts = %d\n",
		Cmd, TIMEOUT - dwTimeout);

	return dwTimeout;
}

static short Dpi_Command(
	struct hpi_hw_obj *pio,
	u32 Cmd
)
{
	if (!Dpi_Command_Handshake(pio, Cmd))
		return (DPI_ERROR);

	Dpi_SafeWriteHCVR(pio, Cmd | CVR_INT);

	/* Wait for interrupt to be acknowledged */
/*	if(!Dpi_Command_Handshake(pio,Cmd)) */
/*		return (DPI_ERROR); */

	return (0);
}

/************************************************************************/
static u32 DpiData_ReadHandshake(
	struct hpi_hw_obj *pio
)
{
	u32 dwTimeout;
	u32 dwHSTR;
	dwTimeout = TIMEOUT;
	do {
		dwTimeout--;
		dwHSTR = Dpi_SafeReadHSTR(pio);
	}
	while ((!(dwHSTR & HSTR_HRRQ)) && dwTimeout);

	if (TIMEOUT - dwTimeout > 500)
		HPI_DEBUG_LOG(DEBUG,
			"DpiData_ReadHandshake() attempts = %d\n",
			TIMEOUT - dwTimeout);

	if (!dwTimeout)
		HPI_DEBUG_LOG(DEBUG, "DataRead - FIFO is staying empty"
			" HSTR=%08X\n", dwHSTR);
	return dwTimeout;
}

/************************************************************************/
/** Reading the FIFO, protected against FIFO empty
 * Returns DPI_ERROR if FIFO stays empty for TIMEOUT loops
 */
static short DpiData_Read32(
	struct hpi_hw_obj *pio,
	u32 *pdwData
)
{
	u32 dwD1 = 0, dwD2 = 0;

	if (!DpiData_ReadHandshake(pio))
		return (DPI_ERROR);

	dwD1 = ioread32(pio->pMemBase + REG56301_HRXS);
	LogRegisterRead(REG56301_HRXS, dwD1, 0);

	if (!DpiData_ReadHandshake(pio))
		return (DPI_ERROR);

	dwD2 = ioread32(pio->pMemBase + REG56301_HRXS);
	LogRegisterRead(REG56301_HRXS, dwD2, 0);

	*pdwData = (dwD2 << 16) | (dwD1 & 0xFFFF);
	return (0);
}

static short DpiData_Read16(
	struct hpi_hw_obj *pio,
	u16 *pwData
)
{
	u32 dwD1 = 0;

	if (!DpiData_ReadHandshake(pio))
		return (DPI_ERROR);

	dwD1 = ioread32(pio->pMemBase + REG56301_HRXS);
	LogRegisterRead(REG56301_HRXS, dwD1, 0);
	dwD1 &= 0xFFFF;
	*pwData = (u16)dwD1;
	return (0);
}

/************************************************************************/
static u32 DpiData_WriteHandshake(
	struct hpi_hw_obj *pio,
	u32 dwBitMask
)
{
	u32 dwTimeout;
	u32 dwHSTR;

	dwTimeout = TIMEOUT;
	do {
		dwTimeout--;
		dwHSTR = Dpi_SafeReadHSTR(pio);
	}
	while ((!(dwHSTR & dwBitMask)) && dwTimeout);
	if (!dwTimeout)
		HPI_DEBUG_LOG(DEBUG, "DataWrite - FIFO is staying full"
			" HSTR=%08X\n", dwHSTR);

	return dwTimeout;
}

/************************************************************************/
/** Writing the FIFO, protected against FIFO full
 * Returns error if FIFO stays full
 */
static short DpiData_Write32(
	struct hpi_hw_obj *pio,
	u32 *pdwData
)
{
	u32 dwD1, dwD2;

	dwD1 = *pdwData & 0xFFFF;
	dwD2 = *pdwData >> 16;

	if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
		return (DPI_ERROR);

	iowrite32(dwD1, pio->pMemBase + REG56301_HTXR);
	LogRegisterWrite(REG56301_HTXR, dwD1);
	/*
	   HPI_DEBUG_LOG(VERBOSE,"Wrote (lo) 0x%08lx, hstr = 0x%08lx," \
	   " attempts = %ld\n",
	   dwD1, dwHSTR, TIMEOUT - dwTimeout);
	 */

	if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
		return (DPI_ERROR);

	iowrite32(dwD2, pio->pMemBase + REG56301_HTXR);
	LogRegisterWrite(REG56301_HTXR, dwD2);
	/*
	   HPI_DEBUG_LOG(VERBOSE,"Wrote (hi) 0x%08lx, hstr = 0x%08lx," \
	   " attempts = %ld\n",
	   dwD2, dwHSTR, TIMEOUT - dwTimeout);
	 */
	return (0);
}

/************************************************************************/
/** Writing the FIFO, protected against FIFO full
 * Returns error if FIFO stays full
 */
static short DpiData_Write24(
	struct hpi_hw_obj *pio,
	u32 *pdwData
)
{
	if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
		return (DPI_ERROR);

	iowrite32(*pdwData, pio->pMemBase + REG56301_HTXR);
	LogRegisterWrite(REG56301_HTXR, *pdwData);
	return (0);
}

/************************************************************************/
/** Writing the FIFO, protected against FIFO full
 * Returns error if FIFO stays full
 */
static short DpiData_Write16(
	struct hpi_hw_obj *pio,
	u16 *pwData
)
{
	u32 wData;

	if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
		return (DPI_ERROR);

	wData = (u32)*pwData;
	iowrite32(wData, pio->pMemBase + REG56301_HTXR);
	LogRegisterWrite(REG56301_HTXR, wData);
	/*	HPI_DEBUG_LOG(VERBOSE,"Wrote 0x%08lx, " */
	/* "hstr = 0x%08lx, attempts = %ld\n", */
	/*			  wData, dwHSTR, TIMEOUT - dwTimeout); */
	return (0);
}

static short DpiData_WriteBlock16(
	struct hpi_hw_obj *pio,
	u16 *pwData,
	u32 dwLength
)
{
	__iomem void *dwAddrFifoWrite = pio->pMemBase + REG56301_HTXR;
	u32 dwD0, dwD1, dwD2, dwD3, dwD4, dwD5;
	u32 dwLeft;

	/* HPI_DEBUG_LOG(VERBOSE,"Length = %ld\n", dwLength); */
	dwLeft = dwLength;
	while (dwLeft >= 6) {
		/* assemble 6 words to transmit */
		dwD0 = *pwData++ & 0xFFFF;
		dwD1 = *pwData++ & 0xFFFF;
		dwD2 = *pwData++ & 0xFFFF;
		dwD3 = *pwData++ & 0xFFFF;
		dwD4 = *pwData++ & 0xFFFF;
		dwD5 = *pwData++ & 0xFFFF;

		/* wait for transmit FIFO to be empty (TRDY==1) */
		if (!DpiData_WriteHandshake(pio, HSTR_TRDY))
			return (DPI_ERROR);

		/* write 6 words to FIFO (only have data in bottom 16bits) */
		iowrite32(dwD0, dwAddrFifoWrite);
		LogRegisterWrite(REG56301_HTXR, dwD0);
#ifdef PARANOID_FLAG_CHECK
		if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
			return (DPI_ERROR);
#endif

		iowrite32(dwD1, dwAddrFifoWrite);
		LogRegisterWrite(REG56301_HTXR, dwD1);
#ifdef PARANOID_FLAG_CHECK
		if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
			return (DPI_ERROR);
#endif

		iowrite32(dwD2, dwAddrFifoWrite);
		LogRegisterWrite(REG56301_HTXR, dwD2);
#ifdef PARANOID_FLAG_CHECK
		if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
			return (DPI_ERROR);
#endif

		iowrite32(dwD3, dwAddrFifoWrite);
		LogRegisterWrite(REG56301_HTXR, dwD3);
#ifdef PARANOID_FLAG_CHECK
		if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
			return (DPI_ERROR);
#endif

		iowrite32(dwD4, dwAddrFifoWrite);
		LogRegisterWrite(REG56301_HTXR, dwD4);
#ifdef PARANOID_FLAG_CHECK
		if (!DpiData_WriteHandshake(pio, HSTR_HTRQ))
			return (DPI_ERROR);
#endif

		iowrite32(dwD5, dwAddrFifoWrite);
		LogRegisterWrite(REG56301_HTXR, dwD5);

		dwLeft -= 6;	/*just sent (6x16 bit words) */
	}
	/* write remaining words */

	while (dwLeft != 0) {
		if (DpiData_Write16(pio, pwData))
			return (DPI_ERROR);
		pwData++;
		dwLeft--;
	}

	/* wait for DSP to empty the fifo prior to issuing DMASTOP */
	if (!DpiData_WriteHandshake(pio, HSTR_TRDY))
		return (DPI_ERROR);

	return (0);
}
