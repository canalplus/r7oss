#ifndef MXL101SF_H
#define MXL101SF_H

#include <linux/dvb/frontend.h>

struct mxl101sf_config {
	/* the tuners i2c address / adapter */
	u8			tuner_address;
	struct i2c_adapter*	tuner_i2c;
	int			serial_not_parallel;

	/* use this demod as master or slave*/
	u8			type;
};

extern struct dvb_frontend* mxl101sf_attach(const struct mxl101sf_config* config,
					   struct i2c_adapter* i2c,u8 demod_address);

extern u8 get_demod_addr(void);
extern struct i2c_adapter * get_i2c_adapter(void);
extern void set_i2c_adapter(struct i2c_adapter *i2c);
#endif /* MXL101SF_H */
