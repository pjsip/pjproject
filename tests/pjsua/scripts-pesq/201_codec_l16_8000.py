#
from inc_cfg import *

# Call with L16/8000/1 codec
test_param = TestParam(
		"PESQ codec L16/8000/1 (RX side uses snd dev)",
		[
			InstanceParam("UA1", "--max-calls=1 --add-codec L16/8000/1 --clock-rate 8000 --play-file wavs/input.8.wav --null-audio"),
			InstanceParam("UA2", "--max-calls=1 --add-codec L16/8000/1 --clock-rate 8000 --rec-file  wavs/tmp.8.wav   --auto-answer 200")
		]
		)

if (HAS_SND_DEV == 0):
	test_param.skip = True

pesq_threshold = 3.5
