#pragma once

#include "global.h"
#include "vapoursynth/VapourSynth.h"

struct MVCSourceVS
{
	VSVideoInfo vi;
	YV12PICT *out, *saved;
	START_PARAMS start_params;
	int view;
	int hw_mode;
	bool timed_out;
	int expected_frame_no;

	int fcount;
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
	bool reached_eof;
	unsigned char *combined_buffer;
	unsigned char *rp, *wp;
};
