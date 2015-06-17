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

#ifndef __REPORT_H
#define __REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fatal_error.h"
#include "osinline.h"

#ifdef CONFIG_ARCH_STI
#include <linux/kern_levels.h>  // for kernel 3.10 and above
#else
#ifndef __KERNEL_PRINTK__
#define KERN_EMERG      "<0>"/* system is unusable*/
#define KERN_ALERT      "<1>"/* action must be taken immediately*/
#define KERN_CRIT       "<2>"/* critical conditions*/
#define KERN_ERR        "<3>"/* error conditions*/
#define KERN_WARNING    "<4>"/* warning conditions*/
#define KERN_NOTICE     "<5>"/* normal but significant condition*/
#define KERN_INFO       "<6>"/* informational*/
#define KERN_DEBUG      "<7>"/* debug-level messages*/
#endif  // __KERNEL_PRINTK__
#endif  // CONFIG_ARCH_STI

typedef enum
{
    severity_fatal          = 0,
    severity_error          = 1,
    severity_warning        = 2,
    severity_info           = 3,
    severity_debug          = 4,
    severity_verbose        = 5,
    severity_extraverb      = 6,
} report_severity_t;

struct SeTraceGroupSetting
{
    // don't use report_severity_t for max_severity_level since might be set to -1 to indicate use of default global level
    int         max_severity_level; // maximum severity level requested for this group
    const char *dir_name;           // dir name to be used for debugfs
    const char *entry_name;         // name to be used for debugfs
    void       *dentry;             // dentry
};

// trace groups
// when adding a group:
// 1. report.c init_se_trace_groups() has to be updated for debugfs dir name and entry name
// 2. player_module.c trace_groups_levels_init[] has to be updated for module init time default level
enum report_groups
{
    // API groups
    group_api,

    // central/wrapper groups
    group_player,
    group_havana,
    group_buffer,
    group_decodebufferm,
    group_timestamps,
    group_misc,

    // collator groups
    group_collator_audio,
    group_collator_video,
    group_esprocessor,

    // frameparser groups
    group_frameparser_audio,
    group_frameparser_video,

    // decoder groups
    group_decoder_audio,
    group_decoder_video,

    // encoder groups
    group_encoder_stream,
    group_encoder_audio_preproc,
    group_encoder_audio_coder,
    group_encoder_video_preproc,
    group_encoder_video_coder,
    group_encoder_transporter,

    // manifestor groups
    group_manifestor_audio_ksound,
    group_manifestor_video_stmfb,
    group_manifestor_audio_grab,
    group_manifestor_video_grab,
    group_manifestor_audio_encode,
    group_manifestor_video_encode,

    // FRC group
    group_frc,

    // timer/AVSYNC groups
    group_output_timer,
    group_avsync,

    // audio groups
    group_audio_reader,
    group_audio_player,
    group_audio_generator,
    group_mixer,

    // not a true group : keep last, keep upper case
    GROUP_LAST,
};

#ifndef TRACE_TAG
#define TRACE_TAG ""
#endif

#define SE_FATAL(fmt, ...)              do { report_print(severity_fatal, KERN_EMERG "(%s,%d) FATAL: " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
                                            fatal_error(NULL); } while(0)
#define SE_ERROR(fmt, ...)              report_print(severity_error, KERN_NOTICE "(%s,%d) ERROR: " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__)
#define SE_WARNING(fmt, ...)            report_print(severity_warning, KERN_NOTICE "(%s,%d) WARNING: " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__)

#define SE_ERROR_ONCE(fmt, ...)              ({ static bool __section(.data.unlikely) __se_warned; if(!__se_warned) { SE_ERROR(fmt, ##__VA_ARGS__); __se_warned = true; } })
#define SE_WARNING_ONCE(fmt, ...)            ({ static bool __section(.data.unlikely) __se_warned; if(!__se_warned) { SE_WARNING(fmt, ##__VA_ARGS__); __se_warned = true; } })

