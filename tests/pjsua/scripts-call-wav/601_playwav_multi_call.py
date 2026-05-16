from inc_cfg import *

# Test playwav with multiple calls to verify call-id parameter
# This test creates two calls and verifies that playwav can target specific calls by ID
test_param = TestParam(
    "Call playwav with multiple calls (call-id test)",
    [
        InstanceParam("callee1", "--null-audio --max-calls=4"),
        InstanceParam("callee2", "--null-audio --max-calls=4"),
        InstanceParam("caller", "--null-audio --max-calls=4")
    ]
)
