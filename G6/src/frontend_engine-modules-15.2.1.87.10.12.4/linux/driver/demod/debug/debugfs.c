/************************************************************************
Copyright (C) 2013 Microelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : debugfs.c
Author :           Deepak G

Debugfs support for FE

Date        Modification                                    Name
----        ------------                                    --------
23-Nov-13   Created                                         Deepak G

************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>/* this is for DebugFS libraries */
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <stm_fe_os.h>
#include <stm_fe.h>
#include <stm_fe_ip.h>
#include <i2c_wrapper.h>
#include <stm_registry.h>
#include <stm_fe_demod.h>
#include <stm_fe_ip.h>
#include <stm_fe_lnb.h>
#include <stm_fe_diseqc.h>
#include "debugfs.h"
#include <stfe_utilities.h>

struct dentry *pdir;
static int stmfe_debugfs_ts_conf(struct seq_file *seq , void *v);

#define DEBUG_ENTRY_OPS(name, ...)                                          \
static int name##_show(struct seq_file *seq, void *v)                       \
{									    \
	return stmfe_debugfs_##name(seq, v);			    \
}									    \
									    \
static int name##_open(struct inode *inode, struct file *file)		    \
{									    \
	return single_open(file, name##_show, inode->i_private);	    \
}

#define DEBUG_ENTRY(n) {			\
	.name = #n,				\
	.fops = {				\
		.owner = THIS_MODULE,		\
		.release = single_release,	\
		.read = seq_read,		\
		.llseek = seq_lseek,		\
		.open = n##_open,		\
	}					\
}

struct debugfs_entry {
	char *name;
	const struct file_operations fops;
};

DEBUG_ENTRY_OPS(ts_conf);

static const struct debugfs_entry pDevice = DEBUG_ENTRY(ts_conf);



