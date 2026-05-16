import time
import sys
import os
import inc_const as const
import inc_util as util
from inc_cfg import *

# Load configuration
cfg_file = util.load_module_from_file("cfg_file", ARGS[1])

# Get test script name to determine which test to run
test_script_name = os.path.basename(ARGS[1])


##############################################################################
# Helper function: Basic playwav start/stop test
##############################################################################
def run_600_playwav_basic(t):
    callee = t.process[0]
    caller = t.process[1]

    if caller.use_telnet:
        caller.send("call new " + t.inst_params[0].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee.expect(const.EVENT_INCOMING_CALL)
    if callee.use_telnet:
        callee.send("call answer 200")
    else:
        callee.send("a")
        callee.send("200")

    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Start playing wav file on caller side to call 0
    if caller.use_telnet:
        caller.send("call playwav start 0 wavs/input.44.wav")
    caller.expect("Playback started")

    time.sleep(1)

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav stop 0")
    caller.expect("Playback stopped")

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


##############################################################################
# Helper function: Playwav with multiple calls
##############################################################################
def run_601_playwav_multi(t):
    callee1 = t.process[0]
    callee2 = t.process[1]
    caller = t.process[2]

    # Caller makes first call to callee1
    if caller.use_telnet:
        caller.send("call new " + t.inst_params[0].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee1.expect(const.EVENT_INCOMING_CALL)
    if callee1.use_telnet:
        callee1.send("call answer 200")
    else:
        callee1.send("a")
        callee1.send("200")

    caller.expect(const.STATE_CONFIRMED)
    callee1.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee1.sync_stdout()

    # Caller makes second call to callee2
    if caller.use_telnet:
        caller.send("call new " + t.inst_params[1].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[1].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee2.expect(const.EVENT_INCOMING_CALL)
    if callee2.use_telnet:
        callee2.send("call answer 200")
    else:
        callee2.send("a")
        callee2.send("200")

    caller.expect(const.STATE_CONFIRMED)
    callee2.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee1.sync_stdout()
    callee2.sync_stdout()

    # Start playing wav file on first call (call-id 0)
    if caller.use_telnet:
        caller.send("call playwav start 0 wavs/input.44.wav")
    caller.expect("Playback started")

    time.sleep(1)

    caller.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav stop 0")
    caller.expect("Playback stopped")

    caller.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav start 1 wavs/input.44.wav")
    caller.expect("Playback started")

    time.sleep(1)

    caller.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav stop 1")
    caller.expect("Playback stopped")

    caller.sync_stdout()
    callee1.sync_stdout()
    callee2.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee2.expect(const.STATE_DISCONNECTED)

    caller.sync_stdout()
    callee1.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee1.expect(const.STATE_DISCONNECTED)


##############################################################################
# Helper function: Playwav with call-id -1
##############################################################################
def run_602_playwav_minus1(t):
    callee = t.process[0]
    caller = t.process[1]

    if caller.use_telnet:
        caller.send("call new " + t.inst_params[0].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee.expect(const.EVENT_INCOMING_CALL)
    if callee.use_telnet:
        callee.send("call answer 200")
    else:
        callee.send("a")
        callee.send("200")

    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Start playing wav file with call-id -1 (should use current call)
    if callee.use_telnet:
        callee.send("call playwav start -1 wavs/input.44.wav")
    callee.expect("Playback started")

    time.sleep(1)

    caller.sync_stdout()
    callee.sync_stdout()

    if callee.use_telnet:
        callee.send("call playwav stop -1")
    callee.expect("Playback stopped")

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Test queueing behavior: start playback before call is established
    # Start playback with call-id -1 when no call is active (should queue)
    if caller.use_telnet:
        caller.send("call playwav start -1 wavs/input.44.wav")
    caller.expect("No active call, playback queued for call setup")

    caller.sync_stdout()

    # Now make a new call - playback should start automatically
    if caller.use_telnet:
        caller.send("call new " + t.inst_params[0].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee.expect(const.EVENT_INCOMING_CALL)
    if callee.use_telnet:
        callee.send("call answer 200")
    else:
        callee.send("a")
        callee.send("200")

    caller.expect("Dynamic playback auto-started")
    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    time.sleep(1)

    if caller.use_telnet:
        caller.send("call playwav stop -1")
    caller.expect("Playback stopped")

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


##############################################################################
# Helper function: Basic recwav start/stop test
##############################################################################
def run_603_recwav_basic(t):
    callee = t.process[0]
    caller = t.process[1]

    if caller.use_telnet:
        caller.send("call new " + t.inst_params[0].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee.expect(const.EVENT_INCOMING_CALL)
    if callee.use_telnet:
        callee.send("call answer 200")
    else:
        callee.send("a")
        callee.send("200")

    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav start 0 wavs/input.44.wav")

    # Start recording on callee side
    if callee.use_telnet:
        callee.send("call recwav start 0 wavs/temp.44.wav")
    callee.expect("Recording started")

    time.sleep(2)

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav stop 0")

    if callee.use_telnet:
        callee.send("call recwav stop 0")
    callee.expect("Recording stopped")

    caller.sync_stdout()
    callee.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


##############################################################################
# Helper function: Error handling test
##############################################################################
def run_604_error_handling(t):
    callee = t.process[0]
    caller = t.process[1]

    caller.trace("Test 1: Invalid call-id (no call exists)")
    if caller.use_telnet:
        caller.send("call playwav start 0 wavs/input.44.wav")

    # Should get an error message
    time.sleep(0.5)
    caller.expect("is not active")

    caller.sync_stdout()

    caller.trace("Test 2: Non-existent file")
    if caller.use_telnet:
        caller.send("call new " + t.inst_params[0].uri)
    else:
        caller.send("m")
        caller.send(t.inst_params[0].uri)
    caller.expect(const.STATE_CALLING)

    time.sleep(0.2)
    callee.expect(const.EVENT_INCOMING_CALL)
    if callee.use_telnet:
        callee.send("call answer 200")
    else:
        callee.send("a")
        callee.send("200")

    caller.expect(const.STATE_CONFIRMED)
    callee.expect(const.STATE_CONFIRMED)

    caller.sync_stdout()
    callee.sync_stdout()

    # Try to play non-existent file
    if caller.use_telnet:
        caller.send("call playwav start 0 /nonexistent/file.wav")

    time.sleep(0.5)
    caller.expect("Failed to create player")

    caller.sync_stdout()

    caller.trace("Test 3: Invalid call-id (out of range)")
    if caller.use_telnet:
        caller.send("call playwav start 99 wavs/input.44.wav")

    time.sleep(0.5)
    caller.expect("Invalid call_id")

    caller.sync_stdout()

    # Test 4: Successfully play a file (to verify system still works)
    caller.trace("Test 4: Valid playback")
    if caller.use_telnet:
        caller.send("call playwav start 0 wavs/input.44.wav")

    time.sleep(1)

    caller.sync_stdout()

    if caller.use_telnet:
        caller.send("call playwav stop 0")

    caller.sync_stdout()
    callee.sync_stdout()

    # Test 5: Try to stop playback when none is active
    caller.trace("Test 5: Stop when no playback active")
    if caller.use_telnet:
        caller.send("call playwav stop 0")

    time.sleep(0.5)
    caller.expect("No active playback")

    caller.sync_stdout()

    if caller.use_telnet:
        caller.send("call hangup")
    else:
        caller.send("h")

    caller.expect(const.STATE_DISCONNECTED)
    callee.expect(const.STATE_DISCONNECTED)


##############################################################################
# Main test function - dispatches to appropriate helper based on script name
##############################################################################
def test_func(t):
    if "600_playwav_basic" in test_script_name:
        run_600_playwav_basic(t)
    elif "601_playwav_multi_call" in test_script_name:
        run_601_playwav_multi(t)
    elif "602_playwav_callid_minus1" in test_script_name:
        run_602_playwav_minus1(t)
    elif "603_recwav_basic" in test_script_name:
        run_603_recwav_basic(t)
    elif "604_playwav_error_handling" in test_script_name:
        run_604_error_handling(t)
    else:
        raise TestError("Unknown test script: " + test_script_name)


# Post function for test 603
def post_func(t):
    if "603_recwav_basic" in test_script_name:
        # Check if the recorded file exists
        output_file = "wavs/temp.44.wav"
        if os.path.exists(output_file):
            file_size = os.path.getsize(output_file)
            if file_size > 0:
                t.process[0].trace("Recording successful, file size: " + str(file_size) + " bytes")
                os.remove(output_file)
            else:
                raise TestError("Recording file is empty")
        else:
            raise TestError("Recording file was not created")


# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func
test.post_func = post_func
