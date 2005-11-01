/* $Id$
 *
 */
/* This file contains file from Sun Microsystems, Inc, with the complete 
 * copyright notice in the second half of this file.
 */
#include <pjmedia/codec.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <string.h>	/* memset */

#define G711_BPS	64000
#define G711_CODEC_CNT	0	/* number of codec to preallocate in memory */

/* These are the only public functions exported to applications */
PJ_DECL(pj_status_t) g711_init_factory (pj_codec_factory *factory, pj_pool_t *pool);
PJ_DECL(pj_status_t) g711_deinit_factory (pj_codec_factory *factory);

/* Algorithm prototypes. */
static unsigned char linear2alaw(int		pcm_val);   /* 2's complement (16-bit range) */
static int	     alaw2linear(unsigned char	a_val);
static unsigned char linear2ulaw(int		pcm_val);
static int	     ulaw2linear(unsigned char	u_val);

/* Prototypes for G711 factory */
static pj_status_t  g711_match_id( pj_codec_factory *factory, const pj_codec_id *id );
static pj_status_t  g711_default_attr( pj_codec_factory *factory, const pj_codec_id *id, pj_codec_attr *attr );
static unsigned	    g711_enum_codecs (pj_codec_factory *factory, unsigned count, pj_codec_id codecs[]);
static pj_codec*    g711_alloc_codec( pj_codec_factory *factory, const pj_codec_id *id);
static void	    g711_dealloc_codec( pj_codec_factory *factory, pj_codec *codec );

/* Prototypes for G711 implementation. */
static pj_status_t  g711_codec_default_attr (pj_codec *codec, pj_codec_attr *attr);
static pj_status_t  g711_init( pj_codec *codec, pj_pool_t *pool );
static pj_status_t  g711_open( pj_codec *codec, pj_codec_attr *attr );
static pj_status_t  g711_close( pj_codec *codec );
static pj_status_t  g711_encode( pj_codec *codec, const struct pj_audio_frame *input,
				 unsigned output_buf_len, struct pj_audio_frame *output);
static pj_status_t  g711_decode( pj_codec *codec, const struct pj_audio_frame *input,
				 unsigned output_buf_len, struct pj_audio_frame *output);

/* Definition for G711 codec operations. */
static pj_codec_op g711_op = 
{
    &g711_codec_default_attr ,
    &g711_init,
    &g711_open,
    &g711_close,
    &g711_encode,
    &g711_decode
};

/* Definition for G711 codec factory operations. */
static pj_codec_factory_op g711_factory_op =
{
    &g711_match_id,
    &g711_default_attr,
    &g711_enum_codecs,
    &g711_alloc_codec,
    &g711_dealloc_codec
};

/* G711 factory private data */
struct g711_factory_private
{
    pj_pool_t  *pool;
    pj_codec	codec_list;
};

/* G711 codec private data. */
struct g711_private
{
    unsigned pt;
};


PJ_DEF(pj_status_t) g711_init_factory (pj_codec_factory *factory, pj_pool_t *pool)
{
    struct g711_factory_private *priv;
    //enum { CODEC_MEM_SIZE = sizeof(pj_codec) + sizeof(struct g711_private) + 4 };

    /* Create pool. */
    /*
    pool = pj_pool_pool_create_pool(pp, "g711ftry", 
					G711_CODEC_CNT*CODEC_MEM_SIZE + 
					sizeof(struct g711_factory_private),
				        CODEC_MEM_SIZE, NULL);
    if (!pool)
	return -1;
    */

    priv = pj_pool_alloc(pool, sizeof(struct g711_factory_private));
    if (!priv)
	return -1;

    factory->factory_data = priv;
    factory->op = &g711_factory_op;

    priv->pool = pool;
    pj_list_init(&priv->codec_list);
    return 0;
}

PJ_DEF(pj_status_t) g711_deinit_factory (pj_codec_factory *factory)
{
    struct g711_factory_private *priv = factory->factory_data;

    /* Invalidate member to help detect errors */
    priv->pool = NULL;
    priv->codec_list.next = priv->codec_list.prev = NULL;
    return 0;
}

static pj_status_t g711_match_id( pj_codec_factory *factory, const pj_codec_id *id )
{
    PJ_UNUSED_ARG(factory)

    /* It's sufficient to check payload type only. */
    return (id->pt==PJ_RTP_PT_PCMU || id->pt==PJ_RTP_PT_PCMA) ? 0 : -1;
}

