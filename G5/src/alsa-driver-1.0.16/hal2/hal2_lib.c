/*
 *  Routines for HAL2 soundcards
 *  Copyright (c) by Ulf Carlsson <ulfc@thepuffingroup.com>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define SNDRV_MAIN_OBJECT_FILE
#include <sound/driver.h>
#include <sound/hal2.h>
#include <sound/control.h>

#include <asm/sgi/sgihpc.h>
#include <asm/addrspace.h>

MODULE_AUTHOR("Ulf Carlsson <ulfc@thepuffingroup.com>");
MODULE_DESCRIPTION("Routines for HAL2 soundcards");
MODULE_LICENSE("GPL");

#undef SNDRV_HAL2_DEBUG

#define H2_INDIRECT_WAIT(regs)	while(regs->isr & H2_ISR_TSTATUS);

#define H2_READ_ADDR(addr)	(addr | (1<<7))
#define H2_WRITE_ADDR(addr)	(addr)

inline void snd_hal2_isr_write(snd_hal2_card_t *hal2, unsigned short val)
{
	hal2->hal2.ctl_regs->isr = val;
}

inline unsigned short snd_hal2_isr_look(snd_hal2_card_t *hal2)
{
	return hal2->hal2.ctl_regs->isr;
}

inline unsigned short snd_hal2_rev_look(snd_hal2_card_t *hal2)
{
	return hal2->hal2.ctl_regs->rev;
}

unsigned short snd_hal2_i_look16(snd_hal2_card_t *hal2, unsigned short addr)
{
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->iar = H2_READ_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
	return (regs->idr0 & 0xffff);
}

unsigned long snd_hal2_i_look32(snd_hal2_card_t *hal2, unsigned short addr)
{
	unsigned long ret;
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->iar = H2_READ_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
	ret = regs->idr0 & 0xffff;
	regs->iar = H2_READ_ADDR(addr | 0x1);
	H2_INDIRECT_WAIT(regs);
	ret |= (regs->idr0 & 0xffff) << 16;
	return ret;
}

void snd_hal2_i_write16(snd_hal2_card_t *hal2, unsigned short addr,
			unsigned short val)
{
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->idr0 = val;
	regs->idr1 = 0;
	regs->idr2 = 0;
	regs->idr3 = 0;
	regs->iar = H2_WRITE_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
}

void snd_hal2_i_write32(snd_hal2_card_t *hal2, unsigned short addr,
			unsigned long val)
{
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->idr0 = val & 0xffff;
	regs->idr1 = val >> 16;
	regs->idr2 = 0;
	regs->idr3 = 0;
	regs->iar = H2_WRITE_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
}

static void snd_hal2_i_setbit16(snd_hal2_card_t *hal2, unsigned short addr,
				unsigned short bit)
{
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->iar = H2_READ_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
	regs->idr0 = regs->idr0 | bit;
	regs->idr1 = 0;
	regs->idr2 = 0;
	regs->idr3 = 0;
	regs->iar = H2_WRITE_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
}

#if 0
static void snd_hal2_i_setbit32(snd_hal2_card_t *hal2, unsigned short addr,
				unsigned long bit)
{
	unsigned long tmp;
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->iar = H2_READ_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
	tmp = regs->idr0 | (regs->idr1 << 16) | bit;
	regs->idr0 = tmp & 0xffff;
	regs->idr1 = tmp >> 16;
	regs->idr2 = 0;
	regs->idr3 = 0;
	regs->iar = H2_WRITE_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
}
#endif

static void snd_hal2_i_clearbit16(snd_hal2_card_t *hal2, unsigned short addr,
				  unsigned short bit)
{
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->iar = H2_READ_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
	regs->idr0 = regs->idr0 & ~bit;
	regs->idr1 = 0;
	regs->idr2 = 0;
	regs->idr3 = 0;
	regs->iar = H2_WRITE_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
}

#if 0
static void snd_hal2_i_clearbit32(snd_hal2_card_t *hal2, unsigned short addr,
				  unsigned long bit)
{
	unsigned long tmp;
	snd_hal2_ctl_regs_t *regs = hal2->hal2.ctl_regs;

	regs->iar = H2_READ_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
	tmp = (regs->idr0 | (regs->idr1 << 16)) & ~bit;
	regs->idr0 = tmp & 0xffff;
	regs->idr1 = tmp >> 16;
	regs->idr2 = 0;
	regs->idr3 = 0;
	regs->iar = H2_WRITE_ADDR(addr);
	H2_INDIRECT_WAIT(regs);
}
#endif

void snd_hal2_dump_regs(snd_hal2_card_t *hal2)
{
	printk("isr: %08hx ", snd_hal2_isr_look(hal2));
	printk("rev: %08hx\n", snd_hal2_rev_look(hal2));
	printk("relay: %04hx\n", snd_hal2_i_look16(hal2, H2I_RELAY_C));
	printk("port en: %04hx ", snd_hal2_i_look16(hal2, H2I_DMA_PORT_EN));
	printk("dma end: %04hx ", snd_hal2_i_look16(hal2, H2I_DMA_END));
	printk("dma drv: %04hx\n", snd_hal2_i_look16(hal2, H2I_DMA_DRV));
	printk("syn ctl: %04hx ", snd_hal2_i_look16(hal2, H2I_SYNTH_C));
	printk("aesrx ctl: %04hx ", snd_hal2_i_look16(hal2, H2I_AESRX_C));
	printk("aestx ctl: %04hx ", snd_hal2_i_look16(hal2, H2I_AESTX_C));
	printk("dac ctl1: %04hx ", snd_hal2_i_look16(hal2, H2I_DAC_C1));
	printk("dac ctl2: %08lx ", snd_hal2_i_look32(hal2, H2I_DAC_C2));
	printk("adc ctl1: %04hx ", snd_hal2_i_look16(hal2, H2I_ADC_C1));
	printk("adc ctl2: %08lx ", snd_hal2_i_look32(hal2, H2I_ADC_C2));
	printk("syn map: %04hx\n", snd_hal2_i_look16(hal2, H2I_SYNTH_MAP_C));
	printk("bres1 ctl1: %04hx ", snd_hal2_i_look16(hal2, H2I_BRES1_C1));
	printk("bres1 ctl2: %04lx ", snd_hal2_i_look32(hal2, H2I_BRES1_C2));
	printk("bres2 ctl1: %04hx ", snd_hal2_i_look16(hal2, H2I_BRES2_C1));
	printk("bres2 ctl2: %04lx ", snd_hal2_i_look32(hal2, H2I_BRES2_C2));
	printk("bres3 ctl1: %04hx ", snd_hal2_i_look16(hal2, H2I_BRES3_C1));
	printk("bres3 ctl2: %04lx\n", snd_hal2_i_look32(hal2, H2I_BRES3_C2));
}

static int snd_hal2_compute_rate(snd_hal2_card_t *hal2, unsigned int rate)
{
	/* We default to 44.1 kHz and if it isn't possible to fall back to
	 * 48.0 kHz with the needed adjustments of real_rate.
	 */

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("rate: %d\n", rate);
#endif
	if (rate < 1)
		rate = 1;	/* this is fixed later.. */

	/* Note: This is NOT the way they set up the bresenham clock generators
	 * in the specification. I've tried to implement that method but it
	 * doesn't work. It's probably another silly bug in the spec.
	 *
	 * I accidently discovered this method while I was testing and it seems
	 * to work very well with all frequencies, and thee shall follow rule #1
	 * of programming :-)
	 */
	
	if (44100 % rate == 0) {
		unsigned short inc;

		inc = 44100 / rate;

		hal2->hal2.master = 44100;
		hal2->hal2.inc = inc;
		hal2->hal2.mod = 1;
	} else {
		unsigned short inc;
		inc = 48000 / rate;
		if (inc < 1) inc = 1;

		rate = 48000 / inc;

		hal2->hal2.master = 48000;
		hal2->hal2.inc = inc;
		hal2->hal2.mod = 1;
	}

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("real_rate: %d\n", rate);
#endif
	return rate;
}

