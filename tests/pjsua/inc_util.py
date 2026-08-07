import re
import subprocess
import sys

def has_rtcp_xr(exe):
   """Return True if the pjsua build under test was compiled with RTCP XR
   (extended reports) support, i.e. PJMEDIA_HAS_RTCP_XR != 0.

   RTCP XR is a compile-time option, off by default. pj_dump_config()
   (pjlib) can't report a pjmedia flag, so the pjsua app prints
   "PJMEDIA_HAS_RTCP_XR : 0|1" in its "--version" output (alongside
   pj_dump_config's own lines). We run "<exe> --version" and parse it --
   the same approach as has_ssl_sock() below. Querying the running binary
   works regardless of static vs. shared linking (the value is compiled
   into the app itself), unlike scanning the executable for a string that
   a shared build would keep in libpjsua.

   Note this only detects the build-time capability. XR must also be
   enabled per stream at run-time via pjsua_acc_config.enable_rtcp_xr,
   whose default pjsua_acc_config_default() derives from the
   PJMEDIA_STREAM_ENABLE_XR build setting; the 415 test enables it
   explicitly with pjsua's --rtcp-xr option regardless of that default.
   """
   try:
      out = subprocess.check_output(exe + " --version", shell=True,
                                     stderr=subprocess.STDOUT,
                                     universal_newlines=True, timeout=10)
   except (OSError, subprocess.CalledProcessError,
           subprocess.TimeoutExpired) as e:
      out = getattr(e, "output", "") or ""

   m = re.search(r'PJMEDIA_HAS_RTCP_XR\s*:\s*(\d+)', out)
   if m:
      return m.group(1) != "0"
   # No flag line (e.g. a pjsua too old to print it, or the probe couldn't
   # run): RTCP XR is off in almost every build, so skip rather than run
   # test 415 and have it fail spuriously on a build that lacks XR.
   return False

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

def has_video(exe):
   """Return True if the pjsua build under test has video support with at
   least one usable video codec.

   A video call test cannot negotiate a video stream otherwise -- either
   the build has PJMEDIA_HAS_VIDEO=0 (only enabled in some CI jobs), or it
   was built without any video codec library (VPX/OpenH264). Unlike
   PJ_HAS_SSL_SOCK, video capability is not reported by pj_dump_config(),
   so we probe the running binary: start it with --video and ask the
   legacy console to list video codecs. A video-enabled build with a
   codec prints "Found N video codecs" with N>=1.

   --local-port 0 makes this auxiliary pjsua bind an ephemeral SIP port
   instead of the default 5060, so the probe can't fail to start because
   5060 (or a concurrently running pjsua) already holds that port.

   A probe that could not run cleanly (couldn't launch, timed out, or
   exited non-zero, e.g. pjsua crashed at startup) is distinct from a
   build that ran fine but has no video. The former fails open (returns
   True, like has_ssl_sock()) so the real test runs and surfaces the
   problem rather than being silently skipped; only a probe that exits
   cleanly yet reports no codec returns False. This is safe to gate on the
   exit code because --video is not compiled out on a non-video build
   (only the "vid" console commands are), so a non-video pjsua still
   accepts --video and exits 0 -- it simply never prints "Found N".
   """
   # Use Popen().communicate(), not subprocess.run(): run() is Python 3.5+
   # but this harness (see load_module_from_file() below) still supports
   # 3.x < 3.5, and has_video() runs at config-load time, so a missing
   # subprocess.run would raise before the test could even be skipped.
   try:
      proc = subprocess.Popen(exe + " --video --null-audio --local-port 0"
                              " --max-calls=1",
                              shell=True, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT,
                              universal_newlines=True)
   except OSError:
      # Couldn't launch at all -- not the same as "video disabled".
      return True

   try:
      out, _ = proc.communicate(input="vid codec list\nq\n", timeout=30)
   except (OSError, subprocess.SubprocessError):
      # Includes TimeoutExpired. Probe couldn't complete: fail open.
      proc.kill()
      proc.wait()
      return True

   if proc.returncode != 0:
      # pjsua didn't start/exit cleanly: probe unreliable, fail open.
      return True

   m = re.search(r'Found\s+(\d+)\s+video codecs', out or "")
   return bool(m) and int(m.group(1)) > 0

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
