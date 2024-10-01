#
# importsym.py: Import C symbol decls (structs, enums, etc) and write them
#               to another file 
#
# Copyright (C)2013 Teluu Inc. (http://www.teluu.com)
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
import pycparser
from pycparser import c_generator
import sys
import os
from modules.win32fix import *

def which(program):
	import os
	def is_exe(fpath):
		return os.path.isfile(fpath) and os.access(fpath, os.X_OK)
	
	if sys.platform == 'win32' and not program.endswith(".exe"):
		program += ".exe"

	fpath, fname = os.path.split(program)
	if fpath:
		if is_exe(program):
			return program
	else:
		for path in os.environ["PATH"].split(os.pathsep):
			path = path.strip('"')
			exe_file = os.path.join(path, program)
			if is_exe(exe_file):
				return exe_file
	return None

#
PJ_ROOT_PATH = "../../../"
   
# CPP is needed by pycparser.
CPP_PATH = which("cpp")
if not CPP_PATH:
	print 'Error: need to have cpp in PATH'
	sys.exit(1)

# If OS is Linux, find local OS path for Unix .local installation of python, and assume user has installed pycparser via pip.
if sys.platform == "linux2":
	script_path_list = os.path.normpath(os.path.abspath(__file__)).split(os.sep)
	home_linux = os.path.join("/", script_path_list[1], script_path_list[2])
# if OS is Windows, find the path to current python environment, and assume user has installed pycparser via pip.
elif sys.platform == "win32":
    # Current Environment Path. it will take the currently active python interpreter path, which means if you are using a virtual environment,
    # it will take the path of the virtual environment.
	home_win = sys.prefix
	if home_win == "C:/msys64/mingw64":
		home_win = "C:/msys64/mingw64/lib/python2.7"
	else:
		pass
		
	# disable the include of pj/compat/socket.h in pj/sock.h, fixes having to go in and manually comment out the include.
	disable_include()
	print("continuing with code")

# Checks if its mingw64, if it is, it will use the path to the python interpreter in the mingw64 folder.
if sys.platform == 'win32' and home_win == "C:/msys64/mingw64/lib/python2.7":
	PYCPARSER_DIR = home_win + "/site-packages/pycparser"
# else it will use the path to the python interpreter in the default location.
elif sys.platform == "win32" and not home_win == "C:/msys64/mingw64/lib/python2.7":
    PYCPARSER_DIR = home_win + "/Lib/site-packages/pycparser"
elif sys.platform == "linux2":
	PYCPARSER_DIR = home_linux + "/.local/lib/python2.7/site-packages/pycparser"
else:
	print("Couldnt find your python location with pycparser.")
	PYCPARSER_DIR = raw_input("Enter the path to your pycparser installation: ")

pycparser_Fakelibc = os.path.dirname(PYCPARSER_DIR) + '/pycparser_fake_libc'

if not os.path.exists(pycparser_Fakelibc):
	print "Error: couldn't find pycparser_fake_libc in '%s'" % os.path.dirname(PYCPARSER_DIR) 
	sys.exit(1)

# Heading, to be placed before the source files
C_HEADING_SECTION = """
#define PJ_AUTOCONF		1
#define jmp_buf			int
#define __attribute__(x)
"""

# CPP (C preprocessor) settings
CPP_CFLAGS   = [
	'-I' + pycparser_Fakelibc,
	"-I" + PJ_ROOT_PATH + "pjlib/include",
	"-I" + PJ_ROOT_PATH + "pjlib-util/include",
	"-I" + PJ_ROOT_PATH + "pjnath/include",
	"-I" + PJ_ROOT_PATH + "pjmedia/include",
	"-I" + PJ_ROOT_PATH + "pjsip/include"
	]


class SymbolVisitor(pycparser.c_ast.NodeVisitor):
	def __init__(self, names):
		self.nodeDict = {}
		for name in names:
			self.nodeDict[name] = None
		
	def _add(self, node):
		if self.nodeDict.has_key(node.name):
			self.nodeDict[node.name] = node
		
	def visit_Struct(self, node):
		self._add(node)
		
	def visit_Enum(self, node):
		self._add(node)

	def visit_Typename(self, node):
		self._add(node)
		
	def visit_Typedef(self, node):
		self._add(node)

		
TEMP_FILE="tmpsrc.h"
		
class SymbolImporter:
	"""
	Import C selected declarations from C source file and move it
	to another file.
	
	Parameters:
	 - listfile	Path of file containing list of C source file
	                and identifier names to be imported. The format
	                of the listfile is:
	                
	                filename        name1  name2  name3
	                
	                for example:
	                
	                pj/sock_qos.h	pj_qos_type  pj_qos_flag
	                pj/types.h	pj_status_t  PJ_SUCCESS
	"""
	def __init__(self):
		pass
	
	def process(self, listfile, outfile):
		
		# Read listfile
		f = open(listfile)
		lines = f.readlines()
		f.close()
		
		# Process each line in list file, while generating the
		# temporary C file to be processed by pycparser
		f = open(TEMP_FILE, "w")
		f.write(C_HEADING_SECTION)
		names = []
		fcnt = 0
		for line in lines:
			spec = line.split()
			if len(spec) < 2:
				continue
			fcnt += 1
			f.write("#include <%s>\n" % spec[0])
			names.extend(spec[1:])
		f.close()
		print 'Parsing %d symbols from %d files..' % (len(names), fcnt)
		
		# Parse the temporary C file
		ast = pycparser.parse_file(TEMP_FILE, use_cpp=True, cpp_path=CPP_PATH, cpp_args=CPP_CFLAGS)
		os.remove(TEMP_FILE)
		
		# Filter the declarations that we wanted
		print 'Filtering..'
		visitor = SymbolVisitor(names)
		visitor.visit(ast)
	
		# Print symbol declarations to outfile
		print 'Writing declarations..'
		f = open(outfile, 'w')
		f.write("// This file is autogenerated by importsym script, do not modify!\n\n")
		gen = pycparser.c_generator.CGenerator()
		for name in names:
			node = visitor.nodeDict[name]
			if not node:
				print "  warning: declaration for '%s' is not found **" % k
			else:
				print "  writing '%s'.." % name
				output = gen.visit(node) + ";\n\n"
				f.write(output)
		f.close()
		print "Done."


if __name__ == "__main__":
	print "Importing symbols: 'symbols.lst' --> 'symbols.i'"
	si = SymbolImporter()
	si.process("symbols.lst", "symbols.i")
	try:
		os.remove("lextab.py")
	except OSError:
		pass
	try:
		os.remove("yacctab.py")
	except OSError:
		pass
	finally: 
		if sys.platform == "win32":
			re_enable_include()
			print("re-enabled pj/compat/socket.h!")
		sys.exit(0)
	