static void snd_hal2_set_rate(snd_hal2_card_t *hal2)
{
	unsigned int master = hal2->hal2.master;
	int inc = hal2->hal2.inc;
	int mod = hal2->hal2.mod;

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("master: %d inc: %d mod: %d\n", master, inc, mod);
#endif
	snd_hal2_i_write16(hal2, H2I_BRES1_C1, (master == 44100) ? 1 : 0);
	snd_hal2_i_write32(hal2, H2I_BRES1_C2, ((0xffff & (mod - inc - 1)) << 16) | 1);
}

void snd_hal2_interrupt(snd_hal2_card_t *hal2)
{
	snd_pcm1_substream_t *substream1;
	
	if (hal2 && hal2->pcm) {
		if (hal2->hal2.dac.pbus.pbus &&
		    hal2->hal2.dac.pbus.pbus->pbdma_ctrl & 0x01) {
			substream1 = (snd_pcm1_substream_t *) hal2->playback_substream1;
			if (substream1 && substream1->ack)
				substream1->ack(hal2->playback_substream);
		}
		if (hal2->hal2.adc.pbus.pbus &&
		    hal2->hal2.adc.pbus.pbus->pbdma_ctrl & 0x01) {
			substream1 = (snd_pcm1_substream_t *) hal2->capture_substream1;
			if (substream1 && substream1->ack)
				substream1->ack(hal2->capture_substream);
		}
	}
}

