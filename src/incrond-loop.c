// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#include "incrond-loop.h"

#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/signalfd.h>
#include <sys/inotify.h>

#include "incrond.h"
#include "incrond-parse-tabs.h"
#include "incrond-dispatch.h"

static int shutdown_flag = 0;
static int hup_flag = 0;

/** wrappers */
static struct epoll_wrapper signalfd_w;
static struct epoll_wrapper inotifyfd_w;

/** */
int system_table_dir_fd;
int user_table_dir_fd;

int loop(int sigfd)
{
    int ret = 0;
    int epollfd = 0;
    int inotifyfd = 0;
    int errsv = 0;
    int events_cnt = 0;
    struct incron_path *p = 0;
    struct epoll_event* event = 0;

    epollfd = epoll_create1(EPOLL_CLOEXEC);

    /** initialize signalfd */
    signalfd_w.type = SIGNAL_FD;
    signalfd_w.fd = sigfd;

    event = &signalfd_w.event;

    event->events = EPOLLIN | EPOLLET;
    event->data.ptr = &signalfd_w;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sigfd, event) == -1) {
        errsv = errno;
        syslog(LOG_CRIT, "epoll_ctl : signalfd failed with [%d] : %s", errsv, strerror(errsv));
        goto fail_close_epollfd;
    }

    events_cnt++;

    inotifyfd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    errsv = errno;

    if(inotifyfd == -1) {
        syslog(LOG_EMERG, "inotify_init1 failed with %d:%s", errsv, strerror(errsv));
        goto fail_close_epollfd;
    }

    inotifyfd_w.type = INOTIFY_FD;
    inotifyfd_w.fd = inotifyfd;

    event = &inotifyfd_w.event;

    event->events = EPOLLIN | EPOLLET;
    event->data.ptr = &inotifyfd_w;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, inotifyfd, event) == -1) {
        errsv = errno;
        syslog(LOG_CRIT, "epoll_ctl : adding inotifyfd failed with %d:%s", errsv, strerror(errsv));
        goto fail_close_inotifyfd;
    }

    events_cnt++;

    /** initialize watched paths */
    for(p = incron_paths; p != NULL; p = p->hh.next) {
        ret = inotify_add_watch(inotifyfd, p->path, p->flags);
        debug_printf_n("inotify_add_watch : %d", ret);
        errsv = errno;

        if(ret == -1)
        {
            syslog(LOG_ERR, "adding watch for %s failed with %d:%s", p->path, errsv, strerror(errsv));
            continue;
        }

        p->wfd = ret;

        syslog(LOG_INFO, "added watch for %s", p->path);
    }

    while(!shutdown_flag) {
        struct epoll_event events[events_cnt];

        struct timeval t1 = {0}; // [sec], [us]
        struct timeval t2 = {0};

        gettimeofday(&t1, NULL);
        int nfds = epoll_wait(epollfd, events, events_cnt, -1); // timeout in milliseconds
        errsv = errno;

        if (nfds == 0) {
            /** timeout */
            continue;
        }

        gettimeofday(&t2, NULL);

        for(int i = 0; i < nfds; i++) {
            struct epoll_wrapper* w = (struct epoll_wrapper*)(events[i].data.ptr);

            switch(w->type) {
                case INOTIFY_FD:
                    debug_printf_n("INOTIFY_FD event fired");
                    ret = handle_events(inotifyfd);
                    break;
                case SIGNAL_FD:
                {
                    syslog(LOG_DEBUG, "SIGNAL_FD event fired");
                    struct signalfd_siginfo fdsi = {0};
                    ssize_t len;
                    len = read(sigfd, &fdsi, sizeof(struct signalfd_siginfo));

                    if (len != sizeof(struct signalfd_siginfo))
                        syslog(LOG_CRIT, "reading sigfd failed");

                    switch(fdsi.ssi_signo)
                    {
                        case SIGINT:
                        case SIGTERM:
                            syslog(LOG_DEBUG, "SIGTERM or SIGINT signal recieved - shutting down...");
                            shutdown_flag = 1;
                            break;
                        case SIGHUP:
                            hup_flag = 1;
                            break;
                        case SIGCHLD:
                            syslog(LOG_INFO, "SIGCHLD signal recieved - child [%d] finished with status %d...", fdsi.ssi_pid, fdsi.ssi_status);
                            pid_t pid = waitpid(fdsi.ssi_pid, 0, WNOHANG);
                            if(pid != -1)
                                hook_clear_spawned(pid);
                            break;
                        default:
                            break;
                    }
                }
                break;
                default:
                    break;
            }
        }
    }

    close(inotifyfd);
    close(epollfd);

    return 0;

    fail_close_inotifyfd:
    close(inotifyfd);

    fail_close_epollfd:
    close(epollfd);

    return -1;
}
