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
#include "QuadTrail.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Core.h"
#include <assert.h>
#include <vector>

QuadTrail::QuadTrail(int maxPoints, float pointDist)
: RenderObject(), maxPoints(maxPoints), pointDist(pointDist), numPoints(0)
{
	quadTrailAlphaEffect = QTAE_NORMAL;
	cull = false;
	repeatTexture = 1;

	lifeRate = 0.5;
}

void QuadTrail::addPoint(const Vector &point)
{
	if (numPoints > 0)
	{
		if ((points.back().point - point).isLength2DIn(pointDist))
		{
			backOffset = point - points.back().point;
			return;
		}
	}

	QuadTrailPoint p;
	p.point = point;

	points.push_back(p);
	numPoints++;
	if (numPoints >= maxPoints)
		points.pop_front();

	backOffset.x = 0;
	backOffset.y = 0;
}

void QuadTrail::onRender()
{
	if (numPoints < 2) return;

	int c = 0;
	Vector p, diff, dl, dr;
	Vector lastPoint;

	const float texScale = texture ? float(numPoints*pointDist)/texture->width : 1.0f;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	SDL_Texture *tex = texture ? texture->sdlTexture : 0;

	// GL_QUAD_STRIP -> triangle list, one quad per consecutive pair of
	// left/right edge points (same conversion as Quad's strip mode).
	static std::vector<SDL_Vertex> verts;
	verts.clear();
	verts.reserve(points.size() * 2);

	for (Points::iterator i = points.begin(); i != points.end(); i++)
	{
		float vAlpha = (quadTrailAlphaEffect == QTAE_NORMAL) ? (*i).life : 1.0f;

		if (c == 0)
		{
			lastPoint = (*i).point;
			c++;
			continue;
		}
		p = (*i).point;

		if (c == numPoints-1)
			p += backOffset;

		diff = p - lastPoint;
		if (texture)
			diff.setLength2D(texture->width*0.5f);
		else
			diff.setLength2D(32);
		dl = diff.getPerpendicularLeft();
		dr = diff.getPerpendicularRight();

		glm::vec4 wl = core->transform.transformPoint(p.x+dl.x, p.y+dl.y);
		glm::vec4 wr = core->transform.transformPoint(p.x+dr.x, p.y+dr.y);

		SDL_Vertex v;
		v.color = {1, 1, 1, vAlpha};
		v.position = {wl.x, wl.y}; v.tex_coord = {0, (float(c)/numPoints)*texScale}; verts.push_back(v);
		v.position = {wr.x, wr.y}; v.tex_coord = {1, (float(c+1)/numPoints)*texScale}; verts.push_back(v);

		c++;
		lastPoint = (*i).point;
	}

	if (verts.size() >= 4)
	{
		static std::vector<int> indices;
		indices.clear();
		size_t numQuads = verts.size()/2 - 1;
		indices.reserve(numQuads * 6);
		for (size_t q = 0; q < numQuads; q++)
		{
			int base = (int)q * 2;
			indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+3);
			indices.push_back(base+0); indices.push_back(base+3); indices.push_back(base+2);
		}

		if (tex)
			RenderState::setTextureBlendMode(tex, currentBlendMode);
		else
			RenderState::setRenderDrawBlendMode(renderer, currentBlendMode);
		SDL_RenderGeometry(renderer, tex, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
		PerfLog::countDrawCall();
	}
}

void QuadTrail::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);

	for (Points::iterator i = points.begin(); i != points.end(); i++)
	{
		(*i).life -= dt * lifeRate;
		if ((*i).life <= 0)
			(*i).life = 0;
	}
}
