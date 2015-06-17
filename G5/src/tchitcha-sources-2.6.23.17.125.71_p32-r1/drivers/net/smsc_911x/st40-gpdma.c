/*********/
/* GPDMA */
/*********/
#define DMA_BASE_ADDR		0xB9161000UL
#define DMA_CHAN_BASE_ADDR(n)	(DMA_BASE_ADDR + ((n + 1) * 0x100UL))
#define DMA_VCR_STATUS		(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x00UL))
#define DMA_VCR_VERSION		(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x08UL))
#define DMA_GLOBAL_ENABLE	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x10UL))
#define DMA_GLOBAL_DISABLE	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x18UL))
#define DMA_GLOBAL_STATUS	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x20UL))
#define DMA_GLOBAL_INTERRUPT	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x28UL))
#define DMA_GLOBAL_ERROR	(*(volatile unsigned long *)(DMA_BASE_ADDR + 0x30UL))
#define DMA_CHAN_ID(n)		(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n)))
#define DMA_CHAN_ENABLE(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x08UL))
#define DMA_CHAN_DISABLE(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x10UL))
#define DMA_CHAN_STATUS(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x18UL))
#define DMA_CHAN_ACTION(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x20UL))
#define DMA_CHAN_POINTER(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x28UL))
#define DMA_CHAN_REQUEST(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x30UL))
#define DMA_CHAN_CONTROL(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x80UL))
#define DMA_CHAN_COUNT(n)	(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x88UL))
#define DMA_CHAN_SAR(n)		(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x90UL))
#define DMA_CHAN_DAR(n)		(*(volatile unsigned long *)(DMA_CHAN_BASE_ADDR(n) + 0x98UL))

/* DMAC bitmasks */
#define DMA_GLOBAL_ENABLE_CHAN_(n)		(0x00000001UL << n)
#define DMA_GLOBAL_DISABLE_CHAN_(n)		(0x00000001UL << n)
#define DMA_CHAN_ENABLE_CHAN_			0x00000001UL
#define DMA_CHAN_DISABLE_ALL_			0x0000003FUL
#define DMA_CHAN_CONTROL_FREE_RUNNING_		0x00000000UL
#define DMA_CHAN_CONTROL_TRIGGER_		0x00000001UL
#define DMA_CHAN_CONTROL_PACED_SOURCE_		0x00000002UL
#define DMA_CHAN_CONTROL_PACED_DESTINATION_	0x00000003UL
#define DMA_CHAN_CONTROL_NO_LINK_LIST_		0x00000000UL
#define DMA_CHAN_CONTROL_FINAL_LINK_ELEM_	0x00000000UL
#define DMA_CHAN_CONTROL_LINK_ELEM_		0x00000080UL
#define DMA_CHAN_CONTROL_SRC_TYPE_CONST_	0x00000000UL
#define DMA_CHAN_CONTROL_SRC_ADDRESSMODE_INC_	0x00010000UL
#define DMA_CHAN_CONTROL_SRC_UNIT_2BYTES_	0x00080000UL
#define DMA_CHAN_CONTROL_SRC_UNIT_4BYTES_	0x00100000UL
#define DMA_CHAN_CONTROL_SRC_UNIT_32BYTES_	0x00280000UL
#define DMA_CHAN_CONTROL_DST_TYPE_CONST_	0x00000000UL
#define DMA_CHAN_CONTROL_DST_ADDRESSMODE_INC_	0x01000000UL
#define DMA_CHAN_CONTROL_DST_UNIT_2BYTES_	0x08000000UL
#define DMA_CHAN_CONTROL_DST_UNIT_4BYTES_	0x10000000UL
#define DMA_CHAN_CONTROL_DST_UNIT_32BYTES_	0x28000000UL
#define DMA_CHAN_ACTION_COMPLETE_ACK_		0x00000002UL
#define DMA_CHAN_STATUS_COMPLETE_		0x00000002UL
#define DMA_CHAN_REQUEST0			0x0
#define DMA_CHAN_REQUEST1			0x1
#define DMA_CHAN_REQUEST2			0x2
#define DMA_CHAN_REQUEST3			0x3

BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh)
{
	/* for sh4/st40 only use channels 1-4, do not use channel 0 */
	if((dwDmaCh >= 1) && (dwDmaCh <= 4)) {
		return TRUE;
	}
	return FALSE;
}

BOOLEAN Platform_DmaDisable(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh)
{
	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh))

	DMA_CHAN_ACTION(dwDmaCh) = DMA_CHAN_ACTION_COMPLETE_ACK_;

	// Disable DMA controller
	DMA_CHAN_DISABLE(dwDmaCh) = DMA_CHAN_DISABLE_ALL_;
	return TRUE;
}

DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData)
{
	return TRANSFER_PIO;
}

DWORD Platform_RequestDmaChannelSg(
	PPLATFORM_DATA platformData)
{
	return TRANSFER_PIO;
}

void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel)
{
	//since Platform_RequestDmaChannel
	//  never returns a dma channel
	//  then this function should never be called
	SMSC_ASSERT(FALSE);
}

BOOLEAN Platform_IsDmaComplete(
	const DWORD dwDmaCh)
{
	// channel is disable
	if ((DMA_CHAN_ENABLE(dwDmaCh) &  DMA_CHAN_ENABLE_CHAN_) == 0UL)
		return TRUE;
	if ((DMA_CHAN_STATUS(dwDmaCh) & DMA_CHAN_STATUS_COMPLETE_) != 0UL) {
		Platform_DmaDisable ((PPLATFORM_DATA) 0, dwDmaCh);
		return TRUE;
	}
	else
		return FALSE;
}

