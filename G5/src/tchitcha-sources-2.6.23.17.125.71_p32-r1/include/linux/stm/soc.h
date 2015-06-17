#ifndef __LINUX_STM_SOC_H
#define __LINUX_STM_SOC_H

#include <linux/lirc.h>
#include <linux/compiler.h>

/* Private data for LPC device driver */
struct plat_lpc_data {
	unsigned int no_hw_req		:1;	/* iomem in sys/serv 5197 */
	unsigned int need_wdt_reset	:1;	/* W/A on 7141 */
	unsigned int irq_edge_level;
	char *clk_id;
};

/* Private platform data for the stpio10 driver */
struct stpio10_data {
	int start_pio;
	int num_pio;
};

/* This is the private platform data for the ssc driver */
struct ssc_pio_t {
	struct {
		unsigned char pio_port;
		unsigned char pio_pin;
	} pio[3]; /* clk, out, in */
	int clk_unidir;
	struct stpio_pin* clk;
	struct stpio_pin* sdout;
	struct stpio_pin* sdin;
	/* chip-select for SPI bus (struct spi_device *spi) -> (void *)*/
	void (*chipselect)(void *spi, int is_on);
};

#define SSC_I2C_CAPABILITY  0x00
#define SSC_SPI_CAPABILITY  0x01
#define SSC_I2C_PIO         0x02	/* spi/i2c in sw pio mode */
#define SSC_UNCONFIGURED    0x04
#define SSC_I2C_CLK_UNIDIR  0x08

#define SSC_BITS_SIZE       0x04
/*
 *   This macro could be used to build the capability field
 *   of struct plat_ssc_data for each SoC
 */
#define ssc_capability(idx_ssc, cap)  \
	(((cap) & \
	 (SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY | SSC_I2C_PIO | \
	  SSC_UNCONFIGURED | SSC_I2C_CLK_UNIDIR)) \
	  << ((idx_ssc)*SSC_BITS_SIZE))

#define ssc0_has(cap)  ssc_capability(0,cap)
#define ssc1_has(cap)  ssc_capability(1,cap)
#define ssc2_has(cap)  ssc_capability(2,cap)
#define ssc3_has(cap)  ssc_capability(3,cap)
#define ssc4_has(cap)  ssc_capability(4,cap)
#define ssc5_has(cap)  ssc_capability(5,cap)
#define ssc6_has(cap)  ssc_capability(6,cap)
#define ssc7_has(cap)  ssc_capability(7,cap)
#define ssc8_has(cap)  ssc_capability(8,cap)
#define ssc9_has(cap)  ssc_capability(9,cap)

/* Set pio[x].pio_port to SSC_NO_PIO for hard wired SSC's */
#define SSC_NO_PIO	0xff

struct plat_ssc_data {
	unsigned long		capability;	/* SSC bitmap capability */
	unsigned long		routing;
	/* chip-select for SPI bus (struct spi_device *spi) -> (void *)*/
	void (*spi_chipselects[])(void *spi, int is_on);
};

#ifdef CONFIG_CPU_SUBTYPE_STX5197
/* SSC0 routine depends on whether port configured for SPI or I2C */

#define SSC1_QAM_SCLT_SDAT	(0<<1)
#define SSC1_QPSK		(1<<1)
#endif

#ifdef CONFIG_CPU_SUBTYPE_STX7105

#define STX7105_SSC_SHIFT(ssc, pin)	(((ssc) * 6) + ((pin) * 2))

#define SSC2_SCLK_PIO2_4	(0 << STX7105_SSC_SHIFT(2, 0)) /* 7106 only! */
#define SSC2_SCLK_PIO3_4	(1 << STX7105_SSC_SHIFT(2, 0))
#define SSC2_SCLK_PIO12_0	(2 << STX7105_SSC_SHIFT(2, 0))
#define SSC2_SCLK_PIO13_4	(3 << STX7105_SSC_SHIFT(2, 0))

#define SSC2_MTSR_PIO2_0	(0 << STX7105_SSC_SHIFT(2, 1))
#define SSC2_MTSR_PIO3_5	(1 << STX7105_SSC_SHIFT(2, 1))
#define SSC2_MTSR_PIO12_1	(2 << STX7105_SSC_SHIFT(2, 1))
#define SSC2_MTSR_PIO13_5	(3 << STX7105_SSC_SHIFT(2, 1))

