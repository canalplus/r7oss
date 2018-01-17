daemon debug/test_pidfile {
    exec daemon = /sbin/test_pidfile;
    pid_file = /var/run/test.pid;
    forks;
}
