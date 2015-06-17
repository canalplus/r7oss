/* --------------------------------------------------------------------
 * stlpc.h
 * --------------------------------------------------------------------
 *
 *  Copyright (C) 2009 : STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License version 2.0 ONLY.  See linux/COPYING for more information.
 *
 */

/*
 * stlpc_write:
 *
 * Sets a new counter
 */
void stlpc_write(unsigned long long tick);

/*
 * stlpc_read:
 *
 * Reads the current counter
 */
unsigned long long stlpc_read(void);

/*
 * stlpc_read_sec:
 *
 * Returns the current counter in seconds
 */
unsigned long stlpc_read_sec(void);


/*
 * stlpc_set:
 *
 * if enable equal zero
 * - disables the lpc (without care on tick)
 * if enable not equal to zero
 * - enable the lpc and
 * - - uses the current lpa value (if tick equal zero)
 * - - sets the lpa to tick (if tick not equal to zero)
 */
int stlpc_set(int enable, unsigned long long tick);
