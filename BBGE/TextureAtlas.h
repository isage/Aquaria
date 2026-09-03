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
#ifndef BBGE_TEXTURE_ATLAS_H
#define BBGE_TEXTURE_ATLAS_H

// Texture atlas support - step 2 of the texture atlas plan
// (TEXTURE_ATLAS_PLAN.md). Loads the .png + .txt pairs produced by
// tools/make_tileset.py (gfx/tilesets/<name>.png and matching .txt,
// "<texture_name> <x> <y> <w> <h>" one entry per line) and exposes a
// global, name-based lookup so Core::doTextureAdd() can check "is this
// texture name inside some already-loaded atlas" before falling back to
// loading it as a separate file. Deliberately not map-scoped - the
// lookup table is a flat, global registry so sprite/UI atlases can
// register into the same table later without a separate lookup path.
//
// Lifetime: TextureAtlas inherits Refcounted, exactly like Texture
// itself - this is a deliberate reuse of the engine's existing,
// battle-tested shared-resource pattern rather than a new one. The
// global registry (s_loadedAtlases) holds one CountedPtr per loaded
// atlas - its own owning reference. Every atlas-backed Texture object
// elsewhere in the engine also holds a CountedPtr<TextureAtlas>, not a
// raw pointer, so the atlas can never be destroyed while anything still
// draws from it, and destruction ordering is handled automatically by
// the refcounting itself rather than manual bookkeeping. See the
// destructor's comment for how the lookup table stays consistent.

#include "Base.h"
#include "Refcounted.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <map>

struct AtlasEntry
{
	std::string name;
	int x, y, w, h;
};

class TextureAtlas : public Refcounted
{
public:
	// Loads <atlasName>.png + <atlasName>.txt from gfx/tilesets/, or
	// returns the existing loaded instance if this name is already
	// registered (checked before touching disk at all). Returns a null
	// CountedPtr (bool-false) on failure - missing file, unparseable
	// metadata, or SDL/image decode failure - callers should treat this
	// the same as any other missing-texture case, not a fatal error.
	static CountedPtr<TextureAtlas> load(const std::string &atlasName);

	// Explicit unload, intended to be called from map-unload logic once
	// hooked up (step 5 of the plan) - drops the registry's own
	// reference. If nothing else still holds a CountedPtr<TextureAtlas>
	// at that point, this destroys the atlas immediately; if something
	// (e.g. a Texture object that outlived its map for some reason)
	// still references it, it stays alive until that reference is also
	// gone. Safe to call on a name that isn't currently loaded (no-op).
	static void unload(const std::string &atlasName);

	// Looks up a texture name across every currently-loaded atlas.
	// Returns true and fills outAtlas/outEntry if found. outAtlas is a
	// CountedPtr (increfs on return), so the caller holding it is
	// sufficient to keep the atlas alive regardless of what else happens
	// to the registry afterward - outEntry's lifetime is tied to
	// outAtlas, not to the registry.
	static bool lookup(const std::string &textureName, CountedPtr<TextureAtlas> *outAtlas, AtlasEntry *outEntry);

	SDL_Texture *sdlTexture;
	int width, height; // full atlas image dimensions, in pixels

	// CPU-side copy of the full atlas's decoded RGBA pixels, retained
	// for the atlas's whole lifetime (unlike Texture::loadGeneric(),
	// which only needs its decode transiently). Needed so
	// Texture::initFromAtlas() can copy out each sub-region's own pixel
	// data into that Texture's own shadowData - preserving
	// getBufferAndSize()/collision detection exactly as before for
	// atlas-backed textures, without any change to the collision code
	// itself. This is a deliberate, temporary memory trade-off: the
	// plan's step 7 (a dedicated 2-bit-per-pixel collision map tool)
	// replaces this with something far smaller, but until that lands,
	// keeping full RGBA here is what keeps collision correct rather than
	// silently broken for any Element whose texture happens to be
	// atlas-backed.
	unsigned char *atlasShadowData;

	const std::string &getName() const { return name; }
	const std::vector<AtlasEntry> &getEntries() const { return entries; }

protected:
	~TextureAtlas(); // Refcounted's destructor is protected - only decref()/delete this should ever destroy one

private:
	TextureAtlas(); // only ever constructed via load()

	std::string name;
	std::vector<AtlasEntry> entries;

	bool loadFromDisk(const std::string &atlasName);
	bool parseMetadata(const std::string &metaPath);

	// The owning registry: one CountedPtr per currently-loaded atlas,
	// keyed by name. This is what keeps an atlas alive between load()
	// and unload() even if no Texture has requested a lookup yet.
	static std::map<std::string, CountedPtr<TextureAtlas> > s_loadedAtlases;

	// Pure lookup index: texture name -> (which atlas, which entry index
	// within it). Raw pointers here are intentional and safe - this
	// table's entries are removed in the destructor (see
	// TextureAtlas.cpp), so a pointer can never remain in this map after
	// the object it points to is actually gone, regardless of whether
	// unload() was the trigger or some other reference simply went out
	// of scope. Storing the entry index alongside the atlas pointer
	// avoids re-searching entries on every lookup.
	struct AtlasLookupRef { TextureAtlas *atlas; size_t entryIndex; };
	static std::map<std::string, AtlasLookupRef> s_textureNameToAtlas;
};

#endif
