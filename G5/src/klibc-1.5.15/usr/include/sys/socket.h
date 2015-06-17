/*
 * sys/socket.h
 */

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <klibc/extern.h>
#include <klibc/compiler.h>
#include <klibc/sysconfig.h>
#include <linux/socket.h>
#if _KLIBC_HAS_ARCHSOCKET_H
#include <klibc/archsocket.h>
#endif

/* Great job, guys!  These are *architecture-specific* ABI constants,
   that are hidden under #ifdef __KERNEL__... what a brilliant idea!
   These are the "common" definitions; if not appropriate, override
   them in <klibc/archsocket.h>. */
   
#ifndef SOCK_STREAM
# define SOCK_STREAM    1
# define SOCK_DGRAM     2
# define SOCK_RAW       3
# define SOCK_RDM       4
# define SOCK_SEQPACKET 5
# define SOCK_PACKET    10
#endif

typedef int socklen_t;

__extern int socket(int, int, int);
__extern int bind(int, struct sockaddr *, int);
__extern int connect(int, struct sockaddr *, socklen_t);
__extern int listen(int, int);
__extern int accept(int, struct sockaddr *, socklen_t *);
__extern int getsockname(int, struct sockaddr *, socklen_t *);
__extern int getpeername(int, struct sockaddr *, socklen_t *);
__extern int socketpair(int, int, int, int *);
__extern int send(int, const void *, size_t, unsigned int);
__extern int sendto(int, const void *, size_t, int, const struct sockaddr *,
			socklen_t);
__extern int recv(int, void *, size_t, unsigned int);
__extern int recvfrom(int, void *, size_t, unsigned int, struct sockaddr *,
			  socklen_t *);
__extern int shutdown(int, int);
__extern int setsockopt(int, int, int, const void *, socklen_t);
__extern int getsockopt(int, int, int, void *, socklen_t *);
__extern int sendmsg(int, const struct msghdr *, unsigned int);
__extern int recvmsg(int, struct msghdr *, unsigned int);

#endif				/* _SYS_SOCKET_H */
