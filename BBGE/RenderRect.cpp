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
#include "Quad.h"
#include "Core.h"


OutlineRect::OutlineRect() : RenderObject()
{
	lineSize = 1;
	renderCenter = false;
}

void OutlineRect::setWidthHeight(int w, int h)
{
	this->w = w;
	this->h = h;
	w2 = w/2;
	h2 = h/2;
}

void OutlineRect::setLineSize(int ls)
{
	lineSize = ls;
}

void OutlineRect::onRender()
{
	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	// TODO: glLineWidth(lineSize) has no SDL_Renderer equivalent
	// lines below always draw at 1px regardless of lineSize.
	// Most call sites use the default lineSize=1 anyway.

	glm::vec4 ul = core->transform.transformPoint(-w2,-h2);
	glm::vec4 ll = core->transform.transformPoint(-w2, h2);
	glm::vec4 ur = core->transform.transformPoint( w2,-h2);
	glm::vec4 lr = core->transform.transformPoint( w2, h2);

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColorFloat(renderer, effectiveColor.x, effectiveColor.y, effectiveColor.z, effectiveAlpha);

	SDL_FPoint lPts[2] = {{ul.x, ul.y}, {ll.x, ll.y}};
	SDL_RenderLines(renderer, lPts, 2);
	SDL_FPoint rPts[2] = {{ur.x, ur.y}, {lr.x, lr.y}};
	SDL_RenderLines(renderer, rPts, 2);
	SDL_FPoint uPts[2] = {{ul.x, ul.y}, {ur.x, ur.y}};
	SDL_RenderLines(renderer, uPts, 2);
	SDL_FPoint dPts[2] = {{ll.x, ll.y}, {lr.x, lr.y}};
	SDL_RenderLines(renderer, dPts, 2);

	if (renderCenter)
	{
		glm::vec4 midL = core->transform.transformPoint(-w2, 0);
		glm::vec4 midR = core->transform.transformPoint( w2, 0);
		glm::vec4 midU = core->transform.transformPoint(0, -h2);
		glm::vec4 midD = core->transform.transformPoint(0,  h2);

		SDL_SetRenderDrawColorFloat(renderer, 0.9f, 0.9f, 1.0f, effectiveAlpha);
		SDL_FPoint lrPts[2] = {{midL.x, midL.y}, {midR.x, midR.y}};
		SDL_RenderLines(renderer, lrPts, 2);
		SDL_FPoint udPts[2] = {{midU.x, midU.y}, {midD.x, midD.y}};
		SDL_RenderLines(renderer, udPts, 2);
	}
}


