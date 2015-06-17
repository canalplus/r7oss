/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#if __KERNEL__
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#ifdef CONFIG_KPTRACE
#include <linux/relay.h>
#include <trace/kptrace.h>
#endif
#else
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#endif

#ifdef REPORT

#include "osinline.h"
#include "report.h"

#undef  TRACE_TAG
#define TRACE_TAG "report"

// global trace level and trace groups are enabled even if debug_fs not present:
// in this case, setting done only once (init) through module parameters;
// global trace level:
static unsigned int active_severity_level = severity_warning;
// trace groups metadata:
static struct SeTraceGroupSetting se_trace_groups[GROUP_LAST];
#if defined(__KERNEL__) && defined(CONFIG_KPTRACE) && defined(CONFIG_PRINTK)
// kptrace usage (off by default)
static bool  use_kptrace = false;
#endif // defined(__KERNEL__) && defined(CONFIG_KPTRACE) && defined(CONFIG_PRINTK)

#if defined(__KERNEL__) && defined(CONFIG_DEBUG_FS)

static ssize_t get_trace_group(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    int val = 1;
    int i;
    int trace_group = GROUP_LAST;

    struct SeTraceGroupSetting *group = file->private_data;
    if (group == NULL) { return 0; }

    // retrieve trace group index
    for (i = 0; i < GROUP_LAST; i++)
    {
        if (&se_trace_groups[i] == group)
        {
            trace_group = i;
            break;
        }
    }

    // indicates whether trace is active (level >= severity_info) or not for given group
    val = report_is_group_active(severity_info, trace_group);

    return simple_read_from_buffer(user_buf, count, ppos,
                                   val ? "1\n" : "0\n", 2);
}

static ssize_t set_trace_group(struct file *file, const char *user_buf, size_t count, loff_t *ppos)
{
    int val = 0;
    char buf[16];

    struct SeTraceGroupSetting *group = file->private_data;
    if (group == NULL) { return 0; }

    if (simple_write_to_buffer(buf, 16, ppos, user_buf, count) != count)
    {
        SE_ERROR("Cannot read the value you are trying to write\n");
        return count;
    }

    buf[count] = '\0';

    if (kstrtoint(buf, 10, &val) != 0)
    {
        SE_ERROR("Cannot read the value you are trying to write: Please try '%d' to '%d' or -1\n",
                 severity_fatal, severity_extraverb);
        return count;
    }

    if (val > severity_extraverb)
    {
        val = severity_extraverb;
    }
    group->max_severity_level = val;

    return count;
}

const struct file_operations trace_activation_fops =
{
    .open   = simple_open,
    .read   = get_trace_group,
    .write  = set_trace_group,
};

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31;1m"
#define KGRN  "\x1B[32;1m"
#define KPUR  "\x1B[35;1m"

#define DBG_ACTIVE_GROUP_LINE_SIZE 78
#define DBG_ACTIVE_GROUP_BUF_SIZE  4096

static const char *lookup_group_level(unsigned int group)
{

    bool enabled = report_is_group_active(severity_info, group);

    if (!enabled)
    {
        return KRED"disabled"KNRM;
    }

    if (se_trace_groups[group].max_severity_level < 0)
    {
        switch (active_severity_level)
        {
        case severity_info:
            return KGRN"INFO"KNRM;
            break;
        case severity_debug:
            return KGRN"DEBUG"KNRM;
            break;
        case severity_verbose:
            return KGRN"VERBOSE"KNRM;
            break;
        case severity_extraverb:
            return KGRN"EXTRAVERB"KNRM;
            break;
        default:
            return KRED"UNDEFINED"KNRM;
            break;
        }
    }
    else
    {
        switch (se_trace_groups[group].max_severity_level)
        {
        case severity_info:
            return KPUR"INFO"KNRM;
            break;
        case severity_debug:
            return KPUR"DEBUG"KNRM;
            break;
        case severity_verbose:
            return KPUR"VERBOSE"KNRM;
            break;
        case severity_extraverb:
            return KPUR"EXTRAVERB"KNRM;
            break;
        default:
            return KRED"UNDEFINED"KNRM;
            break;
        }
    }
}

