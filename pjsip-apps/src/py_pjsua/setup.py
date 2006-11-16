from distutils.core import setup, Extension
pjproject = "../../../"
setup(name="py_pjsua", version="0.1",
	ext_modules = [
		Extension("py_pjsua", ["py_pjsua.c"], include_dirs=[pjproject + "pjsip/include", pjproject + "pjlib/include", pjproject + "pjlib-util/include", pjproject + "pjmedia/include"], library_dirs=[pjproject + "pjsip/lib", pjproject + "pjlib/lib", pjproject + "pjmedia/lib", pjproject + "pjlib-util/lib"], libraries=["pjsua-i686-pc-linux-gnu", "pjsip-ua-i686-pc-linux-gnu", "pjsip-simple-i686-pc-linux-gnu", "pjsip-i686-pc-linux-gnu", "pjmedia-codec-i686-pc-linux-gnu", "pjmedia-i686-pc-linux-gnu", "pjmedia-codec-i686-pc-linux-gnu", "pjlib-util-i686-pc-linux-gnu", "pj-i686-pc-linux-gnu"]),
		
	])
