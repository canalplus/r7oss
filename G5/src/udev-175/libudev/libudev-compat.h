#ifndef LIBUDEV_COMPAT_H_004E82A0_C3DE_4AD1_A6FF_0A3214068298
#define LIBUDEV_COMPAT_H_004E82A0_C3DE_4AD1_A6FF_0A3214068298

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/socket.h>

int libudev_compat_accept4(int sockfd, struct sockaddr *addr,
    socklen_t *addrlen, int flags);

int libudev_compat_epoll_create1(int flags);

int libudev_compat_inotify_init1(int flags);

int libudev_compat_pipe2(int pipefd[2], int flags);

int libudev_compat_signalfd(int fd, const sigset_t *mask, int flags);

int libudev_compat_socket(int domain, int type, int protocol);

int libudev_compat_socketpair(int domain, int type, int protocol, int sv[2]);

#ifndef LIBUDEV_COMPAT_DISABLE_RENAMING
# define accept4(sockfd, addr, addrlen, flags) \
    libudev_compat_accept4((sockfd), (addr), (addrlen), (flags))
# define epoll_create1(flags) libudev_compat_epoll_create1(flags)
# define inotify_init1(flags) libudev_compat_inotify_init1(flags)
# define pipe2(pipefd, flags) libudev_compat_pipe2((pipefd), (flags))
# define signalfd(fd, mask, flags) \
    libudev_compat_signalfd((fd), (mask), (flags))
# define socket(domain, type, protocol) \
    libudev_compat_socket((domain), (type), (protocol))
# define socketpair(domain, type, protocol, sv) \
    libudev_compat_socketpair((domain), (type), (protocol), (sv))
#endif /* !LIBUDEV_COMPAT_DISABLE_RENAMING */

#endif /* File inclusion guard. */
