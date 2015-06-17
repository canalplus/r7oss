#
# modprobe.d/stmfb
#
# Handle the booting of the co-processors
#
# __DEFAULT_RESOLUTION__	default resolution at initialization
# __DEFAULT_OUTPUTS__	default outputs at initialization
# __MEMSIZE__		primary memory size ; on 7109, framebuffers can be allocated in this area only
# __AUX_MEMSIZE__	additional memory size ; on 7109, it is allocated in LMI_VID.
# __BDISP__		0 for old BDisp driver, 1 for BDisp2 driver
# __CPU__		SOC in use
#

options stmfb display0=__DEFAULT_RESOLUTION__:__MEMSIZE__:__AUX_MEMSIZE__:__DEFAULT_OUTPUTS__:__BDISP__

install stmfb \
  /sbin/modprobe stmcore-display-__CPU__ ; \
  /sbin/modprobe --ignore-install stmfb; \
  /sbin/stfbset -a 0
