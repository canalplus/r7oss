/*
 *   STMicroelectronics SOC audio clocks wrapper
 *
 *   Copyright (c) 2010 STMicroelectronics Limited
 *
 *   Authors: Pawel Moll <pawel.moll@st.com>
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
 *
 */

#include <asm/div64.h>
#include <linux/stm/clk.h>
#include <sound/driver.h>
#include <sound/core.h>

#include "common.h"



extern int snd_stm_debug_level;



#define to_snd_stm_clk(clk) \
		container_of(clk, struct snd_stm_clk, clk)

struct snd_stm_clk_parent {
	struct list_head list;

	struct clk *parent;

	int children_count, ref_count;
	unsigned int rate_set:1; /* Is the rate set yet? */
};

struct snd_stm_clk {
	struct clk clk;

	struct snd_stm_clk_parent *parent;

	int adjustment; /* Difference from the nominal rate, in ppm */
	unsigned int enabled:1;

	snd_stm_magic_field;
};



/* Adjustment ALSA control */

static int snd_stm_clk_adjustment_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = -1000000;
	uinfo->value.integer.max = 1000000;

	return 0;
}

static int snd_stm_clk_adjustment_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_clk *snd_stm_clk = snd_kcontrol_chip(kcontrol);

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
			__func__, kcontrol, ucontrol);

	BUG_ON(!snd_stm_clk);
	BUG_ON(!snd_stm_magic_valid(snd_stm_clk));

	ucontrol->value.integer.value[0] = snd_stm_clk->adjustment;

	return 0;
}

static int snd_stm_clk_adjustment_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_clk *snd_stm_clk = snd_kcontrol_chip(kcontrol);
	int old_adjustement;

	snd_stm_printd(1, "%s(kcontrol=0x%p, ucontrol=0x%p)\n",
			__func__, kcontrol, ucontrol);

	BUG_ON(!snd_stm_clk);
	BUG_ON(!snd_stm_magic_valid(snd_stm_clk));

	old_adjustement = snd_stm_clk->adjustment;
	snd_stm_clk->adjustment = ucontrol->value.integer.value[0];

	if (snd_stm_clk->enabled && clk_set_rate(&snd_stm_clk->clk,
			snd_stm_clk->clk.nominal_rate) < 0)
		return -EINVAL;

	return old_adjustement != snd_stm_clk->adjustment;
}

static struct snd_kcontrol_new snd_stm_clk_adjustment_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "PCM Playback Oversampling Freq. Adjustment",
	.info = snd_stm_clk_adjustment_info,
	.get = snd_stm_clk_adjustment_get,
	.put = snd_stm_clk_adjustment_put,
};



/* Clock interface */

static LIST_HEAD(snd_stm_clk_parents);
static spinlock_t snd_stm_clk_parents_lock = SPIN_LOCK_UNLOCKED;

int snd_stm_clk_enable(struct clk *clk)
{
	int result = -EINVAL;
	struct snd_stm_clk *snd_stm_clk;

	snd_stm_printd(1, "%s(clk=%p)\n", __func__, clk);

	BUG_ON(!clk);
	snd_stm_clk = to_snd_stm_clk(clk);
	BUG_ON(!snd_stm_magic_valid(snd_stm_clk));

	spin_lock(&snd_stm_clk_parents_lock);

	if (snd_stm_clk->parent->ref_count++ == 0) {
		result = clk_enable(clk->parent);
		if (result == 0)
			snd_stm_clk->enabled = 1;
		else
			snd_stm_clk->parent->ref_count--;
	}

	BUG_ON(snd_stm_clk->parent->ref_count >
			snd_stm_clk->parent->children_count);

	spin_unlock(&snd_stm_clk_parents_lock);

	return result;
}