static pj_status_t g711_default_attr (pj_codec_factory *factory, 
				      const pj_codec_id *id, 
				      pj_codec_attr *attr )
{
    PJ_UNUSED_ARG(factory)

    memset(attr, 0, sizeof(pj_codec_attr));
    attr->sample_rate = 8000;
    attr->avg_bps = G711_BPS;
    attr->pcm_bits_per_sample = 16;
    attr->ptime = 20;
    attr->pt = id->pt;

    /* Default all flag bits disabled. */

    return PJ_SUCCESS;
}

static unsigned	g711_enum_codecs (pj_codec_factory *factory, 
				  unsigned count, pj_codec_id codecs[])
{
    PJ_UNUSED_ARG(factory)

    if (count > 0) {
	codecs[0].type = PJ_MEDIA_TYPE_AUDIO;
	codecs[0].pt = PJ_RTP_PT_PCMU;
	codecs[0].encoding_name = pj_str("PCMU");
	codecs[0].sample_rate = 8000;
    }
    if (count > 1) {
	codecs[1].type = PJ_MEDIA_TYPE_AUDIO;
	codecs[1].pt = PJ_RTP_PT_PCMA;
	codecs[1].encoding_name = pj_str("PCMA");
	codecs[1].sample_rate = 8000;
    }

    return 2;
}

static pj_codec *g711_alloc_codec( pj_codec_factory *factory, const pj_codec_id *id)
{
    struct g711_factory_private *priv = factory->factory_data;
    pj_codec *codec = NULL;

    /* Allocate new codec if no more is available */
    if (pj_list_empty(&priv->codec_list)) {
	struct g711_private *codec_priv;

	codec = pj_pool_alloc(priv->pool, sizeof(pj_codec));
	codec_priv = pj_pool_alloc(priv->pool, sizeof(struct g711_private));
	if (!codec || !codec_priv)
	    return NULL;

	codec_priv->pt = id->pt;

	codec->factory = factory;
	codec->op = &g711_op;
	codec->codec_data = codec_priv;
    } else {
	codec = priv->codec_list.next;
	pj_list_erase(codec);
    }

    /* Zero the list, for error detection in g711_dealloc_codec */
    codec->next = codec->prev = NULL;

    return codec;
}

static void g711_dealloc_codec( pj_codec_factory *factory, pj_codec *codec )
{
    struct g711_factory_private *priv = factory->factory_data;

    /* Check that this node has not been deallocated before */
    pj_assert (codec->next==NULL && codec->prev==NULL);
    if (codec->next!=NULL || codec->prev!=NULL) {
	return;
    }

    /* Insert at the back of the list */
    pj_list_insert_before(&priv->codec_list, codec);
}

static pj_status_t g711_codec_default_attr  (pj_codec *codec, pj_codec_attr *attr)
{
    struct g711_private *priv = codec->codec_data;
    pj_codec_id id;

    id.pt = priv->pt;
    return g711_default_attr (NULL, &id, attr);
}

static pj_status_t g711_init( pj_codec *codec, pj_pool_t *pool )
{
    /* There's nothing to do here really */
    PJ_UNUSED_ARG(codec)
    PJ_UNUSED_ARG(pool)

    return PJ_SUCCESS;
}

static pj_status_t g711_open( pj_codec *codec, pj_codec_attr *attr )
{
    struct g711_private *priv = codec->codec_data;
    priv->pt = attr->pt;
    return PJ_SUCCESS;
}

static pj_status_t g711_close( pj_codec *codec )
{
    PJ_UNUSED_ARG(codec);
    /* Nothing to do */
    return PJ_SUCCESS;
}

static pj_status_t  g711_encode( pj_codec *codec, const struct pj_audio_frame *input,
				 unsigned output_buf_len, struct pj_audio_frame *output)
{
    pj_int16_t *samples = (pj_int16_t*) input->buf;
    struct g711_private *priv = codec->codec_data;

    /* Check output buffer length */
    if (output_buf_len < input->size / 2)
	return -1;

    /* Encode */
    if (priv->pt == PJ_RTP_PT_PCMA) {
	unsigned i;
	pj_uint8_t *dst = output->buf;

	for (i=0; i!=input->size/2; ++i, ++dst) {
	    *dst = linear2alaw(samples[i]);
	}
    } else if (priv->pt == PJ_RTP_PT_PCMU) {
	unsigned i;
	pj_uint8_t *dst = output->buf;

	for (i=0; i!=input->size/2; ++i, ++dst) {
	    *dst = linear2ulaw(samples[i]);
	}

    } else {
	return -1;
    }

    output->type = PJ_AUDIO_FRAME_AUDIO;
    output->size = input->size / 2;

    return 0;
}

