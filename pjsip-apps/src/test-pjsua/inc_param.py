# $Id$
###########################################
# pjsua instantiation parameter
class Pjsua:
	# instance name
	name = ""
	# command line arguments. Default is empty.
	# sample:
	#  args = "--null-audio --local-port 0"
	args = ""
	# list containing send/expect/title list. Default empty.
	# The inside list contains three items, all are optional:
	#  - the command to be sent to pjsua menu
	#  - the string to expect
	#  - optional string to describe what this is doing
	# Sample of command list containing two list items:
	#  cmds = [["sleep 50",""], ["q","", "quitting.."]]
	cmds = []
	# print out the stdout output of this pjsua?
	echo = False
	# print out commands interacting with this pjsua?
	trace = False
	def __init__(self, name, args="", echo=False, trace=False, cmds=[]):
		self.name = name
		self.args = args
		self.echo = echo
		self.trace = trace
		self.cmds = cmds
			
############################################
# Test parameter class
class Test:
	title = ""
	# params is list containing Pjsua objects
	run = []
	# list of Expect instances, to be filled at run-time by
        # the test program	
	process = []
	# the function for test body
	test_func = None
	def __init__(self, title, run, func=None):
		self.title = title
		self.run = run
		self.test_func = func


