/*
*  DGMVCDecode -- MVC decoder/frame server
*  Copyright (C) 2014 Donald A. Graft
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "global.h"

struct SFLAGS
{
	int base_au_size, dependent_au_size;
};

#define BASE_VIEW 0
#define DEPENDENT_VIEW 1

// Write a byte to the output combined stream buffer, observing wrap-arounds.
// The last byte in the buffer is not used in order to allow a distinction between
// a full buffer and an empty buffer. Block if there is no room to write the byte.
void write_byte(unsigned char value, HANDLE _hUnblockCombiner, HANDLE _hCombinerBlocked, unsigned char *combined_buffer, unsigned char **_rp, unsigned char **_wp)
{
	unsigned char *p;

	p = *_rp;
	p--;
	if (p < combined_buffer)
		p += BUF_SIZE;
	if (*_wp == p) {
		// Let the decoder get started, as data now.
//		OutputDebugString("DGMVCSource: combiner(): Set hCombinerBlocked\n");
		SetEvent(_hCombinerBlocked);
		// No room left in the combined buffer. Block until the reader releases us to continue.
//		OutputDebugString("DGMVCSource: combiner(): Wait for hUnblockCombiner...\n");
		WaitForSingleObject(_hUnblockCombiner, INFINITE);
		//		OutputDebugString("DGMVCSource: combiner(): Got hUnblockCombiner\n");
	}
	*(*_wp) = value;
	(*_wp)++;
	if (*_wp >= combined_buffer + BUF_SIZE)
		(*_wp) -= BUF_SIZE;
}

// Get the next access unit for a base/dependent view.
int get_next_au(int view, unsigned char *buf, FILE *fp, struct SFLAGS *s)
{
	unsigned char *p;
	int hit_eof = 0;
	size_t read;
	unsigned char *base_au = nullptr, *dependent_au = nullptr;

	read = fread(buf, 1, BUF_SIZE, fp);
	p = &buf[3];
	while (1) {
		*p++;
		if (p[-4] == 0 && p[-3] == 0 && p[-2] == 1 && (p[-1] & 0x1f) == (view == BASE_VIEW ? 9 : 24))
			break;
	}
	if (view == BASE_VIEW)
		base_au = &p[-4];
	else
		dependent_au = &p[-4];
	while (1) {
		*p++;
		if (p >= buf + read) {
			hit_eof = 1;
			break;
		}
		if (p[-4] == 0 && p[-3] == 0 && p[-2] == 1 && (p[-1] & 0x1f) == (view == BASE_VIEW ? 9 : 24))
			break;
	}
	if (view == BASE_VIEW)
		s->base_au_size = (int)(p - base_au - (hit_eof ? 0 : 4));
	else
		s->dependent_au_size = (int)(p - dependent_au - (hit_eof ? 0 : 4));
	_fseeki64(fp, -(buf + read - p + (hit_eof ? 0 : 4)), SEEK_CUR);
	return (hit_eof);
}

// Stream combiner thread. This thread feeds the decoder with the merged stream.
DWORD WINAPI combiner(LPVOID _sp)
{
	unsigned char *base_buffer;
	unsigned char *dependent_buffer;
	int status_base = 0, status_dependent = 0;
	START_PARAMS *sp = (START_PARAMS *)_sp;
	FILE *base_fp, *dependent_fp;
	struct SFLAGS sflags;

	base_buffer = (unsigned char *)malloc(BUF_SIZE);
	dependent_buffer = (unsigned char *)malloc(BUF_SIZE);

	// Open the base and dependent view elementary streams.
	fopen_s(&base_fp, sp->base_path, "rb");
	if (base_fp == NULL) {
		return 1;
	}
	fopen_s(&dependent_fp, sp->dependent_path, "rb");
	if (dependent_fp == NULL) {
		fclose(base_fp);
		return 1;
	}
	// Init the combiner.
	sp->hUnblockCombiner = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Extra leading 0 for the first 9 NALU.
	write_byte(0, sp->hUnblockCombiner, sp->hCombinerBlocked, sp->combined_buffer, sp->_rp, sp->_wp);

	// Loop through the access units in the source streams, combine them, and
	// write them to the output combined stream buffer.
	while (1) {
		int i;

		memset(&sflags, 0, sizeof(sflags));
		status_base = get_next_au(BASE_VIEW, base_buffer, base_fp, &sflags);
		status_dependent = get_next_au(DEPENDENT_VIEW, dependent_buffer, dependent_fp, &sflags);
		for (i = 0; i < sflags.base_au_size; i++)
			write_byte(base_buffer[i], sp->hUnblockCombiner, sp->hCombinerBlocked, sp->combined_buffer, sp->_rp, sp->_wp);
		for (i = 0; i < sflags.dependent_au_size; i++)
			write_byte(dependent_buffer[i], sp->hUnblockCombiner, sp->hCombinerBlocked, sp->combined_buffer, sp->_rp, sp->_wp);
		if (status_base || status_dependent) {
			//			OutputDebugString("DGMVCSource: combiner(): Set hCombinerEOF\n");
			SetEvent(sp->hCombinerEOF);
			break;
		}
	}
	if (base_buffer)
		free(base_buffer);
	if (dependent_buffer)
		free(dependent_buffer);
	fclose(base_fp);
	fclose(dependent_fp);
	return 0;
}
