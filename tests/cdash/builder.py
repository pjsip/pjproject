import ccdash
import os
import platform
import re
import subprocess
import sys
import time

class Operation:
    """\
    The Operation class describes the individual ccdash operation to be 
    performed.

    """
    # Types:
    UPDATE = "update"           # Update operation
    CONFIGURE = "configure"     # Configure operation
    BUILD = "build"             # Build operation
    TEST = "test"               # Unit test operation

    def __init__(self, type, cmdline, name="", wdir=""):
        self.type = type
        self.cmdline = cmdline
        self.name = name
        self.wdir = wdir
        if self.type==self.TEST and not self.name:
            raise "name required for tests"

    def encode(self, base_dir):
        s = [self.type]
        if self.type == self.TEST:
            s.append(self.name)
        if self.type != self.UPDATE:
            s.append(self.cmdline)
        s.append("-w")
        if self.wdir:
            s.append(base_dir + "/" + self.wdir)
        else:
            s.append(base_dir)
        return s


#
# Update operation
#
update_ops = [Operation(Operation.UPDATE, "")]

#
# The standard library tests (e.g. pjlib-test, pjsip-test, etc.)
#
std_test_ops= [
    Operation(Operation.TEST, "./pjlib-test-$SUFFIX", name="pjlib test",
              wdir="pjlib/bin"),
    Operation(Operation.TEST, "./pjlib-util-test-$SUFFIX", 
              name="pjlib-util test", wdir="pjlib-util/bin"),
    Operation(Operation.TEST, "./pjnath-test-$SUFFIX", name="pjnath test",
              wdir="pjnath/bin"),
    Operation(Operation.TEST, "./pjmedia-test-$SUFFIX", name="pjmedia test",
              wdir="pjmedia/bin"),
    Operation(Operation.TEST, "./pjsip-test-$SUFFIX", name="pjsip test",
              wdir="pjsip/bin")
]

#
# These are operations to build the software on GNU/Posix systems
#
gnu_build_ops = [
    Operation(Operation.CONFIGURE, "./configure"),
    Operation(Operation.BUILD, "make distclean; make dep && make; cd pjsip-apps/src/python; python setup.py clean build"),
    #Operation(Operation.BUILD, "python setup.py clean build",
    #          wdir="pjsip-apps/src/python")
]

#
# These are pjsua Python based unit test operations
#
def build_pjsua_test_ops():
    ops = []
    cwd = os.getcwd()
    os.chdir("../pjsua")
    os.system("python runall.py --list > list")
    f = open("list", "r")
    for e in f:
        e = e.rstrip("\r\n ")
        (mod,param) = e.split(None,2)
        name = mod[4:mod.find(".py")] + "_" + \
               param[param.find("/")+1:param.find(".py")]
        ops.append(Operation(Operation.TEST, "python run.py " + e, name=name,
                   wdir="tests/pjsua"))
    os.chdir(cwd)
    return ops

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
# Test config
#
class BaseConfig:
    def __init__(self, base_dir, url, site, group, options=None):
        self.base_dir = base_dir
        self.url = url
        self.site = site
        self.group = group
        self.options = options

#
# Base class for test configurator
#
class TestBuilder:
    def __init__(self, config, build_config_name="",
                 user_mak="", config_site="", exclude=[], not_exclude=[]):
        self.config = config                        # BaseConfig instance
        self.build_config_name = build_config_name  # Optional build suffix
        self.user_mak = user_mak                    # To be put in user.mak
        self.config_site = config_site              # To be put in config_s..
        self.saved_user_mak = ""                    # To restore user.mak
        self.saved_config_site = ""                 # To restore config_s..
        self.exclude = exclude                      # List of exclude pattern
        self.not_exclude = not_exclude              # List of include pattern
        self.ccdash_args = []                       # ccdash cmd line

    def stamp(self):
        return time.strftime("%Y%m%d-%H%M", time.localtime())

    def pre_action(self):
        # Override user.mak
        name = self.config.base_dir + "/user.mak"
        if os.access(name, os.F_OK):
            f = open(name, "r")
            self.saved_user_mak = f.read()
            f.close()
        if True:
            f = open(name, "wt")
            f.write(self.user_mak)
            f.close()
        # Override config_site.h
        name = self.config.base_dir + "/pjlib/include/pj/config_site.h"
        if os.access(name, os.F_OK):
            f = open(name, "r")
            self.saved_config_site= f.read()
            f.close()
        if True:
            f = open(name, "wt")
            f.write(self.config_site)
            f.close()


    def post_action(self):
        # Restore user.mak
        name = self.config.base_dir + "/user.mak"
        if self.saved_user_mak:
            f = open(name, "wt")
            f.write(self.saved_user_mak)
            f.close()
        else:
            os.remove(name)
        # Restore config_site.h
        name = self.config.base_dir + "/pjlib/include/pj/config_site.h"
        if self.saved_config_site:
            f = open(name, "wt")
            f.write(self.saved_config_site)
            f.close()
        else:
            os.remove(name)

    def build_tests(self):
        # This should be overridden by subclasses
        pass

    def execute(self):
        if len(self.ccdash_args)==0:
            self.build_tests()
        self.pre_action()
        counter = 0
        for a in self.ccdash_args:
            # Check if this test is in exclusion list
            fullcmd = " ".join(a)
            excluded = False
            included = False
            for pat in self.exclude:
                if re.search(pat, fullcmd) != None:
                    excluded = True
                    break
            if excluded:
                for pat in self.not_exclude:
                    if re.search(pat, fullcmd) != None:
                        included = True
                        break
            if excluded and not included:
                print "Skipping test '%s'.." % (fullcmd)
                continue

            #a.extend(["-o", "/tmp/xx" + a[0] + ".xml"])
            #print a
            #a = ["ccdash.py"].extend(a)
            b = ["ccdash.py"]
            b.extend(a)
            a = b
            #print a
            ccdash.main(a)
            counter = counter + 1
        self.post_action()


#
# GNU test configurator
#
class GNUTestBuilder(TestBuilder):
    def __init__(self, config, build_config_name="", user_mak="", \
                 config_site="", cross_compile="", exclude=[], not_exclude=[]):
        TestBuilder.__init__(self, config, build_config_name=build_config_name,
                             user_mak=user_mak, config_site=config_site,
                             exclude=exclude, not_exclude=not_exclude)
        self.cross_compile = cross_compile
        if self.cross_compile and self.cross_compile[-1] != '-':
            self.cross_compile.append("-")

    def build_tests(self):
        if self.cross_compile:
            suffix = self.cross_compile
            build_name =  suffix + gcc_version(self.cross_compile + "gcc")
        else:
            proc = subprocess.Popen(self.config.base_dir+"/config.guess",
                                    stdout=subprocess.PIPE)
            suffix = proc.stdout.readline().rstrip(" \r\n")
            build_name =  suffix+"-"+gcc_version(self.cross_compile + "gcc")

        if self.build_config_name:
            build_name = build_name + "-" + self.build_config_name
        cmds = []
        cmds.extend(update_ops)
        cmds.extend(gnu_build_ops)
        cmds.extend(std_test_ops)
        cmds.extend(build_pjsua_test_ops())
        self.ccdash_args = []
        for c in cmds:
            c.cmdline = c.cmdline.replace("$SUFFIX", suffix)
            args = c.encode(self.config.base_dir)
            args.extend(["-U", self.config.url, 
                         "-S", self.config.site, 
                         "-T", self.stamp(), 
                         "-B", build_name, 
                         "-G", self.config.group])
            args.extend(self.config.options)
            self.ccdash_args.append(args)


