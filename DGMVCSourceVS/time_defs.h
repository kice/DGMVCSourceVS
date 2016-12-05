/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __TIME_DEFS_H__
#define __TIME_DEFS_H__

#include "mfxdefs.h"

#if defined(_WIN32) || defined(_WIN64)

#include <windows.h>

#define MSDK_SLEEP(msec) Sleep(msec)

#else // #if defined(_WIN32) || defined(_WIN64)

#include <unistd.h>

#define MSDK_SLEEP(msec) usleep(1000*msec)

#endif // #if defined(_WIN32) || defined(_WIN64)

#define MSDK_GET_TIME(T,S,F) ((mfxF64)((T)-(S))/(mfxF64)(F))

typedef mfxI64 msdk_tick;

msdk_tick msdk_time_get_tick(void);
msdk_tick msdk_time_get_frequency(void);

#endif // #ifndef __TIME_DEFS_H__
