/* $Header: /pjproject/pjlib/src/pj/scanner_i.h 4     6/04/05 4:29p Bennylp $ */
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


#define PJ_SCAN_IS_SPACE(c)	((c)==' ' || (c)=='\t')
#define PJ_SCAN_IS_NEWLINE(c)	((c)=='\r' || (c)=='\n')
#define PJ_SCAN_CHECK_EOF(s)	(s != end)
//#define PJ_SCAN_CHECK_EOF(s)	(1)


PJ_IDEF(void) pj_scan_syntax_err(pj_scanner *scanner)
{
    (*scanner->callback)(scanner);
}


