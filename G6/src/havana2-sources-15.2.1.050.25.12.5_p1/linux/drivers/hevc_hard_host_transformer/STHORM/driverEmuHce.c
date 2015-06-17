/*
 * Host driver emulation layer, HCE specific part
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Bruno Lavigueur (bruno.lavigueur@st.com)
 *
 */

#include <asm/cacheflush.h>
#include <linux/dma-direction.h>
#include <linux/semaphore.h>

#include "osdev_time.h"
#include "osdev_mem.h"
#include "osdev_int.h"

#include "driverEmuHce.h"
#include "driverEmuCommon.h"
#include "driver.h"

#include "p2012_rt_conf.h"

/* HAL from $P2012_ESW_HOME/include */
#include "p2012_config_mem.h"
#include "p2012.h"
#include "p2012_memory_map.h"
#include "p2012_pr_control_ipgen_hal.h"
#include "p2012_fc_periph_mailbox_hal.h"
#include "pedf_host_abi.h"
#include "p2012_rab_ipgen_hal1.h"
#include "hades_memory_map.h"

#ifdef CONFIG_HCE //VSOC
#include "eswnative_base_esw.h"
#include "eswnative_generic_esw.h"
#endif

// Cannes
// Chosen by Anthony C. for the Zebu'ized Hades
#define MB_IT_OFFSET  2
#define GEN_IT_OFFSET 0
#define ERR_IT_OFFSET 1
#define RAB_IT_OFFSET 4
// Preprocessor: offset 3

// Orly 2
// #define MB_IT_OFFSET  0
// #define GEN_IT_OFFSET 1
// #define ERR_IT_OFFSET 2
// #define RAB_IT_OFFSET 3

#define MAILBOX_QUEUE_ID_OFFSET	3
#define MAILBOX_QUEUE_ID_MASK	0x1
#define BOOT_MARKER		0xFFFFFFFF // pseudo-answer sent by fabric after its boot

#define P2012_FC_PERIPH_MAILBOX_GET(queue) (P2012_CONF_FC_MBX_BASE + (((unsigned int)(queue))<<MAILBOX_QUEUE_ID_OFFSET))

static p12_msgHeader_t*	lastReceivedMsg_fabric = 0;
static uint32_t 	it_irq_number_base = 0;
static unsigned int	uncachedRegsBase = 0;
static unsigned int     physicalRegsBase = 0;
static unsigned int     physicalRegsSize = 0;
static char		alloc_partition[128]; // BPA2 partition
unsigned int            decode_time = 0;
static struct semaphore CommandReceived; // given by the ISR when a command is received

// Mailbox interrupt handlers
OSDEV_InterruptHandlerEntrypoint(  mb_it_handler );
OSDEV_InterruptHandlerEntrypoint( gen_it_handler );
OSDEV_InterruptHandlerEntrypoint( err_it_handler );
OSDEV_InterruptHandlerEntrypoint( rab_it_handler );

// ============================================================================
// Partial driverEmu.h API implementation, see also driverEmuCommon.c
// ============================================================================

/*
 * Initialize the simulation platform, called before boot
 */
