# $Id$

# Quality test of media calls.
# - UA1 calls UA2
# - UA1 plays a file until finished to be streamed to UA2
# - UA2 records from stream
# - Apply PESQ to played file (reference) and recorded file (degraded)
#
# File should be:
# - naming: xxxxxx.CLOCK_RATE.wav, e.g: test1.8.wav
# - clock-rate of those files can only be 8khz or 16khz

import time
import imp
import sys
import re
import subprocess
import inc_const as const

from inc_cfg import *

# Load configuration
cfg_file = imp.load_source("cfg_file", sys.argv[2])

# PESQ configs
# PESQ_THRESHOLD specifies the minimum acceptable PESQ MOS value, so test can be declared successful
PESQ = "tools/pesq.exe"
PESQ_THRESHOLD = 1.0

# UserData
class mod_pesq_user_data:
	# Sample rate option for PESQ
	pesq_sample_rate_opt = ""
	# Input/Reference filename
	input_filename = ""
	# Output/Degraded filename
	output_filename = ""

# Test body function
def test_func(t, user_data):
	ua1 = t.process[0]
	ua2 = t.process[1]

	# Get input file name
	user_data.input_filename = re.compile(const.MEDIA_PLAY_FILE).search(ua1.inst_param.arg).group(1)

	# Get output file name
	user_data.output_filename = re.compile(const.MEDIA_REC_FILE).search(ua2.inst_param.arg).group(1)

	# Find appropriate clock rate for the input file
	mo_clock_rate = re.compile("\.(\d+)\.wav").search(user_data.output_filename)
	if (mo_clock_rate==None):
		raise TestError("Cannot compare input & output, incorrect output filename format")
	clock_rate = mo_clock_rate.group(1)
	user_data.input_filename = re.sub("\.\d+\.wav", "."+clock_rate+".wav", user_data.input_filename)

	if (clock_rate != "8") & (clock_rate != "16"):
		raise TestError("PESQ only works on clock rate 8kHz or 16kHz, clock rate used = "+clock_rate+ "kHz")

	# Get conference clock rate of UA2 for PESQ sample rate option
	user_data.pesq_sample_rate_opt = "+" + clock_rate + "000"

	# UA1 making call
	ua1.send("m")
	ua1.send(t.inst_params[1].uri)
	ua1.expect(const.STATE_CALLING)
	ua2.expect(const.STATE_CONFIRMED)

	# Disconnect mic -> rec file to avoid echo recorded when using sound device
	ua2.send("cd 0 1")

	# Auto answer, auto play, auto hangup
	# Just wait for call disconnected
	# Assumed WAV input is no more than 30 secs
	while 1:
		line = ua2.proc.stdout.readline()
		if line == "":
			raise TestError(ua2.name + ": Premature EOF")

		# Search for disconnected text
		if re.search(const.STATE_DISCONNECTED, line) != None:
			break
	

# Post body function
def post_func(t, user_data):
	endpt = t.process[0]

	# Execute PESQ
	fullcmd = PESQ + " " + user_data.pesq_sample_rate_opt + " " + user_data.input_filename + " " + user_data.output_filename
	endpt.trace("Popen " + fullcmd)
	pesq_proc = subprocess.Popen(fullcmd, stdout=subprocess.PIPE, universal_newlines=True)
	pesq_out  = pesq_proc.communicate()

	# Parse ouput
	mo_pesq_out = re.compile("Prediction\s+:\s+PESQ_MOS\s+=\s+(.+)\s*").search(pesq_out[0])
	if (mo_pesq_out == None):
		raise TestError("Failed to fetch PESQ result")

	# Evaluate the similarity value
	pesq_res = mo_pesq_out.group(1)
	if (float(pesq_res) >= PESQ_THRESHOLD):
		endpt.trace("Success, PESQ result=" + pesq_res)
	else:
		endpt.trace("Failed, PESQ result=" + pesq_res)
		raise TestError("WAV seems to be degraded badly")


# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func
test.post_func = post_func
test.user_data = mod_pesq_user_data()

