/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file   pti_debug_print.c
   @brief  Generic debug printer helpers

 */

#include <linux/delay.h>

#include "../pti_debug.h"
#include "../pti_tshal_api.h"
#include "../tango/pti_pdevice.h"
#include "../tango/pti_vdevice.h"
#include "../tango/pti_swinjector.h"
#include "../tango/pti_buffer.h"
#include "../tango/pti_filter.h"
#include "../tango/pti_slot.h"

#include "pti_debug_print.h"

/* MACROS ------------------------------------------------------------------ */

/* Private Constants ------------------------------------------------------- */

/* Private Variables ------------------------------------------------------- */

/* Private Function Prototypes --------------------------------------------- */
void stptiHAL_PrintMemory(void *ctx, debug_print printer, const char *template,
			  void *FirstObjectAddress, size_t ObjectSize, int NumberOfObjects);

const char *stptiHAL_ReturnXP70Status(stptiHAL_STxP70_Status_t StatusNumber);

const char *stptiHAL_ReturnObjectString(ObjectType_t ObjectType);

int stptiHAL_pDeviceCheckRunning(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p);

/* Functions --------------------------------------------------------------- */
int stptiHAL_Print_pDevice(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	U32 c1, c2, cs;
	const char *TP_StatusString;
	U16 wos = 0;
	char Version[sizeof (pDevice_p->TP_Interface_p->Version)];

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	TP_StatusString = stptiHAL_ReturnXP70Status(pDevice_p->TP_Status);

	if (pDevice_p->TP_Status == stptiHAL_STXP70_RUNNING) {
		/*
		 * ActivityCounter should change when the TP is running.
		 * CheckSum will get continually overwritten ONLY during TP
		 * initialisation.
		 */
		stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface_p->CheckSum, 0xFFFFFFFF);

		c1 = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->ActivityCounter);

		stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface_p->WaitingOnStreamer, 0);

		msleep(1);

		c2 = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->ActivityCounter);

		cs = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->CheckSum);

		if (c1 == c2) {
			/*
			 * if ActivityCounter static, but not 0xFFFFFFFF
			 * - xp70 has stalled (a failure condition)
			 */
			wos = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->WaitingOnStreamer);
			if (wos == 0) {
				TP_StatusString = "STALLED";
			} else {
				TP_StatusString = "STALLED_ON_STREAMER";
			}
		} else if (c1 != c2 && cs != 0xFFFFFFFF) {
			/*
			 * if ActivityCounter changing, CheckSum has been changed
			 * from 0xFFFFFFFF - xp70 is still awaiting initialisation
			 */
			TP_StatusString = "AWAITING_INIT*";
		}
		/* Otherwise the TP is okay (running) */
	}

	printer(ctx, "   TP_MappedAddress = 0x%08x  TP_Status = %d %-20s\n",
		(unsigned)pDevice_p->TP_MappedAddresses.TP_MappedAddress,
		(unsigned)pDevice_p->TP_Status, TP_StatusString);
	printer(ctx, "  TP_InterruptCount = %010u   UnhandledInterrupts = %010u\n",
		(unsigned)pDevice_p->TP_InterruptCount, (unsigned)pDevice_p->TP_UnhandledInterrupts);

	printer(ctx, "EventQueueOverflows = %010u   StatusBlockBufferOverflows = %010u\n",
		(unsigned)pDevice_p->EventQueueOverflows, (unsigned)pDevice_p->StatusBlockBufferOverflows);

	printer(ctx, "SignallingQueueOverflows = %010u\n", (unsigned)pDevice_p->SignallingQueueOverflows);

	printer(ctx, "TimerCounterConfig = %02x\n", stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.TimerCounter));

	stptiHAL_pDeviceXP70MemcpyFrom(Version, &pDevice_p->TP_Interface_p->Version, sizeof (Version));

	printer(ctx, "TP f/w reported version is %s, driver compiled in f/w API	version %s\n",
		Version, STPTI_TP_VERSION_ID);

	printer(ctx, "Pipeline PacketCounts are "
		"in=%010u sync_error=%010u pushed=%010u pulled=%010u (with %010u PushFailures)\n",
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->PacketsPreHeaderProcessing),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->PacketsWithSyncError),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->PacketsPushedToSP),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->PacketsPulledFromSP),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->PushFailuresToSP));

	printer(ctx, "AverageIdleCycles = %0u\n",
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->AvgIdleCount));

	printer(ctx, "AverageNotIdleCycles = %0u\n",
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->AvgNotIdleCount));

	printer(ctx, "TP DebugScratch is 0x%08x 0x%08x 0x%08x 0x%08x\n",
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[0]),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[1]),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[2]),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[3]));

	printer(ctx, "                   %10u %10u %10u %10u\n",
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[0]),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[1]),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[2]),
		stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->DebugScratch[3]));

	if (wos != 0) {
		/*
		 * Waiting On Streamer is constantly set by the firmware when it
		 * waiting on a streamer.  If the TP appears to have stalled,
		 * but WaitingOnStreamer doesn't stay reset, the number reported
		 * will help guide the f/w developers where in the firmware we
		 * are waiting.
		 * Note: For firmware developers, see top of streamer.h
		 */
		printer(ctx, "Please report as TP STALLED WaitingOnStreamer error %-3d\n", (unsigned)wos);
	}
	return 0;
}

