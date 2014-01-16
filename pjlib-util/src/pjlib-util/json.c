/* $Id$ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
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
#include <pjlib-util/json.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/scanner.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/except.h>
#include <pj/string.h>

#define EL_INIT(p_el, nm, typ)	do { \
				    if (nm) { \
					p_el->name = *nm; \
				    } else { \
					p_el->name.ptr = (char*)""; \
					p_el->name.slen = 0; \
				    } \
				    p_el->type = typ; \
				} while (0)

struct write_state;
struct parse_state;

#define NO_NAME	1

static pj_status_t elem_write(const pj_json_elem *elem,
                              struct write_state *st,
                              unsigned flags);
static pj_json_elem* parse_elem_throw(struct parse_state *st,
                                      pj_json_elem *elem);


PJ_DEF(void) pj_json_elem_null(pj_json_elem *el, pj_str_t *name)
{
    EL_INIT(el, name, PJ_JSON_VAL_NULL);
}

PJ_DEF(void) pj_json_elem_bool(pj_json_elem *el, pj_str_t *name,
                                pj_bool_t val)
{
    EL_INIT(el, name, PJ_JSON_VAL_BOOL);
    el->value.is_true = val;
}

PJ_DEF(void) pj_json_elem_number(pj_json_elem *el, pj_str_t *name,
                                  float val)
{
    EL_INIT(el, name, PJ_JSON_VAL_NUMBER);
    el->value.num = val;
}

PJ_DEF(void) pj_json_elem_string( pj_json_elem *el, pj_str_t *name,
                                  pj_str_t *value)
{
    EL_INIT(el, name, PJ_JSON_VAL_STRING);
    el->value.str = *value;
}

PJ_DEF(void) pj_json_elem_array(pj_json_elem *el, pj_str_t *name)
{
    EL_INIT(el, name, PJ_JSON_VAL_ARRAY);
    pj_list_init(&el->value.children);
}

PJ_DEF(void) pj_json_elem_obj(pj_json_elem *el, pj_str_t *name)
{
    EL_INIT(el, name, PJ_JSON_VAL_OBJ);
    pj_list_init(&el->value.children);
}

PJ_DEF(void) pj_json_elem_add(pj_json_elem *el, pj_json_elem *child)
{
    pj_assert(el->type == PJ_JSON_VAL_OBJ || el->type == PJ_JSON_VAL_ARRAY);
    pj_list_push_back(&el->value.children, child);
}

struct parse_state
{
    pj_pool_t		*pool;
    pj_scanner		 scanner;
    pj_json_err_info 	*err_info;
    pj_cis_t		 float_spec;	/* numbers with dot! */
};

static pj_status_t parse_children(struct parse_state *st,
                                  pj_json_elem *parent)
{
    char end_quote = (parent->type == PJ_JSON_VAL_ARRAY)? ']' : '}';

    pj_scan_get_char(&st->scanner);

    while (*st->scanner.curptr != end_quote) {
	pj_json_elem *child;

	while (*st->scanner.curptr == ',')
	    pj_scan_get_char(&st->scanner);

	if (*st->scanner.curptr == end_quote)
	    break;

	child = parse_elem_throw(st, NULL);
	if (!child)
	    return PJLIB_UTIL_EINJSON;

	pj_json_elem_add(parent, child);
    }

    pj_scan_get_char(&st->scanner);
    return PJ_SUCCESS;
}

/* Return 0 if success or the index of the invalid char in the string */
static unsigned parse_quoted_string(struct parse_state *st,
                                    pj_str_t *output)
{
    pj_str_t token;
    char *op, *ip, *iend;

    pj_scan_get_quote(&st->scanner, '"', '"', &token);

    /* Remove the quote characters */
    token.ptr++;
    token.slen-=2;

    if (pj_strchr(&token, '\\') == NULL) {
	*output = token;
	return 0;
    }

    output->ptr = op = pj_pool_alloc(st->pool, token.slen);

    ip = token.ptr;
    iend = token.ptr + token.slen;

    while (ip != iend) {
	if (*ip == '\\') {
	    ++ip;
	    if (ip==iend) {
		goto on_error;
	    }
	    if (*ip == 'u') {
		ip++;
		if (iend - ip < 4) {
		    ip = iend -1;
		    goto on_error;
		}
		/* Only use the last two hext digits because we're on
		 * ASCII */
		*op++ = (char)(pj_hex_digit_to_val(ip[2]) * 16 +
			       pj_hex_digit_to_val(ip[3]));
		ip += 4;
	    } else if (*ip=='"' || *ip=='\\' || *ip=='/') {
		*op++ = *ip++;
	    } else if (*ip=='b') {
		*op++ = '\b';
		ip++;
	    } else if (*ip=='f') {
		*op++ = '\f';
		ip++;
	    } else if (*ip=='n') {
		*op++ = '\n';
		ip++;
	    } else if (*ip=='r') {
		*op++ = '\r';
		ip++;
	    } else if (*ip=='t') {
		*op++ = '\t';
		ip++;
	    } else {
		goto on_error;
	    }
	} else {
	    *op++ = *ip++;
	}
    }

    output->slen = op - output->ptr;
    return 0;

on_error:
    output->slen = op - output->ptr;
    return ip - token.ptr;
}

