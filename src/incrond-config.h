// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_CONFIG_H__
#define __INCROND_CONFIG_H__

#include <linux/limits.h>
#include <stdbool.h>

#include "list.h"

extern int system_table_dir_fd;
extern int user_table_dir_fd;
extern char *user_table_dir;
extern char *allowed_users_file;
extern char *denied_users_file;
extern int lockfile_dir_fd;
extern char lockfile_name[NAME_MAX];
extern char editor_name[NAME_MAX];

typedef int (*set_value_func)(const char*, bool);

struct incron_config_opt
{
    char* name;
    char* default_value;
    set_value_func set_value;
    int severity;
};

struct list_head users;

/**
 * @brief Allowed or denied users - depends on which file is used
 *
 */
struct user
{
    char* name;
    struct list_head list;
};

int loadConfigLine(int line_num, char* line);
int loadConfigFile(const char* configFile);

char* loadUserLine(char* line);
int loadUserFile(const char* userFile);
void loadUsers();
void freeUsers();

enum incron_allow_policy
{
    INCRON_ALLOW_EXIST,
    INCRON_DENY_EXIST,
    INCRON_EEXIST,
};

enum incron_allow_policy allowPolicy();

// If incrond.allow exists, only the users who are listed in this file can create, edit, display, or remove crontab files.
// If incrond.allow does not exist, all users can submit crontab files, except for users who are listed in incrond.deny.
// If neither incrond.allow nor incrond.deny exists, superuser privileges are required to run the incrond command.
bool userAllowed(const char* /*username*/);

#endif
