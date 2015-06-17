/*
 * Audio support for codec Philips UDA1380
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005 Giorgio Padrin <giorgio@mandarinlogiq.org>
 */

#include "adriver.h"
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/info.h>

#include <asm/byteorder.h>

#include <sound/uda1380.h>

/* begin {{ I2C }} */

static struct i2c_driver snd_uda1380_i2c_driver = {
	.driver = {
		.name = "uda1380-i2c"
	},
};

static int snd_uda1380_i2c_init(void)
{
	return i2c_add_driver(&snd_uda1380_i2c_driver);
}

static void snd_uda1380_i2c_free(void)
{
	i2c_del_driver(&snd_uda1380_i2c_driver);
}

static inline int snd_uda1380_i2c_probe(struct snd_uda1380 *uda)
{
	if (uda->i2c_client.adapter == NULL ||
	    (uda->i2c_client.addr & 0xfd) != 0x18) return -EINVAL;
	if (i2c_smbus_xfer(uda->i2c_client.adapter, uda->i2c_client.addr,
			   0, 0, 0, I2C_SMBUS_QUICK, NULL) < 0)
		return -ENODEV;
	else return 0;
}

static int snd_uda1380_i2c_attach(struct snd_uda1380 *uda)
{
	int ret = 0;
	if ((ret = snd_uda1380_i2c_probe(uda)) < 0) return ret;
	snprintf(uda->i2c_client.name, sizeof(uda->i2c_client.name),
		 "uda1380-i2c at %d-%04x",
		 i2c_adapter_id(uda->i2c_client.adapter), uda->i2c_client.addr);
	return i2c_attach_client(&uda->i2c_client);
}

static void snd_uda1380_i2c_detach(struct snd_uda1380 *uda)
{
	i2c_detach_client(&uda->i2c_client);
}

/* end {{ I2C }} */


/* begin {{ Registers Cache <--> HW }} */

static inline void _cpu_to_be16_string(u16 *to, u16 *from, unsigned int count)
{
#ifdef __BIG_ENDIAN
	memcpy(to, from, count << 1);
#else
	u16 *i_to, *i_from;
	for (i_to = to, i_from = from; i_to < to + count; i_to++, i_from++)
		*i_to = cpu_to_be16(*i_from);
#endif
}

static inline void _be16_to_cpu_string(u16 *to, u16 *from, unsigned int count)
{
#ifdef __BIG_ENDIAN
	memcpy(to, from, count << 1);
#else
	u16 *i_to, *i_from;
	for (i_to = to, i_from = from; i_to < to + count; i_to++, i_from++)
		*i_to = be16_to_cpu(*i_from);
#endif
}

/* transfer of up to 5 consecutive regs (a section)*/
static int snd_uda1380_hwsync(struct snd_uda1380 *uda, int read,
			      u8 start_reg, unsigned int count)
{
	struct i2c_msg msgs[2];
	u8 buf[11];
	int ret;

	snd_assert(count * 2 <= sizeof(buf) - 1, return -EINVAL);
	snd_assert(start_reg < ARRAY_SIZE(uda->regs), return -EINVAL);
	snd_assert(start_reg + count <= ARRAY_SIZE(uda->regs), return -EINVAL);

	/* setup i2c msgs */
	msgs[0].addr = uda->i2c_client.addr;
	msgs[0].flags = 0;
	msgs[0].buf = buf;
	if (!read)
		msgs[0].len = (count << 1) + 1;
	else {
		msgs[1].flags = I2C_M_RD;
		msgs[1].addr = msgs[0].addr;
		msgs[1].buf = msgs[0].buf + 1;
		msgs[0].len = 1;
		msgs[1].len = count << 1;
	}

	buf[0] = start_reg;

	/* regs -> buffer, on write */
	if (!read && start_reg != 0x7f) /* 0x7f: software reset */
		 _cpu_to_be16_string((u16*) (buf + 1), &uda->regs[start_reg], count);

	/* i2c transfer */
	ret = i2c_transfer(uda->i2c_client.adapter, msgs, read ? 2 : 1);
	if (ret != (read ? 2 : 1)) return ret; /* transfer error */ //@@ error ret < 0, or not ?

	/* regs <- buffer, on read */
	if (read && start_reg != 0x7f) /* 0x7f: software reset */
		_be16_to_cpu_string(&uda->regs[start_reg], (u16*) (buf + 1), count);

	return 0;
}

