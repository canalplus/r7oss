#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "vibe_interface.h"

VibeInterface *gVibe;

void useVibeInterface(VibeInterface *vibeInterface) {
    gVibe = vibeInterface;
}

int stm_display_source_queue_flush(stm_display_source_queue_h q, bool flush_buffers_on_display) {
    //ASSERT_TRUE(gVibe != 0);

    return gVibe->stm_display_source_queue_flush(q, flush_buffers_on_display);
}

int stm_display_output_get_current_display_mode(stm_display_output_h output, stm_display_mode_t *mode) {
    //ASSERT_TRUE(gVibe != 0);
    return gVibe->stm_display_output_get_current_display_mode(output, mode);
}

int stm_display_plane_get_capabilities(stm_display_plane_h plane, stm_plane_capabilities_t *caps) {
    //ASSERT_TRUE(gVibe != 0);
    return gVibe->stm_display_plane_get_capabilities(plane, caps);
}

int stm_display_source_get_connected_plane_id(stm_display_source_h source, uint32_t *id, uint32_t max_ids) {
    //ASSERT_TRUE(gVibe != 0);
    return gVibe->stm_display_source_get_connected_plane_id(source, id, max_ids);
}

int stm_display_source_get_interface(stm_display_source_h source, stm_display_source_interface_params_t iface_params, void **iface_handle) {
    return gVibe->stm_display_source_get_interface(source, iface_params, iface_handle);
}


int stm_display_source_get_device_id(stm_display_source_h source, uint32_t *id) {
    return gVibe->stm_display_source_get_device_id(source, id);
}

int stm_display_source_set_control(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t new_val) {
    return gVibe->stm_display_source_set_control(source, ctrl, new_val);
}

int stm_display_source_get_control(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val) {
    return gVibe->stm_display_source_get_control(source, ctrl, ctrl_val);
}

int stm_display_source_queue_unlock(stm_display_source_queue_h queue) {
    return gVibe->stm_display_source_queue_unlock(queue);
}

int stm_display_source_queue_buffer(stm_display_source_queue_h queue, const stm_display_buffer_t *pBuffer) {
    return gVibe->stm_display_source_queue_buffer(queue, pBuffer);
}

int stm_display_source_queue_release(stm_display_source_queue_h queue) {
    return gVibe->stm_display_source_queue_release(queue);
}

int stm_display_source_queue_lock(stm_display_source_queue_h queue, bool force) {
    return  gVibe->stm_display_source_queue_lock(queue, force);
}

int stm_display_open_device(uint32_t id, stm_display_device_h *device) {
    return gVibe->stm_display_open_device(id, device);
}

int stm_display_device_open_plane(stm_display_device_h dev, uint32_t planeID, stm_display_plane_h *plane) {
    return gVibe->stm_display_device_open_plane(dev, planeID, plane);
}

int stm_display_device_open_output(stm_display_device_h dev, uint32_t outputID, stm_display_output_h *output) {
    return gVibe->stm_display_device_open_output(dev, outputID, output);
}

void stm_display_plane_close(stm_display_plane_h plane) {
    gVibe->stm_display_plane_close(plane);
}

int stm_display_plane_show(stm_display_plane_h plane) {
    return gVibe->stm_display_plane_show(plane);
}

int stm_display_plane_hide(stm_display_plane_h plane) {
    return gVibe->stm_display_plane_hide(plane);
}

int stm_display_plane_get_connected_output_id(stm_display_plane_h plane, uint32_t *id, uint32_t number) {
    return gVibe->stm_display_plane_get_connected_output_id(plane, id, number);
}

void stm_display_output_close(stm_display_output_h output) {
    gVibe->stm_display_output_close(output);
}

void stm_display_device_close(stm_display_device_h dev) {
    gVibe->stm_display_device_close(dev);
}

int stm_display_output_set_control(stm_display_output_h output, stm_output_control_t ctrl, uint32_t newVal) {
    return gVibe->stm_display_output_set_control(output, ctrl, newVal);
}

void stm_display_output_soft_reset(stm_display_output_h output) {
    gVibe->stm_display_output_soft_reset(output);
}

int stm_display_plane_get_status(stm_display_plane_h plane, uint32_t *status) {
    return gVibe->stm_display_plane_get_status(plane, status);
}

int stm_display_plane_get_compound_control(stm_display_plane_h plane, stm_display_plane_control_t  ctrl, void *current_val) {
    return gVibe->stm_display_plane_get_compound_control(plane, ctrl, current_val);
}
