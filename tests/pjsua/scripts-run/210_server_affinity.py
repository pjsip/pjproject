#
# Server affinity (#4964): basic end-to-end smoke.
#
# Two pjsua instances:
#   "srv" = SIP server using the built-in simple_registrar +
#           --auto-answer for incoming traffic.
#   "cli" = client started with --server-affinity but no startup
#           account; the test adds the account at runtime via the
#           +a CLI command so REGISTER (and the affinity capture log)
#           fires AFTER the framework's telnet stream is connected,
#           making both observable via expect().
#
# Validates:
#   - REGISTER succeeds with --server-affinity enabled.
#   - The auto-capture log line "server affinity pinned to transport"
#     appears, confirming the regc capture path fired.
#
from inc_cfg import *

srv = InstanceParam("srv",
                    "--null-audio --max-calls=4 --auto-answer=200")

# Start cli with --server-affinity (which also flips the global default
# so runtime-added accounts inherit ENABLED) but no startup account.
cli = InstanceParam("cli",
                    "--null-audio --max-calls=4 --server-affinity")


def add_acc_and_check(t):
    cli_proc = t.process[1]
    srv_port = t.inst_params[0].sip_port
    add_cmd = ('+a "sip:cli@127.0.0.1" "sip:127.0.0.1:%d" "*" '
               'cli secret' % srv_port)
    cli_proc.send(add_cmd)
    cli_proc.expect("registration success", title="register")
    cli_proc.expect("server affinity pinned to transport",
                    title="affinity capture")


test_param = TestParam(
    "Server affinity (#4964) basic",
    [srv, cli],
    func=add_acc_and_check
)
