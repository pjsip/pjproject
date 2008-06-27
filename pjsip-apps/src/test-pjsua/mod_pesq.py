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
import wave
import inc_const as const

from inc_cfg import *

# Load configuration
cfg_file = imp.load_source("cfg_file", sys.argv[2])

# PESQ configs
# PESQ_THRESHOLD specifies the minimum acceptable PESQ MOS value, so test can be declared successful
PESQ = "tools/pesq.exe"
PESQ_DEFAULT_THRESHOLD = 3.4

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

	# Get WAV input length, in seconds
	fin = wave.open(user_data.input_filename, "r")
	if fin == None:
		raise TestError("Failed opening input WAV file")
	inwavlen = fin.getnframes() * 1.0 / fin.getframerate()
	inwavlen += 0.2
	fin.close()
	print "WAV input len = " + str(inwavlen) + "s"

	# Get clock rate of the output
	mo_clock_rate = re.compile("\.(\d+)\.wav").search(user_data.output_filename)
	if (mo_clock_rate==None):
		raise TestError("Cannot compare input & output, incorrect output filename format")
	clock_rate = mo_clock_rate.group(1)
	
	# Get channel count of the output
	channel_count = 1
	if re.search("--stereo", ua2.inst_param.arg) != None:
		channel_count = 2
	
	# Get matched input file from output file
	# (PESQ evaluates only files whose same clock rate & channel count)
	if channel_count == 2:
	    if re.search("\.\d+\.\d+\.wav", user_data.input_filename) != None:
		    user_data.input_filename = re.sub("\.\d+\.\d+\.wav", 
						      "." + str(channel_count) + "."+clock_rate+".wav", user_data.input_filename)
	    else:
		    user_data.input_filename = re.sub("\.\d+\.wav", 
						      "." + str(channel_count) + "."+clock_rate+".wav", user_data.input_filename)

	if (clock_rate != "8") & (clock_rate != "16"):
		raise TestError("PESQ only works on clock rate 8kHz or 16kHz, clock rate used = "+clock_rate+ "kHz")

	# Get conference clock rate of UA2 for PESQ sample rate option
	user_data.pesq_sample_rate_opt = "+" + clock_rate + "000"

	# UA1 making call
	ua1.send("m")
	ua1.send(t.inst_params[1].uri)
	ua1.expect(const.STATE_CALLING)

	# UA2 wait until call established
	ua2.expect(const.STATE_CONFIRMED)

	# Disconnect mic -> rec file, to avoid echo recorded when using sound device
	# Disconnect stream -> spk, make it silent
	# Connect stream -> rec file, start recording
	ua2.send("cd 0 1\ncd 4 0\ncc 4 1")

	# Disconnect mic -> stream, make stream purely sending from file
	# Disconnect stream -> spk, make it silent
	# Connect file -> stream, start sending
	ua1.send("cd 0 4\ncd 4 0\ncc 1 4")

	time.sleep(inwavlen)

	# Disconnect files from bridge
	ua2.send("cd 4 1")
	ua2.expect(const.MEDIA_DISCONN_PORT_SUCCESS)
	ua1.send("cd 1 4")
	ua1.expect(const.MEDIA_DISCONN_PORT_SUCCESS)


# Post body function
def post_func(t, user_data):
	endpt = t.process[0]

	# Execute PESQ
	fullcmd = PESQ + " " + user_data.pesq_sample_rate_opt + " " + user_data.input_filename + " " + user_data.output_filename
	endpt.trace("Popen " + fullcmd)
	pesq_proc = subprocess.Popen(fullcmd, stdout=subprocess.PIPE, universal_newlines=True)
	pesq_out  = pesq_proc.communicate()

	# Parse ouput
	mo_pesq_out = re.compile("Prediction[^=]+=\s+([\-\d\.]+)\s*").search(pesq_out[0])
	if (mo_pesq_out == None):
		raise TestError("Failed to fetch PESQ result")

	# Get threshold
	if (cfg_file.pesq_threshold != None) | (cfg_file.pesq_threshold > -0.5 ):
		threshold = cfg_file.pesq_threshold
	else:
		threshold = PESQ_DEFAULT_THRESHOLD

	# Evaluate the PESQ MOS value
	pesq_res = mo_pesq_out.group(1)
	if (float(pesq_res) >= threshold):
		endpt.trace("Success, PESQ result = " + pesq_res)
	else:
		endpt.trace("Failed, PESQ result = " + pesq_res)
		raise TestError("WAV seems to be degraded badly")


# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func
test.post_func = post_func
test.user_data = mod_pesq_user_data()

