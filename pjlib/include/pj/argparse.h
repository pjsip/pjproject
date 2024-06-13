/* 
 * Copyright (C) 2008-2024 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_ARGPARSE_H__
#define __PJ_ARGPARSE_H__

/**
 * @file argparse.h
 * @brief Command line argument parser
 */
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/string.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_ARGPARSE Command line argument parser
 * @ingroup PJ_MISC
 * @{
 *
 * This module provides header only utilities to parse command line arguments.
 * This is mostly used by PJSIP test and sample apps. Note that there is
 * getopt() implementation in PJLIB-UTIL (but it's in PJLIB-UTIL, so it can't
 * be used by PJLIB)
 */

/**
 * Peek the next possible option from argv. An argument is considered an
 * option if it starts with "-" and followed by at least another letter that
 * is not digit.
 * 
 * @param argv      The argv, which must be null terminated.
 * 
 * @return next option or NULL.
 */
static char* pj_argparse_peek_next_option(char *const argv[])
{
    while (*argv) {
        if (**argv=='-' && **argv && !pj_isdigit(**argv) )
            return *argv;
        ++argv;
    }
    return NULL;
}

/**
 * Check that an option exists, without modifying argv.
 * 
 * @param opt       The option to find, e.g. "-h", "--help"
 * @param argv      The argv, which must be null terminated.
 * 
 * @return PJ_TRUE if the option exists, else PJ_FALSE.
 */
static pj_bool_t pj_argparse_exists(const char *opt, char *const argv[])
{
    int i;
    for (i=1; argv[i]; ++i) {
        if (pj_ansi_strcmp(argv[i], opt)==0)
            return PJ_TRUE;
    }
    return PJ_FALSE;
}

/**
 * Check for an option and if it exists, returns PJ_TRUE remove that option
 * from argc/argv.
 * 
 * @param opt       The option to find, e.g. "-h", "--help"
 * @param argc      Pointer to argc. 
 * @param argv      Null terminated argv.
 * 
 * @return PJ_TRUE if the option exists, else PJ_FALSE.
 */
static pj_bool_t pj_argparse_get(const char *opt, int *argc, char *argv[])
{
    int i;
    for (i=1; argv[i]; ++i) {
        if (pj_ansi_strcmp(argv[i], opt)==0) {
            pj_memmove(&argv[i], &argv[i+1], ((*argc)-i)*sizeof(char*));
            (*argc)--;
            return PJ_TRUE;
        }
    }
    return PJ_FALSE;
}

/**
 * Check for an option and if it exists, get the value and remove both
 * the option the the value from argc/argv. Note that the function only
 * supports whitespace as separator between option and value (i.e. equal
 * sign is not supported).
 * 
 * @param opt           The option to find, e.g. "-t", "--type"
 * @param argc          Pointer to argc. 
 * @param argv          Null terminated argv.
 * @param ptr_value     Pointer to receive the value.
 * 
 * @return PJ_SUCCESS if the option exists and value is found,
 *         PJ_EINVAL if the option exits but value is not found,
 *         PJ_ENOTFOUND if the option does not exist.
 */
static pj_status_t pj_argparse_get_str( const char *opt, int *argc,
                                        char *argv[], char **ptr_value)
{
    int i;
    for (i=1; argv[i]; ++i) {
        if (pj_ansi_strcmp(argv[i], opt)==0) {
            pj_memmove(&argv[i], &argv[i+1], ((*argc)-i)*sizeof(char*));
            (*argc)--;

            if (argv[i]) {
                char *val = argv[i];
                pj_memmove(&argv[i], &argv[i+1], ((*argc)-i)*sizeof(char*));
                (*argc)--;
                *ptr_value = val;
                return PJ_SUCCESS;
            } else {
                return PJ_EINVAL;
            }
        }
    }
    return PJ_ENOTFOUND;
}

/**
 * Check for an option and if it exists, get the integer value and remove both
 * the option the the value from argc/argv. Note that the function only
 * supports whitespace as separator between option and value (i.e. equal
 * sign is not supported)
 * 
 * @param opt           The option to find, e.g. "-h", "--help"
 * @param argc          Pointer to argc. 
 * @param argv          Null terminated argv.
 * @param ptr_value     Pointer to receive the value.
 * 
 * @return PJ_SUCCESS if the option exists and value is found,
 *         PJ_EINVAL if the option exits but value is not found or invalid,
 *         PJ_ENOTFOUND if the option does not exist.
 */
static pj_status_t pj_argparse_get_int( char *opt, int *argc, char *argv[],
                                        int *ptr_value)
{
    char *endptr, *sval;
    long val;
    pj_status_t status = pj_argparse_get_str(opt, argc, argv, &sval);
    if (status != PJ_SUCCESS)
        return status;
    
    val = strtol(sval, &endptr, 10);
    if (*endptr)
        return PJ_EINVAL;
    
    *ptr_value = (int)val;
    return PJ_SUCCESS;
}

/**
 * @}
 */

PJ_END_DECL


#endif  /* __PJ_ARGPARSE_H__ */

