/***********************************************************************
 *
 * File: linux/tests/reglog/reglog.c
 * Description: Userspace program to control and extract register logging
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 \***********************************************************************/

//=============================================================================
//  I N C L U D E    F I L E S
//=============================================================================
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <linux/kernel/drivers/stm/fvdp/fvdp_ioctl.h>
#include "reglog.h"

//=============================================================================
//  D E F I N E S
//=============================================================================
#define DEVICE_FILE "/dev/fvdp"
#define col_delimiter " "

//=============================================================================
// S T A T I C   V A R I A B L E S
//=============================================================================
static int fd;
static reglog_t reglog;
static char col_fmt[REGLOG_MAX_COLS][16];
static int col_maxwidth[REGLOG_MAX_COLS];

//=============================================================================
// S T A T I C   F U N C T I O N S
//=============================================================================

static void usage()
{
    printf ("usage:\n");
    printf ("    configuration:\n");
    printf ("        -pos <pos>             set sampling position (pos:0=input, 1=output)\n");
    printf ("        -t <idx> <reg> <val>   set register to program prior to each sample\n");
    printf ("        -r <reg> ...           set register(s) to sample\n");
    printf ("        -a <reg> ...           add register(s) to sample\n");
    printf ("        -d <col> ...           delete column(s)\n");
    printf ("        -D                     delete all columns\n");
    printf ("        -s                     (re)start logging\n");
    printf ("        -sb                    (re)start logging in background\n");
    printf ("        -P                     force stop logging\n");
    printf ("    output:\n");
    printf ("        -h,--help              this help information\n");
    printf ("        --config               print current configuration to stdout\n");
    printf ("        --status               print current status to stdout\n");
    printf ("        -p [file]              print log to file if specified, otherwise stdout\n");
}

static bool fvdp_ioctl_reglog_get_config (reglog_t* reglog_p)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_REGLOG_GET_CONFIG;
    action.param[0] = (uint32_t) reglog_p;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
    {
        printf ("unable to retrieve configuration\n");
        return FALSE;
    }

    return TRUE;
}

static bool fvdp_ioctl_reglog_get_data (reglog_t* reglog_p)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    uint8_t* data_p = reglog_p->data_p;

    action.action_id = APPTEST_REGLOG_GET_LOG_DATA;
    action.param[0] = (uint32_t) reglog_p;
    action.param[1] = (uint32_t) data_p;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
        return FALSE;

    reglog_p->data_p = data_p;
    return TRUE;
}

static bool fvdp_ioctl_reglog_set_config (reglog_t* local)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_REGLOG_SET_CONFIG;
    action.param[0] = (uint32_t) local;
    local->rows_recorded = 0;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
    {
        printf ("unable to determine if log is active\n");
        return FALSE;
    }

    return TRUE;
}

static bool fvdp_ioctl_reglog_start_capture (void)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_REGLOG_START_CAPTURE;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
    {
        printf("Could not start log.\n");
        return FALSE;
    }

    printf("logging data...\n");

    return TRUE;
}

static bool fvdp_ioctl_reglog_stop_capture (void)
{
    FVDPIO_Apptest_t action;
    memset(&action, 0, sizeof(FVDPIO_Apptest_t));
    action.action_id = APPTEST_REGLOG_STOP_CAPTURE;

    if (ioctl(fd, FVDPIO_APPTEST, &action) < 0)
    {
        printf("Could not acquire reg log.\n");
        return FALSE;
    }

    return TRUE;
}

static bool reglog_setupreg_set (uint8_t index, uint32_t reg, uint32_t val)
{
    if (index >= REGLOG_MAX_SETUP_REGS)
        return FALSE;

    reglog.setup[index].reg_addr  = reg;
    reglog.setup[index].reg_value = val;
    return TRUE;
}

static bool reglog_check_active (void)
{
    reglog_t local;
    if (!fvdp_ioctl_reglog_get_config(&local))
        return FALSE;

    return local.active;
}

static bool reglog_column_set_print_fmt (uint8_t col, char* fmt)
{
    if (reglog.columns > REGLOG_MAX_COLS)
    {
        printf("%s: max columns of %d exceeded\n", __FUNCTION__, REGLOG_MAX_COLS);
        return FALSE;
    }

    strcpy(col_fmt[col],fmt);

    return TRUE;
}

static bool reglog_column_add (uint32_t reg_addr)
{
    if (reglog.columns >= REGLOG_MAX_COLS)
    {
        printf("%s: max columns of %d exceeded\n", __FUNCTION__, REGLOG_MAX_COLS);
        return FALSE;
    }

    //printf("adding column %d for register %x\n", reglog.columns, reg_addr);

    reglog.reg_addr[reglog.columns] = reg_addr;
    reglog.row_size += sizeof(uint32_t);

    reglog.columns ++;

    return TRUE;
}

