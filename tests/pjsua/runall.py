# $Id$
import os
import sys
import time
import re
import shutil

PYTHON = os.path.basename(sys.executable)

# Usage:
#  runall.py [test-to-resume]


# Initialize test list
tests = []

# Excluded tests (because they fail?)
excluded_tests = [
    "svn",
    "pyc",
    "scripts-call/150_srtp_2_1",                     # SRTP optional 'cannot' call SRTP mandatory
    "scripts-call/150_srtp_2_3.py",                  # disabled because #1267 wontfix
    "scripts-call/301_ice_public_a.py",              # Unreliable, proxy returns 408 sometimes
    "scripts-call/301_ice_public_b.py",              # Doesn't work because OpenSER modifies SDP
    "scripts-pres/200_publish.py",                   # Ok from cmdline, error from runall.py
    "scripts-media-playrec/100_resample_lf_8_11.py", # related to clock-rate 11 kHz problem
    "scripts-media-playrec/100_resample_lf_8_22.py", # related to clock-rate 22 kHz problem
    "scripts-media-playrec/100_resample_lf_11",      # related to clock-rate 11 kHz problem
    "pesq",                                          # temporarily disabling all pesq related test due to unreliability
    # TODO check all tests below for false negatives
    "pjmedia-test",
    "pjsip-test",
    "call_305_ice_comp_1_2",
    "scripts-sendto/155_err_sdp_bad_syntax",
    "transfer-attended",
    "uac-inv-and-ack-without-sdp",
    "uac-subscribe",
    "uac-ticket-1148",
    "uas-422-then-200-bad-se",
    "uas-answer-180-multiple-fmts-support-update",
    "uas-inv-answered-with-srtp",
    "uas-mwi-0",
    "uas-mwi",
    "uas-register-ip-change-port-only",
    "uas-register-ip-change",
    "uas-timer-update"
]

# Add basic tests
for f in os.listdir("scripts-run"):
    tests.append("mod_run.py scripts-run/" + f)

# Add basic call tests
for f in os.listdir("scripts-call"):
    tests.append("mod_call.py scripts-call/" + f)

# Add presence tests
for f in os.listdir("scripts-pres"):
    tests.append("mod_pres.py scripts-pres/" + f)

# Add mod_sendto tests
for f in os.listdir("scripts-sendto"):
    tests.append("mod_sendto.py scripts-sendto/" + f)

# Add mod_media_playrec tests
for f in os.listdir("scripts-media-playrec"):
    tests.append("mod_media_playrec.py scripts-media-playrec/" + f)

# Add mod_pesq tests
for f in os.listdir("scripts-pesq"):
    tests.append("mod_pesq.py scripts-pesq/" + f)

# Add recvfrom tests
for f in os.listdir("scripts-recvfrom"):
    tests.append("mod_recvfrom.py scripts-recvfrom/" + f)

# Add sipp tests
for f in os.listdir("scripts-sipp"):
    if f.endswith(".xml"):
        tests.append("mod_sipp.py scripts-sipp/" + f)


resume_script=""
shell_cmd=""
with_log=True