#define SSC2_MRST_PIO2_0	(0 << STX7105_SSC_SHIFT(2, 2))
#define SSC2_MRST_PIO3_5	(1 << STX7105_SSC_SHIFT(2, 2))
#define SSC2_MRST_PIO12_1	(2 << STX7105_SSC_SHIFT(2, 2))
#define SSC2_MRST_PIO13_5	(3 << STX7105_SSC_SHIFT(2, 2))

#define SSC3_SCLK_PIO2_7	(0 << STX7105_SSC_SHIFT(3, 0)) /* 7106 only! */
#define SSC3_SCLK_PIO3_6	(1 << STX7105_SSC_SHIFT(3, 0))
#define SSC3_SCLK_PIO13_2	(2 << STX7105_SSC_SHIFT(3, 0))
#define SSC3_SCLK_PIO13_6	(3 << STX7105_SSC_SHIFT(3, 0))

#define SSC3_MTSR_PIO2_1	(0 << STX7105_SSC_SHIFT(3, 1))
#define SSC3_MTSR_PIO3_7	(1 << STX7105_SSC_SHIFT(3, 1))
#define SSC3_MTSR_PIO13_3	(2 << STX7105_SSC_SHIFT(3, 1))
#define SSC3_MTSR_PIO13_7	(3 << STX7105_SSC_SHIFT(3, 1))

#define SSC3_MRST_PIO2_1	(0 << STX7105_SSC_SHIFT(3, 2))
#define SSC3_MRST_PIO3_7	(1 << STX7105_SSC_SHIFT(3, 2))
#define SSC3_MRST_PIO13_3	(2 << STX7105_SSC_SHIFT(3, 2))
#define SSC3_MRST_PIO13_7	(3 << STX7105_SSC_SHIFT(3, 2))

#endif


#define SPI_LINE_SHIFT		0x0
#define SPI_LINE_MASK		0x7
#define SPI_BANK_SHIFT		0x3
#define SPI_BANK_MASK		0xff
#define spi_get_bank(address)  (((address) >> SPI_BANK_SHIFT) & SPI_BANK_MASK)
#define spi_get_line(address)  (((address) >> SPI_LINE_SHIFT) & SPI_LINE_MASK)
#define spi_set_cs(bank, line) ((((bank) & SPI_BANK_MASK) << SPI_BANK_SHIFT) | \
				 (((line) & SPI_LINE_MASK) << SPI_LINE_SHIFT))
/* each spi bus is able to manage 'all' the pios as chip selector
   therefore each master must have 8(pioline)x20(piobank)
   30 pio banks is enough for our boards
   SPI_NO_CHIPSELECT to specify SPI device with no CS (ie CS tied to 'active')
*/
#define SPI_NO_CHIPSELECT	(spi_set_cs(29, 7) + 1)


#define PCI_PIN_ALTERNATIVE -3 	/* Use alternative PIO rather than default */
#define PCI_PIN_DEFAULT     -2 	/* Use whatever the default is for that pin */
#define PCI_PIN_UNUSED      -1	/* Pin not in use */

/* In the board setup, you can pass in the external interrupt numbers instead
 * if you have wired up your board that way. It has the advantage that the PIO
 * pins freed up can then be used for something else. */
struct pci_config_data {
	/* PCI_PIN_DEFAULT/PCI_PIN_UNUSED. Other IRQ can be passed in */
	int pci_irq[4];
	/* As above for SERR */
	int serr_irq;
	/* Lowest address line connected to an idsel  - slot 0 */
	char idsel_lo;
	/* Highest address line connected to an idsel - slot n */
	char idsel_hi;
	/* Set to PCI_PIN_DEFAULT if the corresponding req/gnt lines are
	 * in use */
	char req_gnt[4];
	/* PCI clock in Hz. If zero default to 33MHz */
	unsigned long pci_clk;

	/* If you supply a pci_reset() function, that will be used to reset
	 * the PCI bus.  Otherwise it is assumed that the reset is done via
	 * PIO, the number is specified here. Specify -EINVAL if no PIO reset
	 * is required either, for example if the PCI reset is done as part
	 * of power on reset. */
	unsigned pci_reset_gpio;
	void (*pci_reset)(void);

	/* You may define a PCI clock name. If NULL it will fall
	 * back to "pci" */
	const char *clk_name;

