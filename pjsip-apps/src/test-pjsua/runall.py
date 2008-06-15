# $Id$
import os
import sys

# Initialize test list
tests = []

# Excluded tests (because they fail?)
excluded_tests = [ "svn",
		   "pyc",
		   "scripts-call/150_srtp_1_2",
		   "scripts-call/150_srtp_2_1",
                   "scripts-call/300_ice_1_1"]

# Add basic tests
for f in os.listdir("scripts-run"):
    tests.append("mod_run.py scripts-run/" + f)

# Add basic call tests
for f in os.listdir("scripts-call"):
    tests.append("mod_call.py scripts-call/" + f)

# Add presence tests
for f in os.listdir("scripts-pres"):
    tests.append("mod_pres.py scripts-pres/" + f)

# Filter-out excluded tests
for pat in excluded_tests:
    tests = [t for t in tests if t.find(pat)==-1]

# Now run the tests
for t in tests:
	cmdline = "python run.py " + t
	print "Running " + cmdline
	ret = os.system(cmdline + " > output.log")
	if ret != 0:
		print "Test " + t + " failed."
		print "Please see 'output.log' for the test log."
		sys.exit(1)

print "All tests completed successfully"