static pj_json_elem* parse_elem_throw(struct parse_state *st,
                                      pj_json_elem *elem)
{
    pj_str_t name = {NULL, 0}, value = {NULL, 0};
    pj_str_t token;

    if (!elem)
	elem = pj_pool_alloc(st->pool, sizeof(*elem));

    /* Parse name */
    if (*st->scanner.curptr == '"') {
	pj_scan_get_char(&st->scanner);
	pj_scan_get_until_ch(&st->scanner, '"', &token);
	pj_scan_get_char(&st->scanner);

	if (*st->scanner.curptr == ':') {
	    pj_scan_get_char(&st->scanner);
	    name = token;
	} else {
	    value = token;
	}
    }

    if (value.slen) {
	/* Element with string value and no name */
	pj_json_elem_string(elem, &name, &value);
	return elem;
    }

    /* Parse value */
    if (pj_cis_match(&st->float_spec, *st->scanner.curptr) ||
	*st->scanner.curptr == '-')
    {
	float val;
	pj_bool_t neg = PJ_FALSE;

	if (*st->scanner.curptr == '-') {
	    pj_scan_get_char(&st->scanner);
	    neg = PJ_TRUE;
	}

	pj_scan_get(&st->scanner, &st->float_spec, &token);
	val = pj_strtof(&token);
	if (neg) val = -val;

	pj_json_elem_number(elem, &name, val);

    } else if (*st->scanner.curptr == '"') {
	unsigned err;
	char *start = st->scanner.curptr;

	err = parse_quoted_string(st, &token);
	if (err) {
	    st->scanner.curptr = start + err;
	    return NULL;
	}

	pj_json_elem_string(elem, &name, &token);

    } else if (pj_isalpha(*st->scanner.curptr)) {

	if (pj_scan_strcmp(&st->scanner, "false", 5)==0) {
	    pj_json_elem_bool(elem, &name, PJ_FALSE);
	    pj_scan_advance_n(&st->scanner, 5, PJ_TRUE);
	} else if (pj_scan_strcmp(&st->scanner, "true", 4)==0) {
	    pj_json_elem_bool(elem, &name, PJ_TRUE);
	    pj_scan_advance_n(&st->scanner, 4, PJ_TRUE);
	} else if (pj_scan_strcmp(&st->scanner, "null", 4)==0) {
	    pj_json_elem_null(elem, &name);
	    pj_scan_advance_n(&st->scanner, 4, PJ_TRUE);
	} else {
	    return NULL;
	}

    } else if (*st->scanner.curptr == '[') {
	pj_json_elem_array(elem, &name);
	if (parse_children(st, elem) != PJ_SUCCESS)
	    return NULL;

    } else if (*st->scanner.curptr == '{') {
	pj_json_elem_obj(elem, &name);
	if (parse_children(st, elem) != PJ_SUCCESS)
	    return NULL;

    } else {
	return NULL;
    }

    return elem;
}

static void on_syntax_error(pj_scanner *scanner)
{
    PJ_UNUSED_ARG(scanner);
    PJ_THROW(11);
}

PJ_DEF(pj_json_elem*) pj_json_parse(pj_pool_t *pool,
                                    char *buffer,
                                    unsigned *size,
                                    pj_json_err_info *err_info)
{
    pj_cis_buf_t cis_buf;
    struct parse_state st;
    pj_json_elem *root;
    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(pool && buffer && size, NULL);

    if (!*size)
	return NULL;

    pj_bzero(&st, sizeof(st));
    st.pool = pool;
    st.err_info = err_info;
    pj_scan_init(&st.scanner, buffer, *size,
                 PJ_SCAN_AUTOSKIP_WS | PJ_SCAN_AUTOSKIP_NEWLINE,
                 &on_syntax_error);
    pj_cis_buf_init(&cis_buf);
    pj_cis_init(&cis_buf, &st.float_spec);
    pj_cis_add_str(&st.float_spec, ".0123456789");

    PJ_TRY {
	root = parse_elem_throw(&st, NULL);
    }
    PJ_CATCH_ANY {
	root = NULL;
    }
    PJ_END

    if (!root && err_info) {
	err_info->line = st.scanner.line;
	err_info->col = pj_scan_get_col(&st.scanner) + 1;
	err_info->err_char = *st.scanner.curptr;
    }

    *size = (buffer + *size) - st.scanner.curptr;

    pj_scan_fini(&st.scanner);

    return root;
}

