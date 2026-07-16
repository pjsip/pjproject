#
import inc_util as util
from inc_cfg import *

# SIPS (secure SIP URI scheme) transport
#
# Unlike 220_tls.py, which just requests ";transport=tls" on a plain
# "sip:" URI, this test uses the "sips:" scheme itself. pjsua only
# renders its own Contact/From as "sips:" when its local "--id" is a
# sips: URI (see acc->is_sips in pjsua_acc.c), so --id is set on both
# sides. The TLS listener is still created on (--local-port + 1), see
# the "use_tls" block in pjsua_app.c.
SIPS_ARGS = ("--null-audio --use-tls --no-tcp --no-udp --max-calls=1 "
             "--id=sips:pjsip@127.0.0.1 "
             "--tls-ca-file=certs/pjsua_test_cert.pem "
             "--tls-cert-file=certs/pjsua_test_cert.pem "
             "--tls-privkey-file=certs/pjsua_test_privkey.pem")

callee = InstanceParam("callee", SIPS_ARGS)
callee.uri = "<sips:pjsip@127.0.0.1:" + str(callee.sip_port + 1) + ">"

caller = InstanceParam("caller", SIPS_ARGS)

test_param = TestParam(
        "SIPS transport",
        [callee, caller]
        )

# Skip this test if the build under test doesn't have TLS transport
# support compiled in (PJ_HAS_SSL_SOCK == 0, e.g. configured with
# --disable-ssl). SIPS mandates a secure (TLS) hop, so it can't run
# without it either.
if not util.has_ssl_sock():
    test_param.skip = True
