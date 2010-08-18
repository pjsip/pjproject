#!/usr/bin/python
import os
import sys
import time
import datetime
import ccdash

INTERVAL = 300

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


if __name__ == "__main__":
	if len(sys.argv)<=1 or sys.argv[1]=="-h" or sys.argv[1]=="--h" or sys.argv[1]=="/h":
		print "This will run both Continuous and Nightly tests"
		print ""
		print "Usage: run_continuous.py scenario1.xml [scenario2.xml ...]"
		print ""
		sys.exit(1)

	# Splice list
	scenarios = sys.argv[1:]

	# Check if scenario exists
	for scenario in scenarios:
		if not os.path.exists(scenario):
			print "Error: file " + scenario + " does not exist"
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
			

