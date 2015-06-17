/*
 * STM PTI DVB driver
 *
 * Copyright (c) STMicroelectronics 2009
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#ifndef __ST_PTI_H
#define __ST_PTI_H

#define STPTI_MAXCHANNEL 128
#define STPTI_MAXADAPTER 4
#define STPTI_DEBUG_BUFFER_SIZE	32768

struct pti_stream_info {
  short int             NumPids;
  short int             Count;
  short int             PidTable[STPTI_MAXCHANNEL];
  short int             Counts[STPTI_MAXCHANNEL];
  short int             Slots[STPTI_MAXCHANNEL];
  short int             RawMode;
} __attribute__ ((packed));

struct stm_pti {
  /* Each PTI is now only responsible for one adapter */
   struct stfe *stfe;

  struct mutex lock;
  struct mutex write_lock;

  /* Mapping from input channel to stfe and if the compiler
     has any smarts directly to the demux bin */
  struct stdemux *mapping[16]; 

  /* Platform information */
  struct platform_device *device_data;

  int id;

  int size_of_header;

  struct pti_stream_info *stream;

  /* Timer lock and timer for interrupt */
  spinlock_t timer_lock;
  struct timer_list timer;	/* timer interrupts for outputs */

  /* Buffer for PTI to output data */
  char *back_buffer;
  unsigned int pti_io;
  int running_feed_count;

  /* Register in the PTI */
  volatile unsigned short int *psize;
  volatile unsigned short int *pread;
  volatile unsigned short int *packet_size;
  volatile unsigned short int *header_size;
  volatile unsigned short int *pwrite;
  volatile unsigned short int *discard;
  volatile unsigned short int *current_stream;
  volatile unsigned char      *buffer;
  volatile unsigned short int *slots;
  volatile unsigned short int *count;
  volatile unsigned short int *multi2_system_key;

  /* Some debug statistics */
  unsigned int loop_count;
  unsigned int loop_count2;
  unsigned int packet_count;

  /* TS Merger Handle */
  struct stm_tsm_handle *tsm;

  /* DebugFS stuff */
  struct dentry *debugfs_entry;
  struct dentry *debugfs_dir;  
  char debug_buf[STPTI_DEBUG_BUFFER_SIZE];

  /* Nicks record of standard dvr ioctl handler */
  struct file_operations original_dvr_fops;

  /* Nicks polarity for each channel */

  char 	channel_polarity[16];
};

#endif