int stptiHAL_Print_pDeviceDEM(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	U32 *u32_tp_p = (U32 *) 0;
	U32 *u32_p;
	U32 *u32_end_p;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	u32_p = (U32 *) & pDevice_p->TP_MappedAddresses.dDEM_p[0];
	u32_end_p = (U32 *) & pDevice_p->TP_MappedAddresses.iDEM_p[0];

	printer(ctx, "; %08x:%08x\n", (unsigned int)u32_p, (unsigned int)u32_end_p - 1);

	while (u32_p < u32_end_p) {
		printer(ctx, "#%08x %08x %08x %08x %08x\n",
			(unsigned int)u32_tp_p,
			stptiHAL_pDeviceXP70Read(u32_p),
			stptiHAL_pDeviceXP70Read(u32_p + 1),
			stptiHAL_pDeviceXP70Read(u32_p + 2), stptiHAL_pDeviceXP70Read(u32_p + 3));

		u32_p += 4;
		u32_tp_p += 4;
	}

	return 0;
}

int stptiHAL_Print_pDeviceSharedMemory(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	U32 *u32_tp_p;
	U32 *u32_p;
	U32 *u32_end_p;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	u32_tp_p = (U32 *) stptiHAL_pDeviceXP70Read((U32 *) & pDevice_p->TP_Interface_p->SharedMemory_p);
	u32_p = (U32 *)pDevice_p->TP_Interface.SharedMemory_p;
	u32_end_p = u32_p + (pDevice_p->TP_Interface.SizeOfSharedMemoryRegion / sizeof(U32));

	printer(ctx, "; %08x:%08x\n", (unsigned int)u32_p, (unsigned int)u32_end_p - 1);

	while (u32_p < u32_end_p) {
		printer(ctx, "#%08x %08x %08x %08x %08x\n", (unsigned int)u32_tp_p,
			stptiHAL_pDeviceXP70Read(u32_p),
			stptiHAL_pDeviceXP70Read(u32_p + 1),
			stptiHAL_pDeviceXP70Read(u32_p + 2), stptiHAL_pDeviceXP70Read(u32_p + 3));

		u32_p += 4;
		u32_tp_p += 4;
	}

	return 0;
}

int stptiHAL_Print_pDeviceTrigger(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p, int Offset)
{
	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	if (Offset == 0) {
		if (stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface_p->Trigger) == 1) {
			printer(ctx, "NotAcknowledged\n");
		} else {
			printer(ctx, "TriggerSent!\n");
			stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface_p->Trigger, 1);
		}
	}
	return 0;
}

int stptiHAL_Print_TPCycleCount(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	U32 NotIdleCount;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	NotIdleCount = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->NotIdleCount);

	/* reset the counters (in case they have been in use previously) */
	stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.pDeviceInfo_p->ResetIdleCounters, 1);

	printer(ctx, "%u\n", NotIdleCount);

	return 0;
}

