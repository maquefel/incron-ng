// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#include "incrond-parse-tabs.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <pwd.h>
#include <unistd.h>

#include <sys/stat.h>

#include "incrond.h"
#include "incrond-config.h"
#include "cmdline.h"

struct incrond_hook_modifier incrond_hook_modifiers[] = {
    { str(IN_ACCESS), IN_ACCESS },
    { str(IN_MODIFY), IN_MODIFY },
    { str(IN_ATTRIB), IN_ATTRIB },
    { str(IN_CLOSE_WRITE), IN_CLOSE_WRITE },
    { str(IN_CLOSE_NOWRITE), IN_CLOSE_NOWRITE },
    { str(IN_OPEN), IN_OPEN },
    { str(IN_MOVED_FROM), IN_MOVED_FROM },
    { str(IN_MOVED_TO), IN_MOVED_TO },
    { str(IN_CREATE), IN_CREATE },
    { str(IN_DELETE), IN_DELETE },
    { str(IN_DELETE_SELF), IN_DELETE_SELF },
    { str(IN_MOVE_SELF), IN_MOVE_SELF },
    { "", INCROD_TAB_ENUM_MAX },
    { str(IN_UNMOUNT), IN_UNMOUNT },
    { str(IN_Q_OVERFLOW), IN_Q_OVERFLOW },
    { str(IN_IGNORED), IN_IGNORED },
    { str(IN_ONLYDIR), IN_ONLYDIR },
    { str(IN_DONT_FOLLOW), IN_DONT_FOLLOW },
    { str(IN_EXCL_UNLINK), IN_EXCL_UNLINK },
    { str(IN_ISDIR), IN_ISDIR },
    { str(IN_ONESHOT), IN_ONESHOT },
    { str(IN_CLOSE), IN_CLOSE },
    { str(IN_MOVE), IN_MOVE },
    { str(IN_ALL_EVENTS), IN_ALL_EVENTS },
    { str(IN_NO_LOOP), IN_NO_LOOP },
    { 0, 0},
};

//  $$ dollar sign ?
//  $@ watched path
//  $# event-related file name
//  $% event flags (textually)
//  $& event flags (numerically)

const char* incrond_special_args_array[] = {
    "$$", // dollar sign
    "$@", // watched filesystem path
    "$#", // event-related file name
    "$%", // event flags (textually)
    "$&", // event flags (numerically)
    0
};

enum INCROD_TAB_ENUM tab_parse_mod(const char* value, size_t length)
{
    int i = 0;

    do {
        if(strncmp(value, incrond_hook_modifiers[i].name, length) == 0)
            return (enum INCROD_TAB_ENUM)(i);
    } while(incrond_hook_modifiers[++i].name != 0);

    return INCROD_TAB_ENUM_MAX;
}

static inline enum INCRON_TAB_ARG_ENUM tab_parse_args(uint8_t value)
{
    enum INCRON_TAB_ARG_ENUM arg = TAB_ARG_MAX;

    switch(value)
    {
        case '$':
            arg = TAB_ARG_DOLLAR;
            break;
        case '@':
            arg = TAB_ARG_PATH;
            break;
        case '#':
            arg = TAB_ARG_EVENT_FILENAME;
            break;
        case '%':
            arg = TAB_ARG_EVENT_TEXT;
            break;
        case '&':
            arg = TAB_ARG_EVENT_NUM;
            break;
        default:
            break;
    }

    return arg;
}

struct incron_path* findPath(const char* buffer, size_t len)
{
    struct incron_path* s = 0;

    HASH_FIND_STR( incron_paths, buffer, s);

    if(s) return s;

    s = malloc(sizeof(struct incron_path));

    s->path = strndup(buffer, len);
    s->flags = 0;

    INIT_LIST_HEAD(&(s->list));
    INIT_LIST_HEAD(&(s->hook_list));

    HASH_ADD_KEYPTR( hh, incron_paths, s->path, len, s );

    return s;
}

int pathAddHook(struct incron_path* path, struct incron_hook* hook)
{
    // list_add_tail(&(arg->list), &(hook->arg_list));
    list_add_tail(&(hook->list), &(path->hook_list));

    path->flags |= hook->flags;

    return 0;
}

