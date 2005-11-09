/* $Id$
 *
 */
#ifndef __PJPP_TYPES_H__
#define __PJPP_TYPES_H__

#include <pj/types.h>

class Pj_Pool;
class Pj_Socket ;
class Pj_Lock;


//
// PJLIB initializer.
//
class Pjlib
{
public:
    Pjlib()
    {
        pj_init();
    }
};

//
// Class Pj_Object is declared in pool.hpp
//

//
// Time value wrapper.
//
class Pj_Time_Val : public pj_time_val
{
public:
    Pj_Time_Val()
    {
    }

    Pj_Time_Val(long init_sec, long init_msec)
    {
        sec = init_sec;
        msec = init_msec;
    }

    Pj_Time_Val(const Pj_Time_Val &rhs) 
    { 
        sec=rhs.sec; 
        msec=rhs.msec; 
    }

    explicit Pj_Time_Val(const pj_time_val &tv) 
    { 
        sec = tv.sec; 
        msec = tv.msec; 
    }

    long get_sec()  const    
    { 
        return sec; 
    }

    long get_msec() const    
    { 
        return msec; 
    }

    void set_sec (long s)    
    { 
        sec = s; 
    }

    void set_msec(long ms)   
    { 
        msec = ms; 
        normalize(); 
    }

    long to_msec() const 
    { 
        return PJ_TIME_VAL_MSEC((*this)); 
    }

    bool operator == (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_EQ((*this), rhs);  
    }

    bool operator >  (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_GT((*this), rhs);  
    }

    bool operator >= (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_GTE((*this), rhs); 
    }

    bool operator <  (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_LT((*this), rhs);  
    }

    bool operator <= (const Pj_Time_Val &rhs) const 
    { 
        return PJ_TIME_VAL_LTE((*this), rhs); 
    }

    Pj_Time_Val & operator = (const Pj_Time_Val &rhs) 
    {
	sec = rhs.sec;
	msec = rhs.msec;
	return *this;
    }
 
    Pj_Time_Val & operator += (const Pj_Time_Val &rhs) 
    {
	PJ_TIME_VAL_ADD((*this), rhs);
	return *this;
    }

    Pj_Time_Val & operator -= (const Pj_Time_Val &rhs) 
    {
	PJ_TIME_VAL_SUB((*this), rhs);
	return *this;
    }

    /* Must include os.hpp to use these, otherwise unresolved in linking */
    inline pj_status_t	   gettimeofday();
    inline pj_parsed_time  decode();
    inline pj_status_t     encode(const pj_parsed_time *pt);
    inline pj_status_t     to_gmt();
    inline pj_status_t     to_local();


private:
    void normalize() 
    { 
        pj_time_val_normalize(this); 
    }

};

#endif	/* __PJPP_TYPES_H__ */
