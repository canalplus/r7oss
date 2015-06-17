
#include <linux/kernel.h>
#include <asm/io.h>

#include "lowpower-ir-wakeup.h"

#define __lpm_code __attribute__ ((section (".text.lpm")))

#define xstr(s) #s
#define str(s) xstr(s)

#define IRB_BASE_ADDR			0xfd018000
#define IRB_RX_REG(offset)		(IRB_BASE_ADDR + (offset))
#define IRB_RX_STATUS			IRB_RX_REG(0x6C)	/* receive status        */
#define IRB_RX_SYM			IRB_RX_REG(0X44)	/* RX sym period capture */
#define IRB_RX_ON			IRB_RX_REG(0x40)	/* RX pulse time capture */
#define IRB_RX_INT_STATUS		IRB_RX_REG(0x4C)	/* RX IRQ status (R/W)   */
#define IRB_RX_INT_EN			IRB_RX_REG(0x48)	/* RX IRQ enable (R/W)   */
#define IRB_RX_INT_CLEAR 		IRB_RX_REG(0x58)	/* overrun status (W)    */

#define LIRC_STM_CLEAR_IRQ		0x38

#define IRB_RX_STA_LAST_SYMB		0x02
#define IRB_RX_STA_OVERRUN		0x04

#define IRB_RX_INT_CLR_LAST_SYMB	0x02
#define IRB_RX_INT_CLR_OVERRUN		0x04

#define IRB_RX_INT_EN_GLOBAL		0x01

#define write32(v, addr)		(*((volatile unsigned int *)(addr)) = (unsigned int)(v))
#define read32(addr)			(*((volatile unsigned int *)(addr)))

#define RSTEP2_SYMBOL_TIME		(500/10)
#define RSTEP2_SYMBOL_HALF_TIME		(RSTEP2_SYMBOL_TIME/2)
#define RSTEP2_FRAME_LEN		23

#define RSTER2_FRAME_STA_MASK		(1 << 22)
#define RSTER2_FRAME_STA_BIT		(RSTER2_FRAME_STA_MASK)
#define RSTER2_FRAME_CUSTOMID_MASK	(0x1f << 16)
#define RSTER2_FRAME_F_MASK		(0xff)

#define RSTEP2_CP_CUSTOM_ID		(0x5 << 16)

#define RSTEP2_VALID_CUSTOM_FRAME(x)	\
	(((x) & (RSTER2_FRAME_STA_MASK | RSTER2_FRAME_CUSTOMID_MASK)) == (RSTER2_FRAME_STA_BIT|RSTEP2_CP_CUSTOM_ID))

#define RSTEP2_F_CODE(x)		((x) & RSTER2_FRAME_F_MASK)

#define MAX_SYMBOLS			25

typedef struct {
	unsigned int symbol[MAX_SYMBOLS], mark[MAX_SYMBOLS];
	unsigned int nsym;
	unsigned int keycode;
	int err, serr, eof;
} ctx_t;


#define DUMP_SPACE_START0 ((sizeof(ctx_t) / sizeof(int)) + 1)
#define DUMP_SPACE_START (60)

extern const unsigned char __lpir_valid_keys[];

unsigned long ____cacheline_aligned lpir_data[LPIR_DATA_SIZE] = {
	[0 ... LPIR_DATA_SIZE-1] = 0,
};


__asm__ (
".balign 32\n"
".global lpir_check_keys\n"
".section .text.lpm\n"
"lpir_check_keys:\n"

/* NO code MUST be before this */

/* change stack  */
"mov.l __lpir_addr_stack, r0\n"
"mov.l r15, @-r0\n"
"mov r0, r15\n"

/* save registers */
"sts.l pr, @-r15\n"
"mov.l r1, @-r15\n"
"mov.l r2, @-r15\n"
"mov.l r3, @-r15\n"
"mov.l r4, @-r15\n"
"mov.l r5, @-r15\n"
"mov.l r6, @-r15\n"
"mov.l r7, @-r15\n"
"mov.l r8, @-r15\n"
"mov.l r9, @-r15\n"
"mov.l r10, @-r15\n"
"mov.l r11, @-r15\n"
"mov.l r12, @-r15\n"
"mov.l r13, @-r15\n"
"mov.l r14, @-r15\n"

/* call C function */
/* r4 and r5 contain the parameters, there are already set by the caller */
"bsr lpir_check_key_code\n"
"nop\n"

/* restore registers & stack */
"mov.l @r15+, r14\n"
"mov.l @r15+, r13\n"
"mov.l @r15+, r12\n"
"mov.l @r15+, r11\n"
"mov.l @r15+, r10\n"
"mov.l @r15+, r9\n"
"mov.l @r15+, r8\n"
"mov.l @r15+, r7\n"
"mov.l @r15+, r6\n"
"mov.l @r15+, r5\n"
"mov.l @r15+, r4\n"
"mov.l @r15+, r3\n"
"mov.l @r15+, r2\n"
"mov.l @r15+, r1\n"
"lds.l @r15+, pr\n"
"mov.l @r15, r15\n"

/* return */
"rts\n"
"nop\n"

".align 2\n"
"__lpir_addr_stack: .long (lpir_data + " str(LPIR_DATA_SIZE) " * 4)\n"
);


__asm(
".balign 32\n"
".section .text.lpm\n"
".global __lpir_valid_keys\n"
"__lpir_valid_keys: \n"
".byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 19, 20, 42, 0xff\n"
);

static unsigned int __lpm_code normalize_time(unsigned int v)
{
	return ((v + (RSTEP2_SYMBOL_TIME/10)) / RSTEP2_SYMBOL_HALF_TIME) * RSTEP2_SYMBOL_HALF_TIME;
}

