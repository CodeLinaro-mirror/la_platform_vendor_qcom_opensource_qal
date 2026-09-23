/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PAL_COMMON_H
#define PAL_COMMON_H

#include "ar_osal_mem_op.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <log/log.h>

#ifdef USE_CLOG
#include <clog.h>
#include <clog_ids.h>
#undef ON
#undef OFF
#endif

#ifdef USE_DLT
#include <stdio.h>
#include <dlt/dlt.h>
extern DltContext palContext;
#endif

#define PAL_LOG_ERR             (0x1) /**< error message, represents code bugs that should be debugged and fixed.*/
#define PAL_LOG_INFO            (0x2) /**< info message, additional info to support debug */
#define PAL_LOG_DBG             (0x4) /**< debug message, required at minimum for debug.*/
#define PAL_LOG_VERBOSE         (0x8)/**< verbose message, useful primarily to help developers debug low-level code */

extern uint32_t pal_log_lvl;

#ifdef USE_CLOG

#define PAL_CLOG_CONTEXT_ID CLOG_LCTX_PALC
#define PAL_CLOG_APP_NAME "PALX"
#define PAL_CLOG_APP_DESC "PAL Audio Logging"
#define PAL_CLOG_CONTEXT_NAME "PALC"
#define PAL_CLOG_CONTEXT_DESC "PAL Context"

static inline clog_context_t pal_get_clog_context(void)
{
    static clog_context_t pal_clog_ctx = NULL;
    static int pal_clog_init_done = 0;

    if (!pal_clog_init_done) {
        if (CLOG_IS_INITIALIZED() != CLOG_RETURN_TRUE) {
            if (CLOG_REGISTER_APP((clog_app_id_t)0,
                                  PAL_CLOG_APP_NAME,
                                  PAL_CLOG_APP_DESC) != CLOG_RETURN_OK) {
                CLOG_REGISTER_LIBRARY();
            }
        }

        if (CLOG_IS_INITIALIZED() == CLOG_RETURN_TRUE) {
            pal_clog_ctx = CLOG_REGISTER_CONTEXT(PAL_CLOG_CONTEXT_ID,
                                                 PAL_CLOG_CONTEXT_NAME,
                                                 PAL_CLOG_CONTEXT_DESC,
                                                 CLOG_DIAG_SSID_NOT_AVAILABLE,
                                                 PAL_CLOG_APP_NAME,
                                                 PAL_CLOG_APP_NAME);
        }
        pal_clog_init_done = 1;
    }

    return pal_clog_ctx;
}

#define PAL_CLOG_WRAPPER(clog_fn, alog_fn, fmt, ...) do { \
    clog_context_t _ctx = pal_get_clog_context(); \
    if (_ctx) \
        clog_fn(_ctx, "%s: %d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
    else \
        alog_fn("%s: %d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

#define PAL_ERR(tag, fmt, ...) PAL_CLOG_WRAPPER(CLOGE, ALOGE, fmt, ##__VA_ARGS__)
#define PAL_DBG(tag, fmt, ...) PAL_CLOG_WRAPPER(CLOGD, ALOGD, fmt, ##__VA_ARGS__)
#define PAL_INFO(tag, fmt, ...) PAL_CLOG_WRAPPER(CLOGI, ALOGI, fmt, ##__VA_ARGS__)
#define PAL_VERBOSE(tag, fmt, ...) PAL_CLOG_WRAPPER(CLOGV, ALOGV, fmt, ##__VA_ARGS__)

#elif defined(USE_DLT)
/*
 * Helper Macro:
 * 1. Creates a temporary buffer.
 * 2. Formats the variables (%d, %s) into the string using snprintf.
 * 3. Sends the final formatted string to DLT.
 */
#define PAL_DLT_WRAPPER(level, fmt, ...) do { \
    char _buf[256]; \
    snprintf(_buf, sizeof(_buf), "%s:%d: " fmt,  __func__, __LINE__, ##__VA_ARGS__); \
    DLT_LOG(palContext, level, DLT_STRING(_buf)); \
} while(0)

// Map your macros to the helper
#define PAL_ERR(tag, fmt, ...) PAL_DLT_WRAPPER(DLT_LOG_ERROR, fmt, ##__VA_ARGS__)
#define PAL_DBG(tag, fmt, ...) PAL_DLT_WRAPPER(DLT_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define PAL_INFO(tag, fmt, ...) PAL_DLT_WRAPPER(DLT_LOG_INFO,  fmt, ##__VA_ARGS__)
#define PAL_VERBOSE(tag, fmt, ...) PAL_DLT_WRAPPER(DLT_LOG_VERBOSE, fmt, ##__VA_ARGS__)
#else
#define PAL_ERR(log_tag, arg,...)                                          \
    if (pal_log_lvl & PAL_LOG_ERR) {                              \
        ALOGE("%s: %d: "  arg, __func__, __LINE__, ##__VA_ARGS__);\
    }
#define PAL_DBG(log_tag,arg,...)                                           \
    if (pal_log_lvl & PAL_LOG_DBG) {                               \
        ALOGD("%s: %d: "  arg, __func__, __LINE__, ##__VA_ARGS__); \
    }
#define PAL_INFO(log_tag,arg,...)                                         \
    if (pal_log_lvl & PAL_LOG_INFO) {                             \
        ALOGI("%s: %d: "  arg, __func__, __LINE__, ##__VA_ARGS__);\
    }
#define PAL_VERBOSE(log_tag,arg,...)                                      \
    if (pal_log_lvl & PAL_LOG_VERBOSE) {                          \
        ALOGV("%s: %d: "  arg, __func__, __LINE__, ##__VA_ARGS__);\
    }
#endif

#endif /* PAL_COMMON_H */
