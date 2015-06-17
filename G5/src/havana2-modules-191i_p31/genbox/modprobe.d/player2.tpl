
# uncomment this this to allow MPEG2 PIP on 7109 and 7200
__MPEG2HW_ENABLE__ options mpeg2hw mmeName=__MPEG2_TRANSFORMER__

# disable platform_device for tkdma
options platform tkdma=0

install player2 \
                /sbin/modprobe multicom; \
                /sbin/modprobe stmfb; \
                /sbin/modprobe mpeg2hw; \
		/sbin/modprobe silencegen; \
                /sbin/modprobe  --ignore-install player2; \
                /sbin/modprobe stmalloc; \
                /sbin/modprobe platform; \
                /sbin/modprobe sth264pp

alias engine player2