int stptiHAL_Print_UtilisationTP(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	U32 IdleCount, NotIdleCount;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	/*
	 * CPU loading, done by looking at the ratio of Idle TP cycles vs
	 * Active TP cycles (between visits here)
	 */
	IdleCount = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->IdleCount);

	NotIdleCount = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->NotIdleCount);

	/* reset the counters (in case they have been in use previously) */
	stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.pDeviceInfo_p->ResetIdleCounters, 1);

	if (IdleCount == 0xFFFFFFFF || NotIdleCount == 0xFFFFFFFF) {
		printer(ctx, "counters saturated (too long between calls)\n");
	} else if (IdleCount > 0 || NotIdleCount > 0) {
		if (IdleCount > 0x00FFFFFF || NotIdleCount > 0x00FFFFFF) {
			/* scale down to avoid an overflow */
			IdleCount = IdleCount >> 8;
			NotIdleCount = NotIdleCount >> 8;
		}
		printer(ctx, "%03d%% average cpu usage\n", (100 * NotIdleCount) / (IdleCount + NotIdleCount));
	} else {
		printer(ctx, "000%% average cpu usage\n");
	}

	/*
	 * Consecutive Live Packets indicator.
	 * This indicates the number of packets from TSINs before the xp70 got
	 * an opportunity to handle playback packets, or was idle.
	 */
	{
		int i, max_clp = 0;
		char string[64];
		BOOL flag = TRUE;
		U32 CLP = stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.pDeviceInfo_p->DebugCLP);

		for (i = 0; i < 32; i++) {
			if ((CLP & 1)) {
				string[i] = 'X';
				if (flag) {
					max_clp = 1 << (31 - i);
					flag = FALSE;
				}
			} else {
				string[i] = flag ? '.' : 'x';
			}
			CLP >>= 1;
		}
		string[i] = 0;

		printer(ctx, "CLP: %s [%010u]\n", string, max_clp);

		/* Done twice, as DebugCLP is updated by a RMW by xp70 */
		stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.pDeviceInfo_p->DebugCLP, 0);
		stptiHAL_pDeviceXP70Write(&pDevice_p->TP_Interface.pDeviceInfo_p->DebugCLP, 0);
	}
	return 0;
}

int stptiHAL_Print_vDeviceInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	printer(ctx,
		"(abs)       PktC     SyncErrC TPErrC   PMisMtch CCErr    BufOvFlw PESErr Tag  PFB_ PFSz Mode "
		"STCWord0 STCWord1 EVENTMSK FLAGS    WCSI      TAGSRC   DataEntry\n");
	stptiHAL_PrintMemory(ctx, printer,
			     "#######4 #######4 #######4 #######4 #######4 #######4 #######4 ###2 ###2 ###2 ###2 #######4 "
			     "#######4 #######4 #######4 ###2 ###2 #######4 *",
			     (void *)pDevice_p->TP_Interface.vDeviceInfo_p,
			     sizeof(stptiTP_vDeviceInfo_t), pDevice_p->TP_Interface.NumberOfvDevices);
	return 0;
}

int stptiHAL_Print_SlotInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	/*
	 * This is only the first part of SlotInfo - the last part depends on
	 * the slot type and so is currently ignored
	 */
	printer(ctx, "(rel)       Stat PH Md SecP SFlg Nslt KeyI EvtMask_ Idx# " "DMA# PktCount\n");
	stptiHAL_PrintMemory(ctx, printer,
			     "###2 #1 #1 ###2 ###2 ###2 ###2 #######4 ###2 ###2 #######4",
			     (void *)pDevice_p->TP_Interface.SlotInfo_p,
			     sizeof(stptiTP_SlotInfo_t), pDevice_p->TP_Interface.NumberOfSlots);
	return 0;
}

int stptiHAL_Print_PIDTable(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	unsigned int i;
	volatile U16 *pid_entry_address_p;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	pid_entry_address_p = pDevice_p->TP_Interface.PIDTable_p;

	printer(ctx, "(rel)       PID  Slot\n");

	for (i = 0; i < pDevice_p->TP_Interface.NumberOfSlots; i++) {
		unsigned int pid = (unsigned int)stptiHAL_pDeviceXP70Read((pid_entry_address_p + i));
		unsigned int slot = (unsigned int)stptiHAL_pDeviceXP70Read((pDevice_p->TP_Interface.
									    PIDSlotMappingTable_p + i));
		printer(ctx, "#%08x:  %04x %04x\n", (U32) (pid_entry_address_p + i), pid, slot);
	}
	return 0;
}

