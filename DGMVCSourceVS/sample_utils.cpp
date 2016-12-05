/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include <math.h>
#include <iostream>
#include "sample_utils.h"
#include "sample_defs.h"
#include "mfxcommon.h"
#include "global.h"

#pragma warning( disable : 4748 )

CSmplBitstreamReader::CSmplBitstreamReader()
{
	m_fSource = NULL;
	m_bInited = false;
}

CSmplBitstreamReader::~CSmplBitstreamReader()
{
	Close();
}

void CSmplBitstreamReader::Close()
{
	if (m_fSource) {
		fclose(m_fSource);
		m_fSource = NULL;
	}

	m_bInited = false;
}

void CSmplBitstreamReader::Reset()
{
	_fseeki64(m_fSource, 0, SEEK_SET);
}

mfxStatus CSmplBitstreamReader::Init(const msdk_char *strFileName)
{
	MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
	MSDK_CHECK_ERROR(msdk_strlen(strFileName), 0, MFX_ERR_NOT_INITIALIZED);

	Close();

	//open file to read input stream
	MSDK_FOPEN(m_fSource, strFileName, MSDK_STRING("rb"));
	MSDK_CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);

	m_bInited = true;
	return MFX_ERR_NONE;
}

mfxStatus CSmplBitstreamReader::ReadNextFrame(mfxBitstream *pBS, VOID *_sp)
{
	MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
	MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);
	mfxU32 nBytesRead = 0;
	mfxU32 nBytesAvailable;
	HANDLE hArray[2];
	START_PARAMS *sp = (START_PARAMS *)_sp;
	unsigned char **_rp = sp->_rp;
	unsigned char **_wp = sp->_wp;

	memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
	pBS->DataOffset = 0;
	if (!sp->hCombinerBlocked) {
		// The combiner is not inited so we must be reading a combined stream.
		nBytesRead = (mfxU32)fread(pBS->Data + pBS->DataLength, 1, pBS->MaxLength - pBS->DataLength, m_fSource);
	} else {
		// Reading from the combined stream buffer.
		nBytesAvailable = (*_rp > *_wp) ? (BUF_SIZE - (*_rp - *_wp)) : (*_wp - *_rp);
		if (nBytesAvailable) {
			if (pBS->MaxLength - pBS->DataLength > nBytesAvailable) {
				nBytesRead = nBytesAvailable;
			} else
				nBytesRead = pBS->MaxLength - pBS->DataLength;
			if (nBytesRead + *_rp >= sp->combined_buffer + BUF_SIZE) {
				int num = sp->combined_buffer + BUF_SIZE - *_rp;
				memcpy(pBS->Data + pBS->DataLength, *_rp, num);
				*_rp = sp->combined_buffer;
				memcpy(pBS->Data + pBS->DataLength + num, *_rp, nBytesRead - num);
				*_rp += nBytesRead - num;
			} else {
				memcpy(pBS->Data + pBS->DataLength, *_rp, nBytesRead);
				*_rp += nBytesRead;
				if (*_rp >= sp->combined_buffer + BUF_SIZE)
					*_rp -= BUF_SIZE;
			}
			// Unblock the combiner to fill the output buffer with combined data.
			ResetEvent(sp->hCombinerBlocked);
			//			OutputDebugString("DGMVCSource: ReadNextFrame(): Set hUnblockCombiner\n");
			SetEvent(sp->hUnblockCombiner);
			hArray[0] = sp->hCombinerBlocked;
			hArray[1] = sp->hCombinerEOF;
			//			OutputDebugString("DGMVCSource: ReadNextFrame(): Wait for hCombinerBlocked or hCombinerEOF...\n");
			DWORD res = WaitForMultipleObjects(2, hArray, FALSE, INFINITE);
			//			OutputDebugString("DGMVCSource: ReadNextFrame(): Got hCombinerBlocked or hCombinerEOF\n");
		}
	}
	if (0 == nBytesRead) {
		return MFX_ERR_MORE_DATA;
	}
	pBS->DataLength += nBytesRead;

	return MFX_ERR_NONE;
}

