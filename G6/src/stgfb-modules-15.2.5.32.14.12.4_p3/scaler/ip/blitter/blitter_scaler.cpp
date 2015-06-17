/***********************************************************************
 *
 * File: scaler/ip/blitter/blitter_scaler.cpp
 * Copyright (c) 2006-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <vibe_os.h>
#include <vibe_debug.h>

#include "blitter_scaler.h"
#include "blitter_pool.h"

// Redefine linux kernel macro as linux header cannot be included here.
#define IS_ERR(x)   ((unsigned long)(x) >= (unsigned long)(-4095))

#define MIN_BLITTER_RECT_WIDTH    8
#define MIN_BLITTER_RECT_HEIGHT   8

CBlitterScaler::CBlitterScaler(uint32_t                   scaler_id,
                               uint32_t                   blitter_id,
                               const CScalerDevice       *pDev) : CScaler(scaler_id, STM_SCALER_CAPS_NONE, pDev)
{
    m_blitterId           = blitter_id;
    m_blitterHandle       = 0;
    m_SrcSurface = 0;
    m_DestSurface = 0;
}


CBlitterScaler::~CBlitterScaler()
{
    // Release source surface if not done
    if (m_SrcSurface)
        stm_blitter_surface_put(m_SrcSurface);
    m_SrcSurface = 0;

    // Release destination surface if not done
    if (m_DestSurface)
        stm_blitter_surface_put(m_DestSurface);
    m_DestSurface = 0;

    // Release blitter handle
    if (m_blitterHandle)
        stm_blitter_put (m_blitterHandle);
    m_blitterHandle = 0;

    // Delete the pool of scaling task nodes
    delete(m_pScalingTaskNodePool);
}


bool CBlitterScaler::Create(void)
{
    TRCIN( TRC_ID_MAIN_INFO, "" );
    if (!CScaler::Create ())
    {
        TRC( TRC_ID_ERROR, "Base class Create failed" );
        return false;
    }

    // Create the pool of scaling task nodes
    m_pScalingTaskNodePool = (CScalingPool *)new CBlitterPool();
    if( (!m_pScalingTaskNodePool) || (!m_pScalingTaskNodePool->Create(MAX_SCALING_TASKS)) )
    {
        TRC( TRC_ID_ERROR, "Failed to allocate pool of scaling task nodes" );
    }

    // Get blitter handle
    m_blitterHandle = stm_blitter_get(m_blitterId);
    if (IS_ERR(m_blitterHandle))
    {
        TRC( TRC_ID_ERROR, "Failed to get a handle on Blitter %d", m_blitterId );
    }

    TRCOUT( TRC_ID_MAIN_INFO, "" );

    return true;
}

scaler_error_t CBlitterScaler::PerformOpening(CScalingCtxNode* const scaling_ctx_node)
{
    // If we need a specific blitter_hanlde for each handle, it could be save in the scaler_handle->specific part
    // But currently, we use the same blitter handle.
    scaling_ctx_node->m_HwCtx = 0;
    return ERR_NONE;
}

scaler_error_t CBlitterScaler::PerformClosing(CScalingCtxNode* const scaling_ctx_node)
{
    // Nothing to do, as nothing done on the PerformOpening
    return ERR_NONE;
}

scaler_error_t CBlitterScaler::PerformScaling(CScalingCtxNode* const scaling_ctx_node, CScalingTaskNode *scaling_task_node)
{
    /*** Perform the scaling ***/
    scaler_error_t error_code = ERR_NONE;
    CBlitterTaskNode *blitter_task_node = (CBlitterTaskNode *)scaling_task_node;

    // prepare the source surface
    if (!PrepareSrcSurfaceToBlit(blitter_task_node))
    {
        error_code = ERR_INTERNAL;
        goto release_surfaces;
    }

    // prepare the destination surface
    if (!PrepareDestSurfaceToBlit(blitter_task_node))
    {
        error_code = ERR_INTERNAL;
        goto release_surfaces;
    }

    // Apply the blit (this call is blocking)
    if(!ApplyBlit(blitter_task_node))
    {
        error_code = ERR_INTERNAL;
        goto release_surfaces;
    }

    // The scaling has been done properly so now
    // Simulate callback reception
    scaling_task_node->m_isScalingTaskContentValid = true;
    ScalingCompletedCbF(scaling_task_node);
    BufferDoneCbF(scaling_task_node);

