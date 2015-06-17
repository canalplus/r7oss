#ifndef __LOWPOWER_IR_WAKEUP_H__
#define __LOWPOWER_IR_WAKEUP_H__

#define LPIR_DATA_SIZE		256
//#define LPIR_DATA_DUMP

extern unsigned long lpir_data[LPIR_DATA_SIZE];


#if defined(LPIR_DATA_DUMP)
extern void lpir_data_dump(void);
#else
static inline void lpir_data_dump(void) {};
#endif /* LPIR_DATA_DUMP */

unsigned int lpir_check_keys(void);

#endif /* __LOWPOWER_IR_WAKEUP_H__ */
