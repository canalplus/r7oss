/*
 *   STMicroelectronics audio DAC buffer (amplifier) driver
 *
 *   Copyright (c) 2005-2014 STMicroelectronics Limited
 *
 *   Author: Guillaume Kouadio Carry <guillaume.kouadio-carry@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include "common.h"

static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);

/* Hardware-related definitions */

/* st6440 audio i2c registers*/
#define SUB_ADDR 0
#define WR0 1
#define WR1 2

/* Content of registers at power-on reset */
#define WR0_POR 0
#define WR1_POR 0
#define SUB_ADDR_POR 0

/* st6440 audio mute control is at WR1 b7 */
#define AUDIO_OUT_MUTE_SHIFT 7
#define AUDIO_OUT_MUTE_MASK (1 << AUDIO_OUT_MUTE_SHIFT)
#define AUDIO_OUT_ACTIVE (0 << AUDIO_OUT_MUTE_SHIFT)
#define AUDIO_OUT_MUTED (1 << AUDIO_OUT_MUTE_SHIFT)

/*  st6440 audio standby mode is at WR1 b2 */
#define AUDIO_OUT_STANDBY_SHIFT 2
#define AUDIO_OUT_STANDBY_MASK (1 << AUDIO_OUT_STANDBY_SHIFT)
#define AUDIO_OUT_ON (0 << AUDIO_OUT_STANDBY_SHIFT)
#define AUDIO_OUT_OFF (1 << AUDIO_OUT_STANDBY_SHIFT)

/* Audio gain at WR0 b6,b5,b4 */
#define AUDIO_GAIN_SHIFT 4
#define AUDIO_GAIN_MASK (7<<AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_0 (0 << AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_6 (1 << AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_12 (2 << AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_16 (3 << AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_20 (4 << AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_24 (5 << AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_26 (6 <<  AUDIO_GAIN_SHIFT)
#define AUDIO_GAIN_28 (7 << AUDIO_GAIN_SHIFT)
#define AUDIO_OUT_DEFAULT_GAIN 5

#define AUDIO_NUM_GAIN 8
#define GAIN_LEVEL 10

struct audio_buffer_gain {
	int gain;
	char name[GAIN_LEVEL];
};

static const struct audio_buffer_gain audio_gain_value[AUDIO_NUM_GAIN] = {
	{ AUDIO_GAIN_0,  "0 = 0db  "},
	{ AUDIO_GAIN_6,  "1 = +6dB "},
	{ AUDIO_GAIN_12, "2 = +12dB"},
	{ AUDIO_GAIN_16, "3 = +16dB"},
	{ AUDIO_GAIN_20, "4 = +20dB"},
	{ AUDIO_GAIN_24, "5 = +24dB"},
	{ AUDIO_GAIN_26, "6 = +26dB"},
	{ AUDIO_GAIN_28, "7 = +28dB"},
};

/* Each i2c transfer has 3 bytes : 1 sub-address and 2 data */
#define PAGE_LEN 3

/* Internal st6440 instance structure */
struct dac_buffer_data {
	struct device *dev;
	struct snd_stm_conv_converter *converter;
	const char *bus_id;
	struct device_node *source_bus_id;
	const char *source_bus_id_description;
	int card_device;
	int enabled;
	int muted;
	int gain_num;
	int gain;
	unsigned char buf[PAGE_LEN];
	struct i2c_client *i2c_client;
	snd_stm_magic_field;
};

static int dac_buffer_set_enabled(int enabled, void *priv)
{
	struct dac_buffer_data *conv = priv;
	int ret = 0;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* return if already enabled */
	if (enabled == conv->enabled)
		return 0;

	conv->buf[WR1] &= ~AUDIO_OUT_STANDBY_MASK;
	if (enabled)
		conv->buf[WR1] |= AUDIO_OUT_ON;
	else
		conv->buf[WR1] |= AUDIO_OUT_OFF;

	ret = i2c_master_send(conv->i2c_client, conv->buf, PAGE_LEN);

	if (ret != PAGE_LEN) {
		dev_err(conv->dev, "%s:failed:%d\n", __func__, ret);
		return ret;
	}

	conv->enabled = enabled;
	return 0;
}

static int dac_buffer_set_muted(int muted, void *priv)
{
	struct dac_buffer_data *conv = priv;
	int ret = 0;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/*return if already muted*/
	if (muted == conv->muted)
		return 0;

	conv->buf[WR0] &= ~(AUDIO_OUT_MUTE_MASK);
	if (muted == 1)
		conv->buf[WR0] |= AUDIO_OUT_MUTED;
	else
		conv->buf[WR0] |= AUDIO_OUT_ACTIVE;

	ret = i2c_master_send(conv->i2c_client, conv->buf, PAGE_LEN);

	if (ret != PAGE_LEN) {
		dev_err(conv->dev, "%s:failed:%d\n", __func__, ret);
		return ret;
	}

	conv->muted = muted;
	return 0;
}

