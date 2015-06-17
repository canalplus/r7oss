/* pthread_spin_lock implementation based on gUSA kernel support
 *
 * Copyright (C) 2010 STMicroelectronics, Ltd
 *
 * Author(s): Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball
 */

#include "pthreadP.h"
#include <atomic.h>

int pthread_spin_lock (pthread_spinlock_t *lock)
{
	/* Atomic cmp&exchange with busy waiting */
	while (atomic_compare_and_exchange_bool_acq(lock, 1, 0));

	/* Lock acquired */
	return 0;
}
