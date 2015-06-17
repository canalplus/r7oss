/* pthread_spin_trylock implementation based on gUSA kernel support
 *
 * Copyright (C) 2010 STMicroelectronics, Ltd
 *
 * Author(s): Carmelo Amoroso <carmelo.amoroso@st.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball
 */

#include <pthread.h>
#include <pthread-errnos.h>
#include <atomic.h>

int pthread_spin_trylock (pthread_spinlock_t *lock)
{
	return atomic_compare_and_exchange_bool_acq(lock, 1, 0) ? EBUSY : 0;
}
