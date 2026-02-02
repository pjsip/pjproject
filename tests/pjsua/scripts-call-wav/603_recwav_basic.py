from inc_cfg import *

# Basic recwav start/stop test
# Test starts a call, caller plays audio, callee records it using 'call recwav'
test_param = TestParam(
    "Call recwav start/stop basic test",
    [
        InstanceParam("callee", "--null-audio --max-calls=4"),
        InstanceParam("caller", "--null-audio --max-calls=4")
    ]
)
