// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_USER_H__
#define __INCROND_USER_H__

#include <sys/types.h>
#include <pwd.h>

// struct passwd {
//     char   *pw_name;       /* username */
//     char   *pw_passwd;     /* user password */
//     uid_t   pw_uid;        /* user ID */
//     gid_t   pw_gid;        /* group ID */
//     char   *pw_gecos;      /* user information */
//     char   *pw_dir;        /* home directory */
//     char   *pw_shell;      /* shell program */
// };

struct incron_user {
    uid_t   pw_uid;        /* user ID */
    gid_t   pw_gid;        /* group ID */
};

#endif