mfxStatus MoveMfxBitstream(mfxBitstream *pTarget, mfxBitstream *pSrc, mfxU32 nBytes)
{
	MSDK_CHECK_POINTER(pTarget, MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(pSrc, MFX_ERR_NULL_PTR);

	mfxU32 nFreeSpaceTail = pTarget->MaxLength - pTarget->DataOffset - pTarget->DataLength;
	mfxU32 nFreeSpace = pTarget->MaxLength - pTarget->DataLength;

	MSDK_CHECK_NOT_EQUAL(pSrc->DataLength >= nBytes, true, MFX_ERR_MORE_DATA);
	MSDK_CHECK_NOT_EQUAL(nFreeSpace >= nBytes, true, MFX_ERR_NOT_ENOUGH_BUFFER);

	if (nFreeSpaceTail < nBytes) {
		memmove(pTarget->Data, pTarget->Data + pTarget->DataOffset, pTarget->DataLength);
		pTarget->DataOffset = 0;
	}
	MSDK_MEMCPY_BITSTREAM(*pTarget, pTarget->DataOffset, pSrc->Data + pSrc->DataOffset, nBytes);
	pTarget->DataLength += nBytes;
	pSrc->DataLength -= nBytes;
	pSrc->DataOffset += nBytes;

	return MFX_ERR_NONE;
}

mfxU16 GetFreeSurfaceIndex(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize)
{
	if (pSurfacesPool) {
		for (mfxU16 i = 0; i < nPoolSize; i++) {
			if (0 == pSurfacesPool[i].Data.Locked) {
				return i;
			}
		}
	}

	return MSDK_INVALID_SURF_IDX;
}

mfxU16 GetFreeSurface(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize)
{
	mfxU32 SleepInterval = 1; // milliseconds

	mfxU16 idx = MSDK_INVALID_SURF_IDX;

	//wait if there's no free surface
	for (mfxU32 i = 0; i < MSDK_WAIT_INTERVAL; i += SleepInterval) {
		idx = GetFreeSurfaceIndex(pSurfacesPool, nPoolSize);

		if (MSDK_INVALID_SURF_IDX != idx) {
			break;
		} else {
			MSDK_SLEEP(SleepInterval);
		}
	}

	return idx;
}

mfxU16 GetFreeSurfaceIndex(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize, mfxU16 step)
{
	if (pSurfacesPool) {
		for (mfxU16 i = 0; i < nPoolSize; i = (mfxU16)(i + step), pSurfacesPool += step) {
			if (0 == pSurfacesPool[0].Data.Locked) {
				return i;
			}
		}
	}

	return MSDK_INVALID_SURF_IDX;
}

mfxStatus InitMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize)
{
	//check input params
	MSDK_CHECK_POINTER(pBitstream, MFX_ERR_NULL_PTR);
	MSDK_CHECK_ERROR(nSize, 0, MFX_ERR_NOT_INITIALIZED);

	//prepare pBitstream
	WipeMfxBitstream(pBitstream);

	//prepare buffer
	pBitstream->Data = new mfxU8[nSize];
	MSDK_CHECK_POINTER(pBitstream->Data, MFX_ERR_MEMORY_ALLOC);

	pBitstream->MaxLength = nSize;

	return MFX_ERR_NONE;
}

mfxStatus ExtendMfxBitstream(mfxBitstream* pBitstream, mfxU32 nSize)
{
	MSDK_CHECK_POINTER(pBitstream, MFX_ERR_NULL_PTR);

	MSDK_CHECK_ERROR(nSize <= pBitstream->MaxLength, true, MFX_ERR_UNSUPPORTED);

	mfxU8* pData = new mfxU8[nSize];
	MSDK_CHECK_POINTER(pData, MFX_ERR_MEMORY_ALLOC);

	memmove(pData, pBitstream->Data + pBitstream->DataOffset, pBitstream->DataLength);

	WipeMfxBitstream(pBitstream);

	pBitstream->Data = pData;
	pBitstream->DataOffset = 0;
	pBitstream->MaxLength = nSize;

	return MFX_ERR_NONE;
}

void WipeMfxBitstream(mfxBitstream* pBitstream)
{
	MSDK_CHECK_POINTER(pBitstream);

	//free allocated memory
	MSDK_SAFE_DELETE_ARRAY(pBitstream->Data);
}

// function for getting a pointer to a specific external buffer from the array
mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId)
{
	if (!ebuffers) return 0;
	for (mfxU32 i = 0; i < nbuffers; i++) {
		if (!ebuffers[i]) continue;
		if (ebuffers[i]->BufferId == BufferId) {
			return ebuffers[i];
		}
	}
	return 0;
}

mfxVersion getMinimalRequiredVersion(const APIChangeFeatures &features)
{
	mfxVersion version = { 1, 1 };

	if (features.MVCDecode || features.MVCEncode || features.LowLatency || features.JpegDecode) {
		version.Minor = 3;
	}

	if (features.ViewOutput) {
		version.Minor = 4;
	}

	if (features.JpegEncode || features.IntraRefresh) {
		version.Minor = 6;
	}

	if (features.LookAheadBRC) {
		version.Minor = 7;
	}

	if (features.AudioDecode) {
		version.Minor = 8;
	}

	return version;
}

namespace {
int g_trace_level = MSDK_TRACE_LEVEL_INFO;
}

int msdk_trace_get_level()
{
	return g_trace_level;
}

void msdk_trace_set_level(int newLevel)
{
	g_trace_level = newLevel;
}

bool msdk_trace_is_printable(int level)
{
	return g_trace_level >= level;
}

msdk_ostream & operator <<(msdk_ostream & os, MsdkTraceLevel tl)
{
	switch (tl) {
	case MSDK_TRACE_LEVEL_CRITICAL:
		os << MSDK_STRING("CRITICAL");
		break;
	case MSDK_TRACE_LEVEL_ERROR:
		os << MSDK_STRING("ERROR");
		break;
	case MSDK_TRACE_LEVEL_WARNING:
		os << MSDK_STRING("WARNING");
		break;
	case MSDK_TRACE_LEVEL_INFO:
		os << MSDK_STRING("INFO");
		break;
	case MSDK_TRACE_LEVEL_DEBUG:
		os << MSDK_STRING("DEBUG");
		break;
	}
	return os;
}

msdk_string NoFullPath(const msdk_string & file_path)
{
	size_t pos = file_path.find_last_of(MSDK_CHAR("\\/"));
	if (pos != msdk_string::npos) {
		return file_path.substr(pos + 1);
	}
	return file_path;
}
