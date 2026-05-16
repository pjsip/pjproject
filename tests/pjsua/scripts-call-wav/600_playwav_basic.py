from inc_cfg import *

# Basic playwav start/stop test with single call
# Test starts a call, plays a WAV file using 'call playwav start', then stops it
test_param = TestParam(
    "Call playwav start/stop basic test",
    [
        InstanceParam("callee", "--null-audio --max-calls=4"),
        InstanceParam("caller", "--null-audio --max-calls=4")
    ]
)
