#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sysrq.h>
#include <linux/serial.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/stm/pio.h>
#include <linux/generic_serial.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/clk.h>
#include <linux/stm/stserial.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <asm/irq-ilc.h>

#include "fdma_firmware_7105.h"

#define ENABLE    ((0x0000 + (0x002<<2)) >> 2)
#define CLK_GATE  ((0x0000 + (0x003<<2)) >> 2)
#define PC        (0x0020 >> 2)
#define REGS(x)   ((0x4000 + (x*4)) >> 2)
#define SYNC      ((0x8000 + (0xfe2<<2)) >> 2)
#define IMEM(x)   ((0xc000 >> 2)+ (x))
#define DMEM(x)   ((0x8000 >> 2)+ (x))

#define DREQ_MASK ((0x8000 + (0xff9 << 2)) >> 2)
#define DREQ_STAT ((0x8000 + (0xff8 << 2)) >> 2)

#define IFACE(x)  regs[(x)>>2]

#define IRQ_CLR   ((0x8000 + (0xff6 << 2)) >> 2)
#define IRQ_MASK  ((0x8000 + (0xff7 << 2)) >> 2)

#define NR_BUFFERS  512 //1024
#define NR_WBUFFERS 512

struct buffer_data {
	short int sta;
	short int rxdata;
};

static volatile unsigned int *regs;
static irqreturn_t (*asc_interrupt)(int irq, void *ptr, struct buffer_data *bd, int nr_bd) = NULL;
static void *asc_ptr = NULL;

static irqreturn_t fdma_irq(int irq, void *ptr)
{
	unsigned long buffer_start = (unsigned long)&IFACE(FDMA_RBUFFER);

	/* Our pointer */
	unsigned long buffer_read = (unsigned long)readl(&IFACE(FDMA_RBUFFER_READ));
	/* FDMA's pointer */
	unsigned long buffer_write = (unsigned long)readl(&IFACE(FDMA_RBUFFER_WRITE));

	struct buffer_data* bd = (struct buffer_data*)(buffer_start + (buffer_read * 4));

	/*
	printk("%s: s:0x%lx r:0x%lx w:0x%lx bd:0x%lx\n",
	       __FUNCTION__,buffer_start,buffer_read,buffer_write,bd);
	*/

	if (!asc_interrupt) {
		printk("no interrupt installed\n");
		writel(0, &IFACE(FDMA_STATE));
		writel(0x0, &IFACE(FDMA_INTOR));
		writel(0xffffffff, &regs[IRQ_CLR]);
		return IRQ_HANDLED;
	}

	if (buffer_write < 0) {
		printk("%s: Buffer overrun (FIFO full)\n",__FUNCTION__);
		writel(0x0, &IFACE(FDMA_RBUFFER_READ));
		writel(0x0, &IFACE(FDMA_RBUFFER_WRITE));
		goto handle_irq;
	}
		

	if (buffer_read == buffer_write)
		asc_interrupt(irq, asc_ptr, NULL, 0);
	else {
		if (buffer_write > buffer_read) {
			asc_interrupt(irq, asc_ptr, bd, (buffer_write-buffer_read));
		} else {
			/* So first do read -> end */
			asc_interrupt(irq, asc_ptr, bd, (NR_BUFFERS - buffer_read));

			/* Then do top->(write - 1) */
			if (buffer_write != buffer_start) {
				bd = (struct buffer_data*)(buffer_start);
				asc_interrupt(irq, asc_ptr, bd, (buffer_write));
			}
		}

//		printk("Update read == %x\n",virt_to_phys(buffer_write));
		writel(buffer_write, &IFACE(FDMA_RBUFFER_READ));
	}

handle_irq:
	writel(0xffffffff, &regs[IRQ_CLR]);
	
	return IRQ_HANDLED;
}

int fdma_hw_txroom(void)
{
	unsigned long wbuffer_write = (unsigned long)readl(&IFACE(FDMA_WBUFFER_WRITE));
	unsigned long wbuffer_read  = (unsigned long)readl(&IFACE(FDMA_WBUFFER_READ));

	//printk("%s: w:0x%lx r:0x%lx\n",__FUNCTION__,wbuffer_write,wbuffer_read);

	/* If we are empty */
	if (wbuffer_write == wbuffer_read)
		return NR_WBUFFERS - 1;

	if (wbuffer_write > wbuffer_read)
		return (NR_WBUFFERS - wbuffer_write) + (wbuffer_read - 1);

	return (wbuffer_read - wbuffer_write) - 1;
}

int fdma_write_fifo(unsigned char c)
{
	unsigned long wbuffer_write = (unsigned long)readl(&IFACE(FDMA_WBUFFER_WRITE));
	unsigned long wbuffer_read  = (unsigned long)readl(&IFACE(FDMA_WBUFFER_READ));
	unsigned int *wbuffer_base  = (unsigned int*)&IFACE(FDMA_WBUFFER);

	//printk("%s: w:0x%lx r:0x%lx b:0x%p c=%x\n",__FUNCTION__,wbuffer_write,wbuffer_read,wbuffer_base,c);

	/* Check for overflow conditions */
	if ((wbuffer_write + 1) % NR_WBUFFERS == wbuffer_read) {
		printk(KERN_ERR "%s: ERROR buffer would overflow if I wrote this byte\n",__FUNCTION__);
		return -1;
	}

	writel(c, &wbuffer_base[wbuffer_write]);
	wbuffer_write = (wbuffer_write + 1) % NR_WBUFFERS;
	writel(wbuffer_write, &IFACE(FDMA_WBUFFER_WRITE));
	
	return 0;
}

