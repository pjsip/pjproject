
Media
=====
Media objects are objects that are capable to either produce media or takes media.

An important subclass of Media is AudioMedia which represents audio media. There are several type of audio media objects supported in PJSUA2:

- Capture device's AudioMedia, to capture audio from the sound device.
- Playback device's AudioMedia, to play audio to the sound device.
- Call's AudioMedia, to transmit and receive audio to/from remote person.
- AudioMediaPlayer, to play WAV file(s).
- AudioMediaRecorder, to record audio to a WAV file.

More media objects may be added in the future.

The Audio Conference Bridge
----------------------------
The conference bridge provides a simple but yet powerful concept to manage audio flow between the audio medias. The principle is very simple, that is you connect audio source to audio destination, and the bridge will make the audio flows from the source to destination, and that's it. If more than one sources are transmitting to the same destination, then the audio from the sources will be mixed. If one source is transmitting to more than one destinations, the bridge will take care of duplicating the audio from the source to the multiple destinations. The bridge will even take care medias with different clock rates and ptime.

In PJSUA2, all audio media objects are plugged-in to the central conference bridge for easier manipulation. At first, a plugged-in audio media will not be connected to anything, so media will not flow from/to any objects. An audio media source can start/stop the transmission to a destination by using the API AudioMedia.startTransmit() / AudioMedia.stopTransmit().

An audio media object plugged-in to the conference bridge will be given a port ID number that identifies the object in the bridge. Application can use the API AudioMedia.getPortId() to retrieve the port ID. Normally, application should not need to worry about the conference bridge and its port ID (as all will be taken care of by the Media class) unless application want to create its own custom audio media.

Playing a WAV File
++++++++++++++++++
To playback the WAV file to the sound device, just start the transmission of the WAV playback object to the sound device's playback media:

.. code-block:: c++

    AudioMediaPlayer player;
    AudioMedia& play_med = Endpoint::instance().audDevManager().getPlaybackDevMedia();
    try {
        player.createPlayer("file.wav");
        player.startTransmit(play_med);
    } catch(Error& err) {
    }

By default, the WAV file will be played in a loop. To disable the loop, specify ``PJMEDIA_FILE_NO_LOOP`` when creating the player:

.. code-block:: c++

        player.createPlayer("file.wav", PJMEDIA_FILE_NO_LOOP);

Without looping, silence will be played once the playback has reached the end of the WAV file.

Once you're done with the playback, just stop the transmission to stop the playback:

.. code-block:: c++

    try {
        player.stopTransmit(play_med);
    } catch(Error& err) {
    }

Resuming the transmission after the playback is stopped will resume playback from the last play position. Use ``player.setPos()`` to set playback position to a desired location.


Recording to WAV File
+++++++++++++++++++++
Or if you want to record the audio from the sound device to the WAV file, simply do this:

.. code-block:: c++

    AudioMediaRecorder recorder;
    AudioMedia& cap_med = Endpoint::instance().audDevManager().getCaptureDevMedia();
    try {
        recorder.createRecorder("file.wav");
        cap_med.startTransmit(recorder);
    } catch(Error& err) {
    }

And the media will flow from the sound device to the WAV record file. As usual, to stop or pause recording, just stop the transmission:

.. code-block:: c++

    try {
       cap_med.stopTransmit(recorder);
    } catch(Error& err) {
    }

Note that stopping the transmission to the WAV recorder as above does not close the WAV file, and you can resume recording by connecting a source to the WAV recorder again. You cannot playback the recorded WAV file before you close it. To close the WAV recorder, simply delete it:

.. code-block:: c++

    delete recorder;


Local Audio Loopback
++++++++++++++++++++
A useful test to check whether the local sound device (capture and playback device) is working properly is by transmitting the audio from the capture device directly to the playback device (i.e. local loopback). You can do this by:

.. code-block:: c++

    cap_med.startTransmit(play_med);


Looping Audio
+++++++++++++
If you want, you can loop the audio of an audio media object to itself (i.e. the audio received from the object will be transmitted to itself). You can loop-back audio from any objects, as long as the object has bidirectional media. That means you can loop the call's audio media, so that audio received from the remote person will be transmitted back to her/him. But you can't loop the WAV player or recorder since these objects can only play or record and not both.

Normal Call
+++++++++++

A single call can have more than one media (for example, audio and video). Application can retrieve the audio media by using the API Call.getMedia(). Then for a normal call, we would want to establish bidirectional audio with the remote person, which can be done easily by connecting the sound device and the call audio media and vice versa:

.. code-block:: c++

    CallInfo ci = call.getInfo();
    AudioMedia *aud_med = NULL;

    // Find out which media index is the audio
    for (unsigned i=0; i<ci.media.size(); ++i) {
        if (ci.media[i].type == PJMEDIA_TYPE_AUDIO) {
            aud_med = (AudioMedia *)call.getMedia(i);
            break;
        }
    }

    if (aud_med) {
        // This will connect the sound device/mic to the call audio media
        cap_med.startTransmit(*aud_med);

        // And this will connect the call audio media to the sound device/speaker
        aud_med->startTransmit(play_med);
    }



Second Call
+++++++++++
Suppose we want to talk with two remote parties at the same time. Since we already have bidirectional media connection with one party, we just need to add bidirectional connection with the other party using the code below:

.. code-block:: c++

    AudioMedia *aud_med2 = (AudioMedia *)call2.getMedia(aud_idx);
    if (aud_med2) {
        cap_med->startTransmit(*aud_med2);
        aud_med2->startTransmit(play_med);
    }

Now we can talk to both parties at the same time, and we will hear audio from either party. But at this stage, the remote parties can't talk or hear each other (i.e. we're not in full conference mode yet).

Conference Call
+++++++++++++++
To enable both parties talk to each other, just establish bidirectional media between them:

.. code-block:: c++

    aud_med->startTransmit(*aud_med2);
    aud_med2->startTransmit(*aud_med);

Now the three parties (us and both remote parties) will be able to talk to each other.

Recording the Conference
++++++++++++++++++++++++

While doing the conference, it perfectly makes sense to want to record the conference to a WAV file, and all we need to do is to connect the microphone and both calls to the WAV recorder:

.. code-block:: c++

    cap_med.startTransmit(recorder);
    aud_med->startTransmit(recorder);
    aud_med2->startTransmit(recorder);


Audio Device Management
-----------------------
Please see `Audio Device Framework <#auddev>`_ below.


Class Reference
---------------
Media Framework
+++++++++++++++
Classes
~~~~~~~
.. doxygenclass:: pj::Media
        :path: xml
        :members:

.. doxygenclass:: pj::AudioMedia
        :path: xml
        :members:

.. doxygenclass:: pj::AudioMediaPlayer
        :path: xml
        :members:

.. doxygenclass:: pj::AudioMediaRecorder
        :path: xml
        :members:

Formats and Info
~~~~~~~~~~~~~~~~
.. doxygenstruct:: pj::MediaFormat
        :path: xml

.. doxygenstruct:: pj::MediaFormatAudio
        :path: xml

.. doxygenstruct:: pj::MediaFormatVideo
        :path: xml

.. doxygenstruct:: pj::ConfPortInfo
        :path: xml

Audio Device Framework
++++++++++++++++++++++
Device Manager
~~~~~~~~~~~~~~
.. _auddev:
.. doxygenclass:: pj::AudDevManager
        :path: xml
        :members:

Device Info
~~~~~~~~~~~
.. doxygenstruct:: pj::AudioDevInfo
        :path: xml

