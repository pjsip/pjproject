/* $Id$
 *
 */
#include <pjsip_simple/pidf.h>
#include <pj/string.h>
#include <pj/pool.h>

struct pjpidf_op_desc pjpidf_op = 
{
    {
	&pjpidf_pres_construct,
	&pjpidf_pres_add_tuple,
	&pjpidf_pres_get_first_tuple,
	&pjpidf_pres_get_next_tuple,
	&pjpidf_pres_find_tuple,
	&pjpidf_pres_remove_tuple,
	&pjpidf_pres_add_note,
	&pjpidf_pres_get_first_note,
	&pjpidf_pres_get_next_note
    },
    {
	&pjpidf_tuple_construct,
	&pjpidf_tuple_get_id,
	&pjpidf_tuple_set_id,
	&pjpidf_tuple_get_status,
	&pjpidf_tuple_get_contact,
	&pjpidf_tuple_set_contact,
	&pjpidf_tuple_set_contact_prio,
	&pjpidf_tuple_get_contact_prio,
	&pjpidf_tuple_add_note,
	&pjpidf_tuple_get_first_note,
	&pjpidf_tuple_get_next_note,
	&pjpidf_tuple_get_timestamp,
	&pjpidf_tuple_set_timestamp,
	&pjpidf_tuple_set_timestamp_np
    },
    {
	&pjpidf_status_construct,
	&pjpidf_status_is_basic_open,
	&pjpidf_status_set_basic_open
    }
};

static pj_str_t PRESENCE = { "presence", 8 };
static pj_str_t ENTITY = { "entity", 6};
static pj_str_t	TUPLE = { "tuple", 5 };
static pj_str_t ID = { "id", 2 };
static pj_str_t NOTE = { "note", 4 };
static pj_str_t STATUS = { "status", 6 };
static pj_str_t CONTACT = { "contact", 7 };
static pj_str_t PRIORITY = { "priority", 8 };
static pj_str_t TIMESTAMP = { "timestamp", 9 };
static pj_str_t BASIC = { "basic", 5 };
static pj_str_t OPEN = { "open", 4 };
static pj_str_t CLOSED = { "closed", 6 };
static pj_str_t EMPTY_STRING = { NULL, 0 };

static void xml_init_node(pj_pool_t *pool, pj_xml_node *node,
			  pj_str_t *name, const pj_str_t *value)
{
    pj_list_init(&node->attr_head);
    pj_list_init(&node->node_head);
    node->name = *name;
    if (value) pj_strdup(pool, &node->content, value);
    else node->content.ptr=NULL, node->content.slen=0;
}

static pj_xml_attr* xml_create_attr(pj_pool_t *pool, pj_str_t *name,
				    const pj_str_t *value)
{
    pj_xml_attr *attr = pj_pool_alloc(pool, sizeof(*attr));
    attr->name = *name;
    pj_strdup(pool, &attr->value, value);
    return attr;
}

/* Presence */
PJ_DEF(void) pjpidf_pres_construct(pj_pool_t *pool, pjpidf_pres *pres,
				   const pj_str_t *entity)
{
    pj_xml_attr *attr;

    xml_init_node(pool, pres, &PRESENCE, NULL);
    attr = xml_create_attr(pool, &ENTITY, entity);
    pj_xml_add_attr(pres, attr);
}

PJ_DEF(pjpidf_tuple*) pjpidf_pres_add_tuple(pj_pool_t *pool, pjpidf_pres *pres,
					    const pj_str_t *id)
{
    pjpidf_tuple *t = pj_pool_alloc(pool, sizeof(*t));
    pjpidf_tuple_construct(pool, t, id);
    pj_xml_add_node(pres, t);
    return t;
}

PJ_DEF(pjpidf_tuple*) pjpidf_pres_get_first_tuple(pjpidf_pres *pres)
{
    return pj_xml_find_node(pres, &TUPLE);
}

PJ_DEF(pjpidf_tuple*) pjpidf_pres_get_next_tuple(pjpidf_pres *pres, 
						 pjpidf_tuple *tuple)
{
    return pj_xml_find_next_node(pres, tuple, &TUPLE);
}

