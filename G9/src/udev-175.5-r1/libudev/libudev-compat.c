#include <errno.h>
#include <fcntl.h>

#define LIBUDEV_COMPAT_DISABLE_RENAMING
#include "libudev-compat.h"

#if defined(HAVE_ACCEPT4) && !HAVE_DECL_ACCEPT4
extern int accept4(int sockfd, struct sockaddr * addr, socklen_t * addrlen,
        int flags);
#endif /* HAVE_ACCEPT4 and !HAVE_DECL_ACCEPT4 */

static int fd_set_non_blocking(int fd) {
	long flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, (flags | O_NONBLOCK));
}

static int fd_set_close_on_exec(int fd) {
	long flags;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFD, (flags | FD_CLOEXEC));
}

int libudev_compat_accept4(int sockfd, struct sockaddr *addr,
		socklen_t *addrlen, int flags) {
  int fd;

#ifdef HAVE_ACCEPT4
  fd = accept4(sockfd, addr, addrlen, flags);
  if (fd != -1 || errno != ENOSYS)
		return fd;
#endif /* HAVE_ACCEPT4 */

	fd = accept(sockfd, addr, addrlen);
	if (fd == -1)
		return fd;

	if ((flags & SOCK_CLOEXEC) != 0 && fd_set_close_on_exec(fd) == -1)
		goto err;

	if ((flags & SOCK_NONBLOCK) != 0 && fd_set_non_blocking(fd) == -1)
		goto err;

	return fd;

err:
	{
		int error = errno;
		close(fd);
		errno = error;
	}
	return -1;
}

int libudev_compat_epoll_create1(int flags) {
  int fd;

#ifdef HAVE_EPOLL_CREATE1
  fd = epoll_create1(flags);
  if (fd != -1 || errno != ENOSYS)
		return fd;
#endif /* HAVE_EPOLL_CREATE1 */

	fd = epoll_create(1);
	if (fd == -1)
		return fd;

	if ((flags & EPOLL_CLOEXEC) != 0 && fd_set_close_on_exec(fd) == -1)
		goto err;

	return fd;

err:
	{
		int error = errno;
		close(fd);
		errno = error;
	}
	return -1;
}

int libudev_compat_inotify_init1(int flags) {
  int fd;

#ifdef HAVE_INOTIFY_INIT1
  fd = inotify_init1(flags);
  if (fd != -1 || errno != ENOSYS)
		return fd;
#endif /* HAVE_INOTIFY_INIT1 */

	fd = inotify_init();
	if (fd == -1)
		return fd;

	if ((flags & IN_CLOEXEC) != 0 && fd_set_close_on_exec(fd) == -1)
		goto err;

	if ((flags & IN_NONBLOCK) != 0 && fd_set_non_blocking(fd) == -1)
		goto err;

	return fd;

err:
	{
		int error = errno;
		close(fd);
		errno = error;
	}
	return -1;
}

int libudev_compat_pipe2(int pipefd[2], int flags) {
  int res;

#ifdef HAVE_PIPE2
  res = pipe2(pipefd, flags);
	if (res != -1 || errno != ENOSYS)
		return res;
#endif /* HAVE_PIPE2 */

	if ((flags & ~(O_NONBLOCK | O_CLOEXEC)) != 0) {
		errno = EINVAL;
		return -1;
	}

	if (pipe(pipefd) == -1)
		return -1;

	if ((flags & O_CLOEXEC) != 0 &&
			(fd_set_close_on_exec(pipefd[0]) == -1 ||
			 fd_set_close_on_exec(pipefd[1]) == -1))
		goto err;

	if ((flags & O_NONBLOCK) != 0 &&
			(fd_set_non_blocking(pipefd[0]) == -1 ||
			fd_set_non_blocking(pipefd[1]) == -1))
		goto err;

	return 0;

err:
	{
		int error = errno;
		close(pipefd[0]);
		close(pipefd[1]);
		errno = error;
	}
	return -1;
}

int libudev_compat_signalfd(int fd, const sigset_t *mask, int flags) {
  int sfd;

  sfd = signalfd(fd, mask, flags);
  if (sfd != -1 || errno != EINVAL ||
			((flags & (SFD_NONBLOCK | SFD_CLOEXEC)) == 0))
		return sfd;

	sfd = signalfd(fd, mask, 0);
	if (sfd == -1)
		return sfd;

	if ((flags & SFD_CLOEXEC) != 0 && fd_set_close_on_exec(sfd) == -1)
		goto err;

	if ((flags & SFD_NONBLOCK) != 0 && fd_set_non_blocking(sfd) == -1)
		goto err;

	return sfd;

err:
	if (sfd != fd) {
		int error = errno;
		close(sfd);
		errno = error;
	}
	return -1;
}

int libudev_compat_socket(int domain, int type, int protocol) {
  int fd;

  fd = socket(domain, type, protocol);
  if (fd != -1 || errno != EINVAL ||
			((type & (SOCK_NONBLOCK | SOCK_CLOEXEC)) == 0))
		return fd;

	fd = socket(domain, type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC), protocol);
	if (fd == -1)
		return fd;

	if ((type & SOCK_CLOEXEC) != 0 && fd_set_close_on_exec(fd) == -1)
		goto err;

	if ((type & SOCK_NONBLOCK) != 0 && fd_set_non_blocking(fd) == -1)
		goto err;

	return fd;

err:
	{
		int error = errno;
		close(fd);
		errno = error;
	}
	return -1;
}

int libudev_compat_socketpair(int domain, int type, int protocol, int sv[2]) {
	int res;

  res = socketpair(domain, type, protocol, sv);
	if (res != -1 || errno != EINVAL ||
			((type & (SOCK_NONBLOCK | SOCK_CLOEXEC)) == 0))
		return res;

	if (socketpair(domain, type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC), protocol, sv)
			== -1)
		return -1;

	if ((type & SOCK_CLOEXEC) != 0 && fd_set_close_on_exec(sv[0]) == -1)
		goto err;
	if ((type & SOCK_CLOEXEC) != 0 && fd_set_close_on_exec(sv[1]) == -1)
		goto err;

	if ((type & SOCK_NONBLOCK) != 0 && fd_set_non_blocking(sv[0]) == -1)
		goto err;
	if ((type & SOCK_NONBLOCK) != 0 && fd_set_non_blocking(sv[1]) == -1)
		goto err;

	return 0;

err:
	{
		int error = errno;
		close(sv[0]);
		close(sv[1]);
		errno = error;
	}
	return -1;
}
