/* $Id$ */
/*
 * Copyright (C) 2008-2013 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJSUA_APP_CONFIG_H__
#define __PJSUA_APP_CONFIG_H__

/* This file defines the default app config. It's used by pjsua
 * *mobile* version only. If you're porting pjsua to new mobile
 * platform, you should only include this file once in one of
 * your source file.
 */
const char *pjsua_app_def_argv[] = { "pjsua",
				     "--use-cli",
				     "--no-cli-console",
				     "--cli-telnet-port=2323",
				     "--no-vad",
				     "--quality=4",
#if defined(PJ_CONFIG_BB10) && PJ_CONFIG_BB10
			             "--add-buddy=sip:169.254.0.2",
#endif
			             NULL };


#endif	/* __PJSUA_APP_CONFIG_H__ */

