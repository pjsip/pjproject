/* $Id$
 *
 */
#ifndef __PJPP_SCANNER_H__
#define __PJPP_SCANNER_H__

#include <pj/scanner.h>
#include <pj++/string.hpp>

class PJ_CharSpec
{
public:
    PJ_CharSpec() { pj_cs_init(cs__); }

    void set(int c) { pj_cs_set(cs__, c); }
    void add_range(int begin, int end) { pj_cs_add_range(cs__, begin, end); }
    void add_alpha() { pj_cs_add_alpha(cs__); }
    void add_num() { pj_cs_add_num(cs__); }
    void add_str(const char *str) { pj_cs_add_str(cs__, str); }
    void del_range(int begin, int end) { pj_cs_del_range(cs__, begin, end); }
    void del_str(const char *str) { pj_cs_del_str(cs__, str); }
    void invert() { pj_cs_invert(cs__); }
    int  match(int c) { return pj_cs_match(cs__, c); }

    pj_char_spec_element_t *cs_()
    {
	return cs__;
    }

    const pj_char_spec_element_t *cs_() const
    {
	return cs__;
    }

private:
    pj_char_spec cs__;
};

class PJ_Scanner
{
public:
    PJ_Scanner() {}

    enum
    {
	SYNTAX_ERROR = 101
    };
    static void syntax_error_handler_throw_pj(pj_scanner *);

    typedef pj_scan_state State;

    void init(char *buf, int len, unsigned options=PJ_SCAN_AUTOSKIP_WS, 
	      pj_syn_err_func_ptr callback = &syntax_error_handler_throw_pj)
    {
	pj_scan_init(&scanner_, buf, len, options, callback);
    }

    void fini()
    {
	pj_scan_fini(&scanner_);
    }

    int eof() const
    {
	return pj_scan_is_eof(&scanner_);
    }

    int peek_char() const
    {
	return *scanner_.current;
    }

    int peek(const PJ_CharSpec *cs, PJ_String *out)
    {
	return pj_scan_peek(&scanner_,  cs->cs_(), out);
    }

    int peek_n(pj_size_t len, PJ_String *out)
    {
	return pj_scan_peek_n(&scanner_, len, out);
    }

    int peek_until(const PJ_CharSpec *cs, PJ_String *out)
    {
	return pj_scan_peek_until(&scanner_, cs->cs_(), out);
    }

    void get(const PJ_CharSpec *cs, PJ_String *out)
    {
	pj_scan_get(&scanner_, cs->cs_(), out);
    }

    void get_n(unsigned N, PJ_String *out)
    {
	pj_scan_get_n(&scanner_, N, out);
    }

    int get_char()
    {
	return pj_scan_get_char(&scanner_);
    }

    void get_quote(int begin_quote, int end_quote, PJ_String *out)
    {
	pj_scan_get_quote(&scanner_, begin_quote, end_quote, out);
    }

    void get_newline()
    {
	pj_scan_get_newline(&scanner_);
    }

    void get_until(const PJ_CharSpec *cs, PJ_String *out)
    {
	pj_scan_get_until(&scanner_, cs->cs_(), out);
    }

    void get_until_ch(int until_ch, PJ_String *out)
    {
	pj_scan_get_until_ch(&scanner_, until_ch, out);
    }

    void get_until_chr(const char *spec, PJ_String *out)
    {
	pj_scan_get_until_chr(&scanner_, spec, out);
    }

    void advance_n(unsigned N, bool skip_ws=true)
    {
	pj_scan_advance_n(&scanner_, N, skip_ws);
    }

    int strcmp(const char *s, int len)
    {
	return pj_scan_strcmp(&scanner_, s, len);
    }

    int stricmp(const char *s, int len)
    {
	return pj_scan_stricmp(&scanner_, s, len);
    }

    void skip_ws()
    {
	pj_scan_skip_whitespace(&scanner_);
    }

    void save_state(State *state)
    {
	pj_scan_save_state(&scanner_, state);
    }

    void restore_state(State *state)
    {
	pj_scan_restore_state(&scanner_, state);
    }

    int get_pos_line() const
    {
	return scanner_.line;
    }

    int get_pos_col() const
    {
	return scanner_.col;
    }


private:
    pj_scanner scanner_;
};

#endif	/* __PJPP_SCANNER_H__ */
