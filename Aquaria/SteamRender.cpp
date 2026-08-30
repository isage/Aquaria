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
#include "DrawBatch.h"
#include "RenderState.h"
#include "PerfLog.h"

#include "../BBGE/AfterEffect.h"

SteamRender::SteamRender() : RenderObject()
{
	cull = false;
	//alpha = 0.1f;
	alpha = 0.7;
	setTexture("Particles/Steam");
	texture->repeat = true;
	rippleDelay = 2;
	setBlendType(BLEND_ADD);
}

void SteamRender::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);
}

void SteamRender::onRender()
{
	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;
	SDL_Texture *tex = texture ? texture->sdlTexture : 0;

	for (Path *p = dsq->game->getFirstPathOfType(PATH_STEAM); p; p = p->nextOfType)
	{
		if (p->active)
		{

			int w2 = p->rect.getWidth()/2;

			if (true)
			{
				const int sz = p->nodes.size()-1;
				for (int n = 0; n < sz; n++)
				{
					const PathNode *n1 = &p->nodes[n];
					const PathNode *n2 = &p->nodes[n+1];
					const Vector p1 = n1->position;

					const Vector p2 = n2->position;
					Vector diff = p2-p1;
					if (!diff.isZero())
					{
						Vector pl = diff.getPerpendicularLeft();
						Vector pr = diff.getPerpendicularRight();
						pl.setLength2D(w2);
						pr.setLength2D(w2);

						if (isTouchingLine(p1, p2, dsq->screenCenter, dsq->cullRadius + p->rect.getWidth()/2.0f))
						{
							const Vector p15 = n1->position + diff * 0.25f;
							const Vector p25 = n2->position - diff * 0.25f;
							const Vector r1 = p1+pl;
							const Vector r2 = p1+pr;
							const Vector r3 = p15+pl;
							const Vector r4 = p15+pr;
							const Vector r5 = p25+pl;
							const Vector r6 = p25+pr;
							const Vector r7 = p2+pl;
							const Vector r8 = p2+pr;
							const float len = diff.getLength2D();
							const float texScale = len/256.0f;

							// GL_QUAD_STRIP (8 points -> 3 quads) -> triangle list.
							glm::vec3 w1 = core->transform.transformPoint(r1.x, r1.y);
							glm::vec3 w2p = core->transform.transformPoint(r2.x, r2.y);
							glm::vec3 w3 = core->transform.transformPoint(r3.x, r3.y);
							glm::vec3 w4 = core->transform.transformPoint(r4.x, r4.y);
							glm::vec3 w5 = core->transform.transformPoint(r5.x, r5.y);
							glm::vec3 w6 = core->transform.transformPoint(r6.x, r6.y);
							glm::vec3 w7 = core->transform.transformPoint(r7.x, r7.y);
							glm::vec3 w8 = core->transform.transformPoint(r8.x, r8.y);

							SDL_FColor edgeCol = {1,1,1,0};
							SDL_FColor midCol = {1,1,1,alpha.x};

							SDL_Vertex v[8];
							v[0].position={w1.x,w1.y}; v[0].tex_coord={(0)*texScale+p->animOffset, 0}; v[0].color=edgeCol;
							v[1].position={w2p.x,w2p.y}; v[1].tex_coord={(0)*texScale+p->animOffset, 1}; v[1].color=edgeCol;
							v[2].position={w3.x,w3.y}; v[2].tex_coord={(0+0.25f)*texScale+p->animOffset, 0}; v[2].color=midCol;
							v[3].position={w4.x,w4.y}; v[3].tex_coord={(0+0.25f)*texScale+p->animOffset, 1}; v[3].color=midCol;
							v[4].position={w5.x,w5.y}; v[4].tex_coord={(1-0.25f)*texScale+p->animOffset, 0}; v[4].color=midCol;
							v[5].position={w6.x,w6.y}; v[5].tex_coord={(1-0.25f)*texScale+p->animOffset, 1}; v[5].color=midCol;
							v[6].position={w7.x,w7.y}; v[6].tex_coord={(1)*texScale+p->animOffset, 0}; v[6].color=edgeCol;
							v[7].position={w8.x,w8.y}; v[7].tex_coord={(1)*texScale+p->animOffset, 1}; v[7].color=edgeCol;

							static const int idx[18] = {
								0,1,3, 0,3,2,
								2,3,5, 2,5,4,
								4,5,7, 4,7,6
							};

							if (tex) RenderState::setTextureBlendMode(tex, currentBlendMode);
							else RenderState::setRenderDrawBlendMode(renderer, currentBlendMode);
							DrawBatch::flush(); // Step 6: not routed through DrawBatch - must flush first to preserve draw order
							SDL_RenderGeometry(renderer, tex, v, 8, idx, 18);
							PerfLog::countDrawCall();
						}
					}
				}
			}
		}
	}
}