static int snd_hal2_playback_ioctl(void *private_data, struct snd_pcm_substream *substream,
				   unsigned int cmd, void *arg)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;
	snd_pcm1_substream_t *substream1 = (snd_pcm1_substream_t *) substream->private_data;

	switch (cmd) {
	case SNDRV_PCM1_IOCTL_RATE:
		substream1->real_rate = snd_hal2_compute_rate(hal2, substream1->rate);
		return 0;
	}
	return -ENXIO;
}

static int snd_hal2_capture_ioctl(void *private_data, struct snd_pcm_substream *substream,
				  unsigned int cmd, void *arg)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;
	snd_pcm1_substream_t *substream1 = (snd_pcm1_substream_t *) substream->private_data;

	switch (cmd) {
	case SNDRV_PCM1_IOCTL_RATE:
		substream1->real_rate = snd_hal2_compute_rate(hal2, substream1->rate);
		return 0;
	}
	return -ENXIO;
}

static int snd_hal2_playback_open(void *private_data, struct snd_pcm_substream *substream)
{
	int err;
	snd_hal2_card_t *hal2;
	snd_hal2_pbus_t *pbus;

	hal2 = (snd_hal2_card_t *) private_data;

	hal2->playback_substream = substream;
	hal2->playback_substream1 = (snd_pcm1_substream_t *) substream->private_data;

	/* Maybe we shouldn't allocate the dma here, but when the driver is
	 * loaded so that we can *beep*.
	 */
	if ((err = snd_pcm1_dma_alloc(substream, hal2->hal2.dac.dmaptr,
				      "HAL2 PCM - playback")) < 0)
		return err;

	pbus = &hal2->hal2.dac.pbus;
	pbus->pbusnr = 1;
	pbus->pbus = &hpc3c0->pbdma[pbus->pbusnr];
	
#ifdef SNDRV_HAL2_DEBUG
	snd_printk("playback open ok!\n");
#endif
	hal2->usecount++;

	return 0;
}

static int snd_hal2_capture_open(void *private_data, struct snd_pcm_substream *substream)
{
	int err;
	snd_hal2_card_t *hal2;
	snd_hal2_pbus_t *pbus;

	hal2 = (snd_hal2_card_t *) private_data;

	hal2->capture_substream = substream;
	hal2->capture_substream1 = (snd_pcm1_substream_t *) substream->private_data;

	if ((err = snd_pcm1_dma_alloc(substream, hal2->hal2.adc.dmaptr,
				      "HAL2 PCM - playback")) < 0)
		return err;

	pbus = &hal2->hal2.adc.pbus;
	pbus->pbusnr = 2;
	pbus->pbus = &hpc3c0->pbdma[pbus->pbusnr];

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("capture open ok!\n");
#endif
	hal2->usecount++;

	return 0;
}

static void snd_hal2_playback_close(void *private_data, struct snd_pcm_substream *substream)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;

	if (hal2->hal2.dac.ringbuf) {
		snd_free_pages((char *) hal2->hal2.dac.ringbuf,
			       hal2->hal2.dac.blocks * sizeof(snd_hal2_ring_t));
		hal2->hal2.dac.ringbuf = NULL;
	}

	snd_pcm1_dma_free(substream);

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("playback close ok!\n");
#endif
	hal2->usecount--;
}

static void snd_hal2_capture_close(void *private_data, struct snd_pcm_substream *substream)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;

	if (hal2->hal2.adc.ringbuf) {
		snd_free_pages((char *) hal2->hal2.adc.ringbuf,
			       hal2->hal2.adc.blocks * sizeof(snd_hal2_ring_t));
		hal2->hal2.adc.ringbuf = NULL;
	}

	snd_pcm1_dma_free(substream);

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("capture close ok!\n");
#endif
	hal2->usecount--;
}

