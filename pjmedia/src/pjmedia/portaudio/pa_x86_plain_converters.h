/* 
 * PJMEDIA - Multimedia over IP Stack 
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
#ifndef PA_X86_PLAIN_CONVERTERS_H
#define PA_X86_PLAIN_CONVERTERS_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/**
 @brief Install optimised converter functions suitable for all IA32 processors
*/
void PaUtil_InitializeX86PlainConverters( void );


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PA_X86_PLAIN_CONVERTERS_H */
