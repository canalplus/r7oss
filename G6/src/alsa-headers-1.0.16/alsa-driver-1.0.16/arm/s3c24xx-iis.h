/* sound/arms3c24xx-iis.h
 *
 * (c) 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C24XX ALSA IIS audio driver core
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* forward declarations */

struct s3c24xx_iis_drv;
struct s3c24xx_iis_ops;

typedef struct s3c24xx_card s3c24xx_card_t;

#define ST_RUNNING		(1<<0)
#define ST_OPENED		(1<<1)

typedef struct s3c24xx_runtime {
	dmach_t			 dma_ch;
	unsigned int		 state;
	spinlock_t		 lock;

	unsigned int		 dma_loaded;
	unsigned int		 dma_limit;
	unsigned int		 dma_period;
	dma_addr_t		 dma_start;
	dma_addr_t		 dma_pos;
	dma_addr_t		 dma_end;

	snd_pcm_substream_t	*stream;
} s3c24xx_runtime_t;

struct s3c24xx_card {
	struct device		*device;
	struct clk		*clock;
	struct s3c24xx_iis_ops	*base_ops;
	struct s3c24xx_iis_ops	*chip_ops;
	unsigned char		 base_ops_claimed;
	unsigned char		 chip_ops_claimed;
	void __iomem		*regs;
	void			*client;
	snd_card_t		*card;
	snd_pcm_t		*pcm;
	struct semaphore	 sem;
	s3c24xx_runtime_t	 playback;
	s3c24xx_runtime_t	 capture;
};

extern s3c24xx_card_t *s3c24xx_iis_probe(struct device *dev);
extern int s3c24xx_iis_remove(struct device *dev);

extern s3c24xx_card_t *s3c24xx_iis_runtime_to_card(s3c24xx_runtime_t *rt);

extern int s3c24xx_iis_cfgclksrc(s3c24xx_card_t *, const char *src);

extern void s3c24xx_iismod_cfg(s3c24xx_card_t *, unsigned long set, unsigned long mask);
