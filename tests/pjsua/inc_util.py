import re
import subprocess
import sys

def has_ssl_sock(exe):
   """Return True if the pjsua build under test was configured with SSL
   socket (TLS transport) support, i.e. PJ_HAS_SSL_SOCK is not 0.

   This runs "<exe> --version", which makes pjsua call pj_dump_config()
   and print (among other things) "PJ_HAS_SSL_SOCK : 0|1". Querying the
   actual binary (rather than parsing a generated header) means this
   works the same regardless of which build system produced it --
   autoconf's pjlib/include/pj/compat/os_auto.h isn't generated at all
   for a CMake build, which instead writes its own copy under the build
   tree at a location this script has no reliable way to know.
   """
   try:
      out = subprocess.check_output(exe + " --version", shell=True,
                                     stderr=subprocess.STDOUT,
                                     universal_newlines=True, timeout=10)
   except (OSError, subprocess.CalledProcessError,
           subprocess.TimeoutExpired) as e:
      out = getattr(e, "output", "") or ""

   m = re.search(r'PJ_HAS_SSL_SOCK\s*:\s*(\d+)', out)
   if m:
      return m.group(1) != "0"
   # Couldn't determine it (e.g. exe not found/too old to recognize
   # --version): assume SSL is available rather than silently skipping
   # the TLS/SIPS tests.
   return True

def load_module_from_file(module_name, module_path):
   if sys.version_info[0] == 3 and sys.version_info[1] >= 5:
      import importlib.util
      spec = importlib.util.spec_from_file_location(module_name, module_path)
      module = importlib.util.module_from_spec(spec)
      spec.loader.exec_module(module)
   elif sys.version_info[0] == 3 and sys.version_info[1] < 5:
      import importlib.machinery
      loader = importlib.machinery.SourceFileLoader(module_name, module_path)
      module = loader.load_module()
   return module
