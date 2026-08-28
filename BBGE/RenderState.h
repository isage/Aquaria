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
#ifndef BBGE_RENDER_STATE_H
#define BBGE_RENDER_STATE_H

// Step 1 of the performance optimization plan: every draw call site in
// this engine calls SDL_SetTextureBlendMode()/SDL_SetRenderDrawBlendMode()
// unconditionally, even when the value hasn't changed since the last
// draw. These wrappers query the *actual current* SDL-side state first
// (SDL_GetTextureBlendMode()/SDL_GetRenderDrawBlendMode(), both cheap
// local reads, not GPU/driver round-trips) and only call the setter if
// the value genuinely needs to change.
//
// Deliberately does NOT maintain its own separate cache (e.g. a
// SDL_Texture* -> SDL_BlendMode map) - querying SDL's own ground-truth
// state directly avoids any risk of that cache going stale (a texture
// being destroyed and a new one reallocated at the same pointer, some
// other code path calling the raw SDL setter directly and silently
// invalidating a side cache, etc.). The extra query call is cheap
// relative to the setter call this is trying to avoid.
//
// Use these in place of the raw SDL calls at every draw call site.

#include <SDL3/SDL.h>
#include "PerfLog.h"

namespace RenderState
{
	inline void setTextureBlendMode(SDL_Texture *tex, SDL_BlendMode mode)
	{
		if (!tex) return;
		SDL_BlendMode current;
		if (SDL_GetTextureBlendMode(tex, &current) && current == mode)
		{
			PerfLog::countStateChangeSkipped();
			return;
		}
		SDL_SetTextureBlendMode(tex, mode);
		PerfLog::countStateChangeApplied();
	}

	inline void setRenderDrawBlendMode(SDL_Renderer *renderer, SDL_BlendMode mode)
	{
		if (!renderer) return;
		SDL_BlendMode current;
		if (SDL_GetRenderDrawBlendMode(renderer, &current) && current == mode)
		{
			PerfLog::countStateChangeSkipped();
			return;
		}
		SDL_SetRenderDrawBlendMode(renderer, mode);
		PerfLog::countStateChangeApplied();
	}
}

#endif
