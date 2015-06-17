#ifndef __SEMA_HELPER_H__
#define __SEMA_HELPER_H__

#include <semaphore.h>


static inline unsigned int
__attribute__((warn_unused_result))
sema_get_event_val (sem_t * const sema)
{
  int semval;

  sem_getvalue (sema, &semval);

  return semval;
}

static inline int
__attribute__((warn_unused_result))
sema_wait_event (sem_t * const sema)
{
  D_DEBUG_AT (SEMA_DEBUG_DOMAIN, "waiting on %p (val: %u)\n", sema, sema_get_event_val (sema));

  return sem_wait (sema);
}

static inline int
__attribute__((warn_unused_result))
sema_trywait (sem_t * const sema)
{
  D_DEBUG_AT (SEMA_DEBUG_DOMAIN, "trywait on %p (val: %u)\n", sema, sema_get_event_val (sema));

  return sem_trywait (sema);
}

static inline void
sema_signal_event (sem_t * const sema)
{
  D_DEBUG_AT (SEMA_DEBUG_DOMAIN, "signalling %p (val: %u)\n", sema, sema_get_event_val (sema));

  sem_post (sema);
}

static inline void
sema_init_event (sem_t        * const sema,
                 unsigned int  val)
{
  D_DEBUG_AT (SEMA_DEBUG_DOMAIN, "init'ing sema %p to %u\n", sema, val);

  sem_init (sema, 0, val);
}

static inline void
sema_close_event (sem_t * const sema)
{
  D_DEBUG_AT (SEMA_DEBUG_DOMAIN, "destroying sema %p\n", sema);

  sem_destroy (sema);
}

#endif /* __SEMA_HELPER_H__ */
