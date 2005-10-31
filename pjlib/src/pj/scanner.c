/* $Header: /pjproject-0.3/pjlib/src/pj/scanner.c 9     10/14/05 12:26a Bennylp $ */
/* $Log: /pjproject-0.3/pjlib/src/pj/scanner.c $
 * 
 * 9     10/14/05 12:26a Bennylp
 * Finished error code framework, some fixes in ioqueue, etc. Pretty
 * major.
 * 
 * 8     9/21/05 1:39p Bennylp
 * Periodic checkin for backup.
 * 
 * 7     9/17/05 10:37a Bennylp
 * Major reorganization towards version 0.3.
 * 
 */
#include <pj/scanner.h>
#include <pj/string.h>
#include <pj/except.h>
#include <pj/os.h>

#define PJ_SCAN_IS_SPACE(c)	((c)==' ' || (c)=='\t')
#define PJ_SCAN_IS_NEWLINE(c)	((c)=='\r' || (c)=='\n')
#define PJ_SCAN_CHECK_EOF(s)	(s != end)


static void pj_scan_syntax_err(pj_scanner *scanner)
{
    (*scanner->callback)(scanner);
}

PJ_DEF(void) pj_cs_init( pj_char_spec cs)
{
    PJ_CHECK_STACK();
    memset(cs, 0, sizeof(cs));
}

PJ_DEF(void) pj_cs_set( pj_char_spec cs, int c)
{
    PJ_CHECK_STACK();
    cs[c] = 1;
}

PJ_DEF(void) pj_cs_add_range( pj_char_spec cs, int cstart, int cend)
{
    PJ_CHECK_STACK();
    while (cstart != cend)
	cs[cstart++] = 1;
}

PJ_DEF(void) pj_cs_add_alpha( pj_char_spec cs)
{
    pj_cs_add_range( cs, 'a', 'z'+1);
    pj_cs_add_range( cs, 'A', 'Z'+1);
}

PJ_DEF(void) pj_cs_add_num( pj_char_spec cs)
{
    pj_cs_add_range( cs, '0', '9'+1);
}

PJ_DEF(void) pj_cs_add_str( pj_char_spec cs, const char *str)
{
    PJ_CHECK_STACK();
    while (*str) {
        cs[(int)*str] = 1;
	++str;
    }
}

PJ_DEF(void) pj_cs_del_range( pj_char_spec cs, int cstart, int cend)
{
    PJ_CHECK_STACK();
    while (cstart != cend)
	cs[cstart++] = 0;
}

PJ_DEF(void) pj_cs_del_str( pj_char_spec cs, const char *str)
{
    PJ_CHECK_STACK();
    while (*str) {
        cs[(int)*str] = 0;
	++str;
    }
}

PJ_DEF(void) pj_cs_invert( pj_char_spec cs )
{
    unsigned i;
    PJ_CHECK_STACK();
    for (i=0; i<sizeof(pj_char_spec)/sizeof(cs[0]); ++i) {
	cs[i] = (pj_char_spec_element_t) !cs[i];
    }
}

PJ_DEF(void) pj_scan_init( pj_scanner *scanner, char *bufstart, int buflen, 
			   unsigned options, pj_syn_err_func_ptr callback )
{
    PJ_CHECK_STACK();

    scanner->begin = scanner->curptr = bufstart;
    scanner->end = bufstart + buflen;
    scanner->line = 1;
    scanner->col = 1;
    scanner->callback = callback;
    scanner->skip_ws = options;

    if (scanner->skip_ws) 
	pj_scan_skip_whitespace(scanner);

    scanner->col = scanner->curptr - scanner->begin + 1;
}


PJ_DEF(void) pj_scan_fini( pj_scanner *scanner )
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(scanner);
}