static pj_status_t  g711_decode( pj_codec *codec, const struct pj_audio_frame *input,
				 unsigned output_buf_len, struct pj_audio_frame *output)
{
    struct g711_private *priv = codec->codec_data;

    /* Check output buffer length */
    if (output_buf_len < input->size * 2)
	return -1;

    /* Decode */
    if (priv->pt == PJ_RTP_PT_PCMA) {
	unsigned i;
	pj_uint8_t *src = input->buf;
	pj_uint16_t *dst = output->buf;

	for (i=0; i!=input->size; ++i) {
	    *dst++ = (pj_uint16_t) alaw2linear(*src++);
	}
    } else if (priv->pt == PJ_RTP_PT_PCMU) {
	unsigned i;
	pj_uint8_t *src = input->buf;
	pj_uint16_t *dst = output->buf;

	for (i=0; i!=input->size; ++i) {
	    *dst++ = (pj_uint16_t) ulaw2linear(*src++);
	}

    } else {
	return -1;
    }

    output->type = PJ_AUDIO_FRAME_AUDIO;
    output->size = input->size * 2;

    return 0;
}


/*
 * This source code is a product of Sun Microsystems, Inc. and is provided
 * for unrestricted use.  Users may copy or modify this source code without
 * charge.
 *
 * SUN SOURCE CODE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
 * THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun source code is provided with no support and without any obligation on
 * the part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS SOFTWARE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */



#ifdef _MSC_VER
#  pragma warning ( disable: 4244 ) /* Conversion from int to char etc */
#endif

/*
 * g711.c
 *
 * u-law, A-law and linear PCM conversions.
 */
#define	SIGN_BIT	(0x80)		/* Sign bit for a A-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of A-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

static short seg_end[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF,
			    0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};

/* copy from CCITT G.711 specifications */
static unsigned char _u2a[128] = {		/* u- to A-law conversions */
	1,	1,	2,	2,	3,	3,	4,	4,
	5,	5,	6,	6,	7,	7,	8,	8,
	9,	10,	11,	12,	13,	14,	15,	16,
	17,	18,	19,	20,	21,	22,	23,	24,
	25,	27,	29,	31,	33,	34,	35,	36,
	37,	38,	39,	40,	41,	42,	43,	44,
	46,	48,	49,	50,	51,	52,	53,	54,
	55,	56,	57,	58,	59,	60,	61,	62,
	64,	65,	66,	67,	68,	69,	70,	71,
	72,	73,	74,	75,	76,	77,	78,	79,
	81,	82,	83,	84,	85,	86,	87,	88,
	89,	90,	91,	92,	93,	94,	95,	96,
	97,	98,	99,	100,	101,	102,	103,	104,
	105,	106,	107,	108,	109,	110,	111,	112,
	113,	114,	115,	116,	117,	118,	119,	120,
	121,	122,	123,	124,	125,	126,	127,	128};

static unsigned char _a2u[128] = {		/* A- to u-law conversions */
	1,	3,	5,	7,	9,	11,	13,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	32,	33,	33,	34,	34,	35,	35,
	36,	37,	38,	39,	40,	41,	42,	43,
	44,	45,	46,	47,	48,	48,	49,	49,
	50,	51,	52,	53,	54,	55,	56,	57,
	58,	59,	60,	61,	62,	63,	64,	64,
	65,	66,	67,	68,	69,	70,	71,	72,
	73,	74,	75,	76,	77,	78,	79,	79,
	80,	81,	82,	83,	84,	85,	86,	87,
	88,	89,	90,	91,	92,	93,	94,	95,
	96,	97,	98,	99,	100,	101,	102,	103,
	104,	105,	106,	107,	108,	109,	110,	111,
	112,	113,	114,	115,	116,	117,	118,	119,
	120,	121,	122,	123,	124,	125,	126,	127};

static int
search(
	int		val,
	short		*table,
	int		size)
{
	int		i;

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return (size);
}

