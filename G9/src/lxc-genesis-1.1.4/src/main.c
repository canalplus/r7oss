/* Copyright (C) 2014 Wyplay, All Rights Reserved.
 * This source code and any compilation or derivative thereof is the
 * proprietary information of Wyplay and is confidential in nature.
 * Under no circumstances is this software to be exposed to or placed under
 * an Open Source License of any type without the expressed written permission
 * of Wyplay. */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

#include "utils.h"

void hello(void)
{
    LOG_INFO(".\n\
..\n\
....\n\
........\n\
................\n\
................................\n\
Init...\n");
}

int opt_env(const char *opt)
{
    if (opt[0] == '\0')
        return 1;
    return putenv((char *)opt);
}

int opt_limit(int resource, const char *limit_s)
{
    int ret;
    struct rlimit rlp;
    unsigned long int limit = strtoul(limit_s, NULL, 10);
    if(limit == ULONG_MAX)
        return 1; // overflow
    rlp.rlim_max = limit;
    rlp.rlim_cur = limit;
    if((ret = setrlimit(resource, &rlp)))
        return ret;
    if((ret = getrlimit(resource, &rlp)))
        return ret;
    if(rlp.rlim_max != limit || rlp.rlim_cur != limit)
        return -2; // limit not set
    return ret;
}

int set_uid(const char *uid)
{
    struct passwd *result;
    uid_t ruid, euid, suid;

    if (!(result = getpwnam(uid))) {
        perror ("getpwnam");
        return 1;
    }

    if (initgroups(result->pw_name, result->pw_gid)) {
        perror ("initgroups");
        return 1;
    }

    if (setresuid(result->pw_uid, result->pw_uid, result->pw_uid)) {
        perror ("setresuid");
        return 1;
    }

    if (getresuid(&ruid, &euid, &suid)) {
        perror ("getresuid");
        return 1;
    }

    if ((ruid != result->pw_uid) || (euid != result->pw_uid) || (suid != result->pw_uid))
        return 1;

    return 0;
}

int set_gid(const char *gid)
{
    struct group *result;
    gid_t rgid, egid, sgid;

    if (!(result = getgrnam(gid))) {
        perror ("getgrnam");
        return 1;
    }

    if (setresgid(result->gr_gid, result->gr_gid, result->gr_gid)) {
        perror ("setresgid");
        return 1;
    }

    if (getresgid(&rgid, &egid, &sgid)) {
        perror ("getresgid");
        return 1;
    }

    if ((rgid != result->gr_gid) || (egid != result->gr_gid) || (sgid != result->gr_gid))
        return 1;

    return 0;
}

int set_umask(const char *mask)
{
  int i;
  for (i = 0; mask[i]; i++)
    if (mask[i] < '0' || mask[i] > '7')
      return -1;
  if (i > 4)
    return -1;
  umask(strtol(mask, NULL, 8));
  return 0;
}

int run(int argc, char *argv[])
{
    int i, ret;
    extern char **environ;
    if (argv[argc] != NULL)
    {
        ret = 1;
        goto err;
    }
    printf("running");
    for (i = 0; i < argc; i++)
        printf(" %s", argv[i]);
    printf("...\n");
    ret = execve(argv[0], argv, environ);
err:
    LOG_ERROR("run failed\n");
    return ret;
}

int main(int argc, char *argv[])
{
    int opt;
    const char *uid = NULL;
    const char *gid = NULL;
#ifndef ENABLE_HARDENED
    fprintf(stderr, "\
#####################################################\n\
#                   -- WARNING --                   #\n\
# (!) Using lxc_genesis init program is deprecated. #\n\
#     Please consider linking liblxc_genesis.       #\n\
#####################################################\n"
    );
#endif /* ENABLE_HARDENED */
    hello();
    while ((opt = getopt(argc, argv, "-e:c:f:w:u:g:m:")) != -1) {
        switch (opt) {
            case 'e': // set env var, argument format NAME=VALUE
                if(opt_env(optarg)) {
                    LOG_ERROR("invalid env option\n");
                    return EXIT_FAILURE;
                }
                LOG_INFO("env %s\n", optarg);
                break;
            case 'c': // set maximum size of core file (0: no core created)
                if(opt_limit(RLIMIT_CORE, optarg)) {
                    LOG_ERROR("invalid core size limit option\n");
                    return EXIT_FAILURE;
                }
                LOG_INFO("core size limit %s\n", optarg);
                break;
            case 'f': // set maximum opened files descriptors
                if(opt_limit(RLIMIT_NOFILE, optarg)) {
                    LOG_ERROR("invalid file desc. limit option\n");
                    return EXIT_FAILURE;
                }
                LOG_INFO("file desc. limit %s\n", optarg);
                break;
            case 'w': // set maximum created file size
                if(opt_limit(RLIMIT_FSIZE, optarg)) {
                    LOG_ERROR("invalid created file size limit option\n");
                    return EXIT_FAILURE;
                }
                LOG_INFO("created file size limit %s\n", optarg);
                break;
            case 'u': // set the user id
                uid = optarg;
                break;
            case 'g': // set the group id
                gid = optarg;
                break;
	    case 'm':
		if (set_umask(optarg)) {
		  LOG_ERROR("invalid umask option\n");
		  return EXIT_FAILURE;
		}
	      break;
            case '?':
            case 1: // handle non-options
            default:
                LOG_ERROR("invalid option\n");
                return EXIT_FAILURE;
        }
    }

    if (gid) {
        if(set_gid(gid)) {
            LOG_ERROR("cannot set gid\n");
            return EXIT_FAILURE;
        }
    }

    if (uid) {
        if(set_uid(uid)) {
            LOG_ERROR("cannot set uid\n");
            return EXIT_FAILURE;
        }
    }

    if(optind >= argc) {
        LOG_ERROR("run command missing\n");
        return EXIT_FAILURE;
    }
    run(argc - optind, &argv[optind]);
    return EXIT_FAILURE; // reached only on error
}