int snd_stm_clk_disable(struct clk *clk)
{
	struct snd_stm_clk *snd_stm_clk;

	snd_stm_printd(1, "%s(clk=%p)\n", __func__, clk);

	BUG_ON(!clk);
	snd_stm_clk = to_snd_stm_clk(clk);
	BUG_ON(!snd_stm_magic_valid(snd_stm_clk));

	spin_lock(&snd_stm_clk_parents_lock);

	if (--snd_stm_clk->parent->ref_count == 0) {
/*              Removed as STMFB doesn't use the clock framework.
 *		With this in place we loose the HDMI clock once we 
 *              have played audio and shut down which means no
 *              display mode changes etc. */
/*		clk_disable(clk->parent); */

		snd_stm_clk->enabled = 0;
		snd_stm_clk->parent->rate_set = 0;
	}
	BUG_ON(snd_stm_clk->parent->ref_count < 0);

	spin_unlock(&snd_stm_clk_parents_lock);

	return 0;
}

int snd_stm_clk_set_rate(struct clk *clk, unsigned long rate)
{
	struct snd_stm_clk *snd_stm_clk;
	int rate_adjusted, rate_achieved;
	int delta;

	snd_stm_printd(1, "%s(clk=%p, rate=%lu)\n", __func__, clk, rate);

	BUG_ON(!clk);
	snd_stm_clk = to_snd_stm_clk(clk);
	BUG_ON(!snd_stm_magic_valid(snd_stm_clk));

	/* User must enable the clock first */
	if (!snd_stm_clk->enabled)
		return -EAGAIN;

	spin_lock(&snd_stm_clk_parents_lock);

	/* Has the rate been set yet? */
	if (snd_stm_clk->parent->rate_set) {
		spin_unlock(&snd_stm_clk_parents_lock);

		/* It's ok to "reuse" the rate, but it must not be changed */
		return rate == clk->nominal_rate ? 0 : -EBUSY;
	}

	snd_stm_clk->parent->rate_set = 1;

	spin_unlock(&snd_stm_clk_parents_lock);

	/*             a
	 * F = f + --------- * f = f + d
	 *          1000000
	 *
	 *         a
	 * d = --------- * f
	 *      1000000
	 *
	 * where:
	 *   f - nominal rate
	 *   a - adjustment in ppm (parts per milion)
	 *   F - rate to be set in synthesizer
	 *   d - delta (difference) between f and F
	 */
	if (snd_stm_clk->adjustment < 0) {
		/* div64_64 operates on unsigned values... */
		delta = -1;
		snd_stm_clk->adjustment = -snd_stm_clk->adjustment;
	} else {
		delta = 1;
	}
	/* 500000 ppm is 0.5, which is used to round up values */
	delta *= (int)div64_64((uint64_t)rate *
			(uint64_t)snd_stm_clk->adjustment + 500000,
			1000000);
	rate_adjusted = rate + delta;

	snd_stm_printd(1, "Setting clock '%s' to rate %d.\n",
			clk->parent->name, rate_adjusted);
	if (clk_set_rate(snd_stm_clk->clk.parent, rate_adjusted) < 0) {
		snd_stm_printe("Failed to set rate %d on clock '%s'!\n",
				rate_adjusted, clk->parent->name);
		return -EINVAL;
	}

	rate_achieved = clk_get_rate(clk->parent);
	snd_stm_printd(1, "Achieved rate %d.\n", rate_achieved);
	BUG_ON(rate_achieved < rate - 1000000);
	BUG_ON(rate_achieved > rate + 1000000);
	delta = rate_achieved - rate;
	if (delta < 0) {
		/* div64_64 operates on unsigned values... */
		delta = -delta;
		snd_stm_clk->adjustment = -1;
	} else {
		snd_stm_clk->adjustment = 1;
	}
	/* frequency/2 is added to round up result */
	snd_stm_clk->adjustment *= (int)div64_64((uint64_t)delta * 1000000 +
			rate / 2, rate);

	clk->nominal_rate = rate;
	clk->rate = rate_achieved;

	return 0;
}

static struct clk_ops snd_stm_clk_ops = {
	.enable = snd_stm_clk_enable,
	.disable = snd_stm_clk_disable,
	.set_rate = snd_stm_clk_set_rate,
};

struct clk *snd_stm_clk_get(struct device *dev, const char *id,
		struct snd_card *card, int card_device)
{
	struct snd_stm_clk *snd_stm_clk;
	struct snd_stm_clk_parent *snd_stm_clk_parent;

	snd_stm_printd(0, "%s(dev=%p, dev->bus_id='%s', id='%s')\n",
			__func__, dev, dev->bus_id, id);

