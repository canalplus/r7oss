/*
 * c8jpg-hw.c, C8JPG1 h/w configuration
 *
 * Copyright (C) 2012, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>

#include "c8jpg.h"

#include "c8jpg-hw.h"
#include "c8jpg-macros.h"
#include "c8jpg-resize-tables.h"

struct selecter {
	int stime, modulo;
};

struct counter {
	int inc, modulo, shift, mult;
};

struct outputstage {
	int swap_en;
	int init_phase;
	int offset_c;
	struct selecter ack;
	struct selecter mode;

	struct selecter dummy0;
	struct selecter dummy1;
	struct selecter mux_op0;
	int mux_op1;

	struct selecter byteen;
	struct selecter byteop0;
	struct selecter byteop1;
	int byteop2;

	struct counter wraddr_y[6];
	struct counter wraddr_uv[6];

	struct counter stbus_y[4];
	struct counter stbus_uv[4];

	struct selecter rd_mode;
	int rd_sync_modulo;
	int rd_full_modulo;
	int rd_mem_modulo;
	int wr_sync_modulo;
	int wr_full_modulo;
	struct selecter chunk;
};

#define GET_CNT(cnt) ((cnt).inc * (cnt).modulo)

void c8jpg_setup_filtering_tables(struct c8jpg_resizer_state *state)
{
	/* luma filtering tables */
	if (state->incr_y < 32768)
		state->y_table = lut_a_legacy;
	else if (state->incr_y == 32768)
		state->y_table = lut_b;
	else if (state->incr_y >= 39321)
		state->y_table = lut_c_luma_legacy;
	else if (state->incr_y >= 45875)
		state->y_table = lut_d_luma_legacy;
	else if (state->incr_y >= 65536)
		state->y_table = lut_e_luma_legacy;
	else
		state->y_table = lut_f_luma_legacy;

	/* chroma filtering tables */
	if (state->incr_uv < 32768)
		state->uv_table = lut_a_legacy;
	else if (state->incr_uv == 32768)
		state->uv_table = lut_b;
	else if (state->incr_uv >= 39321)
		state->uv_table = lut_c_chroma_legacy;
	else if (state->incr_uv >= 45875)
		state->uv_table = lut_d_chroma_legacy;
	else if (state->incr_uv >= 65536)
		state->uv_table = lut_e_chroma_legacy;
	else
		state->uv_table = lut_f_chroma_legacy;
}

void c8jpg_setup_outputstage(struct stm_c8jpg_device *dev,
			     struct c8jpg_resizer_state *state)
{
	const int my_log2[] = {0, 0, 1, 1, 2};

	struct outputstage setup;
	unsigned int data;

	int blk_h[4]  = {8 >> state->vdec_y,
			 8 >> state->vdec_uv,
			 8 >> state->vdec_uv,
			 8 >> state->vdec_uv};

	int nb_mcu     = 4 / state->scan_out.hy;
	int nb_blk_y   = state->scan_in.vy * state->scan_out.hy;
	int nb_blk_uv  = state->scan_in.vcb * state->scan_out.hcb;
	int nb_l4x4_y  = blk_h[0] * nb_blk_y;
	int nb_l4x4_uv = blk_h[1] * nb_blk_uv * 2;
	int width_blk  = state->mcu_width * state->scan_out.hy;
	int pitch_blk  = (width_blk + 3) & ~3;
	int eo_cnt;

	memset(&setup, 0, sizeof(setup));

	setup.swap_en     = 1; /* endianness */
	setup.init_phase  = 0;
	setup.offset_c    = nb_mcu * nb_l4x4_y;
	setup.ack.stime   = 1; /* Unused */
	setup.ack.modulo  = 1; /* Unused */
	setup.mode.stime  = 2 * (nb_l4x4_y); /* switch from luma to chroma */
	setup.mode.modulo = 2 * (nb_l4x4_y + nb_l4x4_uv);

