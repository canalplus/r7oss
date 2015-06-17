/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/vibe_ostest/vibe_ostest.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <vibe_os.h>
#include <thread_vib_settings.h>

/*
 * When stress test is acyvated:
 * Please run 'echo 4 4 1 7 > /proc/sys/kernel/printk' before inserting this module
 * as it will hide debug traces and avoid system to sleep while loading the module.
 */
static int stress = 0; /* stress test is off */
module_param(stress, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(stress, "Run stress tests");

/*
 * Default FW loading test will generate an error at the end
 * So please check that vibe_os is generating a WARNING Trace then force
 * fw cache clean-up
 */
static int fw_noerr = 0; /* When '0' FW loading test will keep some fw unloaded */
module_param(fw_noerr, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(fw_noerr, "When '0' FW loading test will keep some fw unloaded");

#define FIRMWARE_NAME_MAX 30
#define KTHREAD_NAME_MAX  32

#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]))

typedef struct vibe_os_firmware_params_s
{
  char            firmware_name[FIRMWARE_NAME_MAX + 1];
  unsigned int    load_times;
  unsigned int    unload_times;
  int             *load_errors;

  int             thread_id;
  char            thread_name[KTHREAD_NAME_MAX];
  unsigned long   thread_desc;
} vibe_os_firmware_params_t;

static vibe_os_firmware_params_t vibe_os_firmwares[] =
{
    { "denc.fw"     , 5 , 5 } /* Try to load/unload a valid fw */
  , { "hdf.fw"      , 5 , 5 } /* Try to load/unload a valid fw */
  , { "unknown1.fw" , 2 , 0 } /* Try to load/unload unknown fw */
  , { "denc.fw"     , 10, 8 } /* Keep 2 denc.fw unloaded */
  , { "unknown2.fw" , 1 , 0 } /* Try to load/unload unknown fw */
  , { "hdf.fw"      , 10, 8 } /* Keep 2 hdf.fw unloaded */
};

static void vibe_os_firmware_load_unload(vibe_os_firmware_params_t *vibe_os_firmware_params)
{
  int Error = 0;
  int thread_id = 0;
  const STMFirmware **firmware_p;

  unsigned long th_delay = stress ? 100 : 1000000;

  BUG_ON(!vibe_os_firmware_params);

  pr_info("Thread %s is running...\n", vibe_os_firmware_params->thread_name);

  if(fw_noerr)
    vibe_os_firmware_params->unload_times = vibe_os_firmware_params->load_times;
  else if(WARN_ON(vibe_os_firmware_params->unload_times > vibe_os_firmware_params->load_times))
    vibe_os_firmware_params->unload_times = vibe_os_firmware_params->load_times;

  /* Allocate memory for test data */
  Error = -ENOMEM;

  firmware_p = vibe_os_allocate_memory(vibe_os_firmware_params->load_times * sizeof (STMFirmware *));
  if(!firmware_p)
    goto exit_th;

  vibe_os_firmware_params->load_errors = vibe_os_allocate_memory(vibe_os_firmware_params->load_times * sizeof(int));
  if(!vibe_os_firmware_params->load_errors)
    goto exit_th;
  vibe_os_zero_memory((void *)vibe_os_firmware_params->load_errors, sizeof(vibe_os_firmware_params->load_errors));

  /* Memory allocation done successfully */
  Error = 0;

  while(!vibe_os_thread_should_stop())
  {
    /* Reset all Firmware pointers */
    vibe_os_zero_memory((void *)firmware_p, (vibe_os_firmware_params->load_times * sizeof (STMFirmware *)));

    for(thread_id = 0; thread_id < vibe_os_firmware_params->load_times; thread_id++)
    {
      pr_debug("Thread %s [%02d] : Loading firmware %s into %p\n", vibe_os_firmware_params->thread_name
                                                                  , thread_id
                                                                  , vibe_os_firmware_params->firmware_name
                                                                  , (void *)&firmware_p[thread_id]);
      vibe_os_firmware_params->load_errors[thread_id] =
              vibe_os_request_firmware( &firmware_p[thread_id]
                                      , vibe_os_firmware_params->firmware_name);
    }

    for(thread_id = 0; thread_id < vibe_os_firmware_params->unload_times; thread_id++)
    {
      if(vibe_os_firmware_params->load_errors[thread_id] == 0)
      {
        pr_debug("Thread %s [%02d] : Unloading firmware %s from %p\n", vibe_os_firmware_params->thread_name
                                                                      , thread_id
                                                                      , vibe_os_firmware_params->firmware_name
                                                                      , (void *)firmware_p[thread_id]);
        vibe_os_release_firmware(firmware_p[thread_id]);
      }
    }

  if(!vibe_os_thread_should_stop())
    vibe_os_stall_execution(th_delay);
  }

exit_th:
  if(!firmware_p)
    vibe_os_free_memory(firmware_p);

  pr_info("Thread %s exited (Err=%d)\n", vibe_os_firmware_params->thread_name, Error);
}

static int __init vibe_ostest_init(void)
{
  int result = 0;
  bool RetOk = true;
  int thread_id = 0;
  VIBE_OS_thread_settings thread_settings = { THREAD_VIB_TESTFIRMWARE_POLICY, THREAD_VIB_TESTFIRMWARE_PRIORITY };

  pr_err("vibe_os tests started :\n");
  pr_err("\t- Stress Mode is %s\n", stress ? "Enabled" : "Disabled");
  pr_err("\t- FW Load test will%sgenerate errors\n", fw_noerr ? " not " : " ");

  for(thread_id = 0; (thread_id < N_ELEMENTS(vibe_os_firmwares) && RetOk); thread_id++)
  {
    vibe_os_firmwares[thread_id].thread_id = thread_id;

    snprintf( vibe_os_firmwares[thread_id].thread_name
            , sizeof(vibe_os_firmwares[thread_id].thread_name)
            , "VIB-TestFW.%d", vibe_os_firmwares[thread_id].thread_id);

    RetOk=vibe_os_create_thread( (VIBE_OS_ThreadFct_t)vibe_os_firmware_load_unload
                               , &vibe_os_firmwares[thread_id]
                               , vibe_os_firmwares[thread_id].thread_name
                               , &thread_settings
                               , (void *)&(vibe_os_firmwares[thread_id].thread_desc));
  }

  result = (RetOk? 0 : -1);
  return result;
}

static void __exit vibe_ostest_exit(void)
{
  int thread_id = 0;
  int err_idx = 0;

  /* Print out errors */
  for(thread_id = 0; thread_id < N_ELEMENTS(vibe_os_firmwares); thread_id++)
  {
    /* Stop the thread befor */
    vibe_os_stop_thread((void *)vibe_os_firmwares[thread_id].thread_desc);

    if(vibe_os_firmwares[thread_id].load_errors)
    {
      pr_info("Thread %s : %s firmware Loading Results :"
             , vibe_os_firmwares[thread_id].thread_name
             , vibe_os_firmwares[thread_id].firmware_name);
      for(err_idx = 0; err_idx < vibe_os_firmwares[thread_id].load_times; err_idx++)
      {
        pr_info("\t%s[%d]\t= %d"
               , vibe_os_firmwares[thread_id].firmware_name
               , err_idx
               , vibe_os_firmwares[thread_id].load_errors[err_idx]);
      }
      vibe_os_free_memory(vibe_os_firmwares[thread_id].load_errors);
    }
  }

  pr_info("End of vibe_os tests\n");
}

/******************************************************************************
 *  Modularization
 */
#ifdef MODULE

MODULE_AUTHOR ("STMicroelectronics");
MODULE_DESCRIPTION ("VIBE OS Abstraction Layer Test");
MODULE_LICENSE ("GPL");

module_init (vibe_ostest_init);
module_exit (vibe_ostest_exit);

#endif /* MODULE */
