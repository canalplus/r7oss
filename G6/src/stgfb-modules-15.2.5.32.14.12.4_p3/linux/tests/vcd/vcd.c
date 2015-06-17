/***********************************************************************
 *
 * File: linux/tests/vcd/vcd.c
 * Description: Userspace program to control debug log registers and write
 * log into a file in VCD format
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

//******************************************************************************
//  I N C L U D E    F I L E S
//******************************************************************************
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>

#include <linux/kernel/drivers/stm/fvdp/fvdp_ioctl.h>
#include "vcd_print.h"

//******************************************************************************
//  D E F I N E S
//******************************************************************************
#define DEVICE_FILE "/dev/fvdp"

#define MAX_CHANNELS    4

//******************************************************************************
//  E N U M S
//******************************************************************************

typedef enum
{
    HOST_FVDP_LOG = 0,
    FVDP_VCPU_LOG,
    MAX_LOGS
} VcdLogSource_t;

typedef enum
{
    ARG_HELP,
    ARG_HELP_EVENTS,
    ARG_PROCESSOR,
    ARG_PROCESSOR_HOST_FVDP,
    ARG_PROCESSOR_FVDP_VCPU,
    ARG_CHANNEL_MAIN,
    ARG_CHANNEL_PIP,
    ARG_CHANNEL_AUX,
    ARG_CHANNEL_ENC,
    ARG_CAPTURE_MODE__DEPRACATED,
    ARG_CAPTURE_TRIG__DEPRACATED,
    ARG_STOP,
    ARG_AUTO,
    ARG_TRIG,
    ARG_SINGLE,
    ARG_MAX
} arg_t;

typedef struct
{
    arg_t arg_type;
    const char* arg_str;
    const char* desc;
} arg_dict_t;

//******************************************************************************
// L O C A L   V A R I A B L E S
//******************************************************************************
static int fd;

static const char* out_filename[MAX_LOGS] =
{
    "arm_log.vcd",
    "vcpu_log.vcd"
};

static const char* processor_str[MAX_LOGS] =
{
    "HOST-ARM_A9 FVDP",
    "STxP70 VCPU",
};

static const char* capture_mode_str[4] =
{
    "stop",
    "auto",
    "trigger",
    "single"
};

static const char* channel_str[4] =
{
    "main",
    "PIP",
    "AUX",
    "ENC"
};

static const arg_dict_t dict[] =
{
    {   ARG_HELP,                       "--?"               , "show help"                               },
    {   ARG_HELP_EVENTS,                "--show-events"     , "list all events"                         },

    {   ARG_CHANNEL_MAIN,               "main"              , "select MAIN channel"                     },
    {   ARG_CHANNEL_PIP,                "pip"               , "select PIP channel"                      },
    {   ARG_CHANNEL_AUX,                "aux"               , "select AUX channel"                      },
    {   ARG_CHANNEL_ENC,                "enc"               , "select ENC channel"                      },
    {   ARG_PROCESSOR_HOST_FVDP,        "host"              , "select HOST FVDP log"                    },
    {   ARG_PROCESSOR_FVDP_VCPU,        "vcpu"              , "select FVDP VCPU log"                    },
    {   ARG_STOP,                       "stop"              , "stop capture"                            },
    {   ARG_AUTO,                       "auto"              , "select AUTO capture mode"                },
    {   ARG_SINGLE,                     "single"            , "select SINGLE capture mode"              },
    {   ARG_TRIG,                       "trigger"           , "select TRIGGER capture mode"             },

    {   ARG_PROCESSOR,                  "-s"                , "select processor/module (depracated)"    },
    {   ARG_CAPTURE_MODE__DEPRACATED,   "-m"                , "select capture mode (depracated)"        },
    {   ARG_CAPTURE_TRIG__DEPRACATED,   "-t"                , "select trigger (depracated)"             },

    {   ARG_MAX,                        ""                  , ""                                        }
};

//******************************************************************************
// S T A T I C   F U N C T I O N S
//******************************************************************************
static void usage()
{
    int i = 0;
    printf("Usage: vcd [host|vcpu] [main|pip|aux|enc]\n"
           "           [stop|auto|single|trigger <trigger>]\n");
    printf("Options:\n");

    do
    {
        printf ("\t%-20s%s\n", dict[i].arg_str, dict[i].desc);
    } while (dict[++i].arg_type != ARG_MAX);
}

#if 0   // currently not in use
static int fvdp_ioctl_register_read (FVDPIO_Reg_t* reg, uint32_t addr)
{
    reg->address = addr;
    return ioctl(fd, FVDPIO_READ_REG, reg);
}

static int fvdp_ioctl_register_write (FVDPIO_Reg_t* reg, uint32_t addr, uint32_t value)
{
    reg->address = addr;
    reg->value = value;
    return ioctl(fd, FVDPIO_WRITE_REG, reg);
}
#endif

static bool fvdp_ioctl_vcdlog_set_source (uint32_t source)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_VCDLOG_SET_SOURCE;
    action.param[0] = source;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
    {
        printf ("unable to set VCD log source\n");
        return FALSE;
    }

    return TRUE;
}

static bool fvdp_ioctl_vcdlog_get_config (vcdlog_config_t* config_p)
{
    FVDPIO_Apptest_t action;

    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_VCDLOG_GET_CONFIG;
    action.param[0] = (uint32_t) config_p;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
        return FALSE;

    return TRUE;
}

static bool fvdp_ioctl_vcdlog_set_mode (uint8_t mode, uint8_t trig)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_VCDLOG_SET_MODE;
    action.param[0] = mode;
    action.param[1] = trig;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
        return FALSE;

    return TRUE;
}

static int getkey (void) {
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}

arg_t find_arg_in_dict (char* arg_str)
{
    int i = 0;
    do
    {
        if (!strcmp(arg_str, dict[i].arg_str))
        {
            //printf ("found \"%s\" at index %d\n", dict[i].arg_str, i);
            return dict[i].arg_type;
        }
    } while (dict[++i].arg_type != ARG_MAX);

    //printf ("couldn't find \"%s\"\n", arg_str);

    return ARG_MAX;
}

static bool get_vcd_log_data (int fd, FVDPIO_Data_t* container)
{
    // get info into container.kaddr, container.size
    if (ioctl(fd, FVDPIO_GET_DATA_INFO, container) < 0)
    {
        printf(" ioctl error\n");
        return FALSE;
    }

    if (container->kaddr == 0 || container->size == 0)
    {
        printf(" bad params returned\n");
        return FALSE;
    }

    container->uaddr = malloc(container->size);
    if (!container->uaddr)
    {
        printf (" error allocating buffer\n");
        return FALSE;
    }

    // now get container data
    if (ioctl(fd, FVDPIO_COPY_DATA_TO_USER, container) < 0)
    {
        printf(" ioctl error\n");
        return FALSE;
    }

    return TRUE;
}

//******************************************************************************
// M A I N
//******************************************************************************
int main(int argc, char **argv)
{
    bool help = FALSE;
    bool help_events = FALSE;
    bool assumptions = FALSE;
    int logMode = -1;
    int trigEvent = -1;
    int ch = -1;
    int cpu = -1;
    int i;
    FVDPIO_Data_t container;
    VCDLog_t* log_p;
    vcdlog_config_t config;

    fd = open(DEVICE_FILE, O_RDWR);

    if (    fvdp_ioctl_vcdlog_get_config(&config) == FALSE
        ||  config.mode == 0xFF )
    {
        printf ("Unable to retrieve configuration from FVDP driver module!\n" \
                "Please check that:\n" \
                "  (1) FVDP module is loaded\n" \
                "  (2) FVDP is compiled with VCD_OUTPUT_ENABLE set to TRUE\n");
        goto exit;
    }

    argc--;
    argv++;

    while (argc-- > 0)
    {
        char* arg_str = *(argv++);
        arg_t arg = find_arg_in_dict(arg_str);

        switch(arg)
        {
            case ARG_HELP:
                help = TRUE;
                usage();
                goto exit;

            case ARG_HELP_EVENTS:
                help_events = TRUE;
                break;

            case ARG_CHANNEL_MAIN:
                ch = 0;
                break;

            case ARG_CHANNEL_PIP:
                ch = 1;
                break;

            case ARG_CHANNEL_AUX:
                ch = 2;
                break;

            case ARG_CHANNEL_ENC:
                ch = 3;
                break;

            case ARG_PROCESSOR:
                cpu = atoi(*(argv++));
                argc--;
                break;

            case ARG_CAPTURE_TRIG__DEPRACATED:
                trigEvent = atoi(*(argv++));
                argc--;
                break;

            case ARG_CAPTURE_MODE__DEPRACATED:
                logMode = atoi(*(argv++));
                argc--;
                break;

            case ARG_PROCESSOR_HOST_FVDP:
                cpu = 0;
                break;

            case ARG_PROCESSOR_FVDP_VCPU:
                cpu = 1;
                break;

            case ARG_STOP:
                logMode = 0;
                break;

            case ARG_AUTO:
                logMode = 1;
                break;

            case ARG_TRIG:
                logMode = 2;
                if (argc > 0)
                {
                    trigEvent = atoi(*(argv++));
                    argc--;
                }
                break;

            case ARG_SINGLE:
                logMode = 3;
                break;

            default:
                printf ("invalid arg \"%s\"\n", arg_str);
                help = TRUE;
                goto error;
        }
    }

    if (ch == -1)
    {
        ch  = (config.source >> 4) & 0x0F;
        assumptions = TRUE;
    }
    if (cpu == -1)
    {
        cpu = (config.source >> 0) & 0x0F;
        assumptions = TRUE;
    }
    if (trigEvent == -1)
    {
        trigEvent = config.trig;
        assumptions = TRUE;
    }

    if (logMode == -1 && help_events == FALSE)
    {
        printf ("please specify one of {stop,auto,single,trigger}\n\n");
        help = TRUE;
        goto error;
    }

    if (assumptions == TRUE)
    {
        printf ("(assuming ");
        if (help_events == FALSE)
        {
            printf ("capture mode %s ", capture_mode_str[logMode]);
            if (logMode == VCDLOGMODE_TRIG)
            {
                printf ("on event %d ", trigEvent);
            }
            printf ("for ");
        }
        printf ("channel %s on %s", channel_str[ch], processor_str[cpu]);
        printf (")\n");
    }

    config.source = ((ch & 0x0F) << 4) | ((cpu & 0x0F) << 0);

    container.type      = FVDPIO_TYPE_VCD_LOG;
    container.param0    = config.source;
    container.uaddr     = 0;

    if (logMode == VCDLOGMODE_TRIG && trigEvent < 0)
    {
        printf("Please specify trigger.\n");
        help_events = TRUE;
    }

    if (cpu >= MAX_LOGS)
    {
        printf("invalid processor module selection %d\n", cpu);
        goto error;
    }

    if (ch  >= MAX_CHANNELS)
    {
        printf("invalid channel selection %d\n", ch);
        goto error;
    }

    if (    help_events == FALSE
        &&  (logMode < VCDLOGMODE_STOP || logMode > VCDLOGMODE_SINGLE) )
    {
        printf("invalid log mode selection %d\n", logMode);
    }

    if (fd < 0)
    {
        printf("Opening %s failed.\n", DEVICE_FILE);
        goto exit;
    }

    /* Set VCD log source (cpu and channel) */
    if (fvdp_ioctl_vcdlog_set_source(config.source) == FALSE)
        goto exit;

    usleep(300000);     // allow ARM/VCPU to restore previous state in spare regs

    if (help_events == TRUE)
    {
        printf ("\n");

        if (get_vcd_log_data(fd, &container) == FALSE)
        {
            printf ("error getting log data!\n");
            goto exit;
        }

        log_p = (VCDLog_t*) container.uaddr;

        VCD_PrintModulesEvents(log_p);
        goto exit;
    }

    if (fvdp_ioctl_vcdlog_set_mode(VCDLOGMODE_STOP, 0) == FALSE)
    {
        printf ("unable to stop VCD logging\n");
        goto exit;
    }

    usleep(200000);     // allow ARM/VCPU to make complete stop first

    if (fvdp_ioctl_vcdlog_set_mode(logMode, trigEvent) == FALSE)
    {
        printf ("unable to set VCD log to mode %d\n", logMode);
        goto exit;
    }

    if (logMode == VCDLOGMODE_STOP)
        goto exit;

    /* If the log mode was auto, then prompt user to issue stop command */
    if (logMode == VCDLOGMODE_AUTO)
    {
        printf ("Logging data.  Press any key to stop.\n");
        while (getkey() == -1)
        {
            char str[32];

            if (fvdp_ioctl_vcdlog_get_config(&config) == FALSE)
            {
                printf ("unable to set VCD log mode\n");
                goto error;
            }

            sprintf(str, "(%u entries recorded)", config.size);
            printf("%s",str);

            fflush(stdout);
            for (i = 0; i < strlen(str); i++) printf("\b");
            usleep(200000);
        }
        printf ("\n");

        if (fvdp_ioctl_vcdlog_set_mode(VCDLOGMODE_STOP, 0) == FALSE)
        {
            printf ("unable to set VCD log mode\n");
            goto exit;
        }
    }

    /* Wait until stop command appears in control register. Once it does, copy the vcd log from kernel to user space.
    * Then output the vcd log in a .vcd file.
    */
    if (    logMode == VCDLOGMODE_SINGLE
        ||  logMode == VCDLOGMODE_TRIG   )
    {
        bool stop_req = FALSE;
        printf ("capturing data ... ");
        fflush(stdout);

        do
        {
            char str[32];

            if (fvdp_ioctl_vcdlog_get_config(&config) == FALSE)
            {
                printf ("unable to set VCD log mode\n");
                goto error;
            }

            sprintf(str, "(%u entries recorded)", config.size);
            printf("%s",str);

            if (fvdp_ioctl_vcdlog_get_config(&config) == FALSE)
            {
                printf ("unable to get VCD log mode\n");
                goto error;
            }

            usleep(200000);

            if (getkey() != -1)
            {
                printf (" - stop requested\n");
                if (fvdp_ioctl_vcdlog_set_mode(VCDLOGMODE_STOP, 0) == FALSE)
                {
                    printf ("unable to set VCD log mode\n");
                    goto exit;
                }
                stop_req = TRUE;
                break;
            }

            fflush(stdout);
            for (i = 0; i < strlen(str); i++) printf("\b");
            usleep(200000);
        } while (config.mode != VCDLOGMODE_STOP);

        if (!stop_req)
            printf ("\n");
    }

    printf ("retrieving log info...");

    if (get_vcd_log_data(fd, &container) == FALSE)
        goto exit;

    printf (" (%d bytes)\n", container.size);

    log_p = (VCDLog_t*) container.uaddr;
    printf ("rows retrieved = %d\n", log_p->buf_length);
    printf ("num modules    = %d\n", log_p->num_modules);
    printf ("num events     = %d\n", log_p->num_events);

    if (log_p->num_modules > VCD_MAX_NUM_OF_MODULES)
    {
        printf("!!! max number of modules (%d) exceeded !!!\n", VCD_MAX_NUM_OF_MODULES);
        goto exit;
    }

    if (log_p->num_events > VCD_MAX_NUM_OF_EVENTS)
    {
        printf("!!! max number of events (%d) exceeded !!!\n", VCD_MAX_NUM_OF_EVENTS);
        goto exit;
    }

    printf ("writing to \"%s\"... ", out_filename[cpu]);

    FILE* stream;
    stream = fopen(out_filename[cpu],"w+");
    if (stream == NULL)
    {
        printf("failed.\n");
        goto exit;
    }

    VCD_SetOutputFile(stream);
    VCD_PrintLogBuffer(log_p);

    printf ("done\n");

exit:
    if (container.uaddr) free(container.uaddr);
    close(fd);
    return 0;

error:
    if (container.uaddr) free(container.uaddr);
    close(fd);
    if (help) usage();
    return -1;
}
