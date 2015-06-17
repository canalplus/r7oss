#ifndef __LATENCY_COMPAT_H
#define __LATENCY_COMPAT_H

static inline void set_acceptable_latency(char *identifier, int usecs) {}
static inline void modify_acceptable_latency(char *identifier, int usecs) {}
static inline void remove_acceptable_latency(char *identifier) {}
static inline void synchronize_acceptable_latency(void) {}
static inline int system_latency_constraint(void) {return 0;}

static inline int register_latency_notifier(struct notifier_block * nb) {return 0;}
static inline int unregister_latency_notifier(struct notifier_block * nb) {return 0;}

#define INFINITE_LATENCY 1000000

#endif