int stptiHAL_Print_FullSlotInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	/*
	 * This is only the first part of SlotInfo - the last part depends on
	 * the slot type and so is currently ignored
	 */
	printer(ctx, "(rel)       Stat PH Md SecP SFlg Nslt KeyI EvtMask_ Idx#" " DMA# PktCount\n");
	stptiHAL_PrintMemory(ctx, printer,
			     "###2 #1 #1 ###2 ###2 ###2 ###2 #######4 ###2 ###2 "
			     "#######4 *", (void *)pDevice_p->TP_Interface.SlotInfo_p,
			     sizeof(stptiTP_SlotInfo_t), pDevice_p->TP_Interface.NumberOfSlots);
	return 0;
}

int stptiHAL_Print_BufferInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	printer(ctx, "(abs)       BASE____ SIZE____ READ____ WRITE___          " "QWRITE__ Threshld BuffrCnt\n");
	stptiHAL_PrintMemory(ctx, printer,
			     "#######4 #######4 #######4 #######4 #######4 "
			     "#######4 #######4 #######4",
			     (void *)pDevice_p->TP_Interface.DMAInfo_p,
			     sizeof(stptiTP_DMAInfo_t), pDevice_p->TP_Interface.NumberOfDMAStructures);
	return 0;
}

int stptiHAL_Print_BufferOverflowInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	unsigned int Buffers;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	printer(ctx, "ENABLED : OVERFLOW/RESET\n");

	for (Buffers = 0; Buffers < pDevice_p->TP_Interface.NumberOfDMAStructures; Buffers++) {
		if ((unsigned int)stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.DMAInfo_p[Buffers].signal_threshold)
		    & DMA_INFO_ALLOW_OVERFLOW) {
			printer(ctx, "YES     :");
		} else {
			printer(ctx, "NO      :");
		}
		printer(ctx, " %02x", stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.DMAOverflowFlags_p[Buffers]));

		printer(ctx, "                : #%04u\n", Buffers);
	}
	return 0;
}

int stptiHAL_Print_IndexerInfo(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	/*
	 * This is only the first part of SlotInfo - the last part depends on
	 * the slot type and so is currently ignored
	 */
	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	printer(ctx,
		"(abs)       PclSel   IdxrCfg  EventM   AF_M DMA# "
		"______________________MPEG_START_CODE_MASKS____________________________ " "IdxCount\n");
	stptiHAL_PrintMemory(ctx, printer,
			     "#######4 #######4 #######4 ###2 ###2 #######4 #######4 "
			     "#######4 #######4 #######4 #######4 #######4 #######4 "
			     "#######4 #######4",
			     (void *)pDevice_p->TP_Interface.IndexerInfo_p,
			     sizeof(stptiTP_IndexerInfo_t), pDevice_p->TP_Interface.NumberOfIndexers);
	return 0;
}

