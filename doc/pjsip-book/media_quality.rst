
Media Quality
*************

Audio Quality
=============
If you experience any problem with the audio quality, you may want to try the steps below:

1. Follow the guide: `Test the sound device using pjsystest`_.
2. Identify the sound problem and troubleshoot it using the steps described in: `Checking for sound problems`_.

.. _`Checking for sound problems`: http://trac.pjsip.org/repos/wiki/sound-problems
.. _`Test the sound device using pjsystest`: http://trac.pjsip.org/repos/wiki/Testing_Audio_Device_with_pjsystest

It is probably easier to do the testing using lower level API such as PJSUA since we already have a built-in pjsua sample app located in pjsip-apps/bin to do the testing. However, you can also do the testing in your application using PJSUA2 API such as local audio loopback, recording to WAV file as explained in the Media chapter previously.

Video Quality
=============
For video quality problems, the steps are as follows:

1. For lack of video, check account's AccountVideoConfig, especially the fields autoShowIncoming and autoTransmitOutgoing. More about the video API is explained in `Video Users Guide`_.
2. Check local video preview using PJSUA API as described in `Video Users Guide-Video Preview API`_.
3. Since video requires a larger bandwidth, we need to check for network impairments as described in `Checking Network Impairments`_. The document is for troubleshooting audio problem but it applies for video as well.
4. Check the CPU utilization. If the CPU utilization is too high, you can try a different (less CPU-intensive) video codec or reduce the resolution/fps. A general guide on how to reduce CPU utilization can be found here: `FAQ-CPU utilization`_.

.. _`Video Users Guide`: http://trac.pjsip.org/repos/wiki/Video_Users_Guide
.. _`Video Users Guide-Video Preview API`: http://trac.pjsip.org/repos/wiki/Video_Users_Guide#VideopreviewAPI
.. _`Checking Network Impairments`: http://trac.pjsip.org/repos/wiki/audio-check-packet-loss
.. _`FAQ-CPU utilization`: http://trac.pjsip.org/repos/wiki/FAQ#cpu

