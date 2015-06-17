#define STEMI()						\
{							\
	.name = "emi",					\
	.id = -1,					\
}


#define STLPC_DEVICE(_base, _irq, _irq_level,		\
	_no_hw_req, _need_wdt_reset, _clk_id)		\
{							\
	.name = "stlpc",				\
	.id = -1,					\
	.num_resources  = 2,				\
	.dev = {					\
		.power.can_wakeup = 1,			\
		.platform_data = &(struct plat_lpc_data)\
		{					\
			.clk_id = _clk_id,		\
			.irq_edge_level = (_irq_level), \
			.no_hw_req = (_no_hw_req),	\
			.need_wdt_reset = (_need_wdt_reset),\
		},					\
	},						\
	.resource = (struct resource[]) {		\
		{					\
			.start  = _base,		\
			.end    = _base + 0xfff,	\
			.flags  = IORESOURCE_MEM	\
		}, {					\
			.start  = _irq,			\
			.end    = _irq,			\
			.flags  = IORESOURCE_IRQ	\
		}					\
	},						\
}

#define STLIRC_DEVICE(_mem_start, _irq, _wake_irq)	\
{							\
	.name = "lirc",					\
	.id = -1,					\
	.num_resources = 3,				\
	.resource = (struct resource[]) {		\
		{					\
			.start = _mem_start,		\
			.end = _mem_start + 0xa0,	\
			.flags = IORESOURCE_MEM,	\
		}, {					\
			.start = _irq,			\
			.flags = IORESOURCE_IRQ,	\
		}, {					\
			.start = _wake_irq,		\
			.flags = IORESOURCE_IRQ,	\
		},					\
	},						\
	.dev = {					\
		.power.can_wakeup = 1,			\
		.platform_data = &lirc_private_info	\
        },						\
}

#define STPIO_DEVICE(_id, _base, _irq)					\
{									\
	.name		= "stpio",					\
	.id		= _id,						\
	.num_resources	= 2,						\
	.dev.power.can_wakeup = ((_irq) != -1),				\
	.resource	= (struct resource[]) {				\
		{							\
			.start	= _base,				\
			.end	= _base + 0x100,			\
			.flags	= IORESOURCE_MEM			\
		}, {							\
			.start	= _irq,					\
			.flags	= IORESOURCE_IRQ			\
		}							\
	},								\
}

#define STPIO10_DEVICE(_base, _irq, _start_pio, _num_pio)		\
{									\
	.name		= "stpio10",					\
	.id		= -1,						\
	.num_resources	= 2,						\
	.resource	= (struct resource[]) {				\
		{							\
			.start	= _base,				\
			.end	= _base + 0xffff,			\
			.flags	= IORESOURCE_MEM			\
		}, {							\
			.start	= _irq,					\
			.flags	= IORESOURCE_IRQ			\
		}							\
	},								\
	.dev.platform_data = &(struct stpio10_data) {			\
		.start_pio	= _start_pio,				\
		.num_pio	= _num_pio,				\
	},								\
}

#define STASC_DEVICE(_base, _irq, _fdma_req_rx, _fdma_req_tx,		\
		_pio_port, _ptx, _prx, _pcts, _prts, _dptx, _dprx,	\
		_dpcts, _dprts)						\
{									\
	.name		= "stasc",					\
	.num_resources	= 2,						\
	.resource	= (struct resource[]) {				\
		{							\
			.start	= _base,				\
			.end	= _base + 0x100,			\
			.flags	= IORESOURCE_MEM			\
		}, {							\
			.start	= _irq,					\
			.end	= _irq,					\
			.flags	= IORESOURCE_IRQ			\
		}, {							\
			.start	= _fdma_req_rx,				\
			.end    = _fdma_req_rx,				\
			.flags	= IORESOURCE_DMA			\
		}, {							\
			.start	= _fdma_req_tx,				\
			.end    = _fdma_req_tx,				\
			.flags	= IORESOURCE_DMA			\
			}						\
	},								\
	.dev = {							\
		.platform_data = &(struct stasc_uart_data) {		\
			.pios = {					\
				[0] = {	/* TXD */			\
					.pio_port	= _pio_port,	\
					.pio_pin	= _ptx,		\
					.pio_direction	= _dptx,	\
				},					\
				[1] = { /* RXD */			\
					.pio_port	= _pio_port,	\
					.pio_pin	= _prx,		\
					.pio_direction	= _dprx,	\
				},					\
				[2] = {	/* CTS */			\
					.pio_port	= _pio_port,	\
					.pio_pin	= _pcts,	\
					.pio_direction	= _dpcts,	\
				},					\
				[3] = { /* RTS */			\
					.pio_port	= _pio_port,	\
					.pio_pin	= _prts,	\
					.pio_direction	= _dprts,	\
				},					\
			},						\
		}							\
	}								\
}

