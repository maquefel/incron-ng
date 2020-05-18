# new generation inotify daemon

(c) Nikita Shubin, 2020

## Content

1. About
2. Binary version
3. Obtain the source code
4. Requirements
5. Building
5.1 Cross-compiling
6. Testing
7. How to use
8. Bugs, suggestions
9. Licensing

## 1. About

This program is the "inotify cron" system. It consist of a daemon and a table manipulator. You can use it a similar way as the regular cron. The difference is that the inotify cron handles filesystem events rather than time periods.

This project was kicked off by Lukas Jelinek in 2006 and then unfortunatally abandoned in 2012. Upstream development and bug-tracking/fixing continued in 2014 on GitHub: https://github.com/ar-/incron.

And abandoned again in 2017.

Due to some nasty bugs like zombie spawning and my disagreement about how main polling loop should work and forked off child shoud be proccessed i've written my own implentation of incrond daemon in C.

I have no intention on rewriting inotify-cxx through.

Thank you Lukas Jelinek and Andreas Altair Redmer you did a really great job! 

## 2. Binary version

No binary distributions are currently provided.

## 3. Obtain the source code

Main repository is located at https://github.com/maquefel/incron-ng.

See release tab for stable version.

## 4. Requirements

- kernel-headers
- gcc

## 5. Building

```
$ make
# make install
```

### 5.1 Cross-compiling

(arm toolchain for example)

```
$ CROSS_COMPILE=arm-hardfloat-eabi- make
```

## 6. Testing

```
$ make tests
```

## 7. How to use

Tab files are parsed as following:

<watch path> <flags> <command>

Where:

- argv[0]  - watch path
- argv[1]  - flags (coma separated)
- argv[2]+ - path to executable and args (no parsing enviroment currently)

Args are parsed according common cmdline rules see:

https://github.com/maquefel/buildargv/blob/master/test_cmdline.c

Following flags are supported and tested (see http://man7.org/linux/man-pages/man7/inotify.7.html) :

```
IN_ACCESS
IN_ATTRIB
IN_CLOSE_WRITE
IN_CLOSE_NOWRITE
IN_CREATE
IN_DELETE
IN_DELETE_SELF
IN_MODIFY
IN_MOVE_SELF
IN_MOVED_FROM
IN_MOVED_TO
IN_OPEN
```

```
$ make tests
```

After this see a bunch of table in tests/etc/incron.d/ and tests/incron_test_hooks.bats for invokation and usage.

## 8. Bugs, suggestions

https://github.com/maquefel/incron-ng/issues

## 9. Licensing

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License,
version 2  (see LICENSE-GPL).

Some parts may be also covered by other licenses.
Please look into the source files for detailed information.

# SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
# SPDX-License-Identifier: CC0-1.0
