#include <linux/kernel.h>
#include <asm/irq.h>

#define __lpm_code			__attribute__ ((section (".text.lpm")))


#define read8(addr)			(*((volatile unsigned char *)(addr)))
#define read16(addr)			(*((volatile unsigned short *)(addr)))
#define read32(addr)			(*((volatile unsigned int *)(addr)))
#define write8(v, addr)			(*((volatile unsigned char *)(addr)) = (unsigned char)(v))
#define write16(v, addr)		(*((volatile unsigned short *)(addr)) = (unsigned short)(v))
#define write32(v, addr)		(*((volatile unsigned int *)(addr)) = (unsigned int)(v))

#define ST7105_PTI_BASE_ADDRESS		0xfe230000
#define WAKEUP_PTI_OFFSET		0x9984

#define ST7105_PIO15_BASE		0xfe018000
#define ST7105_PIO4_BASE		0xfd024000

#define CKGA_BASE_ADDRESS		0xfe213000
#define CKGB_BASE_ADDRESS		0xfe000000

#define SYS_CONFIG_BASE			0xfe001000
#define SYS_CONFIG_9			(SYS_CONFIG_BASE + 0x124)

#define SH4_STBCR			0xffffff82
#define SH4_STBCR2			0xffffff88

typedef volatile unsigned char *const          sh4_byte_reg_t;
typedef volatile unsigned short *const         sh4_word_reg_t;
typedef volatile unsigned int *const           sh4_dword_reg_t;
typedef volatile unsigned long long *const     sh4_gword_reg_t;

#define SH4_TMU_REGS_BASE		0xffd80000

#define SH4_BYTE_REG(address)  ((sh4_byte_reg_t) (address))
#define SH4_WORD_REG(address)  ((sh4_word_reg_t) (address))
#define SH4_DWORD_REG(address) ((sh4_dword_reg_t) (address))
#define SH4_GWORD_REG(address) ((sh4_gword_reg_t) (address))

/* Timer Unit control registers (common to all SH4 variants) */
#define TSTR		SH4_BYTE_REG(SH4_TMU_REGS_BASE + 0x04)
#define TOCR		SH4_BYTE_REG(SH4_TMU_REGS_BASE + 0x00)
#define TCOR0		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x08)
#define TCNT0		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x0c)
#define TCR0		SH4_WORD_REG(SH4_TMU_REGS_BASE + 0x10)
#define TCOR1		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x14)
#define TCNT1		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x18)
#define TCR1		SH4_WORD_REG(SH4_TMU_REGS_BASE + 0x1c)
#define TCOR2		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x20)
#define TCNT2		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x24)
#define TCR2		SH4_WORD_REG(SH4_TMU_REGS_BASE + 0x28)
#define TCPR2		SH4_DWORD_REG(SH4_TMU_REGS_BASE + 0x2c)

#define LED_ON \
{ \
	write32((1 << 0), ST7105_PIO4_BASE + 0x28); \
	write32((1 << 0), ST7105_PIO4_BASE + 0x34); \
	write32((1 << 0), ST7105_PIO4_BASE + 0x48); \
	write32((1 << 0), ST7105_PIO4_BASE + 0x04); \
	while (1) {} \
}

unsigned long ____cacheline_aligned lpe_stack[128];

__asm__ (
".balign 32\n"
".global lpm_epilog\n"
".section .text.lpm\n"
"lpm_epilog:\n"

"bra __lpe_terminate, r0\n"
"nop\n"
);

static void __lpm_code sh_reset_TRAP0(void)
{
	register ulong sr;


	/* stop all timers */
	write8(0, TSTR);
	/* reinitialise all counters & status registers */
	write32(0xFFFFFFFF, TCNT0);
	write32(0xFFFFFFFF, TCOR0);
	write16(0, TCR0);
	write32(0xFFFFFFFF, TCNT1);
	write32(0xFFFFFFFF, TCOR1);
	write16(0, TCR1);
	write32(0xFFFFFFFF, TCNT2);
	write32(0xFFFFFFFF, TCOR2);
	write16(0, TCR2);
	/* make sure that all registers have been updated */
	wmb();


//	LED_ON

	/* raise trap #0 */
	asm ("stc sr, %0":"=r" (sr));
	sr |= (1 << 28);       /* set block bit */
	asm ("ldc %0, sr": :"r" (sr));

	asm volatile ("trapa #0");
}


#if defined(CONFIG_SH_ST_C275) || defined(CONFIG_SH_ST_CUSTOM002012)
static void __lpm_code  c275_reset(void)
{
	write8(0, SH4_STBCR);
	write8(0, SH4_STBCR2);

	write32(0, CKGA_BASE_ADDRESS+0x10);
	write32(0, CKGA_BASE_ADDRESS+0x14);
	write32(0, CKGA_BASE_ADDRESS+0x24);
	write32(0, CKGA_BASE_ADDRESS+0x4c);

	write32(0x77, CKGB_BASE_ADDRESS+0x58);
	write32(0x18, CKGB_BASE_ADDRESS+0x5c);
	write32(0xff, CKGB_BASE_ADDRESS+0xa0);
	write32(0x200, CKGB_BASE_ADDRESS+0xac);
	write32(0x3fff, CKGB_BASE_ADDRESS+0xb0);

	/* Restore the reset chain. Note that we cannot use the sysconfig
	 * API here, as it calls kmalloc(GFP_KERNEL) but we get here with
	 * interrupts disabled.
	 */

#define DEFAULT_RESET_CHAIN     0xa8c

	write32(DEFAULT_RESET_CHAIN, SYS_CONFIG_9);
	write32(0, 0xfe239974);
}
#endif

void __lpm_code __lpe_terminate(unsigned int evt)
{
	if (evt < 0x400)
		evt = 0; //ilc2irq(evt)
	else
		evt = evt2irq(evt);

#if defined(CONFIG_SH_ST_C275) || defined(CONFIG_SH_ST_CUSTOM002012)
//	write32((1 << 7), ST7105_PIO15_BASE + 0x04); // 1 => PIO15.7
#endif


#if defined(CONFIG_SH_ST_C275) || defined(CONFIG_SH_ST_CUSTOM002012)
	c275_reset();
#endif

	write32(ST7105_PTI_BASE_ADDRESS, ST7105_PTI_BASE_ADDRESS + WAKEUP_PTI_OFFSET);
	write32(evt, ST7105_PTI_BASE_ADDRESS + WAKEUP_PTI_OFFSET + 4);

//	LED_ON
	sh_reset_TRAP0();
}
