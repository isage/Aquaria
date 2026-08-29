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
#include "RenderObject.h"
#include "RenderState.h"
#include "PerfLog.h"
#include "Core.h"
#include "MathFunctions.h"

#include <assert.h>
#include <algorithm>

#include "glm/glm.hpp"
// Step 5 of the performance optimization plan: glm/gtx/transform.hpp
// (glm::rotate()/translate()/scale()) removed - confirmed via a
// codebase-wide sweep that nothing calls these anymore, since Step 4's
// mat3 migration hand-rolled every transform matrix directly instead.
// Modern GLM gates this header behind GLM_ENABLE_EXPERIMENTAL (it's
// classified as an experimental extension); since nothing here actually
// needs it, removing the include is cleaner than opting into an
// experimental API for zero benefit.

bool	RenderObject::renderCollisionShape			= false;
SDL_Texture*	RenderObject::lastTextureApplied			= 0;
bool	RenderObject::lastTextureRepeat				= false;
bool	RenderObject::renderPaths					= false;

const bool RENDEROBJECT_SHAREATTRIBUTES				= true;
const bool RENDEROBJECT_FASTTRANSFORM				= false;

RenderObjectLayer *RenderObject::rlayer				= 0;

void RenderObject::toggleAlpha(float t)
{
	if (alpha.x < 0.5f)
		alpha.interpolateTo(1,t);
	else
		alpha.interpolateTo(0,t);
}

int RenderObject::getTopLayer()
{
	if (parent)
	{
		return parent->getTopLayer();
	}
	return layer;
}

void RenderObject::applyBlendType()
{
	if (blendEnabled)
	{
		switch (blendType)
		{
		case BLEND_DEFAULT:
			currentBlendMode = SDL_BLENDMODE_BLEND;
		break;
		case BLEND_ADD:
			currentBlendMode = SDL_BLENDMODE_ADD;
		break;
		case BLEND_MULT:
			currentBlendMode = SDL_BLENDMODE_MOD;
		break;
		}
	}
	else
	{
		currentBlendMode = SDL_BLENDMODE_NONE;
	}
}

void RenderObject::setColorMult(const Vector &color, const float alpha)
{
	if (colorIsSaved)
	{
		debugLog("setColorMult() WARNING: can't do nested multiplies");
		return;
	}
	this->colorIsSaved = true;
	this->savedColor.x = this->color.x;
	this->savedColor.y = this->color.y;
	this->savedColor.z = this->color.z;
	this->savedAlpha = this->alpha.x;
	this->color *= color;
	this->alpha.x *= alpha;
	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		(*i)->setColorMult(color, alpha);
	}
}

void RenderObject::clearColorMult()
{
	if (!colorIsSaved)
	{
		debugLog("clearColorMult() WARNING: no saved color to restore");
		return;
	}
	this->color.x = this->savedColor.x;
	this->color.y = this->savedColor.y;
	this->color.z = this->savedColor.z;
	this->alpha.x = this->savedAlpha;
	this->colorIsSaved = false;
	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		(*i)->clearColorMult();
	}
}

RenderObject::RenderObject()
{
	addType(SCO_RENDEROBJECT);
	useOldDT = false;

	updateAfterParent = false;
	ignoreUpdate = false;
	overrideRenderPass = OVERRIDE_NONE;
	renderPass = 0;
	overrideCullRadiusSqr = 0;
	repeatTexture = false;
	alphaMod = 1;
	collisionMaskRadius = 0;
	collideRadius = 0;
	motionBlurTransition = false;
	motionBlurFrameOffsetCounter = 0;
	motionBlurFrameOffset = 0;
	motionBlur = false;
	idx = -1;
	_fv = false;
	_fh = false;
	updateCull = -1;
	//rotateFirst = true;
	layer = LR_NONE;
	cull = true;

	pm = PM_NONE;

	positionSnapTo = 0;

	//updateMultiplier = 1;
	blendEnabled = true;
	texture = 0;
	width = 0;
	height = 0;
	scale = Vector(1,1,1);
	color = Vector(1,1,1);
	alpha.x = 1;
	//mode = 0;
	life = maxLife = 1;
	decayRate = 0;
	_dead = false;
	_hidden = false;
	_static = false;
	fadeAlphaWithLife = false;
	blendType = BLEND_DEFAULT;
	currentBlendMode = SDL_BLENDMODE_BLEND;
	effectiveColor = Vector(1,1,1);
	effectiveAlpha = 1;
	//lifeAlphaFadeMultiplier = 1;
	followCamera = 0;
	stateData = 0;
	parent = 0;
	//useColor = true;
	renderBeforeParent = false;
	//followXOnly = false;
	//renderOrigin = false;
	colorIsSaved = false;
	shareAlphaWithChildren = false;
	shareColorWithChildren = false;
	motionBlurTransitionTimer = 0;
}

