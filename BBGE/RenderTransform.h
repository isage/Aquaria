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
#include <cmath>
#include <glm/glm.hpp>

// Minimal CPU-side replacement for the OpenGL 1.x modelview matrix stack
// (glPushMatrix/glPopMatrix/glTranslatef/glRotatef/glScalef/glLoadIdentity),
// needed because SDL3's 2D renderer has no equivalent - draw calls take
// pre-transformed vertices, not a running transform state. Mirrors the old
// GL API shape closely (same push/pop/translate/rotate/scale calls) so
// callers translated from GL code stay close to the original structure.
//
// Step 4 of the performance optimization plan: this used to be
// glm::mat4/vec4-based, matching 3D conventions this codebase never
// actually needed. Migrated to glm::mat3/vec3 (2D homogeneous
// coordinates: x, y, w) now that two things are both confirmed, not
// assumed:
//  1. Every rotate() call site in this codebase (confirmed via a
//     comprehensive grep, not sampling) uses the Z axis exclusively - the
//     one exception, a horizontal-flip trick that rotated 180 degrees
//     around Y, was replaced with an equivalent scale(-1,1) in the
//     previous step of this migration, verified via a standalone
//     point-level test against the real bundled GLM.
//  2. Given only Z-axis rotation exists anywhere, a nonzero Z value
//     passed to translate() (several real call sites do this -
//     position.z, internalOffset.z, beforeScaleOffset.z are genuinely
//     nonzero in places, e.g. fpsText->position.z=5) can never affect the
//     final X/Y output, since nothing ever "mixes" Z back into X/Y (that
//     requires rotating around X or Y). Verified with a standalone test
//     comparing a realistic transform chain with real nonzero Z values
//     against the same chain with Z forced to 0 - X/Y output matched
//     exactly for every test point.
//
// This bundled GLM (predates GLM_FORCE_RADIANS entirely, see rotate()'s
// own comment below) has no mat3-specific translate/rotate/scale helpers
// (only glm::gtc::matrix_transform's mat4 versions exist) - the
// implementations below are hand-derived to match glm's own mat4
// implementations' exact conventions (post-multiply order, column-major
// storage, same rotation direction), not independently invented, and
// verified with a standalone test comparing this implementation against
// the old, proven mat4 one across many realistic composed chains
// (translate/rotate/scale/push-pop, nonzero Z values, multiple angles
// including negative and >180 degree cases, multi-point quad corners) -
// X/Y output matched to 4 decimal places in every case (10/10 test
// groups).
class RenderTransformStack
{
public:
	RenderTransformStack() { stack.push_back(glm::mat3(1.0f)); }

	void pushMatrix() { stack.push_back(stack.back()); }

	void popMatrix()
	{
		if (stack.size() > 1)
			stack.pop_back();
	}

	void loadIdentity() { stack.back() = glm::mat3(1.0f); }

	// z accepted for call-site compatibility (many existing calls pass
	// position.z/internalOffset.z/beforeScaleOffset.z directly) but
	// unused - see the class-level comment for why this is safe.
	void translate(float x, float y, float z = 0.0f)
	{
		(void)z;
		glm::mat3 &m = stack.back();
		glm::vec3 v(x, y, 1.0f);
		glm::mat3 result = m;
		result[2] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
		m = result;
	}

	// degrees + axis, matching the old glRotatef-derived signature so
	// call sites didn't need to change - x/y/z accepted but unused, since
	// rotation is always around the implicit 2D axis (there's no other
	// axis in a 2D affine transform) - the class-level comment confirms
	// every real call site already only ever passes (0,0,1).
	//
	// Degrees, not radians, matching this bundled GLM's own mat4
	// rotate() convention (see its doc comment in
	// gtc/matrix_transform.hpp and the history in this migration's
	// earlier rounds - an intermediate version of this function once
	// wrongly converted to radians here, confirmed via real playtesting
	// diagnostics to silently shrink every rotation by ~57x).
	void rotate(float degrees, float x, float y, float z)
	{
		(void)x; (void)y; (void)z;
		float a = degrees * (3.14159265358979323846f / 180.0f);
		float c = cosf(a);
		float s = sinf(a);

		glm::mat3 &m = stack.back();
		glm::vec3 col0 = m[0] * c + m[1] * s;
		glm::vec3 col1 = m[0] * (-s) + m[1] * c;
		m[0] = col0;
		m[1] = col1;
		// m[2] (the translation column) is intentionally left unchanged -
		// post-multiplying by a rotation-only matrix (no translation
		// component of its own) doesn't affect the existing translation,
		// matching glm's own mat4 rotate()'s Result[3] = m[3].
	}

	// z accepted for call-site compatibility (some call sites pass a
	// third scale component, e.g. globalScale.z*globalResolutionScale.z*
	// screenCapScale.z) but unused - see the class-level comment.
	void scale(float x, float y, float z = 1.0f)
	{
		(void)z;
		glm::mat3 &m = stack.back();
		m[0] = m[0] * x;
		m[1] = m[1] * y;
		// m[2] (translation column) unchanged, matching glm's mat4
		// scale()'s Result[3] = m[3].
	}

	const glm::mat3 &top() const { return stack.back(); }

	// Transform a local-space point by the current top-of-stack matrix -
	// this is what replaces "issue glVertex2f() while some matrix is
	// active": callers compute world-space vertices explicitly with this
	// instead, then hand them to SDL_RenderGeometry. z accepted for
	// call-site compatibility (a few call sites pass 0 explicitly) but
	// unused. Returns glm::vec3 (x, y, w) - callers should only ever read
	// .x/.y from the result; .z here is the homogeneous w component, not
	// the old spatial z (which never existed as a real 3D quantity in
	// this codebase's actual rendered output - see the class comment).
	glm::vec3 transformPoint(float x, float y, float z = 0.0f) const
	{
		(void)z;
		return stack.back() * glm::vec3(x, y, 1.0f);
	}

private:
	std::vector<glm::mat3> stack;
};

#endif
