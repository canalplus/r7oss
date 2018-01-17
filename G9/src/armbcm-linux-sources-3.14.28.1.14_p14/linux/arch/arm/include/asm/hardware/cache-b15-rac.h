#ifndef __ASM_ARM_HARDWARE_CACHE_B15_RAC_H
#define __ASM_ARM_HARDWARE_CACHE_B15_RAC_H

#ifndef __ASSEMBLY__

void b15_flush_kern_cache_all(void);
void b15_flush_kern_cache_louis(void);
void b15_flush_icache_all(void);

#endif

#endif
