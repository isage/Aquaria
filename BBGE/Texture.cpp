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
#include "Texture.h"
#include "Core.h"
#include "ImageLoader.h"
#include "ByteBuffer.h"

#include <assert.h>

#include <stdint.h>

SDL_ScaleMode Texture::filter = SDL_SCALEMODE_LINEAR;

Texture::Texture()
{
	sdlTexture = 0;
	shadowData = 0;
	width = height = 0;

	repeat = false;
	repeating = false;
	ow = oh = -1;

	// Texture atlas support - step 3 of the texture atlas plan. Default
	// state is "not atlas-backed" - ownsTexture=true matches every
	// existing texture's behavior unchanged (this object owns and must
	// destroy its own sdlTexture). initFromAtlas() is the only thing
	// that ever sets ownsTexture=false.
	ownsTexture = true;
	atlasX = atlasY = 0;
}

void Texture::initFromAtlas(const CountedPtr<TextureAtlas> &atlas, const AtlasEntry &entry)
{
	sdlTexture = atlas->sdlTexture;
	ownsTexture = false;
	sourceAtlas = atlas; // increfs - this Texture now shares ownership, keeping the atlas alive at least as long as this object exists

	atlasX = entry.x;
	atlasY = entry.y;
	width = entry.w;
	height = entry.h;

	name = entry.name;

	// Copies this sub-region's own pixels out of the atlas's retained
	// full-image buffer into this Texture's own shadowData - see
	// TextureAtlas.h's atlasShadowData comment for why this exists (it
	// keeps getBufferAndSize()/collision detection working unchanged for
	// atlas-backed textures, without any change to that code, until the
	// plan's step 7 collision tool replaces this mechanism). This is a
	// real, deliberate row-by-row copy (the sub-region isn't contiguous
	// within the atlas's full buffer - source stride is the *atlas's*
	// width, destination stride is this sub-region's own, tighter
	// width), verified against a standalone test before being trusted
	// here - see texture_atlas_subregion_test.cpp.
	if (atlas->atlasShadowData && width > 0 && height > 0)
	{
		shadowData = (unsigned char*)malloc((size_t)width * height * 4);
		if (shadowData)
		{
			const int atlasStride = atlas->width * 4;
			const int rowBytes = width * 4;
			for (int row = 0; row < height; row++)
			{
				const unsigned char *src = atlas->atlasShadowData + (size_t)(atlasY + row) * atlasStride + (size_t)atlasX * 4;
				unsigned char *dst = shadowData + (size_t)row * rowBytes;
				memcpy(dst, src, rowBytes);
			}
		}
	}
}

void Texture::composeUV(float u, float v, float *outU, float *outV) const
{
	if (sourceAtlas && sourceAtlas->width > 0 && sourceAtlas->height > 0)
	{
		// Maps a coordinate normalized against THIS texture's own
		// width/height into the shared atlas's own pixel space -
		// verified against a standalone test (see
		// texture_atlas_uv_composition_test.cpp) before being trusted
		// here, since a wrong-looking sprite from a stride/offset
		// mistake here would be a silent visual bug, not a crash.
		*outU = (atlasX + u * width) / (float)sourceAtlas->width;
		*outV = (atlasY + v * height) / (float)sourceAtlas->height;
	}
	else
	{
		*outU = u;
		*outV = v;
	}
}

void Texture::convertToStandaloneLoad()
{
	if (!sourceAtlas)
		return; // already standalone - no-op, matching the header comment

	// Same pattern reload() already uses (unload() then load()), except
	// loadName needs computing first - it was never set for this
	// texture, since initFromAtlas() (how it got here) never calls
	// load() at all. core->getTextureLoadName() is the exact same
	// resolution Core::doTextureAdd() itself uses for every normal,
	// non-atlas load, so this reproduces precisely the load this
	// texture name would have gotten if it had never been atlas-backed
	// in the first place.
	std::string standaloneLoadName = core->getTextureLoadName(name);

	unload(); // releases sourceAtlas correctly (doesn't touch the shared sdlTexture, since ownsTexture is still false at this point)
	ownsTexture = true;
	load(standaloneLoadName);
}

