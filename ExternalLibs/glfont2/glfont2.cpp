//*******************************************************************
//glfont2.cpp -- glFont Version 2.0 implementation
//Copyright (c) 1998-2002 Brad Fish
//See glfont.html for terms of use
//May 14, 2002
//*******************************************************************

//STL headers
#include <string>
#include "ttvfs_stdio.h"
#include "ByteBuffer.h"
using namespace std;

//glFont header
#include "glfont2.h"
using namespace glfont;

//*******************************************************************
//GLFont Class Implementation
//*******************************************************************
GLFont::GLFont ()
{
	//Initialize header to safe state
	header.tex = -1;
	header.sdlTex = NULL;
	header.tex_width = 0;
	header.tex_height = 0;
	header.start_char = 0;
	header.end_char = 0;
	header.chars = NULL;
}
//*******************************************************************
GLFont::~GLFont ()
{
	//Destroy the font
	Destroy();
}
//*******************************************************************
bool GLFont::Create (const char *file_name, int tex, bool loadTexture)
{
	ByteBuffer::uint32 num_chars, num_tex_bytes;
	char *tex_bytes;

	//Destroy the old font if there was one, just to be safe
	Destroy();

	
	VFILE *fh = vfopen(file_name, "rb");
	if (!fh)
		return false;

	size_t sz = 0;
	if(vfsize(fh, &sz) < 0)
	{
		vfclose(fh);
		return false;
	}

	ByteBuffer bb(sz);
	bb.resize(sz);
	vfread(bb.contents(), 1, sz, fh);
	vfclose(fh);

	// Read the header from file
	header.tex = tex;
	bb.skipRead(4); // skip tex field
	header.tex_width = bb.read<ByteBuffer::uint32>();
	header.tex_height = bb.read<ByteBuffer::uint32>();
	header.start_char = bb.read<ByteBuffer::uint32>();
	header.end_char = bb.read<ByteBuffer::uint32>();
	bb.skipRead(4); // skip chars field

	//Allocate space for character array
	num_chars = header.end_char - header.start_char + 1;
	if ((header.chars = new GLFontChar[num_chars]) == NULL)
		return false;

	//Read character array
	for (unsigned int i = 0; i < num_chars; i++)
	{
		bb >> header.chars[i].dx;
		bb >> header.chars[i].dy;
		bb >> header.chars[i].tx1;
		bb >> header.chars[i].ty1;
		bb >> header.chars[i].tx2;
		bb >> header.chars[i].ty2;
	}

	//Read texture pixel data
	num_tex_bytes = header.tex_width * header.tex_height * 2;
	tex_bytes = new char[num_tex_bytes];
	// HACK: Aquaria uses override textures, so we can live with the truncation.
	bb.read(tex_bytes, std::min(num_tex_bytes, bb.readable()));

	if (loadTexture)
	{
		// The embedded pixel data is 2-channel luminance-alpha, not a
		// standard image container SDL3_image can decode - expand it to
		// RGBA (R=G=B=luminance) and build an SDL_Texture directly from
		// the raw pixels, same technique as ImageLoader.cpp uses for
		// decoded images.
		unsigned int numPixels = header.tex_width * header.tex_height;
		unsigned char *rgba = new unsigned char[(size_t)numPixels * 4];
		const unsigned char *la = (const unsigned char*)tex_bytes;
		for (unsigned int i = 0; i < numPixels; i++)
		{
			unsigned char lum = la[i*2 + 0];
			unsigned char alpha = la[i*2 + 1];
			rgba[i*4 + 0] = lum;
			rgba[i*4 + 1] = lum;
			rgba[i*4 + 2] = lum;
			rgba[i*4 + 3] = alpha;
		}

		SDL_Renderer *renderer = core ? core->getRenderer() : 0;
		if (renderer)
		{
			SDL_Surface *surf = SDL_CreateSurfaceFrom((int)header.tex_width, (int)header.tex_height,
				SDL_PIXELFORMAT_RGBA32, rgba, (int)header.tex_width * 4);
			if (surf)
			{
				header.sdlTex = SDL_CreateTextureFromSurface(renderer, surf);
				SDL_DestroySurface(surf);
				if (header.sdlTex)
				{
					SDL_SetTextureScaleMode(header.sdlTex, SDL_SCALEMODE_LINEAR);
					SDL_SetTextureBlendMode(header.sdlTex, SDL_BLENDMODE_BLEND);
				}
			}
		}

		delete [] rgba;
	}

	//Free texture pixels memory
	delete[] tex_bytes;

	//Return successfully
	return true;
}
//*******************************************************************
bool GLFont::Create (const std::string &file_name, int tex, bool loadTexture)
{
	return Create(file_name.c_str(), tex);
}
//*******************************************************************
void GLFont::Destroy (void)
{
	//Delete the character array if necessary
	if (header.chars)
	{
		delete[] header.chars;
		header.chars = NULL;
	}
	if (header.sdlTex)
	{
		SDL_DestroyTexture(header.sdlTex);
		header.sdlTex = NULL;
	}
}
//*******************************************************************
void GLFont::GetTexSize (std::pair<int, int> *size)
{
	//Retrieve texture size
	size->first = header.tex_width;
	size->second = header.tex_height;
}
//*******************************************************************
int GLFont::GetTexWidth (void)
{
	//Return texture width
	return header.tex_width;
}
//*******************************************************************
int GLFont::GetTexHeight (void)
{
	//Return texture height
	return header.tex_height;
}
//*******************************************************************
void GLFont::GetCharInterval (std::pair<int, int> *interval)
{
	//Retrieve character interval
	interval->first = header.start_char;
	interval->second = header.end_char;
}
//*******************************************************************
int GLFont::GetStartChar (void)
{
	//Return start character
	return header.start_char;
}
//*******************************************************************
int GLFont::GetEndChar (void)
{
	//Return end character
	return header.end_char;
}
//*******************************************************************
void GLFont::GetCharSize (unsigned char c, std::pair<int, int> *size)
{
	//Make sure character is in range
	if (c < header.start_char || c > header.end_char)
	{
		//Not a valid character, so it obviously has no size
		size->first = 0;
		size->second = 0;
	}
	else
	{
		GLFontChar *glfont_char;

		//Retrieve character size
		glfont_char = &header.chars[c - header.start_char];
		size->first = (int)(glfont_char->dx * header.tex_width);
		size->second = (int)(glfont_char->dy *
			header.tex_height);
	}
}
//*******************************************************************
int GLFont::GetCharWidth (unsigned char c)
{
	//Make sure in range
	if (c < header.start_char || c > header.end_char)
		return 0;
	else
	{
		GLFontChar *glfont_char;
		
		//Retrieve character width
		glfont_char = &header.chars[c - header.start_char];

		// hack to fix empty spaces
		if (c == ' ' && glfont_char->dx <= 0)
		{
			GLFontChar *glfont_a = &header.chars['a' - header.start_char];
			glfont_char->dx = glfont_a->dx*0.75f;
			glfont_char->dy = glfont_a->dy;
		}

		return (int)(glfont_char->dx * header.tex_width);
	}
}
//*******************************************************************
int GLFont::GetCharHeight (unsigned char c)
{
	//Make sure in range
	if (c < header.start_char || c > header.end_char)
		return 0;
	else
	{
		GLFontChar *glfont_char;

		//Retrieve character height
		glfont_char = &header.chars[c - header.start_char];
		return (int)(glfont_char->dy * header.tex_height);
	}
}
//*******************************************************************
void GLFont::Begin (void)
{
}

//*******************************************************************
void GLFont::GetStringSize (const std::string &text, std::pair<int, int> *size)
{
	unsigned int i;
	unsigned int c;
	GLFontChar *glfont_char;
	float width;
	
	//debugLog("size->second");
	//Height is the same for now...might change in future
	size->second = (int)(header.chars[header.start_char].dy *
		header.tex_height);

	//Calculate width of string
	width = 0.0F;
	for (i = 0; i < text.size(); i++)
	{
		//Make sure character is in range
		c = (unsigned char)text[i];
		
		if (c < header.start_char || c > header.end_char)
			continue;

		//Get pointer to glFont character
		glfont_char = &header.chars[c - header.start_char];

		//Get width and height
		width += glfont_char->dx * header.tex_width;		
	}

	//Save width
	//debugLog("size first");
	size->first = (int)width;
	
	//debugLog("done");
}

//End of file


