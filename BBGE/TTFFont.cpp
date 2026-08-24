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
#include "TTFFont.h"
#include "Core.h"

// One text engine shared by every TTFFont, since it's scoped to the
// SDL_Renderer (of which this engine only ever has one), not to any
// individual font. Created lazily on first use rather than at a fixed
// engine-init point, so this file doesn't need to know engine startup
// order relative to renderer creation. TTF_Init() must have already been
// called (see Core::initGraphicsLibrary()) before this is used.
static TTF_TextEngine *g_ttfEngine = 0;

static TTF_TextEngine *getTTFEngine()
{
	if (!g_ttfEngine)
	{
		SDL_Renderer *renderer = core->getRenderer();
		if (renderer)
			g_ttfEngine = TTF_CreateRendererTextEngine(renderer);
	}
	return g_ttfEngine;
}

// Called from Core::shutdownGraphicsLibrary() before the renderer is
// destroyed, since the text engine holds a reference to it.
void ttfShutdown()
{
	if (g_ttfEngine)
	{
		TTF_DestroyRendererTextEngine(g_ttfEngine);
		g_ttfEngine = 0;
	}
	TTF_Quit();
}

TTFFont::TTFFont()
{
	font = 0;
}

TTFFont::~TTFFont()
{
	destroy();
}

void TTFFont::destroy()
{
	if (font)
	{
		TTF_CloseFont(font);
		font = 0;
	}
}

void TTFFont::load(const std::string &str, int sz)
{
	destroy();

	// Read via the engine's normal (VFS-aware) file reader rather than
	// TTF_OpenFont()'s raw fopen, so fonts loaded from packed/override
	// data behave the same as every other asset in this codebase.
	unsigned long size = 0;
	char *buf = readFile(str, &size);
	if (!buf || !size)
	{
		errorLog("TTFFont::load() - could not read " + str);
		return;
	}

	SDL_IOStream *io = SDL_IOFromConstMem(buf, size);
	if (io)
		font = TTF_OpenFontIO(io, true, (float)sz);

	delete [] buf;

	if (!font)
		errorLog("TTFFont::load() - TTF_OpenFontIO failed for " + str + ": " + SDL_GetError());
}

void TTFFont::create(const unsigned char *data, unsigned long datalen, int sz)
{
	destroy();

	SDL_IOStream *io = SDL_IOFromConstMem(data, datalen);
	if (io)
		font = TTF_OpenFontIO(io, false, (float)sz);

	if (!font)
		errorLog(std::string("TTFFont::create() - TTF_OpenFontIO failed: ") + SDL_GetError());
}

// Replaces the old FTGLTextureFont::BBox()-based width/height measurement.
static void measureString(TTF_Font *font, const std::string &s, float *outW, float *outH)
{
	int w = 0, h = 0;
	if (font && !s.empty())
		TTF_GetStringSize(font, s.c_str(), s.length(), &w, &h);
	if (outW) *outW = (float)w;
	if (outH) *outH = (float)h;
}

TTFText::TTFText(TTFFont *f) : font(f)
{
	align = ALIGN_LEFT;
	hw = 0;
	h = 0;
	width = 0;
	shadow = false;
	maxW = 0;
}

void TTFText::setText(const std::string &txt)
{
	originalText = txt;
	updateAlign();
	updateFormatting();
}

void TTFText::setAlign(Align align)
{
	this->align = align;
	updateAlign();
	updateFormatting();
}

void TTFText::updateAlign()
{
	if (align == ALIGN_CENTER)
	{
		float w = 0, hh = 0;
		measureString(font->font, originalText, &w, &hh);
		hw = w/2;
		h = (int)hh;
	}
	else
	{
		hw = 0;
	}
}

int TTFText::getNumLines()
{
	return (int)text.size();
}

float TTFText::getHeight()
{
	return text.size()*lineHeight;
}

