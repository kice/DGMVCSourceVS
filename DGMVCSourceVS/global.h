#pragma once
/*
 *  DGMVCDecode -- MVC decoder/frame server
 *  Copyright (C) 2014 Donald A. Graft
 */

#include <windows.h>
#include <math.h>
#include <winreg.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include "vapoursynth/VapourSynth.h"

#define BUF_SIZE 4000000
#define HW_AUTO 0
#define HW_OFF 1
#define HW_ON 2

struct YV12PICT
{
	unsigned char *y, *u, *v;
	int ypitch, uvpitch;
	int ywidth, uvwidth;
	int yheight, uvheight;
	int pf;
};

struct START_PARAMS
{
	// IScriptEnvironment* AVSenv;
	char base_path[1024];
	char dependent_path[1024];
	// VideoInfo *pvi;
	VSVideoInfo *pvi;
	int view;
	YV12PICT *dec;
	HANDLE hInitDone;
	HANDLE hFrameReady;
	HANDLE hGetFrame;
	HANDLE hSkipFrame;
	HANDLE hFrameDone;
	HANDLE hUnblockCombiner;
	HANDLE hCombinerBlocked;
	HANDLE hCombinerEOF;
	HANDLE hKillDecoder;
	HANDLE hDecoderDead;
	bool *reached_eof;
	unsigned char *combined_buffer;
	unsigned char **_rp, **_wp;
	int hw_mode;
};
