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
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Base.h"


class FrameBuffer
{
public:
	FrameBuffer();	
	~FrameBuffer();
	bool init(int width, int height, bool fitToScreen=false, SDL_ScaleMode filter=SDL_SCALEMODE_LINEAR);
	bool isInited() { return inited; }
	bool isEnabled() { return enabled; }
	void setEnabled(bool e);
	// Set self as the active render target (replaces binding the GL FBO);
	// saves whatever was previously the active target so endCapture() can
	// restore it.
	void startCapture();
	void endCapture();

	SDL_Texture *getTexture() { return texture; }

	// Snapshot another FrameBuffer's current contents into this one (a
	// same-renderer texture-to-texture blit). Used by ScreenTransition,
	// which needs a frozen copy of a specific moment rather than a live
	// continuously-updated capture.
	void copyFrom(FrameBuffer &other);

	int getWidth() { return w; }
	int getHeight() { return h; }
	float getWidthP();
	float getHeightP();
	
	void unloadDevice();
	void reloadDevice();

protected:
	int _w, _h;
	bool _fitToScreen;
	SDL_Texture *texture;
	SDL_Texture *savedTarget; // previous render target, saved by startCapture()
	SDL_ScaleMode scaleMode;
	int w,h;
	bool enabled, inited;
};

#endif
