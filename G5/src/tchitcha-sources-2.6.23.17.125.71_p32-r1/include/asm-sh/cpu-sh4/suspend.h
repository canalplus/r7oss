/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __suspend_sh4_h__
#define __suspend_sh4_h__

#define BASE_DATA		(0x0)
	/* to identify the ClockGenA registers */
#define BASE_CLK		(0x1)
	/* to identify the ClockGenB registers */
#define BASE_CLKB		(0x2)
	/* to identify the Sysconf registers */
#define BASE_SYS		(0x3)


#define OP_END			(0*4)	/* no more data in the table */
#define OP_END_NO_SLEEP		(1*4)
#define OP_SOURCE		(2*4)
#define OP_LOAD			(3*4)	/* load  @(offset, Reg_idx) */
#define OP_ILOAD_SRC0		(4*4)	/* load_imm (from itable) on r1 */
#define OP_ILOAD_SRC1		(5*4)	/* load_imm (from itable) on r3 */
#define OP_ILOAD_SRC2		(6*4)	/* load_imm (from itable) on r4 */
#define OP_ILOAD_DEST		(7*4)	/* load_imm (from table) on r2 */

#define OP_STORE		(8*4)	/* store @(offset, Reg_idx) */
#define OP_OR			(9*4)
#define OP_AND			(10*4)
#define OP_NOT			(11*4)
/* WHILE_EQ (idx, offset, mask, value)
 * wait until the mask bits is equal to value
 */
#define OP_WHILE_EQ		(12*4)
/* WHILE_NEQ (idx, offset, mask, value)
 * wait until the mask bits isn't equal to value
 */
#define OP_WHILE_NEQ		(13*4)

#define OP_DELAY		(14*4)	/* A loop delay */

#define OP_LOAD_SRC0		(15*4)	/* Load SRC_0 from resources */
#define OP_LOAD_SRC1		(16*4)	/* Load SRC_1 from  */
#define OP_LOAD_SRC2		(17*4)	/* Load SRC_2 from table */

#define OP_SAVE_TMU_HS		(18*4)	/* CONFIG_SH_LPRTC */
#define OP_SAVE_TMU_LS_INIT	(19*4)
#define OP_SAVE_TMU_LS_EXIT	(20*4)
#define OP_CLOSE_RTC		(21*4)

#define _OPCODE_TABLE_SIZE_	5	/* 5 => room for 5 * L1_CACHE_BYTES
					 * / sizeof(u32) = 40 op codes */

#ifndef __ASSEMBLY__

struct sh4_suspend_t {
	unsigned long *iobase;   /* the external iomemory resource 		*/
	unsigned long l_p_j;
	unsigned long wrt_tbl; /* the writeable table address			*/
	unsigned long wrt_size; /* the writeable table size in dcache line!	*/
	unsigned long stby_tbl;	/* the standby instruction table address	*/
	unsigned long stby_size;/* the standby instruction table size in dcache line*/
	unsigned long mem_tbl;	/* the mem instruction table address		*/
	unsigned long mem_size;	/* the mem instruction table size in dcache line*/
	int (*evt_to_irq)(unsigned long evt); /* translate the INTEVT code
					       * to the irq number */
	void (*lprtc_enter)(void); /* switch to the low-power software RTC	*/
	void (*lprtc_exit)(void); /* switch back to regular timekeeping		*/
	struct pm_ops ops;
};

int sh4_suspend_register(struct sh4_suspend_t *data);

/* Operations */
#define _OR()					OP_OR
#define _AND()					OP_AND
#define _NOT()					OP_NOT
#define _DELAY()				OP_DELAY
#define _WHILE_NEQ()				OP_WHILE_NEQ
#define _WHILE_EQ()				OP_WHILE_EQ
#define _LOAD()					OP_LOAD
#define _STORE()				OP_STORE
/*
 * N.S.: DATA_LOAD and DATA_STORE work directly on DEST reg.
 *       To load something on SCR0, SRC1 and SRC2 Use
 *       following instructions
 */
#define _LOAD_SRC0()				OP_LOAD_SRC0
#define _LOAD_SRC1()				OP_LOAD_SRC1
#define _LOAD_SRC2()				OP_LOAD_SRC2

