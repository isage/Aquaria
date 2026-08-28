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
#include "PerfLog.h"
#include "Core.h"

#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>
#include <limits>

namespace PerfLog
{

// Single-threaded, function-local `static` state throughout this file -
// deliberately not thread_local (unavailable/unreliable on the target
// platform) and not a concern given rendering is single-threaded in this
// engine.

static bool s_enabled = true;
static bool s_initialized = false;
static std::ofstream s_out;

// Per-frame counters, reset by beginFrame().
static uint32_t s_drawCalls = 0;
static uint32_t s_stateChangesSkipped = 0;
static uint32_t s_stateChangesApplied = 0;
static uint32_t s_renderTargetSwitches = 0;
static uint64_t s_frameStartTicks = 0;

// Aggregated across a window of frames, flushed to disk periodically
// rather than every single frame (writing every frame would make the
// profiling tool itself a meaningful part of what it's measuring).
static const int WINDOW_SIZE = 120;
static int s_framesInWindow = 0;
static double s_frameTimeMinMs = 0.0;
static double s_frameTimeMaxMs = 0.0;
static double s_frameTimeSumMs = 0.0;
static uint64_t s_drawCallSum = 0;
static uint64_t s_stateSkippedSum = 0;
static uint64_t s_stateAppliedSum = 0;
static uint64_t s_targetSwitchSum = 0;

static void lazyInit()
{
	if (s_initialized) return;
	s_initialized = true;

	std::string path = "perf.log";
	if (core)
		path = core->getDebugLogPath() + "perf.log";

	s_out.open(path.c_str());
	if (s_out.is_open())
	{
		s_out << "# Aquaria performance log - one line per " << WINDOW_SIZE
			<< "-frame window.\n";
		s_out << "# frameTimeMs(min/avg/max) drawCalls(avg) "
			"stateChangesApplied(avg) stateChangesSkipped(avg) "
			"renderTargetSwitches(avg)\n";
		s_out.flush();
	}
}

static void flushWindow()
{
	if (!s_out.is_open() || s_framesInWindow == 0)
		return;

	double avgFrameMs = s_frameTimeSumMs / s_framesInWindow;
	double avgDrawCalls = double(s_drawCallSum) / s_framesInWindow;
	double avgApplied = double(s_stateAppliedSum) / s_framesInWindow;
	double avgSkipped = double(s_stateSkippedSum) / s_framesInWindow;
	double avgSwitches = double(s_targetSwitchSum) / s_framesInWindow;

	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(3);
	os << s_frameTimeMinMs << "/" << avgFrameMs << "/" << s_frameTimeMaxMs << "ms  ";
	os.precision(1);
	os << "draws=" << avgDrawCalls << "  "
		<< "stateApplied=" << avgApplied << "  "
		<< "stateSkipped=" << avgSkipped << "  "
		<< "targetSwitches=" << avgSwitches;

	s_out << os.str() << std::endl; // flush every window - this is already throttled to WINDOW_SIZE frames, an explicit flush here is cheap and means a crash doesn't lose the last window's data

	s_framesInWindow = 0;
	s_frameTimeMinMs = 0.0;
	s_frameTimeMaxMs = 0.0;
	s_frameTimeSumMs = 0.0;
	s_drawCallSum = 0;
	s_stateSkippedSum = 0;
	s_stateAppliedSum = 0;
	s_targetSwitchSum = 0;
}

void setEnabled(bool enabled)
{
	s_enabled = enabled;
}

void beginFrame()
{
	if (!s_enabled) return;
	lazyInit();

	s_drawCalls = 0;
	s_stateChangesSkipped = 0;
	s_stateChangesApplied = 0;
	s_renderTargetSwitches = 0;
	s_frameStartTicks = SDL_GetPerformanceCounter();
}

void endFrame()
{
	if (!s_enabled) return;
	if (s_frameStartTicks == 0) return; // beginFrame() wasn't called (e.g. disabled mid-frame)

	uint64_t endTicks = SDL_GetPerformanceCounter();
	uint64_t freq = SDL_GetPerformanceFrequency();
	double frameMs = freq ? (double(endTicks - s_frameStartTicks) * 1000.0 / double(freq)) : 0.0;

	if (s_framesInWindow == 0)
	{
		s_frameTimeMinMs = frameMs;
		s_frameTimeMaxMs = frameMs;
	}
	else
	{
		if (frameMs < s_frameTimeMinMs) s_frameTimeMinMs = frameMs;
		if (frameMs > s_frameTimeMaxMs) s_frameTimeMaxMs = frameMs;
	}
	s_frameTimeSumMs += frameMs;
	s_drawCallSum += s_drawCalls;
	s_stateSkippedSum += s_stateChangesSkipped;
	s_stateAppliedSum += s_stateChangesApplied;
	s_targetSwitchSum += s_renderTargetSwitches;
	s_framesInWindow++;

	if (s_framesInWindow >= WINDOW_SIZE)
		flushWindow();
}

void countDrawCall() { if (s_enabled) s_drawCalls++; }
void countStateChangeSkipped() { if (s_enabled) s_stateChangesSkipped++; }
void countStateChangeApplied() { if (s_enabled) s_stateChangesApplied++; }
void countRenderTargetSwitch() { if (s_enabled) s_renderTargetSwitches++; }

} // namespace PerfLog
