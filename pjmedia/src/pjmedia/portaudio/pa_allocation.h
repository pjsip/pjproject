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
#ifndef PA_ALLOCATION_H
#define PA_ALLOCATION_H
/*
 * $Id: pa_allocation.h,v 1.1.2.4 2003/09/20 21:04:44 rossbencina Exp $
 * Portable Audio I/O Library allocation context header
 * memory allocation context for tracking allocation groups
 *
 * Based on the Open Source API proposed by Ross Bencina
 * Copyright (c) 1999-2002 Ross Bencina, Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** @file
 @brief Allocation Group prototypes. An Allocation Group makes it easy to
 allocate multiple blocks of memory and free them all simultanously.
 
 An allocation group is useful for keeping track of multiple blocks
 of memory which are allocated at the same time (such as during initialization)
 and need to be deallocated at the same time. The allocation group maintains
 a list of allocated blocks, and can deallocate them all simultaneously which
 can be usefull for cleaning up after a partially initialized object fails.

 The allocation group implementation is built on top of the lower
 level allocation functions defined in pa_util.h
*/


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


typedef struct
{
    long linkCount;
    struct PaUtilAllocationGroupLink *linkBlocks;
    struct PaUtilAllocationGroupLink *spareLinks;
    struct PaUtilAllocationGroupLink *allocations;
}PaUtilAllocationGroup;



/** Create an allocation group.
*/
PaUtilAllocationGroup* PaUtil_CreateAllocationGroup( void );

/** Destroy an allocation group, but not the memory allocated through the group.
*/
void PaUtil_DestroyAllocationGroup( PaUtilAllocationGroup* group );

/** Allocate a block of memory though an allocation group.
*/
void* PaUtil_GroupAllocateMemory( PaUtilAllocationGroup* group, long size );

/** Free a block of memory that was previously allocated though an allocation
 group. Calling this function is a relatively time consuming operation.
 Under normal circumstances clients should call PaUtil_FreeAllAllocations to
 free all allocated blocks simultaneously.
 @see PaUtil_FreeAllAllocations
*/
void PaUtil_GroupFreeMemory( PaUtilAllocationGroup* group, void *buffer );

/** Free all blocks of memory which have been allocated through the allocation
 group. This function doesn't destroy the group itself.
*/
void PaUtil_FreeAllAllocations( PaUtilAllocationGroup* group );


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PA_ALLOCATION_H */
