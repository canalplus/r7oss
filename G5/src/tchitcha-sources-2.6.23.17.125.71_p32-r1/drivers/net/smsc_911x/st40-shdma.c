#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <linux/stm/stm-dma.h>

static struct stm_dma_params tx_transfer;

static void err_cb(unsigned long);
static DWORD smsc911x_request_dma(const char* chan);

#if defined (CONFIG_SMSC911x_DMA_PACED)
/* Ideally whould be: pDmaXfer->dwDwCnt*2
 * Next best would be: MAX_RX_SKBS*2
 * but for now use: */
#define MAX_NODELIST_LEN 20
static struct stm_dma_params rx_transfer_paced[MAX_NODELIST_LEN];

#define SYSCONF_DEVID 0xb9001000

#define SMSC_SHORT_PTK_CHAN 1
#define SMSC_LONG_PTK_CHAN 0

static struct stm_dma_req_config dma_req_configs[2] = {
{
	/* Long packet: 4*read32 */
	.rw		= REQ_CONFIG_READ,
	.opcode		= REQ_CONFIG_OPCODE_32,
	.count		= 4,
	.increment	= 0,
	.hold_off	= 0,
	.initiator	= 1,
}, {
	/* Short packet: 1*read32 */
	.rw		= REQ_CONFIG_READ,
	.opcode		= REQ_CONFIG_OPCODE_32,
	.count		= 1,
	.increment	= 0,
	.hold_off	= 0,
	.initiator	= 1,
}};

static struct stm_dma_req *dma_reqs[2];

DWORD Platform_RequestDmaChannelSg(
	PPLATFORM_DATA platformData)
{
	DWORD chan;
	int devid = ctrl_inl(SYSCONF_DEVID);
	int chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);
	int dma_req_lines[2];

	chan = smsc911x_request_dma(STM_DMA_CAP_ETH_BUF);
	if (chan < 0)
		return chan;

	if(chip_7109){
		dma_req_lines[SMSC_LONG_PTK_CHAN] = 10;
		dma_req_lines[SMSC_SHORT_PTK_CHAN] = 11;
	}
	else {
		dma_req_lines[SMSC_LONG_PTK_CHAN] = 12;
		dma_req_lines[SMSC_SHORT_PTK_CHAN] = 13;
	}

	dma_reqs[0] = dma_req_config(chan, dma_req_lines[0], &dma_req_configs[0]);
	dma_reqs[1] = dma_req_config(chan, dma_req_lines[1], &dma_req_configs[1]);

	return chan;
}

static void Platform_ReleaseDmaChannel_sg(void)
{
	int i;

	for(i=0;i<MAX_NODELIST_LEN;i++)
		dma_params_free(&rx_transfer_paced[i]);

	dma_req_free(dwDmaChannel, dma_reqs[0]);
	dma_req_free(dwDmaChannel, dma_reqs[1]);
}

static void Platform_DmaInitialize_sg(void)
{
	int i;

	SMSC_TRACE("DMA Rx using paced transfers to main register bank");

	for(i=0;i<MAX_NODELIST_LEN;i++){
		dma_params_init(&rx_transfer_paced[i],
			       MODE_PACED,
			       STM_DMA_LIST_OPEN);
	}
	dma_params_err_cb(&rx_transfer_paced[0], err_cb, 0, STM_DMA_CB_CONTEXT_TASKLET);
}
#else
static struct stm_dma_params rx_transfer_sg;

DWORD Platform_RequestDmaChannelSg(
	PPLATFORM_DATA platformData)
{
	return smsc911x_request_dma(STM_DMA_CAP_LOW_BW);
}

static void Platform_ReleaseDmaChannel_sg(void)
{
	dma_params_free(&rx_transfer_sg);
}

