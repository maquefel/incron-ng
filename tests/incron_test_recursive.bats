#!/usr/bin/env bats
# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0

load test_setup

wait_for_file() {
    cnt=$(($2 - 1))
    while ! test -f "$1"; do
        [ $cnt -eq 0 ] && return 1
        cnt="$(($cnt - 1))"
        sleep 0.1
    done

    return 0
}

RECURSIVE_DIRECTORIES=$(cat <<EOF
1
1/2
1/2/3
1/2/3/4
1/2/3/4/5
EOF
)

@test "hook_in_recursive" {
    TAB_NAME=etc/incron.d/hook_in_recursive
    LOG_NAME=log/RECURSIVE.log

    while IFS='' read -r line || [[ -n "$line" ]]; do
        mkdir tmp/watch_RECURSIVE/$line
        sleep 0.1
    done <<<"$RECURSIVE_DIRECTORIES"

    wait_for_file ${LOG_NAME} 10

    while IFS='' read -r line || [[ -n "$line" ]]; do
        bn=$(basename $line)
        grep_line="tmp/watch_RECURSIVE.*\ $bn\ IN_CREATE"
        echo $grep_line
        run grep ${LOG_NAME} -e "$grep_line"
        [ "$status" -eq 0 ]
    done <<<"$RECURSIVE_DIRECTORIES"
}
