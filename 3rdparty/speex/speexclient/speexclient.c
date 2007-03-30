/***************************************************************************
   Copyright (C) 2004-2006 by Jean-Marc Valin
   Copyright (C) 2006 Commonwealth Scientific and Industrial Research
                      Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   
****************************************************************************/
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */

#include "alsa_device.h"
#include <speex/speex.h>
#include <speex/speex_jitter.h>
#include <speex/speex_preprocess.h>
#include <speex/speex_echo.h>

#include <sched.h>

#define MAX_MSG 1500

#define SAMPLING_RATE 16000
#define FRAME_SIZE 320

int main(int argc, char *argv[])
{
   
   int sd, rc, n;
   int i;
   struct sockaddr_in cliAddr, remoteAddr;
   char msg[MAX_MSG];
   struct hostent *h;
   int local_port, remote_port;
   int nfds;
   struct pollfd *pfds;
   SpeexPreprocessState *preprocess;
   AlsaDevice *audio_dev;
   int tmp;

   if (argc != 5)
   {
      fprintf(stderr, "wrong options\n");
      exit(1);
   }
  
   h = gethostbyname(argv[2]);
   if(h==NULL) {
      fprintf(stderr, "%s: unknown host '%s' \n", argv[0], argv[1]);
      exit(1);
   }

   local_port = atoi(argv[3]);
   remote_port = atoi(argv[4]);
   
   printf("%s: sending data to '%s' (IP : %s) \n", argv[0], h->h_name,
          inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

   {
      remoteAddr.sin_family = h->h_addrtype;
      memcpy((char *) &remoteAddr.sin_addr.s_addr,
            h->h_addr_list[0], h->h_length);
      remoteAddr.sin_port = htons(remote_port);
   }
   /* socket creation */
   sd=socket(AF_INET, SOCK_DGRAM, 0);
   if(sd<0) {
      printf("%s: cannot open socket \n",argv[0]);
      exit(1);
   }

   /* bind any port */
   cliAddr.sin_family = AF_INET;
   cliAddr.sin_addr.s_addr = htonl(INADDR_ANY);
   cliAddr.sin_port = htons(local_port);

   rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
   if(rc<0) {
      printf("%s: cannot bind port\n", argv[0]);
      exit(1);
   }

   /* Setup audio device */
   audio_dev = alsa_device_open(argv[1], SAMPLING_RATE, 1, FRAME_SIZE);
   
   /* Setup the encoder and decoder in wideband */
   void *enc_state, *dec_state;
   enc_state = speex_encoder_init(&speex_wb_mode);
   tmp = 8;
   speex_encoder_ctl(enc_state, SPEEX_SET_QUALITY, &tmp);
   tmp = 2;
   speex_encoder_ctl(enc_state, SPEEX_SET_COMPLEXITY, &tmp);
   dec_state = speex_decoder_init(&speex_wb_mode);
   tmp = 1;
   speex_decoder_ctl(dec_state, SPEEX_SET_ENH, &tmp);
   SpeexBits enc_bits, dec_bits;
   speex_bits_init(&enc_bits);
   speex_bits_init(&dec_bits);
   
   
   struct sched_param param;
   /*param.sched_priority = 40; */
   param.sched_priority = sched_get_priority_min(SCHED_FIFO);
   if (sched_setscheduler(0,SCHED_FIFO,&param))
      perror("sched_setscheduler");

   int send_timestamp = 0;
   int recv_started=0;
   
   /* Setup all file descriptors for poll()ing */
   nfds = alsa_device_nfds(audio_dev);
   pfds = malloc(sizeof(*pfds)*(nfds+1));
   alsa_device_getfds(audio_dev, pfds, nfds);
   pfds[nfds].fd = sd;
   pfds[nfds].events = POLLIN;

   /* Setup jitter buffer using decoder */
   SpeexJitter jitter;
   speex_jitter_init(&jitter, dec_state, SAMPLING_RATE);
   
   /* Echo canceller with 200 ms tail length */
   SpeexEchoState *echo_state = speex_echo_state_init(FRAME_SIZE, 10*FRAME_SIZE);
   tmp = SAMPLING_RATE;
   speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &tmp);

   /* Setup preprocessor and associate with echo canceller for residual echo suppression */
   preprocess = speex_preprocess_state_init(FRAME_SIZE, SAMPLING_RATE);
   speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);
   
   alsa_device_start(audio_dev);
   
   /* Infinite loop on capture, playback and receiving packets */
   while (1)
   {
      /* Wait for either 1) capture 2) playback 3) socket data */
      poll(pfds, nfds+1, -1);
      /* Received packets */
      if (pfds[nfds].revents & POLLIN)
      {
         /*fprintf (stderr, "x");*/
         n = recv(sd, msg, MAX_MSG, 0);
         int recv_timestamp = ((int*)msg)[1];
         int payload = ((int*)msg)[0];
   
         if ((payload & 0x80000000) == 0) 
         {
            /* Put content of the packet into the jitter buffer, except for the pseudo-header */
            speex_jitter_put(&jitter, msg+8, n-8, recv_timestamp);
            recv_started = 1;
         }

      }
      /* Ready to play a frame (playback) */
      if (alsa_device_playback_ready(audio_dev, pfds, nfds))
      {
         short pcm[FRAME_SIZE];
         if (recv_started)
         {
            /* Get audio from the jitter buffer */
            speex_jitter_get(&jitter, pcm, NULL);
         } else {
            for (i=0;i<FRAME_SIZE;i++)
               pcm[i] = 0;
         }
         /* Playback the audio and reset the echo canceller if we got an underrun */
         if (alsa_device_write(audio_dev, pcm, FRAME_SIZE))
            speex_echo_state_reset(echo_state);
         /* Put frame into playback buffer */
         speex_echo_playback(echo_state, pcm);
      }
      /* Audio available from the soundcard (capture) */
      if (alsa_device_capture_ready(audio_dev, pfds, nfds))
      {
         short pcm[FRAME_SIZE], pcm2[FRAME_SIZE];
         char outpacket[MAX_MSG];
         /* Get audio from the soundcard */
         alsa_device_read(audio_dev, pcm, FRAME_SIZE);
         
         /* Perform echo cancellation */
         speex_echo_capture(echo_state, pcm, pcm2);
         for (i=0;i<FRAME_SIZE;i++)
            pcm[i] = pcm2[i];
         
         speex_bits_reset(&enc_bits);
         
         /* Apply noise/echo suppression */
         speex_preprocess_run(preprocess, pcm);
         
         /* Encode */
         speex_encode_int(enc_state, pcm, &enc_bits);
         int packetSize = speex_bits_write(&enc_bits, outpacket+8, MAX_MSG);
         
         /* Pseudo header: four null bytes and a 32-bit timestamp */
         ((int*)outpacket)[0] = htonl(0);
         ((int*)outpacket)[1] = send_timestamp;
         send_timestamp += FRAME_SIZE;
         rc = sendto(sd, outpacket, packetSize+8, 0,
                (struct sockaddr *) &remoteAddr,
                sizeof(remoteAddr));
         
         if(rc<0) {
            printf("cannot send audio data\n");
            close(sd);
            exit(1);
         }
      }
      

   }


   return 0;
}
