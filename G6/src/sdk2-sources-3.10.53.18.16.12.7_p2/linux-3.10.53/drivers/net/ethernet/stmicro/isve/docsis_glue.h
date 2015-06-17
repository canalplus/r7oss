/*
 * docsis_glue header file
 *
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This is a driver used for configuring on some ST platforms the Upstream
 * and Downstream queues before opening the Virtual Interface with ISVE
 * driver.
 * Currently it only enables the Downstream queue but it can be extended
 * to support further settings.
 * It is a platform driver and needs to have some platform fields: it also
 * supports DT model.
 * This driver can be used to expose the queue registers via debugFS support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __DOCSIS_GLUE__
#define __DOCSIS_GLUE__

enum docsis_queue_en {
	DOCSIS_USIIM,
	DOCSIS_DSFWD
};

void docsis_en_queue(enum docsis_queue_en type, bool en, int q);

#endif /* __DOCSIS_GLUE__ */