int sthorm_init(uintptr_t regsBaseAddress, uint32_t regsSize, uint32_t interruptNumberBase, char* BPA2partition)
{
    pr_info("STHORM: library initialization(regs %p(%d), IRQ %d, partition %s\n", (void*)regsBaseAddress, regsSize, interruptNumberBase, BPA2partition);
    sthorm_clearRemapTable();

    // TODO should reset Hades here

    // Register interrupt handlers
    sema_init(&CommandReceived, 0);
    it_irq_number_base = interruptNumberBase;

    if(request_irq(it_irq_number_base + MB_IT_OFFSET, mb_it_handler, IRQF_DISABLED | IRQF_TRIGGER_RISING, "hades", NULL) != 0)
    {
	    pr_err("Error: %s STHORM: cannot install interrupt for HADES mailbox IRQ %d\n", __func__, it_irq_number_base + MB_IT_OFFSET);
	    return -1;
    }
    if(request_irq(it_irq_number_base + GEN_IT_OFFSET, gen_it_handler, IRQF_DISABLED | IRQF_TRIGGER_RISING, "hades", NULL) != 0)
    {
    	    pr_err("Error: %s STHORM: cannot install interrupt for HADES generic IRQ %d\n", __func__, it_irq_number_base + GEN_IT_OFFSET);
    	    return -1;
    }
    if(request_irq(it_irq_number_base + ERR_IT_OFFSET, err_it_handler, IRQF_DISABLED | IRQF_TRIGGER_RISING, "hades", NULL) != 0)
    {
    	    pr_err("Error: %s STHORM: cannot install interrupt for HADES error IRQ %d\n", __func__, it_irq_number_base + ERR_IT_OFFSET);
    	    return -1;
    }
    if(request_irq(it_irq_number_base + RAB_IT_OFFSET, rab_it_handler, IRQF_DISABLED | IRQF_TRIGGER_RISING, "hades", NULL) != 0)
    {
    	    pr_err("Error: %s STHORM: cannot install interrupt for HADES rab IRQ %d\n", __func__, it_irq_number_base + RAB_IT_OFFSET);
    	    return -1;
    }

    // Remap register address space
    physicalRegsBase = regsBaseAddress;
    physicalRegsSize = regsSize;

    uncachedRegsBase = (unsigned int)ioremap_nocache(physicalRegsBase, physicalRegsSize);
    if (uncachedRegsBase == 0)
    {
	    pr_err("Error: %s STHORM: cannot remap register address %p\n", __func__, (void*)regsBaseAddress);
	    return -1;
    }

    // Store BPA2 partition for sthorm_malloc
    strncpy(alloc_partition, BPA2partition, sizeof(alloc_partition));
    alloc_partition[sizeof(alloc_partition) - 1] = '\0';
    // pr_info("sthorm_init: partition %s, BPA2 %s\n", alloc_partition, BPA2partition);

    return 0;
}

/*
 * Clean-up
 */
int sthorm_term()
{
	free_irq(it_irq_number_base + MB_IT_OFFSET, NULL);
	free_irq(it_irq_number_base + GEN_IT_OFFSET, NULL);
	free_irq(it_irq_number_base + ERR_IT_OFFSET, NULL);
	free_irq(it_irq_number_base + RAB_IT_OFFSET, NULL);
	it_irq_number_base = 0;
	iounmap((void*)uncachedRegsBase);
	uncachedRegsBase = 0;

	strncpy(alloc_partition, "", sizeof(alloc_partition));
	alloc_partition[sizeof(alloc_partition) - 1] = '\0';

	return 0;
}

/*
 * Boot
 */

static int get_nb_cluster(void)
{
    // FIXME JLX put it in platform data ?
    return 1;
}

void ReadMailbox(void);

