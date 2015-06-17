/**
 *****************************************************************************
 * @file plt_id.h
 * @author Kevin PETIT <kevin.petit@technicolor.com> / Franck MARILLET
 *
 * @brief Platform ID header
 *
 * Copyright (C) 2012 Technicolor
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301  USA.
 *
 * Contact Information:
 * technicolor.com
 * 1, rue Jeanne d´Arc
 * 92443 Issy-les-Moulineaux Cedex
 * Paris
 * France
 *
 ******************************************************************************/
#ifndef __PLT_ID_H_
#define __PLT_ID_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PLT_INT_MAX ((int)(~0U>>1))

/* ------------------------------------------------------------------------- */
/* ------------------------- Constants and macros -------------------------- */

/* The parameter name and the PLATFORM_ID names are defined hereafter
 * to be shared with user land implementation.
 */

/* The name of the parameter for the kernel's command line (case sensitive) */
#define PLT_PLATFORM_ID_PARAM_NAME "PLATFORM_ID="

#define PLT_PLATFORM_ID_UNDEFINED_STRING "PLATFORM_ID_UNDEFINED"

#define PLT_ID_BOARD_ID_UNDEFINED PLT_INT_MAX
#define PLT_ID_HW_ID_UNDEFINED    PLT_INT_MAX
#define PLT_ID_CORE_UNDEFINED     PLT_INT_MAX

/* ------------------------------------------------------------------------- */
/* ------------------------- Definitions of types -------------------------- */

/** Enumeration of the different supported PLATFORM_ID
 * Used as an index in some arrays, keep this logic!!
 */
typedef enum {
  PLT_DTI744_CNL_LAB1 = 0,
  PLT_DTI744_CNL_LAB2,
  PLT_DTI744_CNL_INDEX0,

  PLT_MAX_PLATFORM_ID,
  PLT_PLATFORM_ID_UNDEFINED = PLT_INT_MAX
} plt_platform_id_e;

/** The different supported PLATFORM_ID names (case sensitive)
 * !! Please use the same order as in plt_platform_id_e !!
 */
static const char * const plt_id_names[PLT_MAX_PLATFORM_ID] = {
  "DTI744_CNL_LAB1",
  "DTI744_CNL_LAB2",  
  "DTI744_CNL_INDEX0",
};

/* The following is to avoid modifying .c files when adding a new platform
 * (if platform identification scheme remains unchanged).
 * This method is actually used "since DTI807_TI_INDEX0",
 * hence the UNDEFINED values before (specific part of the algorithm).
 */

/*
 * BoardId information on 4 bits
 */
static const unsigned int plt_board_id_field[PLT_MAX_PLATFORM_ID] = {
  0x00,                       /* DTI744_CNL_LAB1     */
  0x01,                       /* DTI744_CNL_LAB2     */
  0x02,                       /* DTI744_CNL_INDEX0   */
};

/*
 * HwId information on 4 bits
 */
static const unsigned int plt_hw_id_field[PLT_MAX_PLATFORM_ID] = {
  0x00,                       /* DTI744_CNL_LAB1     */
  0x00,                       /* DTI744_CNL_LAB2     */
  0x00,                       /* DTI744_CNL_INDEX0   */
};

/* ------------------------------------------------------------------------- */
/* -------------------------- Function prototypes -------------------------- */

plt_platform_id_e plt_id_get(void);
int plt_id_get_hw_id(void);
int plt_id_get_board_id(void);
const char *plt_id_get_name(void);
unsigned int plt_id_get_core(void);

#ifdef __cplusplus
}
#endif

#endif /* ndef __TCH_PLT_H */