float TTFText::getStringWidth(const std::string& s)
{
	float w = 0;
	std::string cp = s;
	const char *start = cp.c_str();
	size_t begin = 0;
	for(size_t i = 0; i < cp.length(); ++i)
	{
		const char c = cp[i];
		if(c == '\n')
		{
			cp[i] = 0;
			std::string part(start + begin);
			float pw = 0;
			measureString(font->font, part, &pw, 0);
			w = std::max(w, pw);
			cp[i] = c;
			begin = i + 1;
		}
	}
	if(begin < cp.length())
	{
		std::string part(start + begin);
		float pw = 0;
		measureString(font->font, part, &pw, 0);
		w = std::max(w, pw);
	}
	return w;
}

void TTFText::setWidth(float width)
{
	this->width = width;

	updateAlign();
	updateFormatting();
}

void TTFText::setFontSize(float)
{
}

void TTFText::updateFormatting()
{
	int start = 0, lastSpace = -1;
	text.clear();
	int i=0;
	int sz = originalText.size();
	maxW = 0;
	for (i = 0; i < sz; i++)
	{
		if (originalText[i] == '\n')
		{
			std::string part = originalText.substr(start, i-start);
			text.push_back(part);
			start = i+1;
			float w = 0;
			measureString(font->font, part, &w, 0);
			maxW = std::max(maxW, w);
		}
		else
		{
			if (originalText[i] == ' ')
			{
				lastSpace = i;
			}
			std::string part = originalText.substr(start, i-start);
			float w = 0;
			measureString(font->font, part, &w, 0);

			if (width != 0 && w >= width)
			{
				if (lastSpace != -1)
				{
					part = originalText.substr(start, lastSpace-start);
					i = lastSpace+1;
					lastSpace = -1;
					start = i;
				}
				else
					part = originalText.substr(start, i-start);

				text.push_back(part);
				// recalc width of remaining text after linebreak
				measureString(font->font, part, &w, 0);
			}

			maxW = std::max(maxW, w);
		}
	}
	if (i == sz)
	{
		std::string part = originalText.substr(start, i-start);
		text.push_back(part);
		float w = 0;
		measureString(font->font, part, &w, 0);
		maxW = std::max(maxW, w);
	}
	lineHeight = font->font ? (float)TTF_GetFontLineSkip(font->font) : 0.0f;
}

void TTFText::onUpdate(float dt)
{
	RenderObject::onUpdate(dt);
}

float TTFText::getLineHeight()
{
	return lineHeight;
}

int TTFText::findLine(const std::string &label)
{
	for (int i = 0; i < text.size(); i++)
	{
		if (text[i].find(label) != std::string::npos)
		{
			return i;
		}
	}
	return 0;
}

// Draws one line via the SDL3_ttf renderer text engine at a given local
// (x,y) offset, transformed through core->transform.
//
// TODO: TTF_DrawRendererText() has no way to accept rotation or scale.
static void drawLine(TTFFont *ttfFont, const std::string &line, float x, float y,
	float r, float g, float b, float a)
{
	if (line.empty() || !ttfFont->font) return;

	TTF_TextEngine *engine = getTTFEngine();
	if (!engine) return;

	TTF_Text *text = TTF_CreateText(engine, ttfFont->font, line.c_str(), line.length());
	if (!text) return;

	TTF_SetTextColorFloat(text, r, g, b, a);

	glm::vec4 wp = core->transform.transformPoint(x, y);
	TTF_DrawRendererText(text, wp.x, wp.y);

	TTF_DestroyText(text);
}

void TTFText::onRender()
{
	for (int i = 0; i < text.size(); i++)
	{
		if (shadow)
		{
			drawLine(font, text[i], 1-hw, -1 + (i*-lineHeight),
				0, 0, 0, 0.75f*alpha.x*alphaMod);
		}

		drawLine(font, text[i], -hw, 0 + (i*-lineHeight),
			color.x, color.y, color.z, alpha.x*alphaMod);
	}

	RenderObject::lastTextureApplied = 0;
}