release_surfaces:

    if (m_SrcSurface)
        stm_blitter_surface_put(m_SrcSurface);
    m_SrcSurface = 0;

    if (m_DestSurface)
        stm_blitter_surface_put(m_DestSurface);
    m_DestSurface = 0;

    return error_code;
}

scaler_error_t CBlitterScaler::PerformAborting(CScalingCtxNode* const scaling_ctx_node)
{
    // Blitter does not support abort functionality
    // Need to wait normal end of scaling task
    return ERR_NONE;
}

bool CBlitterScaler::PrepareSrcSurfaceToBlit(const CBlitterTaskNode *blitter_task_node)
{

    m_SrcSurface = stm_blitter_surface_new_preallocated(
                            blitter_task_node->m_blitterSrce.format,
                            blitter_task_node->m_blitterSrce.cspace,
                           &blitter_task_node->m_blitterSrce.buffer_add,
                            blitter_task_node->m_blitterSrce.buffer_size,
                           &blitter_task_node->m_blitterSrce.buffer_dimension,
                            blitter_task_node->m_blitterSrce.pitch);

    if (IS_ERR(m_SrcSurface))
    {
        TRC( TRC_ID_ERROR, "couldn't create source surface: %p", m_SrcSurface );
        m_SrcSurface = 0;
        return false;
    }

    return true;
}

bool CBlitterScaler::PrepareDestSurfaceToBlit(const CBlitterTaskNode *blitter_task_node)
{
    m_DestSurface = stm_blitter_surface_new_preallocated(
                            blitter_task_node->m_blitterDest.format,
                            blitter_task_node->m_blitterDest.cspace,
                           &blitter_task_node->m_blitterDest.buffer_add,
                            blitter_task_node->m_blitterDest.buffer_size,
                           &blitter_task_node->m_blitterDest.buffer_dimension,
                            blitter_task_node->m_blitterDest.pitch);
    if (IS_ERR(m_DestSurface)) {
        TRC( TRC_ID_ERROR, "couldn't create destination surface: %p", m_DestSurface );
        m_DestSurface = 0;
        return false;
    }

    return true;
}

void CBlitterScaler::AdjustBorderRectToHWConstraints(stm_blitter_dimension_t *buffer_dim, stm_blitter_rect_t *black_rect)
{
    long delta;

    if ( black_rect->size.w < MIN_BLITTER_RECT_WIDTH )
    {
        // Get delta size to increase rectangle width for reaching minimum value
        delta = MIN_BLITTER_RECT_WIDTH - black_rect->size.w;
        // Expand width rectangle to its minimum value
        black_rect->size.w += delta;

        if ( black_rect->position.x + black_rect->size.w > buffer_dim->w )
        {
            // width rectangle is outside the buffer area so set it back inside
            black_rect->position.x -= delta;
        }
    }

    if ( black_rect->size.h < MIN_BLITTER_RECT_HEIGHT )
    {
        // Get delta size to increase rectangle height for reaching minimum value
        delta = MIN_BLITTER_RECT_HEIGHT - black_rect->size.h;
        // Expand width rectangle to its minimum value
        black_rect->size.h += delta;

        if ( black_rect->position.y + black_rect->size.h > buffer_dim->h )
        {
            // width rectangle is outside the buffer area so set it back inside
            black_rect->position.y -= delta;
        }
    }
}

