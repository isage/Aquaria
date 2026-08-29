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
#include "Shot.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Game.h"
#include "Avatar.h"

#include "../BBGE/MathFunctions.h"

Beam::Beams Beam::beams;

Beam::Beam(Vector pos, float angle) : Quad()
{
	addType(SCO_BEAM);
	cull = false;
	trace();
	//rotation.z = angle;
	this->angle = angle;
	position = pos;

	setTexture("beam");
	beams.push_back(this);

	setBlendType(BLEND_ADD);
	alpha = 0;
	alpha.interpolateTo(1, 0.1);
	trace();
	damageData.damageType = DT_ENEMY_BEAM;
	damageData.damage = 0.5;

	beamWidth = 16;
}

void Beam::setBeamWidth(float w)
{
	beamWidth = w;
}

void Beam::setDamage(float dmg)
{
	damageData.damage = dmg;
}

void Beam::setFirer(Entity *e)
{
	damageData.attacker = e;
}

void Beam::onEndOfLife()
{
	beams.remove(this);
}

void Beam::killAllBeams()
{
	std::queue<Beam*>beamDeleteQueue;
	for (Beams::iterator i = beams.begin(); i != beams.end(); i++)
	{
		beamDeleteQueue.push(*i);
	}
	Beam *s = 0;
	while (!beamDeleteQueue.empty())
	{
		s = beamDeleteQueue.front();
		if (s)
		{
			s->safeKill();
		}
		beamDeleteQueue.pop();
	}
	beams.clear();
}

void Beam::trace()
{
	float angle = MathFunctions::toRadians(this->angle);
		//(float(-this->angle)/180.0f)*PI;
	//float angle = rotation.z;
	Vector mov(sinf(angle), cosf(angle));
	TileVector t(position);
	Vector startTile(t.x, t.y);

	/*
	std::ostringstream os;
	os << "rotation.z = " << rotation.z << " mov(" << mov.x << ", " << mov.y << ")";
	debugLog(os.str());
	*/

	int moves = 0;
	while (!dsq->game->isObstructed(TileVector(startTile.x, startTile.y)))
	{
		startTile += mov;
		moves++;
		if (moves > 1000)
			break;
	}
	t = TileVector(startTile.x, startTile.y);
	endPos = t.worldVector();

	/*
	offset = endPos - position;
	offset /= 2;
	offset *= -1;
	*/


	//width = (endPos - position).getLength2D();
}

void Beam::render()
{

	/*
	glLineWidth(4);
	glColor4f(1,1,1,1);
	glBegin(GL_LINES);
		glVertex2f(position.x, position.y);
		glVertex2f(endPos.x, endPos.y);
	glEnd();
	*/

	Quad::render();
}

void Beam::onRender()
{
	Vector diff = endPos - position;
	Vector side = diff;
	side.setLength2D(beamWidth*2);
	Vector sideLeft = side.getPerpendicularLeft();
	Vector sideRight = side.getPerpendicularRight();

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;
	SDL_Texture *tex = texture ? texture->sdlTexture : 0;

	glm::vec3 p0 = core->transform.transformPoint(sideLeft.x, sideLeft.y);
	glm::vec3 p1 = core->transform.transformPoint(sideLeft.x+diff.x, sideLeft.y+diff.y);
	glm::vec3 p2 = core->transform.transformPoint(sideRight.x+diff.x, sideRight.y+diff.y);
	glm::vec3 p3 = core->transform.transformPoint(sideRight.x, sideRight.y);

	SDL_FColor col = {effectiveColor.x, effectiveColor.y, effectiveColor.z, effectiveAlpha};
	SDL_Vertex v[4];
	v[0].position={p0.x,p0.y}; v[0].tex_coord={0,0}; v[0].color=col;
	v[1].position={p1.x,p1.y}; v[1].tex_coord={1,0}; v[1].color=col;
	v[2].position={p2.x,p2.y}; v[2].tex_coord={1,1}; v[2].color=col;
	v[3].position={p3.x,p3.y}; v[3].tex_coord={0,1}; v[3].color=col;
	static const int idx[6] = {0,1,2,0,2,3};

	if (tex) RenderState::setTextureBlendMode(tex, currentBlendMode);
	else RenderState::setRenderDrawBlendMode(renderer, currentBlendMode);
	SDL_RenderGeometry(renderer, tex, v, 4, idx, 6);
	PerfLog::countDrawCall();
}

void Beam::onUpdate(float dt)
{
	if (dsq->game->isPaused()) return;

	Quad::onUpdate(dt);

	if (alpha.x > 0.5f)
	{
		FOR_ENTITIES(i)
		{
			Entity *e = *i;
			if (e != damageData.attacker && e->isDamageTarget(damageData.damageType))
			{
				if (isTouchingLine(position, endPos, e->position, beamWidth + e->collideRadius))
				{
					e->damage(damageData);
				}
			}
		}
	}
}

