from inc_cfg import *

# Test playwav with call-id -1 (current call or queue)
# Tests: 1) Using current call when call is active
#        2) Queueing for next call when no call is active
test_param = TestParam(
    "Call playwav with call-id -1 (current/queue)",
    [
        InstanceParam("callee", "--null-audio --max-calls=4"),
        InstanceParam("caller", "--null-audio --max-calls=4")
    ]
)
