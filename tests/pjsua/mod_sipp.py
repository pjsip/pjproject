# $Id$

## Automatic test module for SIPp.
##
## This module will need a test driver for each SIPp scenario:
## - For simple scenario, i.e: make/receive call (including auth), this
##   test module can auto-generate a default test driver, i.e: make call
##   or apply auto answer. Just name the SIPp scenario using "uas" or
##   "uac" prefix accordingly.
## - Custom test driver can be defined in a python script file containing
##   a list of the PJSUA instances and another list for PJSUA expects/
##   commands. The custom test driver file must use the same filename as
##   the SIPp XML scenario. See samples of SIPp scenario + its driver
##   in tests/pjsua/scripts-sipp/ folder for detail.
##
##   Here are defined macros that can be used in the custom driver:
##   - $SIPP_PORT            : SIPp binding port
##   - $SIPP_URI            : SIPp SIP URI
##   - $PJSUA_PORT[N]            : binding port of PJSUA instance #N
##   - $PJSUA_URI[N]            : SIP URI of PJSUA instance #N

import ctypes
import time
import imp
import sys
import os
import re
import subprocess
from inc_cfg import *
import inc_const

# flags that test is running in Unix
G_INUNIX = False
if sys.platform.lower().find("win32")!=-1 or sys.platform.lower().find("microsoft")!=-1:
    G_INUNIX = False
else:
    G_INUNIX = True

# /dev/null handle, for redirecting output when SIPP is not in background mode
FDEVNULL = None

# SIPp executable path and param
#SIPP_PATH = '"C:\\devs\\bin\\Sipp_3.2\\sipp.exe"'
SIPP_PATH = 'sipp'
SIPP_PORT    = 50070
SIPP_PARAM = "-m 1 -i 127.0.0.1 -p " + str(SIPP_PORT)
SIPP_TIMEOUT = 60
# On BG mode, SIPp doesn't require special terminal
# On non-BG mode, on win, it needs env var: "TERMINFO=c:\cygwin\usr\share\terminfo"
# TODO: on unix with BG mode, waitpid() always fails, need to be fixed
SIPP_BG_MODE = False
#SIPP_BG_MODE = not G_INUNIX

# Will be updated based on the test driver file (a .py file whose the same name as SIPp XML file)
PJSUA_INST_PARAM = []
PJSUA_EXPECTS = []

# Default PJSUA param if test driver is not available:
# - no-tcp as SIPp is on UDP only
# - id, username, and realm: to allow PJSUA sending re-INVITE with auth after receiving 401/407 response
PJSUA_DEF_PARAM = "--null-audio --max-calls=1 --no-tcp --id=sip:a@localhost --username=a --realm=*"

# Get SIPp scenario (XML file)
SIPP_SCEN_XML  = ""
if ARGS[1].endswith('.xml'):
    SIPP_SCEN_XML  = ARGS[1]
else:
    exit(-99)


# Functions for resolving macros in the test driver
def resolve_pjsua_port(mo):
    return str(PJSUA_INST_PARAM[int(mo.group(1))].sip_port)

def resolve_pjsua_uri(mo):
    return PJSUA_INST_PARAM[int(mo.group(1))].uri[1:-1]

def resolve_driver_macros(st):
    st = re.sub("\$SIPP_PORT", str(SIPP_PORT), st)
    st = re.sub("\$SIPP_URI", "sip:sipp@127.0.0.1:"+str(SIPP_PORT), st)
    st = re.sub("\$PJSUA_PORT\[(\d+)\]", resolve_pjsua_port, st)
    st = re.sub("\$PJSUA_URI\[(\d+)\]", resolve_pjsua_uri, st)
    return st


# Init test driver
if os.access(SIPP_SCEN_XML[:-4]+".py", os.R_OK):
    # Load test driver file (the corresponding .py file), if any
    cfg_file = imp.load_source("cfg_file", SIPP_SCEN_XML[:-4]+".py")
    for ua_idx, ua_param in enumerate(cfg_file.PJSUA):
        ua_param = resolve_driver_macros(ua_param)
        PJSUA_INST_PARAM.append(InstanceParam("pjsua"+str(ua_idx), ua_param))
    if DEFAULT_TELNET and hasattr(cfg_file, 'PJSUA_CLI_EXPECTS'):
        PJSUA_EXPECTS = cfg_file.PJSUA_CLI_EXPECTS
    else:
        PJSUA_EXPECTS = cfg_file.PJSUA_EXPECTS
else:
    # Generate default test driver
    if os.path.basename(SIPP_SCEN_XML)[0:3] == "uas":
        # auto make call when SIPp is as UAS
        ua_param = PJSUA_DEF_PARAM + " sip:127.0.0.1:" + str(SIPP_PORT)
    else:
        # auto answer when SIPp is as UAC
        ua_param = PJSUA_DEF_PARAM + " --auto-answer=200" 
    PJSUA_INST_PARAM.append(InstanceParam("pjsua", ua_param))