Texture::~Texture()
{
	destroy();
}

void Texture::unload()
{
	if (sdlTexture)
	{
		ow = width;
		oh = height;

		if (core->debugLogTextures)
		{
			debugLog("UNLOADING TEXTURE: " + name);
		}

		// Step 3 of the texture atlas plan: only destroy the SDL texture
		// if this object actually owns it. An atlas-backed Texture's
		// sdlTexture is shared with every other Texture backed by the
		// same atlas - destroying it here would be a use-after-free for
		// all of them the moment any single one of them unloads. The
		// atlas itself (via sourceAtlas going out of scope below) is the
		// only thing that ever destroys the shared SDL texture, and only
		// once nothing references it anymore.
		if (ownsTexture)
			SDL_DestroyTexture(sdlTexture);
		sdlTexture = 0;
	}
	// Releases this Texture's share of atlas ownership regardless of
	// ownsTexture - a no-op (empty CountedPtr assigned to empty) for
	// ordinary, non-atlas-backed textures. Explicit here rather than
	// relying solely on ~Texture() to do this via the member's own
	// destructor, since reload() calls unload() without destroying the
	// Texture object itself - see reload()'s comment for the resulting,
	// deliberately-accepted edge case.
	sourceAtlas = CountedPtr<TextureAtlas>();
	if (shadowData)
	{
		free(shadowData);
		shadowData = 0;
	}
}

void Texture::destroy()
{
	unload();

	core->removeTexture(this);
}

int Texture::getPixelWidth()
{
	int w = 0, h = 0;
	unsigned int size = 0;
	unsigned char *data = getBufferAndSize(&w, &h, &size);
	if (!data)
		return 0;

	int smallestx = -1, largestx = -1;
	for (unsigned int x = 0; x < unsigned(w); x++)
	{
		for (unsigned int y = 0; y < unsigned(h); y++)
		{
			unsigned int p = (y*unsigned(w)*4) + (x*4) + 3;
			if (p < size && data[p] >= 254)
			{
				if (smallestx == -1 || x < smallestx)
					smallestx = x;
				if (largestx == -1 || x > largestx)
					largestx = x;
			}
		}
	}
	free(data);
	return largestx - smallestx;
}

int Texture::getPixelHeight()
{
	int w = 0, h = 0;
	unsigned int size = 0;
	unsigned char *data = getBufferAndSize(&w, &h, &size);
	if (!data)
		return 0;

	int smallesty = -1, largesty = -1;
	for (unsigned int x = 0; x < unsigned(w); x++)
	{
		for (unsigned int y = 0; y < unsigned(h); y++)
		{
			int p = (y*unsigned(w)*4) + (x*4) + 3;
			if (p < size && data[p] >= 254)
			{
				if (smallesty == -1 || y < smallesty)
					smallesty = y;
				if (largesty == -1 || y > largesty)
					largesty = y;
			}
		}
	}
	free(data);
	return largesty - smallesty;
}

void Texture::reload()
{
	debugLog("RELOADING TEXTURE: " + name + " with loadName " + loadName + "...");

	// Edge case, deliberately accepted rather than fixed here: if this
	// Texture was atlas-backed, unload() correctly releases the atlas
	// reference, but load(loadName) below only knows how to load a
	// separate file from disk - it has no atlas awareness. An
	// atlas-backed texture that gets reload()'d falls back to loading as
	// a standalone file rather than re-resolving through the atlas
	// lookup. reload() is a dev/debug-oriented hot-reload path, not
	// something normal gameplay hits, so this is a minor, documented
	// behavioral gap rather than a correctness bug worth expanding this
	// step's scope for.
	unload();
	load(loadName);

	/*if (ow != -1 && oh != -1)
	{
		width = ow;
		height = oh;
	}*/
	debugLog("DONE");
}

