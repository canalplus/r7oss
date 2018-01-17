#ifndef __BRCMSTB_SOC_H
#define __BRCMSTB_SOC_H

int brcmstb_biuctrl_init(void);

/*
 * Helper functions for getting family or product id from the
 * SoC driver.
 */
u32 brcmstb_get_family_id(void);
u32 brcmstb_get_product_id(void);

#endif /* __BRCMSTB_SOC_H */