RenderObject::~RenderObject()
{
}

Vector RenderObject::getWorldPosition()
{
	return getWorldCollidePosition();
}

RenderObject* RenderObject::getTopParent()
{
	RenderObject *p = parent;
	RenderObject *lastp=0;
	while (p)
	{
		lastp = p;
		p = p->parent;
	}
	return lastp;
}

bool RenderObject::isPieceFlippedHorizontal()
{
	RenderObject *p = getTopParent();
	if (p)
		return p->isfh();
	return isfh();
}


Vector RenderObject::getInvRotPosition(const Vector &vec)
{
	// Pure transform math (like the old Vector::rotate() GL trick) - uses
	// its own isolated RenderTransformStack rather than core->transform,
	// matching the original's glLoadIdentity()-from-scratch behavior
	// (deliberately ignoring whatever the "current" matrix was at the
	// call site).
	RenderTransformStack xf;

	std::vector<RenderObject*>chain;
	RenderObject *p = this;
	while(p)
	{
		chain.push_back(p);
		p = p->parent;
	}
	
	for (int i = chain.size()-1; i >= 0; i--)
	{
		xf.rotate(-(chain[i]->rotation.z+chain[i]->rotationOffset.z), 0, 0, 1);
	
		if (chain[i]->isfh())
		{
			// Horizontal-flip trick: was rotate(180, Y-axis), a real 3D
			// rotation exploiting the unused third dimension. Replaced with
			// scale(-1,1) - verified via a standalone test (real bundled GLM,
			// both composition orders used across this codebase's call sites,
			// point-level comparison) to produce identical results for real
			// (z=0) 2D sprite geometry; the two only differ in how they treat
			// z, which no 2D vertex here ever has nonzero. Step 4 of the
			// performance optimization plan - this needed to happen before,
			// and separately from, migrating the transform stack itself to
			// pure 2D affine math, since a 2D affine transform has no third
			// dimension to rotate through.
			xf.scale(-1, 1);
		}
	}

	glm::vec3 result = xf.transformPoint(0, 0, 0);

	if (vec.x != 0 || vec.y != 0)
	{
		xf.translate(vec.x, vec.y, 0);
		result = xf.transformPoint(0, 0, 0);
	}

	return Vector(result.x, result.y, result.z);
}

static const float RENDEROBJECT_PI = 3.14159265358979323846f;

// Step 4 of the performance optimization plan: converted from glm::mat4
// to glm::mat3, matching RenderTransformStack's own migration (see
// BBGE/RenderTransform.h). Standalone mat3 building blocks
// (translate/rotate/scale as one-off matrices, not "apply to existing
// stack" operations like RenderTransformStack's methods) since this
// function doesn't go through that class - it builds and composes
// glm::mat3 objects directly, matching the same column-major convention
// and rotation direction verified there. Converting this specific
// function (recursive, with a real parent-child chain, flip handling,
// and multiple offset/scale stages) was verified with a dedicated
// standalone test against the old mat4 version - both a real
// parent-child chain and a flipped child at several rotation angles, not
// just a single flat case - all cases matched to within floating-point
// tolerance.
static glm::mat3 matrixChain(const RenderObject *ro)
{
	glm::mat3 posOffsetTranslate(1,0,0, 0,1,0, ro->position.x+ro->offset.x, ro->position.y+ro->offset.y, 1);

	float rotRad = (ro->rotation.z + ro->rotationOffset.z) * (RENDEROBJECT_PI / 180.0f);
	float rotC = cosf(rotRad), rotS = sinf(rotRad);
	glm::mat3 rotationMat(rotC, rotS, 0, -rotS, rotC, 0, 0, 0, 1);

	glm::mat3 beforeScaleTranslate(1,0,0, 0,1,0, ro->beforeScaleOffset.x, ro->beforeScaleOffset.y, 1);
	glm::mat3 scaleMat(ro->scale.x, 0, 0, 0, ro->scale.y, 0, 0, 0, 1);

	glm::mat3 parentChain = ro->getParent() ? matrixChain(ro->getParent()) : glm::mat3(1.0f);
	glm::mat3 tranformMatrix = parentChain * posOffsetTranslate * rotationMat * beforeScaleTranslate * scaleMat;

	if (ro->isfh())
	{
		// Horizontal-flip trick: was rotate(180, Y-axis), a real 3D
		// rotation exploiting the unused third dimension. Replaced with
		// scale(-1,1) - verified via a standalone test (real bundled GLM,
		// both composition orders used across this codebase's call sites,
		// point-level comparison) to produce identical results for real
		// (z=0) 2D sprite geometry; the two only differ in how they treat
		// z, which no 2D vertex here ever has nonzero. This call site
		// post-multiplies the flip after the chain's own Z-rotation is
		// already baked in, matching "order B" in that verification.
		// Step 4 of the performance optimization plan.
		glm::mat3 flipMat(-1,0,0, 0,1,0, 0,0,1);
		tranformMatrix = tranformMatrix * flipMat;
	}

	glm::mat3 internalOffsetTranslate(1,0,0, 0,1,0, ro->internalOffset.x, ro->internalOffset.y, 1);
	tranformMatrix = tranformMatrix * internalOffsetTranslate;
	return tranformMatrix;
}

