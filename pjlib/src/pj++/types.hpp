/* $Header: /pjproject/pjlib/src/pj++/types.hpp 3     4/17/05 11:59a Bennylp $ */
#ifndef __PJPP_TYPES_H__
#define __PJPP_TYPES_H__

#include <pj/types.h>

class PJ_Pool;
class PJ_Socket;


class PJ_Time_Val : public pj_time_val
{
public:
    PJ_Time_Val() {}
    PJ_Time_Val(const PJ_Time_Val &rhs) { sec=rhs.sec; msec=rhs.msec; }
    explicit PJ_Time_Val(const pj_time_val &tv) { sec = tv.sec; msec = tv.msec; }

    long get_sec()  const    { return sec; }
    long get_msec() const    { return msec; }
    void set_sec (long s)    { sec = s; }
    void set_msec(long ms)   { msec = ms; normalize(); }
    long to_msec() const { return PJ_TIME_VAL_MSEC((*this)); }

    bool operator == (const PJ_Time_Val &rhs) const { return PJ_TIME_VAL_EQ((*this), rhs);  }
    bool operator >  (const PJ_Time_Val &rhs) const { return PJ_TIME_VAL_GT((*this), rhs);  }
    bool operator >= (const PJ_Time_Val &rhs) const { return PJ_TIME_VAL_GTE((*this), rhs); }
    bool operator <  (const PJ_Time_Val &rhs) const { return PJ_TIME_VAL_LT((*this), rhs);  }
    bool operator <= (const PJ_Time_Val &rhs) const { return PJ_TIME_VAL_LTE((*this), rhs); }

    PJ_Time_Val & operator = (const PJ_Time_Val &rhs) {
	sec = rhs.sec;
	msec = rhs.msec;
	return *this;
    }
 
    PJ_Time_Val & operator += (const PJ_Time_Val &rhs) {
	PJ_TIME_VAL_ADD((*this), rhs);
	return *this;
    }

    PJ_Time_Val & operator -= (const PJ_Time_Val &rhs) {
	PJ_TIME_VAL_SUB((*this), rhs);
	return *this;
    }

    /* Must include os.hpp to use these, otherwise unresolved in linking */
    pj_status_t	   gettimeofday();
    pj_parsed_time decode();
    pj_status_t    encode(const pj_parsed_time *pt);
    pj_status_t    to_gmt();
    pj_status_t    to_local();


private:
    void normalize() { pj_time_val_normalize(this); }

};

#endif	/* __PJPP_TYPES_H__ */