bool CBlitterScaler::FillPictureBorderOfDestSurface(const CBlitterTaskNode *blitter_task_node)
{
    stm_blitter_color_t      black_color;
    stm_blitter_rect_t       black_rect;
    stm_blitter_rect_t       picture_area = blitter_task_node->m_blitterDest.crop_rect;
    stm_blitter_dimension_t  buffer_dim   = blitter_task_node->m_blitterDest.buffer_dimension;

    // Destination surface is always YUV
    if(stm_blitter_surface_set_color_colorspace(m_DestSurface, STM_BLITTER_COLOR_YUV))
    {
        TRC( TRC_ID_ERROR, "couldn't set YUV colorspace for destination surface" );
        return false;
    }

    black_color.a  = 0x80;
    black_color.y  = 0x00;
    black_color.cr = 0x80;
    black_color.cb = 0x80;

    // Set black color for drawing
    if (stm_blitter_surface_set_color(m_DestSurface, &black_color))
    {
        TRC( TRC_ID_ERROR, "couldn't set black background color for destination surface" );
        return false;
    }

    // Fill picture border rectangles: top/left/right/bottom.
    // There are at least 4 possible rectangles to fill with black color around the picture
    // in the destination buffer.
    // ----------------------------------
    // |                                |
    // |           top_rect             |
    // |                                |
    // |--------------------------------|
    // | left_ |               | right_ |
    // | rect  | PICTURE AREA  | rect   |
    // |       |               |        |
    // |--------------------------------|
    // |                                |
    // |          bottom_rect           |
    // |                                |
    // |--------------------------------|
    // picture area is already truncated to buffer dimension
    // due to previous generic treatments

    if ( picture_area.position.y > 0)
    {
        // Fill top rectangle coordinate
        black_rect.position.x = 0;
        black_rect.position.y = 0;
        black_rect.size.w     = buffer_dim.w;
        black_rect.size.h     = picture_area.position.y;

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        TRC( TRC_ID_UNCLASSIFIED, "SCALER TOP BORDER %ld %ld %ld %ld\n",
                black_rect.position.x,
                black_rect.position.y,
                black_rect.size.w,
                black_rect.size.h
                 );

        if (stm_blitter_surface_fill_rect(m_blitterHandle, m_DestSurface, &black_rect))
        {
            TRC( TRC_ID_ERROR, "couldn't fill top black border for destination surface\n" );
            return false;
        }
    }

    if ( picture_area.position.x > 0)
    {
        // Fill left rectangle coordinate
        black_rect.position.x = 0;
        black_rect.position.y = picture_area.position.y;
        black_rect.size.w     = picture_area.position.x;
        black_rect.size.h     = picture_area.size.h;

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        TRC( TRC_ID_UNCLASSIFIED, "SCALER LEFT BORDER %ld %ld %ld %ld\n",
                black_rect.position.x,
                black_rect.position.y,
                black_rect.size.w,
                black_rect.size.h
                 );

        if (stm_blitter_surface_fill_rect(m_blitterHandle, m_DestSurface, &black_rect))
        {
            TRC( TRC_ID_ERROR, "couldn't fill left black border for destination surface\n" );
            return false;
        }
    }

    if ( (picture_area.position.x + picture_area.size.w) < buffer_dim.w)
    {
        // Fill right rectangle coordinate
        black_rect.position.x = picture_area.position.x + picture_area.size.w;
        black_rect.position.y = picture_area.position.y;
        black_rect.size.w     = buffer_dim.w - (picture_area.position.x +
                                                picture_area.size.w);
        black_rect.size.h     = picture_area.size.h;

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        // Blitter workaround
        // It seems that blitter is not filling the last right column of picture area when both picture area x and picture area width are odd.
        if ( (picture_area.position.x & 0x1) && (picture_area.size.w & 0x1)
             && (black_rect.position.x > 0) )
        {
            // So expand right rectangle on the last right column of picture area
            black_rect.position.x -= 1;
            black_rect.size.w     += 1;
        }

        TRC( TRC_ID_UNCLASSIFIED, "SCALER RIGHT BORDER %ld %ld %ld %ld\n",
                black_rect.position.x,
                black_rect.position.y,
                black_rect.size.w,
                black_rect.size.h
                 );

        if (stm_blitter_surface_fill_rect(m_blitterHandle, m_DestSurface, &black_rect))
        {
            TRC( TRC_ID_ERROR, "couldn't fill right black border for destination surface\n" );
            return false;
        }
    }

    if ( (picture_area.position.y + picture_area.size.h) < buffer_dim.h )
    {
        // Fill bottom rectangle coordinate
        black_rect.position.x = 0;
        black_rect.position.y = picture_area.position.y + picture_area.size.h;
        black_rect.size.w     = buffer_dim.w;
        black_rect.size.h     = buffer_dim.h - (picture_area.position.y +
                                                picture_area.size.h);

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        // Blitter workaround
        // It seems that blitter is not filling the last bottom line of picture area when both picture area y and picture area height are odd.
        if ( (picture_area.position.y & 0x1) && (picture_area.size.h & 0x1)
             && (black_rect.position.y > 0) )
        {
            // So expand bottom rectangle on the last bottom line of picture area
            black_rect.position.y -= 1;
            black_rect.size.h     += 1;
        }

        TRC( TRC_ID_UNCLASSIFIED, "SCALER BOTTOM BORDER %ld %ld %ld %ld\n",
                black_rect.position.x,
                black_rect.position.y,
                black_rect.size.w,
                black_rect.size.h
                 );

        if (stm_blitter_surface_fill_rect(m_blitterHandle, m_DestSurface, &black_rect))
        {
            TRC( TRC_ID_ERROR, "couldn't fill bottom black border for destination surface\n" );
            return false;
        }
    }

    return true;

}

