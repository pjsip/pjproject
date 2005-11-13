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
#include <pj++/file.hpp>
#include <pj++/list.hpp>
#include <pj++/lock.hpp>
#include <pj++/hash.hpp>
#include <pj++/os.hpp>
#include <pj++/proactor.hpp>
#include <pj++/sock.hpp>
#include <pj++/string.hpp>
#include <pj++/timer.hpp>
#include <pj++/tree.hpp>

class My_Async_Op : public Pj_Async_Op
{
};

class My_Event_Handler : public Pj_Event_Handler
{
};

int main()
{
    Pjlib lib;
    Pj_Caching_Pool mem;
    Pj_Pool the_pool;
    Pj_Pool *pool = &the_pool;
    
    the_pool.attach(mem.create_pool(4000,4000));

    Pj_Semaphore_Lock lsem(pool);
    Pj_Semaphore_Lock *plsem;

    plsem = new(pool) Pj_Semaphore_Lock(pool);
    delete plsem;

    Pj_Proactor proactor(pool, 100, 100);

    My_Event_Handler *event_handler = new(the_pool) My_Event_Handler;
    proactor.register_socket_handler(pool, event_handler);
    proactor.unregister_handler(event_handler);

    return 0;
}

