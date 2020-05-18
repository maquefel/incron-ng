# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0
CC=$(CROSS_COMPILE)gcc

CFLAGS+=-Wall -std=gnu11 -D_GNU_SOURCE -fPIC

# VERSION
MAJOR=0
MINOR=0
PATCH=0
VERSION = -DVERSION_MAJOR=$(MAJOR) -DVERSION_MINOR=$(MINOR) -DVERSION_PATCH=$(PATCH)

INCLUDE += -Isrc/

.PHONY: all
all: incrond incrontab

.PHONY: debug
debug: CFLAGS+= -O0 -DDEBUG -ggdb -g3 -Wno-unused-variable
debug: all

.PHONY: gcov
gcov: CFLAGS+= -fprofile-arcs -ftest-coverage -DGCOV=1 -DGCOV_PREFIX="${CURDIR}"
gcov: debug

.PHONY: asan
asan: CFLAGS+= -fsanitize=address -fsanitize=leak
asan: gcov

.PHONY: error-check
error-check: CFLAGS+=-Werror -O
error-check: all

.PHONY: tests
tests:
	make -C tests asan

incrond: incrond.o incrond-loop.o incrond-parse-tabs.o incrond-config.o incrond-exec.o incrond-dispatch.o cmdline.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

incrontab: incrontab.o incrond-parse-tabs.o incrond-config.o incrond-dispatch.o incrond-exec.o cmdline.o utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

incrontab.o: src/incrontab.c
	$(CC) $(CFLAGS) -c src/incrontab.c $(INCLUDE)

incrond.o: src/incrond.c
	$(CC) $(CFLAGS) -c src/incrond.c $(INCLUDE)

incrond-loop.o: src/incrond-loop.c
	$(CC) $(CFLAGS) -c src/incrond-loop.c $(INCLUDE)

incrond-parse-tabs.o: src/incrond-parse-tabs.c
	$(CC) $(CFLAGS) -c src/incrond-parse-tabs.c $(INCLUDE)

incrond-config.o: src/incrond-config.c
	$(CC) $(CFLAGS) -c src/incrond-config.c $(INCLUDE)

incrond-exec.o: src/incrond-exec.c
	$(CC) $(CFLAGS) -c src/incrond-exec.c $(INCLUDE)

incrond-dispatch.o: src/incrond-dispatch.c
	$(CC) $(CFLAGS) -c src/incrond-dispatch.c $(INCLUDE)

cmdline.o: src/cmdline.c
	$(CC) $(CFLAGS) -c src/cmdline.c $(INCLUDE) -Wno-unused-variable

utils.o: src/utils.c
	$(CC) $(CFLAGS) -c src/utils.c $(INCLUDE)

DESTDIR=
prefix = /usr/local
exec_prefix = $(DESTDIR)$(prefix)
sbindir = $(exec_prefix)/sbin

.PHONY: install uninstall
install: all
	install -v -m 0755 incrond $(sbindir)/incrond

uninstall:
	-rm $(sbindir)/incrond

clean::
	-rm *.o incrond incrontab

html:
	mkdir -p $@

.PHONY: report

report: html asan tests
	lcov -t "incrond" -o incrond.info -c -d .
	lcov --remove incrond.info \
	'*cmdline.*' \
	'*daemonize*' \
	'*tests/*' \
	-o incrond.info
	genhtml -o html incrond.info

clean::
	-rm *.gcda *.gcno
	-rm -rf html
	-rm -rf incrond.info
