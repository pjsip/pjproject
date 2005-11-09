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

    return 0;
}