	/* Various PCI tuning parameters. Set by SOC layer. You don't have
	 * to specify these as the defaults are usually fine. However, if
	 * you need to change them, you can set ad_override_default and
	 * plug in your own values. */
	unsigned ad_threshold:4;
	unsigned ad_chunks_in_msg:5;
	unsigned ad_pcks_in_chunk:5;
	unsigned ad_trigger_mode:1;
	unsigned ad_posted:1;
	unsigned ad_max_opcode:4;
	unsigned ad_read_ahead:1;
	/* Set to override default values for your board */
	unsigned ad_override_default:1;

	/* Some SOCs have req0 pin connected to req3 signal to work around
	 * some problems with NAND. Also the PCI_NOT_EMI bit should NOT be
	 * set sometimes. These bits will be set by the chip layer, the
	 * board layer should NOT touch this. */
	unsigned req0_to_req3:1;
	unsigned req0_emi:1;
};

u8 pci_synopsys_inb(unsigned long port);
u16 pci_synopsys_inw(unsigned long port);
u32 pci_synopsys_inl(unsigned long port);

u8 pci_synopsys_inb_p(unsigned long port);
u16 pci_synopsys_inw_p(unsigned long port);
u32 pci_synopsys_inl_p(unsigned long port);

void pci_synopsys_insb(unsigned long port, void *dst, unsigned long count);
void pci_synopsys_insw(unsigned long port, void *dst, unsigned long count);
void pci_synopsys_insl(unsigned long port, void *dst, unsigned long count);

void pci_synopsys_outb(u8 val, unsigned long port);
void pci_synopsys_outw(u16 val, unsigned long port);
void pci_synopsys_outl(u32 val, unsigned long port);

void pci_synopsys_outb_p(u8 val, unsigned long port);
void pci_synopsys_outw_p(u16 val, unsigned long port);
void pci_synopsys_outl_p(u32 val, unsigned long port);

void pci_synopsys_outsb(unsigned long port, const void *src, unsigned long count);
void pci_synopsys_outsw(unsigned long port, const void *src, unsigned long count);
void pci_synopsys_outsl(unsigned long port, const void *src, unsigned long count);

/* Macro used to fill in the IO machine vector at the board level */

/* We have to hook all the in/out functions as they cannot be memory
 * mapped with the synopsys PCI IP
 *
 * Also, for PCI we use the generic iomap implementation, and so do
 * not need the ioport_map function, instead using the generic cookie
 * based implementation.
 */
#ifdef CONFIG_SH_ST_SYNOPSYS_PCI
#define STM_PCI_IO_MACHINE_VEC			\
	.mv_inb = pci_synopsys_inb,		\
        .mv_inw = pci_synopsys_inw,		\
        .mv_inl = pci_synopsys_inl,		\
        .mv_outb = pci_synopsys_outb,		\
        .mv_outw = pci_synopsys_outw,		\
        .mv_outl = pci_synopsys_outl,		\
        .mv_inb_p = pci_synopsys_inb_p,		\
        .mv_inw_p = pci_synopsys_inw,		\
        .mv_inl_p = pci_synopsys_inl,		\
        .mv_outb_p = pci_synopsys_outb_p,	\
        .mv_outw_p = pci_synopsys_outw,		\
        .mv_outl_p = pci_synopsys_outl,		\
        .mv_insb = pci_synopsys_insb,		\
        .mv_insw = pci_synopsys_insw,		\
        .mv_insl = pci_synopsys_insl,		\
        .mv_outsb = pci_synopsys_outsb,		\
        .mv_outsw = pci_synopsys_outsw,		\
        .mv_outsl = pci_synopsys_outsl,
#else
#define STM_PCI_IO_MACHINE_VEC
#endif

/* Private data for the SATA driver */
struct plat_sata_data {
	unsigned long phy_init;
	unsigned long pc_glue_logic_init;
	unsigned int only_32bit;
};



/* Private data for the MIPHY driver */

struct stm_miphy {
	int ports_num;
	int (*jtag_tick)(int tms, int tdi, void *priv);
	void *jtag_priv;
};

struct stm_miphy_sysconf_soft_jtag {
	struct sysconf_field *tms;
	struct sysconf_field *tck;
	struct sysconf_field *tdi;
	struct sysconf_field *tdo;
};

#ifdef CONFIG_STM_MIPHY

void stm_miphy_init(struct stm_miphy *miphy, int port);
int stm_miphy_sysconf_jtag_tick(int tms, int tdi, void *priv);

#else

static inline void stm_miphy_init(struct stm_miphy *miphy, int port)
{
}

static inline int stm_miphy_sysconf_jtag_tick(int tms, int tdi, void *priv)
{
	return 0;
}