#define _RTC_SAVE_TMU_HS()			OP_SAVE_TMU_HS
#define _RTC_SAVE_TMU_LS_INIT()		OP_SAVE_TMU_LS_INIT
#define _RTC_SAVE_TMU_LS_EXIT()		OP_SAVE_TMU_LS_EXIT
#define _RTC_CLOSE()				OP_CLOSE_RTC

#define _END()					OP_END
#define _END_NO_SLEEP()				OP_END_NO_SLEEP

#define DATA_SOURCE(idx)					\
	OP_SOURCE, BASE_DATA, (4*(idx))

#define RAW_SOURCE(orig, reg_offset)				\
	OP_SOURCE, (4*(orig)), (reg_offset)

#define SYS_SOURCE(reg_offset)					\
	OP_SOURCE, BASE_SYS, (reg_offset)

#define CLK_SOURCE(reg_offset)					\
	OP_SOURCE, BASE_CLK, (reg_offset)

#define DATA_LOAD(idx)				DATA_SOURCE(idx), _LOAD()
#define DATA_STORE(idx)				DATA_SOURCE(idx), _STORE()

/* a raw load */
#define RAW_LOAD(base, reg_offset)		RAW_SOURCE(base, reg_offset), _LOAD()

#define SYS_LOAD(reg_offset)			RAW_LOAD(BASE_SYS, reg_offset)
#define CLK_LOAD(reg_offset)			RAW_LOAD(BASE_CLK, reg_offset)
#define CLKB_LOAD(reg_offset)			RAW_LOAD(BASE_CLKB, reg_offset)

/* A raw store          */
#define RAW_STORE(base, reg_offset)		RAW_SOURCE(base, reg_offset), _STORE()
#define SYS_STORE(reg_offset)			RAW_STORE(BASE_SYS, reg_offset)
#define CLK_STORE(reg_offset)			RAW_STORE(BASE_CLK, reg_offset)
#define CLKB_STORE(reg_offset)			RAW_STORE(BASE_CLKB, reg_offset)

#define IMMEDIATE_SRC0(value)			OP_ILOAD_SRC0, (value)
#define IMMEDIATE_SRC1(value)			OP_ILOAD_SRC1, (value)
#define IMMEDIATE_SRC2(value)			OP_ILOAD_SRC2, (value)
#define IMMEDIATE_DEST(value)			OP_ILOAD_DEST, (value)

/* Set Or-ing the bits in the register */
#define RAW_OR_LONG(orig, reg_offset, or_bits)	\
	RAW_SOURCE(orig, reg_offset),		\
	 _LOAD(),				\
	IMMEDIATE_SRC0(or_bits),		\
	_OR(),					\
	RAW_SOURCE(orig, reg_offset),		\
	_STORE()

#define SYS_OR_LONG(reg_offset, or_bits)			\
	RAW_OR_LONG(BASE_SYS, reg_offset, or_bits)

#define CLK_OR_LONG(reg_offset, or_bits)			\
	RAW_OR_LONG(BASE_CLK, reg_offset, or_bits)

#define CLKB_OR_LONG(reg_offset, or_bits)			\
	RAW_OR_LONG(BASE_CLKB, reg_offset, or_bits)


#define DATA_OR_LONG(idx_mem, idx_mask)			\
	DATA_SOURCE(idx_mem),				\
	_LOAD_SRC0(),					\
	_LOAD(),					\
	DATA_SOURCE(idx_mask),				\
	_LOAD_SRC0(),					\
	_OR(),						\
	DATA_SOURCE(idx_mem),				\
	_LOAD_SRC0(),					\
	_STORE()

#define DATA_AND_LONG(idx_mem, idx_mask)		\
	DATA_SOURCE(idx_mem),				\
	_LOAD_SRC0(),					\
	_LOAD(),					\
	DATA_SOURCE(idx_mask),				\
	_LOAD_SRC0(),					\
	_AND(),						\
	DATA_SOURCE(idx_mem),				\
	_LOAD_SRC0(),					\
	_STORE()

