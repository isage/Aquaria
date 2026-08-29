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
#include "GridRender.h"
#include "RenderState.h"
#include "PerfLog.h"
#include <vector>

PathRender::PathRender() : RenderObject()
{
	//color = Vector(1, 0, 0);
	position.z = 5;
	cull = false;
	alpha = 0.5f;
}

void PathRender::onRender()
{	
	const int pathcount = dsq->game->getNumPaths();
	if (pathcount <= 0)
		return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	for (int i = 0; i < pathcount; i++)
	{
		Path *p = dsq->game->getPath(i);
		float lineR=1, lineG=0.5f, lineB=0.5f;
#ifdef AQUARIA_BUILD_SCENEEDITOR
		if (dsq->game->sceneEditor.selectedIdx == i)
		{
			lineR=1; lineG=1; lineB=1;
		}
#endif

		if (p->nodes.size() >= 2)
		{
			std::vector<SDL_FPoint> pts;
			pts.reserve(p->nodes.size());
			for (int n = 0; n < p->nodes.size(); n++)
			{
				glm::vec3 wp = core->transform.transformPoint(p->nodes[n].position.x, p->nodes[n].position.y);
				pts.push_back({wp.x, wp.y});
			}
			SDL_SetRenderDrawColorFloat(renderer, lineR, lineG, lineB, 0.75f);
			SDL_RenderLines(renderer, pts.data(), (int)pts.size());
		}

		for (int n = 0; n < p->nodes.size(); n++)
		{
			PathNode *nd = &p->nodes[n];

			if (n == 0)
			{
				if (p->pathShape == PATHSHAPE_RECT)
				{
					glm::vec3 c0 = core->transform.transformPoint(nd->position.x+p->rect.x1, nd->position.y+p->rect.y2);
					glm::vec3 c1 = core->transform.transformPoint(nd->position.x+p->rect.x2, nd->position.y+p->rect.y2);
					glm::vec3 c2 = core->transform.transformPoint(nd->position.x+p->rect.x2, nd->position.y+p->rect.y1);
					glm::vec3 c3 = core->transform.transformPoint(nd->position.x+p->rect.x1, nd->position.y+p->rect.y1);

					SDL_FColor fillCol = {0.5f, 0.5f, 1, 0.2f};
					SDL_Vertex v[4];
					v[0].position={c0.x,c0.y}; v[0].tex_coord={0,0}; v[0].color=fillCol;
					v[1].position={c1.x,c1.y}; v[1].tex_coord={0,0}; v[1].color=fillCol;
					v[2].position={c2.x,c2.y}; v[2].tex_coord={0,0}; v[2].color=fillCol;
					v[3].position={c3.x,c3.y}; v[3].tex_coord={0,0}; v[3].color=fillCol;
					static const int idx[6] = {0,1,2,0,2,3};
					SDL_RenderGeometry(renderer, NULL, v, 4, idx, 6);
					PerfLog::countDrawCall();
					SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 0.3f);
					SDL_FPoint outline[5] = {{c3.x,c3.y}, {c2.x,c2.y}, {c1.x,c1.y}, {c0.x,c0.y}, {c3.x,c3.y}};
					SDL_RenderLines(renderer, outline, 5);
				}
				else
				{
					core->transform.pushMatrix();
					core->transform.translate(nd->position.x, nd->position.y, 0);
					drawCircle(p->rect.getWidth()*0.5f, 16, 0.5f, 0.5f, 1, 0.5f);
					core->transform.popMatrix();
				}
			}

			float a = 0.75f;
			if (!p->active)
				a = 0.3f;

			float circR=1, circG=0.5f, circB=0.5f;
#ifdef AQUARIA_BUILD_SCENEEDITOR
			if (dsq->game->sceneEditor.selectedIdx == i)
			{
				circR=1; circG=1; circB=1;
			}
#endif

			core->transform.pushMatrix();
			core->transform.translate(nd->position.x, nd->position.y, 0);
			drawCircle(32, 1, circR, circG, circB, a);
			core->transform.popMatrix();
		}
	}
}