static inline int snd_uda1380_hwsync_read(struct snd_uda1380 *uda,
				 	  u8 start_reg, unsigned int count)
{
	return snd_uda1380_hwsync(uda, 1, start_reg, count);
}

static inline int snd_uda1380_hwsync_write(struct snd_uda1380 *uda,
					   u8 start_reg, unsigned int count)
{
	return snd_uda1380_hwsync(uda, 0, start_reg, count);
}

/* end {{ Registers Cache <--> HW }} */


/* begin {{ Registers Cache Ops }} */

static inline int _cache_flush1(struct snd_uda1380 *uda, unsigned int i)
{
	snd_uda1380_hwsync_write(uda,
				 uda->cache_dirty[i].start_reg,
				 uda->cache_dirty[i].count);
	uda->cache_dirty[i].start_reg = i << 4;
	uda->cache_dirty[i].count = 0;
	return 0;
}

static inline int snd_uda1380_cache_try_flush(struct snd_uda1380 *uda)
{

	if (uda->cache_dirty[0].count)
		if (uda->powered_on) _cache_flush1(uda, 0);
	if (uda->cache_dirty[1].count)
		if (uda->playback_clock_on) _cache_flush1(uda, 1);
	if (uda->cache_dirty[2].count)
		if (uda->capture_clock_on) _cache_flush1(uda, 2);
	return 0;
}

/* Note: regs in the same section */
static int snd_uda1380_cache_dirty_zone(struct snd_uda1380 *uda,
					u8 start_reg, unsigned int count)
{
	unsigned int i = start_reg >> 4;

	if (!count) return 0;
	if (!uda->cache_dirty[i].count) {
		uda->cache_dirty[i].start_reg = start_reg;
		uda->cache_dirty[i].count = count;
	} else {
		int sr0 = uda->cache_dirty[i].start_reg;
		int er0 = sr0 + uda->cache_dirty[i].count -1;
		int sr1 = start_reg;
		int er1 = sr1 + count -1;
		int sr = (sr1 <= sr0) ? sr1 : sr0;
		int er = (er1 >= er0) ? er1 : er0;
		uda->cache_dirty[i].start_reg = (u8) sr;
		uda->cache_dirty[i].count = (u8) (er - sr + 1);
	}
	snd_uda1380_cache_try_flush(uda);
	return 0;
}

static inline int snd_uda1380_cache_dirty(struct snd_uda1380 *uda, u8 reg)
{
	return snd_uda1380_cache_dirty_zone(uda, reg, 1);
}

static inline int snd_uda1380_cache_dirty_all(struct snd_uda1380 *uda)
{
	snd_uda1380_cache_dirty_zone(uda, 0x00, 5);
	snd_uda1380_cache_dirty_zone(uda, 0x10, 5);
	snd_uda1380_cache_dirty_zone(uda, 0x20, 4);
	return 0;
}

/* end {{ Registers Cache Ops }} */


static inline void snd_uda1380_lock(struct snd_uda1380 *uda)
{
	down(&uda->sem);
}

static inline void snd_uda1380_unlock(struct snd_uda1380 *uda)
{
	up(&uda->sem);
}

#define WRITE_MASK(i, val, mask)	(((i) & ~(mask)) | ((val) & (mask)))


/* begin {{ Controls }} */

/* a control element in a register */
struct snd_uda1380_uctl_reg_elem_int {
	unsigned int
		is_stereo:1,
		inv_range:1,	/* inverted range */
		rot_range:1,	/* rotated half range (from a 2's complement) */
		reg:8,
		shift:4,	/* shift */
		mask:16;
};

#define ROT_RANGE(val, mask) \
	(((val) + (((mask) + 1) >> 1)) & (mask))
#define INV_RANGE(val, mask) \
	(~(val) & (mask))

static int snd_uda1380_actl_reg_elem_int_info(struct snd_kcontrol *kcontrol,
					      struct snd_ctl_elem_info *uinfo)
{
	struct snd_uda1380_uctl_reg_elem_int *uctl =
		(struct snd_uda1380_uctl_reg_elem_int *) kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = uctl->is_stereo ? 2 : 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = uctl->is_stereo ? uctl->mask & 0x00ff : uctl->mask;
	return 0;
}

