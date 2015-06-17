/*
 * dma-st-fdma.h
 *
 * Copyright (C) 2013 STMicroelectronics
 * Author: Ludovic Barre <Ludovic.barre@st.com>
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef ST_FDMA_H
#define ST_FDMA_H

#include <linux/dmaengine.h>

struct st_plat_fdma_slim_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
	unsigned long slim_pc;
};

struct st_plat_fdma_periph_regs {
	unsigned long sync_reg;
	unsigned long cmd_sta;
	unsigned long cmd_set;
	unsigned long cmd_clr;
	unsigned long cmd_mask;
	unsigned long int_sta;
	unsigned long int_set;
	unsigned long int_clr;
	unsigned long int_mask;
};

struct st_plat_fdma_ram {
	unsigned long offset;
	unsigned long size;
};

struct st_plat_fdma_hw {
	struct st_plat_fdma_slim_regs slim_regs;
	struct st_plat_fdma_ram dmem;
	struct st_plat_fdma_periph_regs periph_regs;
	struct st_plat_fdma_ram imem;
};

struct st_plat_fdma_fw_regs {
	unsigned long rev_id;
	unsigned long mchi_rx_nb_cur;
	unsigned long mchi_rx_nb_all;
	unsigned long cmd_statn;
	unsigned long req_ctln;
	unsigned long ptrn;
	unsigned long ctrln;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
	unsigned long node_size;
};

struct st_plat_fdma_data {
	struct st_plat_fdma_hw *hw;
	struct st_plat_fdma_fw_regs *fw;
	char *fw_name;
	u8 xbar;
};

struct st_plat_fdma_xbar_data {
	u8 first_fdma_id;
	u8 last_fdma_id;
};

/*
 * Channel type enumerations
 */
enum st_dma_type {
	ST_DMA_TYPE_FREE_RUNNING,
	ST_DMA_TYPE_PACED,
	ST_DMA_TYPE_AUDIO,		/* S/W enhanced paced Tx channel */
	ST_DMA_TYPE_TELSS,
	ST_DMA_TYPE_MCHI,		/* For Rx only - for Tx use paced */
};

struct st_dma_config {
	enum st_dma_type type;
};

struct st_dma_dreq_config {
	u32 request_line;
	u32 direct_conn;
	u32 initiator;
	u32 increment;
	u32 data_swap;
	u32 hold_off;
	u32 maxburst;
	enum dma_slave_buswidth buswidth;
	enum dma_transfer_direction direction;
};

struct st_dma_paced_config {
	enum st_dma_type type;
	dma_addr_t dma_addr;
	struct st_dma_dreq_config dreq_config;
};

struct st_dma_park_config {
	int sub_periods;
	int buffer_size;
};

struct st_dma_audio_config {
	enum st_dma_type type;
	dma_addr_t dma_addr;
	struct st_dma_dreq_config dreq_config;
	struct st_dma_park_config park_config;
};

struct st_dma_telss_config {
	enum st_dma_type type;
	dma_addr_t dma_addr;
	struct st_dma_dreq_config dreq_config;

	u32 frame_count;		/* No frames to transfer */
	u32 frame_size;			/* No words per frame, 0=1, 1=2 etc */
	u32 handset_count;		/* Number of handsets */
};

struct st_dma_telss_handset_config {
	u16 buffer_offset;
	u16 period_offset;
	u32 period_stride;
	u16 first_slot_id;
	u16 second_slot_id;
	bool second_slot_id_valid;
	bool duplicate_enable;
	bool data_length;
	bool call_valid;
};

struct st_dma_mchi_config {
	enum st_dma_type type;
	dma_addr_t dma_addr;
	struct st_dma_dreq_config dreq_config;
	struct st_dma_dreq_config pkt_start_rx_dreq_config;
	u32 rx_fifo_threshold_addr;
};

/*
 * legacy Helper functions
 * Use to identify the instance of dma when you call request chanel.
 * The request chanel way must be redesign to use common xlate & filter,
 * then to use a custom st dt binding on dma-cells, dmas
 * (see example: Documentation/devicetree/bindings/dma/...)
 */
static inline int st_dma_is_fdma_name(struct dma_chan *chan, const char *name)
{
	return !strcmp(dev_name(chan->device->dev), name);
}

static inline int st_dma_is_fdma(struct dma_chan *chan, int id)
{
	return (chan->dev->dev_id == id);
}

static inline int st_dma_is_fdma_channel(struct dma_chan *chan, int id)
{
	return (chan->chan_id == id);
}

/*
 * Audio channel extensions API
 */
int dma_audio_parking_config(struct dma_chan *chan,
		struct st_dma_park_config *config);
int dma_audio_parking_enable(struct dma_chan *chan);
int dma_audio_is_parking_active(struct dma_chan *chan);
struct dma_async_tx_descriptor *dma_audio_prep_tx_cyclic(struct dma_chan *chan,
		dma_addr_t buf_addr, size_t buf_len, size_t period_len);

/*
 * TELSS channel extensions API
 */
int dma_telss_handset_config(struct dma_chan *chan, int handset,
		struct st_dma_telss_handset_config *config);
int dma_telss_handset_control(struct dma_chan *chan, int handset, int valid);
struct dma_async_tx_descriptor *dma_telss_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t buf_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction);
int dma_telss_get_period(struct dma_chan *chan);

/*
 * MCHI channel extensions API
 */
struct dma_async_tx_descriptor *dma_mchi_prep_rx_cyclic(struct dma_chan *chan,
		struct scatterlist *sgl, unsigned int sg_len);

#endif