int stptiHAL_Print_CAMData(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	int FilterOffsetCamAData;
	int FilterOffsetCamAMask;
	int FilterOffsetCamBData;
	int FilterOffsetCamBMask;
	U8 *Byte_p;
	unsigned int i, Filter, NumberOfFilters;

	if (stptiHAL_pDeviceCheckRunning(ctx, printer, pDevice_p) != 0)
		return 0;

	FilterOffsetCamAData = 0;
	FilterOffsetCamAMask = FilterOffsetCamAData + stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.SizeOfCAM);
	FilterOffsetCamBData = FilterOffsetCamAMask + stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.SizeOfCAM);
	FilterOffsetCamBMask = FilterOffsetCamBData + stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.SizeOfCAM);

	NumberOfFilters = pDevice_p->TP_Interface.NumberOfSectionFilters;

	printer(ctx, "%u filters at CAM offset %010u\n", NumberOfFilters, FilterOffsetCamAData);
	printer(ctx, "Flt CAM_A_Data/Mask  CAM_B_Data/Mask  NM_A\n");

	for (Filter = 0; Filter < NumberOfFilters; Filter++) {
		int NMC_BitOffset = Filter * 2;	/* Two NMC bits per Filter */
		int NMC_Index = NMC_BitOffset / 64;	/* FilterCAMTable_p->NotMatchControlA is a U64 */

		U64 OneShotFilterMask =
		    stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.FilterCAMTables_p->OneShotFilterMask);

		U64 ForceCRCFilterMask =
		    stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.FilterCAMTables_p->ForceCRCFilterMask);

		U64 NotMatchControlA =
		    stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.FilterCAMTables_p->NotMatchControlA[NMC_Index]);

		U8 NotMatchValueA =
		    stptiHAL_pDeviceXP70Read(&pDevice_p->TP_Interface.FilterCAMTables_p->NotMatchValueA[Filter]);

		NMC_BitOffset = NMC_BitOffset % 64;

		printer(ctx, "%03d ", Filter);
		Byte_p = (U8 *) &pDevice_p->TP_Interface.FilterCAMRegion_p[Filter + FilterOffsetCamAData];

		for (i = 0; i < 8; i++) {
			printer(ctx, "%02x", stptiHAL_pDeviceXP70Read(Byte_p++));
		}

		printer(ctx, " ");

		Byte_p = (U8 *) &pDevice_p->TP_Interface.FilterCAMRegion_p[Filter + FilterOffsetCamBData];

		for (i = 0; i < 8; i++) {
			printer(ctx, "%02x", stptiHAL_pDeviceXP70Read(Byte_p++));
		}

		printer(ctx, " %d", (unsigned)(NotMatchControlA >> NMC_BitOffset) & 3);
		printer(ctx, " %02x", (unsigned)NotMatchValueA);

		if (((OneShotFilterMask >> Filter) & 1)) {
			printer(ctx, " : ONESHOT");
		}

		if (((ForceCRCFilterMask >> Filter) & 1)) {
			printer(ctx, " : FORCECRC");
		}

		printer(ctx, "\n    ");

		Byte_p = (U8 *) &pDevice_p->TP_Interface.FilterCAMRegion_p[Filter + FilterOffsetCamAMask];
		for (i = 0; i < 8; i++) {
			printer(ctx, "%02x", stptiHAL_pDeviceXP70Read(Byte_p++));
		}

		printer(ctx, " ");

		Byte_p = (U8 *) &pDevice_p->TP_Interface.FilterCAMRegion_p[Filter + FilterOffsetCamBMask];
		for (i = 0; i < 8; i++) {
			printer(ctx, "%02x", stptiHAL_pDeviceXP70Read(Byte_p++));
		}
		printer(ctx, "\n");
	}
	return 0;
}

int stptiHAL_Print_FullObjectTree(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	stptiHAL_PrintObjectTreeRecurse(ctx, printer, pDevice_p->ObjectHeader.Handle, 0);
	return 0;
}

int stptiHAL_Print_PrintBuffer(void *ctx, debug_print printer, void *ignoreme)
{
	char buf[256];
	int res;

	stptiSUPPORT_ResetPrintfRdPointer();

	do {
		res = stptiSUPPORT_ReturnPrintfBuffer(buf, sizeof(buf));
		printer(ctx, "%s", buf);
	} while (res == (sizeof(buf) - 1));

	return 0;
}

int stptiHAL_Print_TSInput(void *ctx, debug_print printer, void *streamID)
{
	U32 SID = (U32) streamID;
	char buf[256];
	int eof = 1;
	int res = 0;
	int offset = 0;
	do {
		stptiTSHAL_call(TSInput.TSHAL_TSInputIBRegisterDump,
				0, SID, FALSE, buf, &res, sizeof(buf), offset, &eof);
		offset += res;
		printer(ctx, "%s", buf);
	} while (eof != 1);

	return 0;
}

const char *stptiHAL_ReturnXP70Status(stptiHAL_STxP70_Status_t StatusNumber)
{
	switch (StatusNumber) {
	case stptiHAL_STXP70_NO_FIRMWARE:
		return ("NO_FIRMWARE");

	case stptiHAL_STXP70_STOPPED:
		return ("STOPPED");

	case stptiHAL_STXP70_AWAITING_INITIALISATION:
		return ("AWAITING_INIT");

	case stptiHAL_STXP70_RUNNING:
		return ("RUNNING");

	case stptiHAL_STXP70_PREPARING_TO_SLEEP:
		return ("PREPARING_TO_SLEEP");

	case stptiHAL_STXP70_SLEEPING:
		return ("SLEEPING");

	case stptiHAL_STXP70_POWERDOWN:
		return ("POWERDOWN");

	default:
		return ("UNKNOWN_STATE");
	}
}

