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

#include "ImageLoader.h"

#include <cstring>
#include <cstdlib>

#include <SDL3_image/SDL_image.h>

// -------------------------------------------------------------------------
// internal helpers
// -------------------------------------------------------------------------

// Convert a decoded surface into a tightly-packed RawImage (no row padding),
// top-down, with either 3 (RGB) or 4 (RGBA) components depending on whether
// the source had an alpha channel. Consumes (frees) `surf`.
static bool surfaceToRawImage(SDL_Surface *surf, RawImage *out)
{
	if (!surf)
		return false;

	const bool hasAlpha = SDL_ISPIXELFORMAT_ALPHA(surf->format);
	const SDL_PixelFormat targetFmt = hasAlpha ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;
	const unsigned int components = hasAlpha ? 4 : 3;

	SDL_Surface *conv = SDL_ConvertSurface(surf, targetFmt);
	SDL_DestroySurface(surf);
	if (!conv)
		return false;

	const unsigned int w = (unsigned int)conv->w;
	const unsigned int h = (unsigned int)conv->h;
	const size_t rowBytes = (size_t)w * components;

	unsigned char *data = (unsigned char*)malloc(rowBytes * h);
	if (!data)
	{
		SDL_DestroySurface(conv);
		return false;
	}

	if (SDL_MUSTLOCK(conv))
		SDL_LockSurface(conv);

	const unsigned char *src = (const unsigned char*)conv->pixels;
	for (unsigned int y = 0; y < h; y++)
	{
		memcpy(data + y * rowBytes, src + (size_t)y * conv->pitch, rowBytes);
	}

	if (SDL_MUSTLOCK(conv))
		SDL_UnlockSurface(conv);

	SDL_DestroySurface(conv);

	out->Width = w;
	out->Height = h;
	out->Components = components;
	out->Data = data;
	return true;
}

// -------------------------------------------------------------------------
// raw loading
// -------------------------------------------------------------------------

bool img_LoadRaw(const std::string &filename, RawImage *out)
{
	if (!out)
		return false;
	*out = RawImage();

	SDL_Surface *surf = IMG_Load(filename.c_str());
	if (!surf)
		return false;
	return surfaceToRawImage(surf, out);
}

bool img_LoadRawMem(const void *mem, size_t size, const char *typeHint, RawImage *out)
{
	if (!out)
		return false;
	*out = RawImage();

	if (!mem || !size)
		return false;

	SDL_IOStream *io = SDL_IOFromConstMem(mem, size);
	if (!io)
		return false;

	SDL_Surface *surf = typeHint
		? IMG_LoadTyped_IO(io, true, typeHint)
		: IMG_Load_IO(io, true);

	if (!surf)
		return false;
	return surfaceToRawImage(surf, out);
}

void img_FreeRaw(RawImage *img)
{
	if (img && img->Data)
	{
		free(img->Data);
		img->Data = 0;
	}
}

// -------------------------------------------------------------------------
// SDL_Texture loading
// -------------------------------------------------------------------------

static SDL_Texture *uploadRawAsSDLTexture(SDL_Renderer *renderer, const RawImage &img, SDL_ScaleMode scaleMode)
{
	if (!renderer || !img.Data || !img.Width || !img.Height)
		return 0;

	const SDL_PixelFormat fmt = (img.Components == 4) ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;

	// SDL_CreateTextureFromSurface wants an SDL_Surface, and img.Data is
	// already exactly the tightly-packed pixel layout that format expects
	// (see surfaceToRawImage), so wrap it without copying rather than
	// re-decoding.
	SDL_Surface *surf = SDL_CreateSurfaceFrom((int)img.Width, (int)img.Height, fmt,
		(void*)img.Data, (int)(img.Width * img.Components));
	if (!surf)
		return 0;

	SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_DestroySurface(surf); // does not free img.Data, which we don't own via this surface

	if (tex)
		SDL_SetTextureScaleMode(tex, scaleMode);

	return tex;
}

SDL_Texture *img_LoadSDLTexture(SDL_Renderer *renderer, const std::string &filename,
	SDL_ScaleMode scaleMode, unsigned int *outWidth, unsigned int *outHeight)
{
	RawImage img;
	if (!img_LoadRaw(filename, &img))
		return 0;

	SDL_Texture *tex = uploadRawAsSDLTexture(renderer, img, scaleMode);

	if (outWidth) *outWidth = img.Width;
	if (outHeight) *outHeight = img.Height;

	img_FreeRaw(&img);
	return tex;
}

SDL_Texture *img_LoadSDLTextureMem(SDL_Renderer *renderer, const void *mem, size_t size, const char *typeHint,
	SDL_ScaleMode scaleMode, unsigned int *outWidth, unsigned int *outHeight)
{
	RawImage img;
	if (!img_LoadRawMem(mem, size, typeHint, &img))
		return 0;

	SDL_Texture *tex = uploadRawAsSDLTexture(renderer, img, scaleMode);

	if (outWidth) *outWidth = img.Width;
	if (outHeight) *outHeight = img.Height;

	img_FreeRaw(&img);
	return tex;
}
