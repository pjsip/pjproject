# $Id$
#
from inc_cfg import *

# Call with GSM codec
test_param = TestParam(
		"PESQ codec GSM (RX side uses snd dev)",
		[
			InstanceParam("UA1", "--max-calls=1 --add-codec gsm --clock-rate 8000 --play-file wavs/input.8.wav --auto-play-hangup --null-audio"),
			InstanceParam("UA2", "--max-calls=1 --add-codec gsm --clock-rate 8000 --rec-file  wavs/tmp.8.wav   --auto-answer 200 --auto-rec")
		]
		)

if (HAS_SND_DEV == 0):
	test_param.skip = True
