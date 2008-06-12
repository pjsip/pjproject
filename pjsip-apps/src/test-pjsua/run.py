# $Id$
import sys
import imp
import re
import subprocess
import time

import inc_param as param
import inc_const as const

# Defaults
G_ECHO=True
G_TRACE=False
G_EXE="..\\..\\bin\\pjsua_vc6d.exe"

###################################
# TestError exception
class TestError:
	desc = ""
	def __init__(self, desc):
		self.desc = desc

###################################
# Poor man's 'expect'-like class
class Expect:
	proc = None
	echo = False
	trace_enabled = False
	name = ""
	rh = re.compile(const.DESTROYED)
	ra = re.compile(const.ASSERT, re.I)
	rr = re.compile(const.STDOUT_REFRESH)
	def __init__(self, name, exe, args="", echo=G_ECHO, trace_enabled=G_TRACE):
		self.name = name
		self.echo = echo
		self.trace_enabled = trace_enabled
		fullcmd = exe + " " + args + " --stdout-refresh=5 --stdout-refresh-text=" + const.STDOUT_REFRESH
		self.trace("Popen " + fullcmd)
		self.proc = subprocess.Popen(fullcmd, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)
	def send(self, cmd):
		self.trace("send " + cmd)
		self.proc.stdin.writelines(cmd + "\n")
	def expect(self, pattern, raise_on_error=True):
		self.trace("expect " + pattern)
		r = re.compile(pattern, re.I)
		refresh_cnt = 0
		while True:
			line = self.proc.stdout.readline()
		  	if line == "":
				raise TestError("Premature EOF")
			# Print the line if echo is ON
			if self.echo:
				print self.name + ": " + line,
			# Trap assertion error
			if self.ra.search(line) != None:
				raise TestError(line)
			# Count stdout refresh text. 
			if self.rr.search(line) != None:
				refresh_cnt = refresh_cnt+1
				if refresh_cnt >= 6:
					self.trace("Timed-out!")
					if raise_on_error:
						raise TestError("Timeout expecting pattern: " + pattern)
					else:
						return None		# timeout
			# Search for expected text
			if r.search(line) != None:
				return line
	def wait(self):
		self.trace("wait")
		self.proc.wait()
	def trace(self, s):
		if self.trace_enabled:
			print self.name + ": " + "====== " + s + " ======"

#########################
# Error handling
def handle_error(errmsg, t):
	print "====== Caught error: " + errmsg + " ======"
	time.sleep(1)
	for p in t.process:
		p.send("q")
		p.expect(const.DESTROYED)
		p.wait()
	print "Test completed with error: " + errmsg
	sys.exit(1)


#########################
# MAIN	

if len(sys.argv)!=3:
	print "Usage: run.py MODULE CONFIG"
	print "Sample:"
	print "  run.py mod_run.py scripts-run/100_simple.py"
	sys.exit(1)


# Import the test script
script = imp.load_source("script", sys.argv[1])  

# Validate
if script.test == None:
	print "Error: no test defined"
	sys.exit(1)

if len(script.test.run) == 0:
	print "Error: test doesn't contain pjsua run descriptions"
	sys.exit(1)

# Instantiate pjsuas
print "====== Running " + script.test.title + " ======"
for run in script.test.run:
	try:
		p = Expect(run.name, G_EXE, args=run.args, echo=run.echo, trace_enabled=run.trace)
	 	# Wait until initialized
		p.expect(const.PROMPT)
		p.send("echo 1")
		p.send("echo 1")
		# add running instance
		script.test.process.append(p)
		# run initial script
		for cmd in run.cmds:
			if len(cmd) >= 3 and cmd[2]!="":
				print "====== " + cmd[2] + " ======"
			if len(cmd) >= 1 and cmd[0]!="":
				p.send(cmd[0])
			if len(cmd) >= 2 and cmd[1]!="":
				p.expect(cmd[1])

	except TestError, e:
		handle_error(e.desc, script.test)

# Run the test function
if script.test.test_func != None:
	try:
		script.test.test_func(script.test)
	except TestError, e:
		handle_error(e.desc, script.test)

# Shutdown all instances
time.sleep(2)
for p in script.test.process:
	p.send("q")
	time.sleep(0.5)
	p.expect(const.DESTROYED)
	p.wait()

# Done
print "Test " + script.test.title + " completed successfully"
sys.exit(0)

