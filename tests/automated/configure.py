#!/usr/bin/python

import optparse
import os
import platform
import socket
import subprocess
import sys

PROG = "r" + "$Rev: 17 $".strip("$ ").replace("Rev: ", "")

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

def replace_vars(text):
        while True:
                if text.find("$(PJSUA-TESTS)") >= 0:
                        proc = subprocess.Popen("python runall.py --list-xml", cwd="../pjsua",
                                                shell=True, stdout=subprocess.PIPE)
                        content = proc.stdout.read()
                        text = text.replace("$(PJSUA-TESTS)", content)
                elif text.find("$(GCC)") >= 0:
                        text = text.replace("$(GCC)", gcc_version("gcc"))
                elif text.find("$(DISABLED)") >= 0:
                        text = text.replace("$(DISABLED)", "0")
                elif text.find("$(OS)") >= 0:
                        os_info = platform.system() + platform.release() + "-" + platform.machine()
                        if platform.system().lower() == "linux":
                                os_info =  os_info + "-" + "-".join(platform.linux_distribution()[0:2])
                        text = text.replace("$OS", os_info)
                elif text.find("$(SUFFIX)") >= 0:
                        proc = subprocess.Popen("sh config.guess", cwd="../..",
                                                shell=True, stdout=subprocess.PIPE)
                        plat = proc.stdout.readline().rstrip(" \r\n")
                        text = text.replace("$(SUFFIX)", plat)
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
        usage = """Usage: configure.py scenario_template_file
"""

        args.pop(0)
        if not len(args):
                print usage
                return 1
        
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

