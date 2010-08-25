#!/usr/bin/python
import os
import sys
import time
import datetime
import ccdash

INTERVAL = 300
DELAY = 0
CMDLINE = ""

def run_cmdline(group):
	cmdline = CMDLINE + " --group " + group
	return os.system(cmdline)


def usage():
	print """Periodically monitor working directory for Continuous and Nightly builds

Usage:
  run_continuous.py [options] "cmdline"

where:
  cmdline is command to be executed to perform the test. Typically, this is
  a script that calls configure.py and run_scenario.py for each scenario to
  be performed. See perform_test.sh.sample and perform_test.bat.sample for
  sample scripts. Note that the cmdline will be called with added group
  argument (e.g. --group Nightly).

options:
  These are options which will be processed by run_continuous.py:

  --delay MIN   Delay both Continuous and Nightly builds by MIN minutes. 
  		This is useful to coordinate the build with other build 
		machines. By default, Continuous build will be done right
		after changes are detected, and Nightly build will be done
		at 00:00 GMT. MIN is a float number.

"""
	sys.exit(1)

if __name__ == "__main__":
	if len(sys.argv)<=1 or sys.argv[1]=="-h" or sys.argv[1]=="--h" or sys.argv[1]=="--help" or sys.argv[1]=="/h":
		usage()

	# Check args
	i = 1
	while i < len(sys.argv):
		if sys.argv[i]=="--delay":
			i = i + 1
			if i >= len(sys.argv):
				print "Error: missing argument"
				sys.exit(1)
			DELAY = float(sys.argv[i]) * 60
			print "Delay is set to %f minute(s)" % (DELAY / 60)
		else:
			if CMDLINE:
				print "Error: cmdline already specified"
				sys.exit(1)
			CMDLINE = sys.argv[i]
		i = i + 1

	if not CMDLINE:
		print "Error: cmdline is needed"
		sys.exit(1)

	# Current date
	utc = time.gmtime(None)
	day = utc.tm_mday

	# Loop foreva
	while True:
		argv = []

		# Anything changed recently?
		argv.append("ccdash.py")
		argv.append("status")
		argv.append("-w")
		argv.append("../..")
		rc = ccdash.main(argv)

		utc = time.gmtime(None)

		if utc.tm_mday != day or rc != 0:
				group = ""
				if utc.tm_mday != day:
					day = utc.tm_mday
					group = "Nightly"
				elif rc != 0:
					group = "Continuous"
				else:
					group = "Experimental"
				print "Will run %s after %f s.." % (group, DELAY)
				time.sleep(DELAY)
				rc = run_cmdline(group)
				# Sleep even if something does change
				print str(datetime.datetime.now()) + \
					  ": done running " + group + \
					  "tests, will check again in " + str(INTERVAL) + "s.."
				time.sleep(INTERVAL)
		else:
			# Nothing changed
			print str(datetime.datetime.now()) + \
				  ": No update, will check again in " + str(INTERVAL) + "s.."
			time.sleep(INTERVAL)
			