static int dac_buffer_set_gain(int gain, void *priv)
{
	struct dac_buffer_data *conv = priv;
	int ret;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	conv->buf[WR0] &= ~AUDIO_GAIN_MASK;
	conv->buf[WR0] |= (gain & (AUDIO_GAIN_MASK));

	ret = i2c_master_send(conv->i2c_client, conv->buf, PAGE_LEN);

	if (ret != PAGE_LEN) {
		dev_err(conv->dev, "%s:failed:%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

/* ALSA low-level device implementation */

static int dac_buffer_setting(struct dac_buffer_data *conv)
{
	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* Put the buffer into mute */
	dac_buffer_set_muted(1,  conv);

	/* set output level to default*/
	dac_buffer_set_gain(conv->gain, conv);

	/*Take the buffer out of standby */
	dac_buffer_set_enabled(1, conv);

	/* unmute the buffer */
	dac_buffer_set_muted(0,  conv);

	return 0;
}

/* Driver routines */

static int dac_buffer_parse_dt(struct i2c_client *client,
		struct dac_buffer_data *conv)
{
	struct device_node *pnode = client->dev.of_node;
	int val;

	/* get source_bus_id node */
	val = get_property_hdl(&client->dev, pnode, "st,source-bus-id", 0);
	if (!val) {
		dev_err(&client->dev, "unable to find DT source-bus-id node");
		return -1;
	}
	conv->source_bus_id = of_find_node_by_phandle(val);
	of_property_read_string(conv->source_bus_id, "description",
				&conv->source_bus_id_description);

	/* get source bus card device*/
	of_property_read_u32(conv->source_bus_id, "card-device",
			&conv->card_device);

	/* get DAC buffer gain number */
	of_property_read_u32(pnode, "st,buffer-gain", &conv->gain_num);

	return 0;
}

static int dac_buffer_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct dac_buffer_data *conv;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	BUG_ON(!card);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&client->dev));

	/* Allocate driver structure */
	conv = devm_kzalloc(&client->dev, sizeof(*conv), GFP_KERNEL);
	if (!conv) {
		dev_err(&client->dev, "Failed to allocate driver structure");
		return -ENOMEM;
	}

	snd_stm_magic_set(conv);
	conv->dev = &client->dev;
	conv->bus_id = dev_name(&client->dev);
	conv->i2c_client = client;

	conv->buf[SUB_ADDR] = SUB_ADDR_POR;
	conv->buf[WR0] = WR0_POR;
	conv->buf[WR1] = WR1_POR;

	/* Get resources */
	if (!client->dev.of_node) {
		dev_err(conv->dev, "device not in dt");
		return -EINVAL;
	}

	if (dac_buffer_parse_dt(client, conv)) {
		dev_err(conv->dev, "invalid dt");
		return -EINVAL;
	}

	/* Ensure the gain number is correct */
	if (conv->gain_num >= AUDIO_NUM_GAIN) {
		dev_warn(conv->dev,
			 "Invalid audio dac gain number - Using the default");
		conv->gain_num = AUDIO_OUT_DEFAULT_GAIN;
	}

	conv->gain = audio_gain_value[conv->gain_num].gain;
	dev_info(conv->dev, "Dac buffer gain set to %d : %s",
		 conv->gain_num,
		 audio_gain_value[conv->gain_num].name);

	/* Ensure valid bus id */
	if (!conv->source_bus_id) {
		dev_err(conv->dev, "Invalid source bus id!");
		return -EINVAL;
	}

	dac_buffer_setting(conv);

	dev_info(&client->dev, "Audio buffer st6440 probed\n");

	return 0;
}

static int dac_buffer_remove(struct i2c_client *client)
{
	struct dac_buffer_data *conv = i2c_get_clientdata(client);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* Put the buffer into mute */
	dac_buffer_set_muted(AUDIO_OUT_MUTED,  conv->i2c_client);

	/*put the buffer into standby */
	dac_buffer_set_enabled(AUDIO_OUT_OFF, conv->i2c_client);

	snd_stm_magic_clear(conv);

	return 0;
}

static const struct i2c_device_id dac_buffer_i2c_ids[] = {
	{ "snd_dac_buf_st6440", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, dac_buffer_i2c_ids);

static struct i2c_driver dac_buffer_driver = {
	.driver = {
		.name = "snd_dac_buffer",
		.owner = THIS_MODULE,
	},
	.probe = dac_buffer_probe,
	.remove = dac_buffer_remove,
	.id_table = dac_buffer_i2c_ids,
};

static int dac_buffer_init(void)
{
	return i2c_add_driver(&dac_buffer_driver);
}
module_init(dac_buffer_init);

static void dac_buffer_exit(void)
{
	i2c_del_driver(&dac_buffer_driver);
}
module_exit(dac_buffer_exit);

MODULE_AUTHOR("Guillaume Kouadio Carry <guillaume.kouadio-carry@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st6440 audio buffer driver");
MODULE_LICENSE("GPL");
