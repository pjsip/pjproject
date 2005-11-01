/* $Id$
 *
 */
#include <pjsip_simple/xpidf.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/guid.h>

static pj_str_t PRESENCE = { "presence", 8 };
static pj_str_t STATUS = { "status", 6 };
static pj_str_t OPEN = { "open", 4 };
static pj_str_t CLOSED = { "closed", 6 };
static pj_str_t URI = { "uri", 3 };
static pj_str_t ATOM = { "atom", 4 };
static pj_str_t ATOMID = { "atomid", 6 };
static pj_str_t ADDRESS = { "address", 7 };
static pj_str_t SUBSCRIBE_PARAM = { ";method=SUBSCRIBE", 17 };
static pj_str_t PRESENTITY = { "presentity", 10 };
static pj_str_t EMPTY_STRING = { NULL, 0 };

static pj_xml_node* xml_create_node(pj_pool_t *pool, 
				    pj_str_t *name, const pj_str_t *value)
{
    pj_xml_node *node;

    node = pj_pool_alloc(pool, sizeof(pj_xml_node));
    pj_list_init(&node->attr_head);
    pj_list_init(&node->node_head);
    node->name = *name;
    if (value) pj_strdup(pool, &node->content, value);
    else node->content.ptr=NULL, node->content.slen=0;

    return node;
}

static pj_xml_attr* xml_create_attr(pj_pool_t *pool, pj_str_t *name,
				    const pj_str_t *value)
{
    pj_xml_attr *attr = pj_pool_alloc(pool, sizeof(*attr));
    attr->name = *name;
    pj_strdup(pool, &attr->value, value);
    return attr;
}


PJ_DEF(pjxpidf_pres*) pjxpidf_create(pj_pool_t *pool, const pj_str_t *uri_cstr)
{
    pjxpidf_pres *pres;
    pj_xml_node *presentity;
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_node *status;
    pj_xml_attr *attr;
    pj_str_t uri;
    pj_str_t tmp;

    /* <presence> */
    pres = xml_create_node(pool, &PRESENCE, NULL);

    /* <presentity> */
    presentity = xml_create_node(pool, &PRESENTITY, NULL);
    pj_xml_add_node(pres, presentity);

    /* uri attribute */
    uri.ptr = pj_pool_alloc(pool, uri_cstr->slen + SUBSCRIBE_PARAM.slen);
    pj_strcpy( &uri, uri_cstr);
    pj_strcat( &uri, &SUBSCRIBE_PARAM);
    attr = xml_create_attr(pool, &URI, &uri);
    pj_xml_add_attr(presentity, attr);

    /* <atom> */
    atom = xml_create_node(pool, &ATOM, NULL);
    pj_xml_add_node(pres, atom);

    /* atom id */
    pj_create_unique_string(pool, &tmp);
    attr = xml_create_attr(pool, &ATOMID, &tmp);
    pj_xml_add_attr(atom, attr);

    /* address */
    addr = xml_create_node(pool, &ADDRESS, NULL);
    pj_xml_add_node(atom, addr);

    /* address'es uri */
    attr = xml_create_attr(pool, &URI, uri_cstr);
    pj_xml_add_attr(addr, attr);

    /* status */
    status = xml_create_node(pool, &STATUS, NULL);
    pj_xml_add_node(addr, status);

    /* status attr */
    attr = xml_create_attr(pool, &STATUS, &OPEN);
    pj_xml_add_attr(status, attr);

    return pres;
}   



PJ_DEF(pjxpidf_pres*) pjxpidf_parse(pj_pool_t *pool, char *text, pj_size_t len)
{
    pjxpidf_pres *pres;
    pj_xml_node *node;

    pres = pj_xml_parse(pool, text, len);
    if (!pres)
	return NULL;

    /* Validate <presence> */
    if (pj_stricmp(&pres->name, &PRESENCE) != 0)
	return NULL;
    if (pj_xml_find_attr(pres, &URI, NULL) == NULL)
	return NULL;

    /* Validate <presentity> */
    node = pj_xml_find_node(pres, &PRESENTITY);
    if (node == NULL)
	return NULL;

    /* Validate <atom> */
    node = pj_xml_find_node(pres, &ATOM);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &ATOMID, NULL) == NULL)
	return NULL;

    /* Address */
    node = pj_xml_find_node(node, &ADDRESS);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &URI, NULL) == NULL)
	return NULL;


    /* Status */
    node = pj_xml_find_node(node, &STATUS);
    if (node == NULL)
	return NULL;
    if (pj_xml_find_attr(node, &STATUS, NULL) == NULL)
	return NULL;

    return pres;
}