/*
 * linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * linear2alaw() accepts an 16-bit integer and encodes it as A-law data.
 *
 *		Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	0000000wxyza			000wxyz
 *	0000001wxyza			001wxyz
 *	000001wxyzab			010wxyz
 *	00001wxyzabc			011wxyz
 *	0001wxyzabcd			100wxyz
 *	001wxyzabcde			101wxyz
 *	01wxyzabcdef			110wxyz
 *	1wxyzabcdefg			111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static unsigned char
linear2alaw(
	int		pcm_val)	/* 2's complement (16-bit range) */
{
	int		mask;
	int		seg;
	unsigned char	aval;

	if (pcm_val >= 0) {
		mask = 0xD5;		/* sign (7th) bit = 1 */
	} else {
		mask = 0x55;		/* sign bit = 0 */
		pcm_val = -pcm_val - 8;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, seg_end, 8);

	/* Combine the sign, segment, and quantization bits. */

	if (seg >= 8)		/* out of range, return maximum value. */
		return (0x7F ^ mask);
	else {
		aval = seg << SEG_SHIFT;
		if (seg < 2)
			aval |= (pcm_val >> 4) & QUANT_MASK;
		else
			aval |= (pcm_val >> (seg + 3)) & QUANT_MASK;
		return (aval ^ mask);
	}
}

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
static int
alaw2linear(
	unsigned char	a_val)
{
	int		t;
	int		seg;

	a_val ^= 0x55;

	t = (a_val & QUANT_MASK) << 4;
	seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
	switch (seg) {
	case 0:
		t += 8;
		break;
	case 1:
		t += 0x108;
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
	}
	return ((a_val & SIGN_BIT) ? t : -t);
}

#define	BIAS		(0x84)		/* Bias for linear code. */

/*
 * linear2ulaw() - Convert a linear PCM value to u-law
 *
 * In order to simplify the encoding process, the original linear magnitude
 * is biased by adding 33 which shifts the encoding range from (0 - 8158) to
 * (33 - 8191). The result can be seen in the following encoding table:
 *
 *	Biased Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	00000001wxyza			000wxyz
 *	0000001wxyzab			001wxyz
 *	000001wxyzabc			010wxyz
 *	00001wxyzabcd			011wxyz
 *	0001wxyzabcde			100wxyz
 *	001wxyzabcdef			101wxyz
 *	01wxyzabcdefg			110wxyz
 *	1wxyzabcdefgh			111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static unsigned char
linear2ulaw(
	int		pcm_val)	/* 2's complement (16-bit range) */
{
	int		mask;
	int		seg;
	unsigned char	uval;

	/* Get the sign and the magnitude of the value. */
	if (pcm_val < 0) {
		pcm_val = BIAS - pcm_val;
		mask = 0x7F;
	} else {
		pcm_val += BIAS;
		mask = 0xFF;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, seg_end, 8);

	/*
	 * Combine the sign, segment, quantization bits;
	 * and complement the code word.
	 */
	if (seg >= 8)		/* out of range, return maximum value. */
		return (0x7F ^ mask);
	else {
		uval = (seg << 4) | ((pcm_val >> (seg + 3)) & 0xF);
		return (uval ^ mask);
	}

}

/*
 * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
 *
 * First, a biased linear code is derived from the code word. An unbiased
 * output can then be obtained by subtracting 33 from the biased code.
 *
 * Note that this function expects to be passed the complement of the
 * original code word. This is in keeping with ISDN conventions.
 */
static int
ulaw2linear(
	unsigned char	u_val)
{
	int		t;

	/* Complement to obtain normal u-law value. */
	u_val = ~u_val;

	/*
	 * Extract and bias the quantization bits. Then
	 * shift up by the segment number and subtract out the bias.
	 */
	t = ((u_val & QUANT_MASK) << 3) + BIAS;
	t <<= ((unsigned)u_val & SEG_MASK) >> SEG_SHIFT;

	return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

/* A-law to u-law conversion */
unsigned char
alaw2ulaw(
	unsigned char	aval)
{
	aval &= 0xff;
	return ((aval & 0x80) ? (0xFF ^ _a2u[aval ^ 0xD5]) :
	    (0x7F ^ _a2u[aval ^ 0x55]));
}

/* u-law to A-law conversion */
unsigned char
ulaw2alaw(
	unsigned char	uval)
{
	uval &= 0xff;
	return ((uval & 0x80) ? (0xD5 ^ (_u2a[0xFF ^ uval] - 1)) :
	    (0x55 ^ (_u2a[0x7F ^ uval] - 1)));
}



