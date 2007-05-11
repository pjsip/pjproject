/* $Id$ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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
#include <pjmedia/tonegen.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/pool.h>


/* float can be twice slower on i686! */
#define DATA	double

/* amplitude */
#define AMP	8192


#ifndef M_PI
#   define M_PI  ((DATA)3.141592653589793238462643383279)
#endif


#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT!=0
    /*
     * Default floating-point based tone generation using sine wave 
     * generation from:
     *   http://www.musicdsp.org/showone.php?id=10.
     * This produces good quality tone in relatively faster time than
     * the normal sin() generator.
     * Speed = 40.6 cycles per sample.
     */
#   include <math.h>
    struct gen
    {
	DATA a, s0, s1;
    };

#   define GEN_INIT(var,R,F,A)	 var.a = (DATA) (2.0 * sin(M_PI * F / R)); \
				 var.s0 = A; \
				 var.s1 = 0
#   define GEN_SAMP(val,var)	 var.s0 = var.s0 - var.a * var.s1; \
				 var.s1 = var.s1 + var.a * var.s0; \
				 val = (short) var.s0


#elif !defined(PJ_HAS_FLOATING_POINT) || PJ_HAS_FLOATING_POINT==0
    /* 
     * Fallback algorithm when floating point is disabled.
     * This is a very fast fixed point tone generation using sine wave
     * approximation from
     *    http://www.audiomulch.com/~rossb/code/sinusoids/ 
     * Quality wise not so good, but it's blazing fast!
     * Speed: 
     *	- with volume adjustment: 14 cycles per sample 
     *  - without volume adjustment: 12.22 cycles per sample
     */
    PJ_INLINE(int) approximate_sin3(unsigned x)
    {	
	    unsigned s=-(int)(x>>31);
	    x+=x;
	    x=x>>16;
	    x*=x^0xffff;            // x=x*(2-x)
	    x+=x;                   // optional
	    return x^s;
    }
    struct gen
    {
	unsigned add;
	unsigned c;
	unsigned vol;
    };

#   define MAXI			((unsigned)0xFFFFFFFF)
#   define SIN			approximate_sin3
#   if 1    /* set this to 0 to disable volume adjustment */
#	define VOL(var,v)	(((v) * var.vol) >> 16)
#   else
#	define VOL(var,v)	(v)
#   endif
#   define GEN_INIT(var,R,F,A)	var.add = MAXI/R * F, var.c=0, var.vol=A
#   define GEN_SAMP(val,var)	val = (short) VOL(var,SIN(var.c)>>16);\
				var.c += var.add


#else
#   error "Should never get to this part"
#   include <math.h>
    /*
     * Should never really reach here, but anyway it's provided for reference.
     * This is the good old tone generator using sin().
     * Speed = 222.5 cycles per sample.
     */
    struct gen
    {
	DATA add;
	DATA c;
	DATA vol;
    };

#   define GEN_INIT(var,R,F,A) var.add = ((DATA)F)/R, var.c=0, var.vol=A
#   define GEN_SAMP(val,var)   val = (short)(sin(var.c * 2 * M_PI) * var.vol);\
			       var.c += var.add

#endif

struct gen_state
{
    struct gen tone1;
    struct gen tone2;
    pj_bool_t  has_tone2;
};


static void init_generate_single_tone(struct gen_state *state,
				      unsigned clock_rate, 
				      unsigned freq,
				      unsigned vol)
{
    GEN_INIT(state->tone1,clock_rate,freq,vol);
    state->has_tone2 = PJ_FALSE;
}

static void generate_single_tone(struct gen_state *state,
				 unsigned channel_count,
				 unsigned samples,
				 short buf[]) 
{
    short *end = buf + samples;

    if (channel_count==1) {

	while (buf < end) {
	    GEN_SAMP(*buf++, state->tone1);
	}

    } else if (channel_count == 2) {

	while (buf < end) {
	    GEN_SAMP(*buf, state->tone1);
	    *(buf+1) = *buf;
	    buf += 2;
	}
    }
}