static int snd_uda1380_actl_reg_elem_int_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *) kcontrol->private_data;
	struct snd_uda1380_uctl_reg_elem_int *uctl =
		(struct snd_uda1380_uctl_reg_elem_int *) kcontrol->private_value;
	unsigned int val, val_l, val_r, mask1;

	val = uda->regs[uctl->reg] >> uctl->shift & uctl->mask;
	if (uctl->is_stereo) {
		val_l = val >> 8; val_r = val & 0x00ff; mask1 = uctl->mask & 0x00ff;
		if (uctl->rot_range) { val_l = ROT_RANGE(val_l, mask1);
				       val_r = ROT_RANGE(val_r, mask1); }
		if (uctl->inv_range) { val_l = INV_RANGE(val_l, mask1);
				       val_r = INV_RANGE(val_r, mask1); }
		ucontrol->value.integer.value[0] = val_l;
		ucontrol->value.integer.value[1] = val_r;
	} else {
		if (uctl->rot_range) val = ROT_RANGE(val, uctl->mask);
		if (uctl->inv_range) val = INV_RANGE(val, uctl->mask);
		ucontrol->value.integer.value[0] = val;
	}
	return 0;
}

static int snd_uda1380_actl_reg_elem_int_put(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *) kcontrol->private_data;
	struct snd_uda1380_uctl_reg_elem_int *uctl =
		(struct snd_uda1380_uctl_reg_elem_int *) kcontrol->private_value;
	unsigned int val, val_l, val_r, mask1;

	if (uctl->is_stereo) {
		val_l = ucontrol->value.integer.value[0];
		val_r = ucontrol->value.integer.value[1];
		mask1 = uctl->mask & 0x00ff;
		if (uctl->rot_range) { val_l = ROT_RANGE(val_l, mask1);
				       val_r = ROT_RANGE(val_r, mask1); }
		if (uctl->inv_range) { val_l = INV_RANGE(val_l, mask1);
				       val_r = INV_RANGE(val_r, mask1); }
		val = val_l << 8 | val_r;
	} else {
		val = ucontrol->value.integer.value[0];
		if (uctl->rot_range) val = ROT_RANGE(val, uctl->mask);
		if (uctl->inv_range) val = INV_RANGE(val, uctl->mask);
	}

	snd_uda1380_lock(uda);
	uda->regs[uctl->reg] = WRITE_MASK(uda->regs[uctl->reg],
					  val << uctl->shift, uctl->mask << uctl->shift);

	snd_uda1380_cache_dirty(uda, uctl->reg);
	snd_uda1380_unlock(uda);
	return 0;
}

/* declarations of ALSA reg_elem_int controls */

#define ACTL_REG_ELEM_INT(ctl_name, _name, _reg, _shift, _mask, _inv_range, _rot_range) \
static struct snd_uda1380_uctl_reg_elem_int snd_uda1380_actl_ ## ctl_name ## _pvalue = \
{ .reg = _reg, .shift = _shift, .mask = _mask, \
  .inv_range = _inv_range, .rot_range = _rot_range }; \
static struct snd_kcontrol_new snd_uda1380_actl_ ## ctl_name = \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = _name, .info = snd_uda1380_actl_reg_elem_int_info, \
  .get = snd_uda1380_actl_reg_elem_int_get, .put = snd_uda1380_actl_reg_elem_int_put, \
  .private_value = (unsigned long)&snd_uda1380_actl_ ## ctl_name ## _pvalue };

#define ACTL_REG_ELEM_INT_STEREO(ctl_name, _name, _reg, _shift, _mask, _inv_range, _rot_range) \
static struct snd_uda1380_uctl_reg_elem_int snd_uda1380_actl_ ## ctl_name ## _pvalue = \
{ .is_stereo = 1, .reg = _reg, .shift = _shift, .mask = _mask, \
  .inv_range = _inv_range, .rot_range = _rot_range }; \
static struct snd_kcontrol_new snd_uda1380_actl_ ## ctl_name = \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = _name, .info = snd_uda1380_actl_reg_elem_int_info, \
  .get = snd_uda1380_actl_reg_elem_int_get, .put = snd_uda1380_actl_reg_elem_int_put, \
  .private_value = (unsigned long)&snd_uda1380_actl_ ## ctl_name ## _pvalue };\