PJ_DEF(void) pj_scan_skip_whitespace( pj_scanner *scanner )
{
    register char *s = scanner->curptr;

    PJ_CHECK_STACK();

    while (PJ_SCAN_IS_SPACE(*s)) {
	++s;
    }

    if ((scanner->skip_ws & PJ_SCAN_AUTOSKIP_NEWLINE) && PJ_SCAN_IS_NEWLINE(*s)) {
	for (;;) {
	    if (*s == '\r') {
		++s;
		if (*s == '\n') ++s;
		++scanner->line;
		scanner->col = 1;
		scanner->curptr = s;
	    } else if (*s == '\n') {
		++s;
		++scanner->line;
		scanner->col = 1;
		scanner->curptr = s;
	    } else if (PJ_SCAN_IS_SPACE(*s)) {
		do {
		    ++s;
		} while (PJ_SCAN_IS_SPACE(*s));
	    } else {
		break;
	    }
	}
    }

    if (PJ_SCAN_IS_NEWLINE(*s) && (scanner->skip_ws & PJ_SCAN_AUTOSKIP_WS_HEADER)==PJ_SCAN_AUTOSKIP_WS_HEADER) {
	/* Check for header continuation. */
	scanner->col += s - scanner->curptr;
	scanner->curptr = s;

	if (*s == '\r') {
	    ++s;
	}
	if (*s == '\n') {
	    ++s;
	}
	if (PJ_SCAN_IS_SPACE(*s)) {
	    register char *t = s;
	    do {
		++t;
	    } while (PJ_SCAN_IS_SPACE(*t));

	    ++scanner->line;
	    scanner->col = t-s;
	    scanner->curptr = t;
	}
    } else {
	scanner->col += s - scanner->curptr;
	scanner->curptr = s;
    }
}

PJ_DEF(int) pj_scan_peek( pj_scanner *scanner,
			   const pj_char_spec spec, pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner)) {
	pj_scan_syntax_err(scanner);
	return -1;
    }

    while (PJ_SCAN_CHECK_EOF(s) && pj_cs_match(spec, *s))
	++s;

    pj_strset3(out, scanner->curptr, s);
    return s < scanner->end ? *s : 0;
}


PJ_DEF(int) pj_scan_peek_n( pj_scanner *scanner,
			     pj_size_t len, pj_str_t *out)
{
    char *endpos = scanner->curptr + len;

    PJ_CHECK_STACK();

    if (endpos > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }

    pj_strset(out, scanner->curptr, len);
    return *endpos;
}


PJ_DEF(int) pj_scan_peek_until( pj_scanner *scanner,
				  const pj_char_spec spec, 
				  pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner)) {
	pj_scan_syntax_err(scanner);
	return -1;
    }

    while (PJ_SCAN_CHECK_EOF(s) && !pj_cs_match( spec, *s))
	++s;

    pj_strset3(out, scanner->curptr, s);
    return s!=scanner->end ? *s : 0;
}


PJ_DEF(void) pj_scan_get( pj_scanner *scanner,
			   const pj_char_spec spec, pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;
    char *start = s;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner) || !pj_cs_match(spec, *s)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    do {
	++s;
    } while (PJ_SCAN_CHECK_EOF(s) && pj_cs_match(spec, *s));

    pj_strset3(out, scanner->curptr, s);

    scanner->col += (s - start);
    scanner->curptr = s;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);    
    }
}


PJ_DEF(void) pj_scan_get_quote( pj_scanner *scanner,
				 int begin_quote, int end_quote, 
				 pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;
    char *start = s;
    
    PJ_CHECK_STACK();

    /* Check and eat the begin_quote. */
    if (*s != begin_quote) {
	pj_scan_syntax_err(scanner);
	return;
    }
    ++s;

    /* Loop until end_quote is found. 
     */
    do {
	/* loop until end_quote is found. */
	do {
	    ++s;
	} while (s != end && *s != '\n' && *s != end_quote);

	/* check that no backslash character precedes the end_quote. */
	if (*s == end_quote) {
	    if (*(s-1) == '\\') {
		if (s-2 == scanner->begin) {
		    break;
		} else {
		    char *q = s-2;
		    char *r = s-2;

		    while (r != scanner->begin && *r == '\\') {
			--r;
		    }
		    /* break from main loop if we have odd number of backslashes */
		    if (((unsigned)(q-r) & 0x01) == 1) {
			break;
		    }
		}
	    } else {
		/* end_quote is not preceeded by backslash. break now. */
		break;
	    }
	} else {
	    /* loop ended by non-end_quote character. break now. */
	    break;
	}
    } while (1);

    /* Check and eat the end quote. */
    if (*s != end_quote) {
	pj_scan_syntax_err(scanner);
	return;
    }
    ++s;

    pj_strset3(out, scanner->curptr, s);

    scanner->col += (s - start);
    scanner->curptr = s;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}

