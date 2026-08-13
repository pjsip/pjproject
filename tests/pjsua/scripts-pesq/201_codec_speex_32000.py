#
from inc_cfg import *

# Call with Speex/32000 (UWB) codec.
# Note: the PESQ tool only scores 8kHz/16kHz WAV files, so the conference
# bridge/device clock rate is kept at 16000 (input/output WAVs stay
# xxx.16.wav) while the codec itself negotiates and runs at its native
# 32000 clock rate; PJMEDIA transparently resamples between the two.
test_param = TestParam(
		"PESQ codec Speex UWB (RX side uses snd dev)",
		[
			InstanceParam("UA1", "--max-calls=1 --clock-rate 16000 --add-codec speex/32000 --play-file wavs/input.16.wav --null-audio"),
			InstanceParam("UA2", "--max-calls=1 --clock-rate 16000 --add-codec speex/32000 --rec-file  wavs/tmp.16.wav   --auto-answer 200")
		]
		)

if (HAS_SND_DEV == 0):
	test_param.skip = True

pesq_threshold = 2.8