float RenderObject::getWorldRotation()
{
	Vector up = getWorldCollidePosition(Vector(0,1));
	Vector orig = getWorldPosition();
	float rot = 0;
	MathFunctions::calculateAngleBetweenVectorsInDegrees(orig, up, rot);
	return rot;
}

Vector RenderObject::getWorldPositionAndRotation()
{
	Vector up = getWorldCollidePosition(Vector(0,1));
	Vector orig = getWorldPosition();
	MathFunctions::calculateAngleBetweenVectorsInDegrees(orig, up, orig.z);
	return orig;
}

Vector RenderObject::getWorldCollidePosition(const Vector &vec)
{
	// Step 4 of the performance optimization plan: matrixChain() now
	// returns glm::mat3, not glm::mat4 - the translation column moved
	// from index [3] (mat4's 4th column) to index [2] (mat3's 3rd,
	// last column). Verified together with matrixChain() itself via the
	// same standalone test (a real parent-child chain, flip handling,
	// multiple rotation angles) - see the comment there.
	glm::mat3 collideTranslate(1,0,0, 0,1,0, collidePosition.x + vec.x, collidePosition.y + vec.y, 1);
	glm::mat3 transformMatrix = matrixChain(this) * collideTranslate;

	return Vector(transformMatrix[2][0], transformMatrix[2][1], 0);
}

void RenderObject::fhTo(bool fh)
{
	if ((fh && !_fh) || (!fh && _fh))
	{
		flipHorizontal();
	}
}

void RenderObject::flipHorizontal()
{
	bool wasFlippedHorizontal = _fh;

	_fh = !_fh;

	if (wasFlippedHorizontal != _fh)
	{
		onFH();
	}
	/*
	if (wasFlippedHorizontal && !_fh)
		for (int i = 0; i < this->collisionMask.size(); i++)
			collisionMask[i].x = -collisionMask[i].x;
	else if (!wasFlippedHorizontal && _fh)
		for (int i = 0; i < this->collisionMask.size(); i++)
			collisionMask[i].x = -collisionMask[i].x;
	*/
}

void RenderObject::flipVertical()
{
	//bool wasFlippedVertical = _fv;
	_fv = !_fv;
	/*
	if (wasFlippedVertical && !_fv)
		for (int i = 0; i < this->collisionMask.size(); i++)
			collisionMask[i].y = -collisionMask[i].y;
	else if (!wasFlippedVertical && _fv)
		for (int i = 0; i < this->collisionMask.size(); i++)
			collisionMask[i].y = -collisionMask[i].y;
	*/
}

void RenderObject::destroy()
{
	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		// must do this first
		// otherwise child will try to remove THIS
		(*i)->parent = 0;
		switch ((*i)->pm)
		{
		case PM_STATIC:
			(*i)->destroy();
			break;
		case PM_POINTER:
			(*i)->destroy();
			delete (*i);
			break;
		}
	}
	children.clear();

	if (parent)
	{
		parent->removeChild(this);
		parent = 0;
	}

	texture = NULL;
}