PJ_DEF(void) pj_scan_get_n( pj_scanner *scanner,
			     unsigned N, pj_str_t *out)
{
    register char *s = scanner->curptr;
    char *start = scanner->curptr;

    PJ_CHECK_STACK();

    if (scanner->curptr + N > scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    pj_strset(out, s, N);
    
    s += N;
    scanner->col += (s - start);
    scanner->curptr = s;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(int) pj_scan_get_char( pj_scanner *scanner )
{
    char *start = scanner->curptr;
    int chr = *start;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner)) {
	pj_scan_syntax_err(scanner);
	return 0;
    }

    ++scanner->curptr;
    scanner->col += (scanner->curptr - start);

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
    return chr;
}


PJ_DEF(void) pj_scan_get_newline( pj_scanner *scanner )
{
    PJ_CHECK_STACK();

    if (!PJ_SCAN_IS_NEWLINE(*scanner->curptr)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    if (*scanner->curptr == '\r') {
	++scanner->curptr;
    }
    if (*scanner->curptr == '\n') {
	++scanner->curptr;
    }

    ++scanner->line;
    scanner->col = 1;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(void) pj_scan_get_until( pj_scanner *scanner,
				 const pj_char_spec spec, pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;
    char *start = s;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    while (PJ_SCAN_CHECK_EOF(s) && !pj_cs_match(spec, *s)) {
	++s;
    }

    pj_strset3(out, scanner->curptr, s);

    scanner->col += (s - start);
    scanner->curptr = s;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(void) pj_scan_get_until_ch( pj_scanner *scanner, 
				    int until_char, pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;
    char *start = s;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    while (PJ_SCAN_CHECK_EOF(s) && *s != until_char) {
	++s;
    }

    pj_strset3(out, scanner->curptr, s);

    scanner->col += (s - start);
    scanner->curptr = s;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(void) pj_scan_get_until_chr( pj_scanner *scanner,
				     const char *until_spec, pj_str_t *out)
{
    register char *s = scanner->curptr;
    register char *end = scanner->end;
    char *start = scanner->curptr;

    PJ_CHECK_STACK();

    if (pj_scan_is_eof(scanner)) {
	pj_scan_syntax_err(scanner);
	return;
    }

    while (PJ_SCAN_CHECK_EOF(s) && !strchr(until_spec, *s)) {
	++s;
    }

    pj_strset3(out, scanner->curptr, s);

    scanner->col += (s - start);
    scanner->curptr = s;

    if (scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}

PJ_DEF(void) pj_scan_advance_n( pj_scanner *scanner,
				 unsigned N, pj_bool_t skip_ws)
{
    char *start = scanner->curptr;

    PJ_CHECK_STACK();

    if (scanner->curptr + N > scanner->end) {
	pj_scan_syntax_err(scanner);
	return;
    }

    scanner->curptr += N;
    scanner->col += (scanner->curptr - start);

    if (skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
}


PJ_DEF(int) pj_scan_strcmp( pj_scanner *scanner, const char *s, int len)
{
    if (scanner->curptr + len > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }
    return strncmp(scanner->curptr, s, len);
}


PJ_DEF(int) pj_scan_stricmp( pj_scanner *scanner, const char *s, int len)
{
    if (scanner->curptr + len > scanner->end) {
	pj_scan_syntax_err(scanner);
	return -1;
    }
    return strnicmp(scanner->curptr, s, len);
}


PJ_DEF(void) pj_scan_save_state( pj_scanner *scanner, pj_scan_state *state)
{
    PJ_CHECK_STACK();

    state->curptr = scanner->curptr;
    state->line = scanner->line;
    state->col = scanner->col;
}


PJ_DEF(void) pj_scan_restore_state( pj_scanner *scanner, 
				     pj_scan_state *state)
{
    PJ_CHECK_STACK();

    scanner->curptr = state->curptr;
    scanner->line = state->line;
    scanner->col = state->col;
}


