# $Id$
import random
import config_site
import socket
import errno
import time

DEFAULT_ECHO = True
DEFAULT_TRACE = True
DEFAULT_START_SIP_PORT = 50000
DEFAULT_TELNET = True
DEFAULT_START_TELNET_PORT = 60000

# Shared vars
ARGS = []		# arguments containing script module & config
HAS_SND_DEV = config_site.HAS_SND_DEV

# Individual pjsua instance configuration class
class InstanceParam:
	# Name to identify this pjsua instance (e.g. "caller", "callee", etc.)
	name = ""
	# pjsua command line arguments, concatenated in string
	arg = ""
	# Specify whether pjsua telnet CLI is enabled
	telnet_enabled = DEFAULT_TELNET
	# Telnet port number, zero to automatically assign
	telnet_port = 0
	# Specify whether pjsua output should be echoed to stdout
	echo_enabled = DEFAULT_ECHO
	# Enable/disable test tracing
	trace_enabled = DEFAULT_TRACE
	# SIP URI to send request to this instance
	uri = ""
	# SIP port number, zero to automatically assign
	sip_port = 0
	# Does this have registration? If yes then the test function will
	# wait until the UA is registered before doing anything else
	have_reg = False
	# Does this have PUBLISH?
	have_publish = False
	# Enable stdout buffer?
	enable_buffer = False
	def __init__(	self, 
			name,			# Instance name
			arg, 			# Cmd-line arguments
			uri="", 		# URI
			uri_param="",		# Additional URI param
			telnet_port=0, 		# Telnet port
			sip_port=0, 		# SIP port
			have_reg=False,		# Have registration?
			have_publish=False,	# Have publish?
			echo_enabled=DEFAULT_ECHO, 
			trace_enabled=DEFAULT_TRACE,
			telnet_enabled = DEFAULT_TELNET,
			enable_buffer = False):
		# Instance name
		self.name = name
		# Give random telnet_port if it's not specified and telnet CLI is enabled
		if telnet_enabled and telnet_port==0:
			# avoid port conflict
			cnt = 0
			port = 0
			while cnt < 10:
				cnt = cnt + 1
				port = random.randint(DEFAULT_START_TELNET_PORT, 65534)
				s = socket.socket(socket.AF_INET)
				try:
					s.bind(("0.0.0.0", port))
				except socket.error as serr:
					s.close()
					if serr.errno ==  errno.EADDRINUSE:
						continue
				s.close()
				break;
			self.telnet_port = port
		else:
			self.telnet_port = telnet_port
		# Give random sip_port if it's not specified
		if sip_port==0:
			# avoid port conflict
			cnt = 0
			port = 0
			while cnt < 10:
				port = random.randint(DEFAULT_START_SIP_PORT, 60000)
				if port==self.telnet_port:
					continue
				cnt = cnt + 1
				s = socket.socket(socket.AF_INET)
				try:
					s.bind(("0.0.0.0", port))
				except socket.error as serr:
					s.close()
					if serr.errno ==  errno.EADDRINUSE:
						continue
				s.close()
				break;
			self.sip_port = port
			# Give some time for socket close
			time.sleep(0.5)
		else:
			self.sip_port = sip_port
		# Autogenerate URI if it's empty.
		self.uri = uri
		if self.uri=="":
			self.uri = "sip:pjsip@127.0.0.1:" + str(self.sip_port)
		# Add uri_param to the URI
		self.uri = self.uri + uri_param
		# Add bracket to the URI
		if self.uri[0] != "<":
			self.uri = "<" + self.uri + ">"
		# Add SIP local port to the argument
		self.arg = arg + " --local-port=" + str(self.sip_port)
		self.have_reg = have_reg
		self.have_publish = have_publish
		if have_publish and have_reg and not ("--publish" in self.arg):
			self.arg = self.arg + " --publish"
		self.echo_enabled = echo_enabled
		self.trace_enabled = trace_enabled
		self.enable_buffer = enable_buffer


############################################
# Test parameter class
class TestParam:
	title = ""
	# params is list containing InstanceParams objects
	inst_params = []
	# flag if this tes should be skipped
	skip = None
	# list of Expect instances, to be filled at run-time by
	# the test program	
	process = []
	# the function for test body
	test_func = None
	post_func = None
	def __init__(	self, 
			title, 		# Test title
			inst_params, 	# InstanceParam's as list
			func=None,
			skip=False,
			post_func=None,
			need_stdout_buffer=False):
		self.title = title
		self.inst_params = inst_params
		self.skip = skip
		self.test_func = func
		self.post_func = post_func


###################################
# TestError exception
class TestError:
	desc = ""
	def __init__(self, desc):
		self.desc = desc


