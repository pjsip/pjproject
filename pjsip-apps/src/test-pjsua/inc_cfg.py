# $Id$
import random

DEFAULT_ECHO = True
DEFAULT_TRACE = True
DEFAULT_START_SIP_PORT = 50000

# Individual pjsua instance configuration class
class InstanceParam:
	# Name to identify this pjsua instance (e.g. "caller", "callee", etc.)
	name = ""
	# pjsua command line arguments, concatenated in string
	arg = ""
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
	def __init__(	self, 
			name,			# Instance name
			arg, 			# Cmd-line arguments
			uri="", 		# URI
			uri_param="",		# Additional URI param
			sip_port=0, 		# SIP port
			have_reg=False,		# Have registration?
			have_publish=False,	# Have publish?
			echo_enabled=DEFAULT_ECHO, 
			trace_enabled=DEFAULT_TRACE):
		# Instance name
		self.name = name
		# Give random sip_port if it's not specified
		if sip_port==0:
			self.sip_port = random.randint(DEFAULT_START_SIP_PORT, 65534)
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
		if not ("--publish" in self.arg):
			self.arg = self.arg + " --publish"
		self.echo_enabled = echo_enabled
		self.trace_enabled = trace_enabled


############################################
# Test parameter class
class TestParam:
	title = ""
	# params is list containing InstanceParams objects
	inst_params = []
	# list of Expect instances, to be filled at run-time by
        # the test program	
	process = []
	# the function for test body
	test_func = None
	post_func = None
	user_data = None
	def __init__(	self, 
			title, 		# Test title
			inst_params, 	# InstanceParam's as list
			func=None,
			post_func=None,
			user_data=None):
		self.title = title
		self.inst_params = inst_params
		self.test_func = func
		self.post_func = post_func
		self.user_data = user_data



