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
#include "Texture.h"
#include "Core.h"
#include "ImageLoader.h"
#include "ByteBuffer.h"

#include <assert.h>

#if defined(BBGE_BUILD_UNIX)
#include <stdint.h>
#endif

//#include "pngLoad.h"
//#include "jpeg/jpeglib.h"
/*
#include <il/il.h>
#include <il/ilu.h>
#include <il/ilut.h>
*/
#ifdef Z2D_J2K
//..\j2k-codec\j2k-codec.lib
	#include "..\j2k-codec\j2k-codec.h"
#endif

	GLint Texture::filter = GL_LINEAR;

	GLint Texture::format = 0;
bool Texture::useMipMaps = true;


Texture::Texture()
{
	textures[0] = 0;
	width = height = 0;

	repeat = false;
	repeating = false;
	ow = oh = -1;
}

Texture::~Texture()
{
	destroy();
}

void Texture::read(int tx, int ty, int w, int h, unsigned char *pixels)
{
	if (tx == 0 && ty == 0 && w == this->width && h == this->height)
	{
		glBindTexture(GL_TEXTURE_2D, textures[0]);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
	{
		std::ostringstream os;
		os << "Unable to read a texture subimage (size = "
		   << this->width << "x" << this->height << ", requested = "
		   << tx << "," << ty << "+" << w << "x" << h << ")";
		debugLog(os.str());
	}
}

void Texture::write(int tx, int ty, int w, int h, const unsigned char *pixels)
{
	glBindTexture(GL_TEXTURE_2D, textures[0]);

	glTexSubImage2D(GL_TEXTURE_2D, 0,
					tx,
					ty,
					w,
					h,
					GL_RGBA,
					GL_UNSIGNED_BYTE,
					pixels
					);

	glBindTexture(GL_TEXTURE_2D, 0);
	/*
	  target   Specifies the target	texture.  Must be
		   GL_TEXTURE_2D.

	  level	   Specifies the level-of-detail number.  Level	0 is
		   the base image level.  Level	n is the nth mipmap
		   reduction image.

	  xoffset  Specifies a texel offset in the x direction within
		   the texture array.

	  yoffset  Specifies a texel offset in the y direction within
		   the texture array.

	  width	   Specifies the width of the texture subimage.

	  height   Specifies the height	of the texture subimage.

	  format   Specifies the format	of the pixel data.  The
		   following symbolic values are accepted:
		   GL_COLOR_INDEX, GL_RED, GL_GREEN, GL_BLUE,
		   GL_ALPHA, GL_RGB, GL_RGBA, GL_LUMINANCE, and
		   GL_LUMINANCE_ALPHA.

	  type	   Specifies the data type of the pixel	data.  The
		   following symbolic values are accepted:
		   GL_UNSIGNED_BYTE, GL_BYTE, GL_BITMAP,
		   GL_UNSIGNED_SHORT, GL_SHORT,	GL_UNSIGNED_INT,
		   GL_INT, and GL_FLOAT.

	  pixels   Specifies a pointer to the image data in memory.
	  */
}

void Texture::unload()
{
	if (textures[0])
	{
		ow = width;
		oh = height;

		if (core->debugLogTextures)
		{
			debugLog("UNLOADING TEXTURE: " + name);
		}


		glDeleteTextures(1, &textures[0]);
		textures[0] = 0;
	}
}

void Texture::destroy()
{
	unload();

	core->removeTexture(this);
}

int Texture::getPixelWidth()
{
	int w = 0, h = 0;
	unsigned int size = 0;
	unsigned char *data = getBufferAndSize(&w, &h, &size);
	if (!data)
		return 0;

	int smallestx = -1, largestx = -1;
	for (unsigned int x = 0; x < unsigned(w); x++)
	{
		for (unsigned int y = 0; y < unsigned(h); y++)
		{
			unsigned int p = (y*unsigned(w)*4) + (x*4) + 3;
			if (p < size && data[p] >= 254)
			{
				if (smallestx == -1 || x < smallestx)
					smallestx = x;
				if (largestx == -1 || x > largestx)
					largestx = x;
			}
		}
	}
	free(data);
	return largestx - smallestx;
}

int Texture::getPixelHeight()
{
	int w = 0, h = 0;
	unsigned int size = 0;
	unsigned char *data = getBufferAndSize(&w, &h, &size);
	if (!data)
		return 0;

	int smallesty = -1, largesty = -1;
	for (unsigned int x = 0; x < unsigned(w); x++)
	{
		for (unsigned int y = 0; y < unsigned(h); y++)
		{
			int p = (y*unsigned(w)*4) + (x*4) + 3;
			if (p < size && data[p] >= 254)
			{
				if (smallesty == -1 || y < smallesty)
					smallesty = y;
				if (largesty == -1 || y > largesty)
					largesty = y;
			}
		}
	}
	free(data);
	return largesty - smallesty;
}

void Texture::reload()
{
	debugLog("RELOADING TEXTURE: " + name + " with loadName " + loadName + "...");

	unload();
	load(loadName);

	/*if (ow != -1 && oh != -1)
	{
		width = ow;
		height = oh;
	}*/
	debugLog("DONE");
}

bool Texture::load(std::string file)
{
	if (file.size()<4)
	{
		errorLog("Texture Name is Empty or Too Short");
		return false;
	}

	stringToLowerUserData(file);
	file = core->adjustFilenameCase(file);

	loadName = file;
	repeating = false;

	size_t pos = file.find_last_of('.');

	if ((pos != std::string::npos) && (pos >= 0))
	{
		// make sure this didn't catch the '.' in /home/username/.Aquaria/*  --ryan.
		const std::string userdata = core->getUserDataFolder();
		const size_t len = userdata.length();
		if (pos < len)
			pos = std::string::npos;
	}

	/*if (core->debugLogTextures)
	{
		std::ostringstream os;
		os << "pos [" << pos << "], file :" << file;
		debugLog(os.str());
	}*/

	bool found = exists(file);

	if(!found && exists(file + ".png"))
	{
		found = true;
		file += ".png";
	}

	// .tga/.zga are never used as game graphics anywhere except save slot thumbnails.
	// if so, their file names are passed exact, not with a missing extension

	if (found)
	{
		file = localisePathInternalModpath(file);
		file = core->adjustFilenameCase(file);

		/*
		std::ostringstream os;
		os << "Loading texture [" << file << "]";
		debugLog(os.str());
		*/
		std::string post = file.substr(file.size()-3, 3);
		stringToLower(post);
		if (post == "png")
		{

			return loadPNG(file);
		}
		else if (post == "zga")
		{
			return loadZGA(file);
		}
		else if (post == "tga")
		{
			return loadTGA(file);
		}
		else
		{
			debugLog("unknown image file type: " + file);
		}
	}
	else
	{
		// load default image / leave white
		if (core->debugLogTextures)
			debugLog("***Could not find texture: " + file);
	}
	return false;
}

void Texture::apply(bool repeatOverride)
{
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	if (repeat || repeatOverride)
	{
		if (!repeating)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			repeating = true;
		}
	}
	else
	{
		if (repeating)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			repeating = false;
		}
	}
}

