/*
*  DGMVCDecode -- MVC decoder/frame server
*  Copyright (C) 2014 Donald A. Graft
*/

#include <windows.h>
#include "global.h"
#include "pipeline_decode.h"
#include <sstream>

DWORD WINAPI start_pipeline(LPVOID _sp)
{
	sInputParams Params;   // input parameters from command line
	mfxStatus sts = MFX_ERR_NONE; // return value check
	START_PARAMS *sp = (START_PARAMS *)_sp;
	// VideoInfo *pvi = sp->pvi;
	VSVideoInfo *pvi = sp->pvi;
	CDecodingPipeline Pipeline(sp);

	Params.videoType = MFX_CODEC_AVC;
	Params.bIsMVC = (sp->view == 3 ? false : true);
	Params.memType = SYSTEM_MEMORY;
	Params.bRendering = false;
	Params.bLowLat = false;
	Params.bCalLat = false;
	Params.bOutput = true;
	Params.hw_mode = sp->hw_mode;

	std::string sToMatch = sp->base_path;
	size_t iWLen = MultiByteToWideChar(CP_ACP, 0, sToMatch.c_str(), sToMatch.size(), 0, 0);

	wchar_t *lpwsz = new wchar_t[iWLen + 1];
	MultiByteToWideChar(CP_ACP, 0, sToMatch.c_str(), sToMatch.size(), lpwsz, iWLen);

	lpwsz[iWLen] = 0;
	std::wstring wsToMatch(lpwsz);
	delete[] lpwsz;

	_tcscpy_s(Params.strSrcFile, wsToMatch.c_str());

	if (Params.bIsMVC)
		Pipeline.SetMultiView();

	sts = Pipeline.Init(&Params);
	if (sts != MFX_ERR_NONE)
		return 0;

	pvi->width = Pipeline.m_mfxVideoParams.mfx.FrameInfo.Width;
	pvi->height = Pipeline.m_mfxVideoParams.mfx.FrameInfo.Height;
	if (pvi->height == 1088)
		pvi->height = 1080;
	// pvi->fps_numerator = Pipeline.m_mfxVideoParams.mfx.FrameInfo.FrameRateExtN;
	// pvi->fps_denominator = Pipeline.m_mfxVideoParams.mfx.FrameInfo.FrameRateExtD;
	pvi->fpsNum = Pipeline.m_mfxVideoParams.mfx.FrameInfo.FrameRateExtN;
	pvi->fpsDen = Pipeline.m_mfxVideoParams.mfx.FrameInfo.FrameRateExtD;
	//	OutputDebugString("DGMVCSource: start_pipeline: Set hInitDone\n");
	SetEvent(sp->hInitDone);

	for (;;) {
		sts = Pipeline.RunDecoding();
		if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts) {
			if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts) {
				msdk_printf(MSDK_STRING("\nERROR: Incompatible video parameters detected. Recovering...\n"));
			} else {
				msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned unexpected error. Recovering...\n"));
				sts = Pipeline.ResetDevice();
				MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
			}
			sts = Pipeline.ResetDecoder(&Params);
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
			continue;
		} else {
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
			break;
		}
	}
	return 0;
}