int hookAddArg(struct incron_hook* hook, struct incron_hook_arg* arg)
{
    hook->arg_list_size++;

    list_add_tail(&(arg->list), &(hook->arg_list));

    return 0;
}

struct incron_hook* loadTabLine(int line_num, char* line, size_t len)
{
    char* tmp = 0;
    char* tmp1 = 0;
    char* tmp2 = 0;
    int arg_len = 0;
    uint32_t flags = 0;
    uint32_t iflags = 0;
    int errsv = 0;
    int ret = 0;

    line_num++;

    // ignore comment lines
    if(*line == '#')
        return 0;

    char **argv = NULL;
    int argc = 0;

    ret = buildargv(line, &argv, &argc);
    if(ret == -1)
    {
        syslog(LOG_ERR, "failed parsing tab at line: %d", line_num);
        errsv = EINVAL;
        goto fail;
    }

    // argv[0]  - watch path
    // argv[1]  - flags
    // argv[2]+ - path to executable and args (no parsing enviroment currently)

    /** add/find path from argv[0] */
    struct incron_path* path = findPath(argv[0], strlen(argv[0]));

    /** get modifiers from argv[1] */
    char* coma = 0;
    arg_len = strlen(argv[1]);

    tmp1 = argv[1];
    char* pe = tmp2 = argv[1] + strlen(argv[1]);
    do {
        coma = memchr((void*)tmp1, 0x2c, arg_len);
        if(coma != 0)
            tmp2 = coma;

        arg_len = tmp2 - tmp1;

        enum INCROD_TAB_ENUM mod = tab_parse_mod(tmp1, arg_len);

        if(mod == INCROD_TAB_ENUM_MAX)
        {
            syslog(LOG_ERR, "line %d : no known event %s", line_num, tmp1);
            goto next_arg;
        }

        uint32_t flag = incrond_hook_modifiers[mod].value;

        if(mod < INOTIFY_ENUM_MAX)
        {
            flags |= flag;
        } else {
            iflags |= flag;
        }

        fprintf(stderr, "modifier : %.*s\n", arg_len, tmp1);

        next_arg:
        tmp1 = tmp2 + 1;
        arg_len = pe - tmp1;
        tmp2 = pe;
    } while(coma != 0);

    /** */
    struct incron_hook *hook = malloc(sizeof(struct incron_hook));

    hook->flags = flags | IN_IGNORED; // i am really not sure if IN_IGNORED should be added explicitly follow old incrond case
    hook->iflags = iflags;
    hook->fired = 0;

    hook->arg_list_size = 0;
    INIT_LIST_HEAD(&(hook->arg_list));

    pathAddHook(path, hook);

    /** get all args from argv[2]+ */
    hook->argv = (char**)malloc((argc - 2 + 1/*NULL*/)*sizeof(char*));

    int i = 2, j = -1;
    while(argv[i] != NULL) {
        j++;
        arg_len = strlen(argv[i]) + 1;
        debug_printf_n("%s : %d", argv[i], arg_len);
        hook->argv[j] = strndup(argv[i], arg_len);
        char* pe = hook->argv[j] + arg_len;

        tmp = memchr((void*)hook->argv[j], '$', arg_len);

        while(tmp != 0 && *(tmp + 1) != '\0')
        {
            enum INCRON_TAB_ARG_ENUM arg = TAB_ARG_MAX;

            arg = tab_parse_args(*(tmp + 1));

            if(arg != TAB_ARG_MAX)
            {
                struct incron_hook_arg* i_arg = malloc(sizeof(struct incron_hook_arg));

                i_arg->index = j;
                i_arg->arg = arg;
                i_arg->pos = tmp - hook->argv[j];

                list_add_tail(&(i_arg->list), &(hook->arg_list));

                debug_printf_n("found %.2s in %s at %u", tmp, hook->argv[j], i_arg->pos);
            }


            arg_len = pe - tmp;
            tmp = memchr((void*)(tmp + 1), '$', arg_len - 1);
        }

        i++;
    }

