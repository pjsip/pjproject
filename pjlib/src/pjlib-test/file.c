/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
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

#if INCLUDE_FILE_TEST

#define FILENAME                "testfil1.txt"
#define NEWNAME                 "testfil2.txt"
#define INCLUDE_FILE_TIME_TEST  0
#define THIS_FILE               "file.c"

static char buffer[11] = {'H', 'e', 'l', 'l', 'o', ' ',
                          'W', 'o', 'r', 'l', 'd' };

static int file_test_internal(void)
{
    enum { FILE_MAX_AGE = 1000 };
    pj_oshandle_t fd = 0;
    pj_status_t status;
    char readbuf[sizeof(buffer)+16];
    pj_file_stat stat;
    pj_time_val start_time;
    pj_ssize_t size;
    pj_off_t pos;

    PJ_LOG(3,("", "..file io test.."));

    /* Get time. */
    pj_gettimeofday(&start_time);

    /* Delete original file if exists. */
    if (pj_file_exists(FILENAME))
        pj_file_delete(FILENAME);

    /*
     * Write data to the file.
     */
    PJ_TEST_SUCCESS(pj_file_open(NULL, FILENAME, PJ_O_WRONLY, &fd),
                    NULL, return -10);

    size = sizeof(buffer);
    PJ_TEST_SUCCESS(pj_file_write(fd, buffer, &size), NULL,
                    { pj_file_close(fd); return -20; });
    PJ_TEST_EQ(size, sizeof(buffer), NULL, return -25 );
    PJ_TEST_SUCCESS( pj_file_close(fd), NULL, return -30 );

    /* Check the file existance and size. */
    PJ_TEST_TRUE( pj_file_exists(FILENAME), NULL, return -40 );

    PJ_TEST_EQ( pj_file_size(FILENAME), sizeof(buffer), NULL, return -50 );

    /* Get file stat. */
    PJ_TEST_SUCCESS(pj_file_getstat(FILENAME, &stat), NULL, return -60 );

    /* Check stat size. */
    PJ_TEST_EQ(stat.size, sizeof(buffer), NULL, return -70 );

#if INCLUDE_FILE_TIME_TEST
    /* Check file creation time >= start_time. */
    if (!PJ_TIME_VAL_GTE(stat.ctime, start_time))
        return -80;
    /* Check file creation time is not much later. */
    PJ_TIME_VAL_SUB(stat.ctime, start_time);
    if (stat.ctime.sec > FILE_MAX_AGE)
        return -90;

    /* Check file modification time >= start_time. */
    if (!PJ_TIME_VAL_GTE(stat.mtime, start_time))
        return -80;
    /* Check file modification time is not much later. */
    PJ_TIME_VAL_SUB(stat.mtime, start_time);
    if (stat.mtime.sec > FILE_MAX_AGE)
        return -90;

    /* Check file access time >= start_time. */
    if (!PJ_TIME_VAL_GTE(stat.atime, start_time))
        return -80;
    /* Check file access time is not much later. */
    PJ_TIME_VAL_SUB(stat.atime, start_time);
    if (stat.atime.sec > FILE_MAX_AGE)
        return -90;
#endif

    /*
     * Re-open the file and read data.
     */
    PJ_TEST_SUCCESS(pj_file_open(NULL, FILENAME, PJ_O_RDONLY, &fd),
                    NULL, return -100);

    size = 0;
    while (size < (pj_ssize_t)sizeof(readbuf)) {
        pj_ssize_t read;
        read = 1;
        status = pj_file_read(fd, &readbuf[size], &read);
        if (status != PJ_SUCCESS) {
            PJ_LOG(3,("", "...error reading file after %ld bytes "
                          "(error follows)", size));
            app_perror("...error", status);
            pj_file_close(fd);
            return -110;
        }
        if (read == 0) {
            // EOF
            break;
        }
        size += read;
    }

    PJ_TEST_EQ(size, sizeof(buffer), NULL, 
               {pj_file_close(fd); return -120; });

    /*
    if (!pj_file_eof(fd, PJ_O_RDONLY))
        return -130;
     */

    PJ_TEST_EQ(pj_memcmp(readbuf, buffer, size), 0, NULL,
               {pj_file_close(fd); return -140; });

    /* Seek test. */
    PJ_TEST_SUCCESS(pj_file_setpos(fd, 4, PJ_SEEK_SET), NULL,
                    {pj_file_close(fd); return -141; });

    /* getpos test. */
    PJ_TEST_SUCCESS(pj_file_getpos(fd, &pos), NULL,
                    {pj_file_close(fd); return -142; });
    PJ_TEST_EQ(pos, 4, NULL, {pj_file_close(fd); return -143; });

    PJ_TEST_SUCCESS(pj_file_close(fd), NULL, return -150);

    /*
     * Rename test.
     */
    PJ_TEST_SUCCESS(pj_file_move(FILENAME, NEWNAME), NULL, return -160);
    PJ_TEST_EQ(pj_file_exists(FILENAME), 0, NULL, return -170);
    PJ_TEST_TRUE(pj_file_exists(NEWNAME), NULL, return -180);
    PJ_TEST_EQ(pj_file_size(NEWNAME), sizeof(buffer), NULL, return -190);

    /* Delete test. */
    PJ_TEST_SUCCESS(pj_file_delete(NEWNAME), NULL, return -200);
    PJ_TEST_EQ(pj_file_exists(NEWNAME), 0, NULL, return -210);

    return 0;
}


int file_test(void)
{
    int rc = file_test_internal();

    /* Delete test file if exists. */
    if (pj_file_exists(FILENAME))
        pj_file_delete(FILENAME);

    return rc;
}

#else
int dummy_file_test;
#endif