static ssize_t get_active_groups(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    int i = 0, out_buf_size = 0;
    ssize_t ret = 0;
    char *out_buf, *out_buf_ptr;
    out_buf = OS_Malloc(DBG_ACTIVE_GROUP_BUF_SIZE);

    if (!out_buf)
    {
        SE_ERROR("Failed to allocate %d bytes\n", DBG_ACTIVE_GROUP_BUF_SIZE);
        return count;
    }

    out_buf_ptr = out_buf;

    snprintf(out_buf_ptr, 128,
             "\npossible trace levels: FATAL[0] ERROR[1] WARNING[2] INFO[3] DEBUG[4] VERBOSE[5] EXTRAVERB[6]\n\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    snprintf(out_buf_ptr, 128,
             "global trace level applies for fatal, error, warning trace points, and for group trace points with active level set to < 0;\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    snprintf(out_buf_ptr, 128,
             "trace level per group allows to choose if a trace group shall follow global trace level or be disabled or be enabled\n\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    snprintf(out_buf_ptr, 128,
             "to update only global trace level: echo [level value] > active_levels\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    snprintf(out_buf_ptr, 128,
             "to update *all* trace levels at once (including global level):\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    snprintf(out_buf_ptr, 128,
             "  cat active_levels; echo [updated list of values] > active_levels\n\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    // Print trace level info
    snprintf(out_buf_ptr, 128,
             "[global trace level:%d %s%s%s]\n",
             active_severity_level,
             KGRN,
             active_severity_level == severity_fatal     ? "FATAL" :
             active_severity_level == severity_error     ? "ERROR" :
             active_severity_level == severity_warning   ? "WARNING" :
             active_severity_level == severity_info      ? "INFO" :
             active_severity_level == severity_debug     ? "DEBUG" :
             active_severity_level == severity_verbose   ? "VERBOSE" :
             active_severity_level == severity_extraverb ? "EXTRAVERB" :
             "UNDEFINED",
             KNRM);
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    snprintf(out_buf_ptr, 128,
             "individual trace groups with associated level and name:\n");
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    // Print trace groups one by one
    for (i = 0 ; i < GROUP_LAST; i++)
    {
        if (out_buf_size >= (DBG_ACTIVE_GROUP_BUF_SIZE - DBG_ACTIVE_GROUP_LINE_SIZE))
        {
            break;
        }
        snprintf(out_buf_ptr, DBG_ACTIVE_GROUP_LINE_SIZE, "[id:%2d] [level:%2d %20s] %s%s%s\n",
                 i,
                 se_trace_groups[i].max_severity_level,
                 lookup_group_level(i),
                 se_trace_groups[i].dir_name != NULL ? se_trace_groups[i].dir_name : "",
                 se_trace_groups[i].dir_name != NULL ? "/" : "",
                 se_trace_groups[i].entry_name);
        out_buf_size += strlen(out_buf_ptr);
        out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'
    }

    out_buf[DBG_ACTIVE_GROUP_BUF_SIZE - 1] = '\0';
    ret = simple_read_from_buffer(user_buf, count, ppos, out_buf, out_buf_size);

    OS_Free(out_buf);

    return ret;
}

const struct file_operations active_groups_fops =
{
    .open   = simple_open,
    .read   = get_active_groups,
    .write  = NULL,
};

static ssize_t get_active_levels(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    int i = 0;
    ssize_t ret = 0;
    char out_buf[512];
    char *out_buf_ptr;
    int out_buf_size = 0;

    out_buf_ptr = out_buf;
    // Print global trace level info first
    snprintf(out_buf_ptr, 32, "%d", active_severity_level);
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    // Print trace groups one by one
    for (i = 0 ; i < GROUP_LAST; i++)
    {
        snprintf(out_buf_ptr, 32, ",%d", se_trace_groups[i].max_severity_level);
        out_buf_size += strlen(out_buf_ptr);
        out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'
    }

    snprintf(out_buf_ptr, 32, ",\n"); // keep last ","
    out_buf_size += strlen(out_buf_ptr);
    out_buf_ptr  += strlen(out_buf_ptr); // Overwrite the last '\0'

    out_buf[sizeof(out_buf) - 1] = '\0';
    ret = simple_read_from_buffer(user_buf, count, ppos, out_buf, out_buf_size);

    return ret;
}

static ssize_t set_active_levels(struct file *file, const char *user_buf, size_t count, loff_t *ppos)
{
    int i = -1;
    char in_buf[512];
    char *buf;
    char *token;
    int val;

    if (simple_write_to_buffer(in_buf, sizeof(in_buf), ppos, user_buf, count) != count)
    {
        return count;
    }

    in_buf[sizeof(in_buf) - 1] = '\0';

    buf = in_buf; /* since strsep modifies its pointer arg */
    for (token = strsep(&buf, ","); token != NULL; token = strsep(&buf, ","))
    {
        if (kstrtoint(token, 10, &val) != 0)
        {
            SE_ERROR("Cannot read the value you are trying to write: Please try '%d' to '%d' or -1\n",
                     severity_fatal, severity_extraverb);
            return count;
        }

        if (val > severity_extraverb)
        {
            val = severity_extraverb;
        }

        if (i >= 0 && i < GROUP_LAST)
        {
            if (-1 <= val)
            {
                se_trace_groups[i].max_severity_level = val;
                SE_WARNING("setting trace_groups[%d] trace level = %d\n", i, val);
            }
        }
        else
        {
            if (severity_fatal <= val)
            {
                active_severity_level = val;
                SE_WARNING("setting global trace level = %d\n", val);
            }
        }
        i++;

        if (i >= GROUP_LAST)
        {
            break;
        }
    }

    return count;
}

const struct file_operations active_levels_fops =
{
    .open   = simple_open,
    .read   = get_active_levels,
    .write  = set_active_levels,
};

static struct dentry *trace_dbg_dir;
#define TRACE_ACTIVATION_DIR_NAME "trace_activation"

#if defined(CONFIG_KPTRACE) && defined(CONFIG_PRINTK)
#define TUNEABLE_NAME_USE_KPTRACE  "trace_through_kptrace"
#endif

static void report_setup_debugfs(void)
{
    struct dentry *dir, *file;
    int i, j;

    // Use RegisterTuneable interface to install debugfs files
    // in the same directory than other tuneable (i.e. /debugfs/havana/)
    trace_dbg_dir = OS_RegisterTuneableDir(TRACE_ACTIVATION_DIR_NAME, NULL);
    if (trace_dbg_dir == NULL)
    {
        SE_ERROR("Failed to register tuneable directory:%s\n", TRACE_ACTIVATION_DIR_NAME);
        return;
    }

    for (i = 0 ; i < GROUP_LAST; i++)
    {
        if (se_trace_groups[i].entry_name == NULL)
        {
            SE_ERROR("NULL se_trace_groups[%d].entry_name\n", i);
            continue;
        }

        if (se_trace_groups[i].dir_name != NULL)
        {
            dir = NULL;

            // Look if dir entry has already been created and use it
            for (j = 0 ; j < GROUP_LAST; j++)
            {
                if (i != j &&
                    se_trace_groups[j].dir_name != NULL && se_trace_groups[j].dentry != NULL &&
                    !strcmp(se_trace_groups[i].dir_name, se_trace_groups[j].dir_name))
                {
                    dir = se_trace_groups[j].dentry;
                    break;
                }
            }

            // dir entry has not been created, create it now
            if (dir == NULL)
            {
                dir = debugfs_create_dir(se_trace_groups[i].dir_name, trace_dbg_dir);
                if (IS_ERR_OR_NULL(dir))
                {
                    SE_ERROR("Failed to create dir:%s\n", se_trace_groups[i].dir_name);
                    continue;
                }

                se_trace_groups[i].dentry = dir;
            }
        }
        else
        {
            // no specified dir: use default
            dir = trace_dbg_dir;
        }

        file = debugfs_create_file(se_trace_groups[i].entry_name, 0600, dir,
                                   (void *) &se_trace_groups[i], &trace_activation_fops);
        if (IS_ERR_OR_NULL(file))
        {
            SE_ERROR("Failed to create debugfs file:%s\n", se_trace_groups[i].entry_name);
        }
    }

    debugfs_create_file("active_groups", 0600, trace_dbg_dir, NULL, &active_groups_fops);
    debugfs_create_file("active_levels", 0600, trace_dbg_dir, NULL, &active_levels_fops);

#if defined(CONFIG_KPTRACE) && defined(CONFIG_PRINTK)
    OS_RegisterTuneable(TUNEABLE_NAME_USE_KPTRACE, (unsigned int *) &use_kptrace);
#endif
}

static void report_unsetup_debugfs(void)
{
#if defined(CONFIG_KPTRACE) && defined(CONFIG_PRINTK)
    OS_UnregisterTuneable(TUNEABLE_NAME_USE_KPTRACE);
#endif

    debugfs_remove_recursive(trace_dbg_dir);
    OS_UnregisterTuneable(TRACE_ACTIVATION_DIR_NAME); // Clean the Tuneable table
}

#else

static void report_setup_debugfs(void) {}
static void report_unsetup_debugfs(void) {}

#endif // defined(__KERNEL__) && defined(CONFIG_DEBUG_FS)

static void init_se_trace_groups(int trace_groups_levels[GROUP_LAST], int trace_global_level)
{
    int i;

    if (trace_global_level > severity_extraverb)
    {
        trace_global_level = severity_extraverb;
    }
    active_severity_level = trace_global_level;

    memset(se_trace_groups, 0, sizeof(se_trace_groups));

    for (i = 0 ; i < GROUP_LAST; i++)
    {
        if (trace_groups_levels[i] > severity_extraverb)
        {
            trace_groups_levels[i] = severity_extraverb;
        }

        if (trace_groups_levels[i] < severity_fatal)
        {
            se_trace_groups[i].max_severity_level = -1; // will use global level in that case
        }
        else
        {
            se_trace_groups[i].max_severity_level = trace_groups_levels[i];
        }
    }

    // for each group, init dir and entry name
    se_trace_groups[group_api].dir_name                         = NULL;
    se_trace_groups[group_api].entry_name                       = "api";

    se_trace_groups[group_player].dir_name                      = NULL;
    se_trace_groups[group_player].entry_name                    = "player";
    se_trace_groups[group_havana].dir_name                      = NULL;
    se_trace_groups[group_havana].entry_name                    = "havana";
    se_trace_groups[group_buffer].dir_name                      = NULL;
    se_trace_groups[group_buffer].entry_name                    = "buffer";
    se_trace_groups[group_decodebufferm].dir_name               = NULL;
    se_trace_groups[group_decodebufferm].entry_name             = "decodebufferm";
    se_trace_groups[group_timestamps].dir_name                  = NULL;
    se_trace_groups[group_timestamps].entry_name                = "timestamps";

    se_trace_groups[group_misc].dir_name                        = NULL;
    se_trace_groups[group_misc].entry_name                      = "misc";

    se_trace_groups[group_collator_audio].dir_name              = "collator";
    se_trace_groups[group_collator_audio].entry_name            = "collator_audio";
    se_trace_groups[group_collator_video].dir_name              = "collator";
    se_trace_groups[group_collator_video].entry_name            = "collator_video";

    se_trace_groups[group_esprocessor].dir_name                 = NULL;
    se_trace_groups[group_esprocessor].entry_name               = "esprocessor";

    se_trace_groups[group_frameparser_audio].dir_name           = "frameparser";
    se_trace_groups[group_frameparser_audio].entry_name         = "frameparser_audio";
    se_trace_groups[group_frameparser_video].dir_name           = "frameparser";
    se_trace_groups[group_frameparser_video].entry_name         = "frameparser_video";

    se_trace_groups[group_decoder_audio].dir_name               = "decoder";
    se_trace_groups[group_decoder_audio].entry_name             = "decoder_audio";
    se_trace_groups[group_decoder_video].dir_name               = "decoder";
    se_trace_groups[group_decoder_video].entry_name             = "decoder_video";

    se_trace_groups[group_encoder_stream].dir_name              = "encoder";
    se_trace_groups[group_encoder_stream].entry_name            = "encoder_stream";
    se_trace_groups[group_encoder_audio_preproc].dir_name       = "encoder";
    se_trace_groups[group_encoder_audio_preproc].entry_name     = "encoder_audio_preproc";
    se_trace_groups[group_encoder_audio_coder].dir_name         = "encoder";
    se_trace_groups[group_encoder_audio_coder].entry_name       = "encoder_audio_coder";
    se_trace_groups[group_encoder_video_preproc].dir_name       = "encoder";
    se_trace_groups[group_encoder_video_preproc].entry_name     = "encoder_video_preproc";
    se_trace_groups[group_encoder_video_coder].dir_name         = "encoder";
    se_trace_groups[group_encoder_video_coder].entry_name       = "encoder_video_coder";
    se_trace_groups[group_encoder_transporter].dir_name         = "encoder";
    se_trace_groups[group_encoder_transporter].entry_name       = "encoder_transporter";

    se_trace_groups[group_manifestor_audio_ksound].dir_name     = "manifestor";
    se_trace_groups[group_manifestor_audio_ksound].entry_name   = "manifestor_audio_ksound";
    se_trace_groups[group_manifestor_video_stmfb].dir_name      = "manifestor";
    se_trace_groups[group_manifestor_video_stmfb].entry_name    = "manifestor_video_stmfb";
    se_trace_groups[group_manifestor_audio_grab].dir_name       = "manifestor";
    se_trace_groups[group_manifestor_audio_grab].entry_name     = "manifestor_audio_grab";
    se_trace_groups[group_manifestor_video_grab].dir_name       = "manifestor";
    se_trace_groups[group_manifestor_video_grab].entry_name     = "manifestor_video_grab";
    se_trace_groups[group_manifestor_audio_encode].dir_name     = "manifestor";
    se_trace_groups[group_manifestor_audio_encode].entry_name   = "manifestor_audio_encode";
    se_trace_groups[group_manifestor_video_encode].dir_name     = "manifestor";
    se_trace_groups[group_manifestor_video_encode].entry_name   = "manifestor_video_encode";

    se_trace_groups[group_frc].dir_name                         = NULL;
    se_trace_groups[group_frc].entry_name                       = "frc";

    se_trace_groups[group_output_timer].dir_name                = NULL;
    se_trace_groups[group_output_timer].entry_name              = "output_timer";
    se_trace_groups[group_avsync].dir_name                      = NULL;
    se_trace_groups[group_avsync].entry_name                    = "avsync";

    se_trace_groups[group_audio_reader].dir_name                = "audio";
    se_trace_groups[group_audio_reader].entry_name              = "audio_reader";
    se_trace_groups[group_audio_player].dir_name                = "audio";
    se_trace_groups[group_audio_player].entry_name              = "audio_player";
    se_trace_groups[group_audio_generator].dir_name             = "audio";
    se_trace_groups[group_audio_generator].entry_name           = "audio_generator";
    se_trace_groups[group_mixer].dir_name                       = "audio";
    se_trace_groups[group_mixer].entry_name                     = "mixer";
}

void report_init(int trace_groups_levels[GROUP_LAST], int trace_global_level)
{
    init_se_trace_groups(trace_groups_levels, trace_global_level);

    report_setup_debugfs();
}

void report_term(void)
{
    report_unsetup_debugfs();
}

static inline void vreport_print(const char *format, va_list ap)
{
#if __KERNEL__
#if defined(CONFIG_PRINTK) && defined(CONFIG_KPTRACE)
    if (use_kptrace)
    {
        kpprintf((char *)format, ap);
    }
    else
    {
        vprintk(format, ap);
    }
#elif defined(CONFIG_KPTRACE)
    kpprintf((char *)format, ap);
#elif defined(CONFIG_PRINTK)
    vprintk(format, ap);
#endif // defined(CONFIG_PRINTK) && defined(CONFIG_KPTRACE)
#else
    vprintf(format, ap);
    fflush(stdout);
#endif // __KERNEL__
}

void report_print(report_severity_t report_severity, const char *format, ...)
{
    va_list ap;

    if (report_severity > active_severity_level) { return; }

    va_start(ap, format);
    vreport_print(format, ap);
    va_end(ap);
}

inline int report_is_group_active(report_severity_t report_severity, int trace_group)
{
    if (trace_group >= GROUP_LAST) { return 0; } // really an assert in fact would do better

    // for a group: if max_severity_level valid use it, else use global level
    if (se_trace_groups[trace_group].max_severity_level >= severity_fatal)
    {
        if (report_severity > se_trace_groups[trace_group].max_severity_level) { return 0; }
    }
    else
    {
        if (report_severity > active_severity_level) { return 0; }
    }

    return 1;
}

int report_print_stg(report_severity_t report_severity, int trace_group, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vreport_print(format, ap);
    va_end(ap);

    return 0;
}

void report_dump_hex(report_severity_t level,
                     unsigned char    *data,
                     int               length,
                     int               width,
                     void             *start)
{
    int n, i;
    char str[256];
    size_t len = sizeof(str); // char table
    int l;

    for (n = 0; n < length; n += width)
    {
        char *str2 = str;
        l = snprintf(str2, len, "0x%08x:", (unsigned int)start + n);
        len -= l;
        str2 += l;

        for (i = 0; i < ((length - n) < width ? (length - n) : width); i++)
        {
            l = snprintf(str2, len, " %02x", data[n + i] & 0x0ff);
            len -= l;
            str2 += l;
        }

        l = snprintf(str2, len, "\n");

        str[255] = '\0';
        report_print(level, "%s", (const char *)str);
    }
}

#endif // REPORT