#endif



/* Private data for the PWM driver */
struct plat_stm_pwm_data {
	unsigned long flags;
	unsigned long routing;
};

#define PLAT_STM_PWM_OUT0	(1<<0)
#define PLAT_STM_PWM_OUT1	(1<<1)
#define PLAT_STM_PWM_OUT2	(1<<2)
#define PLAT_STM_PWM_OUT3	(1<<3)

#ifdef CONFIG_CPU_SUBTYPE_STX7105
#define PWM_OUT0_PIO4_4		(0 << 0)
#define PWM_OUT0_PIO13_0	(1 << 0)
#define PWM_OUT1_PIO4_5		(0 << 1)
#define PWM_OUT1_PIO13_1	(1 << 1)
#endif

/* Platform data for the temperature sensor driver */
struct plat_stm_temp_data {
	const char *name;
	struct {
		int group, num, lsb, msb;
	} pdn, dcorrect, overflow, data;
	int calibrated:1;
	int calibration_value;
	void (*custom_set_dcorrect)(void *priv);
	unsigned long (*custom_get_data)(void *priv);
	void *custom_priv;
};

/* This is the private platform data for the lirc driver */
#define LIRC_PIO_ON		0x08	/* PIO pin available */
#define LIRC_IR_RX		0x04	/* IR RX PIO line available */
#define LIRC_IR_TX		0x02	/* IR TX PIOs lines available */
#define LIRC_UHF_RX		0x01	/* UHF RX PIO line available */

struct lirc_pio {
	unsigned int bank;
	unsigned int pin;
	unsigned int dir;
	char pinof;
        struct stpio_pin* pinaddr;
};

struct plat_lirc_data {
	unsigned int irbclock;		/* IRB block clock (set to 0 for auto) */
	unsigned int irbclkdiv;		/* IRB block clock divisor (set to 0 for auto) */
	unsigned int irbperiodmult;	/* manual setting period multiplier */
	unsigned int irbperioddiv;	/* manual setting period divisor */
	unsigned int irbontimemult;	/* manual setting pulse period multiplier */
	unsigned int irbontimediv;	/* manual setting pulse period divisor */
	unsigned int irbrxmaxperiod;	/* maximum rx period in uS */
	unsigned int irbversion;	/* IRB version type (1,2 or 3) */
	unsigned int sysclkdiv;		/* factor to divide system bus clock by */
	unsigned int rxpolarity;        /* flag to set gpio rx polarity (usually set to 1) */
	unsigned int subcarrwidth;      /* Subcarrier width in percent - this is used to */
					/* make the subcarrier waveform square after passing */
					/* through the 555-based threshold detector on ST boards */
	struct lirc_pio *pio_pin_arr;	/* PIO pin settings for driver */
	unsigned int num_pio_pins;
	lirc_scd_t *scd_info;		/* SCD settings */
#ifdef CONFIG_PM
	unsigned long clk_on_low_power; /* specify the system clock rate in lowpower mode */
	unsigned int maxperiod_on_low_power;	/* maximum rx period in uS in lowpower mode */
#endif
};

/* Private data for the STM on-board ethernet driver */
struct plat_stmmacenet_data {
	int bus_id;
	int pbl;
	int has_gmac;
	void (*fix_mac_speed)(void *priv, unsigned int speed);
	void (*hw_setup)(void);

	void *bsp_priv;
};

#define STMMAC_PACK_BUS_ID(bus, phy)	((1<<31) | (bus << 8) | (phy))
#define STMMAC_PACKED_BUS_ID(x)		((x) & (1<<31))
#define STMMAC_UNPACK_BUS_ID_BUS(x)	(((x) >> 8) & 0xff)
#define STMMAC_UNPACK_BUS_ID_PHY(x)	((x) & 0xff)

struct plat_stmmacphy_data {
	int bus_id;
	int phy_addr;
	unsigned int phy_mask;
	int interface;
	int (*phy_reset)(void *priv);
	int (*get_mac_addr)(u8 *addr);
	void *priv;
};

struct plat_usb_data {
	unsigned long flags;
};

#define USB_FLAGS_STRAP_8BIT				(1<<0)
#define USB_FLAGS_STRAP_16BIT				(2<<0)

#define USB_FLAGS_STRAP_PLL				(1<<2)

#define USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE			(1<<3)

