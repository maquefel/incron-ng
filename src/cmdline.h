// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#define BUILDARGV_EOK     0
#define BUILDARGV_EDQUOTE 1
#define BUILDARGV_ESQUOTE 2

int buildargv(char* input, char*** argv, int *argc);
#endif
