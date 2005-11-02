/* $Id$
 */
#include <pj/os.h>
#include <pj/compat/high_precision.h>

#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0

#define U32MAX  (0xFFFFFFFFUL)
#define NANOSEC (1000000000UL)
#define USEC    (1000000UL)
#define MSEC    (1000)

static pj_highprec_t get_elapsed( const pj_timestamp *start,
                                  const pj_timestamp *stop )
{
    pj_highprec_t elapsed_hi, elapsed_lo;

    elapsed_hi = stop->u32.hi - start->u32.hi;
    elapsed_lo = stop->u32.lo - start->u32.lo;

    /* elapsed_hi = elapsed_hi * U32MAX */
    pj_highprec_mul(elapsed_hi, U32MAX);

    return elapsed_hi + elapsed_lo;
}

static pj_highprec_t elapsed_usec( const pj_timestamp *start,
                                   const pj_timestamp *stop )
{
    pj_timestamp ts_freq;
    pj_highprec_t freq, elapsed;

    if (pj_get_timestamp_freq(&ts_freq) != PJ_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
    freq = ts_freq.u32.hi;
    pj_highprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * USEC / freq */
    pj_highprec_mul(elapsed, USEC);
    pj_highprec_div(elapsed, freq);

    return elapsed;
}

PJ_DEF(pj_uint32_t) pj_elapsed_nanosec( const pj_timestamp *start,
                                        const pj_timestamp *stop )
{
    pj_timestamp ts_freq;
    pj_highprec_t freq, elapsed;

    if (pj_get_timestamp_freq(&ts_freq) != PJ_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
    freq = ts_freq.u32.hi;
    pj_highprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * USEC / freq */
    pj_highprec_mul(elapsed, NANOSEC);
    pj_highprec_div(elapsed, freq);

    return (pj_uint32_t)elapsed;
}

PJ_DEF(pj_uint32_t) pj_elapsed_usec( const pj_timestamp *start,
                                     const pj_timestamp *stop )
{
    return (pj_uint32_t)elapsed_usec(start, stop);
}

PJ_DEF(pj_time_val) pj_elapsed_time( const pj_timestamp *start,
                                     const pj_timestamp *stop )
{
    pj_highprec_t elapsed = elapsed_usec(start, stop);
    pj_time_val tv_elapsed;

    if (PJ_HIGHPREC_VALUE_IS_ZERO(elapsed)) {
        tv_elapsed.sec = tv_elapsed.msec = 0;
        return tv_elapsed;
    } else {
        pj_highprec_t sec, msec;

        sec = elapsed;
        pj_highprec_div(sec, USEC);
        tv_elapsed.sec = (long)sec;

        msec = elapsed;
        pj_highprec_mod(msec, USEC);
        pj_highprec_div(msec, 1000);
        tv_elapsed.msec = (long)msec;

        return tv_elapsed;
    }
}

PJ_DEF(pj_uint32_t) pj_elapsed_cycle( const pj_timestamp *start,
                                      const pj_timestamp *stop )
{
    return stop->u32.lo - start->u32.lo;
}

#endif  /* PJ_HAS_HIGH_RES_TIMER */

