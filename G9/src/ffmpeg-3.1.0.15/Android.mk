LOCAL_PATH := $(call my-dir)
# For some reasons, calling my-dir doesn't always give the correct module directory
REAL_LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(WY_BUILD_TOOLS)

# autoconf wrapper
ffmpeg:
	$(call wy-launch-build, all)

LOCAL_MODULE := ffmpeg
include $(BUILD_SHARED_LIBRARY)
