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
// GL texture loading
// -------------------------------------------------------------------------

static GLuint uploadRawAsGLTexture(const RawImage &img, bool mipmaps, bool luminanceAlpha,
	GLint wrap, GLint minFilter, GLint magFilter)
{
	if (!img.Data || !img.Width || !img.Height)
		return 0;

	unsigned char *uploadData = img.Data;
	unsigned char *converted = 0;
	GLenum srcFormat = (img.Components == 4) ? GL_RGBA : GL_RGB;

	if (luminanceAlpha)
	{
		converted = (unsigned char*)malloc((size_t)img.Width * img.Height * 2);
		if (converted)
		{
			for (unsigned int i = 0; i < img.Width * img.Height; i++)
			{
				const unsigned char *px = img.Data + (size_t)i * img.Components;
				unsigned char lum = (unsigned char)(((int)px[0] + px[1] + px[2]) / 3);
				unsigned char a = (img.Components == 4) ? px[3] : 255;
				converted[i * 2 + 0] = lum;
				converted[i * 2 + 1] = a;
			}
			uploadData = converted;
			srcFormat = GL_LUMINANCE_ALPHA;
		}
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	if (mipmaps)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);

	glTexImage2D(GL_TEXTURE_2D, 0, srcFormat, img.Width, img.Height, 0,
		srcFormat, GL_UNSIGNED_BYTE, uploadData);

	if (converted)
		free(converted);

	return tex;
}

GLuint img_LoadGLTexture(const std::string &filename,
	bool mipmaps, bool luminanceAlpha,
	GLint wrap, GLint minFilter, GLint magFilter,
	unsigned int *outWidth, unsigned int *outHeight)
{
	RawImage img;
	if (!img_LoadRaw(filename, &img))
		return 0;

	GLuint tex = uploadRawAsGLTexture(img, mipmaps, luminanceAlpha, wrap, minFilter, magFilter);

	if (outWidth) *outWidth = img.Width;
	if (outHeight) *outHeight = img.Height;

	img_FreeRaw(&img);
	return tex;
}

GLuint img_LoadGLTextureMem(const void *mem, size_t size, const char *typeHint,
	bool mipmaps, bool luminanceAlpha,
	GLint wrap, GLint minFilter, GLint magFilter,
	unsigned int *outWidth, unsigned int *outHeight)
{
	RawImage img;
	if (!img_LoadRawMem(mem, size, typeHint, &img))
		return 0;

	GLuint tex = uploadRawAsGLTexture(img, mipmaps, luminanceAlpha, wrap, minFilter, magFilter);

	if (outWidth) *outWidth = img.Width;
	if (outHeight) *outHeight = img.Height;

	img_FreeRaw(&img);
	return tex;
}
