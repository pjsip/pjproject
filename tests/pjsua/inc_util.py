import os
import re
import sys

def has_ssl_sock():
   """Return True if the pjsua build under test was configured with SSL
   socket (TLS transport) support, i.e. PJ_HAS_SSL_SOCK is not 0.

   pjlib/include/pj/config_site.h (if it sets the macro) takes
   precedence over the autoconf-generated default in
   pjlib/include/pj/compat/os_auto.h, mirroring the "#ifndef
   PJ_HAS_SSL_SOCK" guard used by the actual C build, so the check
   below looks at config_site.h first.
   """
   base = os.path.join(os.path.dirname(__file__), "..", "..", "pjlib",
                        "include", "pj")
   site_header = os.path.join(base, "config_site.h")
   auto_header = os.path.join(base, "compat", "os_auto.h")

   def find_value(path):
      try:
         with open(path) as f:
            content = f.read()
      except IOError:
         return None
      # Only match active #define/#undef lines, not commented-out ones
      # (e.g. "//#define PJ_HAS_SSL_SOCK 1"). Autoconf's autoheader
      # leaves the macro as a bare "#undef PJ_HAS_SSL_SOCK" (no value)
      # when SSL support wasn't detected/was disabled, so that must be
      # treated as "not available" rather than ignored.
      for line in content.splitlines():
         m = re.match(r'\s*#\s*(define|undef)\s+PJ_HAS_SSL_SOCK\b\s*(\d+)?',
                      line)
         if m:
            directive, value = m.group(1), m.group(2)
            if directive == "undef":
               return False
            return value is None or value != "0"
      return None

   val = find_value(site_header)
   if val is not None:
      return val
   val = find_value(auto_header)
   if val is not None:
      return val
   # Headers not found (e.g. unusual working directory): assume SSL is
   # available rather than silently skipping the TLS/SIPS tests.
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