	setup.dummy0.stime    = 1; /* Unused */
	setup.dummy0.modulo   = 1; /* Unused */
	setup.dummy1.stime    = 1; /* Default */
	setup.dummy1.modulo   = 1; /* Default */
	if (width_blk & 3) { /* If width_blk is odd */
		/* Number of incomming data before start dummy */
		setup.dummy1.stime  =
				setup.mode.modulo
				* state->mcu_width;
		/* Number of incomming data before end of dummy */
		setup.dummy1.modulo =
				setup.mode.modulo
				* pitch_blk
				/ state->scan_out.hy;
	}
	/* byte swapping for mem writting in addition of byte enable */
	setup.mux_op0.stime   = setup.mode.stime;
	/* 2 case luma:d3d2d1d0d3d2d1d0 with mask 0x0F and 0XF0 */
	setup.mux_op0.modulo  = setup.mode.modulo;
	/* chroma d3d3d2d2d1d1d0d0 wih mask 0xAA and 0x55 */
	setup.mux_op1         = 0;

	/* Luma-chroma toggle */
	setup.byteen.stime   = setup.mode.stime;
	setup.byteen.modulo  = setup.mode.modulo;
	/* YUV mode */
	setup.byteop2        = 0;
	/* Intra luma toggle */
	setup.byteop1.stime  = 8 >> state->vdec_y;
	setup.byteop1.modulo = 16 >> state->vdec_y;
	/* Intra chroma toggle */
	setup.byteop0.stime  = setup.byteen.stime + (16 >> state->vdec_uv);
	/* Starts after luma */
	setup.byteop0.modulo = setup.byteen.stime + (32 >> state->vdec_uv);

	/* Line number in a column 4x8, 4x4, 4x2 and 4x1 for
	 * respectivy decimation v1,v2,v4 and v8
	 */
	/* Increment after each incoming data */
	setup.wraddr_y[0].inc	= 1;
	/* nb line in a block are 8>>vdec */
	setup.wraddr_y[0].modulo = 8 >> state->vdec_y;
	/* increment the address of 4 = (1<<2) after each data */
	setup.wraddr_y[0].shift  = 2;
	eo_cnt = GET_CNT(setup.wraddr_y[0]);

	/* blk_y management */
	/* Increment after each blk */
	setup.wraddr_y[1].inc	= 2 * eo_cnt;
	/* blk_y from 0 to scan_in_h[0] */
	setup.wraddr_y[1].modulo = state->scan_in.vy;
	/* Increment the address of 4*(8>>vdec) */
	setup.wraddr_y[1].shift  = 5 - state->vdec_y;
	eo_cnt = GET_CNT(setup.wraddr_y[1]);

	/* blk_X management */
	/* Increment after each column of blk, */
	setup.wraddr_y[2].inc	= eo_cnt;
	/* blk_x from 0 to scan_out_w[0] */
	setup.wraddr_y[2].modulo = state->scan_out.hy;
	/* Increment the address of 1 */
	setup.wraddr_y[2].shift  = 0;
	eo_cnt = GET_CNT(setup.wraddr_y[2]);

	/* MCU management */
	/* Increment after each mcu */
	setup.wraddr_y[3].inc	= eo_cnt;
	/* Count all mcu from 0 to nb_mcu */
	setup.wraddr_y[3].modulo = nb_mcu;
	/* Increment the address of scan_out_w[0] */
	setup.wraddr_y[3].shift  = my_log2[state->scan_out.hy];
	eo_cnt = GET_CNT(setup.wraddr_y[3]);

	/* Unused */
	setup.wraddr_y[4].inc	= 1;
	setup.wraddr_y[4].modulo = 1;
	setup.wraddr_y[4].shift  = 0;

	/* Ping pong : split the memory in 2 area of 128 data each */
	/* Increment after each group of mcu */
	setup.wraddr_y[5].inc	= eo_cnt;
	/* 2 memory area are used for the ping pong */
	setup.wraddr_y[5].modulo = 2;
	/* Half the mem */
	setup.wraddr_y[5].mult   = (nb_l4x4_y  + nb_l4x4_uv) * nb_mcu;

	/* Line number in a column 4x8, 4x4, 4x2 and 4x1 for
	 * respectivy decimation v1,v2,v4 and v8
	 */
	/* Increment after each incoming data */
	setup.wraddr_uv[0].inc	= 1;
	/* nb line in a block are 8>>vdec */
	setup.wraddr_uv[0].modulo = 8 >> state->vdec_uv;
	/* increment the address of 4 = (1<<2) after each data */
	setup.wraddr_uv[0].shift  = my_log2[nb_mcu] + 1;
	eo_cnt = GET_CNT(setup.wraddr_uv[0]);