bool CBlitterScaler::ApplyBlit(const CBlitterTaskNode *blitter_task_node)
{
    int res;

    // set destination surface blitter flags: usefull for I to P conversion when it is a BOTTOM field.
    res = stm_blitter_surface_set_blitflags(m_DestSurface,(stm_blitter_surface_blitflags_t)(blitter_task_node->m_blitterFlags));
    if(res)
    {
        TRC( TRC_ID_ERROR, "Failed, setting destination surface blitter flags" );
        return false;
    }

    if (!FillPictureBorderOfDestSurface(blitter_task_node))
    {
        TRC( TRC_ID_ERROR, "couldn't set picture border color of destination surface\n" );
        return false;
    }

    // set fence flag for waiting blitting operation to finish without polling
    res = stm_blitter_surface_add_fence(m_DestSurface);
    if(res)
    {
        TRC( TRC_ID_ERROR, "Failed, setting fence flag" );
        return false;
    }

    TRC( TRC_ID_UNCLASSIFIED, "SCALER PICTURE AREA %ld %ld %ld %ld\n",
                blitter_task_node->m_blitterDest.crop_rect.position.x,
                blitter_task_node->m_blitterDest.crop_rect.position.y,
                blitter_task_node->m_blitterDest.crop_rect.size.w,
                blitter_task_node->m_blitterDest.crop_rect.size.h
                 );

    /* This code performs the blitting: with rescaling and format conversion */
    if((blitter_task_node->m_blitterSrce.crop_rect.size.w == blitter_task_node->m_blitterDest.crop_rect.size.w) &&
       (blitter_task_node->m_blitterSrce.crop_rect.size.h == blitter_task_node->m_blitterDest.crop_rect.size.h) &&
       (blitter_task_node->m_blitterFlags == STM_BLITTER_SBF_NONE))
    {
        // progressive input picture with same output picture size to render, use special blitter function
        // that performs a simple copy
        res = stm_blitter_surface_blit(
                     m_blitterHandle,
                     m_SrcSurface,
                    &blitter_task_node->m_blitterSrce.crop_rect,
                     m_DestSurface,
                    &blitter_task_node->m_blitterDest.crop_rect.position, 1);
    }
    else
    {
        // use specific blitter function that performs a rescale
        res = stm_blitter_surface_stretchblit (
                     m_blitterHandle,
                     m_SrcSurface,
                    &blitter_task_node->m_blitterSrce.crop_rect,
                     m_DestSurface,
                    &blitter_task_node->m_blitterDest.crop_rect, 1);
    }
    if (!res)
    {
        /* success - wait for operation to finish */
        stm_blitter_serial_t serial;

        res = stm_blitter_surface_get_serial(m_DestSurface, &serial);
        if(res)
        {
            TRC( TRC_ID_ERROR, "Failed, getting serial" );
            return false;
        }

        // Release system protection for being able to process scaler STKPIs:
        // opening/closing/aborting of scaling context and queueing of new
        // scaling task. But new scaling task could not be pushed to the
        // hardware as scaling thread is currently on an active wait.
        vibe_os_up_semaphore(m_sysProtect);

        res = stm_blitter_wait(m_blitterHandle, STM_BLITTER_WAIT_FENCE, serial);
        if(res)
        {
            TRC( TRC_ID_ERROR, "Failed, waiting serial" );
            return false;
        }

        // Take back system protection
        if( vibe_os_down_semaphore(m_sysProtect) )
        {
            TRC( TRC_ID_ERROR, "Failed, taking semaphore" );
        }

    }
    else
    {
        TRC( TRC_ID_ERROR, "Error (%d) during blitter operation", res );
        return false;
    }

    return true;
}



