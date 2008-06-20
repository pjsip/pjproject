# $Id$
#
from inc_cfg import *

# Simple call
test_param = TestParam(
		"PESQ",
		[
			InstanceParam("UA1", "--null-audio --max-calls=1 --play-file wavs/input.16.wav --auto-play-hangup"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --rec-file  wavs/tmp.16.wav --clock-rate 16000 --auto-answer 200 --auto-rec")
		]
		)
