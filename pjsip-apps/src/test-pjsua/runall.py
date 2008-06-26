# $Id$
import os
import sys
import time

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

# Filter-out excluded tests
for pat in excluded_tests:
    tests = [t for t in tests if t.find(pat)==-1]

# Resume test?
resume_script=""
if len(sys.argv) > 1:
    if sys.argv[1][0]=='-' or sys.argv[1][0]=='/':
        print "Usage:"
	print "  runall.py [RESUME]"
	print "where"
	print "  RESUME is string/substring to specify where to resume tests."
	print "  If this argument is omited, tests will start from the beginning."
	sys.exit(0)
    resume_script=sys.argv[1]


# Now run the tests
for t in tests:
	if resume_script!="" and t.find(resume_script)==-1:
	    print "Skipping " + t +".."
	    continue
	resume_script=""
	cmdline = "python run.py " + t
	t0 = time.time()
	print "Running " + cmdline + "...",
	ret = os.system(cmdline + " > output.log")
	t1 = time.time()
	if ret != 0:
		dur = int(t1 - t0)
		print " failed!! [" + str(dur) + "s]"
		print "Please see 'output.log' for the test log."
		sys.exit(1)
	else:
		dur = int(t1 - t0)
		print " ok [" + str(dur) + "s]"

print "All tests completed successfully"
