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
#include "AfterEffect.h"
#include "DrawBatch.h"
#include "RenderState.h"
#include "PerfLog.h"
//#include "math.h"

#include <assert.h>
#include <vector>

Effect::Effect()
{
	done = false;
	rate = 1;
}

AfterEffectManager::AfterEffectManager(int xDivs, int yDivs)
{
	active = false;
	numEffects = 0;
	bRenderGridPoints = true;

	screenWidth = core->getWindowWidth();
	screenHeight = core->getWindowHeight();

	this->xDivs = 0;
	this->yDivs = 0;

	drawGrid = 0;

	this->xDivs = xDivs;
	this->yDivs = yDivs;

	reloadDevice();

	if (xDivs != 0 && yDivs != 0)
	{
		drawGrid = new Vector * [xDivs];
		for (int i = 0; i < xDivs; i++)
		{
			drawGrid[i] = new Vector [yDivs];
		}
	}
}


AfterEffectManager::~AfterEffectManager()
{
	if (drawGrid)
	{
		int i;
		for (i = 0; i < xDivs; i++)
		{
			delete[] drawGrid[i];
		}	
		delete[] drawGrid;
	}
	deleteEffects();
}

void AfterEffectManager::deleteEffects()
{
	for (int i = 0; i < effects.size(); i++)
	{
		if (effects[i])
		{
			delete effects[i];
		}
	}
	effects.clear();
	numEffects=0;
	while (!openSpots.empty())
		openSpots.pop();
}

void AfterEffectManager::clear()
{
	deleteEffects();
	resetGrid();
}

void AfterEffectManager::update(float dt)
{
	if (core->particlesPaused) return;	

	resetGrid();

	// Restored to match the original: `active` becomes true whenever a
	// frame buffer exists, not just when a real effect is running. The
	// previous round's change (gating this on effects.size() > 0) was a
	// deliberate, documented workaround for a real bug in render()'s xf
	// transform (see there) - now that the actual root cause is fixed
	// (copying the live core->transform instead of hand-rebuilding it),
	// there's no longer a reason to deviate from the original's
	// always-active behavior, and reverting avoids leaving an
	// undocumented, permanent behavior change for a bug that's actually
	// fixed now.
	if (core->frameBuffer.isInited())
		active = true;
	else
		active = false;

	for (int i = 0; i < effects.size(); i++)
	{		
		Effect *e = effects[i];
		if (e)
		{
			active = true;
			e->update(dt, drawGrid, xDivs, yDivs);
			if (e->done)
			{
				numEffects--;
				destroyEffect(i);
			}
		}
	}
}


void AfterEffectManager::resetGrid()
{
	for (int i = 0; i < xDivs; i++)
	{
		for (int j = 0; j < yDivs; j++)
		{
			drawGrid[i][j].x = i/(float)(xDivs-1);
			drawGrid[i][j].y = j/(float)(yDivs-1);
		}
	}
}

void AfterEffectManager::destroyEffect(int id)
{
	delete effects[id];
	effects[id] = 0;
	openSpots.push(id);
}

void AfterEffectManager::render()
{
	assert(core->frameBuffer.isInited());

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	RenderTransformStack xf = core->transform;
	xf.translate(core->cameraPos.x, core->cameraPos.y, 0);
	xf.scale(core->invGlobalScale, core->invGlobalScale, 1);

	// Ends the main-scene capture early and switches drawing back to the
	// real backbuffer - see the comment in Core::render() about why later
	// layers then draw directly on top of this instead of also being
	// captured.
	core->frameBuffer.endCapture();

	DrawBatch::flush(); // Step 6: defensive flush before a raw clear, even though endCapture()'s target switch already flushed internally
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	renderGrid(xf);
}

