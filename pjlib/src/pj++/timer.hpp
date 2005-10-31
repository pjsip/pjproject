/* $Header: /pjproject/pjlib/src/pj++/timer.hpp 4     8/24/05 10:29a Bennylp $ */
#ifndef __PJPP_TIMER_H__
#define __PJPP_TIMER_H__

#include <pj/timer.h>
#include <pj++/types.hpp>

class PJ_Timer_Heap;

class PJ_Timer_Entry : private pj_timer_entry
{
    friend class PJ_Timer_Heap;

public:
    static void timer_heap_callback(pj_timer_heap_t *, pj_timer_entry *);

    PJ_Timer_Entry() { cb = &timer_heap_callback; }
    PJ_Timer_Entry(int arg_id, void *arg_user_data)
    {
	cb = &timer_heap_callback; 
	init(arg_id, arg_user_data);
    }

    virtual void on_timeout() = 0;

    void init(int arg_id, void *arg_user_data)
    {
	id = arg_id;
	user_data = arg_user_data;
    }

    int get_id() const
    {
	return id;
    }

    void set_id(int arg_id)
    {
	id = arg_id;
    }

    void set_user_data(void *arg_user_data)
    {
	user_data = arg_user_data;
    }

    void *get_user_data() const
    {
	return user_data;
    }

    const PJ_Time_Val &get_timeout() const
    {
	pj_assert(sizeof(PJ_Time_Val) == sizeof(pj_time_val));
	return (PJ_Time_Val&)_timer_value;
    }
};

class PJ_Timer_Heap
{
public:
    PJ_Timer_Heap() {}

    bool create(PJ_Pool *pool, pj_size_t initial_count, 
		unsigned flag = PJ_TIMER_HEAP_SYNCHRONIZE)
    {
	ht_ = pj_timer_heap_create(pool->pool_(), initial_count, flag);
	return ht_ != NULL;
    }

    pj_timer_heap_t *get_timer_heap()
    {
	return ht_;
    }

    bool schedule( PJ_Timer_Entry *ent, const PJ_Time_Val &delay)
    {
	return pj_timer_heap_schedule(ht_, ent, &delay) == 0;
    }

    bool cancel(PJ_Timer_Entry *ent)
    {
	return pj_timer_heap_cancel(ht_, ent) == 1;
    }

    pj_size_t count()
    {
	return pj_timer_heap_count(ht_);
    }

    void earliest_time(PJ_Time_Val *t)
    {
	pj_timer_heap_earliest_time(ht_, t);
    }

    int poll(PJ_Time_Val *next_delay = NULL)
    {
	return pj_timer_heap_poll(ht_, next_delay);
    }

private:
    pj_timer_heap_t *ht_;
};

#endif	/* __PJPP_TIMER_H__ */
