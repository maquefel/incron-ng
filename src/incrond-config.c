// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#include "incrond-config.h"

#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>

#include <errno.h>

#include "list.h"

#define ret_fail_do_syslog(ret, line_num, errsv, level, what) do { \
    errsv = errno; \
    if(ret == -1) { \
        syslog(level, "%s failed at line %d with [%d]:%s", what, line_num, errsv, strerror(errsv));\
    }\
} while(0);

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/** */
int system_table_dir_fd;
int set_system_table_dir(const char* value, bool clean)
{
    UNUSED(clean);
    system_table_dir_fd = open(value, O_DIRECTORY);
    if(system_table_dir_fd != -1) return 0;
    return -1;
}

/** */
int user_table_dir_fd;
char *user_table_dir;
int set_user_table_dir(const char* value, bool clean)
{
    if(clean) free(user_table_dir);
    user_table_dir = strndup(value, PATH_MAX);
    return 0;
}

char *allowed_users_file;
int set_allowed_users(const char* value, bool clean)
{
    if(clean) free(allowed_users_file);
    allowed_users_file = strndup(value, PATH_MAX);
    return 0;
}

char *denied_users_file;
int set_denied_users(const char* value, bool clean)
{
    if(clean) free(denied_users_file);
    denied_users_file = strndup(value, PATH_MAX);
    return 0;
}

/** */
int lockfile_dir_fd;
int set_lockfile_dir(const char* value, bool clean)
{
    UNUSED(clean);
    lockfile_dir_fd = open(value, O_DIRECTORY);
    if(lockfile_dir_fd != -1) return 0;
    return -1;
}

char lockfile_name[NAME_MAX];
int set_lockfile_name(const char* value, bool clean)
{
    UNUSED(clean);
    snprintf(lockfile_name, sizeof(lockfile_name), "%s.pid", value);
    return 0;
}

char editor_name[NAME_MAX];
int set_editor(const char* value, bool clean)
{
    UNUSED(clean);
    strncpy(editor_name, value, NAME_MAX);
    return 0;
}

struct incron_config_opt opts[] = {
    {"system_table_dir", "/etc/incron.d", set_system_table_dir, LOG_WARNING},
    {"user_table_dir", "/var/spool/incron", set_user_table_dir, LOG_WARNING},
    {"allowed_users", "/etc/incron.allow", set_allowed_users, LOG_WARNING},
    {"denied_users", "/etc/incron.deny", set_denied_users, LOG_WARNING},
    {"lockfile_dir", "/var/run", set_lockfile_dir, LOG_CRIT},
    {"lockfile_name", "incrond", set_lockfile_name, LOG_CRIT},
    {"editor", "", set_editor, LOG_INFO},
    {0, 0, 0}
};

static void load_defaults()
{
    /** we are silently ignoring all errors */
    int i = 0;
    while(opts[i].name != 0) {
        opts[i].set_value(opts[i].default_value, false);
        i++;
    }
}

static char* eat_space(char* str)
{
    char* tmp = str;

    while(*tmp != '\0') {
        if(*tmp == 0x20 || *tmp == 0x09) {
            tmp++;
            continue;
        }

        break;
    }

    return tmp;
}

// 0x20 100000
// 0x09 001001
// 0x0a 001010
// 0x0d 001101

static char* next_space(char* str)
{
    char* tmp = str;

    while(*tmp != '\0') {
        if(*tmp == 0x20 || *tmp == 0x09 || *tmp == 0x0a || *tmp == 0x0d)
            return tmp;

        tmp++;
    }

    return 0;
}

static char* next_space_or(char* str, int8_t c)
{
    char* tmp = str;

    while(*tmp != '\0') {
        if(*tmp == 0x20 || *tmp == 0x09 || *tmp == 0x0a || *tmp == 0x0d || *tmp == c)
            return tmp;

        tmp++;
    }

    return 0;
}

static int scan_for_option(int line_num, const char* name, const char* value)
{
    int i = 0;
    int ret = -1;
    int errsv;

    while(opts[i].name != 0) {
        if(strncmp(name, opts[i].name, strlen(name)) == 0)
        {
            ret = opts[i].set_value(value, true);
            ret_fail_do_syslog(ret, line_num, errsv, opts[i].severity, opts[i].name);
            return ret;
        }

        i++;
    }

    return -1;
}

int loadConfigLine(int line_num, char* line)
{
    int ret = 0;
    char* tmp1 = eat_space(line);
    char* tmp2 = next_space_or(tmp1, '='); /** space or = */

    if(tmp2 == 0)
        goto fail;

    size_t len = tmp2 - tmp1;

    char* name = strndup(tmp1, len);

    tmp1 = eat_space(tmp2);
    tmp2 = next_space(tmp1);

    if(tmp2 == 0)
        goto fail_free_name;

    /** check if = */
    if(*tmp1 != '=')
        goto fail_free_name;

    tmp1 = eat_space(tmp2);
    tmp2 = next_space(tmp1);

    if(tmp2 == 0)
        goto fail_free_name;

    len = tmp2 - tmp1;

    if(len == 0)
        goto fail_free_name;

    char* value = strndup(tmp1, len);

    ret = scan_for_option(line_num, name, value);

    free(value);

    fail_free_name:
    free(name);

    fail:
    return ret;
}