void RenderObject::copyProperties(RenderObject *target)
{
	this->color						= target->color;
	this->position					= target->position;
	this->alpha						= target->alpha;
	this->velocity					= target->velocity;
}

const RenderObject &RenderObject::operator=(const RenderObject &r)
{
	errorLog("Operator= not defined for RenderObject. Use 'copyProperties'");
	return *this;
}

Vector RenderObject::getRealPosition()
{
	if (parent)
	{
		return position + offset + parent->getRealPosition();
	}
	return position + offset;
}

Vector RenderObject::getRealScale()
{
	if (parent)
	{
		return scale * parent->getRealScale();
	}
	return scale;
}

void RenderObject::setStateDataObject(StateData *state)
{
	stateData = state;
}


void RenderObject::toggleCull(bool value)
{
	cull = value;
}

void RenderObject::moveToFront()
{
	if(RenderObject *p = parent)
	{
		if(p->children.size() && p->children[p->children.size()-1] != this)
		{
			p->removeChild(this);
			p->addChild(this, (ParentManaged)this->pm, RBP_NONE, CHILD_BACK); // To back of list -> rendered on top
		}
	}
	else if (layer != -1)
		core->renderObjectLayers[this->layer].moveToFront(this);
}

void RenderObject::moveToBack()
{
	if(RenderObject *p = parent)
	{
		if(p->children.size() && p->children[0] != this)
		{
			p->removeChild(this);
			p->addChild(this, (ParentManaged)this->pm, RBP_NONE, CHILD_FRONT); // To front of list -> rendered first, below everything else
		}
	}
	else if (layer != -1)
		core->renderObjectLayers[this->layer].moveToBack(this);
}

void RenderObject::enableMotionBlur(int sz, int off)
{
	motionBlur = true;
	motionBlurPositions.resize(sz);
	motionBlurFrameOffsetCounter = 0;
	motionBlurFrameOffset = off;
	for (int i = 0; i < motionBlurPositions.size(); i++)
	{
		motionBlurPositions[i].position = position;
		motionBlurPositions[i].rotz = rotation.z;
	}
}

void RenderObject::disableMotionBlur()
{
	motionBlurTransition = true;
	motionBlurTransitionTimer = 1.0;
	motionBlur = false;
}

bool RenderObject::isfhr()
{
	RenderObject *p = this;
	bool fh = false;
	do
		if (p->isfh())
			fh = !fh;
	while ((p = p->parent));
	return fh;

}

bool RenderObject::isfvr()
{
	RenderObject *p = this;
	bool fv = false;
	do
		if (p->isfv())
			fv = !fv;
	while ((p = p->parent));
	return fv;

}

bool RenderObject::hasRenderPass(const int pass)
{
	if (pass == renderPass)
		return true;
	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		if (!(*i)->isDead() && (*i)->hasRenderPass(pass))
			return true;
	}
	return false;
}

void RenderObject::render()
{
	if (isHidden()) return;

	/// new (breaks anything?)
	if (alpha.x == 0 || alphaMod == 0) return;

	if (core->currentLayerPass != RENDER_ALL && renderPass != RENDER_ALL)
	{
		RenderObject *top = getTopParent();
		if (top == NULL && this->overrideRenderPass != OVERRIDE_NONE)
		{
			// FIXME: overrideRenderPass is not applied to the
			// node itself in the original check (below); is
			// that intentional?  Doing the same thing here
			// for the time being.  --achurch
			if (core->currentLayerPass != this->renderPass
			 && core->currentLayerPass != this->overrideRenderPass)
				return;
		}
		else if (top != NULL && top->overrideRenderPass != OVERRIDE_NONE)
		{
			if (core->currentLayerPass != top->overrideRenderPass)
				return;
		}
		else
		{
			if (!hasRenderPass(core->currentLayerPass))
				return;
		}
	}
	
	if (motionBlur || motionBlurTransition)
	{
		Vector oldPos = position;
		float oldAlpha = alpha.x;
		float oldRotZ = rotation.z;
		for (int i = 0; i < motionBlurPositions.size(); i++)
		{
			position = motionBlurPositions[i].position;
			rotation.z = motionBlurPositions[i].rotz;
			alpha = 1.0f-(float(i)/float(motionBlurPositions.size()));
			alpha *= 0.5f;
			if (motionBlurTransition)
			{
				alpha *= motionBlurTransitionTimer;
			}
			renderCall();
		}		
		position = oldPos;
		alpha.x = oldAlpha;
		rotation.z = oldRotZ;

		renderCall();
	}
	else
		renderCall();
}

