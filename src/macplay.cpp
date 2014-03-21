/*
 * Copyright 1997-2003 Samuel Audet <guardia@step.polymtl.ca>
 *                     Taneli Lepp� <rosmo@sektori.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* The Monkey's Audio Codec player plug-in for PM123 */

#define INCL_DOS
#define INCL_PM
#include <os2.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "format.h"
#include "decoder_plug.h"
#include "plugin.h"
#include "wav.h"

#include "All.h"
#include "GlobalFunctions.h"
#include "MACLib.h"
#include "CharacterHelper.h"
#include "APETag.h"

typedef struct
{
   WAV wavfile;
   FORMAT_INFO formatinfo;

   int (* _System output_play_samples)(void *a, FORMAT_INFO *format,char *buf,int len, int posmarker);
   void *a; /* only to be used with the precedent function */
   int buffersize;

   HEV play,ok;
   char filename[1024];
   BOOL stop,rew,ffwd;
   int jumpto;
   int status;

   ULONG decodertid;

   void (* _System error_display)(char *);
   HEV playsem;
   HWND hwnd;

   int last_length; // keeps the last length in memory to calls to
                    // decoder_length() remain valid after the file is closed
} WAVPLAY;


static void decoder_thread(void *arg)
{
   WAVPLAY *w = (WAVPLAY *) arg;
   ULONG resetcount;

   while(1)
   {
      char *buffer = NULL;
      int read = 0;

      DosWaitEventSem(w->play, (ULONG)-1);
      DosResetEventSem(w->play,&resetcount);

      w->status = DECODER_STARTING;
      buffer = (char*)malloc(w->buffersize);

      w->last_length = -1;

      DosResetEventSem(w->playsem,&resetcount);
      DosPostEventSem(w->ok);

      if(w->wavfile.open(w->filename,w->formatinfo.samplerate,
            w->formatinfo.channels,w->formatinfo.bits,w->formatinfo.format))
      {
         WinPostMsg(w->hwnd,WM_PLAYERROR,0,0);
         w->status = DECODER_STOPPED;
         DosPostEventSem(w->playsem);
         continue;
      }

      w->jumpto = -1;
      w->rew = w->ffwd = 0;
      w->stop = 0;

      w->status = DECODER_PLAYING;
      w->last_length = decoder_length(w);

      while((read = w->wavfile.readData(buffer,w->buffersize)) > 0 && !w->stop)
      {
         int written = w->output_play_samples(w->a, &w->formatinfo, buffer, read, (int)w->wavfile.filepos() );

         if(written < read)
         {
            WinPostMsg(w->hwnd,WM_PLAYERROR,0,0);
            break;
         }

         if(w->jumpto >= 0)
         {
            w->wavfile.jumpto(w->jumpto);
            w->jumpto = -1;
            WinPostMsg(w->hwnd,WM_SEEKSTOP,0,0);
         }
         if(w->rew)
         {
            w->wavfile.jumpto(w->wavfile.filepos()-1000);
         }
         if(w->ffwd)
         {
            w->wavfile.jumpto(w->wavfile.filepos()+1000);
         }
      }
      free(buffer); buffer = NULL;
      w->status = DECODER_STOPPED;
      w->wavfile.close();

      DosPostEventSem(w->playsem);
      WinPostMsg(w->hwnd,WM_PLAYSTOP,0,0);

      DosPostEventSem(w->ok);
   }
}

int _System decoder_init(void **W)
{
   WAVPLAY *w;

   *W = malloc(sizeof(WAVPLAY));
   w = (WAVPLAY *)*W;
   memset(w,0,sizeof(WAVPLAY));

   DosCreateEventSem(NULL,&w->play,0,FALSE);
   DosCreateEventSem(NULL,&w->ok,0,FALSE);

   w->decodertid = _beginthread(decoder_thread,0,64*1024,(void *) w);
   if(w->decodertid != -1)
      return w->decodertid;
   else
   {
      DosCloseEventSem(w->play);
      DosCloseEventSem(w->ok);
      free(w);
      return -1;
   }
}

BOOL _System decoder_uninit(void *W)
{
   WAVPLAY *w = (WAVPLAY *) W;
   int decodertid = w->decodertid;

   DosCloseEventSem(w->play);
   DosCloseEventSem(w->ok);

   free(w);

   return !DosKillThread(decodertid);
}