# Parse arguments
sys.argv.pop(0)
while len(sys.argv):
    if sys.argv[0]=='/h' or sys.argv[0]=='-h' or sys.argv[0]=='--help' or sys.argv[0]=='/help':
        sys.argv.pop(0)
        print "Usage:"
        print "  runall.py [OPTIONS] [run.py-OPTIONS]"
        print "OPTIONS:"
        print "  --list"
        print "    List the tests"
        print "  --list-xml"
        print "    List the tests as XML format suitable for ccdash"
        print "  --resume,-r RESUME"
        print "    RESUME is string/substring to specify where to resume tests."
        print "    If this argument is omited, tests will start from the beginning."
        print "  --disable,-d TEST_NAME"
        print "    Disable a specific test that contains the specified TEST_NAME."
        print "  --shell,-s SHELL"
        print "    Run the tests with the specified SHELL cmd. This can also be"
        print "    used to run the test with ccdash. Example:"
        print "    --shell '/bin/sh -c'"
        print "  --no-log"
        print "    Do not generate log files. By default log files will be generated"
        print "    and put in 'logs' dir."
        print ""
        print "  run.py-OPTIONS are applicable here"
        sys.exit(0)
    elif sys.argv[0] == '-r' or sys.argv[0] == '--resume':
        if len(sys.argv) > 1:
            resume_script=sys.argv[1]
            sys.argv.pop(0)
            sys.argv.pop(0)
        else:
            sys.argv.pop(0)
            sys.stderr.write("Error: argument value required")
            sys.exit(1)
    elif sys.argv[0] == '--list':
        sys.argv.pop(0)
        for t in tests:
              print t
        sys.exit(0)
    elif sys.argv[0] == '--list-xml':
        sys.argv.pop(0)
        for t in tests:
            (mod,param) = t.split(None,2)
            tname = mod[4:mod.find(".py")] + "_" + \
            param[param.find("/")+1:param.rfind(".")]
            c = ""
            if len(sys.argv):
                c = " ".join(sys.argv) + " "
            tcmd = PYTHON + ' run.py ' + c + t
            print '\t\t<Test name="%s" cmd="%s" wdir="tests/pjsua" />' % (tname, tcmd)
        sys.exit(0)
    elif sys.argv[0] == '-s' or sys.argv[0] == '--shell':
        if len(sys.argv) > 1:
            shell_cmd = sys.argv[1]
            sys.argv.pop(0)
            sys.argv.pop(0)
        else:
            sys.argv.pop(0)
            sys.stderr.write("Error: argument value required")
            sys.exit(1)
    elif sys.argv[0] == '-d' or sys.argv[0] == '--disable':
        if len(sys.argv) > 1:
            excluded_tests.append(sys.argv[1])
            sys.argv.pop(0)
            sys.argv.pop(0)
        else:
            sys.argv.pop(0)
            sys.stderr.write("Error: argument value required")
            sys.exit(1)        	
    elif sys.argv[0] == '--no-log':
        sys.argv.pop(0)
        with_log=False
    else:
        # should be run.py options
        break


# Generate arguments for run.py
argv_st = " ".join(sys.argv) + " "

# Init vars
fails_cnt = 0
tests_cnt = 0

# Filter-out excluded tests
for pat in excluded_tests:
    tests = [t for t in tests if t.find(pat)==-1]

# Now run the tests
total_cnt = len(tests)
for t in tests:
    if resume_script!="" and t.find(resume_script)==-1:
        print "Skipping " + t +".."
        total_cnt = total_cnt - 1
        continue
    resume_script=""
    cmdline = "python run.py " + argv_st + t
    if shell_cmd:
        cmdline = "%s '%s'" % (shell_cmd, cmdline)
    t0 = time.time()
    msg = "Running %3d/%d: %s..." % (tests_cnt+1, total_cnt, cmdline)
    sys.stdout.write(msg)
    sys.stdout.flush()
    if with_log:
        logname = re.search(".*\s+(.*)", t).group(1)
        logname = re.sub("[\\\/]", "_", logname)
        logname = re.sub("\.py$", ".log", logname)
        logname = re.sub("\.xml$", ".log", logname)
        logname = "logs/" + logname
    else:
        logname = os.devnull
    ret = os.system(cmdline + " > " + logname)
    t1 = time.time()
    if ret != 0:
        dur = int(t1 - t0)
        print " failed!! [" + str(dur) + "s]"
        if with_log:
            lines = open(logname, "r").readlines()
            print ''.join(lines)
            print "Log file: '" + logname + "'."
        fails_cnt += 1
    else:
        dur = int(t1 - t0)
        print " ok [" + str(dur) + "s]"
    tests_cnt += 1

if fails_cnt == 0:
    print "All " + str(tests_cnt) + " tests completed successfully"
else:
    print str(tests_cnt) + " tests completed, " +  str(fails_cnt) + " test(s) failed"

sys.exit(fails_cnt)
