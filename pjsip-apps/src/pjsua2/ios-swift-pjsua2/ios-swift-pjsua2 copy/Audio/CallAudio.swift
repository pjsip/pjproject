/*
See the LICENSE.txt file for this sample’s licensing information.

Abstract:
High-level call audio management functions
*/

import Foundation

private var audioController: AudioController?

func configureAudioSession() {
    print("Configuring audio session")

    if audioController == nil {
        audioController = AudioController()
    }
}

func startAudio() {
    print("Starting audio")

    audioController?.startIOUnit()
}

func stopAudio() {
    print("Stopping audio")

    audioController?.stopIOUnit()
}
