
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
import os
import sys
import re
import subprocess
import wave
import shutil
import inc_const as const
import inc_util as util
import numpy as np

from inc_cfg import *
from pesq import pesq

# Load configuration
cfg_file = util.load_module_from_file("cfg_file", ARGS[1])

# PESQ configs
PESQ = "tools/pesq"				# PESQ executable path
VISQOL = "tools/visqol"         # Visqol executable path
PESQ_DEFAULT_THRESHOLD = 3.4    # Default minimum acceptable PESQ MOS value
USE_PYTHON_PESQ = 1             # Use python's pesq
ENABLE_VISQOL = 1               # Enable Visqol
VISQOL_DEFAULT_THRESHOLD = 4    # Default minimum acceptable Visqol MOS value
VISQOL_MODEL_FILE = "tools/model/mod.tflite" # Visqol model file

# PESQ params
pesq_sample_rate_opt = ""		# Sample rate option for PESQ
input_filename    = ""			# Input/Reference filename
output_filename = ""			# Output/Degraded filename


# Test body function
def test_func(t):
    global pesq_sample_rate_opt
    global input_filename
    global output_filename
    global ENABLE_VISQOL

    ua1 = t.process[0]
    ua2 = t.process[1]

    # Get input file name
    input_filename = re.compile(const.MEDIA_PLAY_FILE).search(ua1.inst_param.arg).group(1)

    # Get output file name
    output_filename = re.compile(const.MEDIA_REC_FILE).search(ua2.inst_param.arg).group(1)

    # Get WAV input length, in seconds
    fin = wave.open(input_filename, "r")
    if fin == None:
        raise TestError("Failed opening input WAV file")
    inwavlen = fin.getnframes() * 1.0 / fin.getframerate()
    inwavlen += 0.2
    fin.close()
    print("WAV input len = " + str(inwavlen) + "s")

    # Get clock rate of the output
    mo_clock_rate = re.compile("\.(\d+)\.wav").search(output_filename)
    if (mo_clock_rate is None):
        raise TestError("Cannot compare input & output, incorrect output filename format")
    clock_rate = mo_clock_rate.group(1)
    # Get channel count of the output
    channel_count = 1
    if re.search("--stereo", ua2.inst_param.arg) is not None:
        channel_count = 2
    # Get matched input file from output file
    # (PESQ evaluates only files whose same clock rate & channel count)
    if channel_count == 2:
        if re.search("\.\d+\.\d+\.wav", input_filename) != None:
            input_filename = re.sub("\.\d+\.\d+\.wav", "." + str(channel_count) + "."+clock_rate+".wav", input_filename)
        else:
            input_filename = re.sub("\.\d+\.wav", "." + str(channel_count) + "."+clock_rate+".wav", input_filename)

    if (clock_rate != "8") and (clock_rate != "16"):
        raise TestError("PESQ only works on clock rate 8kHz or 16kHz, clock rate used = "+clock_rate+ "kHz")

    if (ENABLE_VISQOL == 1) and (clock_rate != "16"):
        print("VISQOL only works on clock rate 16kHz, clock rate used = "+ clock_rate +"kHz, disabling VISQOL")
        ENABLE_VISQOL = 0

    # Get conference clock rate of UA2 for PESQ sample rate option
    pesq_sample_rate_opt = "+" + clock_rate + "000"

    # UA1 making call
    if ua1.use_telnet:
        ua1.send("call new " + t.inst_params[1].uri)
    else:
        ua1.send("m")
        ua1.send(t.inst_params[1].uri)
    ua1.expect(const.STATE_CALLING)

    # UA2 wait until call established
    ua2.expect(const.STATE_CONFIRMED)

    ua1.sync_stdout()
    ua2.sync_stdout()
    time.sleep(2)

    # Disconnect mic -> rec file, to avoid echo recorded when using sound device
    # Disconnect stream -> spk, make it silent
    # Connect stream -> rec file, start recording
    ua2.send("cd 0 1")
    ua2.send("cd 4 0")
    ua2.send("cc 4 1")

    # Disconnect mic -> stream, make stream purely sending from file
    # Disconnect stream -> spk, make it silent
    # Connect file -> stream, start sending
    ua1.send("cd 0 4")
    ua1.send("cd 4 0")
    ua1.send("cc 1 4")

    time.sleep(inwavlen)

    # Disconnect files from bridge
    ua2.send("cd 4 1")
    ua2.expect(const.MEDIA_DISCONN_PORT_SUCCESS)
    ua1.send("cd 1 4")
    ua1.expect(const.MEDIA_DISCONN_PORT_SUCCESS)

