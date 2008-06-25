# $Id$
#
from inc_cfg import *
from config_site import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Simple call
test_param = TestParam(
		"PESQ codec iLBC",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --add-codec ilbc --clock-rate 8000 --play-file wavs/input.8.wav --auto-play-hangup"),
			InstanceParam("UA2", ADD_PARAM + " --max-calls=1 --add-codec ilbc --clock-rate 8000 --rec-file  wavs/tmp.8.wav   --auto-answer 200 --auto-rec")
		]
		)
