# $Id$

DEFAULT_ECHO = True
DEFAULT_TRACE = True

# Individual pjsua config class
class Config:
	# pjsua command line arguments, concatenated in string
	arg = ""
	# Specify whether pjsua output should be echoed to stdout
	echo_enabled = DEFAULT_ECHO
	# Enable/disable test tracing
	trace_enabled = DEFAULT_TRACE
	def __init__(self, arg, echo_enabled=DEFAULT_ECHO, trace_enabled=DEFAULT_TRACE):
		self.arg = arg
		self.echo_enabled = echo_enabled
		self.trace_enabled = trace_enabled

# Call config class
class CallConfig:
	# additional parameter to be added to target URI
	uri_param = ""
	def __init__(self, title, callee_cfg, caller_cfg):
		self.title = title
		self.callee_cfg = callee_cfg
		self.caller_cfg = caller_cfg