static void Platform_DmaInitialize_sg(void)
{
	dma_params_init(&rx_transfer_sg,
		       MODE_DST_SCATTER,
		       STM_DMA_LIST_OPEN);
	dma_params_err_cb(&rx_transfer_sg, err_cb, 0, STM_DMA_CB_CONTEXT_TASKLET);
#if defined(CONFIG_SMSC911x_DMA_2D)
	SMSC_TRACE("DMA Rx using freefrunning 2D transfers");
	dma_params_DIM_2_x_1(&rx_transfer_sg,0x20,0);
#elif defined(CONFIG_SMSC911x_DMA_FIFOSEL)
	SMSC_TRACE("DMA Rx using freefrunning 1D transfers and FIFOSEL");
	dma_params_DIM_1_x_1(&rx_transfer_sg);
#else
#error Unknown DMA mode
#endif
}
#endif

BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh)
{
	if ((dwDmaCh >= 0) && (dwDmaCh < TRANSFER_PIO))
		return TRUE;
	return FALSE;
}

DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData)
{
	return smsc911x_request_dma(STM_DMA_CAP_LOW_BW);
}

static DWORD smsc911x_request_dma(const char* cap)
{
	int chan;
	const char * dmac_id[] = { STM_DMAC_ID, NULL };
	const char * cap_channel[] = { cap, NULL };

	chan = request_dma_bycap(dmac_id, cap_channel, "smsc911x");
	if (chan < 0)
		return TRANSFER_PIO;
	return chan;
}

void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel)
{
	free_dma(dwDmaChannel);
	dma_params_free(&tx_transfer);
	Platform_ReleaseDmaChannel_sg();
}

static void err_cb(unsigned long dummy)
{
	printk("DMA err callback");
}

/* This gets called twice, once each for for Rx and Tx channels */
BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh)
{
	/* From memory to LAN */
	dma_params_init(  	&tx_transfer,
				MODE_FREERUNNING,
			       	STM_DMA_LIST_OPEN);
	dma_params_err_cb(&tx_transfer, err_cb, 0, STM_DMA_CB_CONTEXT_TASKLET);
	dma_params_DIM_1_x_2(&tx_transfer,0x20,0);

	Platform_DmaInitialize_sg();

	return TRUE;
}


BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	DWORD dwAlignMask;
	DWORD dwLanPhysAddr, dwMemPhysAddr;
	struct stm_dma_params *dmap;
	unsigned long src, dst;
	unsigned long res=0;
	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// 2. make sure the channel's not already running
	if (dma_get_status(pDmaXfer->dwDmaCh) != DMA_CHANNEL_STATUS_IDLE)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	dwLanPhysAddr = pDmaXfer->dwLanReg;
	// 3. calculate the physical transfer addresses
	dwMemPhysAddr = dma_map_single(NULL, pDmaXfer->pdwBuf,
				sizeof(pDmaXfer->pdwBuf), DMA_TO_DEVICE);

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

	// 5. Prepare the DMA channel structure
	BUG_ON(pDmaXfer->fMemWr);
	src = dwMemPhysAddr;
	dst = dwLanPhysAddr;
	dmap = &tx_transfer;

	dma_params_comp_cb(dmap,
			   (void (*)(unsigned long))pCallback,
			   (unsigned long)pCallbackData,
			   STM_DMA_CB_CONTEXT_TASKLET);
	dma_params_addrs(dmap,src,dst, pDmaXfer->dwDwCnt << 2);
	res=dma_compile_list(pDmaXfer->dwDmaCh, dmap, GFP_ATOMIC);
	if(res != 0)
		goto err_exit;
	// 6. Start the transfer
	res=dma_xfer_list(pDmaXfer->dwDmaCh,dmap);
	if(res != 0)
		goto err_exit;

	// DMA Transfering....
	return TRUE;
err_exit:
	SMSC_WARNING("%s cant initialise DMA engine err_code %ld\n",__FUNCTION__, res);
	return FALSE;
}

#if defined (CONFIG_SMSC911x_DMA_PACED)

