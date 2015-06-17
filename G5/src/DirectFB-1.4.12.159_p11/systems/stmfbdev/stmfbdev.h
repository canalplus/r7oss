/*
   (c) Copyright 2010       STMicroelectronics (R&D) Ltd.
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by André Draszik <andre.draszik@st.com>.
 
   Based on work by Denis Oliver Kropp <dok@directfb.org>,
                    Andreas Hundt <andi@fischlustig.de>,
                    Sven Neumann <neo@directfb.org>,
                    Ville Syrjälä <syrjala@sci.fi> and
                    Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __STMFBDEV__STMFBDEV_H__
#define __STMFBDEV__STMFBDEV_H__

#include <core/system.h>

#include <fusion/shmalloc.h>

#include <core/surface_pool.h>

#include "fb.h"
#include "vt.h"


typedef struct {
     int magic;

     struct fb_fix_screeninfo fix; /* fbdev fixed screeninfo, contains info
                                      about memory and type of card */

     int        num_screens;
     int        num_layers;

     unsigned long page_mask;

     FusionSHMPoolShared *shmpool;
} STMfbdevSharedData;

typedef struct {
     int magic;

     CoreDFB *core;

     STMfbdevSharedData *shared;
     int                 fd;
     void               *framebuffer_base;
     VirtualTerminal    *vt;
} STMfbdev;


#endif /* __STMFBDEV__STMFBDEV_H__ */