void fdma_disable_tx_when_empty(void)
{
	writel(0x4, &IFACE(FDMA_INTOR));
}

int is_fdma_tx_disabled(void)
{
	return readl(&IFACE(FDMA_INTOR)) == 0;
}

static int fdma_init(void)
{
	int n;	

	if (!(regs = ioremap(0xfe410000, 0x10000))) {
		printk("%s: Couldn't allocate registers\n",__FUNCTION__);
		goto no_iomem;
	}
	
	if (request_irq( evt2irq(0x13a0)/*ILC_IRQ(45)*/, fdma_irq, IRQF_DISABLED, "stasc-sc-fdma", (void*)regs)) {
		printk("%s: Couldn't get irq line\n",__FUNCTION__);
		goto no_irq;
	}

	/* Let's first reset the FDMA */
	writel(1, &regs[SYNC]);
	writel(0, &regs[ENABLE]);
	writel(5, &regs[CLK_GATE]);
	writel(0, &regs[CLK_GATE]);

	/* Let's load the FDMA firmware */
	for (n=0;n<sizeof(dmem)/4;n++)
		regs[DMEM(n)] = dmem[n];

	for (n=0;n<sizeof(imem)/4;n++)
		regs[IMEM(n)] = imem[n];

	/* Clear and Enable all interrupts */
	writel(0xffffffff, &regs[IRQ_CLR]);
	writel(0xffffffff, &regs[IRQ_MASK]);
	
	/* Allow async reads / writes */
	writel(0x1, &regs[SYNC]);

	/* Enable the DREQ mask */
	writel(0xf, &regs[DREQ_MASK]);

	/* Release the hounds Smithers */
	writel(0x1, &regs[ENABLE]);

	return 0;

no_irq:
	iounmap((void*)regs);
no_iomem:
	return -ENODEV;
}

struct uart_port;

void fdma_irq_set(struct uart_port *port, void *func)
{
	asc_ptr = port;
	asc_interrupt = func;
	writel(0x0, &IFACE(FDMA_INTOR));
	writel(0x1, &IFACE(FDMA_STATE));
}

void fdma_free_irq(int irq, struct uart_port *port)
{
	writel(0x0, &IFACE(FDMA_STATE));
	writel(0x0, &IFACE(FDMA_INTOR));
	asc_interrupt = NULL;
	asc_ptr = NULL;
}

static void fdma_exit(void)
{	
	free_irq(evt2irq(0x13a0), (void*)regs);
	iounmap((void*)regs);
}

module_init(fdma_init);
module_exit(fdma_exit);


#if defined(CONFIG_PROC_FS)

#ifdef FDMA_DEBUGIT
#define LINES 15+16+1
#else
#define LINES 15+1
#endif

static void *fdma_seq_start(struct seq_file *s, loff_t *pos)
{
	seq_printf(s, "FDMA Register Status\n");

	if (*pos >= LINES)
		return NULL;

	return pos;
}

static void fdma_seq_stop(struct seq_file *s, void *v)
{
}

static void *fdma_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	if (++(*pos) >= LINES)
		return NULL;

	return pos;
}

static int fdma_seq_show(struct seq_file *s, void *v)
{
	int reg = *((loff_t*)v);

	if (reg == 0) seq_printf(s,"   PC: 0x%x\n", regs[PC]*4);
	else if (reg <= 15) seq_printf(s,"Reg%02d: 0x%x\n", reg,regs[REGS(reg)]);
#ifdef FDMA_DEBUGIT
	else if (reg <= (15+15)) seq_printf(s,"debugit%d: 0x%x -> %d\n", reg-16,IFACE(FDMA_DEBUGIT+(reg-16)*4),IFACE(FDMA_DEBUGIT+(reg-16)*4));
#endif
	else seq_printf(s,"state:%x sta_mask:%x intor:%x rbs:%x br:%x bw:%x wbr:%x wbw:%x\n",IFACE(FDMA_STATE),IFACE(FDMA_STA_MASK),IFACE(FDMA_INTOR),IFACE(FDMA_RBUFFER),IFACE(FDMA_RBUFFER_READ),IFACE(FDMA_RBUFFER_WRITE),IFACE(FDMA_WBUFFER_READ),IFACE(FDMA_WBUFFER_WRITE));

	return 0;
}

static struct seq_operations fdma_seq_ops = {
	.start = fdma_seq_start,
	.next = fdma_seq_next,
	.stop = fdma_seq_stop,
	.show = fdma_seq_show,
};

static int fdma_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fdma_seq_ops);
}

static struct file_operations fdma_proc_ops = {
	.owner = THIS_MODULE,
	.open = fdma_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* Called from late in the kernel initialisation sequence, once the
 * normal memory allocator is available. */
static int __init fdma_proc_init(void)
{
	struct proc_dir_entry *entry = create_proc_entry("fdma", S_IRUGO, NULL);

	if (entry)
		entry->proc_fops = &fdma_proc_ops;

	return 0;
}
__initcall(fdma_proc_init);
#endif
