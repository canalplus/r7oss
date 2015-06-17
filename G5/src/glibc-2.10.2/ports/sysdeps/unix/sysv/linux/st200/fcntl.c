/* Use the x86 implementation of fcntl. Effectively this ends up calling
   sys_fcntl64 all the time instead of sys_fcntl. This avoids problems
   when trying to lock files when large file support is enabled.
*/
#include <sysdeps/unix/sysv/linux/i386/fcntl.c>
