#!/sbin/itype
# This is a i file, used by initng parsed by install_service

# NAME: stmfb
# DESCRIPTION: STM framebuffer driver
# WWW: http://www.stlinux.com

service modules/stmfb {
        env_file = /etc/initng/env;
        stdall = ${OUTPUT};
        need = system/udev/udevd;

        script start = {
                /sbin/modprobe stmfb

                while ! test -e /sys/class/stmcoredisplay/display0/info ; do
                        echo "waiting for stmfb to load"
                        sleep 1
                done

                # set mixer background to black instead of blue
                # and rescale colors to full range
		/sbin/stfbset -M 0xFF000000 -R fullrange

		# deactivate deinterlacer, much too much bandwidth costy
		/sbin/modprobe stmvout
                while ! test -e /dev/video0 ; do
                        echo "waiting for /dev/video0 to be created"
                        sleep 1
                done
               PRG=/sbin/flexvp_ctrl; if [ -x $PRG ]; then $PRG -i 1; fi
        };
}