const char *stptiHAL_ReturnObjectString(ObjectType_t ObjectType)
{
	switch (ObjectType) {
	case OBJECT_TYPE_PDEVICE:
		return ("pDevice");

	case OBJECT_TYPE_VDEVICE:
		return ("vDevice");

	case OBJECT_TYPE_SESSION:
		return ("Session");

	case OBJECT_TYPE_SOFTWARE_INJECTOR:
		return ("SwInjector");

	case OBJECT_TYPE_BUFFER:
		return ("Buffer");

	case OBJECT_TYPE_FILTER:
		return ("Filter");

	case OBJECT_TYPE_SIGNAL:
		return ("Signal");

	case OBJECT_TYPE_SLOT:
		return ("Slot");

	case OBJECT_TYPE_INDEX:
		return ("Index");

	case OBJECT_TYPE_DATA_ENTRY:
		return ("DataEntry");

	case OBJECT_TYPE_CONTAINER:
		return ("Container");

	case OBJECT_TYPE_ANY:
	case OBJECT_TYPE_INVALID:
		return ("*** OBJECT_TYPE_INVALID ***");

	default:
		break;
	}

	return ("*** OBJECT_TYPE_UNKNOWN ****");
}

/**
   @brief  Prints an objects details, and calls itself for any child objects

   Prints the details of an object and calls itself for the object's children.

   @param  ctx                 Context for printing
   @param  printer             Printer function pointer
   @param  RootObjectHandle    Handle of object to start tree print from
  */
