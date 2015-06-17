/*---------------------------------------------------------------------------
* /drivers/stm/lpm.c
* Copyright (C) 2010 STMicroelectronics Limited
* Author:
* May be copied or modified under the terms of the GNU General Public
* License.  See linux/COPYING for more information.
*----------------------------------------------------------------------------
*/
#ifndef CONFIG_ARCH_STI
#include <linux/stm/lpm.h>
#include <linux/stm/platform.h>
#endif

/*#include <linux/stm/soc.h>*/
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include "lpm_def.h"

#ifdef CONFIG_STM_LPM_DEBUG
#define lpm_debug pr_info
#else
#define lpm_debug(fmt,arg...);
#endif

#define SBC_ADDRESS   0xFE400000
#define ME 0xFE410000



#define SET_NEC
#define SET_RC5
int test(void)
{


	int i=0;

	struct stm_lpm_ir_keyinfo ir_key[2];
#ifdef SET_NEC
	ir_key[0].ir_id=8;
	ir_key[0].time_period=560;
	ir_key[0].tolerance=30;
	ir_key[0].ir_key.key_index=0x0;

	ir_key[0].ir_key.fifo[0].mark=8960;
	ir_key[0].ir_key.fifo[0].symbol=13440;

	for(i=1;i<9;i++)
	{
		ir_key[0].ir_key.fifo[i].mark=560;
		ir_key[0].ir_key.fifo[i].symbol=1120;

	}

	for(i=9;i<16;i++)
	{
		ir_key[0].ir_key.fifo[i].mark=560*1;
		ir_key[0].ir_key.fifo[i].symbol=560*4;

	}

	for(i=16;i<25;i++)
	{
		ir_key[0].ir_key.fifo[i].mark=560;
		ir_key[0].ir_key.fifo[i].symbol=1120;

	}

	for(i=25;i<32;i++)
	{
		ir_key[0].ir_key.fifo[i].mark=560;
		ir_key[0].ir_key.fifo[i].symbol=560*4;

	}

    ir_key[0].ir_key.fifo[32].mark=560;
    ir_key[0].ir_key.fifo[32].symbol=560*4; // Lille = 560 *2
    ir_key[0].ir_key.fifo[33].mark=560;
    ir_key[0].ir_key.fifo[33].symbol=560*2;


    ir_key[0].ir_key.num_patterns=33; // for Lille = 34 symbols
#endif


#ifdef SET_RC5
	ir_key[1].ir_id=0x7;
	ir_key[1].time_period=900;
	ir_key[1].tolerance=30;
	ir_key[1].ir_key.key_index=0x2;

	ir_key[1].ir_key.fifo[0].mark=900;
	ir_key[1].ir_key.fifo[0].symbol=1800;
       ir_key[1].ir_key.fifo[1].mark=1800;
	ir_key[1].ir_key.fifo[1].symbol=2700;
	ir_key[1].ir_key.fifo[2].mark=900;
	ir_key[1].ir_key.fifo[2].symbol=2700;
	ir_key[1].ir_key.fifo[3].mark=1800;
	ir_key[1].ir_key.fifo[3].symbol=2700;
       ir_key[1].ir_key.fifo[4].mark=900;
	ir_key[1].ir_key.fifo[4].symbol=1800;
	ir_key[1].ir_key.fifo[5].mark=900;
	ir_key[1].ir_key.fifo[5].symbol=1800;
	ir_key[1].ir_key.fifo[6].mark=900;
	ir_key[1].ir_key.fifo[6].symbol=1800;
       ir_key[1].ir_key.fifo[7].mark=900;
	ir_key[1].ir_key.fifo[7].symbol=2700;
	ir_key[1].ir_key.fifo[8].mark=900;
	ir_key[1].ir_key.fifo[8].symbol=1800;
	ir_key[1].ir_key.fifo[9].mark=1800;
	ir_key[1].ir_key.fifo[9].symbol=2700;

	ir_key[1].ir_key.num_patterns=10;


#endif

#ifdef SET_SEJIN
	ir_key[1].ir_id=9;
	ir_key[1].time_period=320;
	ir_key[1].tolerance=30;
	ir_key[1].ir_key.key_index=0x1;

	ir_key[1].ir_key.fifo[0].mark=960;
	ir_key[1].ir_key.fifo[0].symbol=1920;
       ir_key[1].ir_key.fifo[1].mark=640;
	ir_key[1].ir_key.fifo[1].symbol=1920;
	ir_key[1].ir_key.fifo[2].mark=320;
	ir_key[1].ir_key.fifo[2].symbol=960;

	for(i=3;i<10;i++)
	{
		ir_key[1].ir_key.fifo[i].mark=320;
		ir_key[1].ir_key.fifo[i].symbol=1280;
	}
	ir_key[1].ir_key.fifo[10].mark=320;
	ir_key[1].ir_key.fifo[10].symbol=1920;
	ir_key[1].ir_key.fifo[11].mark=320;
	ir_key[1].ir_key.fifo[11].symbol=640;
	for(i=12;i<14;i++)
	{
		ir_key[1].ir_key.fifo[i].mark=320;
		ir_key[1].ir_key.fifo[i].symbol=1280;
	}
	ir_key[1].ir_key.fifo[14].mark=320;
	ir_key[1].ir_key.fifo[14].symbol=1920;
	ir_key[1].ir_key.fifo[15].mark=320;
	ir_key[1].ir_key.fifo[15].symbol=960;

	ir_key[1].ir_key.num_patterns=16;

#endif
	stm_lpm_setup_ir(2, ir_key);
	/*stm_lpm_set_wakeup_device(STM_LPM_MSG_SET_IR);*/

	return 0;
}





static int __init stm_lpm_test_init(void)
{
       int err=0;

	test();
        return err;
}

void __exit stm_lpm_test_exit(void){

}

module_init(stm_lpm_test_init);
module_exit(stm_lpm_test_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("lpm device driver for STMicroelectronics devices");
MODULE_LICENSE("GPL");
