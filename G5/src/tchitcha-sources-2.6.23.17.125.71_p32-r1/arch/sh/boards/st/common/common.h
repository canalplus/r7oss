#ifndef __ARCH_SH_BOARDS_ST_COMMON_COMMON_H
#define __ARCH_SH_BOARDS_ST_COMMON_COMMON_H

#include <linux/platform_device.h>
#include <linux/spi/spi.h>

/* epld.c */

struct plat_epld_data {
	int opsize;
};

void epld_write(unsigned long value, unsigned long offset);
unsigned long epld_read(unsigned long offset);
void epld_early_init(struct platform_device *device);

/* harp.c */

void harp_init_irq(void);

/* peripheral boards callbacks */

void mbxxx_configure_audio_pins(int *pcm_reader, int *pcm_player);
void mbxxx_configure_nand_flash(struct platform_device *nand_flash);
void mbxxx_configure_serial_flash(struct spi_board_info *serial_flash,
				  unsigned int n_devices);

#endif