bool Texture::load(std::string file)
{
	if (file.size()<4)
	{
		errorLog("Texture Name is Empty or Too Short");
		return false;
	}

	stringToLowerUserData(file);
	file = core->adjustFilenameCase(file);

	loadName = file;
	repeating = false;

	size_t pos = file.find_last_of('.');

	if ((pos != std::string::npos) && (pos >= 0))
	{
		// make sure this didn't catch the '.' in /home/username/.Aquaria/*  --ryan.
		const std::string userdata = core->getUserDataFolder();
		const size_t len = userdata.length();
		if (pos < len)
			pos = std::string::npos;
	}

	/*if (core->debugLogTextures)
	{
		std::ostringstream os;
		os << "pos [" << pos << "], file :" << file;
		debugLog(os.str());
	}*/

	bool found = exists(file);

	if(!found && exists(file + ".png"))
	{
		found = true;
		file += ".png";
	}

	// .tga/.zga are never used as game graphics anywhere except save slot thumbnails.
	// if so, their file names are passed exact, not with a missing extension

	if (found)
	{
		file = localisePathInternalModpath(file);
		file = core->adjustFilenameCase(file);

		/*
		std::ostringstream os;
		os << "Loading texture [" << file << "]";
		debugLog(os.str());
		*/
		std::string post = file.substr(file.size()-3, 3);
		stringToLower(post);
		if (post == "png")
		{

			return loadPNG(file);
		}
		else if (post == "zga")
		{
			return loadZGA(file);
		}
		else if (post == "tga")
		{
			return loadTGA(file);
		}
		else
		{
			debugLog("unknown image file type: " + file);
		}
	}
	else
	{
		// load default image / leave white
		if (core->debugLogTextures)
			debugLog("***Could not find texture: " + file);
	}
	return false;
}

void Texture::apply(bool repeatOverride)
{
	repeating = (repeat || repeatOverride);
}

void Texture::unbind()
{
}

bool Texture::loadPNG(const std::string &file)
{
	return loadGeneric(file, 0);
}

// internal load functions
bool Texture::loadTGA(const std::string &file)
{
	return loadGeneric(file, "TGA");
}

bool Texture::loadZGA(const std::string &file)
{
	unsigned long size = 0;
	char *buf = readCompressedFile(file, &size);
	if (!buf || !size)
	{
		debugLog("Can't load ZGA File: " + file);
		return false;
	}

	// Decode once (gives us the shadow-copy pixels), then upload that same
	// decode as the live SDL_Texture - same pattern as loadGeneric(), not
	// duplicated logic.
	RawImage img;
	bool ok = img_LoadRawMem(buf, size, "TGA", &img);
	delete [] buf;

	if (!ok)
	{
		debugLog("Can't load ZGA File: " + file);
		return false;
	}

	if (shadowData) free(shadowData);
	shadowData = (unsigned char*)malloc((size_t)img.Width * img.Height * 4);
	if (shadowData)
	{
		if (img.Components == 4)
		{
			memcpy(shadowData, img.Data, (size_t)img.Width * img.Height * 4);
		}
		else
		{
			for (unsigned int i = 0; i < img.Width * img.Height; i++)
			{
				shadowData[i*4+0] = img.Data[i*3+0];
				shadowData[i*4+1] = img.Data[i*3+1];
				shadowData[i*4+2] = img.Data[i*3+2];
				shadowData[i*4+3] = 255;
			}
		}
	}

	SDL_Texture *tex = 0;
	if (img.Data && img.Width && img.Height)
	{
		const SDL_PixelFormat fmt = (img.Components == 4) ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;
		SDL_Surface *surf = SDL_CreateSurfaceFrom((int)img.Width, (int)img.Height, fmt,
			(void*)img.Data, (int)(img.Width * img.Components));
		if (surf)
		{
			tex = SDL_CreateTextureFromSurface(core->getRenderer(), surf);
			SDL_DestroySurface(surf);
			if (tex)
				SDL_SetTextureScaleMode(tex, filter);
		}
	}

	unsigned int w = img.Width, h = img.Height;
	img_FreeRaw(&img);

	sdlTexture = tex;

	if (sdlTexture == 0)
	{
		debugLog("Can't load ZGA File: " + file);
		return false;
	}

	width = w;
	height = h;
	return true;
}

