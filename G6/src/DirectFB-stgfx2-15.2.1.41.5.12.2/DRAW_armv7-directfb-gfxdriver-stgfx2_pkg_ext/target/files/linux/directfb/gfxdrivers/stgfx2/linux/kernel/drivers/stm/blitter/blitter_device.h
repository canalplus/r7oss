#ifndef __STM_BLITTER_DEVICE_H__
#define __STM_BLITTER_DEVICE_H__

#include <linux/device.h>
#include "class.h"

#define STM_BLITTER_MAX_DEVICES 8

int stm_blitter_register_device(struct device                     *dev,
				char                              *device_name,
				const struct stm_blitter_file_ops *fops,
				void                              *data);
int stm_blitter_unregister_device(void *data);


int stm_blitter_validate_address(stm_blitter_t *blitter,
				 unsigned long  start,
				 unsigned long  end);


#endif /* __STM_BLITTER_DEVICE_H__ */