int sthorm_boot()
{
    uintptr_t phys_addr;

    // Runtime configuration written at the begining of the FC TCDM
    phys_addr = sthorm_remapFabricToHost(P2012_CONF_FC_TCDM_BASE);
    LIB_TRACE(0, "Configuring runtime struct @ %p\n", (void*)phys_addr);
    /* L3 base and size set to 0 in order to disable the FW dynamic allocations at 0x80000000 (Hades address). */
    init_runtimeConf(phys_addr, get_nb_cluster(), 0, 0);

    // Boot the fabric controller
    phys_addr = sthorm_remapFabricToHost(P2012_CONF_FC_CTRL_BASE);
    fabricCtrl_boot(phys_addr);
    // Once booted the FC will send a message to notify the host. This message is a
    // single word long and the value of the payload is not important.
    // FIXME : make it not a dummy value but a success 0/1
    LIB_TRACE(0, "Waiting for boot message from FC ..\n");
    // down(&CommandReceived);
    {
        int result;

        do
        {
            // result = down_timeout(&CommandReceived, msecs_to_jiffies(5000)); // FIXME JLX: how much timeout ?
            result = down_timeout(&CommandReceived, msecs_to_jiffies(100000)); // SBAG-lite
            if (result == -ETIME)
            {
                pr_err("Error: %s STHORM: (boot) fabric time-out (1 s), LRM %p\n",
                        __func__, lastReceivedMsg_fabric); // FIXME remove lastReceivedMsg_fabric
                // return -1;
                ReadMailbox();
            }
        }
        while (result == -ETIME);
    }

    if ((unsigned int)lastReceivedMsg_fabric != BOOT_MARKER)
        LIB_TRACE(0, "... warning: expected %#08x as a boot message, got %#08x!\n", BOOT_MARKER, (unsigned int)lastReceivedMsg_fabric);
    LIB_TRACE(0, "... FC has booted\n");

    // Base runtime expects a first configuration message to finish his initialization
    // FIXME: remove this if not necessary
    phys_addr = sthorm_remapFabricToHost(P2012_FC_PERIPH_MAILBOX_GET(0));
    LIB_TRACE(0, "Sending first config message to FC, mailbox @ %p\n", (void*)phys_addr);
    mailbox_read_semaphore(phys_addr);
    mailbox_send_message(phys_addr, 0);

    LIB_TRACE(0, "P2012 library initialization done\n");
    return 0;
}

int sthorm_sendMsg(uint32_t msgFabAddr)
{
    // Get the remapped address of mailbox #0 in the FC
    const unsigned int fabricAddr = P2012_FC_PERIPH_MAILBOX_GET(0);
    const uintptr_t fcMboxAddr = sthorm_remapFabricToHost(fabricAddr);

    // pr_info("----- sthorm_sendMsg: fabric %p, host %p\n", (void*)fabricAddr, (void*)fcMboxAddr);
    // Reserve a slot in mailbox FIFO
    // There should be one free since this driver sends only one command at a time (FIXME JLX: multidecode > FIFOsize=4)
    if (tlm_read32(fcMboxAddr + MAILBOX_SEM_BASE_ADDRESS) == 0)
    {
		pr_err("Error: %s STHORM: fabric mailbox full ! message dropped\n", __func__);
		return -1;
    }

    decode_time = OSDEV_GetTimeInMicroSeconds();

    // Write message address in mailbox
    tlm_write32(fcMboxAddr + MAILBOX_QUEUE_BASE_ADDRESS, msgFabAddr);
    return 0;
}

int sthorm_sendSyncMsg(uint32_t msgFabAddr, p12_msgHeader_t *msg)
{
	int result;

	// Send message and wait for answer
	while (down_timeout(&CommandReceived, msecs_to_jiffies(0)) == 0); // FIXME: kernel function to reset semaphore ??

	lastReceivedMsg_fabric = 0; // FIXME remove
	result = sthorm_sendMsg(msgFabAddr);
	if (result != 0)
		return result;
	//do
	{
		// result = down_timeout(&CommandReceived, msecs_to_jiffies(1000)); // FIXME JLX: how much timeout ?
		result = down_timeout(&CommandReceived, msecs_to_jiffies(10000)); // SBAG-lite
		if (result == -ETIME)
		{
			pr_err("Error: %s STHORM: (non-boot) fabric time-out (1 s), LRM %p\n",
					__func__, lastReceivedMsg_fabric); // FIXME remove lastReceivedMsg_fabric
			// return -1;
			ReadMailbox();
			return -1;
		}
	}
	//while (result == -ETIME);

	// check answer
	if (lastReceivedMsg_fabric != (void*)msg)
	{
		pr_err("Error: %s STHORM: internal error in mailbox message sequencing (got reply %p, expected %p)\n",
				__func__, lastReceivedMsg_fabric, msg);
		return -1;
	}

	//

	return 0;
}

