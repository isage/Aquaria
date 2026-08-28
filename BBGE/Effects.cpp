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
#include <assert.h>
#include "RenderState.h"
#include "PerfLog.h"
#include <vector>

#include "Effects.h"
#include "Core.h"

PostProcessingFX::PostProcessingFX()
{
	blendType = 0;
	layer = renderLayer = 0;
	intensity = 0.1;
	blurTimes = 12;
	radialBlurColor = Vector(1,1,1);
	for (int i = 0; i < FXT_MAX; i++)
		enabled[i] = false;
}

void PostProcessingFX::init()
{
}


void PostProcessingFX::update(float dt)
{
}

void PostProcessingFX::preRender()
{
}

void PostProcessingFX::toggle(FXTypes type)
{
	enabled[int(type)] = !enabled[int(type)];
}

void PostProcessingFX::enable(FXTypes type)
{
	enabled[int(type)] = true;
}

bool PostProcessingFX::isEnabled(FXTypes type)
{
	return enabled[int(type)];
}

void PostProcessingFX::disable(FXTypes type)
{
	enabled[int(type)] = false;
}

void PostProcessingFX::render()
{
	if(!core->frameBuffer.isEnabled())
		return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	for (int i = 0; i < FXT_MAX; i++)
	{
		if (enabled[i])
		{
			// NOTE: drawn with an isolated identity transform (matching the
			// old glPushMatrix()/glLoadIdentity()/glPopMatrix() pair) - this
			// is a screen-space overlay effect, not tied to any object's
			// world transform.
			RenderTransformStack xf;

			FXTypes type = (FXTypes)i;
			switch(type)
			{
			case FXT_RADIALBLUR:
			{
				float windowW = core->getWindowWidth();
				float windowH = core->getWindowHeight();
				float textureW = core->frameBuffer.getWidth();
				float textureH = core->frameBuffer.getHeight();

				float alpha = intensity;

				float offX   = -(core->getVirtualOffX() * windowW / core->getVirtualWidth());
				float offY   = -(core->getVirtualOffY() * windowH / core->getVirtualHeight());

				float width2 = windowW / 2;
				float height2 = windowH / 2;

				float pw = float(windowW)/float(textureW);
				float ph = float(windowH)/float(textureH);

				xf.translate(width2 + offX, height2 + offY, 0);

				SDL_Texture *tex = core->frameBuffer.getTexture();

				SDL_BlendMode blend = (blendType == 1) ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND;
				if (tex) RenderState::setTextureBlendMode(tex, blend);
				else RenderState::setRenderDrawBlendMode(renderer, blend);

				float percentX = pw, percentY = ph;

				float inc = 0.01;
				float spost = 0.0f;										// Starting Texture Coordinate Offset
				float alphadec = alpha / blurTimes;

				static std::vector<SDL_Vertex> verts;
				verts.clear();
				static std::vector<int> indices;
				indices.clear();
				verts.reserve(blurTimes*4);
				indices.reserve(blurTimes*6);

				for (int num = 0;num < blurTimes; num++)					// Number Of Times To Render Blur
				{
					SDL_FColor col = {radialBlurColor.x, radialBlurColor.y, radialBlurColor.z, alpha};

					glm::vec4 p0 = xf.transformPoint(-width2, height2);
					glm::vec4 p1 = xf.transformPoint( width2, height2);
					glm::vec4 p2 = xf.transformPoint( width2, -height2);
					glm::vec4 p3 = xf.transformPoint(-width2, -height2);

					int base = (int)verts.size();
					SDL_Vertex v;
					v.color = col;
					v.position={p0.x,p0.y}; v.tex_coord={(float)spost,(float)spost}; verts.push_back(v);
					v.position={p1.x,p1.y}; v.tex_coord={(float)(percentX-spost),(float)spost}; verts.push_back(v);
					v.position={p2.x,p2.y}; v.tex_coord={(float)(percentX-spost),(float)(percentY-spost)}; verts.push_back(v);
					v.position={p3.x,p3.y}; v.tex_coord={(float)spost,(float)(percentY-spost)}; verts.push_back(v);
					indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
					indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);

					spost += inc;											// Gradually Increase spost (Zooming Closer To Texture Center)
					alpha -= alphadec;										// Gradually Decrease alpha (Gradually Fading Image Out)
				}

				if (!verts.empty())
					SDL_RenderGeometry(renderer, tex, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
					PerfLog::countDrawCall();
				RenderObject::lastTextureApplied = 0;
			}
			break;
			}
		}
	}
}