static void init_generate_dual_tone(struct gen_state *state,
				    unsigned clock_rate, 
				    unsigned freq1,
				    unsigned freq2,
				    unsigned vol)
{
    GEN_INIT(state->tone1,clock_rate,freq1,vol);
    GEN_INIT(state->tone2,clock_rate,freq2,vol);
    state->has_tone2 = PJ_TRUE;
}


static void generate_dual_tone(struct gen_state *state,
			       unsigned channel_count,
			       unsigned samples,
			       short buf[]) 
{
    short *end = buf + samples;

    if (channel_count==1) {
	int val, val2;
	while (buf < end) {
	    GEN_SAMP(val, state->tone1);
	    GEN_SAMP(val2, state->tone2);
	    *buf++ = (short)((val+val2) >> 1);
	}
    } else if (channel_count == 2) {
	int val, val2;
	while (buf < end) {

	    GEN_SAMP(val, state->tone1);
	    GEN_SAMP(val2, state->tone2);
	    val = (val + val2) >> 1;

	    *buf++ = (short)val;
	    *buf++ = (short)val;
	}
    }
}


static void init_generate_tone(struct gen_state *state,
			       unsigned clock_rate, 
			       unsigned freq1,
			       unsigned freq2,
			       unsigned vol)
{
    if (freq2)
	init_generate_dual_tone(state, clock_rate, freq1, freq2 ,vol);
    else
	init_generate_single_tone(state, clock_rate, freq1,vol);
}


static void generate_tone(struct gen_state *state,
			  unsigned channel_count,
			  unsigned samples,
			  short buf[])
{
    if (!state->has_tone2)
	generate_single_tone(state, channel_count, samples, buf);
    else
	generate_dual_tone(state, channel_count, samples, buf);
}


/****************************************************************************/

#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('t', 'n', 'g', 'n')

struct tonegen
{
    pjmedia_port	base;

    /* options */
    unsigned		options;
    unsigned		playback_options;

    /* Digit map */
    pjmedia_tone_digit_map  *digit_map;

    /* Tone generation state */
    struct gen_state	state;

    /* Currently played digits: */
    unsigned		count;		    /* # of digits		*/
    unsigned		cur_digit;	    /* currently played		*/
    unsigned		dig_samples;	    /* sample pos in cur digit	*/
    pjmedia_tone_desc	digits[PJMEDIA_TONEGEN_MAX_DIGITS];/* array of digits*/
};


/* Default digit map is DTMF */
static pjmedia_tone_digit_map digit_map = 
{
    16,
    {
	{ '0', 941,  1336 },
	{ '1', 697,  1209 },
	{ '2', 697,  1336 },
	{ '3', 697,  1447 },
	{ '4', 770,  1209 },
	{ '5', 770,  1336 },
	{ '6', 770,  1447 },
	{ '7', 852,  1209 },
	{ '8', 852,  1336 },
	{ '9', 852,  1447 },
	{ 'a', 697,  1633 },
	{ 'b', 770,  1633 },
	{ 'c', 852,  1633 },
	{ 'd', 941,  1633 },
	{ '*', 941,  1209 },
	{ '#', 941,  1477 },
    }
};


static pj_status_t tonegen_get_frame(pjmedia_port *this_port, 
				     pjmedia_frame *frame);

/*
 * Create an instance of tone generator with the specified parameters.
 * When the tone generator is first created, it will be loaded with the
 * default digit map.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_create2(pj_pool_t *pool,
					    const pj_str_t *name,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned samples_per_frame,
					    unsigned bits_per_sample,
					    unsigned options,
					    pjmedia_port **p_port)
{
    const pj_str_t STR_TONE_GEN = pj_str("tone-gen");
    struct tonegen  *tonegen;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && clock_rate && channel_count && 
		     samples_per_frame && bits_per_sample == 16 && 
		     p_port != NULL, PJ_EINVAL);

    /* Only support mono and stereo */
    PJ_ASSERT_RETURN(channel_count==1 || channel_count==2, PJ_EINVAL);

    /* Create and initialize port */
    tonegen = PJ_POOL_ZALLOC_T(pool, struct tonegen);
    if (name == NULL || name->slen == 0) name = &STR_TONE_GEN;
    status = pjmedia_port_info_init(&tonegen->base.info, name, 
				    SIGNATURE, clock_rate, channel_count, 
				    bits_per_sample, samples_per_frame);
    if (status != PJ_SUCCESS)
	return status;

    tonegen->options = options;
    tonegen->base.get_frame = &tonegen_get_frame;
    tonegen->digit_map = &digit_map;

    /* Done */
    *p_port = &tonegen->base;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_tonegen_create( pj_pool_t *pool,
					    unsigned clock_rate,
					    unsigned channel_count,
					    unsigned samples_per_frame,
					    unsigned bits_per_sample,
					    unsigned options,
					    pjmedia_port **p_port)
{
    return pjmedia_tonegen_create2(pool, NULL, clock_rate, channel_count,
				   samples_per_frame, bits_per_sample, 
				   options, p_port);
}


