/**
 * @file ide_shim.h
 * @brief Force-included shim for CLion IntelliSense on macOS.
 *
 * Declares POSIX Advanced Real-Time Clock symbols that Darwin's libc omits
 * (clock_nanosleep, TIMER_ABSTIME). Real cross builds target Linux/glibc
 * where these live in <time.h>, so this file is only pulled in by the
 * native-debug-ide preset via -include and is gated on __APPLE__.
 *
 * Do NOT include this header from source files. It is injected by the
 * build system for indexer correctness only.
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __APPLE__

#include <time.h>

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

int clock_nanosleep(clockid_t clock_id, int flags,
                    const struct timespec *request,
                    struct timespec *remain);

#ifdef __cplusplus
}
#endif

#endif /* __APPLE__ */