#if 0
// JLX: replaced by remapping functions
uint32_t sthorm_transExtMemHostToFabric(uintptr_t hostAddr)
{
    return (uint32_t) hostAddr;
}

uintptr_t sthorm_transExtMemFabricToHost(uint32_t fabricAddr)
{
    return (uintptr_t) fabricAddr;
}
#endif

//
// Hades registers access
//

uint32_t sthorm_phys_to_virt(uint32_t physicalAddr)
{
	unsigned int offset;

	if (physicalAddr < physicalRegsBase || physicalAddr >= physicalRegsBase + physicalRegsSize)
	{
		pr_err("Error: %s STHORM: register address %p out of range %p/%u\n",
				__func__, (void*)physicalAddr, (void*)physicalRegsBase, physicalRegsSize);
		return 0;
	}
	offset = physicalAddr - physicalRegsBase;
	// pr_info("----- JLX: sthorm_phys_to_virt: phys %p, offset %d from %p, to uncached %p\n", (void*)physicalAddr, offset, (void*)physicalRegsBase, (void*)(uncachedRegsBase + offset));
	return uncachedRegsBase + offset;

}

void sthorm_write32(uint32_t physicalAddr, uint32_t value)
{
	// pr_info("JLX: sthorm_write32: *%p = %u\n", (void*) physicalAddr, value);
	writel(value, (volatile void *)sthorm_phys_to_virt(physicalAddr));
}

uint32_t sthorm_read32(uint32_t physicalAddr)
{
	// pr_info("----- JLX: read32 on %p\n", (void*)physicalAddr);
	return (uint32_t)readl((volatile void *)sthorm_phys_to_virt(physicalAddr));
}

void sthorm_write8(uint32_t physicalAddr, uint8_t value)
{
	// pr_info("JLX: sthorm_write32: *%p = %u\n", (void*) physicalAddr, value);
	writeb(value, (volatile void *)sthorm_phys_to_virt(physicalAddr));
}

void sthorm_write_block(uint32_t physicalAddr, uint8_t *value, unsigned int size)
{
	uint32_t virtualAddress = sthorm_phys_to_virt(physicalAddr);

	while (size > 0 && (virtualAddress % 4) != 0)
	{
		writeb(*value, (volatile void *)virtualAddress);
		++ virtualAddress;
		++ value;
		-- size;
	}

	while (size >= 4)
	{
		writel(*(uint32_t*)value, (volatile void *)virtualAddress);
		virtualAddress += 4;
		value += 4;
		size -= 4;
	}

	while (size > 0)
	{
		writeb(*value, (volatile void *)virtualAddress);
		++ virtualAddress;
		++ value;
		-- size;
	}
}

void sthorm_set_block(uint32_t physicalAddr, uint8_t value, unsigned int size)
{
	uint32_t virtualAddress = sthorm_phys_to_virt(physicalAddr);

	while (size > 0)
	{
		writeb(value, (volatile void *)virtualAddress);
		++ virtualAddress;
		-- size;
	}
}

// ============================================================================
// Interrupt handlers
// ============================================================================

/* General purpose IT */
OSDEV_InterruptHandlerEntrypoint(gen_it_handler)
{
    pr_err("Error: %s STHORM: received unexpected IT from the fabric\n", __func__);
    return IRQ_HANDLED;
}

// Interrupt management: See hades_interrupts.h and backbone spirit file c8had1_backbone_regbank.h
#define c8had1_backbone_regbank_C8HAD1_BACKBONE_REGBANK_BASE_ADDR	0
#define c8had1_backbone_regbank_HADES_IT_MASK_FC_mailbox_it_MASK	0x00000001

