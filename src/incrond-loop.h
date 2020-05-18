// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_LOOP_H__
#define __INCROND_LOOP_H__

#include <sys/epoll.h>

/// loop types used in single epoll loop
enum loop_type {
    INOTIFY_FD,         ///< event from inotify
    SIGNAL_FD,          ///< signals watch file descriptor
    LOOP_TYPE_MAX
};

struct epoll_wrapper {
    enum loop_type type; ///> type of wrapper
    struct epoll_event event; ///> event for passing to epoll_ctl
    union {
        int fd; ///> file descriptor if type is SIGNAL_FD | SYSFS_PIN | LISTEN_PORT
    };
};

int loop(int /*sigfd*/);

#endif
