/* $Id$
 */
#ifndef __PJSIP_PRINT_H__
#define __PJSIP_PRINT_H__

#define copy_advance_check(buf,str)   \
	do { \
	    if ((str).slen+10 >= (endbuf-buf)) return -1;	\
	    pj_memcpy(buf, (str).ptr, (str).slen); \
	    buf += (str).slen; \
	} while (0)

/*
static char *imp_copy_advance_pair(char *buf, char *endbuf, const char *str1, int len1, const pj_str_t *str2)
{
    if (str2->slen) {
	int printed = len1+str2->slen;
	if (printed+10 >= (endbuf-buf)) return NULL;
	pj_memcpy(buf,str1,len1);
	pj_memcpy(buf+len1, str2->ptr, str2->slen);
	return buf + printed;
    } else
	return buf;
}
*/

#define copy_advance_pair_check(buf,str1,len1,str2)   \
	do { \
	    if (str2.slen) { \
		printed = len1+str2.slen; \
		if (printed+10 >= (endbuf-buf)) return -1;	\
		pj_memcpy(buf,str1,len1); \
		pj_memcpy(buf+len1, str2.ptr, str2.slen); \
		buf += printed; \
	    } \
	} while (0)
/*
#define copy_advance_pair(buf,str1,len1,str2)   \
	do { \
	    buf = imp_copy_advance_pair(buf, endbuf, str1, len1, &str2); \
	    if (buf == NULL) return -1; \
	} while (0)
*/

#define copy_advance_pair_quote_check(buf,str1,len1,str2,quotebegin,quoteend) \
	do { \
	    if (str2.slen) { \
		printed = len1+str2.slen+2; \
		if (printed+10 >= (endbuf-buf)) return -1;	\
		pj_memcpy(buf,str1,len1); \
		*(buf+len1)=quotebegin; \
		pj_memcpy(buf+len1+1, str2.ptr, str2.slen); \
		*(buf+printed-1) = quoteend; \
		buf += printed; \
	    } \
	} while (0)

#define copy_advance_no_check(buf,str)   \
	pj_memcpy(buf, (str).ptr, (str).slen); \
	buf += (str).slen;

#define copy_advance_pair_no_check(buf,str1,len1,str2)   \
	if (str2.slen) { \
	    pj_memcpy(buf,str1,len1); \
	    pj_memcpy(buf+len1, str2.ptr, str2.slen); \
	    buf += len1+str2.slen; \
	}

#define copy_advance 		copy_advance_check
#define copy_advance_pair 	copy_advance_pair_check
#define copy_advance_pair_quote	copy_advance_pair_quote_check

#define copy_advance_pair_quote_cond(buf,str1,len1,str2,quotebegin,quoteend) \
	do {	\
	    if (str2.slen && *str2.ptr!=quotebegin) \
		copy_advance_pair_quote(buf,str1,len1,str2,quotebegin,quoteend); \
	    else \
		copy_advance_pair(buf,str1,len1,str2); \
	} while (0)

/*
 * Internal type declarations.
 */
typedef void* (*pjsip_hdr_clone_fptr)(pj_pool_t *, const void*);
typedef int   (*pjsip_hdr_print_fptr)(void *hdr, char *buf, pj_size_t len);

extern const pj_str_t pjsip_hdr_names[];

PJ_INLINE(void) init_hdr(void *hptr, pjsip_hdr_e htype, void *vptr)
{
    pjsip_hdr *hdr = hptr;
    hdr->type = htype;
    hdr->name = hdr->sname = pjsip_hdr_names[htype];
    hdr->vptr = vptr;
    pj_list_init(hdr);
}

#endif	/* __PJSIP_PRINT_H__ */