	/* Column 4x8, 4x4, 4x2 and 4x1 counter  */
	/* Increment At the end of each column */
	setup.wraddr_uv[1].inc	= eo_cnt;
	/* Always 2 col in a blk */
	setup.wraddr_uv[1].modulo = 2;
	/* increment the address of 1 */
	setup.wraddr_uv[1].shift  = 0;
	eo_cnt = GET_CNT(setup.wraddr_uv[1]);

	/* Unused */
	setup.wraddr_uv[2].inc	= 1;
	setup.wraddr_uv[2].modulo = 1;
	setup.wraddr_uv[2].shift  = 0;

	/* MCU management */
	/* Increment At the end of each column */
	setup.wraddr_uv[3].inc	= 2*eo_cnt;
	/* Count all mcu inside a pair */
	setup.wraddr_uv[3].modulo = nb_mcu;
	/* increment the address of 2 */
	setup.wraddr_uv[3].shift  = 1;
	eo_cnt = GET_CNT(setup.wraddr_uv[3]);

	/* Unused */
	setup.wraddr_uv[4].inc	= 1;
	setup.wraddr_uv[4].modulo = 1;
	setup.wraddr_uv[4].shift  = 0;

	/* Ping pong : split the memory in 2 area of 128 data each */
	/* Increment after each group of mcu */
	setup.wraddr_uv[5].inc	= eo_cnt;
	/* 2 memory area are used for the ping pong */
	setup.wraddr_uv[5].modulo = 2;
	/* Half the mem */
	setup.wraddr_uv[5].mult   = (nb_l4x4_y  + nb_l4x4_uv) * nb_mcu;

	/* Store 32 only */
	setup.stbus_y[0].inc	= 1;
	setup.stbus_y[0].modulo = 4;
	setup.stbus_y[0].shift  = 0;
	eo_cnt = GET_CNT(setup.stbus_y[0]);

	/* Line number in a mcu */
	setup.stbus_y[1].inc	= eo_cnt;
	setup.stbus_y[1].modulo = (state->scan_in.vy * 8) >> state->vdec_y;
	setup.stbus_y[1].mult   = pitch_blk;
	eo_cnt = GET_CNT(setup.stbus_y[1]);

	/* Group of mcu in a line */
	setup.stbus_y[2].inc	= eo_cnt;
	setup.stbus_y[2].modulo = pitch_blk / 4;
	setup.stbus_y[2].shift  = 2; /* ST32 */
	eo_cnt = GET_CNT(setup.stbus_y[2]);

	/* Group of mcu line */
	setup.stbus_y[3].inc	= eo_cnt;
	setup.stbus_y[3].modulo = 0x10000;
	setup.stbus_y[3].mult   = eo_cnt;

	/* Store 32 only */
	setup.stbus_uv[0].inc	= 1;
	setup.stbus_uv[0].modulo = 2 << my_log2[nb_mcu];
	setup.stbus_uv[0].shift  = 0;
	eo_cnt = GET_CNT(setup.stbus_uv[0]);

	/* Line number in a mcu */
	setup.stbus_uv[1].inc	= eo_cnt;
	setup.stbus_uv[1].modulo = 8 >> state->vdec_uv;
	setup.stbus_uv[1].mult   = 2 * pitch_blk / state->scan_out.hy;
	eo_cnt = GET_CNT(setup.stbus_uv[1]);

	/* Group of mcu in a line */
	setup.stbus_uv[2].inc	= eo_cnt;
	setup.stbus_uv[2].modulo = pitch_blk / 4;
	setup.stbus_uv[2].shift  = my_log2[nb_mcu] + 1; /* ST32 */
	eo_cnt = GET_CNT(setup.stbus_uv[2]);

	/* Group of mcu line */
	setup.stbus_uv[3].inc	= eo_cnt;
	setup.stbus_uv[3].modulo = 0x10000;
	setup.stbus_uv[3].mult   = eo_cnt;

