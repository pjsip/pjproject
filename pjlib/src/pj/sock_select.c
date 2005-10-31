/* $Header: /pjproject-0.3/pjlib/src/pj/sock_select.c 4     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/sock_select.c $
 * 
 * 4     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 3     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 2     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 * 1     9/15/05 8:40p Bennylp
 * Created.
 */
#include <pj/sock_select.h>
#include <pj/compat/socket.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/errno.h>


#ifdef _MSC_VER
#  pragma warning(disable: 4018)    // Signed/unsigned mismatch in FD_*
#endif

#define PART_FDSET(p_fdsetp)    ((fd_set*)&p_fdsetp->data[1])
#define PART_COUNT(p_fdsetp)    (p_fdsetp->data[0])

PJ_DEF(void) PJ_FD_ZERO(pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    pj_assert(sizeof(pj_fd_set_t)-sizeof(pj_sock_t) >= sizeof(fd_set));

    FD_ZERO(PART_FDSET(fdsetp));
    PART_COUNT(fdsetp) = 0;
}


PJ_DEF(void) PJ_FD_SET(pj_sock_t fd, pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    pj_assert(sizeof(pj_fd_set_t)-sizeof(pj_sock_t) >= sizeof(fd_set));

    if (!PJ_FD_ISSET(fd, fdsetp))
        ++PART_COUNT(fdsetp);
    FD_SET(fd, PART_FDSET(fdsetp));
}


PJ_DEF(void) PJ_FD_CLR(pj_sock_t fd, pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    pj_assert(sizeof(pj_fd_set_t)-sizeof(pj_sock_t) >= sizeof(fd_set));

    if (PJ_FD_ISSET(fd, fdsetp))
        --PART_COUNT(fdsetp);
    FD_CLR(fd, PART_FDSET(fdsetp));
}


PJ_DEF(pj_bool_t) PJ_FD_ISSET(pj_sock_t fd, const pj_fd_set_t *fdsetp)
{
    PJ_CHECK_STACK();
    PJ_ASSERT_RETURN(sizeof(pj_fd_set_t)-sizeof(pj_sock_t) >= sizeof(fd_set),
                     0);

    return FD_ISSET(fd, PART_FDSET(fdsetp));
}

PJ_DEF(pj_size_t) PJ_FD_COUNT(const pj_fd_set_t *fdsetp)
{
    return PART_COUNT(fdsetp);
}

PJ_DEF(int) pj_sock_select( int n, 
			    pj_fd_set_t *readfds, 
			    pj_fd_set_t *writefds,
			    pj_fd_set_t *exceptfds, 
			    const pj_time_val *timeout)
{
    struct timeval os_timeout, *p_os_timeout;

    PJ_CHECK_STACK();

    PJ_ASSERT_RETURN(sizeof(pj_fd_set_t)-sizeof(pj_sock_t) >= sizeof(fd_set),
                     PJ_EBUG);

    if (timeout) {
	os_timeout.tv_sec = timeout->sec;
	os_timeout.tv_usec = timeout->msec * 1000;
	p_os_timeout = &os_timeout;
    } else {
	p_os_timeout = NULL;
    }

    return select(n, PART_FDSET(readfds), PART_FDSET(writefds),
		  PART_FDSET(exceptfds), p_os_timeout);
}

