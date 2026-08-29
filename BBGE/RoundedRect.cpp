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
#include "RoundedRect.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Core.h"
#include "TTFFont.h"

#include <assert.h>
#include <vector>

RoundedRect *RoundedRect::moving=0;

RoundedRect::RoundedRect() : RenderObject()
{
	alphaMod = 0.75;
	color = 0;

	width = 100;
	height = 100;

	radius = 20;
	
	cull = false;

	canMove = false;
	moving = 0;

	followCamera = 1;
}

void RoundedRect::setWidthHeight(int w, int h, int radius)
{
	this->radius = radius;
	width = w-radius*2;
	height = h-radius*2;
}

void RoundedRect::setCanMove(bool on)
{
	canMove = on;
}

void RoundedRect::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);
	if (canMove)
	{
		if (core->mouse.buttons.left)
		{
			if (moving == this)
			{
				position = core->mouse.position + d;

				if (position.x + ((width/2)+radius) > (core->getVirtualWidth() - core->getVirtualOffX()))
					position.x = (core->getVirtualWidth()- core->getVirtualOffX()) - ((width/2)+radius);
				if (position.y + ((height/2)+radius) > core->getVirtualHeight())
					position.y = core->getVirtualHeight() - ((height/2)+radius);
				if (position.x - ((width/2)+radius) < 0 - core->getVirtualOffX())
					position.x = -core->getVirtualOffX() + ((width/2)+radius);
				if (position.y - ((height/2)+radius) < 0 - core->getVirtualOffY())
					position.y = -core->getVirtualOffY() + ((height/2)+radius);
			}
			else if (moving == 0)
			{
				Vector p = core->mouse.position;
				if ((p.x >= (position.x - (width/2 + radius))) && (p.y >= (position.y - (height/2 + radius)))
					&& (p.x <= (position.x + (width/2 + radius)) && (p.y <= (position.y - height/2))))
				{
					d = position - core->mouse.position;
					moving = this;
				}
			}
		}
		else
		{
			if (moving == this)
			{
				// do stuff

				moving = 0;
			}
			
		}
	}
}