	snd_stm_clk = kzalloc(sizeof(*snd_stm_clk), GFP_KERNEL);
	if (!snd_stm_clk) {
		snd_stm_printe("No memory for '%s'('%s') clock!\n",
				dev->bus_id, id);
		goto error_kzalloc_clk;
	}
	snd_stm_magic_set(snd_stm_clk);

	snd_stm_clk->clk.ops = &snd_stm_clk_ops;
	snd_stm_clk->clk.name = dev->bus_id;

	/* Get the parent clock */

	snd_stm_clk->clk.parent = clk_get(dev, id);
	if  (!snd_stm_clk->clk.parent || IS_ERR(snd_stm_clk->clk.parent)) {
		snd_stm_printe("Can't get '%s' clock ('%s's parent)!\n",
				id, dev->bus_id);
		goto error_clk_get;
	}

	/* Find the parent clock description... */

	spin_lock(&snd_stm_clk_parents_lock);

	list_for_each_entry(snd_stm_clk_parent, &snd_stm_clk_parents, list) {
		if (snd_stm_clk_parent->parent == snd_stm_clk->clk.parent) {
			snd_stm_clk->parent = snd_stm_clk_parent;
			snd_stm_clk_parent->children_count++;
			break;
		}
	}

	spin_unlock(&snd_stm_clk_parents_lock);

	/* ... or allocate it now */

	if (!snd_stm_clk->parent) {
		snd_stm_clk->parent = kzalloc(sizeof(*snd_stm_clk->parent),
				GFP_KERNEL);
		if (!snd_stm_clk->parent) {
			snd_stm_printe("No memory for '%s'('%s') parent!\n",
					dev->bus_id, id);
			goto error_kzalloc_parent;
		}
		snd_stm_clk->parent->parent = snd_stm_clk->clk.parent;
		snd_stm_clk->parent->children_count = 1;

		spin_lock(&snd_stm_clk_parents_lock);
		list_add_tail(&snd_stm_clk->parent->list, &snd_stm_clk_parents);
		spin_unlock(&snd_stm_clk_parents_lock);
	}

	/* Register the clock "wrapper" */

	if (clk_register(&snd_stm_clk->clk) < 0) {
		snd_stm_printe("Failed to register '%s'('%s')\n",
				dev->bus_id, id);
		goto error_clk_register;
	}

	/* Rate adjustment ALSA control */

	snd_stm_clk_adjustment_ctl.device = card_device;
	if (snd_ctl_add(card, snd_ctl_new1(&snd_stm_clk_adjustment_ctl,
			snd_stm_clk)) < 0) {
		snd_stm_printe("Failed to add '%s'('%s') clock ALSA control!\n",
				dev->bus_id, id);
		goto error_snd_ctl_add;
	}
	snd_stm_clk_adjustment_ctl.index++;

	return &snd_stm_clk->clk;

error_snd_ctl_add:
	clk_unregister(&snd_stm_clk->clk);
error_clk_register:
	spin_lock(&snd_stm_clk_parents_lock);
	if (--snd_stm_clk->parent->children_count == 0)
		kfree(snd_stm_clk->parent);
	spin_unlock(&snd_stm_clk_parents_lock);
error_kzalloc_parent:
	clk_put(snd_stm_clk->clk.parent);
error_clk_get:
	snd_stm_magic_clear(snd_stm_clk);
	kfree(snd_stm_clk);
error_kzalloc_clk:
	return NULL;
}

void snd_stm_clk_put(struct clk *clk)
{
	struct snd_stm_clk *snd_stm_clk;

	snd_stm_printd(0, "%s(clk=%p)\n", __func__, clk);

	BUG_ON(!clk);
	snd_stm_clk = to_snd_stm_clk(clk);
	BUG_ON(!snd_stm_magic_valid(snd_stm_clk));

	clk_unregister(clk);
	spin_lock(&snd_stm_clk_parents_lock);
	if (--snd_stm_clk->parent->children_count == 0)
		kfree(snd_stm_clk->parent);
	spin_unlock(&snd_stm_clk_parents_lock);
	clk_put(clk->parent);

	snd_stm_magic_clear(snd_stm_clk);
	kfree(snd_stm_clk);
}
