#!/usr/bin/python
import os
import sys
import time
import datetime
import ccdash

INTERVAL = 300
DELAY = 0

def run_scenarios(scenarios, group):
	# Run each scenario
	rc = 0
	for scenario in scenarios:
		argv = []
		argv.append("ccdash.py")
		argv.append("scenario")
		argv.append(scenario)
		argv.append("--group")
		argv.append(group)
		thisrc = ccdash.main(argv)
		if rc==0 and thisrc:
			rc = thisrc
	return rc


def usage():
	print """Periodically monitor working directory for Continuous and Nightly builds

Usage:
  run_continuous.py [options] scenario1.xml [scenario2.xml ...]

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

	# Splice list
	scenarios = []
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
			# Check if scenario exists
			scenario = sys.argv[i]
			if not os.path.exists(scenario):
				print "Error: file " + scenario + " does not exist"
				sys.exit(1)
			scenario.append(scenario)
			print "Scenario %s added" % (scenario)
		i = i + 1

	if len(scenarios) < 1:
		print "Error: scenario is required"
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
				rc = run_scenarios(scenarios, group)
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
			