static pj_bool_t find_tuple_by_id(pj_xml_node *node, const void *id)
{
    return pj_xml_find_attr(node, &ID, id) != NULL;
}

PJ_DEF(pjpidf_tuple*) pjpidf_pres_find_tuple(pjpidf_pres *pres, const pj_str_t *id)
{
    return pj_xml_find(pres, &TUPLE, id, &find_tuple_by_id);
}

PJ_DEF(void) pjpidf_pres_remove_tuple(pjpidf_pres *pres, pjpidf_tuple *t)
{
    PJ_UNUSED_ARG(pres)
    pj_list_erase(t);
}

PJ_DEF(pjpidf_note*) pjpidf_pres_add_note(pj_pool_t *pool, pjpidf_pres *pres, 
					  const pj_str_t *text)
{
    pjpidf_note *note = pj_pool_alloc(pool, sizeof(*note));
    xml_init_node(pool, note, &NOTE, text);
    pj_xml_add_node(pres, note);
    return note;
}

PJ_DEF(pjpidf_note*) pjpidf_pres_get_first_note(pjpidf_pres *pres)
{
    return pj_xml_find_node( pres, &NOTE);
}

PJ_DEF(pjpidf_note*) pjpidf_pres_get_next_note(pjpidf_pres *t, pjpidf_note *note)
{
    return pj_xml_find_next_node(t, note, &NOTE);
}


/* Tuple */
PJ_DEF(void) pjpidf_tuple_construct(pj_pool_t *pool, pjpidf_tuple *t,
				    const pj_str_t *id)
{
    pj_xml_attr *attr;
    pjpidf_status *st;

    xml_init_node(pool, t, &TUPLE, NULL);
    attr = xml_create_attr(pool, &ID, id);
    pj_xml_add_attr(t, attr);
    st = pj_pool_alloc(pool, sizeof(*st));
    pjpidf_status_construct(pool, st);
    pj_xml_add_node(t, st);
}

PJ_DEF(const pj_str_t*) pjpidf_tuple_get_id(const pjpidf_tuple *t)
{
    const pj_xml_attr *attr = pj_xml_find_attr((pj_xml_node*)t, &ID, NULL);
    pj_assert(attr);
    return &attr->value;
}

PJ_DEF(void) pjpidf_tuple_set_id(pj_pool_t *pool, pjpidf_tuple *t, const pj_str_t *id)
{
    pj_xml_attr *attr = pj_xml_find_attr(t, &ID, NULL);
    pj_assert(attr);
    pj_strdup(pool, &attr->value, id);
}


PJ_DEF(pjpidf_status*) pjpidf_tuple_get_status(pjpidf_tuple *t)
{
    pjpidf_status *st = (pjpidf_status*)pj_xml_find_node(t, &STATUS);
    pj_assert(st);
    return st;
}


PJ_DEF(const pj_str_t*) pjpidf_tuple_get_contact(const pjpidf_tuple *t)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)t, &CONTACT);
    if (!node)
	return &EMPTY_STRING;
    return &node->content;
}

PJ_DEF(void) pjpidf_tuple_set_contact(pj_pool_t *pool, pjpidf_tuple *t, 
				      const pj_str_t *contact)
{
    pj_xml_node *node = pj_xml_find_node(t, &CONTACT);
    if (!node) {
	node = pj_pool_alloc(pool, sizeof(*node));
	xml_init_node(pool, node, &CONTACT, contact);
	pj_xml_add_node(t, node);
    } else {
	pj_strdup(pool, &node->content, contact);
    }
}

PJ_DEF(void) pjpidf_tuple_set_contact_prio(pj_pool_t *pool, pjpidf_tuple *t, 
					   const pj_str_t *prio)
{
    pj_xml_node *node = pj_xml_find_node(t, &CONTACT);
    pj_xml_attr *attr;

    if (!node) {
	node = pj_pool_alloc(pool, sizeof(*node));
	xml_init_node(pool, node, &CONTACT, NULL);
	pj_xml_add_node(t, node);
    }
    attr = pj_xml_find_attr(node, &PRIORITY, NULL);
    if (!attr) {
	attr = xml_create_attr(pool, &PRIORITY, prio);
	pj_xml_add_attr(node, attr);
    } else {
	pj_strdup(pool, &attr->value, prio);
    }
}