	/* Nb data luma to write */
	setup.rd_mode.stime   = setup.mode.stime  * nb_mcu / 2;
	/* NB data luma + chroma to write */
	setup.rd_mode.modulo  = setup.mode.modulo * nb_mcu / 2;
	/* Number of read in a group of mcu luma */
	setup.rd_sync_modulo  = setup.stbus_y[2].inc + setup.stbus_uv[2].inc;
	setup.rd_full_modulo  = 0;
	setup.rd_mem_modulo   = 2 * setup.rd_sync_modulo;
	/* Nb data to write before starting stbus writing */
	setup.wr_sync_modulo  = setup.mode.modulo * nb_mcu;
	setup.wr_full_modulo  = 2;
	setup.chunk.stime	 = 1;
	setup.chunk.modulo	= 1;

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG0, init_phase,
			 setup.init_phase);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG0, offset_c,
			  setup.offset_c);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG0, mode_modulo,
			  setup.mode.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG0, mode_stime,
			  setup.mode.stime - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG0, data);

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG1, modulo_l2,
			 setup.wraddr_y[2].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, inc_l2,
			  setup.wraddr_y[2].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, shift_l1,
			  setup.wraddr_y[1].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, modulo_l1,
			  setup.wraddr_y[1].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, inc_l1,
			  setup.wraddr_y[1].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, shift_l0,
			  setup.wraddr_y[0].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, modulo_l0,
			  setup.wraddr_y[0].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG1, inc_l0,
			  setup.wraddr_y[0].inc - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG1, data);

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG2, shift_l4,
			 setup.wraddr_y[4].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG2, modulo_l4,
			  setup.wraddr_y[4].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG2, inc_l4,
			  setup.wraddr_y[4].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG2, shift_l3,
			  setup.wraddr_y[3].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG2, modulo_l3,
			  setup.wraddr_y[3].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG2, inc_l3,
			  setup.wraddr_y[3].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG2, shift_l2,
			  setup.wraddr_y[2].shift);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG2, data);

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG3, mult_c5,
			 setup.wraddr_uv[5].mult);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG3, modulo_c5,
			  setup.wraddr_uv[5].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG3, inc_c5,
			  setup.wraddr_uv[5].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG3, mult_l5,
			  setup.wraddr_y[5].mult);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG3, modulo_l5,
			  setup.wraddr_y[5].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG3, inc_l5,
			  setup.wraddr_y[5].inc - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG3, data);

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG4, modulo_c2,
			 setup.wraddr_uv[2].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, inc_c2,
			  setup.wraddr_uv[2].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, shift_c1,
			  setup.wraddr_uv[1].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, modulo_c1,
			  setup.wraddr_uv[1].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, inc_c1,
			  setup.wraddr_uv[1].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, shift_c0,
			  setup.wraddr_uv[0].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, modulo_c0,
			  setup.wraddr_uv[0].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG4, inc_c0,
			  setup.wraddr_uv[0].inc - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG4, data);

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG5, shift_c4,
			 setup.wraddr_uv[4].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG5, modulo_c4,
			  setup.wraddr_uv[4].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG5, inc_c4,
			  setup.wraddr_uv[4].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG5, shift_c3,
			  setup.wraddr_uv[3].shift);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG5, modulo_c3,
			  setup.wraddr_uv[3].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG5, inc_c3,
			  setup.wraddr_uv[3].inc - 1);
	data |= SET_FIELD(OUTSTAGE_WRADDR_PROG5, shift_c2,
			  setup.wraddr_uv[2].shift);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG5, data);

	data = SET_FIELD(OUTSTAGE_DATAIN_PROG2, mux_op0_modulo,
			 setup.mux_op0.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG2, mux_op0_stime,
			  setup.mux_op0.stime - 1);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG2, dummy_modulo0,
			  setup.dummy0.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG2, dummy_stime0,
			  setup.dummy0.stime - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_DATAIN_PROG2, data);

	data = SET_FIELD(OUTSTAGE_DATAIN_PROG3, swap_en,
			 setup.swap_en);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG3, mux_op1,
			  setup.mux_op1);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG3, dummy_stime1,
			  setup.dummy1.stime - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_DATAIN_PROG3, data);

	data = SET_FIELD(OUTSTAGE_DATAIN_PROG4, ack_modulo,
			 setup.ack.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG4, ack_stime,
			  setup.ack.stime - 1);
	data |= SET_FIELD(OUTSTAGE_DATAIN_PROG4, dummy_modulo1,
			  setup.dummy1.modulo - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_DATAIN_PROG4, data);

	data = SET_FIELD(OUTSTAGE_ByteEnn0, byteop0_modulo,
			 setup.byteop0.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_ByteEnn0, byteop0_stime,
			  setup.byteop0.stime - 1);
	data |= SET_FIELD(OUTSTAGE_ByteEnn0, byteen_mode_modulo,
			  setup.byteen.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_ByteEnn0, byteen_mode_stime,
			  setup.byteen.stime - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_ByteEnn0, data);

	data = SET_FIELD(OUTSTAGE_ByteEnn1, byteop2,
			 setup.byteop2);
	data |= SET_FIELD(OUTSTAGE_ByteEnn1, byteop1_modulo,
			  setup.byteop1.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_ByteEnn1, byteop1_stime,
			  setup.byteop1.stime - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_ByteEnn1, data);

	data = SET_FIELD(OUTSTAGE_WRSync, wr_full_modulo,
			 setup.wr_full_modulo);
	data |= SET_FIELD(OUTSTAGE_WRSync, wr_syncout_modulo,
			  setup.wr_sync_modulo - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_WRSync, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, shift_c0,
			 setup.stbus_uv[0].shift);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, modulo_c0,
			  setup.stbus_uv[0].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, inc_c0,
			  setup.stbus_uv[0].inc - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, shift_l0,
			  setup.stbus_y[0].shift);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, modulo_l0,
			  setup.stbus_y[0].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, inc_l0,
			  setup.stbus_y[0].inc - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, mode_modulo,
			  setup.rd_mode.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, mode_stime,
			  setup.rd_mode.stime - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG0, write_posting, 1);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG0, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG1, mult_l1,
			 setup.stbus_y[1].mult);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG1, modulo_l1,
			  setup.stbus_y[1].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG1, inc_l1,
			  setup.stbus_y[1].inc - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG1, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG2, shift_l2,
			 setup.stbus_y[2].shift);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG2, modulo_l2,
			  setup.stbus_y[2].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG2, inc_l2,
			  setup.stbus_y[2].inc - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG2, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG3, modulo_l3,
			 setup.stbus_y[3].modulo  - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG3, inc_l3,
			  setup.stbus_y[3].inc - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG3, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG4, modulo_c1,
			 setup.stbus_uv[1].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG4, inc_c1,
			  setup.stbus_uv[1].inc - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG4, mult_l3,
			  setup.stbus_y[3].mult);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG4, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG5, modulo_c2,
			 setup.stbus_uv[2].modulo - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG5, inc_c2,
			  setup.stbus_uv[2].inc - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG5, mult_c1,
			  setup.stbus_uv[1].mult);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG5, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG6, inc_c3,
			 setup.stbus_uv[3].inc - 1);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG6, shift_c2,
			  setup.stbus_uv[2].shift);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG6, data);

	data = SET_FIELD(OUTSTAGE_STBUSADDR_PROG7, mult_c3,
			 setup.stbus_uv[3].mult);
	data |= SET_FIELD(OUTSTAGE_STBUSADDR_PROG7, modulo_c3,
			  setup.stbus_uv[3].modulo - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_STBUSADDR_PROG7, data);

	data = SET_FIELD(OUTSTAGE_RDSync, rd_full_modulo,
			 setup.rd_full_modulo);
	data |= SET_FIELD(OUTSTAGE_RDSync, rd_syncout_modulo,
			  setup.rd_sync_modulo - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_RDSync, data);

	data = SET_FIELD(OUTSTAGE_RDCtrl, chunk_modulo,
			 setup.chunk.modulo - 1);
	data |= SET_FIELD(OUTSTAGE_RDCtrl, chunk_stime,
			  setup.chunk.stime - 1);
	data |= SET_FIELD(OUTSTAGE_RDCtrl, rd_mem_modulo,
			  setup.rd_mem_modulo - 1);
	WRITE_REG(dev->iomem, OUTSTAGE_RDCtrl, data);
};

