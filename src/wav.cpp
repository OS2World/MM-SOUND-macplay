/*
 * Copyright 1997-2003 Samuel Audet <guardia@step.polymtl.ca>
 *                     Taneli Lepp„ <rosmo@sektori.com>
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

#define INCL_DOS
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <sys\types.h>
#include <memory.h>

#include "wav.h"

#include "All.h"
#include "APEInfo.h"
#include "APECompress.h"
#include "APEDecompress.h"
#include "WAVInputSource.h"
#include IO_HEADER_FILE
#include "MACProgressHelper.h"
#include "GlobalFunctions.h"
#include "MD5.h"
#include "CharacterHelper.h"

//#define BLOCKS_PER_DECODE               9216
#define BLOCKS_PER_DECODE               1024

WAV::WAV()
{
	pAPEDecompress = NULL;
	m_pWavPool = 0;
}

WAV::~WAV()
{
	close();
}

int WAV::open(char *filename, int &samplerate,
              int &channels, int &bits, int &format)
{
	m_nPos = 0;
	m_nRemain = 0;

	int	nRetVal = 0;
	CSmartPtr<wchar_t> spInput;

	spInput.Assign( GetUTF16FromANSI( filename ), TRUE );

	pAPEDecompress = CreateIAPEDecompress( spInput, &nRetVal );
	if(pAPEDecompress) {
		bits = pAPEDecompress->GetInfo(APE_INFO_BITS_PER_SAMPLE);
		samplerate = pAPEDecompress->GetInfo(APE_INFO_SAMPLE_RATE);
		channels = pAPEDecompress->GetInfo(APE_INFO_CHANNELS);
		format = WAVE_FORMAT_PCM;
		if(m_pWavPool) {
			delete [] m_pWavPool;
		}
		m_pWavPool = new unsigned char [pAPEDecompress->GetInfo(APE_INFO_BLOCK_ALIGN) * BLOCKS_PER_DECODE];
	}

	return (nRetVal != 0) ? NO_WAV_FILE : 0;
}

int WAV::close()
{
	if(pAPEDecompress) {
		delete pAPEDecompress;
	}
	if(m_pWavPool) {
		delete [] m_pWavPool;
		m_pWavPool = 0;
	}
	
	return 0;
}

int WAV::readData(char *buffer, int bytes)
{
	int total = 0;
	while(bytes > 0) {
		if(m_nRemain) {
			register int n = min( bytes, m_nRemain );
			memcpy( buffer, &m_pWavPool[m_nPos], n );

			m_nRemain -= n;
			m_nPos    += n;
			bytes     -= n;
			buffer    += n;
			total     += n;
		} else {
			int nBlocksDecoded = -1;
			int nRetVal = pAPEDecompress->GetData( (char*)m_pWavPool, BLOCKS_PER_DECODE, &nBlocksDecoded);
			if(nRetVal || nBlocksDecoded <= 0) {
				break;
			}
			m_nRemain = (nBlocksDecoded * pAPEDecompress->GetInfo(APE_INFO_BLOCK_ALIGN));
			m_nPos = 0;
		}
	}
	return total;
}

int WAV::filepos()
{
	if(pAPEDecompress) {
		return pAPEDecompress->GetInfo(APE_DECOMPRESS_CURRENT_MS);
	}
	return 0;
}

int WAV::filelength()
{
	if(pAPEDecompress) {
		return pAPEDecompress->GetInfo(APE_INFO_LENGTH_MS);
	}
	return 0;
}

int WAV::jumpto(long offset)
{
	if(pAPEDecompress) {
		int block = (int)((double)offset * (double)pAPEDecompress->GetInfo( APE_INFO_SAMPLE_RATE ) / 1000.0);
		if(0 <= block && block < pAPEDecompress->GetInfo( APE_INFO_TOTAL_BLOCKS )) {
			pAPEDecompress->Seek( block );

			m_nPos = 0;
			m_nRemain = 0;
		}
	}

	return 0;
}

