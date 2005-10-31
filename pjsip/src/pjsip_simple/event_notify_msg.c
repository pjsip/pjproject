/* $Header: /pjproject/pjsip/src/pjsip_simple/event_notify_msg.c 2     6/21/05 12:37a Bennylp $ */
#include <pjsip_simple/event_notify_msg.h>
#include <pjsip/print.h>
#include <pjsip/sip_parser.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/except.h>

static int pjsip_event_hdr_print( pjsip_event_hdr *hdr, 
				  char *buf, pj_size_t size);
static pjsip_event_hdr* pjsip_event_hdr_clone( pj_pool_t *pool, 
					       const pjsip_event_hdr *hdr);
static pjsip_event_hdr* pjsip_event_hdr_shallow_clone( pj_pool_t *pool,
						       const pjsip_event_hdr*);

static pjsip_hdr_vptr event_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &pjsip_event_hdr_clone,
    (pjsip_hdr_clone_fptr) &pjsip_event_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &pjsip_event_hdr_print,
};


PJ_DEF(pjsip_event_hdr*) pjsip_event_hdr_create(pj_pool_t *pool)
{
    pj_str_t event = { "Event", 5 };
    pjsip_event_hdr *hdr = pj_pool_calloc(pool, 1, sizeof(*hdr));
    hdr->type = PJSIP_H_OTHER;
    hdr->name = hdr->sname = event;
    hdr->vptr = &event_hdr_vptr;
    pj_list_init(hdr);
    return hdr;
}

static int pjsip_event_hdr_print( pjsip_event_hdr *hdr, 
				  char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    int printed;

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    copy_advance(p, hdr->event_type);
    copy_advance_pair(p, ";id=", 4, hdr->id_param);
    if (hdr->other_param.slen)
	copy_advance(p, hdr->other_param);
    return p - buf;
}

static pjsip_event_hdr* pjsip_event_hdr_clone( pj_pool_t *pool, 
					       const pjsip_event_hdr *rhs)
{
    pjsip_event_hdr *hdr = pjsip_event_hdr_create(pool);
    pj_strdup(pool, &hdr->event_type, &rhs->event_type);
    pj_strdup(pool, &hdr->id_param, &rhs->id_param);
    pj_strdup(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}

static pjsip_event_hdr* pjsip_event_hdr_shallow_clone( pj_pool_t *pool,
						       const pjsip_event_hdr *rhs )
{
    pjsip_event_hdr *hdr = pj_pool_alloc(pool, sizeof(*hdr));
    pj_memcpy(hdr, rhs, sizeof(*hdr));
    return hdr;
}


static int pjsip_allow_events_hdr_print(pjsip_allow_events_hdr *hdr, 
					char *buf, pj_size_t size);
static pjsip_allow_events_hdr* 
pjsip_allow_events_hdr_clone(pj_pool_t *pool, 
			     const pjsip_allow_events_hdr *hdr);
static pjsip_allow_events_hdr* 
pjsip_allow_events_hdr_shallow_clone(pj_pool_t *pool,
				     const pjsip_allow_events_hdr*);

static pjsip_hdr_vptr allow_event_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &pjsip_allow_events_hdr_clone,
    (pjsip_hdr_clone_fptr) &pjsip_allow_events_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &pjsip_allow_events_hdr_print,
};


PJ_DEF(pjsip_allow_events_hdr*) pjsip_allow_events_hdr_create(pj_pool_t *pool)
{
    pj_str_t allow_events = { "Allow-Events", 12 };
    pjsip_allow_events_hdr *hdr = pj_pool_calloc(pool, 1, sizeof(*hdr));
    hdr->type = PJSIP_H_OTHER;
    hdr->name = hdr->sname = allow_events;
    hdr->vptr = &allow_event_hdr_vptr;
    pj_list_init(hdr);
    return hdr;
}

static int pjsip_allow_events_hdr_print(pjsip_allow_events_hdr *hdr, 
					char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    int printed;

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    if (hdr->event_cnt > 0) {
	int i;
	copy_advance(p, hdr->events[0]);
	for (i=1; i<hdr->event_cnt; ++i) {
	    copy_advance_pair(p, ",", 1, hdr->events[i]);
	}
    }

    return p - buf;
}

static pjsip_allow_events_hdr* 
pjsip_allow_events_hdr_clone(pj_pool_t *pool, 
			     const pjsip_allow_events_hdr *rhs)
{
    int i;

    pjsip_allow_events_hdr *hdr = pjsip_allow_events_hdr_create(pool);
    hdr->event_cnt = rhs->event_cnt;
    for (i=0; i<rhs->event_cnt; ++i) {
	pj_strdup(pool, &hdr->events[i], &rhs->events[i]);
    }
    return hdr;
}

static pjsip_allow_events_hdr* 
pjsip_allow_events_hdr_shallow_clone(pj_pool_t *pool,
				     const pjsip_allow_events_hdr *rhs)
{
    pjsip_allow_events_hdr *hdr = pj_pool_alloc(pool, sizeof(*hdr));
    pj_memcpy(hdr, rhs, sizeof(*hdr));
    return hdr;
}


static int pjsip_sub_state_hdr_print(pjsip_sub_state_hdr *hdr, 
				     char *buf, pj_size_t size);
static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_clone(pj_pool_t *pool, 
			  const pjsip_sub_state_hdr *hdr);
static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_shallow_clone(pj_pool_t *pool,
				  const pjsip_sub_state_hdr*);

static pjsip_hdr_vptr sub_state_hdr_vptr = 
{
    (pjsip_hdr_clone_fptr) &pjsip_sub_state_hdr_clone,
    (pjsip_hdr_clone_fptr) &pjsip_sub_state_hdr_shallow_clone,
    (pjsip_hdr_print_fptr) &pjsip_sub_state_hdr_print,
};