struct buf_writer_data
{
    char	*pos;
    unsigned	 size;
};

static pj_status_t buf_writer(const char *s,
			      unsigned size,
			      void *user_data)
{
    struct buf_writer_data *buf_data = (struct buf_writer_data*)user_data;
    if (size+1 >= buf_data->size)
	return PJ_ETOOBIG;

    pj_memcpy(buf_data->pos, s, size);
    buf_data->pos += size;
    buf_data->size -= size;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_json_write(const pj_json_elem *elem,
                                  char *buffer, unsigned *size)
{
    struct buf_writer_data buf_data;
    pj_status_t status;

    PJ_ASSERT_RETURN(elem && buffer && size, PJ_EINVAL);

    buf_data.pos = buffer;
    buf_data.size = *size;

    status = pj_json_writef(elem, &buf_writer, &buf_data);
    if (status != PJ_SUCCESS)
	return status;

    *buf_data.pos = '\0';
    *size = (unsigned)(buf_data.pos - buffer);
    return PJ_SUCCESS;
}

#define MAX_INDENT 		100
#ifndef PJ_JSON_NAME_MIN_LEN
#  define PJ_JSON_NAME_MIN_LEN	20
#endif
#define ESC_BUF_LEN		64
#ifndef PJ_JSON_INDENT_SIZE
#  define PJ_JSON_INDENT_SIZE	3
#endif

struct write_state
{
    pj_json_writer	 writer;
    void 		*user_data;
    char		 indent_buf[MAX_INDENT];
    int			 indent;
    char		 space[PJ_JSON_NAME_MIN_LEN];
};

#define CHECK(expr) do { \
			status=expr; if (status!=PJ_SUCCESS) return status; } \
		    while (0)

static pj_status_t write_string_escaped(const pj_str_t *value,
                                        struct write_state *st)
{
    const char *ip = value->ptr;
    const char *iend = value->ptr + value->slen;
    char buf[ESC_BUF_LEN];
    char *op = buf;
    char *oend = buf + ESC_BUF_LEN;
    pj_status_t status;

    while (ip != iend) {
	/* Write to buffer to speedup writing instead of calling
	 * the callback one by one for each character.
	 */
	while (ip != iend && op != oend) {
	    if (oend - op < 2)
		break;

	    if (*ip == '"') {
		*op++ = '\\';
		*op++ = '"';
		ip++;
	    } else if (*ip == '\\') {
		*op++ = '\\';
		*op++ = '\\';
		ip++;
	    } else if (*ip == '/') {
		*op++ = '\\';
		*op++ = '/';
		ip++;
	    } else if (*ip == '\b') {
		*op++ = '\\';
		*op++ = 'b';
		ip++;
	    } else if (*ip == '\f') {
		*op++ = '\\';
		*op++ = 'f';
		ip++;
	    } else if (*ip == '\n') {
		*op++ = '\\';
		*op++ = 'n';
		ip++;
	    } else if (*ip == '\r') {
		*op++ = '\\';
		*op++ = 'r';
		ip++;
	    } else if (*ip == '\t') {
		*op++ = '\\';
		*op++ = 't';
		ip++;
	    } else if ((*ip >= 32 && *ip < 127)) {
		/* unescaped */
		*op++ = *ip++;
	    } else {
		/* escaped */
		if (oend - op < 6)
		    break;
		*op++ = '\\';
		*op++ = 'u';
		*op++ = '0';
		*op++ = '0';
		pj_val_to_hex_digit(*ip, op);
		op+=2;
		ip++;
	    }
	}

	CHECK( st->writer( buf, op-buf, st->user_data) );
	op = buf;
    }

    return PJ_SUCCESS;
}

static pj_status_t write_children(const pj_json_list *list,
                                  const char quotes[2],
                                  struct write_state *st)
{
    unsigned flags = (quotes[0]=='[') ? NO_NAME : 0;
    pj_status_t status;

    //CHECK( st->writer( st->indent_buf, st->indent, st->user_data) );
    CHECK( st->writer( &quotes[0], 1, st->user_data) );
    CHECK( st->writer( " ", 1, st->user_data) );

