/***********************************************************************
 *
 * File: private/include/thread_vib_settings.h
 * Copyright (c) 2007-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef THREAD_VIB_SETTINGS_H
#define THREAD_VIB_SETTINGS_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/* Driver threads */

#define THREAD_VIB_HDMIRXMONIT_POLICY    SCHED_NORMAL
#define THREAD_VIB_HDMIRXMONIT_PRIORITY  0

#define THREAD_VIB_HDP_POLICY            SCHED_NORMAL
#define THREAD_VIB_HDP_PRIORITY          0

#define THREAD_VIB_HDMID_POLICY          SCHED_NORMAL
#define THREAD_VIB_HDMID_PRIORITY        0

#define THREAD_VIB_SCALER_POLICY         SCHED_RR
#define THREAD_VIB_SCALER_PRIORITY       80

#if defined(DEBUG)
#define THREAD_VIB_ASYNCPRINT_POLICY     SCHED_NORMAL
#define THREAD_VIB_ASYNCPRINT_PRIORITY   0
#endif


/* Test driver threads */

#define THREAD_VIB_TESTCAPTURE_POLICY    SCHED_RR
#define THREAD_VIB_TESTCAPTURE_PRIORITY  40

#define THREAD_VIB_TESTCECREAD_POLICY    SCHED_RR
#define THREAD_VIB_TESTCECREAD_PRIORITY  52

#define THREAD_VIB_TESTIVP_POLICY        SCHED_NORMAL
#define THREAD_VIB_TESTIVP_PRIORITY      0

#define THREAD_VIB_TESTOVP_POLICY        SCHED_NORMAL
#define THREAD_VIB_TESTOVP_PRIORITY      0

#define THREAD_VIB_TESTFIRMWARE_POLICY   SCHED_NORMAL
#define THREAD_VIB_TESTFIRMWARE_PRIORITY 0




#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* THREAD_VIB_SETTINGS_H */

