/*
 * ST DVB On-Chip DiseqC driver
 *
 * Copyright (c) STMicroelectronics 2009
 *
 *   Author:Peter Bennett <peter.bennett@st.com>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License as
 *      published by the Free Software Foundation; either version 2 of
 *      the License, or (at your option) any later version.
 */
#ifndef _ST_DISEQC_H
#define _ST_DISEQC_H

/* DiseqC registration */
void stm_diseqc_register_frontend ( struct dvb_frontend *fe );

#endif
