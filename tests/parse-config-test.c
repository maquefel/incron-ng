// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: CC0-1.0
#include <check.h>

#include <syslog.h>
#include <stdlib.h>

#include "../src/incrond-config.c"

static char* test_config[] = {
    "system_table_dir = etc/incron.d\n",
    "user_table_dir = var/spool/incron\n",
    "allowed_users = etc/incron.allow\n",
    "denied_users = etc/incron.deny\n",
    "lockfile_dir = var/run\n",
    "lockfile_name = incrond\n",
    "editor = vim\n",
    0
};

START_TEST (parse_config)
{
    int ret = 0;
    int i = 0;
    while(test_config[i] != 0) {
        ret = loadConfigLine(i, test_config[i]);
        ck_assert_msg(ret == 0, "parsing %s failed", test_config[0]);
        i++;
    }
}
END_TEST


Suite * parse_config_suite(void)
{
    Suite *s;
    TCase *tc_parse_config;

    s = suite_create("Testing config parsing function");

    tc_parse_config = tcase_create("parse config");
    tcase_add_test(tc_parse_config, parse_config);
    suite_add_tcase(s, tc_parse_config);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    openlog("parse_config_suite", LOG_PERROR, LOG_DAEMON);

    s = parse_config_suite();
    sr = srunner_create(s);

    if(srunner_has_tap(sr))
        srunner_run_all(sr, CK_SILENT);
    else
        srunner_run_all(sr, CK_VERBOSE);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
