#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with L16/8000/2 codec
test_param = TestParam(
		"PESQ defaults pjsua settings",
		[
			InstanceParam("UA1", ADD_PARAM + " --stereo --max-calls=1 --clock-rate 8000 --add-codec L16/8000/2 --play-file wavs/input.2.8.wav"),
			InstanceParam("UA2", "--null-audio --stereo --max-calls=1 --clock-rate 8000 --add-codec L16/8000/2 --rec-file  wavs/tmp.2.8.wav   --auto-answer 200")
		]
		)

pesq_threshold = None