static void reglog_column_reset_all (void)
{
    int col;
    for (col = 0; col < REGLOG_MAX_COLS; col++)
        reglog.reg_addr[col] = 0;
    reglog.columns = 0;
    reglog.row_size = 0;
    reglog.rows_recorded = 0;
}

static void reglog_column_delete (int col)
{
    if (reglog.reg_addr[col] != 0)
    {
        reglog.reg_addr[col] = 0;
        reglog.columns --;
    }
}

static void reglog_column_recount (void)
{
    int col;
    reglog.columns = 0;
    reglog.row_size = 0;
    for (col = 0; col < REGLOG_MAX_COLS; col++)
    {
        if (reglog.reg_addr[col] != 0)
        {
            reglog.reg_addr[reglog.columns++] = reglog.reg_addr[col];
            reglog.row_size += sizeof(uint32_t);
        }
    }
    for (col = reglog.columns; col < REGLOG_MAX_COLS; col ++)
        reglog.reg_addr[col] = 0;
}

static uint16_t reglog_column_find_max_width (reglog_t* reglog_p, uint8_t col)
{
    int row;
    int width, max_width = 0;
    char dummy_str[80];

    for (row = 0; row < reglog_p->rows_recorded; row ++)
    {
        uint8_t *addr = reglog_p->data_p + ((uint32_t) row * reglog_p->row_size);
        uint32_t data = *((uint32_t*) addr);

        sprintf(dummy_str, col_fmt[col], data);
        width = strlen(dummy_str);

        if (width > max_width)
            max_width = width;
    }

    return max_width;
}

static void reglog_print_config(reglog_t* local)
{
    uint8_t i, col;

    printf ("current configuration:\n");
    for (i = 0; i < REGLOG_MAX_SETUP_REGS; i++)
    {
        if (local->setup[i].reg_addr != 0)
            printf ("    setup  #%d: set  [%08X] = 0x%X (%u)\n", i,
                    local->setup[i].reg_addr, local->setup[i].reg_value, local->setup[i].reg_value);
    }
    if (local->columns == 0)
        printf("    columns not yet configured.\n");
    for (col = 0; col < local->columns; col ++)
    {
        if (local->reg_addr[col])
        {
            printf("    column #%d: read [%08X]\n", col, local->reg_addr[col]);
        }
    }
}

static void reglog_print_status(reglog_t* local)
{
    printf ("current status:\n");
    printf("    row_size = %d, rows_recorded = %d\n", local->row_size, local->rows_recorded);

    if (local->active)
    {
        printf ("    logging is ACTIVE\n");
    }
    else
    {
        printf ("    logging stopped or not active\n");
    }
}

static bool reglog_print_data (FILE* stream, reglog_t* reglog_p)
{
    char str[256], str1[32];
    uint8_t  col;
    uint16_t i, row;
    uint8_t* addr;
    uint32_t data;

    if (reglog_p->columns == 0)
    {
        printf ("no columns defined\n");
        return FALSE;
    }

    fprintf(stream,"\n");

    // first row to print contain register addresses
    sprintf (str, "#     ");
    for (col = 0; col < reglog_p->columns; col ++)
    {
        col_maxwidth[col] = reglog_column_find_max_width(reglog_p, col);

        sprintf (str1, "%X", reglog_p->reg_addr[col]);
        if (col_maxwidth[col] < strlen(str1))
            col_maxwidth[col] = strlen(str1);

        if (col > 0)
            strcat (str, col_delimiter);
        strcat (str, str1);
        for (i = strlen(str1); i < col_maxwidth[col]; i++)
            strcat (str, " ");
    }

    strcat (str, "\n");
    fprintf(stream,str);

    sprintf (str, "----- ");
    for (col = 0; col < reglog_p->columns; col ++)
    {
        if (col > 0)
            strcat (str, col_delimiter);
        for (i = 0; i < col_maxwidth[col]; i++)
            strcat (str, "-");
    }
    strcat (str, "\n");
    fprintf(stream,str);

    // print row data
    for (row = 0; row < reglog_p->rows_recorded; row++)
    {
        addr = reglog_p->data_p + ((uint32_t) row * reglog_p->row_size);

        sprintf (str, "%-5d ", row);

        for (col = 0; col < reglog_p->columns; col ++)
        {
            data = *((uint32_t*) addr);
            sprintf (str1, col_fmt[col], data);
            if (col > 0)
                strcat (str, col_delimiter);
            strcat (str, str1);
            for (i = strlen(str1); i < col_maxwidth[col]; i++)
                strcat (str, " ");
            addr += sizeof(uint32_t);
        }

        strcat (str, "\n");
        fprintf(stream,str);
    }

    return TRUE;
}

