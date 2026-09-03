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
// Usage: call PerfLog::beginFrame() once at the start of a frame,
// PerfLog::endFrame() at the very end - both already wired into
// Core::main()'s own per-frame call, immediately around the
// render()/showBuffer() pair (not inside either of those functions
// themselves - correcting an earlier, inaccurate version of this
// comment), and bump the relevant counter from wherever
// the corresponding real work happens (see PerfLog::* below). Flushing to
// disk is automatic and throttled internally - callers never need to
// think about it.

#include <cstdint>

namespace PerfLog
{
	// Call once per frame, immediately around Core::main()'s own
	// render()/showBuffer() pair (see the detailed reasoning at that
	// call site for why beginFrame() specifically starts there rather
	// than inside Core::render() itself - render() can be called
	// nested). Anything in between counts as "this frame"'s CPU
	// submission time.
	void beginFrame();
	void endFrame();

	// Separate timing for the per-frame game-update phase (particle
	// manager, sound, ActionMapper/StateManager via Core::onUpdate(),
	// etc.) - wraps a *different* span than beginFrame()/endFrame(),
	// letting a frame's cost be split into "update" vs "render" instead
	// of only knowing the combined total. Call beginUpdate() at the
	// start of the update phase and endUpdate() right after
	// Core::onUpdate() returns, both already wired into Core::main().
	void beginUpdate();
	void endUpdate();

	// Counters - bump these from the actual call sites that do the real
	// work they represent. All are per-frame; reset automatically by
	// beginFrame().
	void countDrawCall();               // an SDL_RenderGeometry() call
	void countStateChangeSkipped();     // a blend/scale-mode change that was correctly skipped (Step 1)
	void countStateChangeApplied();     // a blend/scale-mode change that actually went to SDL
	void countRenderTargetSwitch();     // an SDL_SetRenderTarget() call (Step 3's main concern)

	// Step 3-specific: how often the smart capture-gating decided to
	// skip vs engage core->frameBuffer's per-frame capture, when that
	// gating is active (Core::main()'s normal per-frame render() call
	// only - see the detailed reasoning at that call site). Lets a test
	// run directly confirm the gating is actually skipping captures in
	// scenes that don't need them, and isn't skipping suspiciously often
	// somewhere it shouldn't (e.g. a water-heavy area).
	void countCaptureSkipped();
	void countCaptureEngaged();

	// Step 6-specific: how many individual draws were submitted to
	// DrawBatch vs how many actual SDL_RenderGeometry() calls that
	// turned into after merging consecutive same-(texture,blend) draws.
	// The ratio between these two directly shows batching effectiveness
	// for a given test run - e.g. 100 submits collapsing into 10 flushes
	// is a real 10x reduction in SDL calls for that scene.
	void countBatchSubmit();
	void countBatchFlush();

	// Enable/disable at runtime (defaults to on) - useful for isolating
	// a specific test run without a rebuild.
	void setEnabled(bool enabled);
}

#endif
