#ifndef __SND_STM_AUD_TELSS_GLUE_H
#define __SND_STM_AUD_TELSS_GLUE_H


/*
 * Register access macros
 */

#define get__AUD_TELSS_REG(ip, offset, shift, mask) \
	((readl(ip->base + offset) >> shift) & mask)
#define set__AUD_TELSS_REG(ip, offset, shift, mask, value) \
	writel(((readl(ip->base + offset) & ~(mask << shift)) |	\
		(((value) & mask) << shift)), ip->base + offset)


/*
 * TELSS_IT_PRESCALER
 */

#define offset__AUD_TELSS_IT_PRESCALER(ip) 0x24
#define get__AUD_TELSS_IT_PRESCALER(ip) \
	readl(ip->base + offset__AUD_TELSS_IT_PRESCALER(ip))
#define set__AUD_TELSS_IT_PRESCALER(ip, value) \
	writel(value, ip->base + offset__AUD_TELSS_IT_PRESCALER(ip))

/* PRESCALE_VAL */

#define shift__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(ip) 0
#define mask__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(ip) 0x1f
#define get__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_IT_PRESCALER(ip), \
		shift__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(ip), \
		mask__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(IP))
#define set__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(ip, value) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_IT_PRESCALER(ip), \
		shift__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(ip), \
		mask__AUD_TELSS_IT_PRESCALE__PRESCALE_VAL(IP), value)


/*
 * TELSS_IT_NOISE_SUPP_WID
 */

#define offset__AUD_TELSS_IT_NOISE_SUPP_WID(ip)	0x28
#define get__AUD_TELSS_IT_NOISE_SUPP_WID(ip) \
	readl(ip->base + offset__AUD_TELSS_IT_NOISE_SUPP_WID(ip))
#define set__AUD_TELSS_IT_NOISE_SUPP_WID(ip, value) \
	writel(value, ip->base + offset__AUD_TELSS_IT_NOISE_SUPP_WID(ip))

/* NOISE_SUPP_WID */

#define shift__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(ip) 0
#define mask__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(ip) 0xff
#define get__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_IT_NOISE_SUPP_WID(ip), \
		shift__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(ip), \
		mask__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(IP))
#define set__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(ip, value) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_IT_NOISE_SUPP_WID(ip), \
		shift__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(ip), \
		mask__AUD_TELSS_IT_NOISE_SUPP_WID__NOISE_SUPP_WID(IP), value)

/*
 * TELSS_EXT_RST_N
 */

#define offset__AUD_TELSS_EXT_RST_N(ip)		0x2c
#define get__AUD_TELSS_EXT_RST_N(ip)		\
	readl(ip->base + offset__AUD_TELSS_EXT_RST_N(ip))
#define set__AUD_TELSS_EXT_RST_N(ip, value)		\
	writel(value, ip->base + offset__AUD_TELSS_EXT_RST_N(ip))

/* CODEC_SLIC_RST_N */

#define shift__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(ip) 0
#define mask__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(ip) 0x1
#define get__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(IP))
#define set__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N_ASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(IP), 0)
#define set__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N_DEASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__CODEC_SLIC_RST_N(IP), 1)

/* DECT_RST_N */

#define shift__AUD_CODEC_SLIC_RST_N__DECT_RST_N(ip) 1
#define mask__AUD_CODEC_SLIC_RST_N__DECT_RST_N(ip) 0x1
#define get__AUD_CODEC_SLIC_RST_N__DECT_RST_N(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__DECT_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__DECT_RST_N(IP))
#define set__AUD_CODEC_SLIC_RST_N__DECT_RST_N_ASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__DECT_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__DECT_RST_N(IP), 0)
#define set__AUD_CODEC_SLIC_RST_N__DECT_RST_N_DEASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__DECT_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__DECT_RST_N(IP), 1)

/* ZDS_RST_N */

#define shift__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(ip) 2
#define mask__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(ip) 0x1
#define get__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(IP))
#define set__AUD_CODEC_SLIC_RST_N__ZDS_RST_N_ASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(IP), 0)
#define set__AUD_CODEC_SLIC_RST_N__ZDS_RST_N_DEASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_CODEC_SLIC_RST_N(ip), \
		shift__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(ip), \
		mask__AUD_CODEC_SLIC_RST_N__ZDS_RST_N(IP), 1)


/*
 * TELSS_EXT_CS_N
 */

#define offset__AUD_TELSS_EXT_CS_N(ip) 0x30
#define get__AUD_TELSS_EXT_CS_N(ip) \
	readl(ip->base + offset__AUD_TELSS_EXT_CS_N(ip))
#define set__AUD_TELSS_EXT_CS_N(ip, value) \
	writel(value, ip->base + offset__AUD_TELSS_EXT_CS_N(ip))

/* CODEC_SLIC_CS_N */

#define shift__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(ip) 0
#define mask__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(ip) 0x1
#define get__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(IP))
#define set__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N_ASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(IP), 0)
#define set__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N_DEASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__CODEC_SLIC_CS_N(IP), 1)

/* DECT_CS_N */

