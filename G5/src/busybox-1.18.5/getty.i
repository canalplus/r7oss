#!/sbin/itype
# This is a i file, used by initng parsed by install_service

# NAME:
# DESCRIPTION:
# WWW:

service system/getty/detect {
        need = system/udev;
        script start = {
                tty=`grep -Eo 'console=[^,]*' /proc/cmdline | cut -d '=' -f2`
                rm -f /dev/console_tty
                ln -s /dev/${tty} /dev/console_tty
        };
}

daemon system/getty {
        env_file = /etc/initng/env;
        stdall = ${OUTPUT};
        need = system/mountfs system/getty/detect;
        term_timeout = 3;
        exec daemon =  /sbin/getty -L /dev/console_tty 115200 vt102;
        respawn;
}

