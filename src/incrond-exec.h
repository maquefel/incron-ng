// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_EXEC_H__
#define __INCROND_EXEC_H__

#include <sys/types.h>

int exec_start(const char* /*exec_file*/, char *const /*argv*/[], char *const /*envp*/[]);
int exec_stop(pid_t /*pid*/);
int exec_kill(pid_t /*pid*/);

#endif
