# $Id$
import ctypes
import time
import imp
import sys
import os
import re
import subprocess
from inc_cfg import *

# SIPp executable path and param
#SIPP_PATH = '"C:\\Program Files (x86)\\Sipp_3.2\\sipp.exe"'
SIPP_PATH = 'sipp'
SIPP_PARAM = "-i 127.0.0.1 -p 6000 -m 1 127.0.0.1"
SIPP_TIMEOUT = 10
# On BG mode, SIPp doesn't require special terminal
# On non-BG mode, on win, it needs env var: "TERMINFO=c:\cygwin\usr\share\terminfo"
SIPP_BG_MODE = True

PJSUA_DEF_PARAM = "--null-audio --max-calls=1 --no-tcp"
PJSUA_INST_PARAM = []
PJSUA_EXPECTS = []

# Get SIPp scenario (XML file)
SIPP_SCEN_XML  = ""
if ARGS[1].endswith('.xml'):
    SIPP_SCEN_XML  = ARGS[1]
else:
    exit(-99)

# Init PJSUA test instance
if os.access(SIPP_SCEN_XML[:-4]+".py", os.R_OK):
    # Load from configuration file (the corresponding .py file), if any
    cfg_file = imp.load_source("cfg_file", SIPP_SCEN_XML[:-4]+".py")
    for ua_idx, ua_param in enumerate(cfg_file.PJSUA):
	PJSUA_INST_PARAM.append(InstanceParam("pjsua"+str(ua_idx+1), ua_param, sip_port=5060+ua_idx*2))
    PJSUA_EXPECTS = cfg_file.PJSUA_EXPECTS
else:
    # Just use the SIPp XML scenario
    if os.path.basename(SIPP_SCEN_XML)[0:3] == "uas":
	# auto make call when SIPp is as UAS
	ua_param = PJSUA_DEF_PARAM + " sip:127.0.0.1:6000"
    else:
	# auto answer when SIPp is as UAC
	ua_param = PJSUA_DEF_PARAM + " --auto-answer=200" 
    PJSUA_INST_PARAM.append(InstanceParam("pjsua", ua_param, sip_port=5060))
    


# Start SIPp process, returning PID
def start_sipp():
    global SIPP_BG_MODE
    sipp_proc = None

    # run SIPp
    sipp_param = SIPP_PARAM + " -sf " + SIPP_SCEN_XML
    if SIPP_BG_MODE:
	sipp_param = sipp_param + " -bg"
    if SIPP_TIMEOUT:
	sipp_param = sipp_param + " -timeout "+str(SIPP_TIMEOUT)+"s -timeout_error"
    fullcmd = os.path.normpath(SIPP_PATH) + " " + sipp_param
    print "Running SIPP: " + fullcmd
    if SIPP_BG_MODE:
	sipp_proc = subprocess.Popen(fullcmd, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=False)
    else:
	sipp_proc = subprocess.Popen(fullcmd)

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
	sipp.wait()
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

    # If there is no PJSUA EXPECT scenario, must keep polling PJSUA stdout
    # otherwise PJSUA process may stuck (due to stdout pipe buffer full?)
    # Ideally the poll should be done contiunously until SIPp process is
    # terminated.
    if len(PJSUA_EXPECTS)==0:
	import inc_const
	ua[0].expect(inc_const.STDOUT_REFRESH, raise_on_error = False)
	return ""

    ua_err_st = ""
    while len(PJSUA_EXPECTS):
	expect = PJSUA_EXPECTS.pop(0)
	ua_idx = expect[0]
	expect_st = expect[1]
	send_cmd = expect[2]
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

    return ua_err_st


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
        raise TestError("SIPp returned error " + str(sipp_ret_code))


# Here where it all comes together
test = TestParam(SIPP_SCEN_XML[:-4],
		 PJSUA_INST_PARAM,
		 TEST_FUNC)