void RoundedRect::onRender()
{
	int w2 = width/2;
	int h2 = height/2;
	float iter = 0.1f;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	static std::vector<SDL_Vertex> verts;
	verts.clear();
	static std::vector<int> indices;
	indices.clear();
	SDL_FColor col = {effectiveColor.x, effectiveColor.y, effectiveColor.z, effectiveAlpha};

	// helper: push one quad (4 local-space corners, already in the same
	// vertex order the old glBegin(GL_QUADS) block used) as 2 triangles.
	auto pushQuad = [&](float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
	{
		glm::vec3 p0 = core->transform.transformPoint(x0, y0);
		glm::vec3 p1 = core->transform.transformPoint(x1, y1);
		glm::vec3 p2 = core->transform.transformPoint(x2, y2);
		glm::vec3 p3 = core->transform.transformPoint(x3, y3);
		int base = (int)verts.size();
		SDL_Vertex v;
		v.color = col;
		v.position = {p0.x, p0.y}; verts.push_back(v);
		v.position = {p1.x, p1.y}; verts.push_back(v);
		v.position = {p2.x, p2.y}; verts.push_back(v);
		v.position = {p3.x, p3.y}; verts.push_back(v);
		indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
		indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);
	};

	for (float angle = 0; angle < PI_HALF - iter; angle+=iter)
	{
		// top right
		{
			float x1 = sinf(angle)*radius, y1 = -cosf(angle)*radius;
			float x2 = sinf(angle+iter)*radius, y2 = -cosf(angle+iter)*radius;
			pushQuad(w2 + x1, -h2 + y1,  w2 + x2, -h2 + y2,  w2 + x2, -h2 + 0,  w2 + x1, -h2 + 0);
		}
		// top left
		{
			float x1 = -sinf(angle)*radius, y1 = -cosf(angle)*radius;
			float x2 = -sinf(angle+iter)*radius, y2 = -cosf(angle+iter)*radius;
			pushQuad(-w2 + x1, -h2 + y1,  -w2 + x2, -h2 + y2,  -w2 + x2, -h2 + 0,  -w2 + x1, -h2 + 0);
		}
		{
			float x1 = sinf(angle)*radius, y1 = cosf(angle)*radius;
			float x2 = sinf(angle+iter)*radius, y2 = cosf(angle+iter)*radius;
			pushQuad(w2 + x1, h2 + y1,  w2 + x2, h2 + y2,  w2 + x2, h2 + 0,  w2 + x1, h2 + 0);
		}
		{
			float x1 = -sinf(angle)*radius, y1 = cosf(angle)*radius;
			float x2 = -sinf(angle+iter)*radius, y2 = cosf(angle+iter)*radius;
			pushQuad(-w2 + x1, h2 + y1,  -w2 + x2, h2 + y2,  -w2 + x2, h2 + 0,  -w2 + x1, h2 + 0);
		}
	}

	//middle, top, btm
	pushQuad(-w2, -h2 - radius,  w2, -h2 - radius,  w2, h2 + radius,  -w2, h2 + radius);
	// left
	pushQuad(-w2 - radius, -h2,  -w2, -h2,  -w2, h2,  -w2 - radius, h2);
	// right
	pushQuad(w2 + radius, -h2,  w2, -h2,  w2, h2,  w2 + radius, h2);

	if (!verts.empty())
	{
		RenderState::setRenderDrawBlendMode(renderer, currentBlendMode);
		SDL_RenderGeometry(renderer, NULL, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
		PerfLog::countDrawCall();
	}
}

void RoundedRect::show()
{
	if (alpha.x == 0)
	{
		const float t = 0.1;
		alpha = 0;
		alpha.interpolateTo(1, t);
		scale = Vector(0.5, 0.5);
		scale.interpolateTo(Vector(1,1), t);
	}
}

void RoundedRect::hide()
{
	const float t = 0.1;
	alpha = 1.0;
	alpha.interpolateTo(0, t);
	scale = Vector(1, 1);
	scale.interpolateTo(Vector(0.5,0.5), t);
}



RoundButton::RoundButton(const std::string &labelText, TTFFont *font) : RenderObject()
{
	//label = new
	label = new TTFText(font);
	label->setAlign(ALIGN_CENTER);
	label->offset += Vector(0, 3);
	label->setText(labelText);
	addChild(label, PM_POINTER);
	width = 80;
	height = 20;

	mbd = false;

	noNested = true;
}

void RoundButton::setWidthHeight(int w, int h, int radius)
{
	width = w;
	height = h;
}
	
void RoundButton::onUpdate(float dt)
{
	if (noNested && core->isNested()) return;

	RenderObject::onUpdate(dt);

	RenderObject *top = getTopParent();
	if (alpha.x == 1 && top->alpha.x == 1)
	{
		Vector p = core->mouse.position;
		Vector c = getWorldPosition();
		int w2 = width/2;
		int h2 = height/2;
		if ((p.x > (c.x - w2)) && (p.x < (c.x + w2)) && (p.y > (c.y - h2)) && (p.y < (c.y + h2)))
		{
			if (core->mouse.buttons.left && !mbd)
			{
				mbd = true;
			}
			else if (!core->mouse.buttons.left && mbd)
			{
				mbd = false;

				event.call();
			}
		}
		else
		{
			mbd = false;
		}
		if (!core->mouse.buttons.left && mbd)
		{
			mbd = false;
		}
	}
	else
	{
		mbd = false;
	}
}

void RoundButton::onRender()
{
	int w2 = width/2, h2 = height/2;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	glm::vec3 ul = core->transform.transformPoint(-w2, -h2);
	glm::vec3 ur = core->transform.transformPoint( w2, -h2);
	glm::vec3 lr = core->transform.transformPoint( w2,  h2);
	glm::vec3 ll = core->transform.transformPoint(-w2,  h2);

	RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColorFloat(renderer, effectiveColor.x, effectiveColor.y, effectiveColor.z, effectiveAlpha);
	SDL_FPoint pts[5] = {{ul.x,ul.y}, {ur.x,ur.y}, {lr.x,lr.y}, {ll.x,ll.y}, {ul.x,ul.y}};
	SDL_RenderLines(renderer, pts, 5);

	if (mbd)
	{
		SDL_Vertex v[4];
		v[0].position = {ll.x, ll.y};
		v[1].position = {lr.x, lr.y};
		v[2].position = {ur.x, ur.y};
		v[3].position = {ul.x, ul.y};
		v[0].color = v[1].color = v[2].color = v[3].color = {1,1,1,0.5f};
		static const int idx[6] = {0,1,2,0,2,3};
		SDL_RenderGeometry(renderer, NULL, v, 4, idx, 6);
		PerfLog::countDrawCall();
	}
}

