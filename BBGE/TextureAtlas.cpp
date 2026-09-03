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
#include "TextureAtlas.h"
#include "Core.h"
#include "ImageLoader.h"
#include <sstream>
#include <fstream>

std::map<std::string, CountedPtr<TextureAtlas> > TextureAtlas::s_loadedAtlases;
std::map<std::string, TextureAtlas::AtlasLookupRef> TextureAtlas::s_textureNameToAtlas;

TextureAtlas::TextureAtlas()
	: sdlTexture(0), width(0), height(0), atlasShadowData(0)
{
}

TextureAtlas::~TextureAtlas()
{
	// Removes this atlas's own entries from the global lookup table
	// before the SDL texture goes away - deliberately done here, in the
	// destructor, rather than only in unload(), so the lookup table can
	// never point to a dangling TextureAtlas regardless of *why* this
	// object is being destroyed (explicit unload(), or simply the last
	// CountedPtr reference anywhere going out of scope on its own).
	for (std::map<std::string, AtlasLookupRef>::iterator it = s_textureNameToAtlas.begin();
		it != s_textureNameToAtlas.end(); )
	{
		if (it->second.atlas == this)
			it = s_textureNameToAtlas.erase(it);
		else
			++it;
	}

	if (sdlTexture)
	{
		SDL_DestroyTexture(sdlTexture);
		sdlTexture = 0;
	}
	if (atlasShadowData)
	{
		free(atlasShadowData);
		atlasShadowData = 0;
	}
}

bool TextureAtlas::parseMetadata(const std::string &metaPath)
{
	unsigned long size = 0;
	char *buf = readFile(metaPath, &size);
	if (!buf || !size)
	{
		debugLog("TextureAtlas: couldn't read metadata file: " + metaPath);
		return false;
	}

	// readFile()'s buffer isn't guaranteed null-terminated - copy into a
	// std::string (which is) before using istringstream on it, matching
	// how other text-format loaders in this codebase safely handle
	// readFile()'s output.
	std::string content(buf, size);
	delete [] buf;

	std::istringstream in(content);
	std::string line;
	while (std::getline(in, line))
	{
		if (line.empty())
			continue;

		std::istringstream ls(line);
		AtlasEntry entry;
		if (!(ls >> entry.name >> entry.x >> entry.y >> entry.w >> entry.h))
		{
			// Tolerate blank/malformed trailing lines rather than
			// aborting the whole atlas over one bad line - matches this
			// codebase's general "log and continue" pattern for
			// non-critical parse issues elsewhere (e.g. tileset
			// element-template parsing).
			continue;
		}
		// Normalized to match Core::doTextureAdd()'s own lookup key
		// convention (internalTextureName is always lowercased there via
		// stringToLowerUserData()) - without this, a texture name whose
		// case differs between the original tileset .txt and this
		// generated metadata file would silently miss the atlas lookup
		// and fall through to a separate, redundant disk load instead.
		stringToLower(entry.name);
		entries.push_back(entry);
	}

	return !entries.empty();
}

