// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: CC0-1.0
#include <check.h>

#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "../src/incrond-config.c"

#define TEST_USER "user"

static char* test_users[] = {
    TEST_USER "\n",
    "\t  " TEST_USER "\n",   // whitespaces
    TEST_USER "  \t  \n", // trailing whitespaces
    TEST_USER, // no endline
    0
};

START_TEST (parse_users)
{
    int i = 0;
    char* user;
    while(test_users[i] != 0) {
        user = loadUserLine(test_users[i]);
        ck_assert_msg(user != 0, "parsing %s failed", test_users[i]);
        ck_assert_msg(strncmp(user, TEST_USER, strlen(user)) == 0, "parsing %s failed", test_users[i]);
        free(user);
        i++;
    }
}
END_TEST

static char* long_username = "useruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruseruser";

START_TEST (parse_long_username)
{
    char* user;
    user = loadUserLine(long_username);
    ck_assert_msg(user == 0, "parsing parse_long_username failed");
    ck_assert_msg(errno == EINVAL, "parsing parse_long_username failed");
}
END_TEST

static char file_template[] = "/tmp/incrondXXXXXX";

#ifndef MULTI_LINE_STRING
#define MULTI_LINE_STRING(a) #a
#endif

const char* allowed_file = MULTI_LINE_STRING(
    linus
);

START_TEST ( allow_user )
{
    /** allowed users file exists*/
    /** denied users file not exists*/
    static char fname[NAME_MAX];
    int ret;

    strncpy(fname, file_template, strlen(file_template));

    int file_fd = mkstemp(fname);
    ck_assert_msg(file_fd != -1, "creating temporary file failed with %d : %s", errno, strerror(errno));

    ret = write(file_fd, allowed_file, strlen(allowed_file));
    ck_assert_msg(ret != -1, "writing to temporary file failed with %d : %s", errno, strerror(errno));

    allowed_users_file = fname;
    denied_users_file = 0;
    loadUsers();

    ck_assert_msg(allowPolicy() == INCRON_ALLOW_EXIST);
    ck_assert_msg(userAllowed("linus") == true);

    freeUsers();

    close(file_fd);
    unlink(fname);
}
END_TEST

const char* denied_file = MULTI_LINE_STRING(
    lennart
);

START_TEST ( deny_user )
{
    /** allowed users file exists*/
    /** denied users file not exists*/
    static char fname[NAME_MAX];
    int ret;

    strncpy(fname, file_template, strlen(file_template));

    int file_fd = mkstemp(fname);
    ck_assert_msg(file_fd != -1, "creating temporary file failed with %d : %s", errno, strerror(errno));

    ret = write(file_fd, denied_file, strlen(denied_file));
    ck_assert_msg(ret != -1, "writing to temporary file failed with %d : %s", errno, strerror(errno));

    allowed_users_file = 0;
    denied_users_file = fname;
    loadUsers();

    ck_assert_msg(allowPolicy() == INCRON_DENY_EXIST);
    ck_assert_msg(userAllowed("linus") == true);
    ck_assert_msg(userAllowed("lennart") == false);

    freeUsers();

    close(file_fd);
    unlink(fname);
}
END_TEST

START_TEST ( deny_all )
{
    allowed_users_file = 0;
    denied_users_file = 0;

    loadUsers();

    ck_assert_msg(allowPolicy() == INCRON_EEXIST);

    ck_assert_msg(userAllowed("linus") == false);
    ck_assert_msg(userAllowed("lennart") == false);
}
END_TEST

Suite * parse_users_suite(void)
{
    Suite *s;
    TCase *tc_parse_users;
    TCase *tc_parse_long_username;
    TCase *tc_allow_user;
    TCase *tc_deny_user;
    TCase *tc_deny_all;

    s = suite_create("Testing user parsing function");

    tc_parse_users = tcase_create("parse users");
    tcase_add_test(tc_parse_users, parse_users);
    suite_add_tcase(s, tc_parse_users);

    tc_parse_long_username = tcase_create("parse long username");
    tcase_add_test(tc_parse_long_username, parse_long_username);
    suite_add_tcase(s, tc_parse_long_username);

    tc_allow_user = tcase_create("check allow user working");
    tcase_add_test(tc_allow_user, allow_user);
    suite_add_tcase(s, tc_allow_user);

    tc_deny_user = tcase_create("check deny user working");
    tcase_add_test(tc_deny_user, deny_user);
    suite_add_tcase(s, tc_deny_user);

    tc_deny_all = tcase_create("check deny all user working");
    tcase_add_test(tc_deny_all, deny_all);
    suite_add_tcase(s, tc_deny_all);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    openlog("parse_users_suite", LOG_PERROR, LOG_DAEMON);

    s = parse_users_suite();
    sr = srunner_create(s);

    if(srunner_has_tap(sr))
        srunner_run_all(sr, CK_SILENT);
    else
        srunner_run_all(sr, CK_VERBOSE);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
