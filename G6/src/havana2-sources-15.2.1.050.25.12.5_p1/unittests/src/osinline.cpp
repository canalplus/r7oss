#define OSINLINE_IN_IMPL

#include <stm_event.h>

#include "osinline.h"
#include "time_interface.h"

/* UNTESTED_START: test code */

TimeInterface *gTime;

void useTimeInterface(TimeInterface *timeInterface) {
    gTime = timeInterface;
}
/* UNTESTED_STOP */

OS_NameEntry_t          OS_TaskNameTable[OS_MAX_TASKS];

OSINLINE_EXTERN_IMPL_ unsigned long long OS_GetTimeInMicroSeconds(void) {
    struct timeval  Now;

    if (gTime != 0) { return gTime->GetTimeInMicroSeconds(); } /* UNTESTED: test code */

    gettimeofday(&Now, NULL);
    return ((Now.tv_sec * 1000000ull) + (Now.tv_usec));
}

OSINLINE_EXTERN_IMPL_ unsigned int OS_GetTimeInMilliSeconds(void) {
    struct timeval  Now;
// if (gTime != 0) printf("time=%lldms\n",gTime->GetTimeInMicroSeconds()/1000);
    if (gTime != 0) { return gTime->GetTimeInMicroSeconds() / 1000; } /* UNTESTED: test code */
    gettimeofday(&Now, NULL);
    return ((Now.tv_sec * 1000) + (Now.tv_usec / 1000));
}


int stm_event_signal(stm_event_t        *event) { return 0; }  /* UNTESTED: TODO: is it the right place for this? */
