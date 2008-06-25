# $Id$
#
from inc_cfg import *
from config_site import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Simple call
test_param = TestParam(
		"PESQ codec Speex WB",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --clock-rate 16000 --add-codec speex/16000 --play-file wavs/input.16.wav --auto-play-hangup"),
			InstanceParam("UA2", ADD_PARAM + " --max-calls=1 --clock-rate 16000 --add-codec speex/16000 --rec-file  wavs/tmp.16.wav   --auto-answer 200 --auto-rec")
		]
		)
