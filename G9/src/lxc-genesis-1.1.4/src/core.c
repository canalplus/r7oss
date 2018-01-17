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
#include <sys/capability.h>
#include <sys/prctl.h>
#include <linux/securebits.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "config.h"
#include "utils.h"

#define GENESIS_CONFIG_FILE "/lxc_genesis.conf"

typedef struct {
  char *user;
  char *group;
  int umask;
} config_t;

static int set_uid(const char *uid)
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

static int set_gid(const char *gid)
{
    struct group *result;
    gid_t rgid, egid, sgid;

    if (!(result = getgrnam(gid))) {
        perror ("getgrnam error");
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

static int change_ids(const char *uid, const char *gid)
{
    int i;
    int ret = 0;
    struct passwd *result;
    uid_t ruid, euid, suid;

    /*
     * Those capabilities are set by default
     * in the lxc configurations by alcatraz
     */
    cap_value_t cap_values[] = {
        CAP_SETUID,
        CAP_SETGID,
        CAP_SETPCAP
    };
    int ncaps = sizeof(cap_values)/sizeof(cap_values[0]);
    cap_flag_value_t value;
    cap_t caps = cap_get_proc();

    if (caps == NULL){
        perror("cap_get_proc() failed\n");
        return 1;
    }

    /* Check the capabilities */
    for (i=0; i < ncaps; i++) {
        if (cap_get_flag(caps, cap_values[i], CAP_EFFECTIVE, &value)) {
            perror("cap_get_flag() effective failed");
            ret = 1;
            goto err;
        }

        if (value != CAP_SET) {
            if (cap_free(caps) == -1){
                perror("Unable to free capabilities using cap_free()\n");
                ret = 1;
                goto err;
            }

            LOG_ERROR("Missing capabilities : "
                "needs setuid, setgid and setpcap capabilities "
                "to change process id.\n");
            ret = 1;
            goto err;
         }
    }

    /* We want to keep the caps across uid change */
    if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0)) {
        perror("prctl(PR_SET_KEEPCAPS,1) failed\n");
        ret = 1;
        goto err;
    }

    if (uid) {
        /* Establishing a capabilities-only environment */
        if (prctl(  PR_SET_SECUREBITS,
                    SECBIT_KEEP_CAPS_LOCKED |
                    SECBIT_NO_SETUID_FIXUP |
                    SECBIT_NO_SETUID_FIXUP_LOCKED |
                    SECBIT_NOROOT |
                    SECBIT_NOROOT_LOCKED)) {
            perror("prctl(PR_SET_SECUREBITS) failed\n");
            ret = 1;
            goto err;
        }
    }

    /* Change process id */
    if (gid) {
        if(set_gid(gid)) {
            LOG_ERROR("cannot set gid\n");
            ret = 1;
            goto err;
        }
    }

    if (uid) {
        if(set_uid(uid)) {
            LOG_ERROR("cannot set uid\n");
            ret = 1;
            goto err;
        }
    } else {
        /* making sure groups are always well initialized */
        if (getresuid(&ruid, &euid, &suid)) {
            perror ("getresuid");
	    ret = 1;
	    goto err;
	}
	if (!(result = getpwuid(euid))) {
	    perror ("getpwuid error");
	    ret = 1;
	    goto err;
	}
	if (initgroups(result->pw_name, result->pw_gid)) {
	    perror ("initgroups");
	    ret = 1;
	    goto err;
	}
    }

    /* Capability to be attached via ptrace and to coredump, disabled in prod mode */
#ifdef ENABLE_HARDENED
    if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0)) {
        perror("prctl(PR_SET_DUMPABLE,0) failed\n");
        ret = 1;
        goto err;
    }
#else
    if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0)) {
        perror("prctl(PR_SET_DUMPABLE,1) failed\n");
        ret = 1;
        goto err;
    }
#endif /* ENABLE_HARDENED */

    /* Drop capabilities */
    if (cap_set_flag(caps, CAP_EFFECTIVE, ncaps, cap_values, CAP_CLEAR)  == -1) {
        perror("cap_set_flag() effective failed\n");
        ret = 1;
        goto err;
    }

    if (cap_set_flag(caps, CAP_INHERITABLE, ncaps, cap_values, CAP_CLEAR) == -1) {
        perror("cap_set_flag() inheritable failed\n");
        ret = 1;
        goto err;
    }

    if (cap_set_flag(caps, CAP_PERMITTED, ncaps, cap_values, CAP_CLEAR) == -1) {
        perror("cap_set_flag() permitted failed\n");
        ret = 1;
        goto err;
    }

    if (cap_set_proc(caps) == -1) {
        perror("Setting process capabilities by cap_set_proc() failed\n");
        ret = 1;
        goto err;
    }

err:
    if (cap_free(caps) == -1) {
        perror("Unable to free capabilities using cap_free()\n");
        return 1;
    }

    return ret;
}

struct rescase { char* string; int ressource; };

struct rescase cases [] =
{ { "RLIMIT_AS", RLIMIT_AS },
  { "RLIMIT_CORE", RLIMIT_CORE },
  { "RLIMIT_CPU", RLIMIT_CPU },
  { "RLIMIT_DATA", RLIMIT_DATA },
  { "RLIMIT_FSIZE", RLIMIT_FSIZE },
  { "RLIMIT_LOCKS", RLIMIT_LOCKS },
  { "RLIMIT_MEMLOCK", RLIMIT_MEMLOCK },
  { "RLIMIT_MSGQUEUE", RLIMIT_MSGQUEUE },
  { "RLIMIT_NICE", RLIMIT_NICE },
  { "RLIMIT_NOFILE", RLIMIT_NOFILE },
  { "RLIMIT_NPROC", RLIMIT_NPROC },
  { "RLIMIT_RSS", RLIMIT_RSS },
  { "RLIMIT_SIGPENDING", RLIMIT_SIGPENDING },
  { "RLIMIT_STACK", RLIMIT_STACK }
};

