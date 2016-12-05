#include "MVCSourceVS.h"
#include "vapoursynth/VapourSynth.h"
#include "vapoursynth/VSHelper.h"

unsigned int gcd(unsigned int u, unsigned int v)
{
	int shift;

	/* GCD(0,v) == v; GCD(u,0) == u, GCD(0,0) == 0 */
	if (u == 0) return v;
	if (v == 0) return u;

	/* Let shift := lg K, where K is the greatest power of 2
		  dividing both u and v. */
	for (shift = 0; ((u | v) & 1) == 0; ++shift) {
		u >>= 1;
		v >>= 1;
	}

	while ((u & 1) == 0)
		u >>= 1;

	/* From here on, u is always odd. */
	do {
		/* remove all factors of 2 in v -- they are not common */
		/*   note: v is not zero, so while will terminate */
		while ((v & 1) == 0)  /* Loop X */
			v >>= 1;

		/* Now u and v are both odd. Swap if necessary so u <= v,
		   then set v = v - u (which is even). For bignums, the
		   swapping is just pointer movement, and the subtraction
		   can be done in-place. */
		if (u > v) {
			unsigned int t = v; v = u; u = t;
		}  // Swap u and v.
		v = v - u;                       // Here v >= u.
	} while (v != 0);

	/* restore common factors of 2 */
	return u << shift;
}