#define USB_FLAGS_STBUS_CONFIG_THRESHOLD_MASK		(3<<4)
#define USB_FLAGS_STBUS_CONFIG_THRESHOLD_64		(1<<4)
#define USB_FLAGS_STBUS_CONFIG_THRESHOLD_128		(2<<4)
#define USB_FLAGS_STBUS_CONFIG_THRESHOLD_256		(3<<4)

#define USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_MASK	(7<<6)
#define USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_1		(1<<6)
#define USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_2		(2<<6)
#define USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8		(4<<6)

#define USB_FLAGS_STBUS_CONFIG_OPCODE_MASK		(3<<8)
#define USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32		(1<<8)
#define USB_FLAGS_STBUS_CONFIG_OPCODE_LD64_ST64		(2<<8)


/**
 * struct usb_init_data - initialisation data for a USB port
 * @oc_en: enable OC detection (0 or 1)
 * @oc_actlow: whether OC detection is active low (0 or 1)
 * @oc_pinsel: pin to be used for OC detection (defined below)
 * @pwr_en: enable power enable (0 or 1)
 * @pwr_pinsel: pin to be used for power enable (defined below)
 */
struct usb_init_data {
	char oc_en;
	char oc_actlow;
	int oc_pinsel;
	char pwr_en;
	int pwr_pinsel;
};

#ifdef CONFIG_CPU_SUBTYPE_STX7105
#define USB0_OC_PIO4_4   0
#define USB0_OC_PIO12_5  1
#define USB0_OC_PIO14_4  2 /* 7106 only! */
#define USB0_PWR_PIO4_5  0
#define USB0_PWR_PIO12_6 1
#define USB0_PWR_PIO14_5 2 /* 7106 only! */

#define USB1_OC_PIO4_6   0
#define USB1_OC_PIO14_6  1
#define USB1_PWR_PIO4_7  0
#define USB1_PWR_PIO14_7 1
#endif



/*** FDMA platform data ***/

struct stm_plat_fdma_slim_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
};

struct stm_plat_fdma_periph_regs {
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

struct stm_plat_fdma_hw {
	struct stm_plat_fdma_slim_regs slim_regs;
	struct stm_plat_fdma_periph_regs periph_regs;
	unsigned long dmem_offset;
	unsigned long dmem_size;
	unsigned long imem_offset;
	unsigned long imem_size;
};

struct stm_plat_fdma_fw_regs {
	unsigned long rev_id;
	unsigned long cmd_statn;
	unsigned long req_ctln;
	unsigned long ptrn;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
};

struct stm_plat_fdma_fw {
	const char *name;
	struct stm_plat_fdma_fw_regs fw_regs;
	void *dmem;
	unsigned long dmem_len;
	void *imem;
	unsigned long imem_len;
};

struct stm_plat_fdma_data {
	struct stm_plat_fdma_hw *hw;
	struct stm_plat_fdma_fw *fw;
	int min_ch_num;
	int max_ch_num;
};



struct stasc_uart_data {
	struct {
		unsigned char pio_port;
		unsigned char pio_pin;
		unsigned char pio_direction;
	} pios[6]; /* TXD, RXD, CTS, RTS, DTR, RI,Irq */
	unsigned char flags;
};

#define STASC_FLAG_NORTSCTS	(1 << 0)
#define STASC_FLAG_TXFIFO_BUG	(1 << 1)
/*#define STASC_FLAG_DTRRTSRI    (1 << 2)*/
#define STASC_FLAG_SCSUPPORT    (1 << 3)
#define STASC_FLAG_SC_RST_INVERTED    (1 << 4)

#ifdef CONFIG_CPU_SUBTYPE_STX7105
#define STASC_FLAG_ASC2_PIO4	(0 << 2)
#define STASC_FLAG_ASC2_PIO12	(1 << 2)
#endif

extern int stasc_console_device;
extern struct platform_device *stasc_configured_devices[];
extern unsigned int stasc_configured_devices_count;

#ifdef CONFIG_CPU_SUBTYPE_STX7141
#define ASC1_MCARD		0
#define ASC1_PIO10		2

#define ASC2_PIO1		0
#define ASC2_PIO6		4
#endif

#define PLAT_SYSCONF_GROUP(_id, _offset) \
	{ \
		.group = _id, \
		.offset = _offset, \
		.name = #_id \
	}

struct plat_sysconf_group {
	int group;
	unsigned long offset;
	const char *name;
	const char *(*field_name)(int num);
};

struct plat_sysconf_data {
	int groups_num;
	struct plat_sysconf_group *groups;
};



/* STM NAND configuration data (plat_nand/STM_NAND_EMI/FLEX/AFM) */
struct plat_stmnand_data {
	/* plat_nand/STM_NAND_EMI paramters */
	unsigned int	emi_withinbankoffset;  /* Offset within EMI Bank      */
	int		rbn_port;		/*  # : 'nand_RBn' PIO port   */
						/* -1 : if unconnected	      */
	int		rbn_pin;	        /*      'nand_RBn' PIO pin    */
						/* (assumes shared RBn signal */
						/*  for multiple chips)	      */

