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
#include <set>
#include <map>

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
static uint32_t s_captureSkipped = 0;
static uint32_t s_captureEngaged = 0;
static uint32_t s_batchSubmits = 0;
static uint32_t s_batchFlushes = 0;

// New this round: per-frame object counts, recorded once via
// recordObjectCounts() (not incremented like the counters above) -
// mirrors Core's own total/processed/rendered counts exactly.
static unsigned int s_objectsTotal = 0;
static unsigned int s_objectsProcessed = 0;
static unsigned int s_objectsRendered = 0;

// New this round: SDL_RenderPresent()'s own timing, same pattern as
// s_updateStartTicks/s_lastUpdateMs above.
static uint64_t s_presentStartTicks = 0;
static double s_lastPresentMs = 0.0;

// New this round: per-layer-category draw-call attribution.
// setLayerCategory() sets which category subsequent countDrawCall()
// calls should be attributed to; accumulated per window, cleared on
// flush. A std::map<std::string, uint64_t> rather than a fixed set of
// counters, since the actual category names are freely chosen at the
// Core::render() call sites, not fixed at compile time here.
static std::string s_currentLayerCategory;
static std::map<std::string, uint64_t> s_layerDrawsInWindow;

// Distinct game-state names (StateManager::getTopStateData()->name)
// observed during the current 120-frame window - requested directly
// from real playtest feedback, since guessing which window corresponds
// to which game state from draw-count patterns alone was ambiguous and
// error-prone (e.g. the map screen's own draw count can vary widely
// depending on how much has been explored, and a mid-window state
// transition - intro to menu, cutscene to gameplay - previously had no
// way to show up in the log at all). A std::set so multiple states
// within one window (a transition happening mid-window) are captured
// without duplicates, joined and printed on flush, then cleared for the
// next window.
static std::set<std::string> s_statesInWindow;

static uint64_t s_frameStartTicks = 0;
static uint64_t s_updateStartTicks = 0;
static double s_lastUpdateMs = 0.0;

// Aggregated across a window of frames, flushed to disk periodically
// rather than every single frame (writing every frame would make the
// profiling tool itself a meaningful part of what it's measuring).
static const int WINDOW_SIZE = 120;
static int s_framesInWindow = 0;
static double s_frameTimeMinMs = 0.0;
static double s_frameTimeMaxMs = 0.0;
static double s_frameTimeSumMs = 0.0;
static double s_updateTimeMinMs = 0.0;
static double s_updateTimeMaxMs = 0.0;
static double s_updateTimeSumMs = 0.0;
static uint64_t s_drawCallSum = 0;
static uint64_t s_stateSkippedSum = 0;
static uint64_t s_stateAppliedSum = 0;
static uint64_t s_targetSwitchSum = 0;
static uint64_t s_captureSkippedSum = 0;
static uint64_t s_captureEngagedSum = 0;
static uint64_t s_batchSubmitSum = 0;
static uint64_t s_batchFlushSum = 0;

// New this round: object-count aggregation - min/avg/max, same pattern
// as frame/update time above, since counts can vary meaningfully within
// one window (e.g. across a scene transition).
static double s_objectsTotalMin = 0.0, s_objectsTotalMax = 0.0, s_objectsTotalSum = 0.0;
static double s_objectsProcessedMin = 0.0, s_objectsProcessedMax = 0.0, s_objectsProcessedSum = 0.0;
static double s_objectsRenderedMin = 0.0, s_objectsRenderedMax = 0.0, s_objectsRenderedSum = 0.0;

// New this round: present-time aggregation, same pattern as
// s_updateTimeMinMs/etc.
static double s_presentTimeMinMs = 0.0, s_presentTimeMaxMs = 0.0, s_presentTimeSumMs = 0.0;

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
		s_out << "# frameTimeMs(min/avg/max) updateTimeMs(min/avg/max) "
			"drawCalls(avg) stateChangesApplied(avg) stateChangesSkipped(avg) "
			"renderTargetSwitches(avg) captureEngaged(avg)/captureSkipped(avg) "
			"batchSubmits(avg)/batchFlushes(avg) "
			"objectsTotal/Processed/Rendered(min/avg/max) "
			"presentTimeMs(min/avg/max) "
			"layerDraws(per-category avg draws, ':'-separated, only categories with nonzero draws shown) "
			"gameState(distinct names seen this window, '+'-joined if more than one)\n";
		s_out.flush();
	}
}

