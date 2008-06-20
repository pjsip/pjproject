# $Id$
import os
import sys

# Usage:
#  runall.py [test-to-resume]


# Initialize test list
tests = []

# Excluded tests (because they fail?)
excluded_tests = [ "svn",
		   "pyc",
		   #"scripts-call/150_srtp_1_2",
		   "scripts-call/150_srtp_2_1"
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
	print "Running " + cmdline
	ret = os.system(cmdline + " > output.log")
	if ret != 0:
		print "Test " + t + " failed."
		print "Please see 'output.log' for the test log."
		sys.exit(1)

print "All tests completed successfully"
