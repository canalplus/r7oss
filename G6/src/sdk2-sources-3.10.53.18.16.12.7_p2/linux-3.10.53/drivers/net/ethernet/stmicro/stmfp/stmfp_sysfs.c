/*******************************************************************************
  This file  defines the sysfs for ST fastpath on-chip Ethernet controllers.
	Copyright(C) 2009-2014 STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Manish Rathi<manish.rathi@st.com>
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/device.h>

#include <linux/module.h>

#include "stmfp_main.h"

static ssize_t stmfp_show_wlan0_cnt(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	struct fpif_grp *fpgrp = priv->fpgrp;

	return sprintf(buf, "%d\n", fpgrp->wlan0_cnt);
}

static ssize_t stmfp_show_wlan1_cnt(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	struct fpif_grp *fpgrp = priv->fpgrp;

	return sprintf(buf, "%d\n", fpgrp->wlan1_cnt);
}

static ssize_t stmfp_show_wlan0(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	struct fpif_grp *fpgrp = priv->fpgrp;

	return snprintf(buf, sizeof(fpgrp->wlan0), "%s\n", fpgrp->wlan0);
}

static ssize_t stmfp_set_wlan0(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (count >= IFNAMSIZ) {
		pr_err("device name is too long\n");
		return 0;
	}
	strncpy(fpgrp->wlan0, buf, count - 1);

	return count;
}

static ssize_t stmfp_show_wlan1(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	struct fpif_grp *fpgrp = priv->fpgrp;

	return snprintf(buf, sizeof(fpgrp->wlan1), "%s\n", fpgrp->wlan1);
}

static ssize_t stmfp_set_wlan1(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (count >= IFNAMSIZ) {
		pr_err("device name is too long\n");
		return 0;
	}
	strncpy(fpgrp->wlan1, buf, count - 1);

	return count;
}

static ssize_t stmfp_show_stand_cm(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "%s\n", priv->stand_cm_en ? "on" : "off");
}

static ssize_t stmfp_set_stand_cm(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct fpif_priv *priv = netdev_priv(to_net_dev(dev));
	int new_setting = 0;
	unsigned long flags;

	/* Find out the new setting */
	if (!strncmp("on", buf, count - 1) || !strncmp("1", buf, count - 1))
		new_setting = 1;
	else if (!strncmp("off", buf, count - 1) ||
		 !strncmp("0", buf, count - 1))
		new_setting = 0;
	else
		return count;

	if (new_setting == priv->stand_cm_en)
		return count;

	local_irq_save(flags);

	priv->stand_cm_en = new_setting;
	if (new_setting) {
		init_tcam_standalonecm(priv->fpgrp->base);
		writel(PKTLEN_ERR | MALFORM_PKT | IP_SRC_DHCP | IP_DST_CLASS_E |
				SAMEIP_SRC_DEST | IPSRC_LOOP | TTL0_ERR |
				IPV4_BAD_HLEN | MCAST_MAC_SRC,
				priv->fpgrp->base + FILT_BADF);
		writel(PKTLEN_ERR | MALFORM_PKT | IP_SRC_DHCP | IP_DST_CLASS_E |
				SAMEIP_SRC_DEST | IPSRC_LOOP | TTL0_ERR |
				IPV4_BAD_HLEN | MCAST_MAC_SRC,
				priv->fpgrp->base + FILT_BADF_DROP);
		writel(MISC_DEFRAG_EN, priv->fpgrp->base + FP_MISC);
		writel(DEFRAG_REPLACE , priv->fpgrp->base + FP_DEFRAG_CNTRL);
	} else {
		stmfp_hwinit_badf(priv->fpgrp);
		remove_tcam_standalonecm(priv->fpgrp->base);
	}
	local_irq_restore(flags);

	return count;
}

static DEVICE_ATTR(stand_cm, 0644, stmfp_show_stand_cm, stmfp_set_stand_cm);
static DEVICE_ATTR(wlan0_name, 0644, stmfp_show_wlan0, stmfp_set_wlan0);
static DEVICE_ATTR(wlan1_name, 0644, stmfp_show_wlan1, stmfp_set_wlan1);
static DEVICE_ATTR(wlan0_cnt, 0444, stmfp_show_wlan0_cnt, NULL);
static DEVICE_ATTR(wlan1_cnt, 0444, stmfp_show_wlan1_cnt, NULL);

void stmfp_init_sysfs(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct fpif_grp *fpgrp = priv->fpgrp;
	int rc;

	/* Initialize the default values */
	priv->stand_cm_en = 0;
	strcpy(fpgrp->wlan0, "wlan0");
	strcpy(fpgrp->wlan1, "wlan1");
	fpgrp->wlan0_cnt = 0;
	fpgrp->wlan1_cnt = 0;

	/* Create our sysfs files */
	rc = device_create_file(&dev->dev, &dev_attr_stand_cm);
	if (rc)
		dev_err(&dev->dev, "Err create stmfp sysfs file stand_cm\n");

	rc = device_create_file(&dev->dev, &dev_attr_wlan0_name);
	if (rc)
		dev_err(&dev->dev, "Err create stmfp sysfs file wlan0\n");

	rc = device_create_file(&dev->dev, &dev_attr_wlan1_name);
	if (rc)
		dev_err(&dev->dev, "Err create stmfp sysfs file wlan1\n");

	rc = device_create_file(&dev->dev, &dev_attr_wlan0_cnt);
	if (rc)
		dev_err(&dev->dev, "Err create stmfp sysfs file wlan0_cnt\n");

	rc = device_create_file(&dev->dev, &dev_attr_wlan1_cnt);
	if (rc)
		dev_err(&dev->dev, "Err create stmfp sysfs file wlan1_cnt\n");
}
