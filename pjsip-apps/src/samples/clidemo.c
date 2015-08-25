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

static pj_status_t quit_app(pj_cli_cmd_val *cval)
{
    PJ_UNUSED_ARG(cval);
    pj_cli_quit(cval->sess->fe->cli, cval->sess, PJ_FALSE);

    return PJ_CLI_EEXIT;
}

static void get_codec_list(pj_cli_dyn_choice_param *param)
{
    if (param->arg_id == 3) {
	param->cnt = 2;
	pj_strdup2(param->pool, &param->choice[0].value, "iLbc");
	pj_strdup2(param->pool, &param->choice[0].desc, "iLbc Codec");
	pj_strdup2(param->pool, &param->choice[1].value, "g729");
	pj_strdup2(param->pool, &param->choice[1].desc, "g729 Codec");
    }
}

static struct cmd_xml_t cmd_xmls[] = {
    {"<CMD name='sayhello' id='1' sc='  ,h , ,, sh  ,' desc='Will say hello'>"
     "    <ARG name='whom' type='text' desc='Whom to say hello to'/>"
     "</CMD>",
     &sayhello},
    {"<CMD name='saybye' id='2' sc='b,sb' desc='Will say bye'>"
     "    <ARG name='whom' type='text' desc='Whom to say bye to'/>"
     "</CMD>",
     &saybye},
    {"<CMD name='saymsg' id='3' sc='s' desc='Will say something'>"
     "    <ARG name='msg' type='text' desc='Message to say'/>"
     "    <ARG name='whom' type='text' desc='Whom to say to'/>"
     "</CMD>",
     &say},
    {"<CMD name='vid' id='1' desc='Video Command'>"
     "   <CMD name='help' id='2' desc='Show Help' />"
     "   <CMD name='enable' id='3' desc='Enable Video' />"
     "   <CMD name='disable' id='4' desc='Disable Video' />"
     "   <CMD name='call' id='5' desc='Video call' >"
     "            <CMD name='add' id='6' desc='Add Call' />"
     "            <CMD name='cap' id='7' desc='Capture Call' >"
     "               <ARG name='streamno' type='int' desc='Stream No' id='1'/>"
     "               <ARG name='devid' type='int' desc='Device Id' id='2'/>"
     "            </CMD>"
     "   </CMD>"     
     "</CMD>",
     NULL},
    {"<CMD name='disable_codec' id='8' desc='Disable codec'>"
     "	<ARG name='codec_list' type='choice' id='3' desc='Codec list'>"
     "	    <CHOICE value='g711' desc='G711 Codec'/>"
     "	    <CHOICE value='g722' desc='G722 Codec'/>"
     "	</ARG>"
     "</CMD>",
     NULL},
    {"<CMD name='quit_app' id='999' sc='qa' desc='Quit the application'>"
     "</CMD>",
     &quit_app},
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
	    cmd_xmls[i].handler, NULL, get_codec_list);
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
#else
    tcfg.port = 2233;
#endif    
    tcfg.prompt_str = pj_str("CoolWater% ");
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
pj_status_t app_main(pj_cli_t *c)
{
    pj_status_t status;
    pj_cli_sess *sess;
    pj_cli_console_cfg console_cfg;

    pj_cli_console_cfg_default(&console_cfg);
    console_cfg.prompt_str = pj_str("HotWater> ");
    
    /*
     * Create the console front end
     */
    status = pj_cli_console_create(c, &console_cfg, &sess, NULL);
    if (status != PJ_SUCCESS)
	return status;

    pj_log_set_log_func(&log_writer);

    /*
     * Main loop.
     */
    for (;;) {
	char cmdline[PJ_CLI_MAX_CMDBUF];

        status = pj_cli_console_process(sess, &cmdline[0], sizeof(cmdline));
	if (status != PJ_SUCCESS)
	    break;

	//pj_ansi_strcpy(cmdline, "sayhello {Teluu Inc.}");	
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