	/* STM_NAND_EMI/FLEX/AFM paramters */
	void		*timing_data;		/* Timings for EMI/NandC      */
	unsigned char	flex_rbn_connected;	/* RBn signal connected?      */
						/* (Required for NAND_AFM)    */

	/* Legacy data for backwards compatibility with plat_nand driver      */
	/*   will be removed once all platforms updated to use STM_NAND_EMI!  */
	unsigned int	emi_bank;		/* EMI bank                   */
	void		*emi_timing_data;	/* Timing data for EMI config */
	void		*mtd_parts;		/* MTD partition table	      */
	int		nr_parts;		/* Numer of partitions	      */
	unsigned int	chip_delay;		/* Read-busy delay	      */

};

/* STM SPI FSM configuration data */
struct stm_plat_spifsm_data {
	char			*name;
	struct mtd_partition	*parts;
	unsigned int		nr_parts;
	unsigned int		max_freq;
};

void fli7510_early_device_init(void);
void fli7510_configure_asc(const int *ascs, int num_ascs, int console);
void fli7510_configure_pwm(struct plat_stm_pwm_data *data);
void fli7510_configure_ssc(struct plat_ssc_data *data);
enum fli7510_usb_ovrcur_mode {
	fli7510_usb_ovrcur_disabled,
	fli7510_usb_ovrcur_active_high,
	fli7510_usb_ovrcur_active_low,
};
void fli7510_configure_usb(int port, enum fli7510_usb_ovrcur_mode ovrcur_mode);
void fli7510_configure_pata(int bank, int pc_mode, int irq);
enum fli7510_ethernet_mode {
	fli7510_ethernet_mii,
	fli7510_ethernet_gmii,
	fli7510_ethernet_rmii,
	fli7510_ethernet_reverse_mii
};
void fli7510_configure_ethernet(enum fli7510_ethernet_mode mode,
		int ext_clk, int phy_bus);
void fli7510_configure_lirc(lirc_scd_t *scd);
void fli7510_configure_pci(struct pci_config_data *pci_conf);
int fli7510_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin);
void fli7510_configure_nand(struct platform_device *pdev);
struct fli7510_audio_config {
	enum {
		fli7510_audio_i2sa_output_disabled,
		fli7510_audio_i2sa_output_2_channels,
		fli7510_audio_i2sa_output_4_channels,
		fli7510_audio_i2sa_output_6_channels,
		fli7510_audio_i2sa_output_8_channels,
	} i2sa_output_mode;
	int i2sc_output_enabled;
	int spdif_output_enabled;
};
void fli7510_configure_audio(struct fli7510_audio_config *config);

void stx5197_early_device_init(void);
void stx5197_configure_asc(const int *ascs, int num_ascs, int console);
void stx5197_configure_usb(void);
void stx5197_configure_ethernet(int rmii, int ext_clk, int phy_bus);
void stx5197_configure_ssc(struct plat_ssc_data *data);
void stx5197_configure_pwm(struct plat_stm_pwm_data *data);
void stx5197_configure_lirc(lirc_scd_t *scd);

void stx5206_early_device_init(void);
void stx5206_configure_asc(const int *ascs, int num_ascs, int console);
enum stx5206_ethernet_mode {
	stx5206_ethernet_mii,
	stx5206_ethernet_rmii,
	stx5206_ethernet_reverse_mii
};
void stx5206_configure_ethernet(enum stx5206_ethernet_mode mode, int ext_clk,
		int phy_bus);
enum stx5206_lirc_rx_mode {
	stx5206_lirc_rx_disabled,
	stx5206_lirc_rx_ir,
	stx5206_lirc_rx_uhf
};
void stx5206_configure_lirc(enum stx5206_lirc_rx_mode rx_mode, int tx_enabled,
		int tx_od_enabled, lirc_scd_t *scd);
