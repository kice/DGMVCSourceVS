/*
 *  DGMVCDecode -- MVC decoder/frame server
 *  Copyright (C) 2014 Donald A. Graft
 */

#include "global.h"

YV12PICT* create_YV12PICT(int height, int width, int chroma_format)
{
	YV12PICT* pict;
	pict = (YV12PICT*)malloc(sizeof(YV12PICT));
	int uvwidth, uvheight;
	if (chroma_format == 1) // 4:2:0
	{
		uvwidth = width >> 1;
		uvheight = height >> 1;
	} else if (chroma_format == 2) // 4:2:2
	{
		uvwidth = width >> 1;
		uvheight = height;
	} else // 4:4:4
	{
		uvwidth = width;
		uvheight = height;
	}
	int uvpitch = (((uvwidth + 15) >> 4) << 4);
	int ypitch = uvpitch * 2;
	pict->y = (unsigned char*)_aligned_malloc(height*ypitch, 32);
	pict->u = (unsigned char*)_aligned_malloc(uvheight*uvpitch, 16);
	pict->v = (unsigned char*)_aligned_malloc(uvheight*uvpitch, 16);

	pict->ypitch = ypitch;
	pict->uvpitch = uvpitch;
	pict->ywidth = width;
	pict->uvwidth = uvwidth;
	pict->yheight = height;
	pict->uvheight = uvheight;
	return pict;
}

void destroy_YV12PICT(YV12PICT * pict)
{
	_aligned_free(pict->y);
	_aligned_free(pict->u);
	_aligned_free(pict->v);
	free(pict);
}
