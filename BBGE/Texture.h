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
#ifndef __texture__
#define __texture__

#include "Base.h"
#include "TextureAtlas.h"

enum TextureLoadResult
{
	TEX_FAILED  = 0x00,
	TEX_SUCCESS = 0x01,
	TEX_LOADED  = 0x02,
};

class Texture : public Refcounted
{
public:
	Texture();
	~Texture();

	bool load(std::string file);
	void apply(bool repeatOverride=false);
	void unbind();
	void unload();

	int getPixelWidth();
	int getPixelHeight();
	
	void destroy();
	

	int width, height;

	bool repeat, repeating;

	static SDL_ScaleMode filter;

	SDL_Texture *sdlTexture;

	// Texture atlas support - step 3 of the texture atlas plan
	// (TEXTURE_ATLAS_PLAN.md). When sourceAtlas is non-null, sdlTexture
	// is *shared* with that atlas (and every other Texture backed by the
	// same one) rather than owned by this object - ownsTexture=false in
	// that case means unload() must not SDL_DestroyTexture() it; the
	// atlas's own destructor is the only thing that does that, once
	// every CountedPtr<TextureAtlas> reference (including this one) is
	// gone. width/height still mean *this* texture's own logical size
	// (the sub-region, not the full atlas) - every existing call site
	// reading texture->width/height for its own sprite dimensions keeps
	// working unchanged. atlasX/atlasY are this sub-region's pixel
	// offset within the shared sdlTexture, needed by UV composition
	// (step 4) to know where within the atlas to sample from.
	bool ownsTexture;
	int atlasX, atlasY;
	CountedPtr<TextureAtlas> sourceAtlas;

	// Sets this Texture up as an atlas sub-region instead of loading a
	// separate file - called from Core::doTextureAdd() when the
	// requested name is found in TextureAtlas::lookup(). Always
	// succeeds (no disk I/O, no possible failure mode) given a valid
	// atlas/entry pair, which is why this returns void, unlike load().
	void initFromAtlas(const CountedPtr<TextureAtlas> &atlas, const AtlasEntry &entry);

	// Composes a normalized (0-1) UV coordinate, relative to THIS
	// texture's own logical size (width/height above), into the final
	// UV to actually send to SDL_RenderGeometry - relative to the
	// underlying sdlTexture, which for an atlas-backed texture is
	// shared and larger than this texture's own sub-region. For a
	// non-atlas-backed texture (sourceAtlas null) this is an identity
	// transform - u/v pass through unchanged, exactly matching every
	// existing call site's behavior before this method existed. Step 4
	// of the texture atlas plan - deliberately a single, small, always-
	// called helper rather than scattering "is this atlas-backed"
	// branches across every UV-assigning call site.
	void composeUV(float u, float v, float *outU, float *outV) const;

	// If this Texture is currently atlas-backed, forces a full,
	// standalone reload as a separate, fully-owned texture, discarding
	// the atlas sub-region relationship entirely. Needed specifically
	// for repeat/tile-to-fill usage (Quad::repeatTextureToFill()):
	// SDL's texture wrap/repeat addressing applies to an entire texture,
	// not a sub-rectangle within a larger shared atlas image, so there's
	// no way to make repeating correct for an atlas-backed sub-region -
	// the only correct fix is to stop sharing the atlas for this
	// specific texture and load it as its own, standalone SDL_Texture,
	// matching this same texture name's normal, pre-atlas load path
	// exactly. A no-op if this texture isn't currently atlas-backed.
	// Confirmed necessary via real playtesting - a repeating background
	// Element whose texture happened to be atlas-backed rendered garbled
	// (composeUV()'s formula was never designed to handle UV values
	// outside [0,1], which repeat-to-fill relies on).
	void convertToStandaloneLoad();

	void reload();

	unsigned char *getBufferAndSize(int *w, int *h, unsigned int *size); // returned memory must be free()'d

	std::string name;

protected:
	std::string loadName;

	// internal load functions
	bool loadPNG(const std::string &file);
	bool loadTGA(const std::string &file);
	bool loadZGA(const std::string &file);
	bool loadGeneric(const std::string &file, const char *typeHint);

	// CPU-side shadow copy of the texture's current RGBA pixels, needed
	// because SDL's 2D renderer has no "read pixels back out of an
	// arbitrary texture" call (only SDL_RenderReadPixels against the
	// active render target). Kept in sync by write()/loadGeneric(); read()
	// serves from this instead of a GPU round-trip.
	unsigned char *shadowData;

	int ow, oh;
	
};

#define UNREFTEX(x) if (x) {x = NULL;}

#endif