#if 0
static void snd_hal2_pio_memset(void *s, int c, size_t n)
{
	snd_hal2_reg_t *reg;

	reg = (snd_hal2_reg_t *) (s + n);

	while ((void *) reg-- > s)
		*reg = c;
}
#endif

static void snd_hal2_playback_prepare(void *private_data,
				      struct snd_pcm_substream *substream,
				      unsigned char *buffer, unsigned int size,
				      unsigned int offset, unsigned int count)
{
	int blocks, block_size;
	int sample_size;
	int i;
	int pg;
	snd_hal2_ring_t *ring;
	snd_hal2_pbus_t *pbus;

	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;
	snd_pcm1_substream_t *substream1 = (snd_pcm1_substream_t *) substream->private_data;

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("playback prepare..\n");
#endif

	/* Recalculate unsupported values of block and block_size..
	 */
	block_size = substream1->block_size;

	if (block_size > PAGE_SIZE)
		block_size = substream1->block_size = PAGE_SIZE;

	blocks = substream1->blocks = substream1->used_size / block_size;

	if (blocks > PAGE_SIZE / sizeof(snd_hal2_ring_t)) {
		snd_printk("too many blocks: %d\n", blocks);
		return;
	}

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("DAC: blocks: %d block_size: %d size: %u\n",
		   blocks, block_size, size);
#endif

	if (hal2->hal2.dac.ringbuf) {
		snd_free_pages((char *) hal2->hal2.dac.ringbuf,
			       hal2->hal2.dac.blocks * sizeof(snd_hal2_ring_t));
	}

	hal2->hal2.dac.ringbuf = (snd_hal2_ring_t *)
		snd_malloc_pages(blocks * sizeof(snd_hal2_ring_t), &pg, 0);
	if (hal == NULL2->hal2.dac.ringbuf) {
		snd_printd("Oops, no memory for DMA descriptors");
		return;
	}
	hal2->hal2.dac.blocks = blocks;


	/* Setup the ring of DMA descriptors to point to their parts of the DMA
	 * buffer and to generate interrrupts.
	 */
	i = 0;
	ring = hal2->hal2.dac.ringbuf;
	ring->desc.pbuf = PHYSADDR(buffer);
	ring->desc.pnext = PHYSADDR(ring + 1);
	ring->desc.cntinfo = block_size | HPCDMA_XIE;
	while (++i < blocks) {
		ring++;
		ring->desc.pbuf = PHYSADDR(buffer + i * block_size);
		ring->desc.pnext = PHYSADDR(ring + 1);
		ring->desc.cntinfo = block_size | HPCDMA_XIE;
	}
	ring->desc.pnext = PHYSADDR(hal2->hal2.dac.ringbuf);
	
	/* The PBUS can prolly not read thes stuff when it's in the cache so we
	 * have to flush it back to main memory
	 */
	dma_cache_wback_inv((unsigned long) hal2->hal2.dac.ringbuf,
			    blocks * sizeof(snd_hal2_ring_t));


	/* Now we set up some PBUS information. The PBUS needs information about
	 * what portion of the fifo it will use. If it's receiving or
	 * transmitting, and finally whether the stream is little endian or big
	 * endian. The information is written later, on the trigger call.
	 */
	
	sample_size = substream1->voices * 2;

	pbus = &hal2->hal2.dac.pbus;
	pbus->highwater = (sample_size * 2) >> 1;	/* halfwords */
	pbus->fifobeg = 0;				/* playback is first */
	pbus->fifoend = (sample_size * 4) >> 3;		/* doublewords */
	pbus->ctrl = HPC3_PDMACTRL_RT |
		((substream1->mode & SNDRV_PCM1_MODE_BIG) ? 0 : HPC3_PDMACTRL_SEL);

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("DAC: mode: %x voices: %d\n", substream1->mode, substream1->voices);
#endif

	/* We disable everything before we do anything at all
	 */
	pbus->pbus->pbdma_ctrl = HPC3_PDMACTRL_LD;
	snd_hal2_i_clearbit16(hal2, H2I_DMA_PORT_EN, H2I_DMA_PORT_EN_CODECTX);
	snd_hal2_i_clearbit16(hal2, H2I_DMA_DRV, (1 << pbus->pbusnr));

#if 0
	snd_hal2_pio_memset(hal2->hal2.aes_regs, 0, sizeof(snd_hal2_aes_regs_t));
	snd_hal2_pio_memset(hal2->hal2.syn_regs, 0, sizeof(snd_hal2_syn_regs_t));
#endif

	/* Setup the HAL2 for playback
	 */
	substream1->real_rate = snd_hal2_compute_rate(hal2, substream1->rate);
	snd_hal2_set_rate(hal2);
	snd_hal2_i_write16(hal2, H2I_DAC_C1,
			   (pbus->pbusnr << H2I_DAC_C1_DMA_SHIFT) |
			   (1 << H2I_DAC_C1_CLKID_SHIFT) |
			   (substream1->voices << H2I_DAC_C1_DATAT_SHIFT));
	snd_hal2_i_write32(hal2, H2I_DAC_C2, 0); /* disable muting */

	/* The spec says that we should write 0x08248844 but that's WRONG. HAL2
	 * does 8 bit DMA, not 16 bit even if it generates 16 bit audio.
	 */
	hpc3c0->pbus_dmacfgs[pbus->pbusnr][0] = 0x08208844;	/* Magic :-) */
}

