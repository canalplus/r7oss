#ifndef IPCONFIG_IPCONFIG_H
#define IPCONFIG_IPCONFIG_H

#include <stdint.h>
#include <sys/types.h>

#define LOCAL_PORT	68
#define REMOTE_PORT	(LOCAL_PORT - 1)

extern uint16_t cfg_local_port;
extern uint16_t cfg_remote_port;

extern char vendor_class_identifier[];
extern int vendor_class_identifier_len;

int ipconfig_main(int argc, char *argv[]);
uint32_t ipconfig_server_address(void *next);

/*
 * Note for gcc 3.2.2:
 *
 * If you're turning on debugging, make sure you get rid of -Os from
 * the gcc command line, or else ipconfig will fail to link.
 */
#undef IPC_DEBUG

#undef DEBUG
#ifdef IPC_DEBUG
#define DEBUG(x) printf x
#else
#define DEBUG(x) do { } while(0)
#endif

#endif /* IPCONFIG_IPCONFIG_H */
