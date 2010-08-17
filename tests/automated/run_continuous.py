#!/usr/bin/python
import os
import sys
import time
import datetime
import ccdash

GROUP = "Continuous"
INTERVAL = 300

if __name__ == "__main__":
	if len(sys.argv)<=1 or sys.argv[1]=="-h" or sys.argv[1]=="--h" or sys.argv[1]=="/h":
		print "Usage: run_continuous.py scenario1.xml [scenario2.xml ...]"
		sys.exit(0)

	# Splice list
	scenarios = sys.argv[1:]

	# Check if scenario exists
	for scenario in scenarios:
		if not os.path.exists(scenario):
			print "Error: file " + scenario + " does not exist"
			sys.exit(1)

	# Loop foreva
	while True:
		argv = []

		# Anything changed recently?
		argv.append("ccdash.py")
		argv.append("status")
		argv.append("-w")
		argv.append("../..")
		rc = ccdash.main(argv)

		if rc==0:
			# Nothing changed
			print str(datetime.datetime.now()) + ": No update, will check again in " + str(INTERVAL) + "s.."
			time.sleep(INTERVAL)
			continue
			
		# Run each scenario
		for scenario in scenarios:
			argv = []
			argv.append("ccdash.py")
			argv.append("scenario")
			argv.append(scenario)
			argv.append("--group")
			argv.append(GROUP)
			thisrc = ccdash.main(argv)
			if rc==0 and thisrc:
				rc = thisrc

		# Sleep even if something does change
		print str(datetime.datetime.now()) + ": done, will check again in " + str(INTERVAL) + "s.."
		time.sleep(INTERVAL)

