/*
 * Copyright (C) 2026 Teluu Inc. (http://www.teluu.com)
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

/* pjlib-test's config_site.h, named by CONFIG_PJPROJECT_CONFIG_SITE in
 * prj.conf: the Zephyr preset with a bigger I/O queue, since the ioqueue
 * tests register more sockets than the preset's 16. CONFIG_ZVFS_OPEN_MAX in
 * prj.conf must be at least this.
 */
#define PJ_IOQUEUE_MAX_HANDLES      64

#define PJ_CONFIG_ZEPHYR            1
#include <pj/config_site_sample.h>
