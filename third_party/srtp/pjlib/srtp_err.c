/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include "err.h"
#include <pj/log.h>

/* Redirect libsrtp error to PJ_LOG */

srtp_err_reporting_level_t err_level = srtp_err_level_error;

void srtp_err_report(srtp_err_reporting_level_t priority, const char *format, ...)
{
  va_list args;

#if PJ_LOG_MAX_LEVEL >= 1
  if (priority <= err_level) {

    va_start(args, format);
    pj_log("libsrtp", priority, format, args);
    va_end(args);
  }
#endif
}

void srtp_err_reporting_set_level(srtp_err_reporting_level_t lvl)
{ 
  err_level = lvl;
}

srtp_err_status_t srtp_err_reporting_init(void)
{
    return srtp_err_status_ok;
}

srtp_err_status_t srtp_install_err_report_handler(srtp_err_report_handler_func_t func)
{
    PJ_UNUSED_ARG(func);
    return srtp_err_status_ok;
}