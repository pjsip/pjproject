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

# Cache of vid_codec_list() results, keyed by executable path. The
# video capability checks below run at config-load time, once per test
# script, and a script that asks for both video and a specific codec
# would otherwise pay for two pjsua launches.
_vid_codec_list_cache = {}

def vid_codec_list(exe):
   """Return the "vid codec list" output of the pjsua build under test,
   or None if the probe could not be run to completion.

   Video capability is not reported by pj_dump_config() (unlike
   PJ_HAS_SSL_SOCK), so it has to be probed from the running binary:
   start it with --video and ask the legacy console to list video
   codecs. A video-enabled build with a codec prints "Found N video
   codecs" with N>=1, followed by one row per codec.

   --local-port 0 makes this auxiliary pjsua bind an ephemeral SIP port
   instead of the default 5060, so the probe can't fail to start because
   5060 (or a concurrently running pjsua) already holds that port.

   None means "couldn't tell" -- the probe couldn't launch, timed out,
   or exited non-zero (e.g. pjsua crashed at startup) -- as opposed to a
   probe that ran cleanly and simply reported no video. Callers must
   treat the two differently: see has_video().
   """
   if exe in _vid_codec_list_cache:
      return _vid_codec_list_cache[exe]

   # Use Popen().communicate(), not subprocess.run(): run() is Python 3.5+
   # but this harness (see load_module_from_file() below) still supports
   # 3.x < 3.5, and these checks run at config-load time, so a missing
   # subprocess.run would raise before the test could even be skipped.
   out = None
   try:
      proc = subprocess.Popen(exe + " --video --null-audio --local-port 0"
                              " --max-calls=1",
                              shell=True, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT,
                              universal_newlines=True)
   except OSError:
      # Couldn't launch at all -- not the same as "video disabled".
      proc = None

   if proc is not None:
      try:
         out, _ = proc.communicate(input="vid codec list\nq\n", timeout=30)
      except (OSError, subprocess.SubprocessError):
         # Includes TimeoutExpired. Probe couldn't complete.
         proc.kill()
         proc.wait()
         out = None
      else:
         if proc.returncode != 0:
            # pjsua didn't start/exit cleanly: probe unreliable.
            out = None

   _vid_codec_list_cache[exe] = out
   return out

def has_video(exe):
   """Return True if the pjsua build under test has video support with at
   least one usable video codec.

   A video call test cannot negotiate a video stream otherwise -- either
   the build has PJMEDIA_HAS_VIDEO=0 (only enabled in some CI jobs), or it
   was built without any video codec library (VPX/OpenH264).

   A probe that could not run cleanly fails open (returns True, like
   has_ssl_sock()) so the real test runs and surfaces the problem rather
   than being silently skipped; only a probe that exits cleanly yet
   reports no codec returns False. This is safe to gate on the exit code
   because --video is not compiled out on a non-video build (only the
   "vid" console commands are), so a non-video pjsua still accepts
   --video and exits 0 -- it simply never prints "Found N".
   """
   out = vid_codec_list(exe)
   if out is None:
      return True

   m = re.search(r'Found\s+(\d+)\s+video codecs', out)
   return bool(m) and int(m.group(1)) > 0

def has_vid_codec(exe, codec_id):
   """Return True if the pjsua build under test has the named video codec,
   e.g. has_vid_codec(exe, "H264").

   Which video codecs exist depends on the codec libraries the build was
   configured with (OpenH264, VPX, ...), so a test pinned to one codec
   has to check for that codec specifically -- has_video() only says
   *some* codec is available.

   The codec is looked for as a row of the "vid codec list" table, whose
   first column is "<id>/<payload type>" followed by the priority and
   frame rate columns. Matching the row shape rather than a bare name
   keeps an unrelated startup log line that happens to mention the codec
   from being read as a match. As in has_video(), an unusable probe
   fails open.
   """
   out = vid_codec_list(exe)
   if out is None:
      return True

   return re.search(re.escape(codec_id) + r'/\d+\s+\d+\s+\d+\.\d+',
                    out) is not None

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
