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

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include "player_version.h"

#include "report.h"
#include "encoder_test.h"
#include "common.h"

#ifndef __init
#define __init
#endif
#ifndef __exit
#define __exit
#endif

static int  __init              EncoderTestLoadModule(void);
static void __exit              EncoderTestUnloadModule(void);

module_init(EncoderTestLoadModule);
module_exit(EncoderTestUnloadModule);

MODULE_DESCRIPTION("Encode STKPI Test Module");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

extern EncoderTest test_first;
extern EncoderTest test_last;

static EncoderTest *first = (EncoderTest *) &test_first;
static EncoderTest *last  = (EncoderTest *) &test_last;

// A module argument to specify only one test to run
static int run = -1;
static int type = BASIC_TEST;
static int duration = 0;
static int free_memory_target = 0;
// No sysfs parameters as this module will never load successfully!
module_param(run, int, S_IRUGO);
MODULE_PARM_DESC(run, "An index into the test array indicating a single test to run");
module_param(type, int, S_IRUGO);
MODULE_PARM_DESC(type, "Test type to run");
module_param(duration, int, S_IRUGO);
MODULE_PARM_DESC(duration, "Maximum duration in seconds for long test");
module_param(free_memory_target, int, S_IRUGO);
MODULE_PARM_DESC(free_memory_target, "Free memory target for test robustness on memory allocation error");

static void ReportResult(EncoderTest *test)
{
    pr_info("Test %03d (%s): '%-40.40s' %s : Return: %d\n", test - first, Stringify(test->type), test->description,
            PassFail(test->status), test->status);
}

static void FillEncoderTestArg(EncoderTestArg *arg)
{
    arg->duration = duration;
    arg->free_memory_target = free_memory_target;
}

static void RunTest(EncoderTest *t)
{
    EncoderTestArg arg;
    pr_info("Test %d (%s), at %p\n", t - first, Stringify(t->type), t);

    if (t->function && t->description)
    {
        pr_info("Executing '%s'\n", t->description);
        FillEncoderTestArg(&arg);
        t->status = t->function(&arg);
        ReportResult(t);
    }
}

static void RunAllTests(void)
{
    EncoderTest *t;
    pr_info("\nRunning all tests (0x%x)\n", type);
    pr_info("First test at %p\n", &test_first);
    pr_info("Last test at %p\n", &test_last);

    for (t = first; t < last; t++)
    {
        if (t->type & type)
        {
            RunTest(t);
        }
    }
}

static void ShowAllResults(void)
{
    EncoderTest *t;
    pr_info("\nSummarising all test runs (0x%x)\n", type);

    for (t = first; t < last; t++)
    {
        if (t->type & type)
        {
            if (t->function && t->description)
            {
                ReportResult(t);
            }
        }
    }
}

static void ListTests(void)
{
    EncoderTest *t;
    pr_info("\nThe following tests are available:\n");

    for (t = first; t < last; t++)
    {
        if (t->function && t->description)
        {
            pr_info("Test %03d (%s): %-40.40s\n", t - first, Stringify(t->type), t->description);
        }
    }
}

static void RunOneTest(int index)
{
    EncoderTest *t = &first[index];

    if (t < first || t > last)
    {
        pr_err("\nInvalid test index specified (%d/%d)\n", index, last - first);
        ListTests();
        return;
    }

    RunTest(t);
}

static int __init EncoderTestLoadModule(void)
{
    if (run >= 0)
    {
        RunOneTest(run);
    }
    else
    {
        RunAllTests();
        ShowAllResults();
    }

    // Take advantage of returning an error so that we do not have to 'unload' the test module
    return -1;
}

static void __exit EncoderTestUnloadModule(void)
{
    pr_info("%s: Encoder Test Module unloaded\n", __func__);
}
