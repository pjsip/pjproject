#
import os
import inc_util as util
from inc_cfg import *

# TLS transport
#
# Note: pjsua creates its TLS listener on (--local-port + 1), see the
# "use_tls" block in pjsua_app.c, so the callee's URI must target
# sip_port+1 rather than the --local-port value itself.
#
# Cert/key paths are built from this file's own location (rather than
# assumed relative to the current working directory) so the test still
# works when run.py is invoked from somewhere other than tests/pjsua/.
CERTS_DIR = os.path.normpath(os.path.join(
                os.path.dirname(os.path.abspath(__file__)), "..", "certs"))
TLS_CA_FILE = os.path.join(CERTS_DIR, "pjsua_test_cert.pem")
TLS_CERT_FILE = os.path.join(CERTS_DIR, "pjsua_test_cert.pem")
TLS_PRIVKEY_FILE = os.path.join(CERTS_DIR, "pjsua_test_privkey.pem")

TLS_ARGS = ("--null-audio --use-tls --no-tcp --no-udp --max-calls=1 "
            "--tls-ca-file=" + TLS_CA_FILE + " "
            "--tls-cert-file=" + TLS_CERT_FILE + " "
            "--tls-privkey-file=" + TLS_PRIVKEY_FILE)

callee = InstanceParam("callee", TLS_ARGS)
callee.uri = "<sip:pjsip@127.0.0.1:" + str(callee.sip_port + 1) + ";transport=tls>"

caller = InstanceParam("caller", TLS_ARGS)

test_param = TestParam(
        "TLS transport",
        [callee, caller]
        )

# Skip this test if the build under test doesn't have TLS transport
# support compiled in (PJ_HAS_SSL_SOCK == 0, e.g. configured with
# --disable-ssl).
if not util.has_ssl_sock(G_EXE):
    test_param.skip = True