/*
 * Check if the tone generator is still busy producing some tones.
 */
PJ_DEF(pj_bool_t) pjmedia_tonegen_is_busy(pjmedia_port *port)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_TRUE);
    return tonegen->count != 0;
}


/*
 * Instruct the tone generator to stop current processing.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_stop(pjmedia_port *port)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);
    tonegen->count = 0;
    tonegen->cur_digit = 0;
    tonegen->dig_samples = 0;
    return PJ_SUCCESS;
}


/*
 * Fill a frame with tones.
 */
static pj_status_t tonegen_get_frame(pjmedia_port *port, 
				     pjmedia_frame *frame)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    short *dst, *end;
    unsigned clock_rate = tonegen->base.info.clock_rate;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);

    if (tonegen->count == 0) {
	/* We don't have digits to play */
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    if (tonegen->cur_digit > tonegen->count) {
	/* We have played all the digits */
	if ((tonegen->options|tonegen->playback_options)&PJMEDIA_TONEGEN_LOOP)
	{
	    /* Reset back to the first tone */
	    tonegen->cur_digit = 0;
	    tonegen->dig_samples = 0;

	} else {
	    tonegen->count = 0;
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    return PJ_SUCCESS;
	}
    }

    if (tonegen->dig_samples>=(tonegen->digits[tonegen->cur_digit].on_msec+
			       tonegen->digits[tonegen->cur_digit].off_msec)*
			       clock_rate / 1000)
    {
	/* We have finished with current digit */
	tonegen->cur_digit++;
	tonegen->dig_samples = 0;
    }

    if (tonegen->cur_digit > tonegen->count) {
	/* After we're finished with the last digit, we have played all 
	 * the digits 
	 */
	if ((tonegen->options|tonegen->playback_options)&PJMEDIA_TONEGEN_LOOP)
	{
	    /* Reset back to the first tone */
	    tonegen->cur_digit = 0;
	    tonegen->dig_samples = 0;

	} else {
	    tonegen->count = 0;
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    return PJ_SUCCESS;
	}
    }
    
    dst = (short*) frame->buf;
    end = dst + port->info.samples_per_frame;

    while (dst < end) {
	const pjmedia_tone_desc *dig = &tonegen->digits[tonegen->cur_digit];
	unsigned required, cnt, on_samp, off_samp;

	required = end - dst;
	on_samp = dig->on_msec * clock_rate / 1000;
	off_samp = dig->off_msec * clock_rate / 1000;

	/* Init tonegen */
	if (tonegen->dig_samples == 0) {
	    init_generate_tone(&tonegen->state, port->info.clock_rate,
			       dig->freq1, dig->freq2, dig->volume);
	}

	/* Add tone signal */
	if (tonegen->dig_samples < on_samp) {
	    cnt = on_samp - tonegen->dig_samples;
	    if (cnt > required)
		cnt = required;
	    generate_tone(&tonegen->state, port->info.channel_count,
			  cnt, dst);
	    dst += cnt;
	    tonegen->dig_samples += cnt;
	    required -= cnt;

	    if (dst == end)
		break;
	}

	/* Add silence signal */
	cnt = off_samp + on_samp - tonegen->dig_samples;
	if (cnt > required)
	    cnt = required;
	pjmedia_zero_samples(dst, cnt);
	dst += cnt;
	tonegen->dig_samples += cnt;

	/* Move to next digit if we're finished with this tone */
	if (tonegen->dig_samples == on_samp + off_samp) {
	    tonegen->cur_digit++;
	    tonegen->dig_samples = 0;

	    if (tonegen->cur_digit >= tonegen->count) {
		/* All digits have been played */
		if ((tonegen->options & PJMEDIA_TONEGEN_LOOP) ||
		    (tonegen->playback_options & PJMEDIA_TONEGEN_LOOP))
		{
		    tonegen->cur_digit = 0;
		} else {
		    break;
		}
	    }
	}
    }

    if (dst < end)
	pjmedia_zero_samples(dst, end-dst);

    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = port->info.bytes_per_frame;

    if (tonegen->cur_digit >= tonegen->count) {
	if ((tonegen->options|tonegen->playback_options)&PJMEDIA_TONEGEN_LOOP)
	{
	    /* Reset back to the first tone */
	    tonegen->cur_digit = 0;
	    tonegen->dig_samples = 0;

	} else {
	    tonegen->count = 0;
	}
    }

    return PJ_SUCCESS;
}