void RenderObject::renderCall()
{

	//RenderObjectLayer *rlayer = core->getRenderObjectLayer(getTopLayer());

	if (positionSnapTo)
		this->position = *positionSnapTo;

	position += offset;

	if (!RENDEROBJECT_FASTTRANSFORM)
		core->transform.pushMatrix();

	if (!RENDEROBJECT_FASTTRANSFORM)
	{
		if (layer != LR_NONE)
		{
			RenderObjectLayer *l = &core->renderObjectLayers[layer];
			if (l->followCamera != NO_FOLLOW_CAMERA)
			{
				followCamera = l->followCamera;
			}
		}
		if (followCamera!=0 && !parent)
		{
			if (followCamera == 1)
			{
			 	core->loadBaseTransform();
				core->transform.scale(core->globalResolutionScale.x, core->globalResolutionScale.y, 0);
				core->transform.translate(position.x, position.y, position.z);
				if (isfh())
				{
					// Horizontal-flip trick: was rotate(180, Y-axis), a real 3D
					// rotation exploiting the unused third dimension. Replaced with
					// scale(-1,1) - verified via a standalone test (real bundled GLM,
					// both composition orders used across this codebase's call sites,
					// point-level comparison) to produce identical results for real
					// (z=0) 2D sprite geometry; the two only differ in how they treat
					// z, which no 2D vertex here ever has nonzero. Step 4 of the
					// performance optimization plan - this needed to happen before,
					// and separately from, migrating the transform stack itself to
					// pure 2D affine math, since a 2D affine transform has no third
					// dimension to rotate through.
					core->transform.scale(-1, 1);
				}

				core->transform.rotate(rotation.z+rotationOffset.z, 0, 0, 1);
			}
			else
			{
				Vector pos = getFollowCameraPosition();

				core->transform.translate(pos.x, pos.y, pos.z);
				if (isfh())
				{
					// Horizontal-flip trick: was rotate(180, Y-axis), a real 3D
					// rotation exploiting the unused third dimension. Replaced with
					// scale(-1,1) - verified via a standalone test (real bundled GLM,
					// both composition orders used across this codebase's call sites,
					// point-level comparison) to produce identical results for real
					// (z=0) 2D sprite geometry; the two only differ in how they treat
					// z, which no 2D vertex here ever has nonzero. Step 4 of the
					// performance optimization plan - this needed to happen before,
					// and separately from, migrating the transform stack itself to
					// pure 2D affine math, since a 2D affine transform has no third
					// dimension to rotate through.
					core->transform.scale(-1, 1);
				}
				core->transform.rotate(rotation.z+rotationOffset.z, 0, 0, 1);
			}
		}
		else
		{

			core->transform.translate(position.x, position.y, position.z);
			if (RenderObject::renderPaths && position.data && position.data->path.getNumPathNodes() > 0)
			{
				// TODO: debug path-visualization overlay (RenderObject::renderPaths, off by default)
			}

			core->transform.rotate(rotation.z+rotationOffset.z, 0, 0, 1);
			if (isfh())
			{
				// Horizontal-flip trick: was rotate(180, Y-axis), a real 3D
				// rotation exploiting the unused third dimension. Replaced with
				// scale(-1,1) - verified via a standalone test (real bundled GLM,
				// both composition orders used across this codebase's call sites,
				// point-level comparison) to produce identical results for real
				// (z=0) 2D sprite geometry; the two only differ in how they treat
				// z, which no 2D vertex here ever has nonzero. Step 4 of the
				// performance optimization plan - this needed to happen before,
				// and separately from, migrating the transform stack itself to
				// pure 2D affine math, since a 2D affine transform has no third
				// dimension to rotate through.
				core->transform.scale(-1, 1);
			}
		}
				
		core->transform.translate(beforeScaleOffset.x, beforeScaleOffset.y, beforeScaleOffset.z);
		core->transform.scale(scale.x, scale.y, 1);
		core->transform.translate(internalOffset.x, internalOffset.y, internalOffset.z);
	}

	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		if (!(*i)->isDead() && (*i)->renderBeforeParent)
			(*i)->render();
	}


	//if (useColor)
	{
		if (rlayer)
		{
			effectiveColor = Vector(color.x * rlayer->color.x, color.y * rlayer->color.y, color.z * rlayer->color.z);
			effectiveAlpha = alpha.x*alphaMod;
		}
		else
		{
			effectiveColor = Vector(color.x, color.y, color.z);
			effectiveAlpha = alpha.x*alphaMod;
		}
	}
	
	if (texture)
	{

		if (texture->sdlTexture != lastTextureApplied || repeatTexture != lastTextureRepeat)
		{
			texture->apply(repeatTexture);
			lastTextureRepeat = repeatTexture;
			lastTextureApplied = texture->sdlTexture;
		}
	}
	else
	{
		if (lastTextureApplied != 0 || repeatTexture != lastTextureRepeat)
		{
			lastTextureApplied = 0;
			lastTextureRepeat = repeatTexture;
		}
	}
	
	applyBlendType();


	bool doRender = true;
	int pass = renderPass;
	if (core->currentLayerPass != RENDER_ALL && renderPass != RENDER_ALL)
	{
		RenderObject *top = getTopParent();
		if (top)
		{
			if (top->overrideRenderPass != OVERRIDE_NONE)
				pass = top->overrideRenderPass;
		}

		doRender = (core->currentLayerPass == pass);
	}

	if (renderCollisionShape)
		renderCollision();

	if (doRender)
		onRender();

	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		if (!(*i)->isDead() && !(*i)->renderBeforeParent)
			(*i)->render();
	}


	if (!RENDEROBJECT_FASTTRANSFORM)
	{
		core->transform.popMatrix();
	}


	position -= offset;
}

