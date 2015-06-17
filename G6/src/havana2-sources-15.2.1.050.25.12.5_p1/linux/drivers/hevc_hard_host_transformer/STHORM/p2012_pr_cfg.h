/**
 * \brief Configuration for the PR Control
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Bruno GALILEE, STMicroelectronics (bruno.galilee@st.com)
 *
 */

#ifndef _P2012_PR_CFG_included_
#define _P2012_PR_CFG_included_

#define P2012_CONF_PRCTRL_TIMER_OFFSET 0x00000000
#define P2012_CONF_PRCTRL_TIMER_SIZE   0x00008000

#define P2012_CONF_PRCTRL_MSG_OFFSET   (P2012_CONF_PRCTRL_TIMER_OFFSET + P2012_CONF_PRCTRL_TIMER_SIZE)
#define P2012_CONF_PRCTRL_MSG_SIZE     0x00001000

#define P2012_CONF_PRCTRL_REG_OFFSET   (P2012_CONF_PRCTRL_MSG_OFFSET + P2012_CONF_PRCTRL_MSG_SIZE)
#define P2012_CONF_PRCTRL_REG_SIZE     0x00001000

#define P2012_CONF_PRCTRL_ITC_OFFSET   (P2012_CONF_PRCTRL_REG_OFFSET + P2012_CONF_PRCTRL_REG_SIZE)
#define P2012_CONF_PRCTRL_ITC_SIZE     0x00001000

#define P2012_CONF_PRCTRL_T1_OFFSET    (P2012_CONF_PRCTRL_ITC_OFFSET + P2012_CONF_PRCTRL_ITC_SIZE)
#define P2012_CONF_PRCTRL_T1_SIZE      0x00001000

#define P2012_IMPL_PRCTRL_SIZE         (P2012_CONF_PRCTRL_T1_OFFSET + P2012_CONF_PRCTRL_T1_SIZE)

#define COMB_INPUT_IT_SIZE             32
#define COMB_INPUT_ERR_IT_SIZE         32
#define COMB_OUTPUT_IT_SIZE            32
#define COMB_FETCH_ENABLE_SIZE         32
#define COMB_SW_RST_IT_SIZE            32
#define COMB_NMI_IT_SIZE               32

#endif // _P2012_PR_CFG_included_