static const VSFrameRef *VS_CC vs_filter_get_frame(int n, int activation_reason, void **instance_data, void **frame_data, VSFrameContext *frame_ctx, VSCore *core, const VSAPI *vsapi)
{
	if (activation_reason != arInitial)
		return NULL;

	int i = 0;
	DWORD res = 0;

	MVCSourceVS *mvcs = (MVCSourceVS*)*instance_data;
	VSVideoInfo *vi = &mvcs->vi;

	YV12PICT *out = mvcs->out;
	YV12PICT *saved = mvcs->saved;

	VSFrameRef *vs_frame = vsapi->newVideoFrame(mvcs->vi.format, mvcs->vi.width, mvcs->vi.height, nullptr, core);

	out->y = vsapi->getWritePtr(vs_frame, 0);
	out->u = vsapi->getWritePtr(vs_frame, 1);
	out->v = vsapi->getWritePtr(vs_frame, 2);
	out->yheight = mvcs->vi.height;
	out->ywidth = mvcs->vi.width;
	out->uvheight = mvcs->vi.height >> mvcs->vi.format->subSamplingH;
	out->uvwidth = mvcs->vi.width >> mvcs->vi.format->subSamplingW;
	out->ypitch = vsapi->getStride(vs_frame, 0);
	out->uvpitch = vsapi->getStride(vs_frame, 1);

end:
	if (mvcs->reached_eof || mvcs->timed_out) {
		// Return black frames when too many frames are requested
		for (i = 0; i < out->yheight; i++) {
			memset(out->y + i * out->ypitch, 16, out->ywidth);
		}
		for (i = 0; i < mvcs->out->uvheight; i++) {
			memset(out->u + i * out->uvpitch, 128, out->uvwidth);
		}
		for (i = 0; i < mvcs->out->uvheight; i++) {
			memset(out->v + i * out->uvpitch, 128, out->uvwidth);
		}
		return vs_frame;
	}

	if (mvcs->view == 3) {
		//		OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameReady...\n");
		res = WaitForSingleObject(mvcs->hFrameReady, 10000);
		if (res == WAIT_OBJECT_0) {
			//			OutputDebugString("DGMVCSource: GetFrame(): Got hFrameReady\n");
			if (mvcs->reached_eof)
				goto end;
			//			OutputDebugString("DGMVCSource: GetFrame(): Set hGetFrame\n");
			SetEvent(mvcs->hGetFrame);
			//			OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameDone...\n");
			WaitForSingleObject(mvcs->hFrameDone, INFINITE);
			//			OutputDebugString("DGMVCSource: GetFrame(): Got hFrameDone\n");
		} else if (res == WAIT_TIMEOUT) {
			// char buf[80];
			mvcs->timed_out = true;
			//			sprintf(buf, "DGMVCSource: GetFrame(): timeout 4 at frame %d\n", fcount);
			//			OutputDebugString(buf);
			goto end;
		}
	} else if (mvcs->view == 0) {
		// Interleaved output. We need special handling in case selecteven/selectodd are used in the script.
		if (n == mvcs->expected_frame_no + 1) {
			// The script is asking for the second frame of the pair. We cannot seek so we do it like this.
			// Decode 2 frames. Save the first and return the second.
//			OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameReady...\n");
			res = WaitForSingleObject(mvcs->hFrameReady, 10000);
			if (res == WAIT_OBJECT_0) {
				//				OutputDebugString("DGMVCSource: GetFrame(): Got hFrameReady: \n");
				if (mvcs->reached_eof)
					goto end;
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hGetFrame\n");
				SetEvent(mvcs->hGetFrame);
				//				OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameDone...\n");
				WaitForSingleObject(mvcs->hFrameDone, INFINITE);
				//				OutputDebugString("DGMVCSource: GetFrame(): Got hFrameDone\n");
			} else if (res == WAIT_TIMEOUT) {
				// char buf[80];
				mvcs->timed_out = true;
				//				sprintf(buf, "DGMVCSource: GetFrame(): timeout 1 at frame %d\n", fcount);
				//				OutputDebugString(buf);
				goto end;
			}
			vs_bitblt(saved->y, saved->ypitch, out->y, out->ypitch, out->ywidth, out->yheight);
			vs_bitblt(saved->u, saved->uvpitch, out->u, out->uvpitch, out->uvwidth, out->uvheight);
			vs_bitblt(saved->v, saved->uvpitch, out->v, out->uvpitch, out->uvwidth, out->uvheight);
			//			OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameReady...\n");
			res = WaitForSingleObject(mvcs->hFrameReady, 10000);
			if (res == WAIT_OBJECT_0) {
				//				OutputDebugString("DGMVCSource: GetFrame(): Got hFrameReady\n");
				if (mvcs->reached_eof)
					goto end;
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hGetFrame\n");
				SetEvent(mvcs->hGetFrame);
				//				OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameDone...\n");
				WaitForSingleObject(mvcs->hFrameDone, INFINITE);
				//				OutputDebugString("DGMVCSource: GetFrame(): Got hFrameDone\n");
			} else if (res == WAIT_TIMEOUT) {
				// char buf[80];
				mvcs->timed_out = true;
				//				sprintf(buf, "DGMVCSource: GetFrame(): timeout 2 at frame %d\n", fcount);
				//				OutputDebugString(buf);
				goto end;
			}
		} else if (n == mvcs->expected_frame_no - 1) {
			// The script is asking for the first frame of the pair but we already decoded and saved it.
			// Return the saved frame.
			vs_bitblt(out->y, out->ypitch, saved->y, saved->ypitch, saved->ywidth, saved->yheight);
			vs_bitblt(out->u, out->uvpitch, saved->u, saved->uvpitch, saved->uvwidth, saved->uvheight);
			vs_bitblt(out->v, out->uvpitch, saved->v, saved->uvpitch, saved->uvwidth, saved->uvheight);
		} else {
			// There is no left/right swap in the script, so we can operate sequentially.
			// Decode and return one frame.
//			OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameReady...\n");
			res = WaitForSingleObject(mvcs->hFrameReady, 10000);
			if (res == WAIT_OBJECT_0) {
				//				OutputDebugString("DGMVCSource: GetFrame(): Got hFrameReady\n");
				if (mvcs->reached_eof)
					goto end;
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hGetFrame\n");
				SetEvent(mvcs->hGetFrame);
				//				OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameDone...\n");
				WaitForSingleObject(mvcs->hFrameDone, INFINITE);
				//				OutputDebugString("DGMVCSource: GetFrame(): Got hFrameDone\n");
			} else if (res == WAIT_TIMEOUT) {
				// char buf[80];
				mvcs->timed_out = true;
				//				sprintf(buf, "DGMVCSource: GetFrame(): timeout 3 at frame %d\n", fcount);
				//				OutputDebugString(buf);
				goto end;
			}
		}
	} else {
		// Single view output.
//		OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameReady...\n");
		res = WaitForSingleObject(mvcs->hFrameReady, 10000);
		if (res == WAIT_OBJECT_0) {
			//			OutputDebugString("DGMVCSource: GetFrame(): Got hFrameReady\n");
			if (mvcs->reached_eof)
				goto end;
			if (mvcs->view == 1) {
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hGetFrame\n");
				SetEvent(mvcs->hGetFrame);
			} else {
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hSkipFrame\n");
				SetEvent(mvcs->hSkipFrame);
			}
			//			OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameDone...\n");
			WaitForSingleObject(mvcs->hFrameDone, INFINITE);
			//			OutputDebugString("DGMVCSource: GetFrame(): Got hFrameDone\n");
		} else if (res == WAIT_TIMEOUT) {
			// char buf[80];
			mvcs->timed_out = true;
			//			sprintf(buf, "DGMVCSource: GetFrame(): timeout 4 at frame %d\n", fcount);
			//			OutputDebugString(buf);
			goto end;
		}
		//		OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameReady...\n");
		res = WaitForSingleObject(mvcs->hFrameReady, 10000);
		if (res == WAIT_OBJECT_0) {
			//			OutputDebugString("DGMVCSource: GetFrame(): Got hFrameReady\n");
			if (mvcs->reached_eof)
				goto end;
			if (mvcs->view == 1) {
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hSkipFrame\n");
				SetEvent(mvcs->hSkipFrame);
			} else {
				//				OutputDebugString("DGMVCSource: GetFrame(): Set hGetFrame\n");
				SetEvent(mvcs->hGetFrame);
			}
			//			OutputDebugString("DGMVCSource: GetFrame(): Wait for hFrameDone...\n");
			WaitForSingleObject(mvcs->hFrameDone, INFINITE);
			//			OutputDebugString("DGMVCSource: GetFrame(): Got hFrameDone\n");
		} else if (res == WAIT_TIMEOUT) {
			// char buf[80];
			mvcs->timed_out = true;
			//			sprintf(buf, "DGMVCSource: GetFrame(): timeout 4 at frame %d\n", fcount);
			//			OutputDebugString(buf);
			goto end;
		}
	}

	return vs_frame;
}

