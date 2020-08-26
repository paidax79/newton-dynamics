/* Copyright (c) <2003-2019> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#ifndef __D_SHAPE_NULL_H__ 
#define __D_SHAPE_NULL_H__ 

#include "dShapeConvex.h"

class dShapeNull: public dShapeConvex
{
	public:
	D_NEWTON_API dShapeNull();
	//dShapeNull(dUnsigned32 signature);
	//dShapeNull(dgWorld* const world, dgDeserialize deserialization, void* const userData, dgInt32 revisionNumber);
	virtual ~dShapeNull() {}

	virtual dShapeNull* GetAsShapeNull() { return this; }
/*
	protected:
	virtual dgFloat32 GetVolume () const;
	virtual void CalcAABB (const dgMatrix& matrix, dgVector& p0, dgVector& p1) const;
	
	virtual dgVector SupportVertex (const dgVector& dir, dgInt32* const vertexIndex) const;
	virtual dgVector SupportVertexSpecial (const dgVector& dir, dgFloat32 skinThickness, dgInt32* const vertexIndex) const;

	virtual void DebugCollision  (const dgMatrix& matrix, dgCollision::OnDebugCollisionMeshCallback callback, void* const userData) const;
	
	virtual dgVector CalculateVolumeIntegral (const dgMatrix& globalMatrix, const dgVector& plane, const dgCollisionInstance& parentScale) const;
	virtual dgFloat32 RayCast (const dgVector& localP0, const dgVector& localP1, dgFloat32 maxT, dgContactPoint& contactOut, const dgBody* const body, void* const userData, OnRayPrecastAction preFilter) const;

	private:
	virtual dgInt32 CalculateSignature () const;
	virtual void SetCollisionBBox (const dgVector& p0, const dgVector& p1);

	virtual void Serialize(dgSerialize callback, void* const userData) const;
	friend class dgWorld;
*/
};


#endif 

