#ifndef __DEVICE_DT_H__
#define __DEVICE_DT_H__

int stm_blit_dt_get_pdata(struct platform_device *pdev);

void stm_blit_dt_put_pdata(struct platform_device *pdev);

int stm_blit_dt_init(void);

int stm_blit_dt_exit(void);

#endif /* __DEVICE_DT_H__ */
