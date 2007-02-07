from distutils.core import setup, Extension
pjproject = "../../../"
target = "i686-pc-linux-gnu"
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