ACTL_REG_ELEM_INT_STEREO(playback_volume, "Master Playback Volume", 0x10, 0, 0xffff, 1, 0)
ACTL_REG_ELEM_INT(deemphasis, "De-Emphasis", 0x13, 0, 0x0007, 0, 0)
ACTL_REG_ELEM_INT(tone_ctl_strength, "Tone Control - Strength", 0x12, 14, 0x0003, 0, 0)
ACTL_REG_ELEM_INT_STEREO(tone_ctl_treble, "Tone Control - Treble", 0x12, 4, 0x0303, 0, 0)
ACTL_REG_ELEM_INT_STEREO(tone_ctl_bass, "Tone Control - Bass", 0x12, 0, 0x0f0f, 0, 0)
ACTL_REG_ELEM_INT(mic_gain, "Mic Capture Volume", 0x22, 8, 0x000f, 0, 0)
ACTL_REG_ELEM_INT_STEREO(line_in_gain, "Line Capture Volume", 0x21, 0, 0x0f0f, 0, 0)
ACTL_REG_ELEM_INT_STEREO(capture_volume, "Capture Volume", 0x20, 0, 0xffff, 0, 1)

struct snd_uda1380_uctl_bool {
	int (*get) (struct snd_uda1380 *uda);
	int (*set) (struct snd_uda1380 *uda, int on);
};

static int snd_uda1380_actl_bool_info(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	return 0;
}

static int snd_uda1380_actl_bool_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *) kcontrol->private_data;
	struct snd_uda1380_uctl_bool *uctl =
		(struct snd_uda1380_uctl_bool *) kcontrol->private_value;

	ucontrol->value.integer.value[0] = uctl->get(uda);
	return 0;
}

static int snd_uda1380_actl_bool_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *) kcontrol->private_data;
	struct snd_uda1380_uctl_bool *uctl =
		(struct snd_uda1380_uctl_bool *) kcontrol->private_value;

	return uctl->set(uda, ucontrol->value.integer.value[0]);
}

/* Register flags */
#define R00_EN_ADC	0x0800
#define R00_EN_DEC	0x0400
#define R00_EN_DAC	0x0200
#define R00_EN_INT	0x0100
#define R02_PON_HP	0x2000
#define R02_PON_DAC	0x0400
#define R02_PON_BIAS	0x0100
#define R02_PON_LNA	0x0010
#define R02_PON_PGAL	0x0008
#define R02_PON_ADCL	0x0004
#define R02_PON_PGAR	0x0002
#define R02_PON_ADCR	0x0001
#define R13_MTM		0x4000
#define R21_MT_ADC	0x8000
#define R22_SEL_LNA	0x0008
#define R22_SEL_MIC	0x0004
#define R22_SKIP_DCFIL	0x0002
#define R23_AGC_EN	0x0001

static int snd_uda1380_uctl_playback_switch_get(struct snd_uda1380 *uda)
{
	return uda->playback_switch_ureq;
}

static int snd_uda1380_uctl_playback_switch_set(struct snd_uda1380 *uda, int on)
{
	snd_uda1380_lock(uda);
	uda->playback_switch_ureq = on;
	if (uda->playback_on) {
		uda->regs[0x13] = WRITE_MASK(uda->regs[0x13],
					     on ? 0x0000 : R13_MTM, R13_MTM);
		snd_uda1380_hwsync_write(uda, 0x13, 1);
	}
	snd_uda1380_unlock(uda);
	return 0;
}

static int snd_uda1380_uctl_capture_switch_get(struct snd_uda1380 *uda)
{
	return uda->capture_switch_ureq;
}

static int snd_uda1380_uctl_capture_switch_set(struct snd_uda1380 *uda, int on)
{
	snd_uda1380_lock(uda);
	uda->capture_switch_ureq = on;
	if (uda->capture_on) {
		uda->regs[0x21] = WRITE_MASK(uda->regs[0x21],
					     on ? 0x0000 : R21_MT_ADC, R21_MT_ADC);
		snd_uda1380_hwsync_write(uda, 0x21, 1);
	}
	snd_uda1380_unlock(uda);
	return 0;
}

static int snd_uda1380_uctl_agc_get(struct snd_uda1380 *uda)
{
	return uda->regs[0x23] & R23_AGC_EN;
}

static int snd_uda1380_uctl_agc_set (struct snd_uda1380 *uda, int on)
{
	snd_uda1380_lock(uda);
	uda->regs[0x23] = WRITE_MASK(uda->regs[0x23],
				     on ? R23_AGC_EN : 0x0000, R23_AGC_EN);
	snd_uda1380_cache_dirty_zone(uda, 0x23, 1);
	snd_uda1380_unlock(uda);
	return 0;
}