static void flushWindow()
{
	if (!s_out.is_open() || s_framesInWindow == 0)
		return;

	double avgFrameMs = s_frameTimeSumMs / s_framesInWindow;
	double avgUpdateMs = s_updateTimeSumMs / s_framesInWindow;
	double avgDrawCalls = double(s_drawCallSum) / s_framesInWindow;
	double avgApplied = double(s_stateAppliedSum) / s_framesInWindow;
	double avgSkipped = double(s_stateSkippedSum) / s_framesInWindow;
	double avgSwitches = double(s_targetSwitchSum) / s_framesInWindow;
	double avgCaptureEngaged = double(s_captureEngagedSum) / s_framesInWindow;
	double avgCaptureSkipped = double(s_captureSkippedSum) / s_framesInWindow;
	double avgBatchSubmits = double(s_batchSubmitSum) / s_framesInWindow;
	double avgBatchFlushes = double(s_batchFlushSum) / s_framesInWindow;
	double avgObjectsTotal = s_objectsTotalSum / s_framesInWindow;
	double avgObjectsProcessed = s_objectsProcessedSum / s_framesInWindow;
	double avgObjectsRendered = s_objectsRenderedSum / s_framesInWindow;
	double avgPresentMs = s_presentTimeSumMs / s_framesInWindow;

	std::ostringstream os;
	os.setf(std::ios::fixed);
	os.precision(3);
	os << s_frameTimeMinMs << "/" << avgFrameMs << "/" << s_frameTimeMaxMs << "ms  ";
	os << s_updateTimeMinMs << "/" << avgUpdateMs << "/" << s_updateTimeMaxMs << "ms  ";
	os.precision(1);
	os << "draws=" << avgDrawCalls << "  "
		<< "stateApplied=" << avgApplied << "  "
		<< "stateSkipped=" << avgSkipped << "  "
		<< "targetSwitches=" << avgSwitches << "  "
		<< "captureEngaged=" << avgCaptureEngaged << "  "
		<< "captureSkipped=" << avgCaptureSkipped << "  "
		<< "batchSubmits=" << avgBatchSubmits << "  "
		<< "batchFlushes=" << avgBatchFlushes << "  ";
	os.precision(3);
	os << "objectsTotal=" << s_objectsTotalMin << "/" << avgObjectsTotal << "/" << s_objectsTotalMax << "  "
		<< "objectsProcessed=" << s_objectsProcessedMin << "/" << avgObjectsProcessed << "/" << s_objectsProcessedMax << "  "
		<< "objectsRendered=" << s_objectsRenderedMin << "/" << avgObjectsRendered << "/" << s_objectsRenderedMax << "  "
		<< "presentMs=" << s_presentTimeMinMs << "/" << avgPresentMs << "/" << s_presentTimeMaxMs << "  ";
	os.precision(1);
	os << "layerDraws=";
	if (s_layerDrawsInWindow.empty())
	{
		os << "-"; // no per-category draws recorded this window (e.g. setLayerCategory() never called, or nothing drew) - distinct from "0", which would misleadingly suggest categories were tracked but all empty
	}
	else
	{
		bool firstLayer = true;
		for (const auto &entry : s_layerDrawsInWindow)
		{
			if (!firstLayer) os << ",";
			double avgForCategory = double(entry.second) / s_framesInWindow;
			os << entry.first << ":" << avgForCategory;
			firstLayer = false;
		}
	}
	os << "  gameState=";
	if (s_statesInWindow.empty())
	{
		os << "?"; // no state observed this window - shouldn't normally happen once the game has actually started, but never leave this column blank/malformed
	}
	else
	{
		bool first = true;
		for (const std::string &name : s_statesInWindow)
		{
			if (!first) os << "+"; // '+' not ',' - keeps this one whitespace-free field, so simple split-on-whitespace log parsing (e.g. the same log-summarizing scripts already used against this exact log format) doesn't need updating to handle a comma-containing field
			os << name;
			first = false;
		}
	}

	s_out << os.str() << std::endl; // flush every window - this is already throttled to WINDOW_SIZE frames, an explicit flush here is cheap and means a crash doesn't lose the last window's data

	s_statesInWindow.clear();

	s_framesInWindow = 0;
	s_frameTimeMinMs = 0.0;
	s_frameTimeMaxMs = 0.0;
	s_frameTimeSumMs = 0.0;
	s_updateTimeMinMs = 0.0;
	s_updateTimeMaxMs = 0.0;
	s_updateTimeSumMs = 0.0;
	s_drawCallSum = 0;
	s_stateSkippedSum = 0;
	s_stateAppliedSum = 0;
	s_targetSwitchSum = 0;
	s_captureSkippedSum = 0;
	s_captureEngagedSum = 0;
	s_batchSubmitSum = 0;
	s_batchFlushSum = 0;
	s_objectsTotalMin = 0.0;
	s_objectsTotalMax = 0.0;
	s_objectsTotalSum = 0.0;
	s_objectsProcessedMin = 0.0;
	s_objectsProcessedMax = 0.0;
	s_objectsProcessedSum = 0.0;
	s_objectsRenderedMin = 0.0;
	s_objectsRenderedMax = 0.0;
	s_objectsRenderedSum = 0.0;
	s_presentTimeMinMs = 0.0;
	s_presentTimeMaxMs = 0.0;
	s_presentTimeSumMs = 0.0;
	s_layerDrawsInWindow.clear();
}

