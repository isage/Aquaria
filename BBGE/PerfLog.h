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
#ifndef BBGE_PERF_LOG_H
#define BBGE_PERF_LOG_H

// Lightweight, text-file-based performance tracking - Step 0 of the
// performance optimization plan. Deliberately not built on any
// GPU-capture/render-debug tooling (confirmed unavailable in the target
// environment) - just plain counters and SDL_GetPerformanceCounter()-based
// timing, periodically flushed as human-readable text to its own file
// (separate from debug.log, so perf data doesn't get lost in normal
// engine logging and can be grepped/diffed independently across runs).
//
// Single-threaded by design, matching how this engine actually renders -
// uses plain function-local `static` state, not thread_local (confirmed
// unavailable/unreliable on the target platform). Every counter here is
// reset once per frame from Core::render()/showBuffer() and is not safe
// to touch from any other thread.
//
// Usage: call PerfLog::beginFrame() once at the start of a frame (already
// wired into Core::render()), PerfLog::endFrame() at the very end (wired
// into Core::showBuffer()), and bump the relevant counter from wherever
// the corresponding real work happens (see PerfLog::* below). Flushing to
// disk is automatic and throttled internally - callers never need to
// think about it.

#include <cstdint>

namespace PerfLog
{
	// Call once per frame, at the very start of Core::render() and the
	// very end of Core::showBuffer() respectively. Anything in between
	// counts as "this frame"'s CPU submission time.
	void beginFrame();
	void endFrame();

	// Counters - bump these from the actual call sites that do the real
	// work they represent. All are per-frame; reset automatically by
	// beginFrame().
	void countDrawCall();               // an SDL_RenderGeometry() call
	void countStateChangeSkipped();     // a blend/scale-mode change that was correctly skipped (Step 1)
	void countStateChangeApplied();     // a blend/scale-mode change that actually went to SDL
	void countRenderTargetSwitch();     // an SDL_SetRenderTarget() call (Step 3's main concern)

	// Enable/disable at runtime (defaults to on) - useful for isolating
	// a specific test run without a rebuild.
	void setEnabled(bool enabled);
}

#endif