#define ACTL_BOOL(ctl_name, _name) \
static struct snd_uda1380_uctl_bool snd_uda1380_actl_ ## ctl_name ## _pvalue = \
{ .get = snd_uda1380_uctl_ ## ctl_name ## _get, \
  .set = snd_uda1380_uctl_ ## ctl_name ## _set }; \
static struct snd_kcontrol_new snd_uda1380_actl_ ## ctl_name = \
{ .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = _name, .info = snd_uda1380_actl_bool_info, \
  .get = snd_uda1380_actl_bool_get, .put = snd_uda1380_actl_bool_put, \
  .private_value = (unsigned long) &snd_uda1380_actl_ ## ctl_name ## _pvalue };

ACTL_BOOL(playback_switch, "Master Playback Switch")
ACTL_BOOL(agc, "AGC")
ACTL_BOOL(capture_switch, "Capture Switch")

static inline void snd_uda1380_line_out_on(struct snd_uda1380 *uda, int on);

void snd_uda1380_hp_connected(struct snd_uda1380 *uda, int connected)
{
	snd_uda1380_lock(uda);
	if (connected != uda->hp_connected) {
		uda->hp_connected = connected;
		if (uda->playback_on) {
			uda->regs[0x02] =
				WRITE_MASK(uda->regs[0x02],
					   connected ? R02_PON_HP : 0x0000, R02_PON_HP);
			snd_uda1380_hwsync_write(uda, 0x02, 1);
			if (uda->hp_or_line_out)
				snd_uda1380_line_out_on(uda, !connected);
		}
	}
	snd_uda1380_unlock(uda);
}

static int snd_uda1380_uctl_select_capture_source(struct snd_uda1380 *uda,
						  enum snd_uda1380_capture_source capture_source)
{
	snd_uda1380_lock(uda);
	uda->capture_source = capture_source;

	if ((uda->capture_source == SND_UDA1380_CAP_SOURCE_MIC) ==
	    ((uda->regs[0x22] & R22_SEL_MIC) == R22_SEL_MIC))
		 { snd_uda1380_unlock(uda); return 0; }

	if (uda->capture_on) {
		if (uda->capture_source == SND_UDA1380_CAP_SOURCE_MIC) {
			uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
						     R02_PON_LNA, R02_PON_LNA);
			snd_uda1380_hwsync_write(uda, 0x02, 1);
			uda->regs[0x22] = WRITE_MASK(uda->regs[0x22],
						     R22_SEL_LNA | R22_SEL_MIC,
						     R22_SEL_LNA | R22_SEL_MIC);
			snd_uda1380_hwsync_write(uda, 0x22, 1);
			uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
						     0x0000, R02_PON_PGAL | R02_PON_PGAR);
			snd_uda1380_hwsync_write(uda, 0x02, 1);
		} else {
			uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
						     R02_PON_PGAL | R02_PON_PGAR,
						     R02_PON_PGAL | R02_PON_PGAR);
			snd_uda1380_hwsync_write(uda, 0x02, 1);
			uda->regs[0x22] = WRITE_MASK(uda->regs[0x22],
						     0x0000,
						     R22_SEL_LNA | R22_SEL_MIC);
			snd_uda1380_hwsync_write(uda, 0x22, 1);
			uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
						     0x0000, R02_PON_LNA );
			snd_uda1380_hwsync_write(uda, 0x02, 1);
		}
	}
	snd_uda1380_unlock(uda);
	return 0;
}

static int snd_uda1380_actl_capture_source_info(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item == SND_UDA1380_CAP_SOURCE_LINE_IN)
		strcpy(uinfo->value.enumerated.name, "Line");
	else strcpy(uinfo->value.enumerated.name, "Mic");
	return 0;
}

static int snd_uda1380_actl_capture_source_get(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *) kcontrol->private_data;

	ucontrol->value.enumerated.item[0] = uda->capture_source;
	return 0;
}

static int snd_uda1380_actl_capture_source_put(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *) kcontrol->private_data;

	return snd_uda1380_uctl_select_capture_source(uda, ucontrol->value.enumerated.item[0]);
}

static struct snd_kcontrol_new snd_uda1380_actl_capture_source = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = "Capture Source",
	.info = snd_uda1380_actl_capture_source_info,
	.get = snd_uda1380_actl_capture_source_get,
	.put = snd_uda1380_actl_capture_source_put
};

/* end {{ Controls }} */


/* begin {{ Headphone Detected Notification }} */

static void snd_uda1380_hp_detected_w_fn(void *p)
{
	struct snd_uda1380 *uda = (struct snd_uda1380 *)p;

	snd_uda1380_hp_connected(uda, uda->hp_detected.detected);
}