void setEnabled(bool enabled)
{
	s_enabled = enabled;
}

void beginUpdate()
{
	if (!s_enabled) return;
	s_updateStartTicks = SDL_GetPerformanceCounter();
}

void endUpdate()
{
	if (!s_enabled) return;
	if (s_updateStartTicks == 0) return;

	uint64_t endTicks = SDL_GetPerformanceCounter();
	uint64_t freq = SDL_GetPerformanceFrequency();
	double updateMs = freq ? (double(endTicks - s_updateStartTicks) * 1000.0 / double(freq)) : 0.0;

	// Folded into the same per-window min/max/sum aggregation endFrame()
	// uses - accumulated here, applied to the window in endFrame() below
	// once both this frame's update and render timings are known.
	s_lastUpdateMs = updateMs;
	s_updateStartTicks = 0;
}

void beginFrame()
{
	if (!s_enabled) return;
	lazyInit();

	s_drawCalls = 0;
	s_stateChangesSkipped = 0;
	s_stateChangesApplied = 0;
	s_renderTargetSwitches = 0;
	s_captureSkipped = 0;
	s_captureEngaged = 0;
	s_batchSubmits = 0;
	s_batchFlushes = 0;
	s_currentLayerCategory.clear(); // no category attributed until the first setLayerCategory() call this frame
	s_frameStartTicks = SDL_GetPerformanceCounter();
}

