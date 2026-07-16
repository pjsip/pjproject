#
import inc_util as util
from inc_cfg import *

# TLS transport
#
# Note: pjsua creates its TLS listener on (--local-port + 1), see the
# "use_tls" block in pjsua_app.c, so the callee's URI must target
# sip_port+1 rather than the --local-port value itself.
TLS_ARGS = ("--null-audio --use-tls --no-tcp --no-udp --max-calls=1 "
            "--tls-ca-file=certs/pjsua_test_cert.pem "
            "--tls-cert-file=certs/pjsua_test_cert.pem "
            "--tls-privkey-file=certs/pjsua_test_privkey.pem")

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
if not util.has_ssl_sock():
    test_param.skip = True
