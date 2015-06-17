#!/sbin/itype
# This is a i file, used by initng parsed by install_service

# NAME:
# DESCRIPTION:
# WWW:

daemon system/inetd {
        env_file = /etc/initng/env;
        stdall = ${OUTPUT};
        need = system/initial;
        provide = virtual/inetd;
        suid = inetd;
        sgid = inetd;
        exec daemon =  /sbin/inetd -f /etc/inetd.conf;
        respawn;
}