/* IT from the Fabric Controller mailbox */
OSDEV_InterruptHandlerEntrypoint(mb_it_handler)
{
    uintptr_t mbAddr;
    void*     emptyMessage;
    uint32_t  hades_it_mask;
    uint32_t  hades_it_mask_address  = physicalRegsBase + HADES_BACKBONE_OFFSET  + c8had1_backbone_regbank_C8HAD1_BACKBONE_REGBANK_BASE_ADDR + 0xC00;
    uint32_t  hades_it_clear;
    uint32_t  hades_it_clear_address = physicalRegsBase + HADES_BACKBONE_OFFSET  + c8had1_backbone_regbank_C8HAD1_BACKBONE_REGBANK_BASE_ADDR + 0xC04;

    // Set interrupt mask
    hades_it_mask = sthorm_read32(hades_it_mask_address);
    hades_it_mask |= c8had1_backbone_regbank_HADES_IT_MASK_FC_mailbox_it_MASK;
    sthorm_write32(hades_it_mask_address, hades_it_mask);

    // Clear interrupt
    hades_it_clear = sthorm_read32(hades_it_clear_address);
    hades_it_clear |= 1;
    sthorm_write32(hades_it_clear_address, hades_it_clear);
    hades_it_clear &= ~1;
    sthorm_write32(hades_it_clear_address, hades_it_clear);

    // pr_info("STHORM: Mailbox IT handler called\n");

    decode_time = OSDEV_GetTimeInMicroSeconds() - decode_time;

    // FC Mailbox fifo #1 is used to send messages from the FC to the Host
    mbAddr = sthorm_remapFabricToHost(P2012_FC_PERIPH_MAILBOX_GET(1));
    lastReceivedMsg_fabric = (p12_msgHeader_t*) mailbox_read_message(mbAddr);

    // Mailbox should be empty now
    emptyMessage = (void*)mailbox_read_message(mbAddr);
    if (emptyMessage != NULL)
	    pr_err("Error: %s STHORM: mailbox malfunction, should be empty, contained token %p instead\n", __func__, emptyMessage);

    // Clear interrupt mask
    hades_it_mask = sthorm_read32(hades_it_mask_address);
    hades_it_mask &= ~ (c8had1_backbone_regbank_HADES_IT_MASK_FC_mailbox_it_MASK);
    sthorm_write32(hades_it_mask_address, hades_it_mask);

    up(&CommandReceived);
    return IRQ_HANDLED;
}

void ReadMailbox(void)
{
	uintptr_t mbAddr;

	// FC Mailbox fifo #1 is used to send messages from the FC to the Host
	mbAddr = sthorm_remapFabricToHost(P2012_FC_PERIPH_MAILBOX_GET(1));
	lastReceivedMsg_fabric = (p12_msgHeader_t*) mailbox_read_message(mbAddr);
	pr_info("JLX: ReadMailbox: %p\n", lastReceivedMsg_fabric);
}

/* Error IT */
OSDEV_InterruptHandlerEntrypoint(err_it_handler)
{
    pr_err("Error: %s STHORM: Received ERROR IT from the fabric\n", __func__);
    return IRQ_HANDLED;
}

