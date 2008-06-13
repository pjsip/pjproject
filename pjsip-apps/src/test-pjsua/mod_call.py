# $Id:$
import time
import imp
import sys
import inc_param as param
import inc_const as const

# Load configuration
cfg_file = imp.load_source("cfg_file", sys.argv[2])

# Test title
title = cfg_file.config.title
port1 = "9060"

# First pjsua
p1 = param.Pjsua(
		"callee",
		args = cfg_file.config.callee_cfg.arg + " --local-port="+port1,
		echo = cfg_file.config.callee_cfg.echo_enabled,
		trace = cfg_file.config.callee_cfg.trace_enabled
		)

# Second pjsua, make call to the first one
p2 = param.Pjsua(
		"caller",
		args = cfg_file.config.caller_cfg.arg + " --local-port=0",
		echo = cfg_file.config.caller_cfg.echo_enabled,
		trace = cfg_file.config.caller_cfg.trace_enabled
		)

# Test body function
def test_func(t):
	callee = t.process[0]
	caller = t.process[1]

	# Caller making call
	caller.send("m")
	caller.send("sip:localhost:" + port1 + cfg_file.config.uri_param)
	caller.expect(const.STATE_CALLING)
	
	# Callee answers with 200/OK
	time.sleep(1)
	callee.expect(const.EVENT_INCOMING_CALL)
	callee.send("a")
	callee.send("200")

	# Wait until call is connected in both endpoints
	time.sleep(1)
	if callee.expect(const.STATE_CONFIRMED, False)==None:
		raise TestError("Call failed")
	caller.expect(const.STATE_CONFIRMED)

	# Synchronize stdout
	caller.send("echo 1")
	caller.expect("echo 1")
	callee.send("echo 1")
	callee.expect("echo 1")

	# Test that media is okay (with RFC 2833 DTMF)
	time.sleep(2)
	caller.send("#")
	caller.send("1122")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "2")
	callee.expect(const.RX_DTMF + "2")

	# Hold call
	caller.send("H")
	caller.expect(const.MEDIA_HOLD)
	callee.expect(const.MEDIA_HOLD)
	
	# Release hold
	time.sleep(2)
	caller.send("v")
	caller.expect(const.MEDIA_ACTIVE)
	callee.expect(const.MEDIA_ACTIVE)

	# Synchronize stdout
	caller.send("echo 1")
	caller.expect("echo 1")
	callee.send("echo 1")
	callee.expect("echo 1")

	# Test that media is okay (with RFC 2833 DTMF)
	caller.send("#")
	caller.send("1122")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "2")
	callee.expect(const.RX_DTMF + "2")

	# Synchronize stdout
	caller.send("echo 1")
	caller.expect("echo 1")
	callee.send("echo 1")
	callee.expect("echo 1")

	# UPDATE (by caller)
	caller.send("U")
	caller.expect(const.MEDIA_ACTIVE)
	callee.expect(const.MEDIA_ACTIVE)
	
	# Synchronize stdout
	caller.send("echo 1")
	caller.expect("echo 1")
	callee.send("echo 1")
	callee.expect("echo 1")

	# Test that media is okay (with RFC 2833 DTMF)
	time.sleep(2)
	caller.send("#")
	caller.send("1122")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "2")
	callee.expect(const.RX_DTMF + "2")

	# UPDATE (by callee)
	callee.send("U")
	callee.expect(const.MEDIA_ACTIVE)
	caller.expect(const.MEDIA_ACTIVE)
	
	# Synchronize stdout
	caller.send("echo 1")
	caller.expect("echo 1")
	callee.send("echo 1")
	callee.expect("echo 1")

	# Test that media is okay (with RFC 2833 DTMF)
	time.sleep(2)
	caller.send("#")
	caller.send("1122")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "1")
	callee.expect(const.RX_DTMF + "2")
	callee.expect(const.RX_DTMF + "2")

	# Hangup call
	time.sleep(1)
	caller.send("h")

	# Wait until calls are cleared in both endpoints
	caller.expect(const.STATE_DISCONNECTED)
	callee.expect(const.STATE_DISCONNECTED)
	

# Here where it all comes together
test = param.Test(title, run=[p1, p2], func=test_func)


