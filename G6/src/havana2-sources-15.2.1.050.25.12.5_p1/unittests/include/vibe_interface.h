#ifndef _VIBE_INTERFACE_H
#define _VIBE_INTERFACE_H

#include "osinline.h"
#include <stm_display.h>

class VibeInterface {
  public:
    virtual int stm_display_source_queue_flush(stm_display_source_queue_h q, bool flush_buffers_on_display) = 0;
    virtual int stm_display_output_get_current_display_mode(stm_display_output_h output, stm_display_mode_t *mode) = 0;
    virtual int stm_display_plane_get_capabilities(stm_display_plane_h plane, stm_plane_capabilities_t *caps) = 0;
    virtual int stm_display_source_get_connected_plane_id(stm_display_source_h source, uint32_t *id, uint32_t max_ids) = 0;
    virtual int stm_display_source_get_interface(stm_display_source_h source, stm_display_source_interface_params_t iface_params, void **iface_handle) = 0;
    virtual int stm_display_source_get_device_id(stm_display_source_h source, uint32_t *id) = 0;
    virtual int stm_display_source_set_control(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t new_val) = 0;
    virtual int stm_display_source_get_control(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) = 0;
    virtual int stm_display_source_queue_unlock(stm_display_source_queue_h queue) = 0;
    virtual int stm_display_source_queue_buffer(stm_display_source_queue_h queue, const stm_display_buffer_t *pBuffer) = 0;
    virtual int stm_display_source_queue_release(stm_display_source_queue_h queue) = 0;
    virtual int stm_display_source_queue_lock(stm_display_source_queue_h queue, bool force) = 0;
    virtual int stm_display_open_device(uint32_t id, stm_display_device_h *device) = 0;
    virtual int stm_display_device_open_plane(stm_display_device_h dev, uint32_t planeID, stm_display_plane_h *plane) = 0;
    virtual int stm_display_device_open_output(stm_display_device_h dev, uint32_t outputID, stm_display_output_h *output) = 0;
    virtual void stm_display_plane_close(stm_display_plane_h plane) = 0;
    virtual int stm_display_plane_show(stm_display_plane_h plane) = 0;
    virtual int stm_display_plane_hide(stm_display_plane_h plane) = 0;
    virtual int stm_display_plane_get_connected_output_id(stm_display_plane_h plane, uint32_t *id, uint32_t number) = 0;
    virtual void stm_display_output_close(stm_display_output_h output) = 0;
    virtual void stm_display_device_close(stm_display_device_h dev) = 0;
    virtual int stm_display_output_set_control(stm_display_output_h output, stm_output_control_t ctrl, uint32_t newVal) = 0;
    virtual void stm_display_output_soft_reset(stm_display_output_h output) = 0;
    virtual int stm_display_plane_get_status(stm_display_plane_h plane, uint32_t *status) = 0;
    virtual int stm_display_plane_get_compound_control(stm_display_plane_h plane, stm_display_plane_control_t  ctrl, void *current_val) = 0;


};

void useVibeInterface(VibeInterface *vibeInterface);

#endif

