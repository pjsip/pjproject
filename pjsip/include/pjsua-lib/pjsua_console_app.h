/* $Id$ */
/* 
 * Copyright (C) 2003-2006 Benny Prijono <benny@prijono.org>
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
#ifndef __PJSUA_CONSOLE_APP_H__
#define __PJSUA_CONSOLE_APP_H__


pj_status_t pjsua_console_app_logging_init(const pjsua_config *cfg);
void pjsua_console_app_logging_shutdown(void);

void pjsua_console_app_main(void);

extern pjsip_module pjsua_console_app_msg_logger;
extern pjsua_callback console_callback;

#endif	/* __PJSUA_CONSOLE_APP_H__ */