int loadConfigFile(const char* configFile)
{
    int errsv = 0;
    int ret = 0;

    /** initialize defaults */
    load_defaults();

    int fd = open(configFile, O_RDONLY);

    if(fd == -1) {
        errsv = errno;
        syslog(LOG_ERR, "couldn't open config file: %s for reading", configFile);
        goto fail;
    }

    FILE* file = fdopen(fd, "r");

    char* line = NULL;
    size_t len = 0;
    int8_t line_num = -1;
    ssize_t nread;

    do {
        nread = getline(&line, &len, file);
        errsv = errno;

        if(nread == -1)
            break;

        ret = loadConfigLine(++line_num, line);
        if(ret == -1) {
            errsv = errno;
            syslog(LOG_WARNING, "failed parsing %s at %d line", configFile, line_num);
        }
    } while(nread > 0);

    free(line);

    return 0;
    fail:
    errno = errsv;
    return -1;
}

char* loadUserLine(char* line)
{
    int errsv = EINVAL;
    int login_name_max = sysconf(_SC_LOGIN_NAME_MAX);

    char* tmp1 = eat_space(line);
    char* tmp2 = next_space_or(tmp1, '\n');
    if(tmp2 == 0)
        tmp2 = line + strnlen(line, login_name_max + 1);

    if((tmp2 - tmp1) >= login_name_max)
    {
        goto fail;
    }

    int user_len = tmp2 - tmp1 + 1;
    char* user = strndup(tmp1, user_len);
    user[user_len - 1] = '\0';

    return user;

    fail:

    errno = errsv;
    return 0;
}

static inline void add_user(char* username)
{
    /** check if already exists */
    struct list_head *pos = 0;
    struct user *u = 0;

    list_for_each(pos, &(users))
    {
        u = list_entry(pos, struct user, list);
        if(strncmp(username, u->name, strlen(username)) == 0)
            return;
    }

    u = malloc(sizeof(struct user));
    u->name = username;

    list_add_tail(&(u->list), &(users));
};

int loadUserFile(const char* userFile)
{
    int errsv = 0;

    int fd = open(userFile, O_RDONLY);
    if(fd == -1)
    {
        errsv = errno;

        if(errsv != EEXIST)
            syslog(LOG_ERR, "couldn't open user file: %s for reading with %d : %s", userFile, errsv, strerror(errsv));

        goto fail;
    }

    FILE* file = fdopen(fd, "r");

    char* line = NULL;
    size_t len = 0;
    ssize_t nread;

    do {
        nread = getline(&line, &len, file);
        errsv = errno;

        if(nread == -1)
            break;

        char* user = loadUserLine(line);

        if(user != 0)
            add_user(user);
    } while(nread > 0);

    free(line);

    return 0;

    fail:
    errno = errsv;
    return -1;
}

static enum incron_allow_policy allow_policy;

enum incron_allow_policy allowPolicy()
{
    return allow_policy;
}

void loadUsers()
{
    allow_policy = INCRON_EEXIST;

    INIT_LIST_HEAD(&(users));

    int ret = loadUserFile(allowed_users_file);
    if(ret == 0) {
        syslog(LOG_NOTICE, "loaded allowed users from %s file", allowed_users_file);
        allow_policy = INCRON_ALLOW_EXIST;
        return;
    }

    ret = loadUserFile(denied_users_file);
    if(ret == 0) {
        allow_policy = INCRON_DENY_EXIST;
        syslog(LOG_NOTICE, "loaded denied users from %s file", denied_users_file);
        return;
    }

    syslog(LOG_WARNING, "no allow or deny files loaded using allow all policy");
}

bool userAllowed(const char* username)
{
    bool allowed = false;
    int errsv = EEXIST;

    if(allow_policy == INCRON_EEXIST) return false;

    struct list_head *pos = 0;
    struct user *user = 0;

    list_for_each(pos, &(users))
    {
        user = list_entry(pos, struct user, list);
        if(strncmp(username, user->name, strlen(username)) == 0)
        {
            if(allow_policy == INCRON_ALLOW_EXIST)
                allowed = true;
            else
                errsv = EPERM;

            goto out;
        }
    }

    /** not found */
    if(allow_policy == INCRON_DENY_EXIST) allowed = true;

    out:
    errno = errsv;
    return allowed;
}

void freeUsers()
{
    struct list_head *tmp = 0;
    struct list_head *usr = 0;

    list_for_each_safe(usr, tmp, &(users)) {
        struct user *user = list_entry(usr, struct user, list);
        list_del(&(user->list));
        free(user->name);
        free(user);
    }
}
