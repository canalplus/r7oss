/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author:	Sudeep Biswas		<sudeep.biswas@st.com>
 *		Francesco M. Virlinzi	<francesco.virlinzi@st.com>
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#ifndef __SUSPEND_INTERNAL__
#define __SUSPEND_INTERNAL__

/* Common externs put here to avoid in c file and cosequent checkpatch error */
extern long VA2PA;

static inline
void populate_suspend_table_entry(struct sti_suspend_table *entry,
				  long *entry_tbl, long *exit_tbl,
				  int size_entry, int size_exit,
				  unsigned long base_address)
{
	entry->enter = entry_tbl;
	entry->enter_size = size_entry;
	entry->exit = exit_tbl;
	entry->exit_size = size_exit;
	entry->base_address = base_address;
}

/* Maximum number of suspend table entries with entry and exit procedures */
#define MAX_SUSPEND_TABLE_SIZE 6

/* HPS specific call declarations */
void sti_hps_exec(struct sti_suspend_eram_data *pa_eram_data,
		  unsigned long va_2_pa);

int sti_hps_on_eram(void);
extern unsigned long sti_hps_on_eram_sz;

/* CPS specific call declarations */
int sti_cps_exec(unsigned long);

int sti_cps_on_eram(void);
extern unsigned long sti_cps_on_eram_sz;

/* Function that is executed on all cores after a CPS wakeup */
void sti_defrost_kernel(void);

#endif /* __SUSPEND_INTERNAL__ */