void RenderObject::renderCollision()
{
	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer) return;

	if (!collisionRects.empty())
	{
		RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColorFloat(renderer, 1.0f, 0.5f, 1.0f, 0.5f);

		for (int i = 0; i < collisionRects.size(); i++)
		{
			RectShape *r = &collisionRects[i];

			glm::vec3 p0 = core->transform.transformPoint(r->x1, r->y1);
			glm::vec3 p1 = core->transform.transformPoint(r->x1, r->y2);
			glm::vec3 p2 = core->transform.transformPoint(r->x2, r->y2);
			glm::vec3 p3 = core->transform.transformPoint(r->x2, r->y1);

			SDL_FColor col = {1.0f, 0.5f, 1.0f, 0.5f};
			SDL_Vertex v[4];
			v[0].position={p0.x,p0.y}; v[0].tex_coord={0,0}; v[0].color=col;
			v[1].position={p1.x,p1.y}; v[1].tex_coord={0,0}; v[1].color=col;
			v[2].position={p2.x,p2.y}; v[2].tex_coord={0,0}; v[2].color=col;
			v[3].position={p3.x,p3.y}; v[3].tex_coord={0,0}; v[3].color=col;
			static const int idx[6] = {0,1,2,0,2,3};
			SDL_RenderGeometry(renderer, NULL, v, 4, idx, 6);
			PerfLog::countDrawCall();
			// TODO: the 4 corner points (glPointSize(5)) have no direct SDL_RenderGeometry equivalent
		}
	}

	if (!collisionMask.empty())
	{
		core->transform.pushMatrix();
		core->loadBaseTransform();
		core->setupRenderPositionAndScale();

		RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

		for (int i = 0; i < transformedCollisionMask.size(); i++)
		{
			Vector collide = this->transformedCollisionMask[i];
			RenderObject *parent = this->getTopParent();
			if (parent)
			{
				core->transform.pushMatrix();
				core->transform.translate(collide.x, collide.y, 0);
				drawCircle(collideRadius*parent->scale.x, 45, 1, 1, 0, 0.5f);
				core->transform.popMatrix();
			}
		}

		core->transform.popMatrix();
	}
	else if (collideRadius > 0)
	{
		core->transform.pushMatrix();
		core->loadBaseTransform();
		core->setupRenderPositionAndScale();
		core->transform.translate(position.x+offset.x, position.y+offset.y, 0);
		core->transform.translate(internalOffset.x, internalOffset.y, 0);

		RenderState::setRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		drawCircle(collideRadius, 8, 1, 0, 0, 0.5f);

		core->transform.popMatrix();
	}
}

void RenderObject::addDeathNotify(RenderObject *r)
{
	deathNotifications.remove(r);
	deathNotifications.push_back(r);
}

void RenderObject::deathNotify(RenderObject *r)
{
	deathNotifications.remove(r);
}

