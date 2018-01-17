/*
 * sigsuspend.c
 */

#include <signal.h>
#include <sys/syscall.h>
#include <klibc/sysconfig.h>
#include <klibc/havesyscall.h>

#if _KLIBC_USE_RT_SIG

__extern int __rt_sigsuspend(const sigset_t *, size_t);

int sigsuspend(const sigset_t * mask)
{
	return __rt_sigsuspend(mask, sizeof *mask);
}

#else

extern int __sigsuspend_s(sigset_t);
extern int __sigsuspend_xxs(int, int, sigset_t);

int
sigsuspend(const sigset_t *maskp)
{
#ifdef _KLIBC_HAVE_SYSCALL___sigsuspend_s
	return __sigsuspend_s(*maskp);
#elif defined(_KLIBC_HAVE_SYSCALL___sigsuspend_xxs)
	return __sigsuspend_xxs(0, 0, *maskp);
#else
# error "Unknown sigsuspend implementation"
#endif
}

#endif