PJ_DEF(const pj_str_t*) pjpidf_tuple_get_contact_prio(const pjpidf_tuple *t)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)t, &CONTACT);
    pj_xml_attr *attr;

    if (!node)
	return &EMPTY_STRING;
    attr = pj_xml_find_attr(node, &PRIORITY, NULL);
    if (!attr)
	return &EMPTY_STRING;
    return &attr->value;
}


PJ_DEF(pjpidf_note*) pjpidf_tuple_add_note(pj_pool_t *pool, pjpidf_tuple *t,
					   const pj_str_t *text)
{
    pjpidf_note *note = pj_pool_alloc(pool, sizeof(*note));
    xml_init_node(pool, note, &NOTE, text);
    pj_xml_add_node(t, note);
    return note;
}

PJ_DEF(pjpidf_note*) pjpidf_tuple_get_first_note(pjpidf_tuple *t)
{
    return pj_xml_find_node(t, &NOTE);
}

PJ_DEF(pjpidf_note*) pjpidf_tuple_get_next_note(pjpidf_tuple *t, pjpidf_note *n)
{
    return pj_xml_find_next_node(t, n, &NOTE);
}


PJ_DEF(const pj_str_t*) pjpidf_tuple_get_timestamp(const pjpidf_tuple *t)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)t, &TIMESTAMP);
    return node ? &node->content : &EMPTY_STRING;
}

PJ_DEF(void) pjpidf_tuple_set_timestamp(pj_pool_t *pool, pjpidf_tuple *t,
					const pj_str_t *ts)
{
    pj_xml_node *node = pj_xml_find_node(t, &TIMESTAMP);
    if (!node) {
	node = pj_pool_alloc(pool, sizeof(*node));
	xml_init_node(pool, node, &TIMESTAMP, ts);
    } else {
	pj_strdup(pool, &node->content, ts);
    }
}


PJ_DEF(void) pjpidf_tuple_set_timestamp_np(pj_pool_t *pool, pjpidf_tuple *t, 
					   pj_str_t *ts)
{
    pj_xml_node *node = pj_xml_find_node(t, &TIMESTAMP);
    if (!node) {
	node = pj_pool_alloc(pool, sizeof(*node));
	xml_init_node(pool, node, &TIMESTAMP, ts);
    } else {
	node->content = *ts;
    }
}


/* Status */
PJ_DEF(void) pjpidf_status_construct(pj_pool_t *pool, pjpidf_status *st)
{
    pj_xml_node *node;

    xml_init_node(pool, st, &STATUS, NULL);
    node = pj_pool_alloc(pool, sizeof(*node));
    xml_init_node(pool, node, &BASIC, &CLOSED);
    pj_xml_add_node(st, node);
}

PJ_DEF(pj_bool_t) pjpidf_status_is_basic_open(const pjpidf_status *st)
{
    pj_xml_node *node = pj_xml_find_node((pj_xml_node*)st, &BASIC);
    pj_assert(node != NULL);
    return pj_stricmp(&node->content, &OPEN)==0;
}

PJ_DEF(void) pjpidf_status_set_basic_open(pjpidf_status *st, pj_bool_t open)
{
    pj_xml_node *node = pj_xml_find_node(st, &BASIC);
    pj_assert(node != NULL);
    node->content = open ? OPEN : CLOSED;
}

PJ_DEF(pjpidf_pres*) pjpidf_create(pj_pool_t *pool, const pj_str_t *entity)
{
    pjpidf_pres *pres = pj_pool_alloc(pool, sizeof(*pres));
    pjpidf_pres_construct(pool, pres, entity);
    return pres;
}

PJ_DEF(pjpidf_pres*) pjpidf_parse(pj_pool_t *pool, char *text, int len)
{
    pjpidf_pres *pres = pj_xml_parse(pool, text, len);
    if (pres) {
	if (pj_stricmp(&pres->name, &PRESENCE) != 0)
	    return NULL;
    }
    return pres;
}

PJ_DEF(int) pjpidf_print(const pjpidf_pres* pres, char *buf, int len)
{
    return pj_xml_print(pres, buf, len, PJ_TRUE);
}