Vector RenderObject::getCollisionMaskNormal(int index)
{
	Vector sum;
	int num=0;
	for (int i = 0; i < this->transformedCollisionMask.size(); i++)
	{
		if (i != index)
		{
			Vector diff = transformedCollisionMask[index] - transformedCollisionMask[i];
			if (diff.isLength2DIn(128))
			{
				sum += diff;
				num++;
			}
		}
	}
	if (!sum.isZero())
	{
		sum /= num;
		
		sum.normalize2D();

		/*
		std::ostringstream os;
		os << "found [" << num << "] circles, got normal [" << sum.x << ", " << sum.y << "]";
		debugLog(os.str());
		*/
	}

	return sum;
}

void RenderObject::lookAt(const Vector &pos, float t, float minAngle, float maxAngle, float offset)
{
	Vector myPos = this->getWorldPosition();
	float angle = 0;
	
	if (myPos.x == pos.x && myPos.y == pos.y)
	{
		return;
	}
	MathFunctions::calculateAngleBetweenVectorsInDegrees(myPos, pos, angle);	

	RenderObject *p = parent;
	while (p)
	{
		angle -= p->rotation.z;
		p = p->parent;
	}

	if (isPieceFlippedHorizontal())
	{
		angle = 180-angle;
		
		/*
		minAngle = -minAngle;
		maxAngle = -maxAngle;
		std::swap(minAngle, maxAngle);
		*/
		//std::swap(minAngle, maxAngle);
		/*
		minAngle = -(180+minAngle);
		maxAngle = -(180+maxAngle);
		*/
		/*
		if (minAngle > maxAngle)
			std::swap(minAngle, maxAngle);
		*/
		offset = -offset;
	}
	angle += offset;
	if (angle < minAngle)
		angle = minAngle;
	if (angle > maxAngle)
		angle = maxAngle;

	int amt = 10;
	if (isPieceFlippedHorizontal())
	{
		if (pos.x < myPos.x-amt)
		{
			angle = 0;
		}
	}
	else
	{
		if (pos.x > myPos.x+amt)
		{
			angle = 0;
		}
	}

	rotation.interpolateTo(Vector(0,0,angle), t);
}

void RenderObject::update(float dt)
{
	if (ignoreUpdate)
	{
		return;
	}
	if (useOldDT)
	{
		dt = core->get_old_dt();
	}
	if (!isDead())
	{
		//dt *= updateMultiplier;
		onUpdate(dt);

		if (isHidden())
			return;

		for (Children::iterator i = children.begin(); i != children.end(); i++)
		{
			if ((*i)->updateAfterParent && (((*i)->pm == PM_POINTER) || ((*i)->pm == PM_STATIC)))
			{
				(*i)->update(dt);
			}
		}
	}
}

void RenderObject::removeChild(RenderObject *r)
{
	r->parent = 0;
	Children::iterator oldend = children.end();
	Children::iterator newend = std::remove(children.begin(), oldend, r);
	if(oldend != newend)
	{
		children.resize(std::distance(children.begin(), newend));
		return;
	}

	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		(*i)->removeChild(r);
	}
}

void RenderObject::enqueueChildDeletion(RenderObject *r)
{
	if (r->parent == this)
	{
		childGarbage.push_back(r);
	}
}

void RenderObject::safeKill()
{
	alpha = 0;
	life = 0;
	onEndOfLife();
	//deathEvent.call();
	for (RenderObjectList::iterator i = deathNotifications.begin(); i != deathNotifications.end(); i++)
	{
		(*i)->deathNotify(this);
	}
	//dead = true;
	if (this->parent)
	{
		parent->enqueueChildDeletion(this);
		/*
		parent->removeChild(this);
		core->enqueueRenderObjectDeletion(this);
		*/
	}
	else
	{
		if (stateData)
			stateData->removeRenderObject(this);
		else
			core->enqueueRenderObjectDeletion(this);
	}
}

Vector RenderObject::getNormal()
{
	float a = MathFunctions::toRadians(getAbsoluteRotation().z);
	return Vector(sinf(a),cosf(a));
}

// HACK: this is probably a slow implementation
Vector RenderObject::getForward()
{
	Vector v = getWorldCollidePosition(Vector(0,-1, 0));
	Vector r = v - getWorldCollidePosition();
	r.normalize2D();

	/*
	std::ostringstream os;
	os << "forward v(" << v.x << ", " << v.y << ") ";
	os << "r(" << r.x << ", " << r.y << ") ";
	debugLog(os.str());
	*/
	return r;
}