/*!
* @brief	dumps ts configuration for specific demod to ts_config
* @param[in]	m       pointer to seq file to which we want to write
*		It also contains pointer to the demod object passed at the time
*		of file creation
* @return	0
*/
static int stmfe_debugfs_ts_conf(struct seq_file *m, void *v)
{
	struct stm_fe_demod_s *priv;
	if (IS_ERR_OR_NULL(m)) {
		pr_err("%s: seq file ptr is NULL\n", __func__);
		return -ENODEV;
	}
	priv = m->private;
	if (IS_ERR_OR_NULL(priv)) {
		pr_err("%s: demod obj priv is NULL\n", __func__);
		return -ENODEV;
	}

	seq_printf(m, "\nTSin ID   = %d\n", priv->config->demux_tsin_id);
	seq_printf(m, "TS Tag      = %d\n", priv->config->ts_tag);
	seq_printf(m, "TS clock    = %d Hz\n", priv->config->ts_clock);
	seq_printf(m, "TS FORMAT   = %d\n", priv->config->ts_out);
	seq_printf(m, "\n\n---------------TS_FORMAT_DETAILS---------------\n");
	seq_printf(m, "\n---------:       Parameters            : yes/no\n");
	seq_printf(m, "TS_FORMAT:  TS_SERIAL_PUNCT_CLOCK      : %d\n",
		(priv->config->ts_out & DEMOD_TS_SERIAL_PUNCT_CLOCK) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SERIAL_CONT_CLOCK       : %d\n",
		(priv->config->ts_out & DEMOD_TS_SERIAL_CONT_CLOCK) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PARALLEL_PUNCT_CLOCK    : %d\n",
		(priv->config->ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_DVBCI_CLOCK             : %d\n",
		(priv->config->ts_out & DEMOD_TS_DVBCI_CLOCK) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_MANUAL_SPEED            : %d\n",
		(priv->config->ts_out & DEMOD_TS_MANUAL_SPEED) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_AUTO_SPEED              : %d\n",
		(priv->config->ts_out & DEMOD_TS_AUTO_SPEED) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_RISINGEDGE_CLOCK        : %d\n",
		(priv->config->ts_out & DEMOD_TS_RISINGEDGE_CLOCK) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_FALLINGEDGE_CLOCK       : %d\n",
		(priv->config->ts_out & DEMOD_TS_FALLINGEDGE_CLOCK) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SYNCBYTE_ON             : %d\n",
		(priv->config->ts_out & DEMOD_TS_SYNCBYTE_ON) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SYNCBYTE_OFF            : %d\n",
		(priv->config->ts_out & DEMOD_TS_SYNCBYTE_OFF) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PARITYBYTES_ON          : %d\n",
		(priv->config->ts_out & DEMOD_TS_PARITYBYTES_ON) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PARITYBYTES_OFF         : %d\n",
		(priv->config->ts_out & DEMOD_TS_PARITYBYTES_OFF) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SWAP_ON                 : %d\n",
		(priv->config->ts_out & DEMOD_TS_SWAP_ON) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SWAP_OFF                : %d\n",
		(priv->config->ts_out & DEMOD_TS_SWAP_OFF) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SMOOTHER_ON             : %d\n",
		(priv->config->ts_out & DEMOD_TS_SMOOTHER_ON) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_SMOOTHER_OFF            : %d\n",
		(priv->config->ts_out & DEMOD_TS_SMOOTHER_OFF) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PORT_0                  : %d\n",
		(priv->config->ts_out & DEMOD_TS_PORT_0) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PORT_1                  : %d\n",
		(priv->config->ts_out & DEMOD_TS_PORT_1) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PORT_2                  : %d\n",
		(priv->config->ts_out & DEMOD_TS_PORT_2) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_PORT_3                  : %d\n",
		(priv->config->ts_out & DEMOD_TS_PORT_3) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_MUX                     : %d\n",
		(priv->config->ts_out & DEMOD_TS_MUX) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  ADC_IN_A                   : %d\n",
		(priv->config->ts_out & DEMOD_ADC_IN_A) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  ADC_IN_B                   : %d\n",
		(priv->config->ts_out & DEMOD_ADC_IN_B) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  ADC_IN_C                   : %d\n",
		(priv->config->ts_out & DEMOD_ADC_IN_C) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_THROUGH_CABLE_CARD      : %d\n",
		(priv->config->ts_out & DEMOD_TS_THROUGH_CABLE_CARD) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_MERGED                  : %d\n",
		(priv->config->ts_out & DEMOD_TS_MERGED) ? 1 : 0);
	seq_printf(m, "TS_FORMAT:  TS_TIMER_TAG               : %d\n",
		(priv->config->ts_out & DEMOD_TS_TIMER_TAG) ? 1 : 0);

	return 0;
}


/*!
* @brief	creates debugfs parent directory for Frontend Engine
* @param[in]	void
* @return	0 on SUCCESS and error on FAILURE.
*/
int stmfe_debugfs_create(void)
{
	int ret = 0;
	pdir = debugfs_create_dir("stm_fe", NULL);
	if (IS_ERR_OR_NULL(pdir)) {
		pr_err("%s: unable to create debugfs dir for FE\n", __func__);
		return -ENODEV;
	}
	return ret;
}

/*!
* @brief	removes debugfs directories recursively.
* @param[in]	dir     pointer to parent directory from where
*		recursive deletion will start
* @return	void
*/
int stmfe_debugfs_remove(void)
{
	int ret = 0;
	pr_info("%s: Removing debugfs directories recursively\n", __func__);
	if (IS_ERR_OR_NULL(pdir)) {
		pr_warn("%s: ptr to directory is NULL\n", __func__);
		ret = -ENODEV;
	}
	debugfs_remove_recursive(pdir);
	return ret;

}

/*!
* @brief	creates sub directory corres to RF object as well as
*		various info files to dump information to user
* @param[in]	priv	stm_fe_demod_s pointer which points to FE object
*		corresponding to which a dedicated dir as well as different
*		info files will be created
* @return	0 on SUCCESS AND eror on FAILURE;
*/
int stmfe_demod_debugfs_create(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	char *name = priv->demod_name;

	if (IS_ERR_OR_NULL(priv)) {
		pr_err("%s: demod obj priv is NULL\n", __func__);
		return -EINVAL;
	}

	priv->demod_dump = debugfs_create_dir(name, pdir);
	if (IS_ERR_OR_NULL(priv->demod_dump)) {
		pr_err("%s: creation of dir for %s failed\n", __func__, name);
		ret = -ENODEV;
	}
	return ret;
}
/*!
* @brief	creates ts_conf file corres to RF object to store info
*               about the ts_configuration  selected for demod.
* @param[in]    priv	stm_fe_demod_s pointer which points to FE object
*               corresponding to which a ts_conf file will be created.
* @return       0 on SUCCESS AND eror on FAILURE;
*/
int stmfe_ts_conf_debugfs_create(struct stm_fe_demod_s *priv)
{
	int ret = 0;
	struct dentry *ts_fmt;
	if (IS_ERR_OR_NULL(priv)) {
		pr_err("%s: demod obj priv is NULL\n", __func__);
		return -EINVAL;
	}

	ts_fmt = debugfs_create_file("ts_config", 0444, priv->demod_dump, priv,
								 &pDevice.fops);
	if (IS_ERR_OR_NULL(ts_fmt)) {
		pr_err("%s: debugfs file for ts_format failed\n", __func__);
		ret = -ENODEV;
	}
	return ret;
}