ULONG _System decoder_command(void *W, ULONG msg, DECODER_PARAMS *params)
{
   WAVPLAY *w = (WAVPLAY *) W;
   ULONG resetcount;

   switch(msg)
   {
      case DECODER_PLAY:
         if(w->status == DECODER_STOPPED)
         {
            strncpy(w->filename, params->filename,sizeof(w->filename)-1);
            DosResetEventSem(w->ok,&resetcount);
            DosPostEventSem(w->play);
            if(DosWaitEventSem(w->ok, 10000) == 640)
            {
               w->status = DECODER_STOPPED;
               DosKillThread(w->decodertid);
               w->decodertid = _beginthread(decoder_thread,0,64*1024,(void *) w);
               return 102;
            }
         }
         else
            return 101;
         break;

      case DECODER_STOP:
         if(w->status != DECODER_STOPPED)
         {
            DosResetEventSem(w->ok,&resetcount);
            w->stop = TRUE;
            if(DosWaitEventSem(w->ok, 10000) == 640)
            {
               w->status = DECODER_STOPPED;
               DosKillThread(w->decodertid);
               w->decodertid = _beginthread(decoder_thread,0,64*1024,(void *) w);
               return 102;
            }
         }
         else
            return 101;
         break;

      case DECODER_FFWD:
         w->ffwd = params->ffwd;
         break;

      case DECODER_REW:
         w->rew = params->rew;
         break;

      /* I multiply by the channels and bits last because I need to fall on
         a byte which is a multiple of those or else I'll get garbage */
      case DECODER_JUMPTO:
         w->jumpto = params->jumpto;
         break;

      case DECODER_EQ:
         return 1;

      case DECODER_SETUP:
         w->output_play_samples = params->output_play_samples;
         w->a = params->a;
         w->buffersize = params->audio_buffersize;
         w->error_display = params->error_display;
         w->hwnd = params->hwnd;
         w->playsem = params->playsem;
         DosPostEventSem(w->playsem);
         break;
   }
   return 0;
}

ULONG _System decoder_length(void *W)
{
   WAVPLAY *w = (WAVPLAY *) W;

   if(w->status == DECODER_PLAYING)
      w->last_length = (int)w->wavfile.filelength();

   return w->last_length;
}

ULONG _System decoder_status(void *W)
{
   WAVPLAY *w = (WAVPLAY *) W;
   return w->status;
}

ULONG _System decoder_fileinfo(char *filename, DECODER_INFO *info)
{
   memset(info,0,sizeof(*info));
   info->size = sizeof(*info);
   info->mpeg = 0;
   info->numchannels = 0;

	int					nRetVal = 0;										// generic holder for return values
	IAPEDecompress *	pAPEDecompress = NULL;								// APE interface
	CSmartPtr<wchar_t> spInput;

	spInput.Assign( GetUTF16FromANSI( filename ), TRUE );

	pAPEDecompress = CreateIAPEDecompress( spInput, &nRetVal );
	if(pAPEDecompress) {
		info->songlength = pAPEDecompress->GetInfo(APE_INFO_LENGTH_MS);
		info->format.samplerate = pAPEDecompress->GetInfo(APE_INFO_SAMPLE_RATE);
		info->format.channels = pAPEDecompress->GetInfo(APE_INFO_CHANNELS);
		info->format.bits = pAPEDecompress->GetInfo(APE_INFO_BITS_PER_SAMPLE);
		info->format.format = WAVE_FORMAT_PCM;
		sprintf( info->tech_info, "%d bits, %.1f kHz, %s",
			pAPEDecompress->GetInfo(APE_INFO_BITS_PER_SAMPLE),
			(float)pAPEDecompress->GetInfo(APE_INFO_SAMPLE_RATE) / 1000.0f,
			pAPEDecompress->GetInfo(APE_INFO_CHANNELS) == 1 ? "Mono" : "Stereo" );
		delete pAPEDecompress;
	}

	return (nRetVal != 0) ? 200 : 0;
}

ULONG _System decoder_trackinfo(char *drive, int track, DECODER_INFO *info)
{
   return 200;
}

ULONG _System decoder_cdinfo(char *drive, DECODER_CDINFO *info)
{
   return 100;
}


ULONG _System decoder_support(char *ext[], int *size)
{
   if(size)
   {
      if(ext != NULL && *size >= 1)
      {
         strcpy(ext[0],"*.ape");
      }
      *size = 1;
   }

   return DECODER_FILENAME;
}

void _System plugin_query(PLUGIN_QUERYPARAM *param)
{
   param->type = PLUGIN_DECODER;
   param->author = "SofiyaCat";
   param->desc = "Monkey's Audio Play 0.00";
   param->configurable = FALSE;
}

