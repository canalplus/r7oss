/*
 * acc_mme_wrapper.c
 *
 * Copyright (C) STMicroelectronics Limited 2005-2006. All rights reserved.
 *
 * Wrap MME calls in order to provide a sequenced summary of command flow.
 */


#define _ACC_MME_WRAPPER_C_

#include <linux/module.h>
#include <linux/init.h>
#include <asm/semaphore.h>

#include "acc_mme.h"
#include "report.h"
#include "st_relay.h"

MODULE_LICENSE("GPL");

DECLARE_MUTEX(cmd_mutex);


/* C library wrapping */
#define printf(fmt, args...) printk(fmt, ##args)


/* to avoid opening and closing a relay (which would break it) we leave
 * it open at all times and use a 'fake' FOPEN() from within cmd_log_lock()
 */

#define FILE void

FILE * usb_fout;
#define FWRITE(p,s,n,f) st_relayfs_write(ST_RELAY_TYPE_MME_LOG, ST_RELAY_SOURCE_MME_LOG, \
                                         (unsigned char *) p, s*n, 0)
#define FOPEN(n,p)      usb_fout
#define FFOPEN(n,p)     usb_fout
#define FCLOSE(f)       f = NULL
#define FPUTC(c,f)      st_relayfs_write(ST_RELAY_TYPE_MME_LOG, ST_RELAY_SOURCE_MME_LOG, &c, 1, 0)
#define CMD_LOG_FILE "acc_mme_wrapper"

enum eBoolean 
  {
    ACC_FALSE,
    ACC_TRUE,
  };	


static int log_enable = ACC_FALSE;

enum eCmdType
  {
    CMD_GETCAPABILITY, CMD_INIT, CMD_SEND, CMD_ABORT, CMD_TERM, CMD_LAST
  };

static unsigned char * cmd_str[CMD_LAST] = 
  {
    "CMD_GETC", "CMD_INIT", "CMD_SEND", "CMD_ABORT", "CMD_TERM"
  };

#define NB_MAX_TRANSFORMERS 8

int handles[NB_MAX_TRANSFORMERS + 1];

enum eWarning 
  {
    ACC_WARNING_NO_SCATTERPAGE_IN_BUFFER,

    // do not edit
    ACC_LAST_WARNING
  };

char warning[ACC_LAST_WARNING][64]=
  {
#define E(x) #x
    E(ACC_WARNING_NO_SCATTERPAGE_IN_BUFFER),
#undef E
  };


/* static void acc_warning(enum eWarning wn) */
/* { */
/* 	printk("acc_mme_wrapper: %s\n", warning[wn]); */
/* } */


static void handles_clear(void)
{
  int i;
	
  for (i = 0; i < NB_MAX_TRANSFORMERS; i++) 
    {
      handles[i] = 0;
    }
}

static int handles_search(int hdl)
{
  int i;
	
  for (i = 0; i < NB_MAX_TRANSFORMERS; i++) 
    {
      if (handles[i] == hdl) return (i);
    }
	
  return i;
	
}
static int handles_new(void)
{
  int i;
	
  for (i = 0; i < NB_MAX_TRANSFORMERS; i++) 
    {
      if (handles[i] == 0) return (i);
    }
  return i;
}

FILE * cmd_log_lock(void)
{
  FILE * fin;

  down(&cmd_mutex);

  fin=FOPEN(CMD_LOG_FILE, "ab");

  return(fin);
}

void cmd_log_release(FILE * fin)
{
  FCLOSE(fin);

  up(&cmd_mutex);
}

void acc_mme_logreset(void)
{
  FILE * fcmd;
	
  fcmd = FFOPEN(CMD_LOG_FILE, "wb");
  FCLOSE(fcmd);
}

void acc_mme_logstart(void)
{
  printf("Init of MME log wrapper\n");
  usb_fout = FFOPEN(CMD_LOG_FILE, "ab");
}

void acc_mme_logstop(void)
{
  log_enable =  ACC_FALSE;
}

MME_ERROR acc_MME_InitTransformer(const char *Name,
				  MME_TransformerInitParams_t * Params_p,
				  MME_TransformerHandle_t     * Handle_p)
{
  MME_ERROR  mme_status;
	
  FILE     * fcmd = cmd_log_lock();
	
  if (log_enable == ACC_TRUE)
    {
      FWRITE(cmd_str[CMD_INIT], strlen(cmd_str[CMD_INIT]) + 1, 1, fcmd);
      FWRITE((char *)Name, strlen(Name) + 1, 1, fcmd);
      FWRITE(Params_p, sizeof(MME_TransformerInitParams_t), 1, fcmd);
      FWRITE(Params_p->TransformerInitParams_p, Params_p->TransformerInitParamsSize, 1, fcmd);
    }
  cmd_log_release(fcmd);
	
  mme_status =  MME_InitTransformer(Name, Params_p, Handle_p);
	
  if (log_enable)
    {
      if (mme_status == MME_SUCCESS)  handles[handles_new()] = (int) *Handle_p;
    }

  return mme_status;
}