void c8jpg_setup_h_resize(struct stm_c8jpg_device *dev,
			  struct c8jpg_resizer_state *state)
{
	unsigned int data, phase;
	unsigned int offset;

	int *y_lut = state->y_table;
	int *uv_lut = state->uv_table;

	WRITE_REG(dev->iomem, RSZ_FILT_INIT_Y, state->init_y);
	WRITE_REG(dev->iomem, RSZ_FILT_INIT_UV, state->init_uv);
	WRITE_REG(dev->iomem, RSZ_FILT_INCR_Y, state->incr_y);
	WRITE_REG(dev->iomem, RSZ_FILT_INCR_UV, state->incr_uv);

	offset = REG_OFFSET(RSZ_FILTER_HRND0);

	/* luma horizontal filtering */
	for (phase = 0; phase < 32; phase++, offset += 16) {
		WRITE_ADDR(dev->iomem, offset, lut_rounding_y[phase]);

		data = SET_FIELD(RSZ_FILTER_HCOEFFA0, hcoeff0, *y_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFA0, hcoeff1, *y_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFA0, hcoeff2, *y_lut++ - 64);
		WRITE_ADDR(dev->iomem, offset + 4, data);

		data = SET_FIELD(RSZ_FILTER_HCOEFFB0, hcoeff3, *y_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFB0, hcoeff4, *y_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFB0, hcoeff5, *y_lut++ - 64);
		WRITE_ADDR(dev->iomem, offset + 8, data);

		data = SET_FIELD(RSZ_FILTER_HCOEFFC0, hcoeff6, *y_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFC0, hcoeff7, *y_lut++ - 64);
		WRITE_ADDR(dev->iomem, offset + 12, data);
	}