void snd_uda1380_hp_detected(struct snd_uda1380 *uda, int detected)
{
	if (detected != uda->hp_detected.detected) {
		uda->hp_detected.detected = detected;
		queue_work(uda->hp_detected.wq, &uda->hp_detected.w);
	}
}

static int snd_uda1380_hp_detected_init(struct snd_uda1380 *uda)
{
	INIT_WORK(&uda->hp_detected.w, snd_uda1380_hp_detected_w_fn, uda);
	uda->hp_detected.detected = uda->hp_connected;
	uda->hp_detected.wq = create_singlethread_workqueue("uda1380");
	if (uda->hp_detected.wq) return 0;
	else return -1;
}

static void snd_uda1380_hp_detected_free(struct snd_uda1380 *uda)
{
	destroy_workqueue(uda->hp_detected.wq);
}

/* end {{ Headphone Detected Notification }} */


/* begin {{ Codec Control }} */

static inline int _wait_mute(struct snd_uda1380 *uda, u8 reg, int on)
{
#define MUTE_STATE 0x0004
	unsigned int timeout_count = 50;

	snd_uda1380_hwsync_read(uda, reg, 1);
	while (((uda->regs[reg] & MUTE_STATE) == MUTE_STATE) != on) {
		if (--timeout_count == 0) break;
		msleep(1);
		snd_uda1380_hwsync_read(uda, reg, 1);
	}
	return (timeout_count) ? 0 : -1;
}

static inline int snd_uda1380_power_on(struct snd_uda1380 *uda)
{
	uda->power_on_chip(1);
	uda->reset_pin(1);
	mdelay(1);
	uda->reset_pin(0);
	uda->powered_on = 1;
	return 0;
}

static inline int snd_uda1380_power_off(struct snd_uda1380 *uda)
{
	uda->powered_on = 0;
	uda->power_on_chip(0);
	return 0;
}

static inline void snd_uda1380_line_out_on(struct snd_uda1380 *uda, int on)
{
	if (uda->line_out_on) uda->line_out_on(on);
}

static inline void snd_uda1380_mic_on(struct snd_uda1380 *uda, int on)
{
	if (uda->mic_on) uda->mic_on(on);
}

static inline void snd_uda1380_line_in_on(struct snd_uda1380 *uda, int on)
{
	if (uda->line_in_on) uda->line_in_on(on);
}

static int snd_uda1380_playback_on(struct snd_uda1380 *uda)
{
	unsigned int val;

	if (uda->playback_on) return 0;

	/* power-up */
	uda->regs[0x00] = WRITE_MASK(uda->regs[0x00],
				     R00_EN_DAC | R00_EN_INT, R00_EN_DAC | R00_EN_INT);
	val = R02_PON_DAC;
	val |= uda->hp_connected ? R02_PON_HP : 0x0000;
	uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
				     val, R02_PON_HP | R02_PON_DAC);
	snd_uda1380_hwsync_write(uda, 0x00, 3);
	uda->playback_clock_on = 1;

	snd_uda1380_cache_try_flush(uda);

	/* notify line out is on */
	if (!(uda->hp_or_line_out && uda->hp_connected))
		snd_uda1380_line_out_on(uda, 1);

	/* unmute, in case */
	if (uda->playback_switch_ureq) {
		uda->regs[0x13] = WRITE_MASK(uda->regs[0x13], 0x0000, R13_MTM);
		snd_uda1380_hwsync_write(uda, 0x13, 1);
		_wait_mute(uda, 0x18, 0);
	}

	uda->playback_on = 1;

	return 0;
}

static int snd_uda1380_playback_off(struct snd_uda1380 *uda)
{
	if (!uda->playback_on) return 0;

	uda->playback_on = 0;

	/* mute */
	if (!(uda->regs[0x13] & R13_MTM)) {
		uda->regs[0x13] = WRITE_MASK(uda->regs[0x13], R13_MTM, R13_MTM);
		snd_uda1380_hwsync_write(uda, 0x13, 1);
	}
	_wait_mute(uda, 0x18, 1);

	/* notify line out going off */
	if (!(uda->hp_or_line_out && uda->hp_connected))
		snd_uda1380_line_out_on(uda, 0);

	/* power-down */
	uda->playback_clock_on = 0;
	uda->regs[0x00] = WRITE_MASK(uda->regs[0x00], 0x0000, R00_EN_DAC | R00_EN_INT);
	uda->regs[0x02] = WRITE_MASK(uda->regs[0x02], 0x0000, R02_PON_HP | R02_PON_DAC);
	snd_uda1380_hwsync_write(uda, 0x00, 3);

	return 0;
}

