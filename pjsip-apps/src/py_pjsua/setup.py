from distutils.core import setup, Extension
import os

pjproject = "../../../"

# Determine target
#target = "i686-pc-linux-gnu"
f = os.popen("grep TARGET_NAME ../../../build.mak")
line = f.readline()
tokens = line.split()
found = 0
for token in tokens:
	if token == ":=" or token == "=":
		found = 1
	elif found != 0:
		target = token
		break

print "Building py_pjsua module for " + target

setup(name="py_pjsua", version="0.1",
	ext_modules = [
		Extension("py_pjsua", 
			  ["py_pjsua.c"], 
			   include_dirs=[pjproject + "pjsip/include", 
					 pjproject + "pjlib/include", 
					 pjproject + "pjlib-util/include", 						 pjproject + "pjmedia/include"], 
			   library_dirs=[pjproject + "pjsip/lib", 
					 pjproject + "pjlib/lib", 
					 pjproject + "pjmedia/lib", 
					 pjproject + "pjlib-util/lib"], 
			   libraries=[	"pjsua-" + target, 
					"pjsip-ua-" + target, 
					"pjsip-simple-" + target, 
					"pjsip-" + target, 
					"pjmedia-codec-" + target, 
					"pjmedia-" + target, 
					"pjmedia-codec-" + target, 
					"pjlib-util-" + target, 
					"pj-" + target,
					"ssl",
					"crypto",
					"asound"]),
	])