	/* chroma horizontal filtering */
	for (phase = 0; phase < 32; phase++, offset += 16) {
		WRITE_ADDR(dev->iomem, offset, lut_rounding_uv[phase]);

		data = SET_FIELD(RSZ_FILTER_HCOEFFA0, hcoeff0, *uv_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFA0, hcoeff1, *uv_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFA0, hcoeff2, *uv_lut++ - 64);
		WRITE_ADDR(dev->iomem, offset + 4, data);

		data = SET_FIELD(RSZ_FILTER_HCOEFFB0, hcoeff3, *uv_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFB0, hcoeff4, *uv_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFB0, hcoeff5, *uv_lut++ - 64);
		WRITE_ADDR(dev->iomem, offset + 8, data);

		data = SET_FIELD(RSZ_FILTER_HCOEFFC0, hcoeff6, *uv_lut++ - 64);
		data |= SET_FIELD(RSZ_FILTER_HCOEFFC0, hcoeff7, *uv_lut++ - 64);
		WRITE_ADDR(dev->iomem, offset + 12, data);
	}
}

void c8jpg_setup_v_resize(struct stm_c8jpg_device *dev,
			  struct c8jpg_resizer_state *state)
{
	unsigned data;

	int *y_lut = lut_vdec_default[state->vdec_y];
	int *uv_lut = lut_vdec_default[state->vdec_uv];

	/* luma vertical filtering */
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET, lut_rounding_v[0]);

	data = SET_FIELD(RSZ_FILTER_VCOEFFA0, vcoeff0, *y_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFA0, vcoeff1, *y_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFA0, vcoeff2, *y_lut++ - 64);
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 4, data);

	data = SET_FIELD(RSZ_FILTER_VCOEFFB0, vcoeff3, *y_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFB0, vcoeff4, *y_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFB0, vcoeff5, *y_lut++ - 64);
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 8, data);

	data = SET_FIELD(RSZ_FILTER_VCOEFFC0, vcoeff6, *y_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFC0, vcoeff7, *y_lut++ - 64);
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 12, data);

	/* chroma vertical filtering */
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 16, lut_rounding_v[1]);

	data = SET_FIELD(RSZ_FILTER_VCOEFFA0, vcoeff0, *uv_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFA0, vcoeff1, *uv_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFA0, vcoeff2, *uv_lut++ - 64);
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 20, data);

	data = SET_FIELD(RSZ_FILTER_VCOEFFB0, vcoeff3, *uv_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFB0, vcoeff4, *uv_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFB0, vcoeff5, *uv_lut++ - 64);
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 24, data);

	data = SET_FIELD(RSZ_FILTER_VCOEFFC0, vcoeff6, *uv_lut++ - 64);
	data |= SET_FIELD(RSZ_FILTER_VCOEFFC0, vcoeff7, *uv_lut++ - 64);
	WRITE_ADDR(dev->iomem, RSZ_FILTER_VRND0_OFFSET + 28, data);
}

