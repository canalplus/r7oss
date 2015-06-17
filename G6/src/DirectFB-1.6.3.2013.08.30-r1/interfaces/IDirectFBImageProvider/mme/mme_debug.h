/**************************************************************
 * Copyright (C) 2010   STMicroelectronics. All Rights Reserved.
 * This file is part of the latest release of the Multicom4 project. This release 
 * is fully functional and provides all of the original MME functionality.This 
 * release  is now considered stable and ready for integration with other software 
 * components.

 * Multicom4 is a free software; you can redistribute it and/or modify it under the 
 * terms of the GNU General Public License as published by the Free Software Foundation 
 * version 2.

 * Multicom4 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along with Multicom4; 
 * see the file COPYING.  If not, write to the Free Software Foundation, 59 Temple Place - 
 * Suite 330, Boston, MA 02111-1307, USA.

 * Written by Multicom team at STMicroelectronics in November 2010.  
 * Contact multicom.support@st.com. 
**************************************************************/

/* 
 * 
 */ 

#ifndef _MME_DEBUG_H
#define _MME_DEBUG_H

typedef enum mme_debug_flags 
{
  MME_DBG             = 0x0000,		/* Always displayed */

  MME_DBG_ERR         = 0x0001,		/* Display all error paths */
  MME_DBG_INIT        = 0x0002,		/* Initialisation operations */
  MME_DBG_MANAGER     = 0x0004,		/* MME management operations */

  MME_DBG_RECEIVER    = 0x0010,		/* Transformer receiver task (companion) */
  MME_DBG_TRANSFORMER = 0x0020,		/* Transformer operations (host) */
  MME_DBG_EXEC        = 0x0040,		/* Execution tasks */

  MME_DBG_COMMAND     = 0x0100,		/* Transformer Command issue */
  MME_DBG_BUFFER      = 0x0200,		/* DataBuffer Alloc/Free */

} MME_DBG_FLAGS;

const char *MME_ErrorStr (MME_ERROR res);
MME_ERROR   MME_DebugFlags (MME_DBG_FLAGS flags);

/* These were inconsistently named originally */
#define MME_Error_Str		MME_ErrorStr
#define MME_Debug_Flags		MME_DebugFlags

#endif /* _MME_DEBUG_H */

/*
 * Local Variables:
 *  tab-width: 8
 *  c-indent-level: 2
 *  c-basic-offset: 2
 * End:
 */