    hook->argv[j + 1] = 0;
    hook->argc = j + 1;
    hook->command = "/bin/bash";

    hook->pw_uid = -1;
    hook->pw_gid = -1;

    /* free argv */
    for(int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);

    return hook;

    fail:
    errno = errsv;
    return 0;
}

int loadTab(int dirfd, const char* fileName, uid_t uid, gid_t gid)
{
    int errsv = 0;

    int fd = openat(dirfd, fileName, O_RDONLY);
    errsv = errno;
    if(fd == -1) {
        syslog(LOG_ERR, "Couldn't open file: %s for reading", fileName);
        goto fail;
    }

    FILE* file = fdopen(fd, "r");

    char* line = NULL;
    size_t len = 0;
    int8_t line_num = -1;
    ssize_t nread;

    while ((nread = getline(&line, &len, file)) != -1) {
        line[nread - 1] = '\0';
        debug_printf_n("parsing line [%ld] : %s", nread, line);
        struct incron_hook* hook = loadTabLine(++line_num, line, nread);

        if(!hook) {
            syslog(LOG_ERR, "Failed loading line at %d in %s", line_num, fileName);
            continue;
        }

        hook->pw_uid = uid;
        hook->pw_gid = gid;
    }
    free(line);

    return 0;

    fail:
    errno = errsv;
    return -1;
}

int loadSystemTabs(int dirfd)
{
    int errsv = 0;
    struct dirent* dentry = 0;

    DIR* dir = fdopendir(dirfd);
    errsv = errno;

    if (dir == 0)
        goto fail;

    while (1) {
        errno = 0;
        dentry = readdir(dir);

        if(dentry == 0)
        {
            errsv = errno;
            if(errsv == 0) break;
            continue;
        }

        loadTab(dirfd, dentry->d_name, getuid(), getgid());
    }

    closedir(dir);

    return 0;

    fail:
    errno = errsv;
    return -1;
}

int loadUserTabs(int dirfd)
{
    int errsv = 0;
    struct dirent* dentry = 0;

    DIR* dir = fdopendir(dirfd);
    errsv = errno;

    if (dir == 0)
        goto fail;

    while (1) {
        errno = 0;
        dentry = readdir(dir);
        errsv = errno;

        if(dentry == 0)
        {
            if(errsv == 0) break;
            continue;
        }

        /* check if user is allowed to use incrond  */
        bool allowed = userAllowed(dentry->d_name);

        if(!allowed) {
            syslog(LOG_INFO, "not loading %s user doesn't exists or isn't allowed", dentry->d_name);
            continue;
        }

        struct passwd *pwd = getpwnam(dentry->d_name);
        if(pwd == 0) {
            syslog(LOG_INFO, "not loading %s user %s doesn't exists", dentry->d_name, dentry->d_name);
            continue;
        }

        loadTab(dirfd, dentry->d_name, pwd->pw_uid, pwd->pw_gid);
    }

    closedir(dir);

    return 0;

    fail:
    errno = errsv;
    return -1;
}

static void freeHook(struct incron_hook* hook)
{
    struct list_head *tmp = 0;
    struct list_head *ag = 0;

    list_for_each_safe(ag, tmp, &(hook->arg_list)) {
        struct incron_hook_arg* arg = list_entry(ag, struct incron_hook_arg, list);
        list_del(&(arg->list));
        free(arg);
    }

    /* free argv */
    for(int i = 0; i < hook->argc; i++)
        free(hook->argv[i]);

    free(hook->argv);
    free(hook);
}

static void freePath(struct incron_path* path)
{
    struct list_head *tmp = 0;
    struct list_head *hk = 0;

    list_for_each_safe(hk, tmp, &(path->hook_list)) {
        struct incron_hook* hook = list_entry(hk, struct incron_hook, list);
        list_del(&(hook->list));
        freeHook(hook);
    }

    free(path->path);
    free(path);
}

void freeTabs()
{
    struct incron_path *s, *tmp;
    HASH_ITER(hh, incron_paths, s, tmp)
    {
        HASH_DEL(incron_paths, s);
        freePath(s);
    }
}
