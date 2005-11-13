/* $Id$ */
/* 
 * PJLIB - PJ Foundation Library
 * (C)2003-2005 Benny Prijono <bennylp@bulukucing.org>
 *
 * Author:
 *  Benny Prijono <bennylp@bulukucing.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/file_access.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <windows.h>
#include <time.h>

/*
 * pj_file_exists()
 */
PJ_DEF(pj_bool_t) pj_file_exists(const char *filename)
{
    HANDLE hFile;

    PJ_ASSERT_RETURN(filename != NULL, 0);

    hFile = CreateFile(filename, READ_CONTROL, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    CloseHandle(hFile);
    return PJ_TRUE;
}


/*
 * pj_file_size()
 */
PJ_DEF(pj_off_t) pj_file_size(const char *filename)
{
    HANDLE hFile;
    DWORD sizeLo, sizeHi;
    pj_off_t size;

    PJ_ASSERT_RETURN(filename != NULL, -1);

    hFile = CreateFile(filename, READ_CONTROL, 
                       FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    sizeLo = GetFileSize(hFile, &sizeHi);
    if (sizeLo == INVALID_FILE_SIZE) {
        DWORD dwStatus = GetLastError();
        if (dwStatus != NO_ERROR) {
            CloseHandle(hFile);
            return -1;
        }
    }

    size = sizeHi;
    size = (size << 32) + sizeLo;

    CloseHandle(hFile);
    return size;
}


/*
 * pj_file_delete()
 */
PJ_DEF(pj_status_t) pj_file_delete(const char *filename)
{
    PJ_ASSERT_RETURN(filename != NULL, PJ_EINVAL);

    if (DeleteFile(filename) == FALSE)
        return PJ_RETURN_OS_ERROR(GetLastError());

    return PJ_SUCCESS;
}


/*
 * pj_file_move()
 */
PJ_DEF(pj_status_t) pj_file_move( const char *oldname, const char *newname)
{
    BOOL rc;

    PJ_ASSERT_RETURN(oldname!=NULL && newname!=NULL, PJ_EINVAL);

#if PJ_WIN32_WINNT >= 0x0400
    rc = MoveFileEx(oldname, newname, 
                    MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING);
#else
    rc = MoveFile(oldname, newname);
#endif

    if (!rc)
        return PJ_RETURN_OS_ERROR(GetLastError());

    return PJ_SUCCESS;
}


static pj_status_t file_time_to_time_val(const FILETIME *file_time,
                                         pj_time_val *time_val)
{
    SYSTEMTIME systemTime, localTime;
    struct tm tm;

    if (!FileTimeToSystemTime(file_time, &systemTime))
        return -1;

    if (!SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &localTime))
        return -1;

    memset(&tm, 0, sizeof(struct tm));
    tm.tm_year = localTime.wYear - 1900;
    tm.tm_mon = localTime.wMonth - 1;
    tm.tm_mday = localTime.wDay;
    tm.tm_hour = localTime.wHour;
    tm.tm_min = localTime.wMinute;
    tm.tm_sec = localTime.wSecond;
    tm.tm_isdst = 0;

    time_val->sec = mktime(&tm);
    if (time_val->sec == (time_t)-1)
        return -1;

    time_val->msec = localTime.wMilliseconds;

    return PJ_SUCCESS;
}

/*
 * pj_file_getstat()
 */
PJ_DEF(pj_status_t) pj_file_getstat(const char *filename, pj_file_stat *stat)
{
    HANDLE hFile;
    DWORD sizeLo, sizeHi;
    FILETIME creationTime, accessTime, writeTime;

    PJ_ASSERT_RETURN(filename!=NULL && stat!=NULL, PJ_EINVAL);

    hFile = CreateFile(filename, READ_CONTROL, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return PJ_RETURN_OS_ERROR(GetLastError());

    sizeLo = GetFileSize(hFile, &sizeHi);
    if (sizeLo == INVALID_FILE_SIZE) {
        DWORD dwStatus = GetLastError();
        if (dwStatus != NO_ERROR) {
            CloseHandle(hFile);
            return PJ_RETURN_OS_ERROR(dwStatus);
        }
    }

    stat->size = sizeHi;
    stat->size = (stat->size << 32) + sizeLo;

    if (GetFileTime(hFile, &creationTime, &accessTime, &writeTime)==FALSE) {
        DWORD dwStatus = GetLastError();
        CloseHandle(hFile);
        return PJ_RETURN_OS_ERROR(dwStatus);
    }

    CloseHandle(hFile);

    if (file_time_to_time_val(&creationTime, &stat->ctime) != PJ_SUCCESS)
        return PJ_RETURN_OS_ERROR(GetLastError());

    file_time_to_time_val(&accessTime, &stat->atime);
    file_time_to_time_val(&writeTime, &stat->mtime);

    return PJ_SUCCESS;
}

