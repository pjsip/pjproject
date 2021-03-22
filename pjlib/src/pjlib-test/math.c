/* $Id$ */
/* 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "test.h"
#include <pjlib.h>

#if INCLUDE_MATH_TEST

static pj_status_t test_stddev_with_floating_point() {
    pj_math_stat stat;
    pj_math_stat_init(&stat);
    pj_math_stat_update(&stat, 1);
    if (pj_math_stat_get_stddev(&stat) != 0) {
      return -1;
    }
    pj_math_stat_update(&stat, 70000);
    const unsigned expected = 34999;
    const unsigned actual = pj_math_stat_get_stddev(&stat);
    if (pj_math_stat_get_stddev(&stat) != expected) {
      printf("Computed a stddev of %d but expected %d\n", actual, expected);
      return -1;
    }
    return PJ_SUCCESS;
}

int math_test(void)
{
    return test_stddev_with_floating_point();
}

#else
int dummy_file_test;
#endif