static void snd_hal2_capture_prepare(void *private_data,
				     struct snd_pcm_substream *substream,
				     unsigned char *buffer, unsigned int size,
				     unsigned int offset, unsigned int count)
{
	int blocks, block_size;
	int sample_size;
	int i;
	int pg;
	snd_hal2_ring_t *ring;
	snd_hal2_pbus_t *pbus;

	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;
	snd_pcm1_substream_t *substream1 = (snd_pcm1_substream_t *) substream->private_data;

	if (hal == NULL2) {
		snd_printk("prepare: no private data?\n");
		return;
	}

	block_size = substream1->block_size;

	if (block_size > PAGE_SIZE)
		block_size = substream1->block_size = PAGE_SIZE;

	blocks = substream1->blocks = substream1->used_size / block_size;

	if (blocks > PAGE_SIZE / sizeof(snd_hal2_ring_t)) {
		snd_printk("too many blocks: %d\n", blocks);
		return;
	}

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("ADC: blocks: %d block_size: %d size: %u\n",
		   blocks, block_size, size);
#endif

	if (hal2->hal2.adc.ringbuf) {
		snd_free_pages((char *) hal2->hal2.adc.ringbuf,
			       hal2->hal2.adc.blocks * sizeof(snd_hal2_ring_t));
	}

	hal2->hal2.adc.ringbuf = (snd_hal2_ring_t *)
		snd_malloc_pages(blocks * sizeof(snd_hal2_ring_t), &pg, 0);
	if (hal == NULL2->hal2.adc.ringbuf) {
		snd_printd("Oops, no memory for DMA descriptors");
		return;
	}
	hal2->hal2.adc.blocks = blocks;

	/* Setup the ring of DMA descriptors to point to their parts of the DMA
	 * buffer and to generate interrrupts.
	 */
	i = 0;
	ring = hal2->hal2.adc.ringbuf;
	ring->desc.pbuf = PHYSADDR(buffer);
	ring->desc.pnext = PHYSADDR(ring + 1);
	ring->desc.cntinfo = block_size | HPCDMA_XIE;
	while (++i < blocks) {
		ring++;
		ring->desc.pbuf = PHYSADDR(buffer + i * block_size);
		ring->desc.pnext = PHYSADDR(ring + 1);
		ring->desc.cntinfo = block_size | HPCDMA_XIE;
	}
	ring->desc.pnext = PHYSADDR(hal2->hal2.adc.ringbuf);
	
	/* The PBUS can prolly not read thes stuff when it's in the cache so we
	 * have to flush it back to main memory
	 */
	dma_cache_wback_inv((unsigned long) hal2->hal2.adc.ringbuf,
			    blocks * sizeof(snd_hal2_ring_t));

	/* Now we set up some PBUS information. The PBUS needs information about
	 * what portion of the fifo it will use. If it's receiving or
	 * transmitting, and finally whether the stream is little endian or big
	 * endian. The information is written later, on the trigger call.
	 */
	
	sample_size = substream1->voices * 2;

	pbus = &hal2->hal2.adc.pbus;
	pbus->highwater = (sample_size * 2) >> 1;	/* halfwords */
	pbus->fifobeg = (4 * 4) >> 3;			/* capture is second */
	pbus->fifoend = (sample_size * 4) >> 3;		/* doublewords */
	pbus->ctrl = HPC3_PDMACTRL_RT | HPC3_PDMACTRL_RCV |
		((substream1->mode & SNDRV_PCM1_MODE_BIG) ? 0 : HPC3_PDMACTRL_SEL);

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("ADC: mode: %x voices: %d\n", substream1->mode, substream1->voices);
#endif

	/* We disable everything before we do anything at all
	 */
	pbus->pbus->pbdma_ctrl = HPC3_PDMACTRL_LD;
	snd_hal2_i_clearbit16(hal2, H2I_DMA_PORT_EN, H2I_DMA_PORT_EN_CODECR);
	snd_hal2_i_clearbit16(hal2, H2I_DMA_DRV, (1 << pbus->pbusnr));

	/* Setup the HAL2 for capture
	 */
	substream1->real_rate = snd_hal2_compute_rate(hal2, substream1->rate);
	snd_hal2_set_rate(hal2);
	snd_hal2_i_write16(hal2, H2I_ADC_C1,
			   (pbus->pbusnr << H2I_ADC_C1_DMA_SHIFT) |
			   (1 << H2I_ADC_C1_CLKID_SHIFT));
	snd_hal2_i_write16(hal2, H2I_DAC_C1,
			   (1 << H2I_DAC_C1_CLKID_SHIFT) |
			   (substream1->voices << H2I_DAC_C1_DATAT_SHIFT));
	snd_hal2_i_write32(hal2, H2I_ADC_C2, 0);	/* disable muting */

	/* The spec says that we should write 0x08248844 but that's WRONG. HAL2
	 * does 8 bit DMA, not 16 bit even if it generates 16 bit audio.
	 */
	hpc3c0->pbus_dmacfgs[pbus->pbusnr][0] = 0x08208844;	/* Magic :-) */
}