PJ_DEF(int) pjxpidf_print( pjxpidf_pres *pres, char *text, pj_size_t len)
{
    return pj_xml_print(pres, text, len, PJ_TRUE);
}


PJ_DEF(pj_str_t*) pjxpidf_get_uri(pjxpidf_pres *pres)
{
    pj_xml_node *presentity;
    pj_xml_attr *attr;

    presentity = pj_xml_find_node(pres, &PRESENTITY);
    if (!presentity)
	return &EMPTY_STRING;

    attr = pj_xml_find_attr(presentity, &URI, NULL);
    if (!attr)
	return &EMPTY_STRING;

    return &attr->value;
}


PJ_DEF(pj_status_t) pjxpidf_set_uri(pj_pool_t *pool, pjxpidf_pres *pres, 
				    const pj_str_t *uri)
{
    pj_xml_node *presentity;
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_attr *attr;
    pj_str_t dup_uri;

    presentity = pj_xml_find_node(pres, &PRESENTITY);
    if (!presentity) {
	pj_assert(0);
	return -1;
    }
    atom = pj_xml_find_node(pres, &ATOM);
    if (!atom) {
	pj_assert(0);
	return -1;
    }
    addr = pj_xml_find_node(atom, &ADDRESS);
    if (!addr) {
	pj_assert(0);
	return -1;
    }

    /* Set uri in presentity */
    attr = pj_xml_find_attr(presentity, &URI, NULL);
    if (!attr) {
	pj_assert(0);
	return -1;
    }
    pj_strdup(pool, &dup_uri, uri);
    attr->value = dup_uri;

    /* Set uri in address. */
    attr = pj_xml_find_attr(addr, &URI, NULL);
    if (!attr) {
	pj_assert(0);
	return -1;
    }
    attr->value = dup_uri;

    return 0;
}


PJ_DEF(pj_bool_t) pjxpidf_get_status(pjxpidf_pres *pres)
{
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_node *status;
    pj_xml_attr *attr;

    atom = pj_xml_find_node(pres, &ATOM);
    if (!atom) {
	pj_assert(0);
	return PJ_FALSE;
    }
    addr = pj_xml_find_node(atom, &ADDRESS);
    if (!addr) {
	pj_assert(0);
	return PJ_FALSE;
    }
    status = pj_xml_find_node(atom, &STATUS);
    if (!status) {
	pj_assert(0);
	return PJ_FALSE;
    }
    attr = pj_xml_find_attr(status, &STATUS, NULL);
    if (!attr) {
	pj_assert(0);
	return PJ_FALSE;
    }

    return pj_stricmp(&attr->value, &OPEN) ? PJ_TRUE : PJ_FALSE;
}


PJ_DEF(pj_status_t) pjxpidf_set_status(pjxpidf_pres *pres, pj_bool_t online_status)
{
    pj_xml_node *atom;
    pj_xml_node *addr;
    pj_xml_node *status;
    pj_xml_attr *attr;

    atom = pj_xml_find_node(pres, &ATOM);
    if (!atom) {
	pj_assert(0);
	return -1;
    }
    addr = pj_xml_find_node(atom, &ADDRESS);
    if (!addr) {
	pj_assert(0);
	return -1;
    }
    status = pj_xml_find_node(addr, &STATUS);
    if (!status) {
	pj_assert(0);
	return -1;
    }
    attr = pj_xml_find_attr(status, &STATUS, NULL);
    if (!attr) {
	pj_assert(0);
	return -1;
    }

    attr->value = ( online_status ? OPEN : CLOSED );
    return 0;
}

