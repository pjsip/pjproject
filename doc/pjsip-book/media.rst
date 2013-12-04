

Media
=====
Media objects are objects that are capable to either produce media or takes media. In ​PJMEDIA terms, these objects are implemented as media ports (​pjmedia_port).

An important subclass of Media is AudioMedia which represents audio media. There are several type of audio media objects supported in PJSUA2:

- CallAudioMedia, to transmit and receive audio to/from remote person.
- AudioMediaPlayer, to play WAV file(s).
- AudioMediaRecorder, to record audio to a WAV file.

More media objects may be added in the future.

The Audio Conference Bridge
----------------------------
The conference bridge provides a simple but yet powerful concept to manage audio flow between the audio medias. The principle is very simple, that is you connect audio source to audio destination, and the bridge will make the audio flows from the source to destination, and that's it. If more than one sources are transmitting to the same destination, then the audio from the sources will be mixed. If one source is transmitting to more than one destinations, the bridge will take care of duplicating the audio from the source to the multiple destinations.

In ​PJSUA2, all audio media objects are plugged-in to the central conference bridge for easier manipulation. A plugged-in audio media will not be connected to anything, so media will not flow from/to any objects. An audio media source can start/stop the transmission to a destination by using the API AudioMedia.startTransmit() / AudioMedia.stopTransmit().

An audio media object plugged-in to the conference bridge will be given a port ID number that identifies the object in the bridge. Application can use the API AudioMedia.getPortId() to retrieve the port ID. Normally, application should not need to worry about the port ID (as all will be taken care of by the bridge) unless application want to create its own custom audio media.

Playing a WAV File
++++++++++++++++++
To playback the WAV file to the speaker, just start the transmission of the WAV playback object to the sound device::

    AudioMediaPlayer player;
    try {
        player.createPlayer(“file.wav”);
        player.startTransmit();
    } catch(Error& err) {
    }

Once you're done with the playback, just stop the transmission n to stop the playback::

    try {
        player.stopTransmit();
    } catch(Error& err) {
    }

Recording to WAV File
+++++++++++++++++++++
Or if you want to record the microphone to the WAV file, simply do this::

    AudioMediaRecorder recorder;
    try {
        recorder.createRecorder(“file.wav”);
        .startTransmit(recorder);
    } catch(Error& err) {
    }

And the media will flow from the sound device to the WAV record file. As usual, to stop or pause recording, just stop the transmission::

    try {
       .stopTransmit(recorder);
    } catch(Error& err) {
    }

(Note that stopping the transmission to the WAV recorder as above does not close the WAV file, and you can resume recording by connecting a source to the WAV recorder again. You cannot playback the recorded WAV file before you close it.)

Looping Audio
+++++++++++++
If you want, you can loop the audio of a media object to itself (i.e. the audio received from the object will be transmitted to itself). For example, you can loop the audio of the sound device with::

    .startTransmit();

With the above connection, audio received from the microphone will be played back to the speaker. This is useful to test whether the microphone and speaker are working properly.

You can loop-back audio from any objects, as long as the object has bidirectional media. That means you can loop the call's audio media, so that audio received from the remote person will be transmitted back to her/him. But you can't loop the WAV player or recorder since these objects can only play or record and not both.

Normal Call
+++++++++++

A single call can has several audio medias. Application can retrieve the audio media by using the API Call.getMedia()::

    AudioMedia *aud_med = (AudioMedia *)call.getMedia(0);

In the above, we assume that the audio media is located in index 0. Of course on a real application, we should iterate the medias to find the correct media index by checking whether the media returned by getMedia() is valid. More on this will be explained later in the Call section. Then for a normal call, we would want to establish bidirectional audio with the remote person, which can be done easily by connecting the sound device and the call audio media and vice versa::

    // This will connect the sound device/mic to the call audio media
    ->startTransmit(*aud_med);

    // And this will connect the call audio media to the sound device/speaker
    aud_med->startTransmit();

Second Call
+++++++++++
Suppose we want to talk with two remote parties at the same time. Since we already have bidirectional media connection with one party, we just need to add bidirectional connection with the other party using the code below::

    AudioMedia *aud_med2 = (AudioMedia *)call2.getMedia(0);
    ->startTransmit(*aud_med2);
    aud_med2->startTransmit();

Now we can talk to both parties at the same time, and we will hear audio from either party. But at this stage, the remote parties can't talk or hear each other (i.e. we're not in full conference mode yet).

Conference Call
+++++++++++++++
To enable both parties talk to each other, just establish bidirectional media between them::

    aud_med->startTransmit(*aud_med2);
    aud_med2->startTransmit(*aud_med);

Now the three parties (us and both remote parties) will be able to talk to each other.

Recording the Conference
++++++++++++++++++++++++

While doing the conference, it perfectly makes sense to want to record the conference to a WAV file, and all we need to do is to connect the microphone and both calls to the WAV recorder::

    ->startTransmit(recorder);
    aud_med->startTransmit(recorder);
    aud_med2->startTransmit(recorder);