PJ_DEF(pjsip_sub_state_hdr*) pjsip_sub_state_hdr_create(pj_pool_t *pool)
{
    pj_str_t sub_state = { "Subscription-State", 18 };
    pjsip_sub_state_hdr *hdr = pj_pool_calloc(pool, 1, sizeof(*hdr));
    hdr->type = PJSIP_H_OTHER;
    hdr->name = hdr->sname = sub_state;
    hdr->vptr = &sub_state_hdr_vptr;
    hdr->expires_param = -1;
    hdr->retry_after = -1;
    pj_list_init(hdr);
    return hdr;
}

static int pjsip_sub_state_hdr_print(pjsip_sub_state_hdr *hdr, 
				     char *buf, pj_size_t size)
{
    char *p = buf;
    char *endbuf = buf+size;
    int printed;

    copy_advance(p, hdr->name);
    *p++ = ':';
    *p++ = ' ';

    copy_advance(p, hdr->sub_state);
    copy_advance_pair(p, ";reason=", 8, hdr->reason_param);
    if (hdr->expires_param >= 0) {
	pj_memcpy(p, ";expires=", 9);
	p += 9;
	printed = pj_utoa(hdr->expires_param, p);
	p += printed;
    }
    if (hdr->retry_after >= 0) {
	pj_memcpy(p, ";retry-after=", 13);
	p += 9;
	printed = pj_utoa(hdr->retry_after, p);
	p += printed;
    }
    if (hdr->other_param.slen)
	copy_advance(p, hdr->other_param);

    return p - buf;
}

static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_clone(pj_pool_t *pool, 
			  const pjsip_sub_state_hdr *rhs)
{
    pjsip_sub_state_hdr *hdr = pjsip_sub_state_hdr_create(pool);
    pj_strdup(pool, &hdr->sub_state, &rhs->sub_state);
    pj_strdup(pool, &hdr->reason_param, &rhs->reason_param);
    hdr->retry_after = rhs->retry_after;
    hdr->expires_param = rhs->expires_param;
    pj_strdup(pool, &hdr->other_param, &rhs->other_param);
    return hdr;
}

static pjsip_sub_state_hdr* 
pjsip_sub_state_hdr_shallow_clone(pj_pool_t *pool,
				  const pjsip_sub_state_hdr *rhs)
{
    pjsip_sub_state_hdr *hdr = pj_pool_alloc(pool, sizeof(*hdr));
    pj_memcpy(hdr, rhs, sizeof(*hdr));
    return hdr;
}

static pjsip_event_hdr *parse_hdr_event(pj_scanner *scanner, 
					pj_pool_t *pool)
{
    pjsip_event_hdr *hdr = pjsip_event_hdr_create(pool);
    const pj_str_t id_param = { "id", 2 };

    pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->event_type);

    while (*scanner->current == ';') {
	pj_str_t pname, pvalue;
	pj_scan_get_char(scanner);
	pjsip_parse_param_imp(scanner, &pname, &pvalue, 0);
	if (pj_stricmp(&pname, &id_param)==0) {
	    hdr->id_param = pvalue;
	} else {
	    pjsip_concat_param_imp(&hdr->other_param, pool, &pname, &pvalue, ';');
	}
    }
    pjsip_parse_end_hdr_imp( scanner );
    return hdr;
}

static pjsip_allow_events_hdr *parse_hdr_allow_events(pj_scanner *scanner, 
						      pj_pool_t *pool)
{
    pjsip_allow_events_hdr *hdr = pjsip_allow_events_hdr_create(pool);

    pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->events[0]);
    hdr->event_cnt = 1;

    while (*scanner->current == ',') {
	pj_scan_get_char(scanner);
	pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->events[hdr->event_cnt++]);
	if (hdr->event_cnt == PJSIP_MAX_ALLOW_EVENTS) {
	    PJ_THROW(PJSIP_SYN_ERR_EXCEPTION);
	}
    }

    pjsip_parse_end_hdr_imp( scanner );
    return hdr;
}

static pjsip_sub_state_hdr *parse_hdr_sub_state(pj_scanner *scanner, 
						pj_pool_t *pool)
{
    pjsip_sub_state_hdr *hdr = pjsip_sub_state_hdr_create(pool);
    const pj_str_t reason = { "reason", 6 },
		   expires = { "expires", 7 },
		   retry_after = { "retry-after", 11 };
    pj_scan_get(scanner, pjsip_TOKEN_SPEC, &hdr->sub_state);

    while (*scanner->current == ';') {
	pj_str_t pname, pvalue;

	pj_scan_get_char(scanner);
	pjsip_parse_param_imp(scanner, &pname, &pvalue, 0);
	if (pj_stricmp(&pname, &reason) == 0) {
	    hdr->reason_param = pvalue;
	} else if (pj_stricmp(&pname, &expires) == 0) {
	    hdr->expires_param = pj_strtoul(&pvalue);
	} else if (pj_stricmp(&pname, &retry_after) == 0) {
	    hdr->retry_after = pj_strtoul(&pvalue);
	} else {
	    pjsip_concat_param_imp(&hdr->other_param, pool, &pname, &pvalue, ';');
	}
    }

    pjsip_parse_end_hdr_imp( scanner );
    return hdr;
}

PJ_DEF(void) pjsip_event_notify_init_parser(void)
{
    pjsip_register_hdr_parser( "Event", NULL, (pjsip_parse_hdr_func*) &parse_hdr_event);
    pjsip_register_hdr_parser( "Allow-Events", NULL, (pjsip_parse_hdr_func*) &parse_hdr_allow_events);
    pjsip_register_hdr_parser( "Subscription-State", NULL, (pjsip_parse_hdr_func*) &parse_hdr_sub_state);
}
