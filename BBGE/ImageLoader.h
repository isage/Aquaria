/*
Copyright (C) 2007, 2010 - Bit-Blot

This file is part of Aquaria.

Aquaria is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef BBGE_IMAGE_LOADER_H
#define BBGE_IMAGE_LOADER_H

#include <string>
#include "Base.h"

struct RawImage
{
	unsigned int Width;
	unsigned int Height;
	unsigned int Components; // 3 = RGB, 4 = RGBA
	unsigned char *Data;     // free with img_FreeRaw()

	RawImage() : Width(0), Height(0), Components(0), Data(0) {}
};

bool img_LoadRaw(const std::string &filename, RawImage *out);

bool img_LoadRawMem(const void *mem, size_t size, const char *typeHint, RawImage *out);

void img_FreeRaw(RawImage *img);

GLuint img_LoadGLTexture(const std::string &filename,
	bool mipmaps, bool luminanceAlpha,
	GLint wrap, GLint minFilter, GLint magFilter,
	unsigned int *outWidth = 0, unsigned int *outHeight = 0);

GLuint img_LoadGLTextureMem(const void *mem, size_t size, const char *typeHint,
	bool mipmaps, bool luminanceAlpha,
	GLint wrap, GLint minFilter, GLint magFilter,
	unsigned int *outWidth = 0, unsigned int *outHeight = 0);

#endif