Vector RenderObject::getAbsoluteRotation()
{
	Vector r = rotation;
	if (parent)
	{
		return parent->getAbsoluteRotation() + r;
	}
	return r;
}

void RenderObject::onUpdate(float dt)
{
	if (isDead()) return;
	//collisionShape.updatePosition(position);
	updateLife(dt);

	// FIXME: We might not need to do lifetime checks either; I just
	// left that above for safety since I'm not certain.  --achurch
	if (isHidden()) return;

	position += velocity * dt;
	velocity += gravity * dt;
	position.update(dt);
	velocity.update(dt);
	scale.update(dt);
	rotation.update(dt);
	color.update(dt);
	alpha.update(dt);
	offset.update(dt);
	internalOffset.update(dt);
	beforeScaleOffset.update(dt);
	rotationOffset.update(dt);

	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		if (shareAlphaWithChildren)
			(*i)->alpha.x = this->alpha.x;
		if (shareColorWithChildren)
			(*i)->color = this->color;

		if (!(*i)->updateAfterParent && (((*i)->pm == PM_POINTER) || ((*i)->pm == PM_STATIC)))
		{
			(*i)->update(dt);
		}
	}
	
	if (!childGarbage.empty())
	{
		for (Children::iterator i = childGarbage.begin(); i != childGarbage.end(); i++)
		{
			removeChild(*i);
			(*i)->destroy();
			delete (*i);
		}
		childGarbage.clear();
	}

	if (motionBlur)
	{
		if (motionBlurFrameOffsetCounter >= motionBlurFrameOffset)
		{
			motionBlurFrameOffsetCounter = 0;
			motionBlurPositions[0].position = position;
			motionBlurPositions[0].rotz = rotation.z;
			for (int i = motionBlurPositions.size()-1; i > 0; i--)
			{
				motionBlurPositions[i] = motionBlurPositions[i-1];
			}
		}
		else
			motionBlurFrameOffsetCounter ++;
	}
	if (motionBlurTransition)
	{
		motionBlurTransitionTimer -= dt*2;
		if (motionBlurTransitionTimer <= 0)
		{
			motionBlur = motionBlurTransition = false;
			motionBlurTransitionTimer = 0;
		}
	}

//	updateCullVariables();
}

void RenderObject::unloadDevice()
{
	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		(*i)->unloadDevice();
	}
}

void RenderObject::reloadDevice()
{
	for (Children::iterator i = children.begin(); i != children.end(); i++)
	{
		(*i)->reloadDevice();
	}
}

bool RenderObject::setTexture(const std::string &n)
{
	std::string name = n;
	stringToLowerUserData(name);

	if (name.empty())
	{
		setTexturePointer(NULL);
		return false;
	}

	if(texture && name == texture->name)
		return true; // no texture change

	TextureLoadResult res = TEX_FAILED;
	CountedPtr<Texture> tex = core->addTexture(name, &res);
	setTexturePointer(tex);
	return !!tex && res != TEX_FAILED;
}

float RenderObject::getSortDepth()
{
	return position.y;
}

void RenderObject::addChild(RenderObject *r, ParentManaged pm, RenderBeforeParent rbp, ChildOrder order)
{
	if (r->parent)
	{
		errorLog("Engine does not support multiple parents");
		return;
	}

	if (order == CHILD_BACK)
		children.push_back(r);
	else
		children.insert(children.begin(), r);

	r->pm = pm;

	if (rbp == RBP_OFF)
		r->renderBeforeParent = 0;
	else if (rbp == RBP_ON)
		r->renderBeforeParent = 1;

	r->parent = this;
}

StateData *RenderObject::getStateData()
{
	if (parent)
	{
		return parent->getStateData();
	}
	else
		return stateData;
}

void RenderObject::setPositionSnapTo(InterpolatedVector *positionSnapTo)
{
	this->positionSnapTo = positionSnapTo;
}

void RenderObject::setOverrideCullRadius(float ovr)
{
	overrideCullRadiusSqr = ovr * ovr;
}

bool RenderObject::isCoordinateInRadius(const Vector &pos, float r)
{
	Vector d = pos-getRealPosition();
	
	return (d.getSquaredLength2D() < r*r);
}