static void snd_hal2_playback_trigger(void *private_data,
				      struct snd_pcm_substream *substream, int up)
{
	snd_hal2_card_t *hal2;
	snd_hal2_pbus_t *pbus;

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("Trigger %s\n", up ? "up" : "down");
#endif

	hal2 = (snd_hal2_card_t *) private_data;
	pbus = &hal2->hal2.dac.pbus;

	if (up) {
		unsigned long fifobeg, fifoend, highwater;

		if (hal == NULL2->hal2.dac.ringbuf) {
			snd_printd("Oops, there was no ringbuf to trigger!\n");
			return;
		}

		fifobeg = pbus->fifobeg;
		fifoend = pbus->fifoend;
		highwater = pbus->highwater;

		pbus->pbus->pbdma_dptr = PHYSADDR(hal2->hal2.dac.ringbuf);

		/* We can not activate the PBUS at the same time as we configure
		 * it. We have to write the configuration bits first and then
		 * write both the configuration bits and the activation bits.
		 * If we do not do so, we might get noise in the DMA stream.
		 */
		pbus->ctrl |= ((highwater << 8) | (fifobeg << 16) | (fifoend << 24) | HPC3_PDMACTRL_LD);
		pbus->pbus->pbdma_ctrl = pbus->ctrl;
		pbus->ctrl |= (HPC3_PDMACTRL_ACT);
		pbus->pbus->pbdma_ctrl = pbus->ctrl;

#if 0
		/* Shouldn't this make difference? It works fine without it..
		 */
		if (substream1->mode & SNDRV_PCM1_MODE_BIG)
			snd_hal2_i_clearbit16(hal2, H2I_DMA_END,
					      H2I_DMA_END_CODECTX);
		else
			snd_hal2_i_setbit16(hal2, H2I_DMA_END,
					    H2I_DMA_END_CODECTX);
#endif

		snd_hal2_i_setbit16(hal2, H2I_DMA_DRV, (1 << pbus->pbusnr));
		snd_hal2_i_setbit16(hal2, H2I_DMA_PORT_EN, H2I_DMA_PORT_EN_CODECTX);
#ifdef SNDRV_HAL2_DEBUG
		snd_hal2_dump_regs(hal2);
#endif
	} else {
		pbus->pbus->pbdma_ctrl = HPC3_PDMACTRL_LD;
		/* The HAL2 itself may remain enabled safely */
	}
}

static void snd_hal2_capture_trigger(void *private_data,
				     struct snd_pcm_substream *substream, int up)
{
	snd_hal2_card_t *hal2;
	snd_hal2_pbus_t *pbus;

#ifdef SNDRV_HAL2_DEBUG
	snd_printk("Trigger %s\n", up ? "up" : "down");
#endif

	hal2 = (snd_hal2_card_t *) private_data;
	pbus = &hal2->hal2.adc.pbus;

