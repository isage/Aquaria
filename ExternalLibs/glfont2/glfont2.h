//*******************************************************************
//glfont2.h -- Header for glfont2.cpp
//Copyright (c) 1998-2002 Brad Fish
//See glfont.html for terms of use
//May 14, 2002
//*******************************************************************

#ifndef GLFONT2_H
#define GLFONT2_H

#include <assert.h>
#include <vector>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include "Core.h"
#include "RenderState.h"
#include "DrawBatch.h"

//*******************************************************************
//GLFont Interface
//*******************************************************************

//glFont namespace
namespace glfont
{
	class GLFont;	
}

//glFont class
class glfont::GLFont
{
private:

	//glFont character structure
	typedef struct
	{
		float dx, dy;
		float tx1, ty1;
		float tx2, ty2;
	} GLFontChar;

	//glFont header structure
	struct
	{
		unsigned int tex; // legacy GL texture id field, no longer set/used
		SDL_Texture *sdlTex;
		unsigned int tex_width, tex_height;
		unsigned int start_char, end_char;
		GLFontChar *chars;
	} header;

public:

	//Constructor
	GLFont ();

	//Destructor
	~GLFont ();

public:

	//Creates the glFont
	bool Create (const char *file_name, int tex, bool loadTexture=true);
	bool Create (const std::string &file_name, int tex, bool loadTexture=true);

	//Destroys the glFont
	void Destroy (void);

	//Texture size retrieval methods
	void GetTexSize (std::pair<int, int> *size);
	int GetTexWidth (void);
	int GetTexHeight (void);
	SDL_Texture *GetSDLTexture (void) { return header.sdlTex; }

	//Character interval retrieval methods
	void GetCharInterval (std::pair<int, int> *interval);
	int GetStartChar (void);
	int GetEndChar (void);

	//Character size retrieval methods
	void GetCharSize (unsigned char c, std::pair<int, int> *size);
	int GetCharWidth (unsigned char c);
	int GetCharHeight (unsigned char c);
	
	void GetStringSize (const std::string &text, std::pair<int, int> *size);


	//Begins text output with this font
	void Begin (void);	
	
	//Template function to output a scaled, colored std::basic_string
	template<class T> void DrawString (
		const std::basic_string<T> &text, float scalar, float x,
		float y, const float *top_color, const float *bottom_color, float alpha, float lastAlpha,
		SDL_Texture *textureOverride = 0)
	{
		unsigned int i;
		GLFontChar *glfont_char;
		float width, height;

		SDL_Renderer *renderer = core->getRenderer();
		if (!renderer) return;

		// TEMPORARY-note-turned-permanent-fix: BmpFont instances are
		// always loaded with loadTexture=false and supply the real font
		// atlas via BmpFont::overrideTexture instead (see
		// DSQ::loadFonts()) - the old GL code got this "for free" via
		// glBindTexture()'s global state (Texture::apply() bound the
		// override texture just before DrawString()'s own glBegin/
		// glTexCoord calls implicitly sampled it). SDL_RenderGeometry
		// needs the texture explicitly, so the caller
		// (BitmapText::onRender()) passes it through here - same pattern
		// as Quad::textureOverride.
		SDL_Texture *tex = textureOverride ? textureOverride : header.sdlTex;

		unsigned int sz = text.size();
		if (sz == 0) return;

		// One batched draw for the whole string (was one glBegin(GL_QUADS)
		// block per string, 4 immediate-mode vertices per character).
		std::vector<SDL_Vertex> verts;
		std::vector<int> indices;
		verts.reserve((size_t)sz * 4);
		indices.reserve((size_t)sz * 6);

		float a = 0;
		//Loop through characters
		for (i = 0; i < sz; i++)
		{
			//Make sure character is in range
			unsigned int c = (unsigned char)text[i];
			if (c < header.start_char || c > header.end_char)
				continue;

			//Get pointer to glFont character
			glfont_char = &header.chars[c - header.start_char];

			//Get width and height
			width = (glfont_char->dx * header.tex_width) * scalar;
			height = (glfont_char->dy * header.tex_height) * scalar;
			
			if (i == (sz-1))
				a = alpha*lastAlpha;
			else
				a = alpha;

			glm::vec3 p0 = core->transform.transformPoint(x, y);
			glm::vec3 p1 = core->transform.transformPoint(x + width, y);
			glm::vec3 p2 = core->transform.transformPoint(x + width, y + height);
			glm::vec3 p3 = core->transform.transformPoint(x, y + height);

			SDL_FColor topCol = {top_color[0], top_color[1], top_color[2], a};
			SDL_FColor btmCol = {bottom_color[0], bottom_color[1], bottom_color[2], a};

			int base = (int)verts.size();
			SDL_Vertex v;
			v.position = {p0.x, p0.y}; v.tex_coord = {glfont_char->tx1, glfont_char->ty1}; v.color = topCol; verts.push_back(v);
			v.position = {p1.x, p1.y}; v.tex_coord = {glfont_char->tx2, glfont_char->ty1}; v.color = topCol; verts.push_back(v);
			v.position = {p2.x, p2.y}; v.tex_coord = {glfont_char->tx2, glfont_char->ty2}; v.color = btmCol; verts.push_back(v);
			v.position = {p3.x, p3.y}; v.tex_coord = {glfont_char->tx1, glfont_char->ty2}; v.color = btmCol; verts.push_back(v);

			indices.push_back(base+0); indices.push_back(base+1); indices.push_back(base+2);
			indices.push_back(base+0); indices.push_back(base+2); indices.push_back(base+3);

			//Move to next character
			x += width;
		}

		if (!verts.empty())
		{
			if (tex)
				RenderState::setTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
			else
				RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			// Step 6 of the performance optimization plan: this call was
			// missed by the original codebase-wide sweep (it lives in
			// ExternalLibs/, which that sweep's search scope didn't
			// cover - BBGE/*.cpp and Aquaria/*.cpp patterns only).
			// Confirmed via real playtesting: without this flush, any
			// pending DrawBatch-accumulated content (e.g. a dark
			// background box behind this text, submitted earlier but not
			// yet actually drawn) would stay deferred while this text
			// draws immediately - making the text appear to jump ahead
			// of content that was supposed to be drawn first, or a
			// still-pending box appear on top of text drawn after it,
			// depending on timing. This is exactly why DrawBatch.h's
			// safety model requires every non-participating draw site to
			// flush first, without exception.
			DrawBatch::flush();
			SDL_RenderGeometry(renderer, tex, verts.data(), (int)verts.size(), indices.data(), (int)indices.size());
		}
	}
};

//*******************************************************************

#endif

//End of file


