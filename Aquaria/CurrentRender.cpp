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

CurrentRender::CurrentRender() : RenderObject()
{
	cull = false;
	//alpha = 0.2f;
	setTexture("Particles/Current");
	texture->repeat = true;
	rippleDelay = 2;
}

void CurrentRender::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);

	/*
	rippleDelay -= dt;
	if (rippleDelay < 0)
	{
		for (int i = 0; i < dsq->game->paths.size()-1; i++)
		{
			Path *p = dsq->game->paths[i];
			for (int n = 0; n < p->nodes.size()-1; i++)
			{
				PathNode *n1 = &p->nodes[n];
				PathNode *n2 = &p->nodes[n+1];
				Vector diff = n2->position - n1->position;
				Vector pos = n1->position + diff*p->animOffset;
				// spawn effect at pos
				if (core->afterEffectManager)
					core->afterEffectManager->addEffect(new ShockEffect(Vector(core->width/2, core->height/2),pos,0.04,0.06,15,0.2f));
			}
		}
		rippleDelay = 1.0;
	}
	*/
}

void CurrentRender::onRender()
{
	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;
	SDL_Texture *tex = texture ? texture->sdlTexture : 0;

	for (Path *p = dsq->game->getFirstPathOfType(PATH_CURRENT); p; p = p->nextOfType)
	{
		if (p->active)
		{
			int w2 = p->rect.getWidth()/2;

			int sz = p->nodes.size()-1;
			for (int n = 0; n < sz; n++)
			{
				PathNode *n1 = &p->nodes[n];
				PathNode *n2 = &p->nodes[n+1];
				Vector p1 = n1->position;
				Vector p2 = n2->position;
				Vector diff = p2-p1;
				Vector d = diff;
				d.setLength2D(p->rect.getWidth());
				p1 -= d*0.75f;
				p2 += d*0.75f;
				diff = p2 - p1;

				if (!diff.isZero())
				{
					Vector pl = diff.getPerpendicularLeft();
					Vector pr = diff.getPerpendicularRight();
					pl.setLength2D(w2);
					pr.setLength2D(w2);

					Vector p15 = p1 + diff * 0.25f;
					Vector p25 = p2 - diff * 0.25f;
					Vector r1 = p1+pl;
					Vector r2 = p1+pr;
					Vector r3 = p15+pl;
					Vector r4 = p15+pr;
					Vector r5 = p25+pl;
					Vector r6 = p25+pr;
					Vector r7 = p2+pl;
					Vector r8 = p2+pr;
					float len = diff.getLength2D();
					float texScale = len/256.0f;

					if (isTouchingLine(p1, p2, dsq->screenCenter, dsq->cullRadius+p->rect.getWidth()/2.0f))
					{
						// GL_QUAD_STRIP (r1,r2, r3,r4, r5,r6, r7,r8) with
						// per-vertex alpha fading the ends to 0 -> 3 quads,
						// 6 triangles, one batched draw.
						Vector pts[8] = {r1,r2,r3,r4,r5,r6,r7,r8};
						float u[4] = {
							(0)*texScale+p->animOffset,
							(0+0.25f)*texScale+p->animOffset,
							(1-0.25f)*texScale+p->animOffset,
							(1)*texScale+p->animOffset
						};
						float a[4] = {0, p->amount, p->amount, 0};

						SDL_Vertex verts[8];
						for (int i = 0; i < 4; i++)
						{
							glm::vec3 wl = core->transform.transformPoint(pts[i*2].x, pts[i*2].y);
							glm::vec3 wr = core->transform.transformPoint(pts[i*2+1].x, pts[i*2+1].y);
							SDL_FColor col = {1,1,1,a[i]};
							verts[i*2+0].position={wl.x,wl.y}; verts[i*2+0].tex_coord={u[i],0}; verts[i*2+0].color=col;
							verts[i*2+1].position={wr.x,wr.y}; verts[i*2+1].tex_coord={u[i],1}; verts[i*2+1].color=col;
						}

						static const int idx[18] = {
							0,1,3, 0,3,2,
							2,3,5, 2,5,4,
							4,5,7, 4,7,6
						};

						if (tex)
							RenderState::setTextureBlendMode(tex, currentBlendMode);
						else
							RenderState::setRenderDrawBlendMode(renderer, currentBlendMode);
						DrawBatch::flush(); // Step 6: not routed through DrawBatch - must flush first to preserve draw order
						SDL_RenderGeometry(renderer, tex, verts, 8, idx, 18);
						PerfLog::countDrawCall();
					}
				}
			}
		}
	}
}