void stptiHAL_PrintObjectTreeRecurse(void *ctx, debug_print printer, FullHandle_t RootObjectHandle, int level)
{
	int i;
	ObjectType_t ObjectType = stptiOBJMAN_GetObjectType(RootObjectHandle);
	BOOL PrintAssociations = TRUE;
	Object_t *Object_p;

	if (ObjectType == OBJECT_TYPE_PDEVICE) {
		/* For pDevices this variable is ONLY use for the printf below */
		Object_p = (Object_t *) stptiHAL_GetObjectpDevice_p(RootObjectHandle);
	} else {
		Object_p = stptiOBJMAN_HandleToObjectPointer(RootObjectHandle,
							     stptiOBJMAN_GetObjectType(RootObjectHandle));
	}

	printer(ctx, "0x%08x [%08x]  ", (unsigned)Object_p, RootObjectHandle.word);

	for (i = 0; i < level; i++) {
		printer(ctx, " ");
	}

	switch (ObjectType) {
	case OBJECT_TYPE_PDEVICE:
		printer(ctx, "pDevice%d\n", RootObjectHandle.member.pDevice);
		PrintAssociations = FALSE;
		/* Step through each vDevice */
		{
			int index, vDevices;
			FullHandle_t vDeviceHandleArray[MAX_NUMBER_OF_VDEVICES];

			vDevices = stptiOBJMAN_ReturnChildObjects(RootObjectHandle,
								  vDeviceHandleArray, MAX_NUMBER_OF_VDEVICES,
								  OBJECT_TYPE_VDEVICE);

			for (index = 0; index < vDevices; index++) {
				/* Note this is recursive */
				stptiHAL_PrintObjectTreeRecurse(ctx, printer, vDeviceHandleArray[index], level + 1);
			}
		}
		break;

	case OBJECT_TYPE_VDEVICE:

		printer(ctx, "vDevice%d StreamID=%04x Protocol=%d\n",
			RootObjectHandle.member.vDevice,
			((stptiHAL_vDevice_t *) Object_p)->StreamID, ((stptiHAL_vDevice_t *) Object_p)->TSProtocol);

		PrintAssociations = FALSE;
		/* Step through each Session */
		{
			Object_t *SessionObject;
			Object_t *vDeviceObject_p = Object_p;
			int index;

			index = -1;
			do {
				stptiOBJMAN_NextInList(&vDeviceObject_p->ChildObjects, (void *)&SessionObject, &index);
				if (index >= 0) {
					/* Note this is recursive */
					stptiHAL_PrintObjectTreeRecurse(ctx,
									printer,
									stptiOBJMAN_ObjectPointerToHandle
									(SessionObject), level + 1);
				}
			} while (index >= 0);
		}
		break;

	case OBJECT_TYPE_SESSION:
		printer(ctx, "Session%d\n", RootObjectHandle.member.Session);
		PrintAssociations = FALSE;
		/* Step through each Object */
		{
			Object_t *ChildObject;
			Object_t *SessionObject_p = Object_p;
			int index;

			index = -1;
			do {
				stptiOBJMAN_NextInList(&SessionObject_p->ChildObjects, (void *)&ChildObject, &index);
				if (index >= 0) {
					/* Note this is recursive */
					stptiHAL_PrintObjectTreeRecurse(ctx,
									printer,
									stptiOBJMAN_ObjectPointerToHandle(ChildObject),
									level + 1);
				}
			} while (index >= 0);
		}
		break;

	case OBJECT_TYPE_SOFTWARE_INJECTOR:
		printer(ctx, "%s%d Channel=%d IsActive=%d NC=%d INC=%d",
			stptiHAL_ReturnObjectString(ObjectType),
			(unsigned)RootObjectHandle.member.Object,
			((stptiHAL_SoftwareInjector_t *) Object_p)->Channel,
			((stptiHAL_SoftwareInjector_t *) Object_p)->IsActive,
			((stptiHAL_SoftwareInjector_t *) Object_p)->NodeCount,
			((stptiHAL_SoftwareInjector_t *) Object_p)->InjectionNodeCount);
		break;

	case OBJECT_TYPE_BUFFER:
		printer(ctx, "%s%d DMA=%d,Size=0x%x",
			stptiHAL_ReturnObjectString(ObjectType),
			(unsigned)RootObjectHandle.member.Object,
			((stptiHAL_Buffer_t *) Object_p)->BufferIndex, ((stptiHAL_Buffer_t *) Object_p)->BufferSize);
		break;

	case OBJECT_TYPE_FILTER:
		printer(ctx, "%s%d Idx=%d,Type=%d",
			stptiHAL_ReturnObjectString(ObjectType),
			(unsigned)RootObjectHandle.member.Object,
			((stptiHAL_Filter_t *) Object_p)->FilterIndex, ((stptiHAL_Filter_t *) Object_p)->FilterType);
		break;

	case OBJECT_TYPE_SLOT:
		printer(ctx, "%s%d SlotIdx=%d,Mode=%d,Pid=0x%04x,PidIdx=%d",
			stptiHAL_ReturnObjectString(ObjectType),
			(unsigned)RootObjectHandle.member.Object,
			((stptiHAL_Slot_t *) Object_p)->SlotIndex,
			((stptiHAL_Slot_t *) Object_p)->SlotMode, ((stptiHAL_Slot_t *) Object_p)->PID,
			((stptiHAL_Slot_t *) Object_p)->PidIndex);
		break;

	case OBJECT_TYPE_CONTAINER:
	case OBJECT_TYPE_SIGNAL:
	case OBJECT_TYPE_INDEX:
	case OBJECT_TYPE_DATA_ENTRY:
		printer(ctx, "%s%d", stptiHAL_ReturnObjectString(ObjectType), (unsigned)RootObjectHandle.member.Object);
		break;

	case OBJECT_TYPE_ANY:
	case OBJECT_TYPE_INVALID:
		PrintAssociations = FALSE;
		printer(ctx, "*** OBJECT_TYPE_INVALID ***");
		break;

	default:
		PrintAssociations = FALSE;
		printer(ctx, "*** OBJECT_TYPE_UNKNOWN %d ***", ObjectType);
		break;
	}

	if (PrintAssociations) {
		Object_t *AssocObject_p;
		int index;
		int count = 0;

		stptiOBJMAN_FirstInList(&Object_p->AssociatedObjects, (void *)&AssocObject_p, &index);
		while (index >= 0) {
			if (count == 0) {
				printer(ctx, "  {");
			} else {
				printer(ctx, ",");
			}
			printer(ctx, "%s%d",
				stptiHAL_ReturnObjectString(AssocObject_p->Handle.member.ObjectType),
				(unsigned)AssocObject_p->Handle.member.Object);
			count++;
			stptiOBJMAN_NextInList(&Object_p->AssociatedObjects, (void *)&AssocObject_p, &index);
		}
		if (count > 0) {
			printer(ctx, "}");
		}
		printer(ctx, "\n");
	}
}