BOOLEAN Platform_DmaStartSgXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	int res=0;
	int sg_count;
	struct scatterlist *sg;

	struct stm_dma_params *param;

	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// Validate this is a LAN to memory transfer
	SMSC_ASSERT(pDmaXfer->fMemWr)

	// 2. make sure the channel's not already running
	if (dma_get_status(pDmaXfer->dwDmaCh) != DMA_CHANNEL_STATUS_IDLE)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	// 4. Map (flush) the buffer
	sg = (struct scatterlist*)pDmaXfer->pdwBuf;
	sg_count = dma_map_sg(NULL, sg,
			      pDmaXfer->dwDwCnt,
			      pDmaXfer->fMemWr ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	// 5. Prepare the DMA channel structure
	param = rx_transfer_paced;
	for ( ; sg_count; sg_count--) {
		int long_len = sg_dma_len(sg) & (~127);
		int short_len = sg_dma_len(sg) & 127;

		if (long_len) {
			dma_params_DIM_0_x_1(param);
			dma_params_addrs(param,
					pDmaXfer->dwLanReg,
					sg_dma_address(sg),
					long_len);
			dma_params_req(param, dma_reqs[SMSC_LONG_PTK_CHAN]);
			dma_params_link(param, param+1);
			param++;
		}

		if (short_len) {
			dma_params_DIM_0_x_1(param);
			dma_params_addrs(param,
					pDmaXfer->dwLanReg,
					sg_dma_address(sg) + long_len,
					short_len);
			dma_params_req(param, dma_reqs[SMSC_SHORT_PTK_CHAN]);
			dma_params_link(param, param+1);
			param++;
		}

		sg++;
	}

	param--;
	dma_params_link(param, NULL);

	dma_params_comp_cb(rx_transfer_paced,
			   (void (*)(unsigned long))pCallback,
			   (unsigned long)pCallbackData,
			   STM_DMA_CB_CONTEXT_TASKLET);
	res=dma_compile_list(pDmaXfer->dwDmaCh, rx_transfer_paced, GFP_ATOMIC);
	if(res != 0)
		goto err_exit;
	// 6. Start the transfer
	dma_xfer_list(pDmaXfer->dwDmaCh,rx_transfer_paced);

	// DMA Transfering....
	return TRUE;
err_exit:
	SMSC_WARNING("%s cant initialise DMA engine err_code %d\n",__FUNCTION__,(int)res);
	return FALSE;
}

#else

BOOLEAN Platform_DmaStartSgXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{
	DWORD dwLanPhysAddr;
	int res;
	int sg_count;

	// 1. validate the requested channel #
	SMSC_ASSERT(Platform_IsValidDmaChannel(pDmaXfer->dwDmaCh))

	// Validate this is a LAN to memory transfer
	SMSC_ASSERT(pDmaXfer->fMemWr)

	// 2. make sure the channel's not already running
	if (dma_get_status(pDmaXfer->dwDmaCh) != DMA_CHANNEL_STATUS_IDLE)
	{
		SMSC_WARNING("Platform_DmaStartXfer -- requested channel (%ld) is still running", pDmaXfer->dwDmaCh);
		return FALSE;
	}

	// 3. calculate the physical transfer addresses
	dwLanPhysAddr = pDmaXfer->dwLanReg;

#ifdef CONFIG_SMSC911x_DMA_FIFOSEL
	dwLanPhysAddr += (1<<16);
#endif

	// 4. Map (flush) the buffer
	sg_count = dma_map_sg(NULL, (struct scatterlist*)pDmaXfer->pdwBuf,
			      pDmaXfer->dwDwCnt,
			      pDmaXfer->fMemWr ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	// 5. Prepare the DMA channel structure
	dma_params_comp_cb(&rx_transfer_sg,
			   (void (*)(unsigned long))pCallback,
			   (unsigned long)pCallbackData,
			   STM_DMA_CB_CONTEXT_TASKLET);
	dma_params_addrs(&rx_transfer_sg, dwLanPhysAddr, 0, 0);
	dma_params_sg(&rx_transfer_sg, (struct scatterlist*)pDmaXfer->pdwBuf, sg_count);
	res=dma_compile_list(pDmaXfer->dwDmaCh, &rx_transfer_sg, GFP_ATOMIC);
	if(res != 0)
		goto err_exit;

	// 6. Start the transfer
	dma_xfer_list(pDmaXfer->dwDmaCh, &rx_transfer_sg);

	// DMA Transfering....
	return TRUE;
err_exit:
	SMSC_WARNING("%s cant initialise DMA engine err_code %d\n",__FUNCTION__,(int)res);
	return FALSE;
}

#endif