	if (up) {
		unsigned long fifobeg, fifoend, highwater;

		if (hal == NULL2->hal2.adc.ringbuf) {
			snd_printd("Oops, there was no ringbuf to trigger!\n");
			return;
		}

		fifobeg = pbus->fifobeg;
		fifoend = pbus->fifoend;
		highwater = pbus->highwater;

		pbus->pbus->pbdma_dptr = PHYSADDR(hal2->hal2.adc.ringbuf);

		pbus->ctrl |= ((highwater << 8) | (fifobeg << 16) | (fifoend << 24) | HPC3_PDMACTRL_LD);
		pbus->pbus->pbdma_ctrl = pbus->ctrl;
		pbus->ctrl |= (HPC3_PDMACTRL_ACT);
		pbus->pbus->pbdma_ctrl = pbus->ctrl;

#if 0
		if (substream1->mode & SNDRV_PCM1_MODE_BIG)
			snd_hal2_i_clearbit16(hal2, H2I_DMA_END,
					      H2I_DMA_END_CODECR);
		else
			snd_hal2_i_setbit16(hal2, H2I_DMA_END,
					    H2I_DMA_END_CODECR);
#endif
		snd_hal2_i_setbit16(hal2, H2I_DMA_DRV, (1 << pbus->pbusnr));
		snd_hal2_i_setbit16(hal2, H2I_DMA_PORT_EN, H2I_DMA_PORT_EN_CODECR);
	} else {
		pbus->pbus->pbdma_ctrl = HPC3_PDMACTRL_LD;
		/* The HAL2 itself may safely remain enabled */
	}
}

static snd_pcm_uframes_t snd_hal2_playback_pointer(void *private_data,
					      struct snd_pcm_substream *substream,
					      unsigned int used_size)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;

	return hal2->hal2.dac.pbus.pbus->pbdma_bptr;
}

static snd_pcm_uframes_t snd_hal2_capture_pointer(void *private_data,
					     struct snd_pcm_substream *substream,
					     unsigned int used_size)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) private_data;

	return hal2->hal2.adc.pbus.pbus->pbdma_bptr;
}

static int snd_hal2_playback_dma(void *private_data, struct snd_pcm_substream *substream,
				 unsigned char *buffer, unsigned int offset,
				 unsigned char *user, unsigned int count)
{
	int ret;
	ret = snd_pcm1_playback_dma(private_data, substream, buffer, offset, user,
				    count);
	dma_cache_wback_inv((unsigned long) &buffer[offset], count);
	return ret;
}

static int snd_hal2_capture_dma(void *private_data, struct snd_pcm_substream *substream,
				unsigned char *buffer, unsigned int offset,
				unsigned char *user, unsigned int count)
{
	int ret;
	dma_cache_inv((unsigned long) &buffer[offset], count);
	ret = snd_pcm1_capture_dma(private_data, substream, buffer, offset, user,
				   count);
	return ret;
}

static int snd_hal2_dma_move(void *private_data,
			    struct snd_pcm_substream *substream,
			     unsigned char *dbuffer,
			     unsigned int dest_offset,
			     unsigned char *sbuffer,
			     unsigned int src_offset,
			     unsigned int count)
{
	int ret; 
	ret = snd_pcm1_dma_move(private_data, substream, dbuffer, dest_offset,
				sbuffer, src_offset, count);
	dma_cache_wback_inv((unsigned long) &dbuffer[dest_offset], count);
	return ret;
}

static int snd_hal2_playback_dma_neutral(void *private_data,
					 struct snd_pcm_substream *substream,
					 unsigned char *buffer,
					 unsigned int offset,
					 unsigned int count,
					 unsigned char neutral_byte)
{
	int ret;
	ret = snd_pcm1_playback_dma_neutral(private_data, substream, buffer,
					    offset, count, neutral_byte);
	dma_cache_wback_inv((unsigned long) &buffer[offset], count);
	return ret;
}

struct _snd_pcm1_hardware snd_hal2_playback =
{
	SNDRV_PCM1_HW_16BITONLY | SNDRV_PCM1_HW_AUTODMA,	/* flags */
	SNDRV_PCM_FMTBIT_MU_LAW | SNDRV_PCM_FMTBIT_S16_LE |
		SNDRV_PCM_FMTBIT_S16_BE,			/* formats */
	SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE,	/* hardware formats */
	0,
	2,	/* XXX */	/* minimal fragment */
	4000,			/* min. rate */
	48000,			/* max. rate */
	2,			/* max. voices */
	snd_hal2_playback_open,
	snd_hal2_playback_close,
	snd_hal2_playback_ioctl,
	snd_hal2_playback_prepare,
	snd_hal2_playback_trigger,
	snd_hal2_playback_pointer,
	snd_hal2_playback_dma,
	snd_hal2_dma_move,
	snd_hal2_playback_dma_neutral
};

