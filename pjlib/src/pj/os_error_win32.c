/* $Header: /pjproject-0.3/pjlib/src/pj/os_error_win32.c 3     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/os_error_win32.c $
 * 
 * 3     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 2     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 1     9/17/05 10:36a Bennylp
 * Created.
 *
 */
#include <pj/errno.h>
#include <pj/assert.h>
#include <pj/compat/stdarg.h>
#include <pj/compat/sprintf.h>
#include <pj/compat/vsprintf.h>
#include <pj/string.h>


#if defined(PJ_HAS_WINSOCK2_H) && PJ_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#elif defined(PJ_HAS_WINSOCK_H) && PJ_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif


/*
 * From Apache's APR:
 */
static const struct {
    pj_os_err_type code;
    const char *msg;
} gaErrorList[] = {
    {WSAEINTR,           "Interrupted system call"},
    {WSAEBADF,           "Bad file number"},
    {WSAEACCES,          "Permission denied"},
    {WSAEFAULT,          "Bad address"},
    {WSAEINVAL,          "Invalid argument"},
    {WSAEMFILE,          "Too many open sockets"},
    {WSAEWOULDBLOCK,     "Operation would block"},
    {WSAEINPROGRESS,     "Operation now in progress"},
    {WSAEALREADY,        "Operation already in progress"},
    {WSAENOTSOCK,        "Socket operation on non-socket"},
    {WSAEDESTADDRREQ,    "Destination address required"},
    {WSAEMSGSIZE,        "Message too long"},
    {WSAEPROTOTYPE,      "Protocol wrong type for socket"},
    {WSAENOPROTOOPT,     "Bad protocol option"},
    {WSAEPROTONOSUPPORT, "Protocol not supported"},
    {WSAESOCKTNOSUPPORT, "Socket type not supported"},
    {WSAEOPNOTSUPP,      "Operation not supported on socket"},
    {WSAEPFNOSUPPORT,    "Protocol family not supported"},
    {WSAEAFNOSUPPORT,    "Address family not supported"},
    {WSAEADDRINUSE,      "Address already in use"},
    {WSAEADDRNOTAVAIL,   "Can't assign requested address"},
    {WSAENETDOWN,        "Network is down"},
    {WSAENETUNREACH,     "Network is unreachable"},
    {WSAENETRESET,       "Net connection reset"},
    {WSAECONNABORTED,    "Software caused connection abort"},
    {WSAECONNRESET,      "Connection reset by peer"},
    {WSAENOBUFS,         "No buffer space available"},
    {WSAEISCONN,         "Socket is already connected"},
    {WSAENOTCONN,        "Socket is not connected"},
    {WSAESHUTDOWN,       "Can't send after socket shutdown"},
    {WSAETOOMANYREFS,    "Too many references, can't splice"},
    {WSAETIMEDOUT,       "Connection timed out"},
    {WSAECONNREFUSED,    "Connection refused"},
    {WSAELOOP,           "Too many levels of symbolic links"},
    {WSAENAMETOOLONG,    "File name too long"},
    {WSAEHOSTDOWN,       "Host is down"},
    {WSAEHOSTUNREACH,    "No route to host"},
    {WSAENOTEMPTY,       "Directory not empty"},
    {WSAEPROCLIM,        "Too many processes"},
    {WSAEUSERS,          "Too many users"},
    {WSAEDQUOT,          "Disc quota exceeded"},
    {WSAESTALE,          "Stale NFS file handle"},
    {WSAEREMOTE,         "Too many levels of remote in path"},
    {WSASYSNOTREADY,     "Network system is unavailable"},
    {WSAVERNOTSUPPORTED, "Winsock version out of range"},
    {WSANOTINITIALISED,  "WSAStartup not yet called"},
    {WSAEDISCON,         "Graceful shutdown in progress"},
    {WSAHOST_NOT_FOUND,  "Host not found"},
    {WSANO_DATA,         "No host data of that type was found"},
    {0,                  NULL}
};


PJ_DEF(pj_status_t) pj_get_os_error(void)
{
    return PJ_STATUS_FROM_OS(GetLastError());
}

PJ_DEF(void) pj_set_os_error(pj_status_t code)
{
    SetLastError(PJ_STATUS_TO_OS(code));
}

PJ_DEF(pj_status_t) pj_get_netos_error(void)
{
    return PJ_STATUS_FROM_OS(WSAGetLastError());
}

PJ_DEF(void) pj_set_netos_error(pj_status_t code)
{
    WSASetLastError(PJ_STATUS_TO_OS(code));
}

/* 
 * platform_strerror()
 *
 * Platform specific error message. This file is called by pj_strerror() 
 * in errno.c 
 */
int platform_strerror( pj_os_err_type os_errcode, 
                       char *buf, pj_size_t bufsize)
{
    int len;

    pj_assert(buf != NULL);
    pj_assert(bufsize >= 0);

    /*
     * MUST NOT check stack here.
     * This function might be called from PJ_CHECK_STACK() itself!
       //PJ_CHECK_STACK();
     */

    len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM 
			 | FORMAT_MESSAGE_IGNORE_INSERTS,
			 NULL,
			 os_errcode,
			 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
			 (LPTSTR)buf,
			 (DWORD)bufsize,
			 NULL);

    if (!len) {
	int i;
        for (i = 0; gaErrorList[i].msg; ++i) {
            if (gaErrorList[i].code == os_errcode) {
                len = strlen(gaErrorList[i].msg);
		if ((pj_size_t)len >= bufsize) {
		    len = bufsize-1;
		}
		pj_memcpy(buf, gaErrorList[i].msg, len);
		buf[len] = '\0';
                break;
            }
        }
    }

    if (!len) {
	len = snprintf( buf, bufsize, "Unknown native error %u", (unsigned)os_errcode);
	buf[len] = '\0';
    }

    return len;
}