void Platform_DmaComplete(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh)
{
	DWORD dwTimeOut = 1000000;
	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh))

	// channel is disable
	if ((DMA_CHAN_ENABLE(dwDmaCh) &  DMA_CHAN_ENABLE_CHAN_) == 0UL)
		return;

	while((dwTimeOut) && ((DMA_CHAN_STATUS(dwDmaCh) & DMA_CHAN_STATUS_COMPLETE_) == 0UL))
	{
		udelay(1);
		dwTimeOut--;
	}
	Platform_DmaDisable(platformData, dwDmaCh);
	if(dwTimeOut == 0)
	{
		SMSC_WARNING("Platform_DmaComplete: Timed out");
	}
}

DWORD Platform_DmaGetDwCnt(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh)
{
	DWORD dwCount;

	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh));
	if (Platform_IsDmaComplete(dwDmaCh) == TRUE)
		return 0UL;
	else {
		dwCount = DMA_CHAN_COUNT(dwDmaCh);
		if (dwCount)
			return dwCount;
		else
			return 1UL;
	}
}

BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh)
{
	SMSC_ASSERT(Platform_IsValidDmaChannel(dwDmaCh))

	DMA_CHAN_DISABLE(dwDmaCh) = DMA_CHAN_DISABLE_ALL_;
	DMA_CHAN_COUNT(dwDmaCh) = 0;
	DMA_GLOBAL_ENABLE = DMA_GLOBAL_ENABLE_CHAN_(dwDmaCh);
	DMA_CHAN_STATUS(dwDmaCh) = 0UL;		/* no request outstanding */
	DMA_CHAN_ACTION(dwDmaCh) = DMA_CHAN_ACTION_COMPLETE_ACK_;
	DMA_CHAN_REQUEST(dwDmaCh) = (dwDmaCh & 3);	/* req# == chan# */

	SMSC_TRACE("Platform_DmaInitialize -- initialising channel %ld", dwDmaCh);
	SMSC_TRACE("Platform_DmaInitialize -- DMA_CHANn_ENABLE=0x%08lX", DMA_CHAN_ENABLE(dwDmaCh));
	SMSC_TRACE("Platform_DmaInitialize -- DMA_CHANn_COUNT=0x%08lX", DMA_CHAN_COUNT(dwDmaCh));
	SMSC_TRACE("Platform_DmaInitialize -- DMA_GLOBAL_ENABLE=0x%08lX", DMA_GLOBAL_ENABLE);
	SMSC_TRACE("Platform_DmaInitialize -- DMA_ERROR=0x%08lX", DMA_GLOBAL_ERROR);

	return TRUE;
}

BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	DWORD dwSrcAddr, dwDestAddr;
	DWORD dwAlignMask, dwControlRegister;
	DWORD dwLanPhysAddr, dwMemPhysAddr;

	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// 2. make sure the channel's not already running
	if (DMA_CHAN_COUNT(pDmaXfer->dwDmaCh) != 0UL)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	// 3. calculate the physical transfer addresses
	dwLanPhysAddr = CpuToPhysicalAddr((void *)pDmaXfer->dwLanReg);
	dwMemPhysAddr = 0x1fffffffUL & CpuToPhysicalAddr((void *)pDmaXfer->pdwBuf);

	// 4. validate the address alignments
	// need CL alignment for CL bursts
	dwAlignMask = (PLATFORM_CACHE_LINE_BYTES - 1UL);

	if ((dwLanPhysAddr & dwAlignMask) != 0UL)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- bad dwLanPhysAddr (0x%08lX) alignment", dwLanPhysAddr);
		return FALSE;
	}

	if ((dwMemPhysAddr & dwAlignMask) != 0UL)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- bad dwMemPhysAddr (0x%08lX) alignment", dwMemPhysAddr);
		return FALSE;
	}

	// 6. Config Control reg
	// Disable the selected channel first
	DMA_CHAN_DISABLE(pDmaXfer->dwDmaCh) = DMA_CHAN_DISABLE_ALL_;

	// Select correct ch and set SRC, DST and counter
	dwControlRegister = DMA_CHAN_CONTROL_FREE_RUNNING_ |
		DMA_CHAN_CONTROL_NO_LINK_LIST_ |
		DMA_CHAN_CONTROL_SRC_UNIT_32BYTES_ |
		DMA_CHAN_CONTROL_DST_UNIT_32BYTES_;

	if (pDmaXfer->fMemWr == TRUE)
	{
		dwSrcAddr = dwLanPhysAddr;
		dwDestAddr = dwMemPhysAddr;
		dwControlRegister |= (DMA_CHAN_CONTROL_SRC_TYPE_CONST_ | DMA_CHAN_CONTROL_DST_ADDRESSMODE_INC_);
	}
	else
	{
		dwSrcAddr = dwMemPhysAddr;
		dwDestAddr = dwLanPhysAddr;
		dwControlRegister |= (DMA_CHAN_CONTROL_DST_TYPE_CONST_ | DMA_CHAN_CONTROL_SRC_ADDRESSMODE_INC_);
	}

	// Set Source and destination addresses
	DMA_CHAN_SAR(pDmaXfer->dwDmaCh) = dwSrcAddr;
	DMA_CHAN_DAR(pDmaXfer->dwDmaCh) = dwDestAddr;

	// Set the transmit size in terms of the xfer mode
	DMA_CHAN_CONTROL(pDmaXfer->dwDmaCh) = dwControlRegister;
	DMA_CHAN_COUNT(pDmaXfer->dwDmaCh) = (pDmaXfer->dwDwCnt << 2);

	// Enable DMA controller ch x
	DMA_CHAN_ENABLE(pDmaXfer->dwDmaCh) = DMA_CHAN_ENABLE_CHAN_;

	// DMA Transfering....
	return TRUE;
}