void AfterEffectManager::renderGrid(const RenderTransformStack &xf)
{
	screenWidth = core->getWindowWidth();
	screenHeight = core->getWindowHeight();

	float percentX, percentY;
	percentX = (float)screenWidth/(float)textureWidth;
	percentY = (float)screenHeight/(float)textureHeight;

	int vw = core->getVirtualWidth();
	int vh = core->getVirtualHeight();
	int offx = -core->getVirtualOffX();
	int offy = -core->getVirtualOffY();

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;
	SDL_Texture *tex = core->frameBuffer.getTexture();

	// One batched draw for the whole grid (xDivs-1)*(yDivs-1) quads,
	// rather than the old per-cell glBegin/glEnd immediate-mode calls.
	const int cellsX = xDivs - 1;
	const int cellsY = yDivs - 1;
	if (cellsX <= 0 || cellsY <= 0) return;

	static std::vector<SDL_Vertex> verts;
	verts.clear();
	static std::vector<int> indices;
	indices.clear();
	verts.reserve((size_t)cellsX * cellsY * 4);
	indices.reserve((size_t)cellsX * cellsY * 6);

	SDL_FColor white = {1,1,1,1};

	for (int i = 0; i < cellsX; i++)
	{
		for (int j = 0; j < cellsY; j++)
		{
			float u0 = i/(float)(xDivs-1)*percentX;
			float u1 = (i+1)/(float)(xDivs-1)*percentX;
			float v0 = (j)/(float)(yDivs-1)*percentY;
			float v1 = (j+1)/(float)(yDivs-1)*percentY;

			glm::vec3 p00 = xf.transformPoint(offx + vw*drawGrid[i][j].x,   offy + vh*drawGrid[i][j].y);
			glm::vec3 p01 = xf.transformPoint(offx + vw*drawGrid[i][j+1].x, offy + vh*drawGrid[i][j+1].y);
			glm::vec3 p11 = xf.transformPoint(offx + vw*drawGrid[i+1][j+1].x, offy + vh*drawGrid[i+1][j+1].y);
			glm::vec3 p10 = xf.transformPoint(offx + vw*drawGrid[i+1][j].x, offy + vh*drawGrid[i+1][j].y);

			int base = (int)verts.size();
			SDL_Vertex v;
			v.color = white;

			v.position = {p00.x, p00.y}; v.tex_coord = {u0, v0}; verts.push_back(v);
			v.position = {p01.x, p01.y}; v.tex_coord = {u0, v1}; verts.push_back(v);
			v.position = {p11.x, p11.y}; v.tex_coord = {u1, v1}; verts.push_back(v);
			v.position = {p10.x, p10.y}; v.tex_coord = {u1, v0}; verts.push_back(v);

			indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
			indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
		}
	}

	DrawBatch::flush(); // Step 6: not routed through DrawBatch - must flush first to preserve draw order
	SDL_RenderGeometry(renderer, tex, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
	PerfLog::countDrawCall();
	RenderObject::lastTextureApplied = 0;
}


void AfterEffectManager::unloadDevice()
{
	backupBuffer.unloadDevice();
}

void AfterEffectManager::reloadDevice()
{
	screenWidth = core->getWindowWidth();
	screenHeight = core->getWindowHeight();

	if (core->frameBuffer.isInited())
	{
		textureWidth = core->frameBuffer.getWidth();
		textureHeight = core->frameBuffer.getHeight();
	}
	else
	{
		textureWidth = screenWidth;
		sizePowerOf2Texture(textureWidth);
		textureHeight = screenHeight;
		sizePowerOf2Texture(textureHeight);
	}

	if(backupBuffer.isInited())
		backupBuffer.reloadDevice();
	else
		backupBuffer.init(-1, -1, true);
}

void AfterEffectManager::addEffect(Effect *e)
{

	if (!openSpots.empty())
	{
		int i = openSpots.front();
		openSpots.pop();
		effects[i] = e;
	}
	else
	{
		effects.push_back(e);
	}
	numEffects++;
	//float lowest = 9999;
	Vector base(0,0,0);
	//Vector *newPos = &base;
	//Vector *v;
	e->position.x /= screenWidth;
	//e->position.x *= xDivs;
	e->position.y /= screenHeight;
	//e->position.y *= yDivs;

	/*
	for (int x = 1; x < xDivs-1; x++)
	{
		for (int y = 1; y < yDivs-1; y++)
		{
			v = &drawGrid[x][y];
			float dist = (v->x - e->position.x)*(v->x - e->position.x)+(v->y - e->position.y)*(v->y - e->position.y);
			if (dist < lowest)
			{
				lowest = dist;
				newPos = &drawGrid[x][y];
			}
		}
	}
	e->position = Vector(newPos->x, newPos->y, newPos->z);
	*/

}


void ShockEffect::update(float dt, Vector ** drawGrid, int xDivs, int yDivs)
{
	dt *= timeMultiplier;
	Effect::update(dt, drawGrid, xDivs, yDivs);
	//GLdouble sx, sy,sz;
	/*
	gluProject(position.x,position.y,position.z,
		nCameraPointer->modelMatrix,nCameraPointer->projMatrix,nCameraPointer->viewport,
		&sx,&sy,&sz); // Find out where the light is on the screen.
	centerPoint.Set(sx/(float)nCameraPointer->viewport[2],1-sy/(float)nCameraPointer->viewport[3],sz);

  */
	centerPoint = position;
	centerPoint -= ((core->screenCenter-originalCenter)*core->globalScale.x)/core->width;
	//centerPoint = position/xDivs;
	//centerPoint = drawGrid[xDivs/2][yDivs/2];
	float xDist,yDist,tDist;


	amplitude-=dt*rate;
	currentDistance+=dt*frequency;


	//float distFromCamp =(core->cameraPos - position).getLength2D();//v3dDist(nCameraPointer->pos, position);
	//if (distFromCamp < 4)
	float	distFromCamp = 4;

	float adjWaveLength = waveLength/distFromCamp;
	float adjAmplitude = amplitude/distFromCamp;

	if (amplitude < 0)
		done=true;

	for (int i = 1; i < (xDivs-1); i++)
	{
		for (int j = 1; j < (yDivs-1); j++)
		{
			/*
			Vector p = getNearestPointOnLine(centerPoint, centerPoint + Vector(-200, -200), Vector(drawGrid[i][j].x*core->width, drawGrid[i][j].y*core->height));
			
			p.x /= core->width;
			p.y /= core->height;
			*/
			
			xDist = (centerPoint.x - drawGrid[i][j].x)/.75;
			yDist = centerPoint.y - drawGrid[i][j].y;
			
			/*
			xDist = (p.x - drawGrid[i][j].x)/.75;
			yDist = p.y - drawGrid[i][j].y;
			*/

			//xDist = 1;
			//yDist = 2;
			tDist = sqrtf(xDist*xDist+yDist*yDist);

			//drawGrid[i][j].x += (rand()%100)/10000.0f;
			//drawGrid[i][j].y += (rand()%100)/10000.0f;


			if (tDist < currentDistance*adjWaveLength)
			{
				//drawGrid[i][j].x += rand()%50;
				//drawGrid[i][j].y += rand()%50;
				drawGrid[i][j].x += adjAmplitude*sinf(-tDist/adjWaveLength+currentDistance)*.75f;
				drawGrid[i][j].y += adjAmplitude*cosf(-tDist/adjWaveLength+currentDistance);
			}
		}
	}
}


RippleEffect::RippleEffect() : Effect()
{
	time = 0;
}

void RippleEffect::update(float dt, Vector ** drawGrid, int xDivs, int yDivs)
{
	/*
	// whole screen roll
	time += dt;
	float amp = 0.01;
	for (int i = 0; i < (xDivs-1); i++)
	{
		for (int j = 0; j < (yDivs-1); j++)
		{
			float offset = +i/float(xDivs) +j/float(xDivs);
			//drawGrid[i][j].x += sinf(time+offset)*amp;
			drawGrid[i][j].y += cosf((time+offset)*2.5f)*amp;
		}
	}
	*/
	time += dt*0.5f;
	float amp = 0.002;
	for (int i = 0; i < (xDivs-1); i++)
	{
		for (int j = 0; j < (yDivs-1); j++)
		{
			float offset = i/float(xDivs) + (core->screenCenter.x/float(core->width)/2) +j/float(xDivs) + (core->screenCenter.y/float(core->height)/2);
			//drawGrid[i][j].x += sinf(time+offset)*amp;
			drawGrid[i][j].x += sinf((time+offset)*7.5f)*(amp*0.5f);
			drawGrid[i][j].y += cosf((time+offset)*7.5f)*amp;
		}
	}
}
