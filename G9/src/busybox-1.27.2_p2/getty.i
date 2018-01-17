#!/sbin/itype
# This is a i file, used by initng parsed by install_service

# NAME:
# DESCRIPTION:
# WWW:

daemon system/getty {
        env_file = /etc/initng/env;
        stdall = ${OUTPUT};
        need = system/mountfs;
        provide = virtual/getty;
        term_timeout = 3;
        exec daemon =  /sbin/getty -L ttyS0 115200 vt102;
        respawn;
}