void endFrame()
{
	if (!s_enabled) return;
	if (s_frameStartTicks == 0) return; // beginFrame() wasn't called (e.g. disabled mid-frame)

	// Records the current game state's name for this window - see
	// s_statesInWindow's declaration comment for why. core is a global,
	// always valid by the time any frame is being timed; getTopStateData()
	// can still be null very early (before any state has been pushed) or
	// during shutdown, both real, expected cases, not a bug - silently
	// skip rather than log an empty/misleading label in either case.
	//
	// gameSubModeSuffix appended directly (":cutscene"/":map"/"") rather
	// than tracked as a separate field - distinguishes gameplay,
	// cutscene, and the map screen, which otherwise all share this one
	// state name ("game") with nothing to tell them apart. See
	// Core::gameSubModeSuffix's declaration comment for the full
	// reasoning - added specifically because a real playtest log showed
	// exactly this ambiguity (cutscene, gameplay, and the map screen all
	// logged as indistinguishable "gameState=game" lines).
	if (core)
	{
		StateData *topState = core->getTopStateData();
		if (topState && !topState->name.empty())
			s_statesInWindow.insert(topState->name + core->gameSubModeSuffix);
	}

	uint64_t endTicks = SDL_GetPerformanceCounter();
	uint64_t freq = SDL_GetPerformanceFrequency();
	double frameMs = freq ? (double(endTicks - s_frameStartTicks) * 1000.0 / double(freq)) : 0.0;

	if (s_framesInWindow == 0)
	{
		s_frameTimeMinMs = frameMs;
		s_frameTimeMaxMs = frameMs;
		s_updateTimeMinMs = s_lastUpdateMs;
		s_updateTimeMaxMs = s_lastUpdateMs;
		s_objectsTotalMin = s_objectsTotal;
		s_objectsTotalMax = s_objectsTotal;
		s_objectsProcessedMin = s_objectsProcessed;
		s_objectsProcessedMax = s_objectsProcessed;
		s_objectsRenderedMin = s_objectsRendered;
		s_objectsRenderedMax = s_objectsRendered;
		s_presentTimeMinMs = s_lastPresentMs;
		s_presentTimeMaxMs = s_lastPresentMs;
	}
	else
	{
		if (frameMs < s_frameTimeMinMs) s_frameTimeMinMs = frameMs;
		if (frameMs > s_frameTimeMaxMs) s_frameTimeMaxMs = frameMs;
		if (s_lastUpdateMs < s_updateTimeMinMs) s_updateTimeMinMs = s_lastUpdateMs;
		if (s_lastUpdateMs > s_updateTimeMaxMs) s_updateTimeMaxMs = s_lastUpdateMs;
		if (double(s_objectsTotal) < s_objectsTotalMin) s_objectsTotalMin = s_objectsTotal;
		if (double(s_objectsTotal) > s_objectsTotalMax) s_objectsTotalMax = s_objectsTotal;
		if (double(s_objectsProcessed) < s_objectsProcessedMin) s_objectsProcessedMin = s_objectsProcessed;
		if (double(s_objectsProcessed) > s_objectsProcessedMax) s_objectsProcessedMax = s_objectsProcessed;
		if (double(s_objectsRendered) < s_objectsRenderedMin) s_objectsRenderedMin = s_objectsRendered;
		if (double(s_objectsRendered) > s_objectsRenderedMax) s_objectsRenderedMax = s_objectsRendered;
		if (s_lastPresentMs < s_presentTimeMinMs) s_presentTimeMinMs = s_lastPresentMs;
		if (s_lastPresentMs > s_presentTimeMaxMs) s_presentTimeMaxMs = s_lastPresentMs;
	}
	s_frameTimeSumMs += frameMs;
	s_updateTimeSumMs += s_lastUpdateMs;
	s_drawCallSum += s_drawCalls;
	s_stateSkippedSum += s_stateChangesSkipped;
	s_stateAppliedSum += s_stateChangesApplied;
	s_targetSwitchSum += s_renderTargetSwitches;
	s_captureSkippedSum += s_captureSkipped;
	s_captureEngagedSum += s_captureEngaged;
	s_batchSubmitSum += s_batchSubmits;
	s_batchFlushSum += s_batchFlushes;
	s_objectsTotalSum += s_objectsTotal;
	s_objectsProcessedSum += s_objectsProcessed;
	s_objectsRenderedSum += s_objectsRendered;
	s_presentTimeSumMs += s_lastPresentMs;
	s_framesInWindow++;

	if (s_framesInWindow >= WINDOW_SIZE)
		flushWindow();
}

void countDrawCall()
{
	if (!s_enabled) return;
	s_drawCalls++;
	if (!s_currentLayerCategory.empty())
		s_layerDrawsInWindow[s_currentLayerCategory]++;
}
void countStateChangeSkipped() { if (s_enabled) s_stateChangesSkipped++; }
void countStateChangeApplied() { if (s_enabled) s_stateChangesApplied++; }
void countRenderTargetSwitch() { if (s_enabled) s_renderTargetSwitches++; }
void countCaptureSkipped() { if (s_enabled) s_captureSkipped++; }
void countCaptureEngaged() { if (s_enabled) s_captureEngaged++; }
void countBatchSubmit() { if (s_enabled) s_batchSubmits++; }
void countBatchFlush() { if (s_enabled) s_batchFlushes++; }

void recordObjectCounts(unsigned int total, unsigned int processed, unsigned int rendered)
{
	if (!s_enabled) return;
	s_objectsTotal = total;
	s_objectsProcessed = processed;
	s_objectsRendered = rendered;
}

void beginPresent()
{
	if (!s_enabled) return;
	s_presentStartTicks = SDL_GetPerformanceCounter();
}

void endPresent()
{
	if (!s_enabled) return;
	if (s_presentStartTicks == 0) return;

	uint64_t endTicks = SDL_GetPerformanceCounter();
	uint64_t freq = SDL_GetPerformanceFrequency();
	s_lastPresentMs = freq ? (double(endTicks - s_presentStartTicks) * 1000.0 / double(freq)) : 0.0;
	s_presentStartTicks = 0;
}

void setLayerCategory(const char *category)
{
	if (!s_enabled) return;
	s_currentLayerCategory = category ? category : "";
}

} // namespace PerfLog
