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
#include "DrawBatch.h"
#include "RenderState.h"
#include "PerfLog.h"

namespace DrawBatch
{

// Single-threaded, function-local `static` state throughout - see the
// header's safety model comment.
static SDL_Renderer *s_renderer = nullptr;
static SDL_Texture *s_tex = nullptr;
static SDL_BlendMode s_blend = SDL_BLENDMODE_NONE;
static bool s_hasBatch = false;

static std::vector<SDL_Vertex> s_verts;
static std::vector<int> s_indices;

void submit(SDL_Renderer *renderer, SDL_Texture *tex, SDL_BlendMode blend,
	const SDL_Vertex *verts, int numVerts, const int *indices, int numIndices)
{
	bool matches = s_hasBatch && s_renderer == renderer && s_tex == tex && s_blend == blend;

	if (!matches)
	{
		flush();
		s_renderer = renderer;
		s_tex = tex;
		s_blend = blend;
		s_hasBatch = true;
	}

	// Index offset: this call's indices are 0-based relative to its own
	// verts array, but they need to point into the shared, accumulated
	// vertex buffer - offset by however many vertices are already in it.
	int base = (int)s_verts.size();
	s_verts.insert(s_verts.end(), verts, verts + numVerts);
	size_t oldSize = s_indices.size();
	s_indices.resize(oldSize + numIndices);
	for (int i = 0; i < numIndices; i++)
		s_indices[oldSize + i] = indices[i] + base;
}

void flush()
{
	if (!s_hasBatch)
		return;

	if (!s_verts.empty() && s_renderer)
	{
		// Re-applied here, immediately before the actual draw, rather
		// than trusted from whenever submit() was originally called -
		// SDL_SetTextureBlendMode() modifies the texture object's own
		// persistent state, not a separate per-draw flag, so anything
		// else touching this same texture's blend mode between submit()
		// and this flush() could otherwise corrupt an already-deferred
        // batch. RenderState::setTextureBlendMode() is cheap to call
        // even when unchanged (queries and skips redundant SDL calls,
        // Step 1 of this same plan).
		if (s_tex)
			RenderState::setTextureBlendMode(s_tex, s_blend);
		else
			RenderState::setRenderDrawBlendMode(s_renderer, s_blend);

		SDL_RenderGeometry(s_renderer, s_tex, s_verts.data(), (int)s_verts.size(),
			s_indices.data(), (int)s_indices.size());
		PerfLog::countDrawCall();
		PerfLog::countBatchFlush();
	}

	s_verts.clear();
	s_indices.clear();
	s_hasBatch = false;
	s_tex = nullptr;
	s_renderer = nullptr;
}

void setRenderTarget(SDL_Renderer *renderer, SDL_Texture *target)
{
	flush();
	SDL_SetRenderTarget(renderer, target);
	PerfLog::countRenderTargetSwitch();
}

} // namespace DrawBatch
