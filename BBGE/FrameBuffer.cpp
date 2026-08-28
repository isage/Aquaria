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
#include "FrameBuffer.h"
#include "Core.h"
#include "PerfLog.h"

// Render-to-texture via SDL3's SDL_TEXTUREACCESS_TARGET textures, replacing
// the old GL_EXT_framebuffer_object implementation. Shared by AfterEffect,
// WaterSurfaceRender, and ScreenTransition (each holds its own FrameBuffer
// instance; ScreenTransition's is populated via copyFrom() rather than a
// live startCapture()/endCapture() pair - see the migration plan).

FrameBuffer::FrameBuffer()
{
	inited = false;
	enabled = false;
	w = 0;
	h = 0;
	texture = 0;
	savedTarget = 0;
	scaleMode = SDL_SCALEMODE_LINEAR;
	_w = _h = 0;
	_fitToScreen = false;
}

FrameBuffer::~FrameBuffer()
{
	unloadDevice();
}

float FrameBuffer::getWidthP()
{
	if (w <= 0) return 1.0f;
	int sw = core->getWindowWidth();
	return (float)sw / (float)w;
}

float FrameBuffer::getHeightP()
{
	if (h <= 0) return 1.0f;
	int sh = core->getWindowHeight();
	return (float)sh / (float)h;
}

bool FrameBuffer::init(int width, int height, bool fitToScreen, SDL_ScaleMode filter)
{
	_w = width;
	_h = height;
	_fitToScreen = fitToScreen;

	if (width == -1)
		width = core->width;

	if (height == -1)
		height = core->height;

	if (width <= 0 || height <= 0)
	{
		inited = false;
		enabled = false;
		return false;
	}

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer)
	{
		inited = false;
		enabled = false;
		return false;
	}

	// Drop any previous texture before making a new one (matches the old
	// behavior of unloadDevice()+recreate on reinit).
	if (texture)
	{
		SDL_DestroyTexture(texture);
		texture = 0;
	}

	w = width;
	h = height;

    scaleMode = filter;
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
	if (!texture)
	{
		debugLog(std::string("FrameBuffer::init() - SDL_CreateTexture failed: ") + SDL_GetError());
		inited = false;
		enabled = false;
		return false;
	}

	SDL_SetTextureScaleMode(texture, scaleMode);
	// So drawing this texture back into the scene with normal alpha
	// blending (the common case for every current consumer) behaves the
	// way the old GL_RGBA framebuffer texture did.
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	inited = true;
	enabled = true;
	return true;
}

void FrameBuffer::setEnabled(bool e)
{
	enabled = e;
}

void FrameBuffer::startCapture()
{
	if (!inited || !texture) return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	savedTarget = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, texture);
	PerfLog::countRenderTargetSwitch();

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
}

void FrameBuffer::endCapture()
{
	if (!inited) return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	SDL_SetRenderTarget(renderer, savedTarget);
	PerfLog::countRenderTargetSwitch();
	savedTarget = 0;
}

void FrameBuffer::copyFrom(FrameBuffer &other)
{
	if (!inited || !texture || !other.inited || !other.texture) return;

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	SDL_Texture *prevTarget = SDL_GetRenderTarget(renderer);
	SDL_SetRenderTarget(renderer, texture);
	PerfLog::countRenderTargetSwitch();

	SDL_BlendMode prevBlend = SDL_BLENDMODE_BLEND;
	SDL_GetTextureBlendMode(other.texture, &prevBlend);
	SDL_SetTextureBlendMode(other.texture, SDL_BLENDMODE_NONE); // plain copy, not blended
	SDL_RenderTexture(renderer, other.texture, NULL, NULL);
	SDL_SetTextureBlendMode(other.texture, prevBlend);

	SDL_SetRenderTarget(renderer, prevTarget);
	PerfLog::countRenderTargetSwitch();
}

void FrameBuffer::unloadDevice()
{
	if (texture)
	{
		SDL_DestroyTexture(texture);
		texture = 0;
	}
	inited = false;
}

void FrameBuffer::reloadDevice()
{
	init(_w, _h, _fitToScreen);
}
