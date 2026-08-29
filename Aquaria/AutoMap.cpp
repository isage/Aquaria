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
#include "AutoMap.h"
#include "RenderState.h"
#include "DSQ.h"
#include "Game.h"
#include "Avatar.h"

#define AUTOMAP_GRIDTILE		512

//#define FOR(ir,mx) for (int ir=0;ir<mx;ir++)

AutoMap::AutoMap() : RenderObject(), ActionMapper()
{
	//texture = 0;
	followCamera = 1;
	scale = Vector(0.4, 0.4);
	position = Vector(400,300);
	alpha = 0;
	vis = false;

	blip = Vector(1,1);
	blip.interpolateTo(Vector(4,4), 0.75, -1, 1, 1);

	initedGrid = false;

	//shadowTex = core->addTexture("particles/glow");
	setTexture("particles/glow");

	paintColor = Vector(1,1,1);
	autoMapMode = 0;

	addAction(MakeFunctionEvent(AutoMap, lmb), ActionMapper::MOUSE_BUTTON_LEFT);

	shadowTex = 0;
}

/*
void AutoMap::create(const std::string &startMap)
{
	bool done = false;
	
	while (!done)
	{
		dsq->game->smallLoadXML("NAIJACAVE");
	}
}
*/

void AutoMap::destroy()
{
	RenderObject::destroy();
	if (shadowTex)
	{
		shadowTex->destroy();
		delete shadowTex;
		shadowTex = 0;
	}
}

bool AutoMap::isOn()
{
	return vis;
}

void AutoMap::toggle(bool on)
{
	const float t = 0.2;
	if (on)
	{	
		autoMapMode = 0;
		dsq->game->togglePause(true);
		dsq->fade(1, t);
		dsq->main(t);
		core->overrideStartLayer = LR_AFTER_EFFECTS;
		core->overrideEndLayer = LR_MAX;
		vis = true;

		/*
		for (Children::iterator i = children.begin(); i != children.end(); i++)
		{
			(*i)->safeKill();
		}

		Vector c(dsq->game->cameraMax.x/2, dsq->game->cameraMax.y/2);
		int drawsz = AUTOMAP_GRIDTILE/TILE_SIZE;
		int hsz = drawsz*0.5f;

		c *= AUTOMAP_GRIDTILE;
		c /= TILE_SIZE;
		c += Vector(hsz, hsz);

		FOR(x,MAX_AUTOMAP_GRID)
		{
			FOR(y,MAX_AUTOMAP_GRID)
			{
				if (grid[x][y])
				{
					Quad *q = new Quad;
					q->setTexture("particles/WhiteGlow");

					float rx = float(x * AUTOMAP_GRIDTILE)/float(TILE_SIZE) + hsz - c.x;
					float ry = float(y * AUTOMAP_GRIDTILE)/float(TILE_SIZE) + hsz - c.y;

					q->position = Vector(rx, ry);
					q->color = Vector(0,0,0);
					addChild(q);
				}
			}
		}
		*/
		
		//alpha.interpolateTo(1, 0.5);
		alpha = 1;

		dsq->fade(0, t);
		dsq->main(t);		
	}
	else
	{
		dsq->fade(1, t);
		dsq->main(t);		

		core->overrideStartLayer = 0;
		core->overrideEndLayer = 0;
		vis = false;
		alpha.interpolateTo(0, 0.5);
		alpha = 0;
		dsq->game->togglePause(false);

		dsq->fade(0, t);
		dsq->main(t);
	}
}

void AutoMap::setGridFromWorld(Vector worldPos, int gridValue)
{
	int gx = worldPos.x / AUTOMAP_GRIDTILE;
	int gy = worldPos.y / AUTOMAP_GRIDTILE;

	if (gx<0 || gy<0 || gx >= MAX_AUTOMAP_GRID || gy >= MAX_AUTOMAP_GRID)
	{
		std::ostringstream os;
		os << "AutoMap::setGridFromWorld - exceeded grid size";
		os << "(" << gx <<", " << gy << ")";
		debugLog(os.str());
		return;	
	}

	grid[gx][gy] = gridValue;
}

void AutoMap::initGrid()
{
	/*
	int xsz = dsq->game->cameraMax.x/200;
	int ysz = dsq->game->cameraMax.y/200;
	*/
	//std::vector<std::vector> grid
	for (int x=0;x<MAX_AUTOMAP_GRID;x++)
	{
		for (int y=0;y<MAX_AUTOMAP_GRID;y++)
		{
			grid[x][y] = 1;
		}
	}

	initedGrid = true;
}