static int __lpm_code valid_key_code(unsigned int keycode)
{
	int i, res = 0;
	unsigned char key;

	if (RSTEP2_VALID_CUSTOM_FRAME(keycode)) {
		key = RSTEP2_F_CODE(keycode);

		for (i = 0; __lpir_valid_keys[i] < 0xff; i++) {
			if (__lpir_valid_keys[i] == key) {
				res = 1;
				break;
			}
		}
	}

	return res;
}

static __lpm_code void reset_state(ctx_t *x)
{
	x->serr = x->err;

	x->nsym =
	x->err =
	x->eof = 0;
}

static unsigned int __lpm_code process_bits(unsigned int *nsym, unsigned int keycode, unsigned int tmpreg)
{
	/* process collected symbols, if any (2-bits per symbol) */
	while (*nsym >= 2) {
		/* make a room for 1 bit */
		keycode <<= 1;
		/* insert most significant bit of symbol */
		keycode |=  (tmpreg >> (*nsym-1)) & 1;
		*nsym -= 2;
	}

	return keycode;
}

static int __lpm_code process_symbols(ctx_t *x)
{
	unsigned int i, symbol, mark, tmpreg = 0, nsym = 0, keycode = 0;
	int result = 0;

	for (i = 0; i < x->nsym; i++) {
		symbol = normalize_time(x->symbol[i]);
		mark = normalize_time(x->mark[i]);


		/* convert time to numbers of bits per each symbol */
		symbol /= RSTEP2_SYMBOL_HALF_TIME;
		mark /= RSTEP2_SYMBOL_HALF_TIME;

		if (!symbol || symbol > 4 || !mark) {
			x->err = 3;
			goto reset;
		}
		x->symbol[i] = symbol;
		x->mark[i] = mark;
	}

	for (i = 0; i < x->nsym; i++) {
		symbol = x->symbol[i];
		mark = x->mark[i];
		/*
		 * a simple RSTEP2 protocol frame conversion to 32-bits word
		 *
		 *   250
		 * --------
		 *        |
		 *        |  250    => ON TIME = 250, PULSE TIME = 500 => {1} + {0}
		 *        --------
		 * <---- 500 ---->
		 *
		 *       500
		 * ---------------
		 *               |
		 *               |  250    => ON TIME = 500, PULSE TIME = 750 => {1, 1} + {0}
		 *               --------
		 * <-------- 750 ------->
		 *
		 *           750
		 * -----------------------
		 *                       |
		 *                       |  250    => ON TIME = 750, PULSE TIME = 1000 => {1, 1, 1} + {0}
		 *                        --------
		 * <------------ 1000 ------------>
		 *
		 */

		/* insert particular number 1's and zero's into a temporary register */
		tmpreg <<= mark;			/* make a room for mark-bits */
		tmpreg |= (1 << mark)-1;		/* set mark-bits to '1' */
		tmpreg <<= (symbol - mark);	/* insert zeros */
		nsym += symbol;			/* update number of symbols in tmpreg */

		keycode = process_bits(&nsym, keycode, tmpreg);
	}

	x->keycode = keycode;

	if (!x->err && valid_key_code(keycode)) {
		result = 1;
	}
reset:
	reset_state(x);

	return result;
}

unsigned int __lpm_code lpir_check_key_code(void)
{
	ctx_t *x = (ctx_t *)&lpir_data[0]; // the only place where we can put our "global" variables
	unsigned int status;
	unsigned int result = 0, symbol = 0, mark = 0;

	/*
	 * just in case we have enabled IRB_WAKEUP IRQ
	 */
#if 0
	/*Clear IRB Wakeup Interrupt*/
	status = read32(0xFD000000 + 0x488);
	status |= 0x10;
	write32(status, 0xFD000000 + 0x488);

	/*Clear IRB Wakeup Status*/
	status = read32(0xFD000000 + 0x288);
	status |= 0x10;
	write32(status, 0xFD000000 + 0x288);
#endif
	status = read32(IRB_RX_STATUS);

	while (status & 0x700) {

		if (status & 0x700) {
			symbol = (read32(IRB_RX_SYM) & 0xffff);
			mark = (read32(IRB_RX_ON) & 0xffff);

			if (x->nsym <  MAX_SYMBOLS) {
				x->symbol[x->nsym] = symbol;
				x->mark[x->nsym] = mark;
				x->nsym++;
			} else {
				x->err = 2;
			}
		}

		if (status & IRB_RX_STA_OVERRUN) {
			write32(IRB_RX_INT_CLR_OVERRUN, IRB_RX_INT_CLEAR);
			x->err = 1;
		}

		write32(0x38, IRB_RX_INT_CLEAR);

		if (symbol == 0xffff) { // EOF
			write32(0x3E, IRB_RX_INT_CLEAR);
			if (x->err || !x->nsym) {
				/* overrun occured - collected pulses may not be complete */
				reset_state(x);
			} else {
				x->eof = 1;
				x->symbol[x->nsym-1] = x->mark[x->nsym-1] + RSTEP2_SYMBOL_HALF_TIME;
				if (process_symbols(x)) {
					// block interrupts
#if 0
					status = read32(IRB_RX_INT_EN);
					status &= ~IRB_RX_INT_EN_GLOBAL;
					write32(status, IRB_RX_INT_EN);
#endif
					result = 1;
				}
			}
			break;
		}

		status = read32(IRB_RX_STATUS);
	}

	return result;
}


#if defined(LPIR_DATA_DUMP)

void lpir_data_dump(void)
{
	int i;

	for (i = 0; i < 100; i++)
		printk("lpir_data[%d] = %lu (0x%08lx)\n", i, lpir_data[i], lpir_data[i]);
}

#endif
