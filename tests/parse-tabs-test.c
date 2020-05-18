// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: CC0-1.0
#include <check.h>

#include <syslog.h>
#include <stdlib.h>

#include "../src/cmdline.c"
#include "../src/incrond-config.c"
#include "../src/incrond-parse-tabs.c"

static char* test_string[] = {
    "/tmp\tIN_ALL_EVENTS\tabcd $@/$# $%",
    "/usr/bin\tIN_ACCESS,IN_NO_LOOP\tabcd $#",
    "/home\tIN_CREATE\t/usr/local/bin/abcd $#",
    "/var/log\t12\tabcd $@/$#",
};

START_TEST (legacy_tables_parse)
{
    struct incron_hook* hook = 0;

    hook = loadTabLine(0, test_string[0], strlen(test_string[0]));
    ck_assert_msg(hook != 0, "parsing %s failed", test_string[0]);

    hook = loadTabLine(1, test_string[1], strlen(test_string[1]));
    ck_assert_msg(hook != 0, "parsing %s failed", test_string[1]);

    hook = loadTabLine(2, test_string[2], strlen(test_string[2]));
    ck_assert_msg(hook != 0, "parsing %s failed", test_string[2]);

    hook = loadTabLine(3, test_string[3], strlen(test_string[3]));
    ck_assert_msg(hook != 0, "parsing %s failed", test_string[3]);

    freeTabs();
}
END_TEST


Suite * parse_tabs_suite(void)
{
    Suite *s;
    TCase *tc_legacy_tables_parse;

    s = suite_create("Testing tab parsing function");

    tc_legacy_tables_parse = tcase_create("parse legacy tables");
    tcase_add_test(tc_legacy_tables_parse, legacy_tables_parse);
    suite_add_tcase(s, tc_legacy_tables_parse);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    openlog("parse_tabs_suite", LOG_PERROR, LOG_DAEMON);

    s = parse_tabs_suite();
    sr = srunner_create(s);

    if(srunner_has_tap(sr))
        srunner_run_all(sr, CK_SILENT);
    else
        srunner_run_all(sr, CK_VERBOSE);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