static void manage_rab_exception(const char* rab_name, uint32_t rab_offset)
{
    // compute RAB address
    uint32_t rab_base = physicalRegsBase + rab_offset;

    // read status register
    uint32_t err_sts = hal1_read_gp_rab_err_sts_uint32(rab_base);

    if (err_sts != 0)
    {
	    // clear interrupt
	    hal1_write_gp_rab_err_bclr_uint32(rab_base, err_sts);

	    // parse error status
	    if (err_sts & RAB_ERR_STS__MISS_FMSK)
		    pr_err("Error: %s STHORM: RAB %s miss\n", __func__, rab_name);
	    else if (err_sts & RAB_ERR_STS__MULTI_HITS_FMSK)
		    pr_err("Error: %s STHORM: RAB %s multi hits\n", __func__, rab_name);
	    else if (err_sts & RAB_ERR_STS__WRITE_ERROR_FMSK)
		    pr_err("Error: %s STHORM: RAB %s R/O violation\n", __func__, rab_name);
	    else if (err_sts & RAB_ERR_STS__READ_ERROR_FMSK)
		    pr_err("Error: %s STHORM: RAB %s W/O violation\n", __func__, rab_name);

	    // if multi or protection error, get responsible entry
	    if (err_sts & (RAB_ERR_STS__MULTI_HITS_FMSK | RAB_ERR_STS__WRITE_ERROR_FMSK | RAB_ERR_STS__READ_ERROR_FMSK))
	    {
		    uint32_t hit_err = hal1_read_gp_rab_hit_err_sts0_uint32(rab_base);
		    pr_err("Error: %s STHORM: ... RAB %s exception entry mask %#0x\n", __func__, rab_name, hit_err);
	    }
    }
}

/* RAB exception IT */
OSDEV_InterruptHandlerEntrypoint(rab_it_handler)
{
    pr_info("STHORM: Received RAB IT from the fabric\n");

    manage_rab_exception("slave",    HOST_RAB_SLAVE_OFFSET);
    manage_rab_exception("master C", HOST_CIC_RAB_MASTER_OFFSET);
    manage_rab_exception("master D", HOST_DIC_RAB_MASTER_OFFSET);
    manage_rab_exception("backdoor", HOST_RAB_BACKDOOR_1_OFFSET);

    return IRQ_HANDLED;
}

// ============================================================================
// STHORM memory allocator
// See sthormAlloc.h for specs
// ============================================================================
int sthorm_malloc(HostAddress* ha, size_t size)
{
	if (ha == NULL || size == 0)
		return 0;

	ha->size = size;
	ha->physical = OSDEV_MallocPartitioned(alloc_partition, ha->size);
	if (ha->physical == NULL)
	{
		ha->virtual = NULL;
		ha->size = 0;
		return 0;
	}

	ha->virtual = (void*) ioremap_cache((unsigned int)ha->physical, ha->size);
	if (ha->virtual == NULL)
	{
		OSDEV_FreePartitioned(alloc_partition, ha->physical);
		ha->physical = NULL;
		ha->size = 0;
		return 0;
	}

	// pr_info("JLX: STHORM alloc: P %p - V %p - %d\n", ha->physical, ha->virtual, ha->size);
	return 1;
}

#ifndef HEVC_HADES_CUT1

#define MP_MAX_BLOCKS 32

typedef struct {
    uint32_t pool_base;
    uint32_t block_size;
    uint32_t num_blocks;
    uint8_t block_free[MP_MAX_BLOCKS];
} mem_pool_t;

int mem_pool_init(mem_pool_t *mp, uintptr_t memBase, uint32_t blockSize, uint32_t numBlocks);

// return 32 bit because it is assumed to be in STHORM space
uint32_t mem_pool_alloc(mem_pool_t *mp);

void mem_pool_free(mem_pool_t *mp, uint32_t addr);

static inline int32_t mem_pool_index(const mem_pool_t *mp, uint32_t addr)
{
    uint32_t index = (addr - mp->pool_base) / mp->block_size;
    if (index >= MP_MAX_BLOCKS) {
        return -1;
    }
    return index;
}



int mem_pool_init(mem_pool_t *mp, uintptr_t memBase, uint32_t blockSize, uint32_t numBlocks)
{
    int rv = 1;
    uint32_t i;
    if (mp) {
        mp->pool_base = memBase;
        mp->block_size = blockSize;
        if (numBlocks < MP_MAX_BLOCKS) {
            mp->num_blocks = numBlocks;
            rv = 0;
        } else {
            mp->num_blocks = MP_MAX_BLOCKS;
            rv = 2;
        }
        for (i = 0; i < mp->num_blocks; i++) {
            mp->block_free[i] = 1;
        }
    }
    return rv;
}

