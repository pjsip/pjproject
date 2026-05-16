from inc_cfg import *

# Test error handling for invalid call-id and missing files
# Tests various error conditions:
# - Invalid call-id (no call exists)
# - Non-existent file
# - Invalid call-id (out of range)
# - Stop when no playback active
test_param = TestParam(
    "Call playwav error handling test",
    [
        InstanceParam("callee", "--null-audio --max-calls=4"),
        InstanceParam("caller", "--null-audio --max-calls=4")
    ]
)
