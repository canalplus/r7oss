/*
 * Copyright (C) 2010 Wyplay, All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of Wyplay and is confidential in nature.
 * Under no circumstances is this software to be exposed to or placed
 * under an Open Source License of any type without the expressed written
 * permission of Wyplay.
*/
#ifndef DEBUG_H_
#define DEBUG_H_

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#undef NDEBUG
#define WYTRACE_ERROR(fmt, args...) printf("%s:%s():%d : ERROR : " fmt, __FILE__, __FUNCTION__, __LINE__, ##args);

#ifndef NDEBUG
    #define WY_TRACK(__classname) printf("%s:%d - %s::%s()\n", __FILE__, __LINE__, #__classname, __FUNCTION__);
    #define WYTRACE_DEBUG(fmt, args...) printf("%s:%s():%d : DEBUG : " fmt, __FILE__, __FUNCTION__, __LINE__, ##args);
#else
    #define WY_TRACK(__classname) ;
    #define WYTRACE_DEBUG(_domain, fmt, args...)
#endif

#define WY_NOT_IMPLEMENTED()    WYTRACE_ERROR("NOT IMPLEMENTED\n");

#define DEFINE_DYNAMIC_TRACE(__ENV_VARIABLE_NAME) \
        bool    g_b##__ENV_VARIABLE_NAME = (getenv(#__ENV_VARIABLE_NAME) == NULL) ? false : true;
#define DYNAMIC_TRACE_IS_ENABLED(__ENV_VARIABLE_NAME) (g_b##__ENV_VARIABLE_NAME)

#if 0
// Use wylog
#define DYNAMIC_TRACE(__ENV_VARIABLE_NAME, fmt, args...) \
        if (g_b##__ENV_VARIABLE_NAME)  wl_debug(fmt, ##args);
#else
#define DYNAMIC_TRACE(__ENV_VARIABLE_NAME, fmt, args...) \
        if (g_b##__ENV_VARIABLE_NAME)  printf(fmt "\n", ##args);
#endif

#endif /* DEBUG_H_ */
