#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/stm/coprocessor.h>
#include <linux/stm/sysconf.h>
#include <linux/pfn.h>
#include <asm-generic/sections.h>
#include <asm/io.h>

struct coproc_board_info coproc_info = {
	.name = "st231",
	.max_coprs = CONFIG_STM_NUM_COPROCESSOR,
};

coproc_t coproc[CONFIG_STM_NUM_COPROCESSOR];

static struct sysconf_field* copro_reset_out;

struct cpu_reg {
	struct sysconf_field* boot;
	struct sysconf_field* reset;
};
static struct cpu_reg cpu_regs[CONFIG_STM_NUM_COPROCESSOR];

int coproc_cpu_open(coproc_t * cop)
{
	return (0);
}

int __init coproc_cpu_init(coproc_t * cop)
{
	unsigned int id = cop->pdev.id;

#if defined CONFIG_CPU_SUBTYPE_STB7100
	const unsigned int boot_lookup[] =   { 28, 26 };
	const unsigned int reset_lookup[]  = { 29, 27 };
#elif defined CONFIG_CPU_SUBTYPE_STX7105
	const unsigned int boot_lookup[] =   { 28, 26 };
	const unsigned int reset_lookup[]  = { 29, 27 };
#elif defined CONFIG_CPU_SUBTYPE_STX7108
	const unsigned int boot_lookup[] =   { 6, 7, 8};
	const unsigned int reset_lookup[]  = { 4, 5, 6,};
#elif defined CONFIG_CPU_SUBTYPE_STX7111
	const unsigned int boot_lookup[] =   { 28, 26 };
	const unsigned int reset_lookup[]  = { 29, 27 };
#elif defined CONFIG_CPU_SUBTYPE_STX7141
	const unsigned int boot_lookup[] =   { 28, 26 };
	const unsigned int reset_lookup[]  = { 29, 27 };
#elif defined CONFIG_CPU_SUBTYPE_STX7200
	const unsigned int boot_lookup[] =   { 28, 36, 26, 34 };
	const unsigned int reset_lookup[]  = { 29, 37, 27, 35 };
#else
#error Need to define the sysconf configuration for this CPU subtype
#endif

	BUG_ON(id >= ARRAY_SIZE(boot_lookup));
	BUG_ON(id >= coproc_info.max_coprs);

#ifdef CONFIG_CPU_SUBTYPE_STX7108
		
	if(!copro_reset_out)
	if(!(copro_reset_out=sysconf_claim(SYS_CFG_BANK0, 9, 27, 28, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_9\n");
		return 1;
		}
		
	if(!cpu_regs[id].boot)
	if(!(cpu_regs[id].boot = sysconf_claim(SYS_CFG_BANK0, boot_lookup[id], 0, 31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n", boot_lookup[id]);
		return 1;
		}
	
	if(!cpu_regs[id].reset)
	if(!(cpu_regs[id].reset = sysconf_claim(SYS_CFG_BANK0, 0, reset_lookup[id],reset_lookup[id], NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n", reset_lookup[id]);
		return 1;
		}
	
#else
		
	if(!copro_reset_out)
	if(!(copro_reset_out=sysconf_claim(SYS_CFG, 9, 27, 28, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_9\n");
		return 1;
		}

	if(!cpu_regs[id].boot)
	if(!(cpu_regs[id].boot = sysconf_claim(SYS_CFG, boot_lookup[id], 0, 31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n", boot_lookup[id]);
		return 1;
		}

	if(!cpu_regs[id].reset)
	if(!(cpu_regs[id].reset = sysconf_claim(SYS_CFG, reset_lookup[id], 0,31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n", reset_lookup[id]);
		return 1;
		}

#endif

	return 0;
}

#ifdef CONFIG_CPU_SUBTYPE_STX7108

int coproc_cpu_grant(coproc_t * cop, unsigned long arg)
{
	u_long bootAddr;
	int id = cop->pdev.id;

	BUG_ON(id >= coproc_info.max_coprs);

	if (arg == 0)
		bootAddr = COPR_ADDR(cop, 0);
	else
		bootAddr = arg;

	DPRINTK(">>> platform: st231.%u start from 0x%x...\n",
					id, (unsigned int)bootAddr);

	sysconf_write(cpu_regs[id].boot, bootAddr);
	msleep(5);

	sysconf_write(cpu_regs[id].reset, 0) ;
	msleep(5);

	sysconf_write(cpu_regs[id].reset, 1);
	msleep(10);

	cop->control |= COPROC_RUNNING;
	return (0);
}

#else

int coproc_cpu_grant(coproc_t * cop, unsigned long arg)
{
	u_long bootAddr;
	int id = cop->pdev.id;

	BUG_ON(id >= coproc_info.max_coprs);

	if (arg == 0)
		bootAddr = COPR_ADDR(cop, 0);
	else
		bootAddr = arg;

	DPRINTK(">>> platform: st231.%u start from 0x%x...\n",
					id, (unsigned int)bootAddr);

	/* bypass the st40 to reset only the coprocessor */
	sysconf_write(copro_reset_out, 3);
	msleep(5);

	sysconf_write(cpu_regs[id].boot, bootAddr);
	msleep(5);

	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) | 1) ;
	msleep(5);

	/* Now set the least significant bit to trigger the ST231 start */
   	bootAddr |= 1;
	sysconf_write(cpu_regs[id].boot, bootAddr);
	msleep(5);

      	bootAddr |= 0;
	sysconf_write(cpu_regs[id].boot, bootAddr);

	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) & ~1);
	msleep(10);

	/* remove the st40 bypass */
	sysconf_write(copro_reset_out, 0);

	cop->control |= COPROC_RUNNING;
	return (0);
}
#endif

int coproc_cpu_release(coproc_t * cop)
{
	/* do nothing! */
	return (0);
}

int coproc_cpu_reset(coproc_t * cop)
{
 	int id = cop->pdev.id;

 	DPRINTK("\n");
 	/* bypass the st40 to reset only the coprocessor */
 	sysconf_write(copro_reset_out,  1);
 	msleep(5);
 	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) | 1);
 	msleep(5);
 	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) & ~1);
 	msleep(10);

 	/* remove the st40 bypass */
 	sysconf_write(copro_reset_out, 0);

 	return 0;
}

void coproc_proc_other_info(coproc_t * cop_dump, struct seq_file *s_file)
{
	return;			/* Do nothing, doesn't delete it */
}

int coproc_check_area(u_long addr, u_long size, int i, coproc_t * coproc)
{
	/*
	 * This function is called if we failed to reserve the
	 * requested memory with the bootmem allocator.  This could be
	 * because the memory is outside the memory known to the boot
	 * memory allocator, or because it has been already reserved.
	 */
	unsigned long start_pfn = PFN_DOWN(addr);
	unsigned long end_pfn = PFN_UP(addr + size);

	if ((start_pfn >= min_low_pfn) && (end_pfn <= max_low_pfn)) {
		/*
		 * Region is contained entirely within Linux memory, and
		 * so should have been allocated.
		 *
		 * However we need to allow the region between the start of
		 * memory and the start of the kernel (typically
		 * CONFIG_ZERO_PAGE_OFFSET has been raised).
		 */
		if (end_pfn <= PFN_DOWN(__pa(_text)))
			return 0;

		printk(KERN_ERR "st-coprocessor: Region already reserved\n");
	} else if ((start_pfn > max_low_pfn) || (end_pfn < min_low_pfn)) {
		/* Region is entirely outside Linux memory. */
		return 0;
	} else {
		printk(KERN_ERR "st-coprocessor: Region spans memory boundary\n");
	}

	coproc[i].ram_offset = coproc[i].ram_size = 0;
	return 1;
}