void c8jpg_hw_init(struct stm_c8jpg_device *dev)
{
	WRITE_REG(dev->iomem, MCU_SOFT_RESET, 3);
	WRITE_REG(dev->iomem, MCU_CLK_CTRL, 1);
	WRITE_REG(dev->iomem, MCU_ITM, 0);
	WRITE_REG(dev->iomem, MCU_WATCHDOG_CNT_INIT, 64000);
}

void c8jpg_hw_reset(struct stm_c8jpg_device *dev)
{
	WRITE_REG(dev->iomem, MCU_SOFT_RESET, 3);
	WRITE_REG(dev->iomem, MCU_CLK_CTRL, 0);
}

void c8jpg_set_source(struct stm_c8jpg_device *dev,
		      unsigned long addr, unsigned long size)
{
	WRITE_REG(dev->iomem, MCU_BIT_BUFFER_BEGIN, addr);
	WRITE_REG(dev->iomem, MCU_PICTURE_BEGIN, addr);
	WRITE_REG(dev->iomem, MCU_BIT_BUFFER_STOP, addr + size);
	WRITE_REG(dev->iomem, MCU_PICTURE_STOP, addr + size);
}

void c8jpg_set_destination(struct stm_c8jpg_device *dev,
			   unsigned long yaddr, unsigned long uvaddr)
{
	WRITE_REG(dev->iomem, OUTSTAGE_RCN_Y_BUFFER_BEGIN, yaddr);
	WRITE_REG(dev->iomem, OUTSTAGE_RCN_C_BUFFER_BEGIN, uvaddr);
}

void c8jpg_set_cropping(struct stm_c8jpg_device *dev,
			struct c8jpg_output_info *output)
{
	unsigned int data = 0;

	data = SET_FIELD(RSZ_CROPPING_L_R, cropping_l, output->region.left);
	data |= SET_FIELD(RSZ_CROPPING_L_R, cropping_r,
			  output->region.left + output->region.width - 1);
	WRITE_REG(dev->iomem, RSZ_CROPPING_L_R, data);

	data = SET_FIELD(RSZ_CROPPING_T_B, cropping_t, output->region.top);
	data |= SET_FIELD(RSZ_CROPPING_T_B, cropping_b,
			  output->region.top + output->region.height - 1);
	WRITE_REG(dev->iomem, RSZ_CROPPING_T_B, data);
}

void c8jpg_set_output_format(struct stm_c8jpg_device *dev,
			     struct c8jpg_resizer_state *state)
{
	unsigned int data = 0;

	WRITE_REG(dev->iomem, RSZ_SCAN_OUT, state->scan_out.sampling);

	data = SET_FIELD(OUTSTAGE_WRADDR_PROG0, init_phase, 1);
	WRITE_REG(dev->iomem, OUTSTAGE_WRADDR_PROG0, data);

	data = SET_FIELD(RSZ_PIC_OUT_SIZE, Picture_width, state->mcu_width);
	WRITE_REG(dev->iomem, RSZ_PIC_OUT_SIZE, data);

	data = SET_FIELD(RSZ_FORMAT_OUT, LumaVDEC, state->vdec_y);
	data |= SET_FIELD(RSZ_FORMAT_OUT, ChromaVDEC, state->vdec_uv);
	WRITE_REG(dev->iomem, RSZ_FORMAT_OUT, data);
}

void c8jpg_decode_image(struct stm_c8jpg_device *dev)
{
	unsigned data = 0;

	data = SET_FIELD(MCU_JPG_START, decoding_start, 1);
	WRITE_REG(dev->iomem, MCU_JPG_START, data);
}
