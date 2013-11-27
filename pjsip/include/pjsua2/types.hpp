/* $Id$ */
/* 
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA2_TYPES_HPP__
#define __PJSUA2_TYPES_HPP__

#ifdef _MSC_VER
#   pragma warning( disable : 4290 ) // exception spec ignored
#endif

/**
 * @file pjsua2/types.hpp
 * @brief PJSUA2 Base Types
 */
#include <pjsua2/config.hpp>

#include <string>
#include <vector>

/**
 * @defgroup PJSUA2_TYPES Data structure
 * @ingroup PJSUA2_Ref
 * @{
 */

/** PJSUA2 API is inside pj namespace */
namespace pj
{
using std::string;
using std::vector;

/** Array of strings */
typedef std::vector<std::string> StringVector;

/** Array of integers */
typedef std::vector<int> IntVector;

/**
 * Type of token, i.e. arbitrary application user data
 */
typedef void *Token;

/**
 * Socket address, encoded as string. The socket address contains host
 * and port number in "host[:port]" format. The host part may contain
 * hostname, domain name, IPv4 or IPv6 address. For IPv6 address, the
 * address will be enclosed with square brackets, e.g. "[::1]:5060".
 */
typedef string SocketAddress;

/**
 * Transport ID is an integer.
 */
typedef int TransportId;

/**
 * Transport handle, corresponds to pjsip_transport instance.
 */
typedef void *TransportHandle;

/*
 * Forward declaration of Account to be used
 * by Endpoint.
 */
class Account;


/**
 * Constants
 */
enum
{
    /** Invalid ID, equal to PJSUA_INVALID_ID */
    INVALID_ID	= -1,

    /** Success, equal to PJ_SUCCESS */
    SUCCESS = 0
};

//////////////////////////////////////////////////////////////////////////////

/**
 * This structure contains information about an error that is thrown
 * as an exception.
 */
struct Error
{
    /** The error code. */
    pj_status_t	status;

    /** The PJSUA API operation that throws the error. */
    string	title;

    /** The error message */
    string	reason;

    /** The PJSUA source file that throws the error */
    string	srcFile;

    /** The line number of PJSUA source file that throws the error */
    int		srcLine;

    /** Build error string. */
    string	info(bool multi_line=false) const;

    /** Default constructor */
    Error();

    /**
     * Construct an Error instance from the specified parameters. If
     * \a prm_reason is empty, it will be filled with the error description
     *  for the status code.
     */
    Error(pj_status_t prm_status,
          const string &prm_title,
          const string &prm_reason,
          const string &prm_src_file,
          int prm_src_line);
};


/*
 * Error utilities.
 */
#if PJSUA2_ERROR_HAS_EXTRA_INFO
#   define PJSUA2_RAISE_ERROR(status)		\
	PJSUA2_RAISE_ERROR2(status, __FUNCTION__)

#   define PJSUA2_RAISE_ERROR2(status,op)	\
	PJSUA2_RAISE_ERROR3(status, op, string())

#   define PJSUA2_RAISE_ERROR3(status,op,txt)	\
	do { \
	    Error err_ = Error(status, op, txt, __FILE__, __LINE__); \
	    PJ_LOG(1,(THIS_FILE, "%s", err_.info().c_str())); \
	    throw err_; \
	} while (0)

#else
#   define PJSUA2_RAISE_ERROR(status)		\
	PJSUA2_RAISE_ERROR2(status, string())

#   define PJSUA2_RAISE_ERROR2(status,op)	\
	PJSUA2_RAISE_ERROR3(status, op, string())

#   define PJSUA2_RAISE_ERROR3(status,op,txt)	\
	do { \
	    Error err_ = Error(status, op, txt, string(), 0); \
	    PJ_LOG(1,(THIS_FILE, "%s", err_.info().c_str())); \
	    throw err_; \
	} while (0)

#endif

#define PJSUA2_CHECK_RAISE_ERROR2(status, op)	\
	do { \
	    if (status != PJ_SUCCESS) { \
		PJSUA2_RAISE_ERROR2(status, op); \
	    } \
	} while (0)

#define PJSUA2_CHECK_RAISE_ERROR(status)	\
	PJSUA2_CHECK_RAISE_ERROR2(status, "")

#define PJSUA2_CHECK_EXPR(expr)			\
	do { \
	    pj_status_t the_status = expr; 	\
	    PJSUA2_CHECK_RAISE_ERROR2(the_status, #expr); \
	} while (0)

//////////////////////////////////////////////////////////////////////////////

struct Version
{
    /** Major number */
    int		major;

    /** Minor number */
    int		minor;

    /** Additional revision number */
    int		rev;

    /** Version suffix (e.g. "-svn") */
    string	suffix;

    /** The full version info (e.g. "2.1.0-svn") */
    string	full;

    /**
     * PJLIB version number as three bytes with the following format:
     * 0xMMIIRR00, where MM: major number, II: minor number, RR: revision
     * number, 00: always zero for now.
     */
    unsigned	numeric;
};


} // namespace pj

/**
 * @}  PJSUA2
 */



#endif	/* __PJSUA2_TYPES_HPP__ */