/**
   @brief  Pretty prints an array of bytes following a template.

   This function is used to print out objects (2 dimensional arrays of bytes). The template
   expresses how the string is printed and output of each line is limited by template or by the
   ObjectSize.  Each row of text is a single object, so NumberOfObjects indicates the number of rows.

   The template follows the following format...
      - the space character is printed as is
      - # character is ignored used for padding out the template to match headers
      - . character is also ignored
      - 1 print a U8
      - 2 print a (LE) U16
      - 4 print a (LE) U32
      - * print the rest of the object as bytes

   The output is also capped by the size of the destination string (detailed in ctx_p).

   @param  ctx                 Context for printing
   @param  printer             Printer function pointer
   @param  template            Template used for printing (see above)
   @param  FirstObjectAddress  Starting address of byte array
   @param  ObjectSize          Size of the object (in bytes) this corresponds to the number of bytes
                               from the byte array that will be printed in a row.
   @param  NumberOfObjects     The number of objects (effectively the number of objects).

 */
void stptiHAL_PrintMemory(void *ctx, debug_print printer, const char *template, void *FirstObjectAddress,
			  size_t ObjectSize, int NumberOfObjects)
{
	U8 *Byte_p, *FirstObject_p = (U8 *) FirstObjectAddress;
	int ObjectIndex, TemplateIndex;
	int NumberOfBytesLeft;
	char c;
	unsigned int number;

	for (ObjectIndex = 0; ObjectIndex < NumberOfObjects; ObjectIndex++) {
		Byte_p = FirstObject_p + (ObjectIndex * ObjectSize);
		printer(ctx, "#%08x:  ", (U32) Byte_p);
		TemplateIndex = 0;
		NumberOfBytesLeft = (int)ObjectSize;
		while ((c = template[TemplateIndex++]) != 0) {
			switch (c) {
			case '1':	/* print U8 */
				NumberOfBytesLeft -= 1;
				if (NumberOfBytesLeft >= 0) {
					number = (unsigned int)stptiHAL_pDeviceXP70Read(Byte_p);
					printer(ctx, "%02x", number);
					Byte_p += 1;
				}
				break;

			case '2':	/* print (LE) U16 */
				NumberOfBytesLeft -= 2;
				if (NumberOfBytesLeft >= 0) {
					number = (unsigned int)stptiHAL_pDeviceXP70Read((U16 *) Byte_p);
					printer(ctx, "%04x", number);
					Byte_p += 2;
				}
				break;

			case '4':	/* print (LE) U32 */
				NumberOfBytesLeft -= 4;
				if (NumberOfBytesLeft >= 0) {
					number = (unsigned int)stptiHAL_pDeviceXP70Read((U32 *) Byte_p);
					printer(ctx, "%08x", number);
					Byte_p += 4;
				}
				break;

			case '*':	/* print remainder as bytes */
				while (NumberOfBytesLeft > 0) {
					number = (unsigned int)stptiHAL_pDeviceXP70Read(Byte_p);
					printer(ctx, "%02x", number);
					Byte_p += 1;
					NumberOfBytesLeft -= 1;
				}
				break;

			case '#':	/* Used for padding */
			case '.':
				break;

			default:
				printer(ctx, " ");
				break;
			}
		}
		printer(ctx, "    ; #%02x %c\n", ObjectIndex, NumberOfBytesLeft == 0 ? ' ' : '*');
	}
}

int stptiHAL_pDeviceCheckRunning(void *ctx, debug_print printer, stptiHAL_pDevice_t * pDevice_p)
{
	if (pDevice_p == NULL) {
		printer(ctx, "Physical device not allocated.\n");
		return -1;
	}

	if (pDevice_p->TP_Status != stptiHAL_STXP70_RUNNING) {
		const char *TP_StatusString = stptiHAL_ReturnXP70Status(pDevice_p->TP_Status);

		printer(ctx, "The transport processor memory "
			"interface is unavailable.  TP status = %-20s\n", TP_StatusString);
		return -1;
	}
	return 0;
}