#define STSSC_DEVICE(_base, _irq, _pio_port, _pclk, _pdin, _pdout)	\
{									\
	.num_resources  = 2,						\
	.resource       = (struct resource[]) {				\
		{							\
			.start  = _base,				\
			.end    = _base + 0x10C,			\
			.flags  = IORESOURCE_MEM			\
		}, {							\
			.start  = _irq,					\
			.flags  = IORESOURCE_IRQ			\
		}							\
	},								\
	.dev = {							\
		.platform_data = &(struct ssc_pio_t ) {			\
			.pio = {					\
				{ _pio_port, _pclk },			\
				{ _pio_port, _pdin },			\
				{ _pio_port, _pdout }			\
			}						\
		}                                                       \
	}								\
}

#define USB_DEVICE(_port, _eh_base, _eh_irq, _oh_base, _oh_irq,		\
	_wrapper_base, _protocol_base, _flags)				\
{									\
	.name = "st-usb",						\
	.id = _port,							\
	.dev = {							\
		.dma_mask = &st40_dma_mask,				\
		.coherent_dma_mask = DMA_32BIT_MASK,			\
		.platform_data = &(struct plat_usb_data){		\
			.flags = _flags,				\
			},						\
	},								\
	.num_resources = 6,						\
	.resource = (struct resource[]) {				\
		{							\
			.name  = "ehci",				\
			.start = _eh_base,				\
			.end   = _eh_base + 0xff,			\
			.flags = IORESOURCE_MEM,			\
		}, {							\
			.name  = "ehci",				\
			.start = _eh_irq,				\
			.end   = _eh_irq,				\
			.flags = IORESOURCE_IRQ,			\
		}, {							\
			.name  = "ohci",				\
			.start = _oh_base,				\
			.end   = _oh_base + 0xff,			\
			.flags = IORESOURCE_MEM,			\
		}, {							\
			.name  = "ohci",				\
			.start = _oh_irq,				\
			.end   = _oh_irq,				\
			.flags = IORESOURCE_IRQ,			\
		}, {							\
			.name  = "wrapper",				\
			.start = _wrapper_base,				\
			.end   = _wrapper_base + 0xff,			\
			.flags = IORESOURCE_MEM,			\
		}, {							\
			.name = "protocol",				\
			.start = _protocol_base,			\
			.end   = _protocol_base + 0xff,			\
			.flags = IORESOURCE_MEM,			\
		},							\
	},								\
}

#define EMI_NAND_DEVICE(_id)							\
{										\
	.name		= "gen_nand",						\
	.id		= _id,							\
	.num_resources	= 1,							\
	.resource	= (struct resource[]) {					\
		{								\
			.flags		= IORESOURCE_MEM,			\
		}								\
	},									\
	.dev		= {							\
		.platform_data	= &(struct platform_nand_data) {		\
			.chip		=					\
			{							\
				.nr_chips		= 1,			\
				.options		= NAND_NO_AUTOINCR,	\
				.part_probe_types 	= nand_part_probes,	\
			},							\
			.ctrl		=					\
			{							\
				.cmd_ctrl		= nand_cmd_ctrl,	\
				.write_buf		= nand_write_buf,	\
				.read_buf		= nand_read_buf,	\
			}							\
		}								\
	}									\
}