/*
 * Play tones.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_play( pjmedia_port *port,
					  unsigned count,
					  const pjmedia_tone_desc tones[],
					  unsigned options)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    unsigned i;

    PJ_ASSERT_RETURN(port && port->info.signature == SIGNATURE &&
		     count && tones, PJ_EINVAL);

    /* Don't put more than available buffer */
    PJ_ASSERT_RETURN(count+tonegen->count <= PJMEDIA_TONEGEN_MAX_DIGITS,
		     PJ_ETOOMANY);

    /* Set playback options */
    tonegen->playback_options = options;
    
    /* Copy digits */
    pj_memcpy(tonegen->digits + tonegen->count,
	      tones, count * sizeof(pjmedia_tone_desc));
    
    /* Normalize volume */
    for (i=0; i<count; ++i) {
	pjmedia_tone_desc *t = &tonegen->digits[i+tonegen->count];
	if (t->volume == 0)
	    t->volume = AMP;
	else if (t->volume < 0)
	    t->volume = (short) -t->volume;
    }

    tonegen->count += count;

    return PJ_SUCCESS;
}


/*
 * Play digits.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_play_digits( pjmedia_port *port,
						 unsigned count,
						 const pjmedia_tone_digit digits[],
						 unsigned options)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    pjmedia_tone_desc tones[PJMEDIA_TONEGEN_MAX_DIGITS];
    const pjmedia_tone_digit_map *map = tonegen->digit_map;
    unsigned i;

    PJ_ASSERT_RETURN(port && port->info.signature == SIGNATURE &&
		     count && digits, PJ_EINVAL);
    PJ_ASSERT_RETURN(count < PJMEDIA_TONEGEN_MAX_DIGITS, PJ_ETOOMANY);

    for (i=0; i<count; ++i) {
	int d = pj_tolower(digits[i].digit);
	unsigned j;

	/* Translate ASCII digits with digitmap */
	for (j=0; j<map->count; ++j) {
	    if (d == map->digits[j].digit)
		break;
	}
	if (j == map->count)
	    return PJMEDIA_RTP_EINDTMF;

	tones[i].freq1 = map->digits[j].freq1;
	tones[i].freq2 = map->digits[j].freq2;
	tones[i].on_msec = digits[i].on_msec;
	tones[i].off_msec = digits[i].off_msec;
	tones[i].volume = digits[i].volume;
    }

    return pjmedia_tonegen_play(port, count, tones, options);
}


/*
 * Get the digit-map currently used by this tone generator.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_get_digit_map(pjmedia_port *port,
						  const pjmedia_tone_digit_map **m)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);
    PJ_ASSERT_RETURN(m != NULL, PJ_EINVAL);

    *m = tonegen->digit_map;

    return PJ_SUCCESS;
}


/*
 * Set digit map to be used by the tone generator.
 */
PJ_DEF(pj_status_t) pjmedia_tonegen_set_digit_map(pjmedia_port *port,
						  pjmedia_tone_digit_map *m)
{
    struct tonegen *tonegen = (struct tonegen*) port;
    
    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE, PJ_EINVAL);
    PJ_ASSERT_RETURN(m != NULL, PJ_EINVAL);

    tonegen->digit_map = m;

    return PJ_SUCCESS;
}