static int snd_uda1380_capture_on(struct snd_uda1380 *uda)
{
	unsigned int val;

	if (uda->capture_on) return 0;

	/* power-up */
	uda->regs[0x00] = WRITE_MASK(uda->regs[0x00],
				     R00_EN_ADC | R00_EN_DEC, R00_EN_ADC | R00_EN_DEC);
	snd_uda1380_hwsync_write(uda, 0x00, 1);
	uda->capture_clock_on = 1;

	snd_uda1380_cache_try_flush(uda);

	val = R02_PON_ADCL | R02_PON_ADCR;
	val |= (uda->regs[0x22] & R22_SEL_MIC) ?
	       R02_PON_LNA :
	       R02_PON_PGAL | R02_PON_PGAR;
	uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
				     val, R02_PON_ADCL | R02_PON_ADCR |
					  R02_PON_LNA | R02_PON_PGAL | R02_PON_PGAR);
	snd_uda1380_hwsync_write(uda, 0x02, 1);

	/* notify input sources */
	if (uda->regs[0x22] & R22_SEL_MIC) snd_uda1380_mic_on(uda, 1);
	else snd_uda1380_line_in_on(uda, 1);

	/* unmute, in case */
	if (uda->playback_switch_ureq) {
		uda->regs[0x21] = WRITE_MASK(uda->regs[0x21], 0x0000, R21_MT_ADC);
		snd_uda1380_hwsync_write(uda, 0x21, 1);
		_wait_mute(uda, 0x28, 0);
	}

	uda->capture_on = 1;

	return 0;
}

static int snd_uda1380_capture_off(struct snd_uda1380 *uda)
{
	if (!uda->capture_on) return 0;

	uda->capture_on = 0;

	/* mute */
	if (!(uda->regs[0x21] & R21_MT_ADC)) {
		uda->regs[0x21] = WRITE_MASK(uda->regs[0x21], R21_MT_ADC, R21_MT_ADC);
		snd_uda1380_hwsync_write(uda, 0x21, 1);
	}
	_wait_mute(uda, 0x28, 1);

	/* notify input sources going off */
	if (uda->regs[0x22] & R22_SEL_MIC) snd_uda1380_mic_on(uda, 0);
	else snd_uda1380_line_in_on(uda, 0);

	/* power-down */
	uda->capture_clock_on = 0;
	uda->regs[0x00] = WRITE_MASK(uda->regs[0x00], 0x0000, R00_EN_ADC | R00_EN_DEC);
	uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
				     0x0000, R02_PON_ADCL | R02_PON_ADCR |
				     R02_PON_LNA | R02_PON_PGAL | R02_PON_PGAR);
	snd_uda1380_hwsync_write(uda, 0x00, 3);

	return 0;
}

int snd_uda1380_open_stream(struct snd_uda1380 *uda, int stream)
{
	snd_uda1380_lock(uda);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		uda->playback_stream_opened = 1;
		snd_uda1380_playback_on(uda);
	} else {
		uda->capture_stream_opened = 1;
		snd_uda1380_capture_on(uda);
	}
	snd_uda1380_unlock(uda);
	return 0;
}

int snd_uda1380_close_stream(struct snd_uda1380 *uda, int stream)
{
	snd_uda1380_lock(uda);
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		uda->playback_stream_opened = 0;
		snd_uda1380_playback_off(uda);
	} else {
		uda->capture_stream_opened = 0;
		snd_uda1380_capture_off(uda);
	}
	snd_uda1380_unlock(uda);
	return 0;
}

static int snd_uda1380_init_regs(struct snd_uda1380 *uda)
{
	snd_uda1380_hwsync_read(uda, 0x00, 5);
	snd_uda1380_hwsync_read(uda, 0x10, 5);
	snd_uda1380_hwsync_read(uda, 0x20, 4);

	//@@ MEMO: add some configs

	if (uda->capture_source == SND_UDA1380_CAP_SOURCE_MIC) {
		uda->regs[0x22] = WRITE_MASK(uda->regs[0x22],
					     R22_SEL_LNA | R22_SEL_MIC,
					     R22_SEL_LNA | R22_SEL_MIC);
	}
	uda->regs[0x22] = WRITE_MASK(uda->regs[0x22], 0x0000, R22_SKIP_DCFIL);
	snd_uda1380_hwsync_write(uda, 0x22, 1);

	uda->regs[0x00] = WRITE_MASK(uda->regs[0x00],
				     0x0000, R00_EN_DEC | R00_EN_INT);
	uda->regs[0x02] = WRITE_MASK(uda->regs[0x02],
				     R02_PON_BIAS, R02_PON_BIAS);
	snd_uda1380_hwsync_write(uda, 0x00, 3);

	return 0;
}