    if (!pj_list_empty(list)) {
	pj_bool_t indent_added = PJ_FALSE;
	pj_json_elem *child = list->next;

	if (child->name.slen == 0) {
	    /* Simple list */
	    while (child != (pj_json_elem*)list) {
		status = elem_write(child, st, flags);
		if (status != PJ_SUCCESS)
		    return status;

		if (child->next != (pj_json_elem*)list)
		    CHECK( st->writer( ", ", 2, st->user_data) );
		child = child->next;
	    }
	} else {
	    if (st->indent < sizeof(st->indent_buf)) {
		st->indent += PJ_JSON_INDENT_SIZE;
		indent_added = PJ_TRUE;
	    }
	    CHECK( st->writer( "\n", 1, st->user_data) );
	    while (child != (pj_json_elem*)list) {
		status = elem_write(child, st, flags);
		if (status != PJ_SUCCESS)
		    return status;

		if (child->next != (pj_json_elem*)list)
		    CHECK( st->writer( ",\n", 2, st->user_data) );
		else
		    CHECK( st->writer( "\n", 1, st->user_data) );
		child = child->next;
	    }
	    if (indent_added) {
		st->indent -= PJ_JSON_INDENT_SIZE;
	    }
	    CHECK( st->writer( st->indent_buf, st->indent, st->user_data) );
	}
    }
    CHECK( st->writer( &quotes[1], 1, st->user_data) );

    return PJ_SUCCESS;
}

static pj_status_t elem_write(const pj_json_elem *elem,
                              struct write_state *st,
                              unsigned flags)
{
    pj_status_t status;

    if (elem->name.slen) {
	CHECK( st->writer( st->indent_buf, st->indent, st->user_data) );
	if ((flags & NO_NAME)==0) {
	    CHECK( st->writer( "\"", 1, st->user_data) );
	    CHECK( write_string_escaped(&elem->name, st) );
	    CHECK( st->writer( "\": ", 3, st->user_data) );
	    if (elem->name.slen < PJ_JSON_NAME_MIN_LEN /*&&
		elem->type != PJ_JSON_VAL_OBJ &&
		elem->type != PJ_JSON_VAL_ARRAY*/)
	    {
		CHECK( st->writer( st->space,
		                   PJ_JSON_NAME_MIN_LEN - elem->name.slen,
				   st->user_data) );
	    }
	}
    }

    switch (elem->type) {
    case PJ_JSON_VAL_NULL:
	CHECK( st->writer( "null", 4, st->user_data) );
	break;
    case PJ_JSON_VAL_BOOL:
	if (elem->value.is_true)
	    CHECK( st->writer( "true", 4, st->user_data) );
	else
	    CHECK( st->writer( "false", 5, st->user_data) );
	break;
    case PJ_JSON_VAL_NUMBER:
	{
	    char num_buf[65];
	    int len;

	    if (elem->value.num == (int)elem->value.num)
		len = pj_ansi_snprintf(num_buf, sizeof(num_buf), "%d",
		                       (int)elem->value.num);
	    else
		len = pj_ansi_snprintf(num_buf, sizeof(num_buf), "%f",
		                       elem->value.num);

	    if (len < 0 || len >= sizeof(num_buf))
		return PJ_ETOOBIG;
	    CHECK( st->writer( num_buf, len, st->user_data) );
	}
	break;
    case PJ_JSON_VAL_STRING:
	CHECK( st->writer( "\"", 1, st->user_data) );
	CHECK( write_string_escaped( &elem->value.str, st) );
	CHECK( st->writer( "\"", 1, st->user_data) );
	break;
    case PJ_JSON_VAL_ARRAY:
	CHECK( write_children(&elem->value.children, "[]", st) );
	break;
    case PJ_JSON_VAL_OBJ:
	CHECK( write_children(&elem->value.children, "{}", st) );
	break;
    default:
	pj_assert(!"Unhandled value type");
    }

    return PJ_SUCCESS;
}

#undef CHECK

PJ_DEF(pj_status_t) pj_json_writef( const pj_json_elem *elem,
                                    pj_json_writer writer,
                                    void *user_data)
{
    struct write_state st;

    PJ_ASSERT_RETURN(elem && writer, PJ_EINVAL);

    st.writer 		= writer;
    st.user_data	= user_data,
    st.indent 		= 0;
    pj_memset(st.indent_buf, ' ', MAX_INDENT);
    pj_memset(st.space, ' ', PJ_JSON_NAME_MIN_LEN);

    return elem_write(elem, &st, 0);
}