void Texture::unbind()
{
}

bool Texture::loadPNG(const std::string &file)
{
	return loadGeneric(file, 0);
}

// internal load functions
bool Texture::loadTGA(const std::string &file)
{
	return loadGeneric(file, "TGA");
}

bool Texture::loadZGA(const std::string &file)
{
	unsigned long size = 0;
	char *buf = readCompressedFile(file, &size);
	if (!buf || !size)
	{
		debugLog("Can't load ZGA File: " + file);
		return false;
	}

	bool luminanceAlpha = (format != 0 && format == GL_LUMINANCE_ALPHA);

	unsigned int w = 0, h = 0;
	if (filter == GL_NEAREST)
	{
		textures[0] = img_LoadGLTextureMem(buf, size, "TGA", false, luminanceAlpha,
			GL_CLAMP_TO_EDGE, filter, filter, &w, &h);
	}
	else
	{
		textures[0] = img_LoadGLTextureMem(buf, size, "TGA", true, luminanceAlpha,
			GL_CLAMP_TO_EDGE, GL_LINEAR_MIPMAP_LINEAR, filter, &w, &h);
	}

	delete [] buf;

	if (textures[0] == 0)
	{
		debugLog("Can't load ZGA File: " + file);
		return false;
	}

	width = w;
	height = h;
	return true;
}

