// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INCROND_PARSE_TABS_H__
#define __INCROND_PARSE_TABS_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include <linux/limits.h>

#include "list.h"
#include "uthash.h"

// incrond special modifiers
#define IN_NO_LOOP (1U << 0)

enum INCROD_TAB_ENUM {
    E_IN_ACCESS,
    E_IN_MODIFY,
    E_IN_ATTRIB,
    E_IN_CLOSE_WRITE,
    E_IN_CLOSE_NOWRITE,
    E_IN_OPEN,
    E_IN_MOVED_FROM,
    E_IN_MOVED_TO,
    E_IN_CREATE,
    E_IN_DELETE,
    E_IN_DELETE_SELF,
    E_IN_MOVE_SELF,
    E_IN_UNMOUNT = 13,
    E_IN_Q_OVERFLOW,
    E_IN_IGNORED,
    E_IN_ONLYDIR,
    E_IN_DONT_FOLLOW,
    E_IN_EXCL_UNLINK,
    E_IN_ISDIR,
    E_IN_ONESHOT,
    E_IN_CLOSE,
    E_IN_MOVE,
    E_IN_ALL_EVENTS,
    E_IN_NO_LOOP,
    INOTIFY_ENUM_MAX = E_IN_NO_LOOP,
    INCROD_TAB_ENUM_MAX
};

#define xstr(s) str(s)
#define str(s) #s

#define STRLEN(s) (sizeof(s))

struct incrond_hook_modifier
{
    const char* name;
    uint32_t value;
};

extern struct incrond_hook_modifier incrond_hook_modifiers[];

#define MAX_TEXT_ARGS_STRLEN STRLEN(str(IN_ACCESS)) + STRLEN(str(IN_MODIFY)) + STRLEN(str(IN_ATTRIB)) + STRLEN(str(IN_CLOSE_WRITE)) + STRLEN(str(IN_CLOSE_NOWRITE)) + STRLEN(str(IN_CLOSE)) + STRLEN(str(IN_OPEN)) + STRLEN(str(IN_MOVED_FROM)) + STRLEN(str(IN_MOVED_TO)) + STRLEN(str(IN_MOVE)) + STRLEN(str(IN_CREATE)) + STRLEN(str(IN_DELETE)) + STRLEN(str(IN_DELETE_SELF)) + STRLEN(str(IN_MOVE_SELF)) + STRLEN(str(IN_UNMOUNT)) + STRLEN(str(E_IN_Q_OVERFLOW)) + STRLEN(str(E_IN_IGNORED)) + STRLEN(str(E_IN_ONLYDIR)) + STRLEN(str(E_IN_DONT_FOLLOW)) + STRLEN(str(E_IN_EXCL_UNLINK)) + STRLEN(str(E_IN_MASK_CREATE)) + STRLEN(str(E_IN_MASK_ADD)) + STRLEN(str(E_IN_ISDIR)) + STRLEN(str(E_IN_ONESHOT)) + STRLEN(str(E_IN_ALL_EVENTS))

enum INCRON_TAB_ARG_ENUM {
    TAB_ARG_DOLLAR = 0,
    TAB_ARG_PATH,
    TAB_ARG_EVENT_FILENAME,
    TAB_ARG_EVENT_TEXT,
    TAB_ARG_EVENT_NUM,
    TAB_ARG_MAX
};

struct incron_hook_arg {
    enum INCRON_TAB_ARG_ENUM arg;
    uint32_t index;
    uint32_t pos;
    char* argv;
    struct list_head list;
};

struct incron_path {
    char* path;                 ///> path to watch
    uint32_t flags;             ///> current ordered flags (passed with inotify_add_watch)
    int wfd;                    ///> inotify watch fd

    UT_hash_handle hh;          ///> makes this structure hashable
    struct list_head list;      ///>
    struct list_head hook_list; ///>
};

struct incron_path *incron_paths;

struct incron_hook {
    char* command;              ///> path to executable with arguments
    uint32_t flags;             ///> reaction flags
    uint32_t iflags;            ///> special incrond flags
    int8_t fired;               ///> hook was fired at least one time

    uint8_t arg_list_size;      ///> size of proccessed argument list
    struct list_head list;      ///>
    struct list_head arg_list;  ///> argument list

    int argc;                   ///> parsed argument count
    char** argv;                ///> parsed argv list

    uid_t pw_uid;               ///> user ID
    gid_t pw_gid;               ///> group ID
};

struct incron_hook_single {
    struct incron_hook hook;
    pid_t spawned;              ///> pid of the spawned proccess hook
};

struct incron_hook_loopable {
    struct incron_hook hook;
    int8_t max_spawned;         ///> maximum allowed forks per hook
    pid_t spawned[];            ///> array of pids spawned
};

int loadTab(int /*dirfd*/, const char* /*fileName*/, uid_t /*uid*/, gid_t /*gid*/);
struct incron_hook* loadTabLine(int /*line_num*/, char* /*line*/, size_t /*len*/);
int loadSystemTabs(int /*dirfd*/);
int loadUserTabs(int /*dirfd*/);
void freeTabs();

#endif