#define nr_rescase (sizeof(cases)/sizeof(cases[0]))

static int rescase_cmp( const void *p1, const void *p2 )
{
    return strcmp( ((struct rescase*)p1)->string, ((struct rescase*)p2)->string);
}

static int get_rlimit_ressource_int (char *resource)
{
    struct rescase res;

    qsort(cases, nr_rescase, sizeof(struct rescase), rescase_cmp);

    res.string = resource;
    void* strptr = bsearch( &res, cases, nr_rescase, sizeof( struct rescase), rescase_cmp );
    if (strptr) {
        struct rescase* res_val = (struct rescase*)strptr;
        return res_val->ressource;
    }
    return -1;
}

static int set_rlimit(char *s_resource, const char *s_limit_cur, const char *s_limit_max)
{
    int ret;
    struct rlimit rlp;
    rlim_t limit_cur = strtoull(s_limit_cur, NULL, 10);
    rlim_t limit_max = strtoull(s_limit_max, NULL, 10);
    int resource = get_rlimit_ressource_int (s_resource);

    if (resource < 0)
        return -1; // unknown ressource

    if(limit_max == ULONG_MAX || limit_cur == ULONG_MAX)
        return 1; // overflow
    rlp.rlim_max = limit_max;
    rlp.rlim_cur = limit_cur;
    if((ret = setrlimit(resource, &rlp)))
        return ret;
    if((ret = getrlimit(resource, &rlp)))
        return ret;
    if(rlp.rlim_max != limit_max || rlp.rlim_cur != limit_cur)
        return -2; // limit not set
    return ret;
}

static bool config_callback(const config_entry_t *entry, void *priv)
{
    config_t *config = (config_t *)priv;

    switch(entry->type) {
        case CONFIG_ENTRY_USER:
            config->user = lxc_genesis_strdup(entry->u.user);
            LOG_INFO("set user to '%s'\n", config->user);
            break;

        case CONFIG_ENTRY_GROUP:
            config->group = lxc_genesis_strdup(entry->u.group);
            LOG_INFO("set group to '%s'\n", config->group);
            break;

        case CONFIG_ENTRY_UMASK:
            config->umask = entry->u.umask;
            LOG_INFO("set umask to %#o\n", config->umask);
            break;

        case CONFIG_ENTRY_RLIMIT:
            LOG_INFO("set rlimit '%s' to current '%s', maximum '%s'\n",
                entry->u.rlimit.name, entry->u.rlimit.current,
                entry->u.rlimit.maximum);
            if (set_rlimit(entry->u.rlimit.name, entry->u.rlimit.current,
                        entry->u.rlimit.maximum) != 0) {
                fprintf (stderr, "ERROR: cannot set rlimit '%s'\n",
                        entry->u.rlimit.name);
                return false;
            }
            break;


        case CONFIG_ENTRY_ENV:
            LOG_INFO("set environment variable '%s' '%s'\n",
                entry->u.env.name, entry->u.env.value);
            if (setenv(entry->u.env.name, entry->u.env.value, 1)) {
                fprintf (stderr, "ERROR: cannot set environment variable "
                        "'%s'\n", entry->u.env.name);
                return false;
            }
            break;

        default:
            fprintf(stderr, "internal error while loading configuration "
                    "file\n");
            return false;
    }

    return true;
}

void lxc_genesis_setup () {
    int r = 0;
    FILE *config_file = NULL;
    config_t config;

    config.user = config.group = NULL;
    config.umask = -1;

#ifndef ENABLE_HARDENED
    char *conf = getenv("GENESIS_CONFIG_FILE");
    if (conf)
    {
        fprintf(stderr,
            "####LXC-Genesis################################\n"
            "#                -- NOTICE --      [dev mode] #\n"
            "# (i) You are using custom config file.       #\n"
            "#     NOTE: this is not allowed in prod mode. #\n"
            "###############################################\n"
        );
        config_file = fopen(conf, "r");
    }
    else
#endif /* ENABLE_HARDENED */
    {
        config_file = fopen(GENESIS_CONFIG_FILE, "r");
    }

    if (!config_file) {
#ifndef ENABLE_HARDENED
        fprintf(stderr,
            "####LXC-Genesis##############################################\n"
            "#                       -- WARNING --            [dev mode] #\n"
            "# (!) Config file '/lxc_genesis.conf' not found!            #\n"
            "#     Running component as root:root with default settings. #\n"
            "#     NOTE: this is not allowed in prod mode.               #\n"
            "#############################################################\n"
        );
        return;
#endif /* ENABLE_HARDENED */
        perror ("open /lxc_genesis.conf");
        exit (EXIT_FAILURE);
    }

    r = lxc_genesis_config_load(config_file, config_callback, &config);

    fclose (config_file);

    if (!r)
        exit (EXIT_FAILURE);

    r = change_ids(config.user, config.group);

    if (config.umask != -1)
        umask(config.umask);
    lxc_genesis_free(config.user);
    lxc_genesis_free(config.group);

    if (r)
        exit(EXIT_FAILURE);

    r = unsetenv("PATH");

    if (r)
        exit(EXIT_FAILURE);
}