// Shared implementation for PNG and TGA loading
bool Texture::loadGeneric(const std::string &file, const char *typeHint)
{
	if (file.empty()) return false;

	bool luminanceAlpha = (format != 0 && format == GL_LUMINANCE_ALPHA);

	unsigned long memsize = 0;
	const char *memptr = readFile(file, &memsize);
	if (!memptr || !memsize)
	{
		debugLog("Can't load image file: " + file);
		return false;
	}

	unsigned int w = 0, h = 0;
	if (filter == GL_NEAREST)
	{
		textures[0] = img_LoadGLTextureMem(memptr, memsize, typeHint, false, luminanceAlpha,
			GL_CLAMP_TO_EDGE, filter, filter, &w, &h);
	}
	else
	{
		textures[0] = img_LoadGLTextureMem(memptr, memsize, typeHint, true, luminanceAlpha,
			GL_CLAMP_TO_EDGE, GL_LINEAR_MIPMAP_LINEAR, filter, &w, &h);
	}

	delete [] memptr;

	if (textures[0] == 0)
	{
		debugLog("Can't load image file: " + file);
		return false;
	}

	width = w;
	height = h;
	return true;
}


// ceil to next power of 2
static unsigned int clp2(unsigned int x)
{
	--x;
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return x + 1;
}

unsigned char * Texture::getBufferAndSize(int *wparam, int *hparam, unsigned int *sizeparam)
{
	unsigned char *data = NULL;
	unsigned int size = 0;
	int tw = 0, th = 0;
	int w = 0, h = 0;

	// This can't happen. If it does we're doomed.
	if(width <= 0 || height <= 0)
		goto fail;

	glBindTexture(GL_TEXTURE_2D, textures[0]);

	// As returned by graphics driver

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

	// As we know it - but round to nearest power of 2 - OpenGL does this internally anyways.
	tw = clp2(width); // known to be > 0.
	th = clp2(height);

	if (w != tw || h != th)
	{
		std::ostringstream os;
		os << "Texture::getBufferAndSize() WARNING: width/height disagree: ";
		os << "Driver says (" << w << ", " << h << "); ";
		os << "Texture says (" << width << ", " << height << "); ";
		os << "Rounded to (" << tw << ", " << th << ")";
		debugLog(os.str());
		// choose max. for size calculation
		w = w > tw ? w : tw;
		h = h > th ? h : th;
	}

	size = w * h * 4;
	if (!size)
		goto fail;

	data = (unsigned char*)malloc(size + 32);
	if (!data)
	{
		std::ostringstream os;
		os << "Game::fillGridFromQuad allocation failure, size = " << size;
		errorLog(os.str());
		goto fail;
	}
	memcpy(data + size, "SAFE", 5);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Not sure but this might be the case with nouveau drivers on linux... still investigating. -- fg
	if(memcmp(data + size, "SAFE", 5))
	{
		errorLog("Texture::getBufferAndSize(): Broken graphics driver! Wrote past end of buffer!");
		free(data); // in case we are here, this will most likely cause a crash.
		goto fail;
	}

	*wparam = w;
	*hparam = h;
	*sizeparam = size;
	return data;


fail:
	*wparam = 0;
	*hparam = 0;
	*sizeparam = 0;
	return NULL;
}