static void VS_CC vs_filter_free(void *instance_data, VSCore *core, const VSAPI *vsapi)
{
	extern void destroy_YV12PICT(YV12PICT * pict);

	MVCSourceVS* mvcs = (MVCSourceVS*)instance_data;

	SetEvent(mvcs->hKillDecoder);
	WaitForSingleObject(mvcs->hDecoderDead, INFINITE);
	if (mvcs->out != NULL) {
		free(mvcs->out);
		mvcs->out = NULL;
	}
	destroy_YV12PICT(mvcs->saved);
	if (mvcs->combined_buffer)
		free(mvcs->combined_buffer);

	delete mvcs;
}

static void VS_CC vs_filter_init(VSMap *in, VSMap *out, void **instance_data, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
	MVCSourceVS* mvcs = (MVCSourceVS*)*instance_data;
	vsapi->setVideoInfo(&mvcs->vi, 1, node);
}

static inline void set_option_string(const char **opt,
	const char  *default_value,
	const char  *arg,
	const VSMap *in,
	const VSAPI *vsapi)
{
	int e;
	*opt = vsapi->propGetData(in, arg, 0, &e);
	if (e)
		*opt = default_value;
}

static void VS_CC Create_MVCSource(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
	const char *base_name = vsapi->propGetData(in, "base", 0, NULL);
	const char *dependent_name = vsapi->propGetData(in, "dependent", 0, NULL);
	int view = vsapi->propGetInt(in, "view", 0, NULL);
	int frames = vsapi->propGetInt(in, "frames", 0, NULL);

	const char *mode_name;
	set_option_string(&mode_name, nullptr, "mode", in, vsapi);

	MVCSourceVS* mvcs = new MVCSourceVS{ 0 };
	mvcs->out = (YV12PICT*)malloc(sizeof(YV12PICT));
	if (out == NULL) {
		vsapi->setError(out, "MVCSource: could not malloc YV12 picture structure!");
	}

	HMODULE hm = NULL;
	char dllpath[1024] = { 0 };
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)&Create_MVCSource,
		&hm)) {
		GetModuleFileNameA(hm, dllpath, sizeof(dllpath));
	}
	char *p = &dllpath[strlen(dllpath)];
	while (p[-1] != '\\') p--;
	*p = 0;

