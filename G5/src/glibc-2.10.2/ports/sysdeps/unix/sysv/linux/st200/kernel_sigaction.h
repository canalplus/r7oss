
#undef HAVE_SA_RESTORER

/* No definition of old_kernel_sigaction, because there is
   no st200 linux kernel that does not have the rt sigactions,
   so __ASSUME_REALTIME_SIGNALS is never 0.                       */

/* This is the sigaction structure from the Linux 2.5.66 kernel.  */

struct kernel_sigaction {
	__sighandler_t k_sa_handler;
	unsigned long sa_flags;
	sigset_t sa_mask;		/* mask last for extensibility */
};