#define shift__AUD_TELSS_EXT_CS_N__DECT_CS_N(ip) 1
#define mask__AUD_TELSS_EXT_CS_N__DECT_CS_N(ip) 0x1
#define get__AUD_TELSS_EXT_CS_N__DECT_CS_N(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__DECT_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__DECT_CS_N(IP))
#define set__AUD_TELSS_EXT_CS_N__DECT_CS_N_ASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__DECT_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__DECT_CS_N(IP), 0)
#define set__AUD_TELSS_EXT_CS_N__DECT_CS_N_DEASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__DECT_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__DECT_CS_N(IP), 1)

/* ZDS_CS_N */

#define shift__AUD_TELSS_EXT_CS_N__ZDS_CS_N(ip) 2
#define mask__AUD_TELSS_EXT_CS_N__ZDS_CS_N(ip) 0x1
#define get__AUD_TELSS_EXT_CS_N__ZDS_CS_N(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__ZDS_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__ZDS_CS_N(IP))
#define set__AUD_TELSS_EXT_CS_N__ZDS_CS_N_ASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__ZDS_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__ZDS_CS_N(IP), 0)
#define set__AUD_TELSS_EXT_CS_N__ZDS_CS_N_DEASSERTED(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_EXT_CS_N(ip), \
		shift__AUD_TELSS_EXT_CS_N__ZDS_CS_N(ip), \
		mask__AUD_TELSS_EXT_CS_N__ZDS_CS_N(IP), 1)


/*
 * TELSS_TDM_CTRL
 */

#define offset__AUD_TELSS_TDM_CTRL(ip) 0x34
#define get__AUD_TELSS_TDM_CTRL(ip) \
	readl(ip->base + offset__AUD_TELSS_TDM_CTRL(ip))
#define set__AUD_TELSS_TDM_CTRL(ip, value) \
	writel(value, ip->base + offset__AUD_TELSS_TDM_CTRL(ip))

/* CODEC_SLIC_FS_SEL */

#define shift__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(ip) 0
#define mask__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(IP))
#define set__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL_FS01(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(IP), 0)
#define set__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL_FS02(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL(IP), 1)

/* DECT_FS_SEL */

#define shift__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(ip) 1
#define mask__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(IP))
#define set__AUD_TELSS_TDM_CTRL__DECT_FS_SEL_FS01(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(IP), 1)
#define set__AUD_TELSS_TDM_CTRL__DECT_FS_SEL_FS02(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__DECT_FS_SEL(IP), 0)

/* EXT_TDM_DATA_IN_LEVEL */

#define shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(ip) 4
#define mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(IP))
#define set__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL_PULL_UP(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(IP), 0)
#define set__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL_PULL_DOWN(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL(IP), 1)

/* ZSI_TDM_DATA_IN_DEF_LEVEL */

#define shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(ip) 5
#define mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(IP))
#define set__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL_PULL_UP(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(IP), 0)
#define set__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL_PULL_DOWN(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_DATA_IN_DEF_LEVEL(IP), 1)

/* EXT_TDM_PATH */

#define shift__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(ip) 6
#define mask__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(IP))
#define set__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH_ENABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(IP), 0)
#define set__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH_DISABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH(IP), 1)

/* ZSI_TDM_PATH */

#define shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(ip) 7
#define mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(ip), \
		mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(IP))
#define set__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH_ENABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(ip), \
		mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(IP), 0)
#define set__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH_DISABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(ip), \
		mask__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH(IP), 1)

/* EXT_TDM_DATA_IN_DEL */

#define shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(ip) 8
#define mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(ip) 0x1
#define get__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(IP))
#define set__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL_ENABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(IP), 0)
#define set__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL_DISABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_CTRL(ip), \
		shift__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(ip), \
		mask__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL(IP), 1)


/*
 * TELSS_TDM_DEBUG
 */

#define offset__AUD_TELSS_TDM_DEBUG(ip) 0x100
#define get__AUD_TELSS_TDM_DEBUG(ip) \
	readl(ip->base + offset__AUD_TELSS_TDM_DEBUG(ip))
#define set__AUD_TELSS_TDM_DEBUG(ip, value) \
	writel(value, ip->base + offset__AUD_TELSS_TDM_DEBUG(ip))

/* DEBUG_SEL */

#define shift__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(ip) 0
#define mask__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(ip) 0xf
#define get__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_DEBUG(ip), \
		shift__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(ip), \
		mask__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(IP))
#define set__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL_ENABLE(ip, value) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_DEBUG(ip), \
		shift__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(ip), \
		mask__AUD_TELSS_TDM_DEBUG__TDM_DEBUG_SEL(IP), value)

/* TDM_LOOPBACK */

#define shift__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(ip) 6
#define mask__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(ip) 0x1
#define get__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(ip) \
	get__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_DEBUG(ip), \
		shift__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(ip), \
		mask__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(IP))
#define set__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK_ENABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_DEBUG(ip), \
		shift__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(ip), \
		mask__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(IP), 1)
#define set__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK_DISABLE(ip) \
	set__AUD_TELSS_REG(ip, \
		offset__AUD_TELSS_TDM_DEBUG(ip), \
		shift__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(ip), \
		mask__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK(IP), 0)


#endif