#ifdef _WIN64
	strcat_s(p, 15, "libmfxsw64.dll");
#else
	strcat_s(p, 15, "libmfxsw32.dll");
#endif

	HANDLE hMfx = LoadLibraryA(dllpath);
	if (hMfx == NULL) {
		vsapi->setError(out, "DGMVCSource: cannot load libmfxsw32.dll(win64: libmfxsw64.dll)");
		return;
	}

	if (!dependent_name[0]) {
		FILE *t = nullptr;
		unsigned char *p;
		unsigned char *buffer;

		// This may be a simple ACV stream or a combined MVC stream.
		// Determine which one it is. If MVC the view == _view, otherwise it is set to 3.
		fopen_s(&t, base_name, "rb");
		if (t == nullptr)
			vsapi->setError(out, "DGMVCSource: cannot load the specified base file");
		buffer = (unsigned char *)malloc(BUF_SIZE);
		fread(buffer, 1, BUF_SIZE, t);
		// Assume we are not an MVC stream. We may change this below after we check the stream.
		mvcs->view = 3;
		p = &buffer[3];
		while (p < buffer + BUF_SIZE) {
			p++;
			if (p[-4] == 0 && p[-3] == 0 && p[-2] == 1) {
				// The base stream is a combined MVC stream.
				if ((p[-1] & 0x1f) == 15) {
					mvcs->view = view;
					break;
				}
			}
		}
		free(buffer);
		fclose(t);
	} else {
		mvcs->view = view;
	}

	mvcs->reached_eof = false;
	mvcs->timed_out = false;
	mvcs->expected_frame_no = -1;
	if (!strncmp(mode_name, "auto", 4))
		mvcs->hw_mode = 0;
	else if (!strncmp(mode_name, "sw", 2))
		mvcs->hw_mode = 1;
	else if (!strncmp(mode_name, "hw", 2))
		mvcs->hw_mode = 2;
	else
		mvcs->hw_mode = 0;

	mvcs->vi.numFrames = frames;
	mvcs->vi.format = vsapi->getFormatPreset(pfYUV420P8, core);
	if (!mvcs->view) {
		mvcs->vi.numFrames *= 2;
	}
	// vi.SetFieldBased(false);

	extern DWORD WINAPI start_pipeline(LPVOID);
	extern DWORD WINAPI combiner(LPVOID _sp);
	extern YV12PICT* create_YV12PICT(int height, int width, int chroma_format);
	DWORD decoder_threadId, combiner_threadId;

	mvcs->hInitDone = CreateEvent(NULL, FALSE, FALSE, NULL);
	mvcs->hFrameReady = CreateEvent(NULL, FALSE, FALSE, NULL);
	mvcs->hGetFrame = CreateEvent(NULL, FALSE, FALSE, NULL);
	mvcs->hSkipFrame = CreateEvent(NULL, FALSE, FALSE, NULL);
	mvcs->hFrameDone = CreateEvent(NULL, FALSE, FALSE, NULL);
	mvcs->hKillDecoder = CreateEvent(NULL, FALSE, FALSE, NULL);
	mvcs->hDecoderDead = CreateEvent(NULL, FALSE, FALSE, NULL);

	strcpy_s(mvcs->start_params.base_path, base_name);
	strcpy_s(mvcs->start_params.dependent_path, dependent_name);
	mvcs->start_params.pvi = &mvcs->vi;
	mvcs->start_params.view = mvcs->view;
	mvcs->start_params.dec = mvcs->out;
	// mvcs->start_params.AVSenv = env;
	mvcs->start_params.hInitDone = mvcs->hInitDone;
	mvcs->start_params.hFrameReady = mvcs->hFrameReady;
	mvcs->start_params.hGetFrame = mvcs->hGetFrame;
	mvcs->start_params.hSkipFrame = mvcs->hSkipFrame;
	mvcs->start_params.hFrameDone = mvcs->hFrameDone;
	mvcs->start_params.hUnblockCombiner = mvcs->hUnblockCombiner;
	mvcs->start_params.hKillDecoder = mvcs->hKillDecoder;
	mvcs->start_params.hDecoderDead = mvcs->hDecoderDead;
	mvcs->start_params.reached_eof = &mvcs->reached_eof;
	mvcs->start_params.hw_mode = mvcs->hw_mode;

	if (mvcs->start_params.dependent_path[0] != 0) {
		mvcs->combined_buffer = (unsigned char *)malloc(BUF_SIZE);
		mvcs->start_params.combined_buffer = mvcs->combined_buffer;
		mvcs->rp = mvcs->wp = mvcs->combined_buffer;
		mvcs->start_params._rp = &mvcs->rp;
		mvcs->start_params._wp = &mvcs->wp;
		mvcs->hCombinerBlocked = CreateEvent(NULL, FALSE, FALSE, NULL);
		mvcs->start_params.hCombinerBlocked = mvcs->hCombinerBlocked;
		mvcs->hCombinerEOF = CreateEvent(NULL, TRUE, FALSE, NULL);
		mvcs->start_params.hCombinerEOF = mvcs->hCombinerEOF;
		ResetEvent(mvcs->hCombinerEOF);
		CreateThread(NULL, 0, combiner, (LPVOID)&mvcs->start_params, 0, &combiner_threadId);
		//		OutputDebugString("DGMVCSource: DGMVCSource(): Wait for hCombinerBlocked...\n");
		WaitForSingleObject(mvcs->hCombinerBlocked, INFINITE);
		//		OutputDebugString("DGMVCSource: DGMVCSource(): Got hCombinerBlocked\n");
	} else {
		mvcs->combined_buffer = NULL;
		mvcs->hCombinerBlocked = mvcs->hCombinerEOF = 0;
		mvcs->start_params.hCombinerBlocked = mvcs->hCombinerBlocked;
		mvcs->start_params.hCombinerEOF = mvcs->hCombinerEOF;
	}
	CreateThread(NULL, 0, start_pipeline, (LPVOID)&mvcs->start_params, 0, &decoder_threadId);

