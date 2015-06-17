#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <report.h>

using ::testing::_;

/*
 * Make sure the report system is initialized.
 */
class ReportEnvironment : public ::testing::Environment {
  public:
    virtual ~ReportEnvironment() {}

    virtual void SetUp() {
        int trace_groups[GROUP_LAST];
        memset(&trace_groups[0], -1, sizeof(trace_groups));

        char *trace_groups_env = getenv("SE_TRACE_GROUPS");
        if (trace_groups_env) {
            int i = 0;
            char *buf = trace_groups_env; /* since strsep modifies its pointer arg */
            for (char *token = strsep(&buf, ","); token != NULL; token = strsep(&buf, ",")) {
                int val = strtol(token, NULL, 0);

                if (val > severity_extraverb) {
                    val = severity_extraverb;
                }

                if (i < GROUP_LAST) {
                    if (-1 <= val) {
                        trace_groups[i] = val;
                    }
                }
                i++;

                if (i >= GROUP_LAST) {
                    break;
                }
            }

        }

        int trace_level = 0;
        char *trace_level_env = getenv("SE_TRACE_LEVEL");
        if (trace_level_env) {
            trace_level = strtol(trace_level_env, NULL, 0);
        }

        printf("Enabling trace groups level %d\n", trace_level);
        report_init(trace_groups, trace_level);
    }

    virtual void TearDown() {
    }
};

::testing::Environment *const report_env = ::testing::AddGlobalTestEnvironment(new ReportEnvironment);
