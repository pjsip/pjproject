# $Id$
import os
import sys
import time
import re
import shutil


# Usage:
#  runall.py [test-to-resume]


# Initialize test list
tests = []

# Excluded tests (because they fail?)
excluded_tests = [ "svn",
		   "pyc",
		   "scripts-call/150_srtp_2_1",				# SRTP optional 'cannot' call SRTP mandatory
		   "scripts-call/301_ice_public_a.py",			# Unreliable, proxy returns 408 sometimes
		   "scripts-call/301_ice_public_b.py",			# Doesn't work because OpenSER modifies SDP
		   "scripts-pres/200_publish.py",			# Ok from cmdline, error from runall.py
		   "scripts-media-playrec/100_resample_lf_8_11.py",	# related to clock-rate 11 kHz problem
		   "scripts-media-playrec/100_resample_lf_8_22.py",	# related to clock-rate 22 kHz problem
		   "scripts-media-playrec/100_resample_lf_11"		# related to clock-rate 11 kHz problem
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

# Filter-out excluded tests
for pat in excluded_tests:
    tests = [t for t in tests if t.find(pat)==-1]

# Resume test?
resume_script=""
if len(sys.argv) > 1:
    if sys.argv[1]=='-r' or sys.argv[1]=='--resume':
	resume_script=sys.argv[2]
    if sys.argv[1]=='/h' or sys.argv[1]=='-h' or sys.argv[1]=='--help' or sys.argv[1]=='/help':
        print "Usage:"
	print "  runall.py [OPTIONS] [run.py-OPTIONS]"
	print "Options:"
	print "  --resume,-r RESUME"
	print "      RESUME is string/substring to specify where to resume tests."
	print "      If this argument is omited, tests will start from the beginning."
	print "  run.py-OPTIONS are applicable here"
	sys.exit(0)


# Generate arguments for run.py
argv = sys.argv
argv_to_skip = 1
if resume_script != "":
    argv_to_skip += 2
argv_st = ""
for a in argv:
    if argv_to_skip > 0:
        argv_to_skip -= 1
    else:
        argv_st += a + " "


# Init vars
fails_cnt = 0
tests_cnt = 0

# Re-create "logs" directory
try:
    shutil.rmtree("logs")
except:
    print "Warning: failed in removing directory 'logs'"

try:
    os.mkdir("logs")
except:
    print "Warning: failed in creating directory 'logs'"

# Now run the tests
for t in tests:
	if resume_script!="" and t.find(resume_script)==-1:
	    print "Skipping " + t +".."
	    continue
	resume_script=""
	cmdline = "python run.py " + argv_st + t
	t0 = time.time()
	print "Running " + cmdline + "...",
	ret = os.system(cmdline + " > output.log")
	t1 = time.time()
	if ret != 0:
		dur = int(t1 - t0)
		print " failed!! [" + str(dur) + "s]"
		logname = re.search(".*\s+(.*)", t).group(1)
		logname = re.sub("[\\\/]", "_", logname)
		logname = re.sub("\.py$", ".log", logname)
		logname = "logs/" + logname
		shutil.move("output.log", logname)
		print "Please see '" + logname + "' for the test log."
		fails_cnt += 1
	else:
		dur = int(t1 - t0)
		print " ok [" + str(dur) + "s]"
	tests_cnt += 1

if fails_cnt == 0:
	print "All " + str(tests_cnt) + " tests completed successfully"
else:
	print str(tests_cnt) + " tests completed, " +  str(fails_cnt) + " test(s) failed"

