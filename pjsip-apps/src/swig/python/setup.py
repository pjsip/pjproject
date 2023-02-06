#
# pjsua2 Setup script.
#
# Copyright (C)2012 Teluu Inc. (http://www.teluu.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
#
from distutils.core import setup, Extension
import os
import sys
import platform

# find pjsip version
pj_version=""
pj_version_major=""
pj_version_minor=""
pj_version_rev=""
pj_version_suffix=""
write=sys.stdout.write

# This is needed for building wheels, because pip moves files to a temporary folder
path_version_relative = '../../../../version.mak'
path_version_by_env = os.path.join(os.environ.get("PJDIR", ""), "version.mak")
path_version = path_version_relative if os.path.exists(path_version_relative) else path_version_by_env
f = open(path_version, 'r')
for line in f:
    tokens=""
    if line.find("export PJ_VERSION_MAJOR") != -1:
        tokens=line.split("=")
    if len(tokens)>1:
        pj_version_major= tokens[1].strip()
    elif line.find("export PJ_VERSION_MINOR") != -1:
        tokens=line.split("=")
    if len(tokens)>1:
        pj_version_minor= line.split("=")[1].strip()
    elif line.find("export PJ_VERSION_REV") != -1:
        tokens=line.split("=")
    if len(tokens)>1:
        pj_version_rev= line.split("=")[1].strip()
    elif line.find("export PJ_VERSION_SUFFIX") != -1:
        tokens=line.split("=")
    if len(tokens)>1:
        pj_version_suffix= line.split("=")[1].strip()

f.close()
if not pj_version_major:
    write("Unable to get PJ_VERSION_MAJOR" + "\r\n")
    sys.exit(1)

pj_version = pj_version_major + "." + pj_version_minor
if pj_version_rev:
    pj_version += "." + pj_version_rev
if pj_version_suffix:
    pj_version += "-" + pj_version_suffix

#print 'PJ_VERSION = "'+ pj_version + '"'

# Get 'make' from environment variable if any
MAKE = os.environ.get('MAKE') or "make"

# Get targetname
f = os.popen("%s --no-print-directory -f helper.mak target_name" % MAKE)
pj_target_name = f.read().rstrip("\r\n")
f.close()

# Fill in extra_compile_args
extra_compile_args = []
f = os.popen("%s --no-print-directory -f helper.mak cflags" % MAKE)
for line in f:
    extra_compile_args.append(line.rstrip("\r\n"))
f.close()

# Fill in libraries
libraries = []
f = os.popen("%s --no-print-directory -f helper.mak libs" % MAKE)
for line in f:
    libraries.append(line.rstrip("\r\n"))
f.close()

# Fill in extra_link_args
extra_link_args = []
f = os.popen("%s --no-print-directory -f helper.mak ldflags" % MAKE)
for line in f:
    extra_link_args.append(line.rstrip("\r\n"))
f.close()

# MinGW specific action: put current working dir to PATH, so Python distutils
# will invoke our dummy gcc/g++ instead, which is in the current working dir.
if platform.system()=='Windows' and os.environ["MSYSTEM"].find('MINGW')!=-1:
    os.environ["PATH"] = "." + os.pathsep + os.environ["PATH"]

setup(name="pjsua2", 
      version=pj_version,
      description='SIP User Agent Library based on PJSIP',
      url='http://www.pjsip.org',
      ext_modules = [Extension("_pjsua2", 
                               ["pjsua2_wrap.cpp"],
                               libraries=libraries,
                               extra_compile_args=extra_compile_args,
                               extra_link_args=extra_link_args
                              )
                    ],
      py_modules=["pjsua2"]
     )