bool TextureAtlas::loadFromDisk(const std::string &atlasName)
{
	name = atlasName;

	const std::string base = "gfx/tilesets/" + atlasName;
	// adjustFilenameCase() does a case-insensitive on-disk lookup on
	// Linux (no-op elsewhere) - needed because the atlas name reaching
	// this function is typically already lowercased (e.g.
	// Game::loadElementTemplates() calls stringToLower(pack) before
	// this), but tools/make_tileset.py doesn't lowercase its own output
	// filename - it uses the tileset name's original casing straight
	// from the map XML's tileset="..." attribute. Without this, a
	// mixed-case tileset name would silently fail to find its own atlas
	// file on a case-sensitive filesystem. Matches exactly how
	// Texture::load() already handles this same class of problem.
	const std::string pngPath = core->adjustFilenameCase(base + ".png");
	const std::string metaPath = core->adjustFilenameCase(base + ".txt");

	if (!parseMetadata(metaPath))
		return false;

	unsigned long memsize = 0;
	const char *memptr = readFile(pngPath, &memsize);
	if (!memptr || !memsize)
	{
		debugLog("TextureAtlas: couldn't read atlas image: " + pngPath);
		return false;
	}

	RawImage img;
	bool ok = img_LoadRawMem(memptr, memsize, 0, &img);
	delete [] memptr;

	if (!ok)
	{
		debugLog("TextureAtlas: couldn't decode atlas image: " + pngPath);
		return false;
	}

	// Retained for the atlas's lifetime, widened to RGBA if needed -
	// see the header comment on atlasShadowData for why this differs
	// from Texture::loadGeneric()'s transient-only decode.
	if (img.Data && img.Width && img.Height)
	{
		atlasShadowData = (unsigned char*)malloc((size_t)img.Width * img.Height * 4);
		if (atlasShadowData)
		{
			if (img.Components == 4)
			{
				memcpy(atlasShadowData, img.Data, (size_t)img.Width * img.Height * 4);
			}
			else
			{
				for (unsigned int i = 0; i < img.Width * img.Height; i++)
				{
					atlasShadowData[i*4+0] = img.Data[i*3+0];
					atlasShadowData[i*4+1] = img.Data[i*3+1];
					atlasShadowData[i*4+2] = img.Data[i*3+2];
					atlasShadowData[i*4+3] = 255;
				}
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
				SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
			else
				debugLog(std::string("TextureAtlas: SDL_CreateTextureFromSurface failed: ") + SDL_GetError()
					+ " (renderer=" + (core->getRenderer() ? "valid" : "NULL") + ")");
		}
		else
		{
			debugLog(std::string("TextureAtlas: SDL_CreateSurfaceFrom failed: ") + SDL_GetError());
		}
	}

	unsigned int w = img.Width, h = img.Height;
	img_FreeRaw(&img);

	if (!tex)
	{
		debugLog("TextureAtlas: couldn't create SDL texture for: " + pngPath);
		if (atlasShadowData) { free(atlasShadowData); atlasShadowData = 0; }
		return false;
	}

	sdlTexture = tex;
	width = (int)w;
	height = (int)h;
	return true;
}

CountedPtr<TextureAtlas> TextureAtlas::load(const std::string &atlasName)
{
	std::map<std::string, CountedPtr<TextureAtlas> >::iterator existing = s_loadedAtlases.find(atlasName);
	if (existing != s_loadedAtlases.end())
		return existing->second;

	TextureAtlas *atlas = new TextureAtlas();
	CountedPtr<TextureAtlas> ptr(atlas); // increfs immediately - if loadFromDisk() fails below, letting this go out of scope on return correctly destroys it via the normal refcount path, not a manual delete.

	if (!atlas->loadFromDisk(atlasName))
	{
		debugLog("TextureAtlas: failed to load atlas: " + atlasName);
		return CountedPtr<TextureAtlas>();
	}

	s_loadedAtlases[atlasName] = ptr;

	for (size_t i = 0; i < atlas->entries.size(); i++)
	{
		AtlasLookupRef ref;
		ref.atlas = atlas;
		ref.entryIndex = i;
		// Deliberately overwrites on duplicate texture names across
		// atlases (last-loaded wins) rather than asserting/rejecting -
		// matches Core::doTextureAdd()'s own by-name-cache semantics,
		// where a later addTexture() for the same name doesn't clash
		// with an earlier one either.
		s_textureNameToAtlas[atlas->entries[i].name] = ref;
	}

	return ptr;
}

void TextureAtlas::unload(const std::string &atlasName)
{
	// Erasing the map entry drops the registry's own CountedPtr
	// reference. If nothing else references this atlas, its destructor
	// runs right here (via CountedPtr's own decref-to-zero), which in
	// turn cleans up s_textureNameToAtlas - no separate cleanup needed
	// in this function itself.
	s_loadedAtlases.erase(atlasName);
}

bool TextureAtlas::lookup(const std::string &textureName, CountedPtr<TextureAtlas> *outAtlas, AtlasEntry *outEntry)
{
	std::map<std::string, AtlasLookupRef>::iterator it = s_textureNameToAtlas.find(textureName);
	if (it == s_textureNameToAtlas.end())
		return false;

	TextureAtlas *atlas = it->second.atlas;
	size_t idx = it->second.entryIndex;

	if (idx >= atlas->entries.size())
		return false; // shouldn't happen, but don't trust a stale index over a real bounds check

	if (outAtlas)
		*outAtlas = CountedPtr<TextureAtlas>(atlas); // increfs - caller now shares ownership
	if (outEntry)
		*outEntry = atlas->entries[idx];

	return true;
}
