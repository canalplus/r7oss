#!/sbin/itype
# This is a i file, used by initng parsed by install_service

# NAME: player2
# DESCRIPTION: Loads STMicroelectronics player2 modules
# WWW: http://www.wyplay.com

service modules/engine {
        env_file = /etc/initng/env;
        stdall = ${OUTPUT};
        need = system/initial modules/stmfb;

        script start = {
                /sbin/modprobe player2 || exit 1

                /usr/bin/toposet \
                    channel_assignment[0] LT_RT \
                    channel_assignment[1] LT_RT \
                    channel_assignment[2] LT_RT
        };
}