int snd_uda1380_suspend(struct snd_uda1380 *uda, pm_message_t state)
{
	snd_uda1380_lock(uda);
	if (uda->playback_on) snd_uda1380_playback_off(uda);
	if (uda->capture_on) snd_uda1380_capture_off(uda);
	snd_uda1380_power_off(uda);
	snd_uda1380_cache_dirty_all(uda);
	snd_uda1380_unlock(uda);
	return 0;
}

int snd_uda1380_resume(struct snd_uda1380 *uda)
{
	snd_uda1380_lock(uda);
	snd_uda1380_power_on(uda);
	snd_uda1380_cache_try_flush(uda);
	if (uda->playback_stream_opened) snd_uda1380_playback_on(uda);
	if (uda->capture_stream_opened) snd_uda1380_capture_on(uda);
	snd_uda1380_unlock(uda);
	return 0;
}

static void snd_uda1380_init_uda(struct snd_uda1380 *uda)
{
	init_MUTEX(&uda->sem);
	uda->i2c_client.driver = &snd_uda1380_i2c_driver;
}

int snd_uda1380_activate(struct snd_uda1380 *uda)
{
	int ret = 0;

	snd_uda1380_init_uda(uda);
	snd_uda1380_lock(uda);
	snd_uda1380_power_on(uda);
	if ((ret = snd_uda1380_i2c_attach(uda)) < 0)
		goto failed_i2c_attach;
	snd_uda1380_init_regs(uda);
	if ((ret = snd_uda1380_hp_detected_init(uda)) < 0)
		goto failed_hp_detected_init;
	snd_uda1380_unlock(uda);
	return 0;

 failed_hp_detected_init:
	snd_uda1380_i2c_detach(uda);
 failed_i2c_attach:
	snd_uda1380_power_off(uda);
	snd_uda1380_unlock(uda);
	return ret;
}

void snd_uda1380_deactivate(struct snd_uda1380 *uda)
{
	snd_uda1380_lock(uda);
	snd_uda1380_hp_detected_free(uda);
	snd_uda1380_i2c_detach(uda);
	snd_uda1380_power_off(uda);
	snd_uda1380_unlock(uda);
}

int snd_uda1380_add_mixer_controls(struct snd_uda1380 *uda, struct snd_card *card)
{
	snd_uda1380_lock(uda);
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_playback_volume, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_playback_switch, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_deemphasis, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_tone_ctl_strength, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_tone_ctl_treble, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_tone_ctl_bass, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_capture_volume, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_capture_switch, uda));
	if (uda->mic_connected)
		snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_mic_gain, uda));
	if (uda->line_in_connected)
		snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_line_in_gain, uda));
	if (uda->mic_connected && uda->line_in_connected)
		snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_capture_source, uda));
	snd_ctl_add(card, snd_ctl_new1(&snd_uda1380_actl_agc, uda));
	snd_uda1380_unlock(uda);
	return 0;
}

/* end {{ Codec Control }} */


/* begin {{ Module }} */

static int __init snd_uda1380_module_on_load(void)
{
	snd_uda1380_i2c_init();
	return 0;
}

static void __exit snd_uda1380_module_on_unload(void)
{
	snd_uda1380_i2c_free();
}

module_init(snd_uda1380_module_on_load);
module_exit(snd_uda1380_module_on_unload);

EXPORT_SYMBOL(snd_uda1380_activate);
EXPORT_SYMBOL(snd_uda1380_deactivate);
EXPORT_SYMBOL(snd_uda1380_add_mixer_controls);
EXPORT_SYMBOL(snd_uda1380_open_stream);
EXPORT_SYMBOL(snd_uda1380_close_stream);
EXPORT_SYMBOL(snd_uda1380_suspend);
EXPORT_SYMBOL(snd_uda1380_resume);
EXPORT_SYMBOL(snd_uda1380_hp_connected);
EXPORT_SYMBOL(snd_uda1380_hp_detected);

MODULE_AUTHOR("Giorgio Padrin");
MODULE_DESCRIPTION("Audio support for codec Philips UDA1380");
MODULE_LICENSE("GPL");

/* end {{ Module }} */