void stx5206_configure_pata(int bank, int pc_mode, int irq);
void stx5206_configure_pwm(struct plat_stm_pwm_data *data);
void stx5206_configure_ssc(struct plat_ssc_data *data);
void stx5206_configure_usb(void);
void stx5206_configure_pci(struct pci_config_data *pci_conf);
int stx5206_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin);
void stx5206_configure_nand(struct platform_device *pdev);
void stx5206_configure_spifsm(struct stm_plat_spifsm_data *spifsm_data);

void stx7100_early_device_init(void);
void stb7100_configure_asc(const int *ascs, int num_ascs, int console);
void sysconf_early_init(struct platform_device *pdevs, int pdevs_num);
void stpio_early_init(struct platform_device *pdev, int num_pdevs, int irq);

void stx7100_configure_sata(void);
void stx7100_configure_pwm(struct plat_stm_pwm_data *data);
void stx7100_configure_ssc(struct plat_ssc_data *data);
void stx7100_configure_usb(void);
void stx7100_configure_ethernet(int rmii_mode, int ext_clk, int phy_bus);
void stx7100_configure_lirc(lirc_scd_t *scd);
void stx7100_configure_pata(int bank, int pc_mode, int irq);

void stx7105_early_device_init(void);
void stx7105_configure_tsin(unsigned int port, unsigned char serial);
void stx7105_configure_asc(const int *ascs, int num_ascs, int console);
void stx7105_configure_pwm(struct plat_stm_pwm_data *data);
void stx7105_configure_ssc(struct plat_ssc_data *data);
void stx7105_configure_usb(int port, struct usb_init_data *data);
enum stx7105_ethernet_mode {
	stx7105_ethernet_mii,
	stx7105_ethernet_gmii,
	stx7105_ethernet_rmii,
	stx7105_ethernet_reverse_mii
};
#define STX7105_ETHERNET_MII1_MDIO_MASK	(1 << 0)
#define STX7105_ETHERNET_MII1_MDIO_11_0	(0 << 0)
#define STX7105_ETHERNET_MII1_MDIO_3_4	(1 << 0)
#define STX7105_ETHERNET_MII1_MDC_MASK	(1 << 1)
#define STX7105_ETHERNET_MII1_MDC_11_1	(0 << 1)
#define STX7105_ETHERNET_MII1_MDC_3_5	(1 << 1)
void stx7105_configure_ethernet(int port, enum stx7105_ethernet_mode mode,
		int ext_clk, int phy_bus,
		int mii0_mdint_workaround, int mii1_routing);
void stx7105_configure_nand(struct platform_device *pdev);
void stx7105_configure_lirc(lirc_scd_t *scd);
void stx7105_configure_sata(int port);
void stx7105_configure_pata(int bank, int pc_mode, int irq);
void stx7105_configure_audio_pins(int pcmout1, int pcmout2, int spdif,
		int pcmin);
void stx7105_configure_pci(struct pci_config_data *pci_config);
int stx7105_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin);
void stx7106_configure_spifsm(struct stm_plat_spifsm_data *spifsm_data);

void stx7108_early_device_init(void);
struct stx7108_asc_config {
	union {
		struct {
			enum {
				stx7108_asc3_txd_pio21_0,
				stx7108_asc3_txd_pio24_4,
			} txd;
			enum {
				stx7108_asc3_rxd_pio21_1,
				stx7108_asc3_rxd_pio24_5,
			} rxd;
			enum {
				stx7108_asc3_cts_pio21_4,
				stx7108_asc3_cts_pio25_0,
			} cts;
			enum {
				stx7108_asc3_rts_pio21_3,
				stx7108_asc3_rts_pio24_7,
			} rts;
		} asc3;
	} routing;
	int hw_flow_control:1;
	int is_console:1;
};
void stx7108_configure_asc(int asc, struct stx7108_asc_config *config);
struct stx7108_pwm_config {
	int out0_enabled:1;
	int out1_enabled:1;
};
void stx7108_configure_pwm(struct stx7108_pwm_config *config);
struct stx7108_ssc_config {
	union {
		struct {
			enum {
				stx7108_ssc2_sclk_pio1_3,
				stx7108_ssc2_sclk_pio14_4
			} sclk;
			enum {
				stx7108_ssc2_mtsr_pio1_4,
				stx7108_ssc2_mtsr_pio14_5
			} mtsr;
			enum {
				stx7108_ssc2_mrst_pio1_5,
				stx7108_ssc2_mrst_pio14_6
			} mrst;
		} ssc2;
	} routing;
	int clk_unidir:1;
	int i2c_pio:1;
	void (*spi_chipselect)(void *spi, int is_on);
};
int stx7108_configure_ssc_i2c(int ssc, struct stx7108_ssc_config *config);
int stx7108_configure_ssc_spi(int ssc, struct stx7108_ssc_config *config);
void stx7108_configure_usb(int port);
void stx7108_configure_sata(int port);
void stx7108_configure_pata(int bank, int pc_mode, int irq);
struct stx7108_ethernet_config {
	enum {
		stx7108_ethernet_mode_mii,
		stx7108_ethernet_mode_gmii,
		stx7108_ethernet_mode_gmii_gtx,
		stx7108_ethernet_mode_rmii,
		stx7108_ethernet_mode_reverse_mii
	} mode;
	int ext_clk;
	int phy_bus;
	void (*txclk_select)(int txclk_250_not_25_mhz);
};
void stx7108_configure_ethernet(int port,
		struct stx7108_ethernet_config *config);
