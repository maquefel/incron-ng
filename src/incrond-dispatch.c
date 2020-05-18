// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#include "incrond-dispatch.h"

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <alloca.h>
#include <stdio.h>

#include <linux/limits.h>

#include <sys/time.h>
#include <sys/types.h>
#include <pwd.h>

#include <syslog.h>

#include "incrond.h"

#include "incrond-parse-tabs.h"
#include "incrond-exec.h"

#include "uthash.h"

#include "c_bitops.h"

struct pid_list_t {
    pid_t pid;
    struct incron_hook *hook;
    UT_hash_handle hh;
};

struct pid_list_t* pid_list = 0;

static char text_argument_list[MAX_TEXT_ARGS_STRLEN];

char* print_text_events(uint32_t events)
{
    unsigned bit = 0;

    char* pTmp = text_argument_list;

    for_each_set_bit(bit, events, BITS_PER_TYPE(events))
    {
        uint16_t length = strlen(incrond_hook_modifiers[bit].name);
        memcpy(pTmp, incrond_hook_modifiers[bit].name, length);
        pTmp += length;
        *pTmp = ',';
        pTmp++;
    }

    *(pTmp - 1) = '\0';

    return text_argument_list;
}

static char num_argument[16];
static char* print_num_events(uint32_t events)
{
    snprintf(num_argument, sizeof(num_argument), "%u", events);
    return num_argument;
}

static char two_dollars[3] = "$$";
/** @todo calculate size of arg string on loadTabs */
char* bash_arg = "/bin/bash";
char* bash_arg1 = "-c";
char shell_arg[ARG_MAX];

char **build_shell_argv(const struct incron_path* path, const struct incron_hook *hook, const struct inotify_event* event, uint32_t cross)
{
    /** form  args list */
    char** argv = (char**)malloc(4*sizeof(char*));

    argv[0] = bash_arg;
    argv[1] = bash_arg1;
    argv[2] = shell_arg;
    argv[3] = 0;

    struct incron_hook_arg* arg = 0;

    arg = list_first_entry(&(hook->arg_list), struct incron_hook_arg, list);

    int j = 0;
    int offset = 0;
    int len = 0;
    while(hook->argv[j] != 0) {
        debug_printf_n("hook->argv[%d] = %s", j, hook->argv[j]);

        if(arg && arg->index == j) {
            debug_printf_n("found incron arg[%u] at %u", arg->index, arg->pos);

            while(arg && arg->index == j) {
                memcpy(shell_arg + offset, hook->argv[j], arg->pos);
                offset += arg->pos;

                char* r_arg = 0; /** replace string */

                switch(arg->arg) {
                    case TAB_ARG_DOLLAR:
                        /** do nothing, why should do anything in this case ?*/
                        /** i don't get why original incron considered this as a special case*/
                        r_arg = two_dollars;
                        break;
                    case TAB_ARG_PATH:
                        r_arg = path->path;
                        break;
                    case TAB_ARG_EVENT_FILENAME:
                        if(event->len > 0)
                            r_arg = (char*)event->name;
                        else {
                            debug_printf_n("event->name is empty replacing with nothing");
                        }
                        break;
                    case TAB_ARG_EVENT_TEXT:
                        r_arg = print_text_events(cross);
                        break;
                    case TAB_ARG_EVENT_NUM:
                        r_arg = print_num_events(cross);
                        break;
                    default:
                        break;
                }

                if(r_arg) {
                    len = strlen(r_arg) + 1;
                    memcpy(shell_arg + offset, r_arg, len);
                    offset += len;
                    shell_arg[offset - 1] = ' ';
                }

                if(!list_is_last(&(arg->list), &(hook->arg_list)))
                    arg = list_next_entry(arg, list);
                else
                    arg = 0;
            }
        } else {
            len = strlen(hook->argv[j]) + 1;
            strncpy(shell_arg + offset, hook->argv[j], len);
            offset += len;
            shell_arg[offset - 1] = ' ';
        }

        j++;
    };

    shell_arg[offset - 1] = '\0';

    // shell_arg
    debug_printf_n("shell_arg = %s", shell_arg);

    return argv;
}

#ifdef GCOV
void __gcov_flush(void);
#endif

static void prepare_and_exec(struct incron_path* path,
                             const struct inotify_event* event,
                             const struct incron_hook *hook,
                             uint32_t cross) __attribute__ ((noreturn));