// convert string to uint32 (dec and hex supported)
static bool atoi32(char* str, uint32_t* val)
{
    if (*str == '0' && *(str+1) == 'x')
    {
        if (sscanf(str + 2, "%x", val) == 0)
            return FALSE;
    }
    else
    {
        if (sscanf(str, "%d", val) == 0)
            return FALSE;
    }

    return TRUE;
}

static bool get_u32_token(int* argc, char ***argv, uint32_t* val)
{
    char** arg = *argv;

    if (*argc == 0)
        return FALSE;

    if (!(*arg) || atoi32(*arg,val) == FALSE)
    {
        return FALSE;
    }

    (*argc) --;
    (*argv) ++;

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

//******************************************************************************
// M A I N
//******************************************************************************
int main(int argc, char **argv)
{
    int i;

    memset (&reglog, 0, sizeof(reglog_t));
    for (i = 0; i < REGLOG_MAX_COLS; i++)
        strcpy(col_fmt[i],"%x");

    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0)
    {
        printf("Opening %s failed.\n", DEVICE_FILE);
        goto exit;
    }

    if (argc == 1)
    {
        reglog_t local;
        usage();
        if (!fvdp_ioctl_reglog_get_config(&local))
        {
            printf ("unable to get active configuration\n");
            goto error;
        }
        reglog_print_config(&local);
        reglog_print_status(&local);
        goto exit;
    }

    argv++;

    while (argc && *argv)
    {
        if (!strcmp(*argv, "-pos"))
        {
            uint32_t pos;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &pos))
            {
                printf("expecting position (0 = input udpate, 1 = output update)\n");
                goto error;
            }

            fvdp_ioctl_reglog_get_config(&reglog);
            reglog.pos = pos;
            fvdp_ioctl_reglog_set_config(&reglog);
        }
        else
        if (!strcmp(*argv, "-t"))
        {
            uint32_t idx,reg,val;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &idx))
            {
                printf("expecting index number in range [0,%d]\n", REGLOG_MAX_SETUP_REGS-1);
                goto error;
            }

            if (!get_u32_token(&argc, &argv, &reg))
            {
                printf("expecting register address\n");
                goto error;
            }

            if (!get_u32_token(&argc, &argv, &val))
            {
                printf("expecting register value\n");
                goto error;
            }

            fvdp_ioctl_reglog_get_config(&reglog);
            reglog_setupreg_set(idx,reg,val);
            fvdp_ioctl_reglog_set_config(&reglog);
            continue;
        }
        else
        if (!strcmp(*argv, "-r"))
        {
            uint32_t reg;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &reg))
            {
                printf("expecting register address\n");
                goto error;
            }

            if (reglog_check_active())
            {
                printf("stopping logging activity... ");
                if (fvdp_ioctl_reglog_stop_capture()) printf("done.\n");
            }
            fvdp_ioctl_reglog_get_config(&reglog);
            reglog_column_reset_all();
            reglog_column_add(reg);
            while (get_u32_token(&argc, &argv, &reg))
                reglog_column_add(reg);
            fvdp_ioctl_reglog_set_config(&reglog);
            continue;
        }
        else
        if (!strcmp(*argv, "-a"))
        {
            uint32_t reg;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &reg))
            {
                printf("expecting register address\n");
                goto error;
            }

            if (reglog_check_active())
            {
                printf("stopping logging activity... ");
                if (fvdp_ioctl_reglog_stop_capture()) printf("done.\n");
            }
            fvdp_ioctl_reglog_get_config(&reglog);
            reglog_column_add(reg);
            while (get_u32_token(&argc, &argv, &reg))
                reglog_column_add(reg);
            reglog.rows_recorded = 0;
            fvdp_ioctl_reglog_set_config(&reglog);
            continue;
        }
        else
        if (!strcmp(*argv, "-d"))
        {
            uint32_t col;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &col))
            {
                printf("expecting column number\n");
                goto error;
            }

            if (reglog_check_active())
            {
                printf("stopping logging activity... ");
                if (fvdp_ioctl_reglog_stop_capture()) printf("done.\n");
            }
            fvdp_ioctl_reglog_get_config(&reglog);
            reglog_column_delete(col);
            while (get_u32_token(&argc, &argv, &col))
                reglog_column_delete(col);
            reglog_column_recount();
            reglog.rows_recorded = 0;
            fvdp_ioctl_reglog_set_config(&reglog);
            continue;
        }
        else
        if (!strcmp(*argv, "-D"))
        {
            uint32_t col;

            argc--;
            argv++;

            if (reglog_check_active())
            {
                printf("stopping logging activity... ");
                if (fvdp_ioctl_reglog_stop_capture()) printf("done.\n");
            }
            fvdp_ioctl_reglog_get_config(&reglog);
            reglog_column_reset_all();
            fvdp_ioctl_reglog_set_config(&reglog);
            continue;
        }
        else if (!strcmp(*argv, "-f"))
        {
            uint32_t col;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &col))
            {
                printf("expecting column\n");
                goto error;
            }

            if (!reglog_column_set_print_fmt(col, *argv))
                goto error;

            argc--;
            argv++;
        }
        else
        if (!strcmp(*argv, "-sb"))
        {
            argc--;
            argv++;
            fvdp_ioctl_reglog_start_capture();
            continue;
        }
        else
        if (!strcmp(*argv, "-s"))
        {
            reglog_t local;

            argc--;
            argv++;
            fvdp_ioctl_reglog_start_capture();

            printf ("number of rows captured: ");
            do
            {
                char number[16];
                int i;
                fvdp_ioctl_reglog_get_config(&local);
                sprintf(number, "%u/%u (%u%%)", (uint16_t) local.rows_recorded,
                    (uint16_t) (REGLOG_MAX_SIZE/local.row_size),
                    (uint16_t) (100 * local.rows_recorded / (REGLOG_MAX_SIZE/local.row_size)));
                printf("%s",number);

                if (getkey() != -1)
                {
                    printf (" -- user aborted");
                    fvdp_ioctl_reglog_stop_capture();
                    break;
                }

                fflush(stdout);
                for (i = 0; i < strlen(number); i++) printf("\b");
                usleep(200000);
            } while (local.active == TRUE);

            printf ("\n");

            continue;
        }
        else
        if (!strcmp(*argv, "-P"))
        {
            argc--;
            argv++;
            fvdp_ioctl_reglog_stop_capture();
            continue;
        }
        else
        if (!strcmp(*argv, "--config"))
        {
            reglog_t local;
            argc--;
            argv++;
            if (!fvdp_ioctl_reglog_get_config(&local))
            {
                printf ("unable to get active configuration\n");
                goto error;
            }
            reglog_print_config(&local);
            continue;
        }
        else
        if (!strcmp(*argv, "--status"))
        {
            reglog_t local;
            argc--;
            argv++;
            if (!fvdp_ioctl_reglog_get_config(&local))
            {
                printf ("unable to get active configuration\n");
                goto error;
            }
            reglog_print_status(&local);
            continue;
        }
        else
        if (!strcmp(*argv, "-p"))
        {
            FILE* stream = stdout;
            char* filename = NULL;

            argc--;
            argv++;
            reglog.data_p = malloc(REGLOG_MAX_SIZE);
            if (!reglog.data_p)
            {
                printf("unable to allocate %u bytes for log\n", REGLOG_MAX_SIZE);
                goto error;
            }

            if (argc > 0 && (*argv) && (*argv)[0] != '-')
            {
                filename = *argv;
                stream = fopen(*argv, "w+");
                if (!stream)
                {
                    printf("unable to open file %s\n", filename);
                    goto error;
                }
                argc --;
                argv ++;
            }

            if (reglog_check_active())
            {
                printf("stopping logging activity... ");
                if (fvdp_ioctl_reglog_stop_capture()) printf("done.\n");
            }

            fvdp_ioctl_reglog_get_data(&reglog);

            if (filename)
                printf("(%d rows recorded to \"%s\")\n", reglog.rows_recorded, filename);
            else
                printf("(%d rows recorded)\n", reglog.rows_recorded);

            reglog_print_data(stream,&reglog);
            free(reglog.data_p);

            if (stream != stdout)
                fclose(stream);
            continue;
        }
        else
        if (!strcmp(*argv, "-reg"))
        {
            FVDPIO_Reg_t reg;

            argc--;
            argv++;

            if (!get_u32_token(&argc, &argv, &reg.address))
            {
                printf("expecting register address\n");
                goto error;
            }
            else
            if (!get_u32_token(&argc, &argv, &reg.value))
            {
                if (ioctl(fd, FVDPIO_READ_REG, &reg) < 0)
                {
                    printf ("unable to perform register read\n");
                    goto error;
                }
                printf ("%x\n",reg.value);
            }
            else
            {
                if (ioctl(fd, FVDPIO_WRITE_REG, &reg) < 0)
                {
                    printf ("unable to perform register write\n");
                    goto error;
                }
            }
            continue;
        }
        else
        if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help"))
        {
            argc --;
            argv ++;
            usage();
            continue;
        }
        else
        {
            printf ("erroneous option \"%s\"\n", *argv);
            goto error;
        }
    }

exit:
    close(fd);
    return 0;

error:
    close(fd);
    usage();
    return -1;
}