uint32_t mem_pool_alloc(mem_pool_t *mp)
{
    uint32_t i;
    uint32_t addr = 0;
    for (i = 0; i < mp->num_blocks; i++) {
        if (mp->block_free[i]) {
            mp->block_free[i] = 0;
            addr = mp->pool_base + (i * mp->block_size);
            break;
        }
    }
    return addr;
}

void mem_pool_free(mem_pool_t *mp, uint32_t addr)
{
    int32_t idx;
    if ( addr && (addr >= mp->pool_base)
            && (addr < mp->pool_base + (mp->block_size * mp->num_blocks) )
       ) {
        idx = mem_pool_index(mp, addr);
        if (idx != -1) mp->block_free[idx] = 1;
    }
}

static mem_pool_t memPool;
int sthorm_allocMessage(HostAddress* ha,size_t size)
{
    uint32_t tcdmAddr = 0;
    ha->size = size;

    if (memPool.block_size == 0) {
        runtimeConf_t *rc = (runtimeConf_t*) sthorm_remapFabricToHost(P2012_CONF_FC_TCDM_BASE);
        mem_pool_init(&memPool, (uintptr_t)rc->msgBuffer, FC_STATICMSG_MAX_MSG_SIZE, FC_STATICMSG_MAX_NUM_MSG);
    }
    if (size > FC_STATICMSG_MAX_MSG_SIZE) {
        pr_err("Error: %s requested message buffer is too big %d\n", __func__, size);
        return 0;
    } else {
        tcdmAddr = mem_pool_alloc(&memPool);

    }
    ha->physical=(void*)tcdmAddr;
    ha->virtual = (void*) sthorm_phys_to_virt((unsigned int)ha->physical);

    if (ha->virtual == NULL)
    {
        mem_pool_free(&memPool, (uint32_t)ha->physical);
        ha->physical = NULL;
        ha->size = 0;
        return 0;
    }

    return (1);
}

void sthorm_freeMessage(HostAddress* ha)
{
    mem_pool_free(&memPool, (uint32_t)ha->physical);
}
#endif
void sthorm_free(HostAddress* ha)
{
	// pr_info("JLX: STHORM free: P %p - V %p - %d\n", ha->physical, ha->virtual, ha->size);
	if (ha->virtual != NULL)
		iounmap((void*)ha->virtual);
	ha->virtual = NULL;
	if (ha->physical != NULL)
		OSDEV_FreePartitioned(alloc_partition, ha->physical);
	ha->physical = NULL;
	ha->size = 0;
}

void sthorm_unmap(HostAddress* ha)
{
	if (ha->virtual != NULL)
	{
		iounmap((void*)ha->virtual);
		ha->physical = NULL;
		ha->virtual = NULL;
		ha->size = 0;
	}
}

int sthorm_map(HostAddress* ha, void* buffer, size_t size)
{
	ha->physical = buffer;
	ha->size = size;
	ha->virtual = (void*) ioremap_cache((unsigned int)ha->physical, ha->size);
	if (ha->virtual == NULL)
	{
		ha->physical = NULL;
		ha->size = 0;
		return 0;
	}
	return 1;
}

void sthorm_flush(HostAddress* ha)
{
	phys_addr_t phys = (phys_addr_t)(ha->physical);

	dmac_map_area((const void *)ha->virtual, (size_t)ha->size, DMA_TO_DEVICE);
	outer_clean_range(phys, phys + ha->size);
	mb();
}

void sthorm_invalidate(HostAddress* ha)
{
	phys_addr_t phys = (phys_addr_t)(ha->physical);

	// dmac_flush_range does a writeback and invalidate
	dmac_unmap_area((const void *)ha->virtual, (size_t)ha->size, DMA_FROM_DEVICE);
	outer_inv_range((u32)phys, (u32)phys + ha->size);
	mb();
}
