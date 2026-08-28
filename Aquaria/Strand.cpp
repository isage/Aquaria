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
#include "Segmented.h"
#include "RenderState.h"
#include "../BBGE/Core.h"

Strand::Strand(const Vector &position, int segs, int dist) : RenderObject(), Segmented(dist, dist)
{
	cull = false;
	segments.resize(segs);
	for (int i = 0; i < segments.size(); i++)
	{
		segments[i] = new RenderObject;
	}
	initSegments(position);
}

void Strand::destroy()
{
	RenderObject::destroy();
	for (int i = 0; i < segments.size(); i++)
	{
		segments[i]->destroy();
		delete segments[i];
	}
	segments.clear();
}

void Strand::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);
	updateSegments(position);
}

void Strand::onRender()
{
	const int numSegments = segments.size();
	if (numSegments == 0) return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	// Undoes this object's own world-position translate (segments[] store
	// absolute/world positions already) - matches the original's
	// un-paired glTranslatef(-position...): the enclosing
	// RenderObject::renderCall() pop's the whole accumulated transform
	// after onRender() returns, so there's nothing to restore here either.
	core->transform.translate(-position.x, -position.y, 0);

	RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	// Use fixed-point math to speed things up.  --achurch
	unsigned int r = (unsigned int)(color.x * (255<<8));
	unsigned int g = (unsigned int)(color.y * (255<<8));
	unsigned int b = (unsigned int)(color.z * (255<<8));
	unsigned int a = (255<<8);
	unsigned int dr = r/50;
	unsigned int dg = g/50;
	unsigned int db = b/50;
	unsigned int da = a/numSegments;

	// GL_LINE_STRIP had smooth per-vertex color interpolation; SDL_RenderLine
	// only takes one flat color per call, so each segment below is drawn
	// with its leading vertex's color instead of a true gradient - a close
	// approximation for this thin strand visual, not pixel-identical.
	glm::vec4 prevPt = core->transform.transformPoint(position.x, position.y);
	Uint8 pr=r>>8, pg=g>>8, pb=b>>8, pa=a>>8;

	glm::vec4 seg0 = core->transform.transformPoint(segments[0]->position.x, segments[0]->position.y);
	SDL_SetRenderDrawColor(renderer, pr, pg, pb, pa);
	SDL_RenderLine(renderer, prevPt.x, prevPt.y, seg0.x, seg0.y);
	prevPt = seg0;

	const int colorLimit = numSegments<50 ? numSegments : 50;
	int i;
	for (i = 1; i < colorLimit; i++)
	{
		r -= dr; g -= dg; b -= db; a -= da;
		glm::vec4 pt = core->transform.transformPoint(segments[i]->position.x, segments[i]->position.y);
		SDL_SetRenderDrawColor(renderer, r>>8, g>>8, b>>8, a>>8);
		SDL_RenderLine(renderer, prevPt.x, prevPt.y, pt.x, pt.y);
		prevPt = pt;
	}
	for (; i < numSegments; i++)
	{
		a -= da;
		glm::vec4 pt = core->transform.transformPoint(segments[i]->position.x, segments[i]->position.y);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, a>>8);
		SDL_RenderLine(renderer, prevPt.x, prevPt.y, pt.x, pt.y);
		prevPt = pt;
	}
}
