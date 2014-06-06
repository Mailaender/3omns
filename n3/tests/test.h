/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    n3 - net communication library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

    3omns is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    3omns is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with 3omns.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __test_h__
#define __test_h__

#include <stdio.h>
#include <stdlib.h>


#define test_assert(x, n) do { \
    if(!(x)) \
        test_assert_failed(__FILE__, __LINE__, __func__, #x, n); \
} while(0)

static inline void test_assert_failed(
    const char *restrict file,
    int line,
    const char *restrict func,
    const char *restrict test,
    const char *restrict name
) {
    printf("FAILED: %s (%s) at %s:%d(%s)\n", name, test, file, line, func);
    exit(1);
}


#endif
