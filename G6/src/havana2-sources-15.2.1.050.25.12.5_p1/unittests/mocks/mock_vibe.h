#ifndef _MOCK_VIBE_H
#define _MOCK_VIBE_H

#include "gmock/gmock.h"
#include "vibe_interface.h"

class MockVibe_c : public VibeInterface {
  public:
    MockVibe_c();
    virtual ~MockVibe_c();

    MOCK_METHOD2(stm_display_source_queue_flush,
                 int(stm_display_source_queue_h q, bool flush_buffers_on_display));
    MOCK_METHOD2(stm_display_output_get_current_display_mode,
                 int(stm_display_output_h output, stm_display_mode_t *mode));
    MOCK_METHOD2(stm_display_plane_get_capabilities,
                 int(stm_display_plane_h plane, stm_plane_capabilities_t *caps));
    MOCK_METHOD3(stm_display_source_get_connected_plane_id,
                 int(stm_display_source_h source, uint32_t *id, uint32_t max_ids));
    MOCK_METHOD3(stm_display_source_get_interface,
                 int(stm_display_source_h source, stm_display_source_interface_params_t iface_params, void **iface_handle));
    MOCK_METHOD2(stm_display_source_get_device_id,
                 int(stm_display_source_h source, uint32_t *id));
    MOCK_METHOD3(stm_display_source_set_control,
                 int(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t new_val));
    MOCK_METHOD3(stm_display_source_get_control,
                 int(stm_display_source_h source, stm_display_source_ctrl_t ctrl, uint32_t *ctrl_val));
    MOCK_METHOD1(stm_display_source_queue_unlock,
                 int(stm_display_source_queue_h queue));
    MOCK_METHOD2(stm_display_source_queue_buffer,
                 int(stm_display_source_queue_h queue, const stm_display_buffer_t *pBuffer));
    MOCK_METHOD1(stm_display_source_queue_release,
                 int(stm_display_source_queue_h queue));
    MOCK_METHOD2(stm_display_source_queue_lock,
                 int(stm_display_source_queue_h queue, bool force));
    MOCK_METHOD2(stm_display_open_device,
                 int(uint32_t id, stm_display_device_h *device));
    MOCK_METHOD3(stm_display_device_open_plane,
                 int(stm_display_device_h dev, uint32_t planeID, stm_display_plane_h *plane));
    MOCK_METHOD3(stm_display_device_open_output,
                 int(stm_display_device_h dev, uint32_t outputID, stm_display_output_h *output));
    MOCK_METHOD1(stm_display_plane_close,
                 void(stm_display_plane_h plane));
    MOCK_METHOD1(stm_display_plane_show,
                 int(stm_display_plane_h plane));
    MOCK_METHOD1(stm_display_plane_hide,
                 int(stm_display_plane_h plane));
    MOCK_METHOD3(stm_display_plane_get_connected_output_id,
                 int(stm_display_plane_h plane, uint32_t *id, uint32_t number));
    MOCK_METHOD1(stm_display_output_close,
                 void(stm_display_output_h output));
    MOCK_METHOD1(stm_display_device_close,
                 void(stm_display_device_h dev));
    MOCK_METHOD3(stm_display_output_set_control,
                 int(stm_display_output_h output, stm_output_control_t ctrl, uint32_t newVal));
    MOCK_METHOD1(stm_display_output_soft_reset,
                 void(stm_display_output_h output));
    MOCK_METHOD2(stm_display_plane_get_status,
                 int(stm_display_plane_h plane, uint32_t *status));
    MOCK_METHOD3(stm_display_plane_get_compound_control,
                 int(stm_display_plane_h plane, stm_display_plane_control_t ctrl, void *current_val));
};

#endif

