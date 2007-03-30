#!/bin/sh
echo gcc -Wall speexclient.c alsa_device.c -o speexclient -lspeex -lasound -lm
gcc -Wall speexclient.c alsa_device.c -o speexclient -lspeex -lasound -lm