# Start SIPp process, returning PID
def start_sipp():
    global SIPP_BG_MODE
    sipp_proc = None

    sipp_param = SIPP_PARAM + " -sf " + SIPP_SCEN_XML
    if SIPP_BG_MODE:
        sipp_param = sipp_param + " -bg"
    if SIPP_TIMEOUT:
        sipp_param = sipp_param + " -timeout "+str(SIPP_TIMEOUT)+"s -timeout_error" + " -deadcall_wait "+str(SIPP_TIMEOUT)+"s"

    # add target param
    sipp_param = sipp_param + " 127.0.0.1:" + str(PJSUA_INST_PARAM[0].sip_port)

    # run SIPp
    fullcmd = os.path.normpath(SIPP_PATH) + " " + sipp_param
    print "Running SIPP: " + fullcmd
    if SIPP_BG_MODE:
        sipp_proc = subprocess.Popen(fullcmd, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=G_INUNIX, universal_newlines=False)
    else:
        # redirect output to NULL
        global FDEVNULL
        #FDEVNULL  = open(os.devnull, 'w')
        FDEVNULL  = open("logs/sipp_output.tmp", 'w')
        sipp_proc = subprocess.Popen(fullcmd, shell=G_INUNIX, stdout=FDEVNULL, stderr=FDEVNULL)

    if not SIPP_BG_MODE:
        if sipp_proc == None or sipp_proc.poll():
            return None
        return sipp_proc

    else:
        # get SIPp child process PID
        pid = 0
        r = re.compile("PID=\[(\d+)\]", re.I)

        while True:
            line = sipp_proc.stdout.readline()
            pid_r = r.search(line)
            if pid_r:
                pid = int(pid_r.group(1))
                break
            if not sipp_proc.poll():
                break

        if pid != 0:
            # Win specific: get process handle from PID, as on win32, os.waitpid() takes process handle instead of pid
            if (sys.platform == "win32"):
                SYNCHRONIZE = 0x00100000
                PROCESS_QUERY_INFORMATION = 0x0400
                hnd = ctypes.windll.kernel32.OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, False, pid)
                pid = hnd

        return pid


# Wait SIPp process to exit, returning SIPp exit code
def wait_sipp(sipp):
    if not SIPP_BG_MODE:
        global FDEVNULL
        sipp.wait()
        FDEVNULL.close()
        return sipp.returncode

    else:
        print "Waiting SIPp (PID=" + str(sipp) + ") to exit.."
        wait_cnt = 0
        while True:
            try:
                wait_cnt = wait_cnt + 1
                [pid_, ret_code] = os.waitpid(sipp, 0)
                if sipp == pid_:
                    #print "SIPP returned ", ret_code
                    ret_code = ret_code >> 8

                    # Win specific: Close process handle
                    if (sys.platform == "win32"):
                        ctypes.windll.kernel32.CloseHandle(sipp)
                    
                    return ret_code
            except os.error:
                if wait_cnt <= 5:
                    print "Retry ("+str(wait_cnt)+") waiting SIPp.."
                else:
                    return -99


# Execute PJSUA flow
def exec_pjsua_expects(t, sipp):
    # Get all PJSUA instances
    ua = []
    for ua_idx in range(len(PJSUA_INST_PARAM)):
        ua.append(t.process[ua_idx])

    ua_err_st = ""
    while len(PJSUA_EXPECTS):
        expect = PJSUA_EXPECTS.pop(0)
        ua_idx = expect[0]
        expect_st = expect[1]
        send_cmd = resolve_driver_macros(expect[2])
        # Handle exception in pjsua flow, to avoid zombie SIPp process
        try:
            if expect_st != "":
                ua[ua_idx].expect(expect_st, raise_on_error = True)
            if send_cmd != "":
                ua[ua_idx].send(send_cmd)
        except TestError, e:
            ua_err_st = e.desc
            break;
        except:
            ua_err_st = "Unknown error"
            break;

    # Need to poll here for handling these cases:
    # - If there is no PJSUA EXPECT scenario, we must keep polling the stdout,
    #   otherwise PJSUA process may stuck (due to stdout pipe buffer full?).
    # - last PJSUA_EXPECT contains a pjsua command that needs time to
    #   finish, for example "v" (re-INVITE), the SIPp XML scenario may expect
    #   that re-INVITE transaction to be completed and without stdout poll
    #   PJSUA process may stuck.
    # Ideally the poll should be done contiunously until SIPp process is
    # terminated.
    # Update: now pjsua stdout is polled continuously by a dedicated thread,
    #         so the poll is no longer needed
    #for ua_idx in range(len(ua)):
    #        ua[ua_idx].expect(inc_const.STDOUT_REFRESH, raise_on_error = False)

    return ua_err_st


def sipp_err_to_str(err_code):
    if err_code == 0:
        return "All calls were successful"
    elif err_code == 1:
        return "At least one call failed"
    elif err_code == 97:
        return "exit on internal command. Calls may have been processed"
    elif err_code == 99:
        return "Normal exit without calls processed"
    elif err_code == -1:
        return "Fatal error (timeout)"
    elif err_code == -2:
        return "Fatal error binding a socket"
    else:
        return "Unknown error"


# Test body function
def TEST_FUNC(t):

    sipp_ret_code = 0
    ua_err_st = ""

    sipp = start_sipp()
    if not sipp:
        raise TestError("Failed starting SIPp")

    ua_err_st = exec_pjsua_expects(t, sipp)

    sipp_ret_code = wait_sipp(sipp)

    if ua_err_st != "":
        raise TestError(ua_err_st)

    if sipp_ret_code:
        rc = ctypes.c_byte(sipp_ret_code).value
        raise TestError("SIPp returned error " + str(rc) + ": " + sipp_err_to_str(rc))


# Here where it all comes together
test = TestParam(SIPP_SCEN_XML[:-4],
                 PJSUA_INST_PARAM,
                 TEST_FUNC)