#define SE_INFO(group, fmt, ...) \
    do {\
        if (report_is_group_active(severity_info, group)) \
            report_print_stg(severity_info, group, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)
#define SE_DEBUG(group, fmt, ...) \
    do {\
        if (report_is_group_active(severity_debug, group)) \
            report_print_stg(severity_debug, group, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)
#define SE_VERBOSE(group, fmt, ...) \
    do {\
        if (report_is_group_active(severity_verbose, group)) \
            report_print_stg(severity_verbose, group, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)
#define SE_EXTRAVERB(group, fmt, ...) \
    do {\
        if (report_is_group_active(severity_extraverb, group)) \
            report_print_stg(severity_extraverb, group, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)

#define SE_INFO2(group1, group2, fmt, ...) \
    do {\
        if (report_is_group_active(severity_info, group1)) \
            report_print_stg(severity_info, group1, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
        else if (report_is_group_active(severity_info, group2)) \
            report_print_stg(severity_info, group2, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)
#define SE_DEBUG2(group1, group2, fmt, ...) \
    do {\
        if (report_is_group_active(severity_debug, group1)) \
            report_print_stg(severity_debug, group1, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
        else if (report_is_group_active(severity_debug, group2)) \
            report_print_stg(severity_debug, group2, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)
#define SE_VERBOSE2(group1, group2, fmt, ...) \
    do {\
        if (report_is_group_active(severity_verbose, group1)) \
            report_print_stg(severity_verbose, group1, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
        else if (report_is_group_active(severity_verbose, group2)) \
            report_print_stg(severity_verbose, group2, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)
#define SE_EXTRAVERB2(group1, group2, fmt, ...) \
    do {\
        if (report_is_group_active(severity_extraverb, group1)) \
            report_print_stg(severity_extraverb, group1, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
        else if (report_is_group_active(severity_extraverb, group2)) \
            report_print_stg(severity_extraverb, group2, KERN_INFO "(%s,%d) " TRACE_TAG "::%s - " fmt, OS_ThreadName(), OS_ThreadID(), __func__, ##__VA_ARGS__); \
    } while(0)

#define SE_IS_DEBUG_ON(group)           report_is_group_active(severity_debug, group)
#define SE_IS_VERBOSE_ON(group)         report_is_group_active(severity_verbose, group)
#define SE_IS_EXTRAVERB_ON(group)       report_is_group_active(severity_extraverb, group)

#define SE_ASSERT(x)                    do if(!(x)) SE_FATAL("Assertion '%s' failed at %s:%d\n", #x, __FILE__, __LINE__); while(0)
#define SE_PRECONDITION(x)              do if(!(x)) SE_FATAL("Precondition '%s' failed at %s:%d\n", #x, __FILE__, __LINE__); while(0)
#define SE_POSTCONDITION(x)             do if(!(x)) SE_FATAL("Postcondition '%s' failed at %s:%d\n", #x, __FILE__, __LINE__); while(0)

#ifdef REPORT

void report_init(int trace_groups_levels[GROUP_LAST], int trace_global_level);
void report_term(void);
void __attribute__((format(printf, 2, 3))) report_print(report_severity_t report_severity, const char *format, ...);
int  __attribute__((format(printf, 3, 4))) report_print_stg(report_severity_t report_severity, int trace_group, const char *format, ...);
int report_is_group_active(report_severity_t report_severity, int trace_group);
void report_dump_hex(report_severity_t level, unsigned char *data, int length, int width, void *start);

#else // !REPORT

static inline void report_init(int trace_groups_levels[GROUP_LAST], int trace_global_level) {}
static inline void report_term(void) {}
static inline void __attribute__((format(printf, 2, 3))) report_print(report_severity_t report_severity, const char *format, ...) {}
static inline int  __attribute__((format(printf, 3, 4))) report_print_stg(report_severity_t report_severity, int trace_group, const char *format, ...) { return 0; }
static inline int report_is_group_active(report_severity_t report_severity, int trace_group) { return 0; }
static inline void report_dump_hex(report_severity_t level, unsigned char *data, int length, int width, void *start) {}

#endif // REPORT

#ifdef __cplusplus
}
#endif
#endif
