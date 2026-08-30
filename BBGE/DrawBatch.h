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
#ifndef BBGE_DRAW_BATCH_H
#define BBGE_DRAW_BATCH_H

// Step 6 of the performance optimization plan: adjacent-run draw-call
// batching. Deliberately NOT a full scene sort by texture - overlapping
// alpha-blended sprites are order-dependent (painter's algorithm), and
// reordering across an overlap can visibly change output. This only
// merges *consecutive* draws (in the existing, untouched draw order)
// that already share the same texture and blend mode into one
// SDL_RenderGeometry() call - draw order is never changed, only how many
// separate SDL calls it takes to submit it.
//
// SAFETY MODEL - read this before adding a new draw call site anywhere
// in the codebase: DrawBatch::submit() may *defer* actual submission.
// Anything that draws by any other means (a different SDL_RenderGeometry
// call not routed through here, SDL_RenderTexture, SDL_RenderClear, a
// render-target switch, or the end of a frame/layer) MUST call
// DrawBatch::flush() first, or a deferred, not-yet-submitted batch could
// end up drawn in the wrong order relative to it (after it, when it
// should have been before) or onto the wrong render target entirely.
// Every call site in this codebase that draws by any means was updated
// to flush first - see the migration notes for the specific list. If you
// add a new one, it needs the same treatment.
//
// Single-threaded, matching how this engine actually renders - plain
// function-local `static` state (not thread_local, unavailable/
// unreliable on the target platform per this project's own history).

#include <SDL3/SDL.h>
#include <vector>

namespace DrawBatch
{
	// Submits a draw, possibly deferring it if it matches the currently
	// accumulating batch's (texture, blend mode). indices are relative
	// to this call's own verts array (0-based, as normal) - offsetting
	// into the accumulated buffer is handled internally.
	void submit(SDL_Renderer *renderer, SDL_Texture *tex, SDL_BlendMode blend,
		const SDL_Vertex *verts, int numVerts, const int *indices, int numIndices);

	// Submits whatever's currently accumulated (if anything) as one
	// SDL_RenderGeometry() call, then clears the accumulator. Safe to
	// call when nothing is accumulated (no-op). See the safety model
	// above for when this must be called.
	void flush();

	// Safe wrapper for SDL_SetRenderTarget() - flushes first, always.
	// Every render-target switch in this codebase (confirmed via a
	// codebase-wide sweep: 4 sites in FrameBuffer.cpp, 4 in Core.cpp's
	// screenshot code) goes through this instead of the raw SDL call, so
	// a deferred batch can never end up submitted to the wrong target
	// after the target has already changed underneath it.
	void setRenderTarget(SDL_Renderer *renderer, SDL_Texture *target);
}

#endif
