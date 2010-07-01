/* $Id$ */
/*
 * Copyright (C) 2010 Teluu Inc. (http://www.teluu.com)
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

/*
 * Sample CLI application
 */
#include <pjlib-util/cli.h>
#include <pjlib-util/cli_imp.h>
#include <pjlib-util/cli_console.h>
#include <pjlib-util/cli_telnet.h>
#include <pjlib-util/errno.h>
#include <pjlib.h>

#define THIS_FILE	"clidemo.c"

/* Set this to 1 if you want to let the system assign a port
 * for the CLI telnet daemon.
 * Default: 1
 */
#define USE_RANDOM_PORT 1

struct cmd_xml_t {
    char * xml;
    pj_cli_cmd_handler handler;
};

/*
 * Declaration of system specific main loop, which will be defined in
 * a separate file.
 */
pj_status_t app_main(pj_cli_t *cli);

#define print_msg(arg) \
    do { \
        unsigned d = pj_log_get_decor(); \
        pj_log_set_decor(0); \
        PJ_LOG(1, arg); \
        pj_log_set_decor(d); \
    } while (0)

static pj_cli_t *cli = NULL;

/* Handler for sayhello command */
static pj_status_t sayhello(pj_cli_cmd_val *cval)
{
    print_msg(("", "Hello %.*s!\r\n", 
              (int)cval->argv[1].slen, cval->argv[1].ptr));
    return PJ_SUCCESS;
}

/* Handler for saybye command */
static pj_status_t saybye(pj_cli_cmd_val *cval)
{
    print_msg(("", "Bye %.*s!\r\n",
              (int)cval->argv[1].slen, cval->argv[1].ptr));
    return PJ_SUCCESS;
}

/* Handler for say command */
static pj_status_t say(pj_cli_cmd_val *cval)
{
    print_msg(("", "%.*s %.*s\r\n",
              (int)cval->argv[1].slen, cval->argv[1].ptr,
              (int)cval->argv[2].slen, cval->argv[2].ptr));
    return PJ_SUCCESS;
}

static pj_status_t quit(pj_cli_cmd_val *cval)
{
    PJ_UNUSED_ARG(cval);
    pj_cli_end_session(cval->sess);

    return PJ_CLI_EEXIT;
}

static struct cmd_xml_t cmd_xmls[] = {
    {"<CMD name='sayhello' id='1' sc='  ,h , ,, sh  ,' desc='Will say hello'>"
     "  <ARGS>"
     "    <ARG name='whom' type='text' desc='Whom to say hello to'/>"
     "  </ARGS>"
     "</CMD>",
     &sayhello},
    {"<CMD name='saybye' id='2' sc='b,sb' desc='Will say bye'>"
     "  <ARGS>"
     "    <ARG name='whom' type='text' desc='Whom to say bye to'/>"
     "  </ARGS>"
     "</CMD>",
     &saybye},
    {"<CMD name=' say ' id='3' sc='s' desc='Will say something'>"
     "  <ARGS>"
     "    <ARG name='msg' type='text' desc='Message to say'/>"
     "    <ARG name='whom' type='text' desc='Whom to say to'/>"
     "  </ARGS>"
     "</CMD>",
     &say},
    {"<CMD name='quit' id='999' sc='q' desc='Quit the application'>"
     "</CMD>",
     &quit},
};

static void log_writer(int level, const char *buffer, int len)
{
    if (cli)
        pj_cli_write_log(cli, level, buffer, len);
}

int main()
{
    pj_caching_pool cp;
    pj_cli_cfg cli_cfg;
    pj_cli_telnet_cfg tcfg;
    pj_str_t xml;
    pj_status_t status;
    int i;

    pj_init();
    pj_caching_pool_init(&cp, NULL, 0);
    pjlib_util_init();

    /*
     * Create CLI app.
     */
    pj_cli_cfg_default(&cli_cfg);
    cli_cfg.pf = &cp.factory;
    cli_cfg.name = pj_str("mycliapp");
    cli_cfg.title = pj_str("My CLI Application");

    status = pj_cli_create(&cli_cfg, &cli);
    if (status != PJ_SUCCESS)
	goto on_return;

    /*
     * Register some commands.
     */
    for (i = 0; i < sizeof(cmd_xmls)/sizeof(cmd_xmls[0]); i++) {
        xml = pj_str(cmd_xmls[i].xml);
        status = pj_cli_add_cmd_from_xml(cli, NULL, &xml, 
                                         cmd_xmls[i].handler, NULL);
        if (status != PJ_SUCCESS)
	    goto on_return;
    }

    /*
     * Start telnet daemon
     */
    pj_cli_telnet_cfg_default(&tcfg);
//    tcfg.passwd = pj_str("pjsip");
#if USE_RANDOM_PORT
    tcfg.port = 0;
#endif
    status = pj_cli_telnet_create(cli, &tcfg, NULL);
    if (status != PJ_SUCCESS)
	goto on_return;

    /*
     * Run the system specific main loop.
     */
    status = app_main(cli);

on_return:

    /*
     * Destroy
     */
    pj_cli_destroy(cli);
    cli = NULL;
    pj_caching_pool_destroy(&cp);
    pj_shutdown();

    return (status != PJ_SUCCESS ? 1 : 0);
}


/*xxxxxxxxxxxxxxxxxxxxxxxxxxxxx main_console.c xxxxxxxxxxxxxxxxxxxxxxxxxxxx */
/*
 * Simple implementation of app_main() for console targets
 */
pj_status_t app_main(pj_cli_t *cli)
{
    pj_status_t status;
    pj_cli_sess *sess;

    /*
     * Create the console front end
     */
    status = pj_cli_console_create(cli, NULL, &sess, NULL);
    if (status != PJ_SUCCESS)
	return status;

    pj_log_set_log_func(&log_writer);

    /*
     * Main loop.
     */
    for (;;) {
	char cmdline[PJ_CLI_MAX_CMDBUF];
        pj_status_t status;

        status = pj_cli_console_readline(sess, cmdline, sizeof(cmdline));
	if (status != PJ_SUCCESS)
	    break;

//        pj_ansi_strcpy(cmdline, "sayhello {Teluu Inc.}");
	status = pj_cli_exec(sess, cmdline, NULL);
	if (status == PJ_CLI_EEXIT) {
	    /* exit is called */
	    break;
	} else if (status != PJ_SUCCESS) {
	    /* Something wrong with the cmdline */
	    PJ_PERROR(1,(THIS_FILE, status, "Exec error"));
	}
    }

    return PJ_SUCCESS;
}
