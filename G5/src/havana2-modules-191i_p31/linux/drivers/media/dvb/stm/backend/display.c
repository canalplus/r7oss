/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : display.c
Author :           Julian

Access to all platform specific display information etc


Date        Modification                                    Name
----        ------------                                    --------
05-Apr-07   Created                                         Julian

************************************************************************/

#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <include/stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include "osdev_user.h"
#include "player_module.h"
#include "display.h"

#define MAX_PIPELINES                           4

#ifdef CONFIG_DUAL_DISPLAY
static const stm_plane_id_t                     PlaneId[MAX_PIPELINES]  = {OUTPUT_VID1, OUTPUT_GDP3, OUTPUT_VID1, OUTPUT_GDP2};
#else
static const stm_plane_id_t                     PlaneId[MAX_PIPELINES]  = {OUTPUT_VID1, OUTPUT_VID2, OUTPUT_VID1, OUTPUT_GDP2};
#endif

static struct stmcore_display_pipeline_data     PipelineData[STMCORE_MAX_PLANES];
static unsigned int                             Pipelines;

/*{{{  DisplayInit*/
int DisplayInit (void)
{
    int i;

    Pipelines   = 0;
    for (i = 0; i < MAX_PIPELINES; i++)
    {
        if (stmcore_get_display_pipeline (i, &PipelineData[i]) == 0)
            PLAYER_TRACE("Pipeline %d Device %p, Name = %s, Preferred video plane = %x\n", i, PipelineData[i].device, PipelineData[i].name, PipelineData[i].preferred_video_plane);
        else
            break;
    }
    Pipelines   = i;
    return 0;
}
/*}}}  */
/*{{{  GetDisplayInfo*/
int GetDisplayInfo     (unsigned int            Id,
                        DeviceHandle_t*         Device,
                        unsigned int*           DisplayPlaneId,
                        unsigned int*           OutputId,
                        BufferLocation_t*       BufferLocation)
{
    int                                         i;
    struct stmcore_display_pipeline_data*       Pipeline;

    *DisplayPlaneId     = PlaneId[Id];
#if defined (CONFIG_DUAL_DISPLAY)
    Pipeline            = &PipelineData[Id];
#else
    if (Id == DISPLAY_ID_PIP)
        Pipeline        = &PipelineData[DISPLAY_ID_MAIN];
    else
        Pipeline        = &PipelineData[Id];
#endif

    for (i = 0; i < STMCORE_MAX_PLANES; i++)
    {
        if (Pipeline->planes[i].id == *DisplayPlaneId)
        {
            PLAYER_TRACE("PlaneList %d, Plane %x, Flags %x\n", i, *DisplayPlaneId, Pipeline->planes[i].flags);

            if ((Pipeline->planes[i].flags & STMCORE_PLANE_MEM_ANY) == STMCORE_PLANE_MEM_ANY)
                *BufferLocation = BufferLocationEither;
            else if ((Pipeline->planes[i].flags & STMCORE_PLANE_MEM_SYS) == STMCORE_PLANE_MEM_SYS)
                *BufferLocation = BufferLocationSystemMemory;
            break;
        }
    }

    *Device     = Pipeline->device;
    *OutputId   = Pipeline->main_output_id;
    PLAYER_TRACE ("Device %p, DisplayPlaneId %x, OutputId %d, Pipeline %d\n", *Device, *DisplayPlaneId, *OutputId, Id);

    return  0;
}
/*}}}  */