void AutoMap::paint(const Vector &pos)
{
	Quad *q = new Quad("particles/WhiteGlow", pos);
	q->setLife(1);
	q->setDecayRate(0.5);
	q->color = paintColor;
	q->followCamera = 1;
	q->setWidthHeight(8,8);
	dsq->game->addRenderObject(q, this->layer);
}

void AutoMap::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);

	if (!initedGrid)
	{
		initGrid();
	}

	
	if (dsq->game->avatar)
	{
		setGridFromWorld(dsq->game->avatar->position, 0);
	}
	

	if (vis && alpha.x == 1)
	{
		const float maxScale=1.25, minScale=0.5;
		blip.update(dt);
		/*
		static Vector lastHeldPos;
		if (core->mouse.buttons.left)
		{
			if (!lastHeldPos.isZero() && !(core->mouse.position - lastHeldPos).isLength2DIn(32))
			{
				Vector diff = core->mouse.position - lastHeldPos;
				int len = diff.getLength2D();
				for (int i = 0; i < len; i+=8)
				{
					paint(lastHeldPos + diff * i);
				}
			}
			else
			{
				paint(core->mouse.position);
			}
			lastHeldPos = core->mouse.position;
		}
		else
		{
			lastHeldPos = Vector(0,0);
		}
		*/

		//bool c=false;

		Vector sTarget=scale;		
		float spd = 0.5;
		if (core->mouse.scrollWheelChange < 0)
		{
			scale -= Vector(spd*0.05f,spd*0.05f);
		}
		else if (core->mouse.scrollWheelChange > 0)
		{
			scale += Vector(spd*0.05f,spd*0.05f);
		}
		if (scale.x < minScale)
			scale = Vector(minScale, minScale);
		if (scale.x > maxScale)
			scale = Vector(maxScale, maxScale);
		if (core->mouse.buttons.middle)
		{
			offset += core->mouse.change*scale;
		}
		//scale.interpolateTo(sTarget, 0.1);
	}
}

void AutoMap::lmb()
{
	if (autoMapMode == 0)
		autoMapMode = 1;
	else
		autoMapMode = 0;
}

void AutoMap::onRender()
{
//	if (!doRender) return;
	if (alpha.x == 0) return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	RenderObject::lastTextureApplied = 0;
	float alphaValue = alpha.x;

	int ysz = dsq->game->cameraMax.y/TILE_SIZE;
	int xsz = dsq->game->cameraMax.x/TILE_SIZE;

	TileVector t(Vector(dsq->game->cameraMax.x/2, dsq->game->cameraMax.y/2));

	int skip = 4;

	if (alphaValue > 0)
	{
		RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

		for (int y = 0; y < ysz; y += skip)
		{
			float f = float(y)/float(ysz);
			f = 0.8f-(f*0.5f);
			SDL_SetRenderDrawColorFloat(renderer, 0.5f*f, 0.75f*f, 1*f, alphaValue);

			int rowStart = -1;
			int x = 0;
			for (x = 0; x < xsz; x++)
			{
				if (dsq->game->getGrid(TileVector(x,y))!=OT_BLACK)
				{
					if (rowStart == -1)	
					{
						rowStart = x;
					}
				}
				else
				{
					if (rowStart != -1)
					{
						glm::vec3 p0 = core->transform.transformPoint(rowStart-t.x, y-t.y);
						glm::vec3 p1 = core->transform.transformPoint(x-t.x, y-t.y);
						SDL_FPoint pts[2] = {{p0.x,p0.y},{p1.x,p1.y}};
						SDL_RenderLines(renderer, pts, 2);
						rowStart = -1;
					}
				}
			}
			if (rowStart != -1)
			{
				glm::vec3 p0 = core->transform.transformPoint(rowStart-t.x, y-t.y);
				glm::vec3 p1 = core->transform.transformPoint(x-t.x, y-t.y);
				SDL_FPoint pts[2] = {{p0.x,p0.y},{p1.x,p1.y}};
				SDL_RenderLines(renderer, pts, 2);
			}
		}
	}

	TileVector nt(dsq->game->avatar->position);

	core->transform.pushMatrix();
	core->transform.translate(nt.x - t.x, nt.y - t.y, 0);

	drawCircle(blip.x*16, 8, 0.5f, 0.75f, 1.0f, alphaValue*0.5f);

	core->transform.popMatrix();
}

