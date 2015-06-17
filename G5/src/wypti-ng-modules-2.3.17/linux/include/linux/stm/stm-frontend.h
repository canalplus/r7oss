/*
 * STM-Frontend Platform header file
 *
 * Copyright (c) STMicroelectronics 2005
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#ifndef __STM_FRONTEND_H
#define __STM_FRONTEND_H

#define STM_NONE 0xff

#define STM_GET_CHANNEL(x)       ((x & 0x0f0000) >> 16)

enum plat_frontend_options {
       STM_DISEQC_STONCHIP     = 0x000200,

       STM_TSM_CHANNEL_0       = 0x000000,
       STM_TSM_CHANNEL_1       = 0x010000,
       STM_TSM_CHANNEL_2       = 0x020000,
       STM_TSM_CHANNEL_3       = 0x030000,

       STM_SERIAL_NOT_PARALLEL = 0x100000,
       STM_INVERT_CLOCK        = 0x200000,
       STM_PACKET_CLOCK_VALID  = 0x400000 
};

struct plat_frontend_channel {
  enum plat_frontend_options option;

  struct dvb_frontend* (*demod_attach)(void *config, struct i2c_adapter *i2c);
  struct dvb_frontend* (*tuner_attach)(void *config, struct dvb_frontend *fe, struct i2c_adapter *i2c);
  struct dvb_frontend* (*lnb_attach)  (void *config, struct dvb_frontend *fe, struct i2c_adapter *i2c);
  
  void *                     config;

  unsigned char              demod_i2c_bus;
  const char                *demod_i2c_bus_name;

  unsigned char              tuner_i2c_bus;
  const char                *tuner_i2c_bus_name;

  unsigned char              lnb_i2c_bus;
  const char                *lnb_i2c_bus_name;

  unsigned char              lock;
  unsigned char              drop;

  unsigned char              pio_reset_bank;
  unsigned char              pio_reset_pin;
  unsigned char              pio_reset_n;
};

struct plat_frontend_config {
  int                           nr_channels;
  struct plat_frontend_channel *channels;
};

struct plat_tsm_config {
  unsigned long              tsm_swts_channel;
  
  unsigned long              tsm_sram_buffer_size;
  unsigned long              tsm_num_pti_alt_out;
  unsigned long              tsm_num_1394_alt_out;
  unsigned long              tsm_num_channels;
};

struct plat_diseqc_config {
  // Only relevent for kernel 2.3 or earlier
  unsigned char              diseqc_pio_bank;
  unsigned char              diseqc_pio_pin;
};

#endif