#if _DEBUG
	//	OutputDebugString("DGMVCSource: DGMVCSource(): Wait for hInitDone...\n");
	if (WaitForSingleObject(mvcs->hInitDone, INFINITE) == WAIT_TIMEOUT)
#else
	//	OutputDebugString("DGMVCSource: DGMVCSource(): Wait for hInitDone...\n");
	if (WaitForSingleObject(mvcs->hInitDone, 2000) == WAIT_TIMEOUT)
#endif
	{
		vsapi->setError(out, "DGMVCSource: cannot open the source file or Intel Media SDK init failed");
	}

	if (mvcs->view == 0)
		mvcs->vi.fpsNum *= 2;

	mvcs->saved = create_YV12PICT(mvcs->vi.height, mvcs->vi.width, 1);

	unsigned int d = gcd(mvcs->vi.fpsNum, mvcs->vi.fpsDen) - 1;
	if (d != 0) {
		mvcs->vi.fpsNum = mvcs->vi.fpsNum >> d;
		mvcs->vi.fpsDen = mvcs->vi.fpsDen >> d;
	}

	vsapi->createFilter(in, out, "DGMVCSource", vs_filter_init,
		vs_filter_get_frame, vs_filter_free, fmUnordered, nfMakeLinear, mvcs, core);
}

//////////////////////////////////////////////////////////////////////////
// Init
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin)
{
	configFunc("systems.innocent.DGMVC",
		"DGMVC",
		"DGMVCSource for VapourSynth",
		VAPOURSYNTH_API_VERSION, 1, plugin);

	registerFunc("DGMVCSource",
		"base:data;dependent:data;view:int;frames:int;mode:data:opt",
		Create_MVCSource, nullptr, plugin);
}