MME_ERROR acc_MME_GetTransformerCapability(const char * TransformerName, MME_TransformerCapability_t *TransformerInfo_p)
{ 
  if (log_enable == ACC_TRUE)
    {
      FILE * fcmd = cmd_log_lock();

      FWRITE(cmd_str[CMD_GETCAPABILITY], strlen(cmd_str[CMD_GETCAPABILITY]) + 1, 1, fcmd);
      FWRITE((char *)TransformerName, strlen((char *)TransformerName) + 1, 1, fcmd);
      cmd_log_release(fcmd);
    }
	
  return MME_GetTransformerCapability(TransformerName, TransformerInfo_p);
	
}

MME_ERROR acc_MME_SendCommand(MME_TransformerHandle_t Handle, MME_Command_t * CmdInfo_p)
{
  MME_ERROR   mme_status;
  FILE      * fcmd = NULL;
  int         i,j, nbuf;
	

  if (log_enable == ACC_TRUE)
    {	
      // log the command;

		
      int    hdl_idx =  handles_search(Handle);

      fcmd = cmd_log_lock();
		
      FWRITE(cmd_str[CMD_SEND], strlen(cmd_str[CMD_SEND])+ 1 , 1, fcmd);
      FWRITE(&hdl_idx, sizeof(int), 1, fcmd);
      FWRITE(CmdInfo_p, sizeof(MME_Command_t), 1, fcmd);
      FWRITE(CmdInfo_p->Param_p, CmdInfo_p->ParamSize, 1, fcmd);
      //		printk("Param: %d\n", ((int*)CmdInfo_p->Param_p)[2]);

      nbuf =  CmdInfo_p->NumberInputBuffers + CmdInfo_p->NumberOutputBuffers;
      for (i = 0; i < nbuf; i++)
	{
	  MME_DataBuffer_t  * db = CmdInfo_p->DataBuffers_p[i];
	  MME_ScatterPage_t * sc;
	  FWRITE(db, sizeof(MME_DataBuffer_t), 1, fcmd);

	  if (db->NumberOfScatterPages == 0)
	    {
	      // check whether a buffer is sent without any pages  !! should never happen.
	      //acc_warning(ACC_WARNING_NO_SCATTERPAGE_IN_BUFFER);
	    }
			
	  for (j = 0; j < db->NumberOfScatterPages; j++)
	    {
	      sc = &db->ScatterPages_p[j];
					
	      FWRITE(sc, sizeof(MME_ScatterPage_t), 1, fcmd);
	      if (sc->Size != 0)
		{
		  if (i<CmdInfo_p->NumberInputBuffers)
		    {
		      FWRITE(sc->Page_p, sizeof(unsigned char), sc->Size,fcmd);
		    }
		}
	    }
	}
    }
	
  // send the command to get back the ID generated by Multicom
  mme_status =  MME_SendCommand(Handle, CmdInfo_p);

  if (log_enable == ACC_TRUE)
    {
      // replace the ID in the local copy
      FWRITE(&CmdInfo_p->CmdStatus.CmdId, sizeof(unsigned int), 1 ,fcmd);

      cmd_log_release(fcmd);
    }
	
  return mme_status;
}

MME_ERROR acc_MME_AbortCommand(MME_TransformerHandle_t Handle, MME_CommandId_t CmdId)
{
  if (log_enable == ACC_TRUE)
    {
      FILE * fcmd    = cmd_log_lock();
      int    hdl_idx =  handles_search(Handle);

      FWRITE(cmd_str[CMD_ABORT], strlen(cmd_str[CMD_ABORT]) + 1, 1, fcmd);
      FWRITE(&hdl_idx, sizeof(int), 1, fcmd);
      FWRITE(&CmdId, sizeof(int), 1, fcmd);
			
      cmd_log_release(fcmd);	
    }

  return MME_AbortCommand(Handle, CmdId);
}


/* MME_TermTransformer()
 * Terminate a transformer instance
 */
MME_ERROR acc_MME_TermTransformer(MME_TransformerHandle_t handle)
{
  MME_ERROR mme_status;
  int      hdl_idx =  handles_search(handle);

  if (log_enable == ACC_TRUE)
    {
      FILE * fcmd    = cmd_log_lock();

      FWRITE(cmd_str[CMD_TERM], strlen(cmd_str[CMD_TERM]) + 1, 1, fcmd);
      FWRITE(&hdl_idx, sizeof(int), 1, fcmd);
			
      cmd_log_release(fcmd);
    }

  mme_status =  MME_TermTransformer(handle);

  if (log_enable == ACC_TRUE)
    {
      if (mme_status == MME_SUCCESS) handles[hdl_idx] = 0; 
    }

  return mme_status;
}



static int __init acc_mme_loginit(void)
{
  acc_mme_logstart();

  handles_clear();
	
  return 0;
}
module_init(acc_mme_loginit);

static void __exit acc_mme_logcleanup(void)
{
  acc_mme_logstop();
}
module_exit(acc_mme_logcleanup);

module_param(log_enable, bool, S_IRUGO|S_IWUSR);


// #if defined __KERNEL__ && defined _ACC_WRAPP_MME_

EXPORT_SYMBOL(acc_MME_InitTransformer);
EXPORT_SYMBOL(acc_MME_TermTransformer);
EXPORT_SYMBOL(acc_MME_AbortCommand);
EXPORT_SYMBOL(acc_MME_SendCommand);
EXPORT_SYMBOL(acc_MME_GetTransformerCapability);

// #endif // __KERNEL__ && _ACC_WRAPP_MME_

