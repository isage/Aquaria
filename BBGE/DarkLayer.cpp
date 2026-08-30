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
#include "DarkLayer.h"
#include "DrawBatch.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Core.h"

DarkLayer::DarkLayer()
{
	quality = 0;
	active = false;
	layer = -1;
	renderLayer = -1;

	stretch = 4;
}

void DarkLayer::unloadDevice()
{
	if (useFrameBuffer)
		frameBuffer.unloadDevice();
}

void DarkLayer::reloadDevice()
{
	if (useFrameBuffer)
		frameBuffer.reloadDevice();
}

int DarkLayer::getRenderLayer()
{
	return renderLayer;
}

bool DarkLayer::isUsed()
{
	return layer > -1 && active;
}

void DarkLayer::setLayers(int layer, int rl)
{
	this->layer = layer;
	this->renderLayer = rl;
}

void DarkLayer::init(int quality, bool useFrameBufferParam)
{
	useFrameBuffer = useFrameBufferParam;

	this->quality = quality;

	if (!frameBuffer.init(quality, quality))
		debugLog("Dark Layer: not using framebuffer, expect bugs");
	else
		debugLog("Dark Layer: using framebuffer");
}

int DarkLayer::getLayer()
{
	return layer;
}

void DarkLayer::toggle(bool on)
{
	this->active = on;
}

void DarkLayer::preRender()
{
	bool verbose = core->coreVerboseDebug;
	if (layer != -1)
	{
		if (verbose) debugLog("startCapture");

		frameBuffer.startCapture();

		if (verbose) debugLog("clearColor");

		Vector savedClearColor = core->getClearColor();
		core->setClearColor(Vector(1, 1, 1));

		if (verbose) debugLog("render");

		core->render(layer, layer, false); 

		core->setClearColor(savedClearColor);

		if (verbose) debugLog("endCapture");

		frameBuffer.endCapture();

		if (verbose) debugLog("done");
	}
}

void DarkLayer::render()
{
	if (renderLayer != -1)
	{
		SDL_Renderer *renderer = core->getRenderer();
		if (!renderer) return;

		SDL_Texture *tex = useFrameBuffer ? frameBuffer.getTexture() : 0;

		// subtractive blend! (using color) - GL_ZERO,GL_SRC_COLOR is exactly
		// SDL_BLENDMODE_MOD's formula.
		if (tex)
			RenderState::setTextureBlendMode(tex, SDL_BLENDMODE_MOD);
		else
			RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_MOD);

		float width  =  core->getWindowWidth();
		float height =  core->getWindowHeight();
		float offX   = 0.0f;
		float offY   = 0.0f;

		// NOTE: drawn with an isolated identity transform (matching the old
		// glPushMatrix()/glLoadIdentity()/glPopMatrix() pair) - this overlay
		// is meant to cover the whole screen in device coordinates, not
		// whatever the current object's world transform happens to be.
		RenderTransformStack xf;

		glm::vec3 p0 = xf.transformPoint(offX-stretch, offY-stretch);
		glm::vec3 p1 = xf.transformPoint(offX-stretch, height+offY+stretch);
		glm::vec3 p2 = xf.transformPoint(width+offX+stretch, height+offY+stretch);
		glm::vec3 p3 = xf.transformPoint(width+offX+stretch, offY-stretch);

		SDL_FColor white = {1,1,1,1};
		SDL_Vertex v[4];
		v[0].position={p0.x,p0.y}; v[0].tex_coord={0,1}; v[0].color=white;
		v[1].position={p1.x,p1.y}; v[1].tex_coord={0,0}; v[1].color=white;
		v[2].position={p2.x,p2.y}; v[2].tex_coord={1,0}; v[2].color=white;
		v[3].position={p3.x,p3.y}; v[3].tex_coord={1,1}; v[3].color=white;
		static const int idx[6] = {0,1,2,0,2,3};

		DrawBatch::flush(); // Step 6: not routed through DrawBatch - must flush first to preserve draw order
		SDL_RenderGeometry(renderer, tex, v, 4, idx, 6);
		PerfLog::countDrawCall();
		RenderObject::lastTextureApplied = 0;
	}
}
