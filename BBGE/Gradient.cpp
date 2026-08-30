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
#include "Gradient.h"
#include "DrawBatch.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Core.h"

Gradient::Gradient() : RenderObject()
{
	autoWidth = autoHeight = 0;
}

void Gradient::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);

	if (autoWidth == AUTO_VIRTUALWIDTH)
	{
		scale.x = core->getVirtualWidth();
	}

	if (autoHeight == AUTO_VIRTUALHEIGHT)
	{
		scale.y = core->getVirtualWidth();
	}
}

void Gradient::makeVertical(Vector c1, Vector c2)
{
	ulc0 = c1;
	ulc1 = c1;
	ulc2 = c2;
	ulc3 = c2;
}

void Gradient::makeHorizontal(Vector c1, Vector c2)
{
	ulc0 = c1;
	ulc1 = c2;
	ulc2 = c2;
	ulc3 = c1;
}

void Gradient::onRender()
{
	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	// NOTE: uses `color`/`alpha` directly (not effectiveColor/
	// effectiveAlpha) matching the original, which called glColor4f()
	// here using its own raw color/alpha - overriding whatever the
	// render-layer-multiplied color renderCall() had set beforehand.
	// Preserved exactly rather than "corrected" to use the layer-aware
	// value, since that would be a behavior change.
	glm::vec3 p0 = core->transform.transformPoint(-0.5f,  0.5f);
	glm::vec3 p1 = core->transform.transformPoint( 0.5f,  0.5f);
	glm::vec3 p2 = core->transform.transformPoint( 0.5f, -0.5f);
	glm::vec3 p3 = core->transform.transformPoint(-0.5f, -0.5f);

	SDL_Vertex v[4];
	v[0].position = {p0.x, p0.y}; v[0].tex_coord = {0,0};
	v[0].color = {ulc2.x*color.x, ulc2.y*color.y, ulc2.z*color.z, alpha.x};

	v[1].position = {p1.x, p1.y}; v[1].tex_coord = {0,0};
	v[1].color = {ulc3.x*color.x, ulc3.y*color.y, ulc3.z*color.z, alpha.x};

	v[2].position = {p2.x, p2.y}; v[2].tex_coord = {0,0};
	v[2].color = {ulc0.x*color.x, ulc0.y*color.y, ulc0.z*color.z, alpha.x};

	v[3].position = {p3.x, p3.y}; v[3].tex_coord = {0,0};
	v[3].color = {ulc1.x*color.x, ulc1.y*color.y, ulc1.z*color.z, alpha.x};

	static const int indices[6] = {0,1,2,0,2,3};

	RenderState::setRenderDrawBlendMode(renderer, currentBlendMode);
	DrawBatch::flush(); // Step 6: not routed through DrawBatch - must flush first to preserve draw order
	SDL_RenderGeometry(renderer, NULL, v, 4, indices, 6);
	PerfLog::countDrawCall();
}

