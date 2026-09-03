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
#ifndef BBGE_QUAD_DRAW_HELPER_H
#define BBGE_QUAD_DRAW_HELPER_H

#include "Core.h"
#include "DrawBatch.h"
#include "RenderState.h"
#include "PerfLog.h"

// Replaces the extremely common old-code idiom, seen dozens of times
// across the bespoke gameplay-visual renderers (MiniMapRender,
// CurrentRender, Emitter, DarkLayer, AutoMap, PathRender, SteamRender,
// Beam, Hair, Strand, Web, GridRender, SchoolFish):
//
//   glTranslatef(cx, cy, 0);
//   glBegin(GL_QUADS);
//     glTexCoord2f(0,1); glVertex2f(-hw, hh);
//     glTexCoord2f(1,1); glVertex2f( hw, hh);
//     glTexCoord2f(1,0); glVertex2f( hw,-hh);
//     glTexCoord2f(0,0); glVertex2f(-hw,-hh);
//   glEnd();
//   glTranslatef(-cx, -cy, 0);
//
// i.e. draw an axis-aligned UV-mapped quad centered at a given local-space
// point, then undo the translate. core->transform.transformPoint()
// already gives the same placement without needing the undo step, so this
// collapses the whole idiom into one call.
//
// (cx, cy) and (hw, hh) are in the *local* space of whatever
// core->transform's current top-of-stack represents (i.e. the same space
// the old glVertex2f() calls were implicitly in).
inline void drawTexturedQuad(SDL_Renderer *renderer, SDL_Texture *tex,
	float cx, float cy, float hw, float hh,
	float r, float g, float b, float a,
	SDL_BlendMode blend,
	float u0=0.0f, float v1=1.0f, float u1=1.0f, float v0=0.0f)
{
	if (!renderer) return;

	glm::vec3 p0 = core->transform.transformPoint(cx-hw, cy+hh);
	glm::vec3 p1 = core->transform.transformPoint(cx+hw, cy+hh);
	glm::vec3 p2 = core->transform.transformPoint(cx+hw, cy-hh);
	glm::vec3 p3 = core->transform.transformPoint(cx-hw, cy-hh);

	SDL_FColor col = {r, g, b, a};
	SDL_Vertex v[4];
	v[0].position={p0.x,p0.y}; v[0].tex_coord={u0,v1}; v[0].color=col;
	v[1].position={p1.x,p1.y}; v[1].tex_coord={u1,v1}; v[1].color=col;
	v[2].position={p2.x,p2.y}; v[2].tex_coord={u1,v0}; v[2].color=col;
	v[3].position={p3.x,p3.y}; v[3].tex_coord={u0,v0}; v[3].color=col;
	static const int idx[6] = {0,1,2,0,2,3};

	if (tex)
		RenderState::setTextureBlendMode(tex, blend);
	else
		RenderState::setRenderDrawBlendMode(renderer, blend);
	// Routed through DrawBatch instead of a forced flush + direct
	// SDL_RenderGeometry() call - this helper is shared by ~13 different
	// bespoke renderers (see the comment above), several of which draw
	// many consecutive, same-texture quads in a tight loop (MiniMapRender
	// in particular: one call per visible tile cell, often sharing the
	// same water/land texture across many consecutive cells) - every one
	// of those was getting a forced, unbatched flush per call until now,
	// confirmed as a real, measured bottleneck via a real playtest's
	// layerDraws= breakdown showing far more UI-category draws during
	// the map screen than the actual map-tile-overlay logic accounts
	// for. DrawBatch::submit() already handles the non-atlas-backed
	// identity case safely (verified in Step 6), so this is a pure
	// batching win with no behavior change for callers passing a
	// standalone, non-atlas texture.
	DrawBatch::submit(renderer, tex, blend, v, 4, idx, 6);
	PerfLog::countBatchSubmit();
}

// Convenience overload for the common "quad centered at (cx,cy), texture's
// own texture pointer, full [0,1] UV range" case.
inline void drawTexturedQuad(SDL_Renderer *renderer, const Texture *texture,
	float cx, float cy, float hw, float hh,
	float r, float g, float b, float a, SDL_BlendMode blend)
{
	// Composes through the texture's own composeUV() (identity transform
	// for a non-atlas-backed texture, correct sub-region mapping
	// otherwise) rather than passing the raw [0,1] default straight
	// through - closing the same class of bug proactively here that
	// previously had to be found and fixed three separate times
	// (Emitter.cpp, Quad::renderGrid(), Quad's strip-rendering mode)
	// after already shipping and being reported as a real, visible
	// regression each time. None of this helper's current callers
	// happen to use an atlas-backed texture today, but this makes that
	// safe by construction rather than by every future caller having to
	// remember to check.
	float u0=0.0f, v1=1.0f, u1=1.0f, v0=0.0f;
	if (texture)
	{
		texture->composeUV(0.0f, 1.0f, &u0, &v1);
		texture->composeUV(1.0f, 0.0f, &u1, &v0);
	}
	drawTexturedQuad(renderer, texture ? texture->sdlTexture : 0, cx, cy, hw, hh, r, g, b, a, blend, u0, v1, u1, v0);
}

// Overload for CountedPtr<Texture>-typed members (Refcounted.h), which has
// no implicit conversion to Texture* (only .content()/operator bool()).
inline void drawTexturedQuad(SDL_Renderer *renderer, const CountedPtr<Texture> &texture,
	float cx, float cy, float hw, float hh,
	float r, float g, float b, float a, SDL_BlendMode blend)
{
	drawTexturedQuad(renderer, texture.content(), cx, cy, hw, hh, r, g, b, a, blend);
}

#endif
