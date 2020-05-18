// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_DISPATCH_H__
#define __INCROND_DISPATCH_H__

#include <unistd.h>
#include <stdint.h>
#include <time.h>

char* print_text_events(uint32_t events);

struct incron_path;
struct inotify_event;

int dispatch_hooks(struct incron_path* /*path*/, const struct inotify_event* /*event*/);
int hook_clear_spawned(pid_t /*pid*/);

int handle_events(int inotifyfd);

#endif