static void prepare_and_exec(struct incron_path* path,
                            const struct inotify_event* event,
                            const struct incron_hook *hook,
                            uint32_t cross)
{
    /** @todo check if no loop and hook already in progress */
    /** @todo check if oneshot already fired */
    /** @todo check if fire delay is set */
    /** @todo alter look for ps command */

    /** setsid */
    pid_t pid = setsid();

    if(pid == (pid_t)-1)
    {
        syslog(LOG_WARNING, "failed setsid with %d : %s", errno, strerror(errno));
    }

    if(hook->pw_uid != getuid()) {
        debug_printf_n("hook->pw_uid != getuid() : %d != %d", hook->pw_uid, getuid());

        /** change dirs, uid, gid */
        size_t bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
        char buffer[bufsize];
        struct passwd pwd;
        struct passwd *result;

        /** change to user directory */
        int ret = getpwuid_r(hook->pw_uid, &pwd, buffer, bufsize, &result);
        if(ret != 0) {
            syslog(LOG_CRIT, "failed getting user (uid=%d) creditals after fork with [%d] : %s", hook->pw_uid, ret , strerror(ret));
            exit(EXIT_FAILURE);
        }

        ret = chdir(pwd.pw_dir);
        if(ret == -1) {
            syslog(LOG_CRIT, "failed changing user (uid=%d) dir to %s with [%d] : %s", hook->pw_uid, pwd.pw_dir, errno, strerror(errno));
            exit(EXIT_FAILURE);
        }

        /** set git first since once we set uid, we've lost root privledges, set uid*/
        if (setgid(hook->pw_gid) == -1 || setuid(hook->pw_uid) == -1) {
            syslog(LOG_CRIT, "failed setting user gid/uid after fork with [%d] : %s", errno , strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    char pathenv[strlen(getenv("PATH")) + sizeof("PATH=")];
    sprintf(pathenv, "PATH=%s", getenv("PATH"));
    char *envp[] = {pathenv, 0};

    char **argv = build_shell_argv(path, hook, event, cross);

#ifdef GCOV
    __gcov_flush();
#endif
    exec_start("/bin/bash", argv, envp);

    syslog(LOG_CRIT, "failed exec with %d : %s", errno, strerror(errno));

    exit(EXIT_FAILURE);
};

int dispatch_hooks(struct incron_path* path, const struct inotify_event* event)
{
    int errsv = 0;

    if(list_empty(&(path->hook_list)))
        return 0; /** empty list is a good list*/

    uint32_t mask = event->mask;

    struct incron_hook *hook = 0;
    struct list_head *pos = 0;

    list_for_each(pos, &(path->hook_list)) {
        hook = list_entry(pos, struct incron_hook, list);

        /** check if event is applicable */
        uint32_t cross = mask & hook->flags;
        if(!cross)
            continue;

        /** fork here to prevent main program wasting time for preparing launch */
        pid_t pid = fork();

        switch(pid) {
            case -1:
                errsv = errno;
                goto fail;
            case 0:
                prepare_and_exec(path, event, hook, cross);
                break;
            default:
                break;
        }

        hook->fired = 1;

        struct pid_list_t* new_pid = (struct pid_list_t*)malloc(sizeof(struct pid_list_t));
        new_pid->hook = hook;
        new_pid->pid = pid;
        HASH_ADD(hh, pid_list, pid, sizeof(pid_t), new_pid);

        syslog(LOG_NOTICE, "spawned child %s [%d]", hook->command, new_pid->pid);
    }

    return 0;

    fail:
    errno = errsv;
    return -1;
}

int clear_hooks()
{
    return 0;
}

int hook_clear_spawned(pid_t pid)
{
    struct pid_list_t* pid_ = 0;
    HASH_FIND(hh, pid_list, &pid, sizeof(pid_t), pid_);

    if(pid_ == 0)
        return -1;

    /** find assosiated hook if any */
    // struct incron_hook* hook = pid_->hook;

    HASH_DEL(pid_list, pid_);
    free(pid_);

    return 0;
}

static struct incron_path* find_path(int wfd)
{
    struct incron_path *p = 0;

    for(p = incron_paths; p != NULL; p = p->hh.next)
        if(p->wfd == wfd)
            return p;

    return 0;
}

int handle_events(int inotifyfd)
{
    char buffer[4096]
    __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    int errsv = 0;
    ssize_t len;
    char *ptr;

    do {
        len = read(inotifyfd, buffer, sizeof buffer);
        errsv = errno;

        if(len == -1) {
            if(errsv == EAGAIN) break;
            goto fail;
        }

        for (ptr = buffer; ptr < buffer + len;
             ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            struct incron_path* path = find_path(event->wd);

            if(path == 0) {
                debug_printf_n("watch descriptor %d not found in path array", event->wd);
                continue;
            }

            debug_printf_n("firing hooks for %s", path->path);
            dispatch_hooks(path, event);
        }
    } while(1);

    return 0;

    fail:
    errno = errsv;
    return -1;
}
