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

// Decode + upload as an SDL_Texture (replaces the old GL-texture loading
// path now that Quad/RenderObject draw via SDL_RenderGeometry instead of
// raw GL). scaleMode maps 1:1 from the old GL_NEAREST/GL_LINEAR filter
// choice. Unlike the old GL path, there is no mipmap parameter - SDL's 2D
// renderer has no mipmap chain concept, filtering is purely per-draw via
// scaleMode - and no per-texture wrap/repeat mode in this SDL3 version;
// repeat-filled quads need tiled geometry at the call site instead (see
// Quad::repeatTextureToFill()).
SDL_Texture *img_LoadSDLTexture(SDL_Renderer *renderer, const std::string &filename,
	SDL_ScaleMode scaleMode,
	unsigned int *outWidth = 0, unsigned int *outHeight = 0);

SDL_Texture *img_LoadSDLTextureMem(SDL_Renderer *renderer, const void *mem, size_t size, const char *typeHint,
	SDL_ScaleMode scaleMode,
	unsigned int *outWidth = 0, unsigned int *outHeight = 0);

#endif