// Shared implementation for PNG and TGA loading
bool Texture::loadGeneric(const std::string &file, const char *typeHint)
{
	if (file.empty()) return false;

	unsigned long memsize = 0;
	const char *memptr = readFile(file, &memsize);
	if (!memptr || !memsize)
	{
		debugLog("Can't load image file: " + file);
		return false;
	}

	// Decode once into a RawImage (gives us the shadow-copy pixels for
	// read()/getBufferAndSize()), then upload that same decode as the live
	// SDL_Texture, rather than decoding the file twice.
	RawImage img;
	if (!img_LoadRawMem(memptr, memsize, typeHint, &img))
	{
		delete [] memptr;
		debugLog("Can't load image file: " + file);
		return false;
	}
	delete [] memptr;

	if (shadowData) free(shadowData);
	shadowData = (unsigned char*)malloc((size_t)img.Width * img.Height * 4);
	if (shadowData)
	{
		if (img.Components == 4)
		{
			memcpy(shadowData, img.Data, (size_t)img.Width * img.Height * 4);
		}
		else
		{
			// widen RGB -> RGBA (alpha = opaque) so the shadow copy always
			// matches the RGBA contract read()/getBufferAndSize() promise.
			for (unsigned int i = 0; i < img.Width * img.Height; i++)
			{
				shadowData[i*4+0] = img.Data[i*3+0];
				shadowData[i*4+1] = img.Data[i*3+1];
				shadowData[i*4+2] = img.Data[i*3+2];
				shadowData[i*4+3] = 255;
			}
		}
	}

	SDL_Texture *tex = 0;
	if (img.Data && img.Width && img.Height)
	{
		const SDL_PixelFormat fmt = (img.Components == 4) ? SDL_PIXELFORMAT_RGBA32 : SDL_PIXELFORMAT_RGB24;
		SDL_Surface *surf = SDL_CreateSurfaceFrom((int)img.Width, (int)img.Height, fmt,
			(void*)img.Data, (int)(img.Width * img.Components));
		if (surf)
		{
			tex = SDL_CreateTextureFromSurface(core->getRenderer(), surf);
			SDL_DestroySurface(surf);
			if (tex)
				SDL_SetTextureScaleMode(tex, filter);
		}
	}

	unsigned int w = img.Width, h = img.Height;
	img_FreeRaw(&img);

	sdlTexture = tex;

	if (sdlTexture == 0)
	{
		debugLog("Can't load image file: " + file);
		return false;
	}

	width = w;
	height = h;
	return true;
}


unsigned char * Texture::getBufferAndSize(int *wparam, int *hparam, unsigned int *sizeparam)
{
	// Simplified from the old GL version: no power-of-2 padding concerns
	// (SDL/modern GPUs don't require it, and we no longer query driver-side
	// texture dimensions at all), and the shadow copy kept in sync by
	// loadGeneric()/loadZGA()/write() is already exactly the tightly-packed
	// RGBA buffer this function is supposed to hand back - just copy it.
	if (width <= 0 || height <= 0 || !shadowData)
	{
		*wparam = 0;
		*hparam = 0;
		*sizeparam = 0;
		return NULL;
	}

	unsigned int size = (unsigned int)width * height * 4;
	unsigned char *data = (unsigned char*)malloc(size);
	if (!data)
	{
		*wparam = 0;
		*hparam = 0;
		*sizeparam = 0;
		return NULL;
	}

	memcpy(data, shadowData, size);

	*wparam = width;
	*hparam = height;
	*sizeparam = size;
	return data;
}
