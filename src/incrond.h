// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_H__
#define __INCROND_H__

#ifndef MAJOR
#define MAJOR 0
#endif

#ifndef MINOR
#define MINOR 0
#endif

#ifndef VERSION_PATCH
#define VERSION_PATCH 0
#endif

#define PACKAGE_VERSION MAJOR "." MINOR "." VERSION_PATCH

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "incrond"
#endif

#ifndef INCROND_PID_FILE
#define INCROND_PID_FILE "/var/run/" PACKAGE_NAME ".pid"
#endif

#ifndef INCROND_CONFIG_FILE
#define INCROND_CONFIG_FILE "/etc/" PACKAGE_NAME "/" PACKAGE_NAME ".conf"
#endif

#define INCROND_ERROR_CLOSING_FD          1
#define INCROND_ERROR_OPENING_PID         2
#define INCROND_ERROR_REGISTER_SIGNAL     3
#define INCROND_ERROR_OPEN_CONFIG         4
#define INCROND_ERROR_LOADING_CONFIG      5
#define INCROND_ERROR_CONTROL_FIFO        6

#ifdef DEBUG
#include <stdio.h>
#define debug_printf(format, ...) printf("%s:%s:%d: " format, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define debug_printf_n(format, ...) debug_printf(format "\n", ##__VA_ARGS__)
#else
#define debug_printf_n(format, args...) do{}while(0)
#endif

#include <stddef.h>

#define container_of(ptr, type, member) ({ \
                const typeof( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member) );})


#include <string.h>

static inline int check_prefix(const char *str, const char *prefix)
{
        return strlen(str) > strlen(prefix) &&
                strncmp(str, prefix, strlen(prefix)) == 0;
}

extern int devnullfd;

#endif
