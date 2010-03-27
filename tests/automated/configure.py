#!/usr/bin/python

import optparse
import os
import platform
import socket
import subprocess
import sys

PROG = "r" + "$Rev: 17 $".strip("$ ").replace("Rev: ", "")
PYTHON = os.path.basename(sys.executable)
build_type = ""
vs_target = ""

#
# Get gcc version
#
def gcc_version(gcc):
    proc = subprocess.Popen(gcc + " -v", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, shell=True)
    ver = ""
    while True:
        s = proc.stdout.readline()
        if not s:
            break
        if s.find("gcc version") >= 0:
            ver = s.split(None, 3)[2]
            break
    proc.wait()
    return "gcc-" + ver

#
# Get Visual Studio version
#
class VSVersion:
    def __init__(self):
	    self.version = "8"
	    self.release = "2005"

	    proc = subprocess.Popen("cl", stdout=subprocess.PIPE,
				    stderr=subprocess.STDOUT)
	    while True:
		s = proc.stdout.readline()
		if s=="":
		    break
		pos = s.find("Version")
		if pos > 0:
		    proc.wait()
		    s = s[pos+8:]
		    ver = s.split(None, 1)[0]
		    major = ver[0:2]
		    if major=="12":
			self.version = "6"
			self.release = "98"
			break
		    elif major=="13":
			self.version = "7"
			self.release = "2003"
			break
		    elif major=="14":
			self.version = "8"
			self.release = "2005"
			break
		    elif major=="15":
			self.version = "9"
			self.release = "2008"
			break
		    else:
			self.version = "10"
			self.release = "2010"
			break
	    proc.wait()
	    self.vs_version = "vs" + self.version
	    self.vs_release = "vs" + self.release
    

def replace_vars(text):
	global vs_target, build_type
	suffix = ""
        os_info = platform.system() + platform.release() + "-" + platform.machine()

	# osinfo
	if platform.system().lower() == "windows" or platform.system().lower() == "microsoft":
		if platform.system().lower() == "microsoft":
			os_info = platform.release() + "-" + platform.version() + "-" + platform.win32_ver()[2]
	elif platform.system().lower() == "linux":
                os_info =  + "-" + "-".join(platform.linux_distribution()[0:2])

	# Build vs_target
	if not vs_target and text.find("$(VSTARGET)") >= 0:
		if build_type != "vs":
			sys.stderr.write("Error: $(VSTARGET) only valid for Visual Studio\n")
			sys.exit(1)
		else:
			print "Enter Visual Studio vs_target name (e.g. Release, Debug) [Release]: ",
			vs_target = sys.stdin.readline().replace("\n", "").replace("\r", "")
			if not vs_target:
				vs_target = "Release"

	# Suffix
	if build_type == "vs":
		suffix = "i386-Win32-vc8-" + vs_target
	elif build_type == "gnu":
		proc = subprocess.Popen("sh config.guess", cwd="../..",
					shell=True, stdout=subprocess.PIPE)
		suffix = proc.stdout.readline().rstrip(" \r\n")
	else:
		sys.stderr.write("Error: unsupported built type " + build_type + "\n")
		sys.exit(1)

        while True:
                if text.find("$(PJSUA-TESTS)") >= 0:
			# Determine pjsua exe to use
			exe = "../../pjsip-apps/bin/pjsua-" + suffix
                        proc = subprocess.Popen(PYTHON + " runall.py --list-xml -e " + exe, 
						cwd="../pjsua",
                                                shell=True, stdout=subprocess.PIPE)
                        content = proc.stdout.read()
                        text = text.replace("$(PJSUA-TESTS)", content)
                elif text.find("$(GCC)") >= 0:
                        text = text.replace("$(GCC)", gcc_version("gcc"))
                elif text.find("$(VS)") >= 0:
			vsver = VSVersion()
                        text = text.replace("$(VS)", vsver.vs_release)
                elif text.find("$(VSTARGET)") >= 0:
                        text = text.replace("$(VSTARGET)", vs_target)
                elif text.find("$(DISABLED)") >= 0:
                        text = text.replace("$(DISABLED)", "0")
                elif text.find("$(OS)") >= 0:
                        text = text.replace("$(OS)", os_info)
                elif text.find("$(SUFFIX)") >= 0:
			text = text.replace("$(SUFFIX)", suffix)
                elif text.find("$(HOSTNAME)") >= 0:
                        text = text.replace("$(HOSTNAME)", socket.gethostname())
                elif text.find("$(PJDIR)") >= 0:
                        wdir = os.path.join(os.getcwd(), "../..")
                        wdir = os.path.normpath(wdir)
                        text = text.replace("$(PJDIR)", wdir)
                else:
                        break
        return text


def main(args):
	global vs_target, build_type
        usage = """Usage: configure.py [OPTIONS] scenario_template_file

Where OPTIONS:
  -t TYPE           Specify build type for Windows since we support both
                    Visual Studio and Mingw. If not specified, it will be
		    asked if necessary. Values are: 
		       vs:    Visual Studio
		       gnu:   Makefile based
  -T TARGETNAME     Specify Visual Studio target name if build type is set
                    to "vs", e.g. Release or Debug. If not specified then 
		    it will be asked.
"""

        args.pop(0)
	while len(args):
		if args[0]=='-T':
			args.pop(0)
			if len(args):
				vs_target = args[0]
				args.pop(0)
			else:
				sys.stderr.write("Error: needs value for -T\n")
				sys.exit(1)
		elif args[0]=='-t':
			args.pop(0)
			if len(args):
				build_type = args[0].lower()
				args.pop(0)
			else:
				sys.stderr.write("Error: needs value for -t\n")
				sys.exit(1)
			if not ["vs", "gnu"].count(build_type):
				sys.stderr.write("Error: invalid -t argument value\n")
				sys.exit(1)
		else:
			break

        if len(args) != 1:
                print usage
                return 1
        
	if not build_type and (platform.system().lower() == "windows" or platform.system().lower() == "microsoft"):
	    print "Enter the build type (values: vs, gnu) [vs]: ",
	    build_type = sys.stdin.readline().replace("\n", "").replace("\r", "")
	    if not build_type:
		   build_type = "vs"
		

        tpl_file = args[len(args)-1]
        if not os.path.isfile(tpl_file):
                print "Error: unable to find template file '%s'" % (tpl_file)
                return 1
                
        f = open(tpl_file, "r")
        tpl = f.read()
        f.close()
        
        tpl = replace_vars(tpl)
        print tpl
        return 0


if __name__ == "__main__":
    rc = main(sys.argv)
    sys.exit(rc)

