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

#ifndef BBGE_RENDER_TRANSFORM_H
#define BBGE_RENDER_TRANSFORM_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Minimal CPU-side replacement for the OpenGL 1.x modelview matrix stack
// (glPushMatrix/glPopMatrix/glTranslatef/glRotatef/glScalef/glLoadIdentity),
// needed because SDL3's 2D renderer has no equivalent - draw calls take
// pre-transformed vertices, not a running transform state. Mirrors the old
// GL API shape closely (same push/pop/translate/rotate/scale calls, same
// glRotatef-style "degrees + axis" signature) so callers translated from
// GL code stay close to the original structure and are easy to diff
// against it.
//
// Uses glm::mat4/vec4 (not the leaner glm::mat3/vec2 2D-affine form) to
// match the convention BBGE/Vector.cpp already established for glm usage
// in this codebase.
class RenderTransformStack
{
public:
	RenderTransformStack() { stack.push_back(glm::mat4(1.0f)); }

	void pushMatrix() { stack.push_back(stack.back()); }

	void popMatrix()
	{
		if (stack.size() > 1)
			stack.pop_back();
	}

	void loadIdentity() { stack.back() = glm::mat4(1.0f); }

	void translate(float x, float y, float z = 0.0f)
	{
		stack.back() = glm::translate(stack.back(), glm::vec3(x, y, z));
	}

	// degrees + axis, matching glRotatef's own signature/convention.
	// NOTE: this project's bundled GLM (ExternalLibs/glm) predates the
	// GLM_FORCE_RADIANS mechanism entirely - glm::rotate() here always
	// takes the angle in *degrees* (see its doc comment in
	// gtc/matrix_transform.hpp: "angle expressed in degrees"), unlike
	// modern GLM where it expects radians. Passing degrees directly
	// (not glm::radians(degrees)) is correct for this version - an
	// earlier version of this function called glm::radians() first,
	// which silently shrank every rotation by a factor of ~57x (180/pi),
	// confirmed via real playtesting diagnostics: an intended ~50 degree
	// rotation was landing as roughly 0.87 degrees, matching to 5+
	// significant figures across multiple independent test values.
	void rotate(float degrees, float x, float y, float z)
	{
		stack.back() = glm::rotate(stack.back(), degrees, glm::vec3(x, y, z));
	}

	void scale(float x, float y, float z = 1.0f)
	{
		stack.back() = glm::scale(stack.back(), glm::vec3(x, y, z));
	}

	const glm::mat4 &top() const { return stack.back(); }

	// Transform a local-space point by the current top-of-stack matrix -
	// this is what replaces "issue glVertex2f() while some matrix is
	// active": callers compute world-space vertices explicitly with this
	// instead, then hand them to SDL_RenderGeometry.
	glm::vec4 transformPoint(float x, float y, float z = 0.0f) const
	{
		return stack.back() * glm::vec4(x, y, z, 1.0f);
	}

private:
	std::vector<glm::mat4> stack;
};

#endif