#define STM_NAND_DEVICE(_driver, _id, _nand_config,			\
			_parts, _nr_parts, _chip_options)		\
{									\
	.name		= _driver,					\
	.id		= _id,						\
	.num_resources	= 2,  /* Note: EMI mem configured by driver */	\
	.resource	= (struct resource[]) {				\
		[0] = { /* NAND controller base address (FLEX/AFM) */	\
			.name		= "flex_mem",			\
			.flags		= IORESOURCE_MEM,		\
		},							\
		[1] = { /* NAND controller IRQ (FLEX/AFM) */		\
			.name		= "flex_irq",			\
			.flags		= IORESOURCE_IRQ,		\
		},							\
	},								\
	.dev		= {						\
		.platform_data = &(struct platform_nand_data) {		\
			.chip =						\
			{						\
				.partitions	= _parts,		\
				.nr_partitions	= _nr_parts,		\
				.options	= NAND_NO_AUTOINCR |	\
						_chip_options,		\
			},						\
			.ctrl =						\
			{						\
				.priv = _nand_config,			\
			},						\
		},							\
	},								\
}

#define SATA_DEVICE(_port, _base, _irq_hostc, _irq_dmac, _private)	\
{									\
	.name = "sata_stm",						\
	.id = _port,							\
	.dev = {							\
		.platform_data = _private,				\
	},								\
	.num_resources = 3,						\
	.resource = (struct resource[]) {				\
		[0] = {							\
			.start = _base,					\
			.end   = _base + 0xfff,				\
			.flags = IORESOURCE_MEM,			\
		},							\
		[1] = {							\
			.start = _irq_hostc,				\
			.end   = _irq_hostc,				\
			.flags = IORESOURCE_IRQ,			\
		},							\
		[2] = {							\
			.start = _irq_dmac,				\
			.end   = _irq_dmac,				\
			.flags = IORESOURCE_IRQ,			\
		}							\
	}								\
}
#define STASC_SC_DEVICE(_base, _irq, _fdma_req_rx, _fdma_req_tx,	\
			_base_sc, _pio_port, _ptx, _prx, _pcts,		\
			_prts, _pdtr, _pri, _dptx, _dprx, _dpcts,	\
			_dprts, _dpdtr, _dpri, _dflag)			\
{									\
	.name		= "stasc",					\
	.num_resources	= 5,						\
	.resource	= (struct resource[]) {				\
		{							\
			.start	= _base,				\
			.end	= _base + 0x100,			\
			.flags	= IORESOURCE_MEM			\
		}, {							\
			.start	= _irq,					\
			.end	= _irq,					\
			.flags	= IORESOURCE_IRQ			\
		}, {							\
			.start	= _fdma_req_rx,				\
			.end    = _fdma_req_rx,				\
			.flags	= IORESOURCE_DMA			\
		}, {							\
			.start	= _fdma_req_tx,				\
			.end    = _fdma_req_tx,				\
			.flags	= IORESOURCE_DMA			\
		}, {							\
			.start	= _base_sc,				\
			.end    = _base_sc + 0x8,			\
			.flags	= IORESOURCE_MEM			\
		}							\
  },									\
    .dev = {								\
    .platform_data = &(struct stasc_uart_data) {			\
      .pios = {								\
	[0] = {	/* TXD */						\
	  .pio_port	= _pio_port,					\
	  .pio_pin	= _ptx,						\
	  .pio_direction = _dptx,					\
	},								\
	[1] = { /* RXD */						\
	  .pio_port	= _pio_port,					\
	  .pio_pin	= _prx,						\
	  .pio_direction = _dprx,					\
	},								\
	[2] = {	/* CTS */						\
	  .pio_port	= _pio_port,					\
	  .pio_pin	= _pcts,					\
	  .pio_direction = _dpcts,				\
	},								\
	[3] = { /* RTS */						\
	  .pio_port	= _pio_port,					\
	  .pio_pin	= _prts,					\
	  .pio_direction = _dprts,					\
	},								\
	[4] = { /* DTR */						\
	  .pio_port	= _pio_port,					\
	  .pio_pin	= _pdtr,					\
	  .pio_direction = _dpdtr,				\
	},								\
	[5] = { /* RI */						\
	  .pio_port	= _pio_port,					\
	  .pio_pin	= _pri,						\
	  .pio_direction = _dpri,					\
	},								\
      },								\
      .flags = _dflag							\
    }									\
  }									\
}