#define DATA_AND_NOT_LONG(idx_mem, idx_mask)		\
	DATA_SOURCE(idx_mem),				\
	_LOAD_SRC0(),					\
	_LOAD(),					\
	DATA_SOURCE(idx_mask),				\
	_LOAD_SRC1(),					\
	_NOT(),						\
	_AND(),						\
	DATA_SOURCE(idx_mem),				\
	_LOAD_SRC0(),					\
	_STORE()

/* Set And-ing the bits in the register */
#define RAW_AND_LONG(orig, reg_offset, and_bits)	\
	RAW_SOURCE(orig, reg_offset),			\
	_LOAD(), /* dest = @(iomem) */			\
	IMMEDIATE_SRC0(and_bits),			\
	_AND(),	 /* dest &= src0 */			\
	RAW_SOURCE(orig, reg_offset),			\
	_STORE() /* @(iomem) = dest */

#define SYS_AND_LONG(reg_offset, and_bits)			\
		RAW_AND_LONG(BASE_SYS, reg_offset, and_bits)

#define CLK_AND_LONG(reg_offset, and_bits)			\
		RAW_AND_LONG(BASE_CLK, reg_offset, and_bits)

#define CLKB_AND_LONG(reg_offset, and_bits)			\
		RAW_AND_LONG(BASE_CLKB, reg_offset, and_bits)

/* Standard Poke */
#define RAW_POKE(base, reg_offset, value)		\
	IMMEDIATE_DEST(value),				\
	RAW_SOURCE(base, reg_offset),			\
	_STORE()

#define SYS_POKE(reg_offset, value)			\
	RAW_POKE(BASE_SYS, reg_offset, value)

#define CLK_POKE(reg_offset, value)			\
	RAW_POKE(BASE_CLK, reg_offset, value)

#define CLKB_POKE(reg_offset, value)			\
	RAW_POKE(BASE_CLKB, reg_offset, value)

/* While operation */
#define RAW_WHILE_EQ(orig, offset, mask, value)		\
	RAW_SOURCE(orig, offset),			\
	IMMEDIATE_SRC1(mask),				\
	IMMEDIATE_SRC2(value),				\
	_WHILE_EQ()

#define RAW_WHILE_NEQ(orig, offset, mask, value)	\
	RAW_SOURCE(orig, offset),			\
	IMMEDIATE_SRC1(mask),				\
	IMMEDIATE_SRC2(value),				\
	_WHILE_NEQ()

#define DATA_WHILE_EQ(idx_iomem, idx_mask, idx_value)	\
	DATA_SOURCE(idx_value),				\
	_LOAD_SRC2(),					\
	DATA_SOURCE(idx_mask),				\
	_LOAD_SRC1(),					\
	DATA_SOURCE(idx_iomem),				\
	_LOAD_SRC0(),					\
	_WHILE_EQ()

#define DATA_WHILE_NEQ(idx_iomem, idx_mask, idx_value)	\
	DATA_SOURCE(idx_value),				\
	_LOAD_SRC2(),					\
	DATA_SOURCE(idx_mask),				\
	_LOAD_SRC1(),					\
	DATA_SOURCE(idx_iomem),				\
	_LOAD_SRC0(),					\
	_WHILE_NEQ()

#define SYS_WHILE_EQ(offset, mask, value)		\
	RAW_WHILE_EQ(BASE_SYS, offset, mask, value)

#define CLK_WHILE_EQ(offset, mask, value)		\
	RAW_WHILE_EQ(BASE_CLK, offset, mask, value)

#define CLKB_WHILE_EQ(offset, mask, value)		\
	RAW_WHILE_EQ(BASE_CLKB, offset, mask, value)

#define SYS_WHILE_NEQ(offset, mask, value)		\
	RAW_WHILE_NEQ(BASE_SYS, offset, mask, value)

#define CLK_WHILE_NEQ(offset, mask, value)		\
	RAW_WHILE_NEQ(BASE_CLK, offset, mask, value)

#define CLKB_WHILE_NEQ(offset, mask, value)		\
	RAW_WHILE_NEQ(BASE_CLKB, offset, mask, value)

#endif
#endif