struct _snd_pcm1_hardware snd_hal2_capture =
{
	SNDRV_PCM1_HW_16BITONLY | SNDRV_PCM1_HW_AUTODMA,	/* flags */
	SNDRV_PCM_FMTBIT_MU_LAW | SNDRV_PCM_FMTBIT_S16_LE |
		SNDRV_PCM_FMTBIT_S16_BE,			/* formats */
	SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE,	/* hardware formats */
	0,
	2,	/* XXX */	/* minimal fragment */
	4000,			/* min. rate */
	48000,			/* max. rate */
	2,			/* max. voices */
	snd_hal2_capture_open,
	snd_hal2_capture_close,
	snd_hal2_capture_ioctl,
	snd_hal2_capture_prepare,
	snd_hal2_capture_trigger,
	snd_hal2_capture_pointer,
	snd_hal2_capture_dma,
	snd_hal2_dma_move,
	NULL
};

static void snd_hal2_free_device(void *ptr)
{
	snd_hal2_card_t *hal2 = (snd_hal2_card_t *) ptr;
	snd_kfree(hal2);
}

struct snd_pcm *snd_hal2_new_device(struct snd_card *card,
			       snd_irq_t *irqptr,
			       snd_dma_t *dma1ptr,
			       snd_dma_t *dma2ptr,
			       snd_hal2_ctl_regs_t *ctl_regs,
			       snd_hal2_aes_regs_t *aes_regs,
			       snd_hal2_vol_regs_t *vol_regs,
			       snd_hal2_syn_regs_t *syn_regs)
{
	struct snd_pcm *pcm;
	snd_pcm1_channel_t *pchn1;
	snd_hal2_card_t *hal2;

	pcm = snd_pcm1_new_device(card, "HAL2", 1, 1);
	if (pcm == NULL)
		return NULL;
	hal2 = (snd_hal2_card_t *) snd_kcalloc(sizeof(snd_hal2_card_t), GFP_KERNEL);
	if (hal2 == NULL)
		return NULL;

	hal2->pcm = pcm;
	hal2->card = card;
	hal2->hal2.irqptr = irqptr;
	hal2->hal2.dac.dmaptr = dma1ptr;
	if (dma2ptr)
		hal2->hal2.adc.dmaptr = dma2ptr;
	hal2->hal2.ctl_regs = ctl_regs;
	hal2->hal2.aes_regs = aes_regs;
	hal2->hal2.vol_regs = vol_regs;
	hal2->hal2.syn_regs = syn_regs;
	card->private_data = hal2;
	card->private_free = snd_hal2_free_device;

	pchn1 = (snd_pcm1_channel_t *) pcm->playback.private_data;
	memcpy(&pchn1->hw, &snd_hal2_playback,
	       sizeof(snd_hal2_playback));
	pchn1->private_data = hal2;

	pchn1 = (snd_pcm1_channel_t *) pcm->capture.private_data;
	memcpy(&pchn1->hw, &snd_hal2_capture,
	       sizeof(snd_hal2_capture));
	pchn1->private_data = hal2;

	pcm->info_flags = SNDRV_PCM_INFO_CODEC | /* SNDRV_PCM_INFO_MMAP | */
		SNDRV_PCM_INFO_PLAYBACK | SNDRV_PCM_INFO_CAPTURE |
		SNDRV_PCM_INFO_DUPLEX;
	pcm->private_data = hal2;
	pcm->private_free = snd_hal2_free_device;
	strcpy(pcm->name, "HAL2");

	return pcm;
}

/* XXX proc interface? */

EXPORT_SYMBOL(snd_hal2_create);
EXPORT_SYMBOL(snd_hal2_free);
EXPORT_SYMBOL(snd_hal2_interrupt);
EXPORT_SYMBOL(snd_hal2_pcm);

static int __init alsa_hal2_init(void)
{
	return 0;
}

static void __exit alsa_hal2_exit(void)
{
}

module_init(alsa_hal2_init)
module_exit(alsa_hal2_exit)
