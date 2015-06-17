/**
 * \brief Standard and share symbol over models and test code
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

#ifndef _P2012_H_
#define _P2012_H_

#ifdef __cplusplus
namespace p2012
{
#endif

  /*------------------------------------------------------------------------------
   * Shared values
   *----------------------------------------------------------------------------*/
  // We prefer using enum type instead of 'const int' type in order to enhance
  // robustness against multiple error code name using same value.
  // Drawback: introduce implicit type cast into models
  typedef enum {
    ERR_FATAL         = -1,
    ERR_SEGFAULT      = -2,
    ERR_BUSERROR      = -3,
    ERR_OUTOFMEMORY   = -4,
    ERR_SUCCESS       = 0,
    ERR_USER_STOP     = 2,
    ERR_HOST_FAIL     = 3,
    ERR_LOADER_FAIL   = 4,
    ERR_RAM_DIFFER    = 5,
    ERR_NOT_SUPPORTED = 6,
    ERR_DEAD_LOCK     = 7,
    ERR_TBD_OK        = 8,
    ERR_TBD_KO        = 9
  } error_code_t;

  // Error and warning levels to be used with tlm_message (or others)
  static const unsigned int FATAL_LEVEL  = 0;
  static const unsigned int HIGH_LEVEL   = 1;
  static const unsigned int MEDIUM_LEVEL = 2;
  static const unsigned int LOW_LEVEL    = 3;


  // Model time accuracy specification. Could be changed at run time by ESW
  typedef enum {
    P2012_UNTIMED =0,
    P2012_PACKET,
    P2012_FLIT
  } p2012_accuracy_level_t;



  static const char P2012_XP70_PE_PROG_NAME[]="xp70_pe";
  static const char P2012_XP70_FP_PROG_NAME[]="xp70_fp";


  // P2012 IP interrupt ports
  typedef enum {
    P2012_IT_HOST = 0,
    P2012_MB_IT_HOST,
    P2012_IT_HOST_ERROR,
    P2012_IT_HOST_MAX_PORT_ID
  } p2012_irq_port_e;

#ifdef __cplusplus
} // namespace
#endif

#endif
