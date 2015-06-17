#ifndef _TRACE_KPTRACE_H
#define _TRACE_KPTRACE_H
/*
 *  KPTrace - KProbes-based tracing
 *  include/trace/kptrace.h
 *
 * GENERIC KPTRACE HEADER FILE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2007, 2012
 *
 * 2007-July    Created by Chris Smith <chris.smith@st.com>
 * 2008-August  kpprintf added by Chris Smith <chris.smith@st.com>
 */

#ifdef CONFIG_KPTRACE

#include <linux/mtt/mtt.h>

/* LEGACY KPTRACE API */

/* Mark a particular point in the code as "interesting" in the kptrace log */
void kptrace_mark(void);

/* Write a string to the kptrace log */
void kptrace_write_record(const char *buf);

/* Allow printf-style records to be added. Note that kptrace_write_record
 * is a lighter alternative when no formatting is required. */
void kpprintf(char *fmt, ...);

/* Stop logging trace records until kptrace_restart() is called */
void kptrace_pause(void);

/* Restart logging of trace records after a kptrace_pause() */
void kptrace_restart(void);

#endif
#endif