def run_visqol(input_filename, output_filename):
    visqol_bin = os.path.abspath(VISQOL)
    model_file = os.path.abspath(VISQOL_MODEL_FILE)

    cmd = [
        visqol_bin,
        "--similarity_to_quality_model", model_file,
        "--use_speech_mode",
        "--reference_file", input_filename,
        "--degraded_file", output_filename
    ]

    try:
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True
        )
        stdout, stderr = process.communicate()

        if process.returncode != 0:
            raise Exception(f"ViSQOL failed with exit code {process.returncode}: {stderr}")

        match = re.search(r"MOS-LQO:\s+([\d\.]+)", stdout)

        if not match:
            print(f"DEBUG OUTPUT: {stdout}")
            raise Exception("Failed to parse MOS-LQO from ViSQOL output")

        return float(match.group(1))

    except Exception as e:
        print("Visqol Error: " + str(e))
        raise

def read_wav_to_array(filename):
    with wave.open(filename, 'rb') as wf:
        rate = wf.getframerate()
        if rate != 8000 and rate != 16000:
            raise TestError("PESQ only works on clock rate 8kHz or 16kHz, clock rate used = "+str(rate)+ "kHz")

        # Get raw bytes and convert to 16-bit integers
        frames = wf.readframes(wf.getnframes())
        audio_array = np.frombuffer(frames, dtype=np.int16)

        return rate, audio_array


# Post body function
def post_func(t):
    global pesq_sample_rate_opt
    global input_filename
    global output_filename
    pesq_test_res = 0
    visqol_test_res = 0

    endpt = t.process[0]

    # Get threshold
    if (cfg_file.pesq_threshold is not None and cfg_file.pesq_threshold > -0.5 ):
        pesq_threshold = cfg_file.pesq_threshold
    else:
        pesq_threshold = PESQ_DEFAULT_THRESHOLD

    if (USE_PYTHON_PESQ == 1):
        # Evaluate the PESQ MOS value
        in_rate, in_data = read_wav_to_array(input_filename)
        out_rate, out_data = read_wav_to_array(output_filename)
        if (in_rate != out_rate):
            raise TestError("Different rate from input file = "+ in_rate + " and output file = " + out_rate)

        if in_rate == 8000:
            mode='nb'
        else:
            mode='wb'

        pesq_res = pesq(in_rate, in_data, out_data, mode)
    else:
        # Execute PESQ
        fullcmd = os.path.normpath(PESQ) + " " + pesq_sample_rate_opt + " " + input_filename + " " + output_filename
        endpt.trace("Popen " + fullcmd)
        pesq_proc = subprocess.Popen(fullcmd, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
        pesq_out  = pesq_proc.communicate()

        # Parse output
        mo_pesq_out = re.compile("Prediction[^=]+=\s+([\-\d\.]+)\s*").search(pesq_out[0])
        if (mo_pesq_out is None):
            raise TestError("Failed to fetch PESQ result")

        pesq_res = mo_pesq_out.group(1)

    if (float(pesq_res) >= pesq_threshold):
        endpt.trace("Success, PESQ result = " + str(pesq_res) + " (target=" + str(pesq_threshold) + ").")
        pesq_test_res = 1
    else:
        endpt.trace("Failed, PESQ result = " + str(pesq_res) + " (target=" + str(pesq_threshold) + ").")

    if (ENABLE_VISQOL == 1):
        visqol_threshold = getattr(cfg_file, 'visqol_threshold', None)
        if (visqol_threshold is None) or (visqol_threshold <= 0 ):
            visqol_threshold = VISQOL_DEFAULT_THRESHOLD

        visqol_res = run_visqol(input_filename, output_filename)

        if (float(visqol_res) >= visqol_threshold):
            endpt.trace("Success, VISQOL result = " + str(visqol_res) + " (target=" + str(visqol_threshold) + ").")
            visqol_test_res = 1
        else:
            endpt.trace("Failed, VISQOL result = " + str(visqol_res) + " (target=" + str(visqol_threshold) + ").")

    if (pesq_test_res == 0 and visqol_test_res == 0):
        # Save the wav file
        wavoutname = ARGS[1]
        wavoutname = re.sub("[\\\/]", "_", wavoutname)
        wavoutname = re.sub("\.py$", ".wav", wavoutname)
        wavoutname = "logs/" + wavoutname
        try:
            shutil.copyfile(output_filename, wavoutname)
            print("Output WAV is copied to " + wavoutname)
        except:
            print("Couldn't copy output WAV, please check if 'logs' directory exists.")

        err_str = "WAV seems to be degraded badly, PESQ = "+ str(pesq_res) + " (target=" + str(pesq_threshold) + ")."
        if (ENABLE_VISQOL == 1):
            err_str += ", VISQOL = " + str(visqol_res) + " (target=" + str(visqol_threshold) + ")."

        raise TestError(err_str)


# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func
test.post_func = post_func