struct stx7108_audio_config {
	enum {
		stx7108_audio_pcm_output_disabled,
		stx7108_audio_pcm_output_2_channels,
		stx7108_audio_pcm_output_4_channels,
		stx7108_audio_pcm_output_6_channels,
		stx7108_audio_pcm_output_8_channels,
	} pcm_output_mode;
	int pcm_input_enabled;
	int spdif_output_enabled;
};
void stx7108_configure_audio(struct stx7108_audio_config *config);
struct stx7108_lirc_config {
	enum {
		stx7108_lirc_rx_disabled,
		stx7108_lirc_rx_mode_ir,
		stx7108_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
	lirc_scd_t *scd;
};
void stx7108_configure_lirc(struct stx7108_lirc_config *config);
void stx7108_configure_nand(struct platform_device *pdev);
void stx7108_configure_pci(struct pci_config_data *pci_config);
int stx7108_pcibios_map_platform_irq(struct pci_config_data *pci_config,
		u8 pin);

void stx7111_early_device_init(void);
void stx7111_configure_asc(const int *ascs, int num_ascs, int console);
void stx7111_configure_pwm(struct plat_stm_pwm_data *data);
void stx7111_configure_ssc(struct plat_ssc_data *data);
void stx7111_configure_usb(int inv_enable);
void stx7111_configure_ethernet(int en_mii, int sel, int ext_clk, int phy_bus);
void stx7111_configure_nand(struct platform_device *pdev);
void stx7111_configure_lirc(lirc_scd_t *scd);
void stx7111_configure_pci(struct pci_config_data *pci_config);
int  stx7111_pcibios_map_platform_irq(struct pci_config_data *pci_config, u8 pin);

void stx7141_early_device_init(void);
void stx7141_configure_asc(const int *ascs, int num_ascs, int console);
void stx7141_configure_sata(void);
void stx7141_configure_pwm(struct plat_stm_pwm_data *data);
void stx7141_configure_ssc(struct plat_ssc_data *data);
void stx7141_configure_usb(int port);
void stx7141_configure_ethernet(int port, int reverse_mii, int mode,
				int phy_bus);
void stx7141_configure_audio_pins(int pcmout1, int pcmout2, int spdif,
		int pcmin1, int pcmint2);
struct stx7141_lirc_config {
	enum {
		stx7141_lirc_rx_disabled,
		stx7141_lirc_rx_mode_ir,
		stx7141_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
	lirc_scd_t *scd;
};
void stx7141_configure_lirc(struct stx7141_lirc_config *config);
void stx7141_configure_nand(struct plat_stmnand_data *dat);

void stx7200_early_device_init(void);
void stx7200_configure_asc(const int *ascs, int num_ascs, int console);
void stx7200_configure_pwm(struct plat_stm_pwm_data *data);
void stx7200_configure_ssc(struct plat_ssc_data *data);
void stx7200_configure_usb(int port);
void stx7200_configure_sata(unsigned int port);
void stx7200_configure_ethernet(int mac, int rmii_mode, int ext_clk,
				int phy_bus);
void stx7200_configure_lirc(lirc_scd_t *scd);
void stx7200_configure_nand(struct platform_device *pdev);
void stx7200_configure_pata(int bank, int pc_mode, int irq);

void stx7108_configure_mmc(void);
void stx7106_configure_mmc(void);
void stx5289_configure_mmc(void);
#endif /* __LINUX_ST_SOC_H */
