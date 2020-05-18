// SPDX-FileCopyrightText: 2020 Nikita Shubin <me@maquefel.me>
// SPDX-License-Identifier: GPL-2.0-only
#ifndef __C_BITOPTS_H__
#define __C_BITOPTS_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <strings.h> // for ffs
#include <string.h>  // for ffsl and ffsll

#define _ffs(x) _Generic((x), \
                char: ffs, \
                int: ffs, \
                long int: ffsl, \
                long long int: ffsll, \
                default: ffsll)(x)

#define __ffs(x, size) (_ffs(x) == 0 ? size : _ffs(x) - 1)

#define __popcount(x) _Generic((x), \
                  char: __builtin_popcount, \
                  int: __builtin_popcount, \
                  unsigned int: __builtin_popcount, \
                  unsigned long: __builtin_popcountl, \
                  default: __builtin_popcountl)(x)

#define BITS_PER_BYTE 8
#define BITS_PER_TYPE(type) (sizeof(type) * BITS_PER_BYTE)
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_TYPE(long))
#define BITS_TO_BYTES(nr) DIV_ROUND_UP(nr, BITS_PER_BYTE)

#define BIT_MASK(nr)    (nr == BITS_PER_TYPE(1ULL) ? 0x0 : ~((1ULL << nr) - 1))

#define first_set_bit_from(addr, bit, size) __ffs((addr) & BIT_MASK(bit), size)

#define for_each_set_bit(bit, addr, size)                       \
  for ((bit) = __ffs((addr), (size));                           \
       (bit) < (size);                                          \
       (bit) = first_set_bit_from((addr), (bit + 1), (size)))

#define for_each_set_bit_from(bit, addr, size)                  \
  for ((bit) = first_set_bit_from((addr), (bit), (size));       \
       (bit) < (size);                                          \
       (bit) = first_set_bit_from((addr), (bit), (size)))

#define popcount(x) __popcount(x)

#endif // __C_BITOPTS_H__
