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
#include "ScreenTransition.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Core.h"

ScreenTransition::ScreenTransition() : RenderObject()
{
	createTexture();

	cull = false;
	followCamera = 1;
	alpha = 0;
}

void ScreenTransition::createTexture()
{
	width = core->getVirtualWidth();
	height = core->getVirtualHeight();
	
	windowWidth = core->getWindowWidth();
	windowHeight = core->getWindowHeight();

	// No power-of-2 padding needed - SDL target textures aren't
	// constrained to POT sizes the way the old GL FBO texture was, so
	// captureBuffer is created at exactly the window size.
	captureBuffer.init(windowWidth, windowHeight);
}

void ScreenTransition::destroyTexture()
{
	captureBuffer.unloadDevice();
}

void ScreenTransition::unloadDevice()
{
	RenderObject::unloadDevice();
	destroyTexture();
}

void ScreenTransition::reloadDevice()
{
	RenderObject::reloadDevice();
	createTexture();
}

void ScreenTransition::capture()
{	
	core->render();

	if (captureBuffer.isInited() && core->frameBuffer.isInited())
	{
		captureBuffer.copyFrom(core->frameBuffer);
	}

	core->showBuffer();
}

void ScreenTransition::go(float time)
{
	capture();
	transition(time);
}

void ScreenTransition::transition(float time)
{
	core->resetTimer();
	alpha = 1;
	alpha.interpolateTo(0, time);
}

bool ScreenTransition::isGoing()
{
	return alpha.isInterpolating();
}

void ScreenTransition::onRender()
{
	if (alpha.x == 0) return;
	
	float width2 = float(width)/2;
	float height2 = float(height)/2;

	// pw/ph are always 1.0 now (no POT padding - see createTexture()),
	// kept as named values matching the ratio the old code computed, in
	// case captureBuffer's size and the window ever diverge again.
	const float pw = captureBuffer.getWidth() > 0 ? float(windowWidth)/float(captureBuffer.getWidth()) : 1.0f;
	const float ph = captureBuffer.getHeight() > 0 ? float(windowHeight)/float(captureBuffer.getHeight()) : 1.0f;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer || !captureBuffer.isInited()) return;

	SDL_Texture *tex = captureBuffer.getTexture();

	glm::vec3 c0 = core->transform.transformPoint(-width2, +height2);
	glm::vec3 c1 = core->transform.transformPoint(+width2, +height2);
	glm::vec3 c2 = core->transform.transformPoint(+width2, -height2);
	glm::vec3 c3 = core->transform.transformPoint(-width2, -height2);

	SDL_FColor col = {1, 1, 1, alpha.x};

	// c0/c1 are the BOTTOM screen vertices (+height2, Y-down), c2/c3 are
	// the TOP screen vertices. V=0 is the texture's top row (confirmed
	// empirically), so bottom vertices sample V=ph (texture bottom) and
	// top vertices sample V=0 (texture top). The previous assignment had
	// this backward (copied from the original GL code's convention, which
	// doesn't apply to this SDL3 pipeline - see the migration notes).
	SDL_Vertex v[4];
	v[0].position = {c0.x, c0.y}; v[0].tex_coord = {0,  ph}; v[0].color = col;
	v[1].position = {c1.x, c1.y}; v[1].tex_coord = {pw, ph}; v[1].color = col;
	v[2].position = {c2.x, c2.y}; v[2].tex_coord = {pw, 0};  v[2].color = col;
	v[3].position = {c3.x, c3.y}; v[3].tex_coord = {0,  0};  v[3].color = col;

	static const int idx[6] = {0,1,2,0,2,3};

	RenderState::setTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
	SDL_RenderGeometry(renderer, tex, v, 4, idx, 6);
	PerfLog::countDrawCall();
	RenderObject::lastTextureApplied = 0;
}
