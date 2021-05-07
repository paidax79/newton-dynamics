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

#include "dCoreStdafx.h"
#include "ndCollisionStdafx.h"
#include "ndShapeInstance.h"
#include "ndContactSolver.h"
#include "ndCollisionStdafx.h"
#include "ndShapeCompoundConvex.h"


ndShapeCompoundConvex::ndShapeCompoundConvex()
	:ndShape(m_compoundConvex)
{
	dAssert(0);
}

ndShapeCompoundConvex::~ndShapeCompoundConvex()
{
	dAssert(0);
}

void ndShapeCompoundConvex::CalcAABB(const dMatrix& matrix, dVector &p0, dVector &p1) const
{
	dAssert(0);
	dVector origin(matrix.TransformVector(m_boxOrigin));
	dVector size(matrix.m_front.Abs().Scale(m_boxSize.m_x) + matrix.m_up.Abs().Scale(m_boxSize.m_y) + matrix.m_right.Abs().Scale(m_boxSize.m_z));

	p0 = (origin - size) & dVector::m_triplexMask;
	p1 = (origin + size) & dVector::m_triplexMask;
}


//dInt32 ndShapeCompoundConvex::CalculatePlaneIntersection(const dFloat32* const vertex, const dInt32* const index, dInt32 indexCount, dInt32 stride, const dPlane& localPlane, dVector* const contactsOut) const
dInt32 ndShapeCompoundConvex::CalculatePlaneIntersection(const dFloat32* const, const dInt32* const, dInt32, dInt32, const dPlane&, dVector* const) const
{
	dAssert(0);
	return 0;
	//dInt32 count = 0;
	//dInt32 j = index[indexCount - 1] * stride;
	//dVector p0(&vertex[j]);
	//p0 = p0 & dVector::m_triplexMask;
	//dFloat32 side0 = localPlane.Evalue(p0);
	//for (dInt32 i = 0; i < indexCount; i++) {
	//	j = index[i] * stride;
	//	dVector p1(&vertex[j]);
	//	p1 = p1 & dVector::m_triplexMask;
	//	dFloat32 side1 = localPlane.Evalue(p1);
	//
	//	if (side0 < dFloat32(0.0f)) {
	//		if (side1 >= dFloat32(0.0f)) {
	//			dVector dp(p1 - p0);
	//			dAssert(dp.m_w == dFloat32(0.0f));
	//			dFloat32 t = localPlane.DotProduct(dp).GetScalar();
	//			dAssert(dgAbs(t) >= dFloat32(0.0f));
	//			if (dgAbs(t) < dFloat32(1.0e-8f)) {
	//				t = dgSign(t) * dFloat32(1.0e-8f);
	//			}
	//			dAssert(0);
	//			contactsOut[count] = p0 - dp.Scale(side0 / t);
	//			count++;
	//
	//		}
	//	}
	//	else if (side1 <= dFloat32(0.0f)) {
	//		dVector dp(p1 - p0);
	//		dAssert(dp.m_w == dFloat32(0.0f));
	//		dFloat32 t = localPlane.DotProduct(dp).GetScalar();
	//		dAssert(dgAbs(t) >= dFloat32(0.0f));
	//		if (dgAbs(t) < dFloat32(1.0e-8f)) {
	//			t = dgSign(t) * dFloat32(1.0e-8f);
	//		}
	//		dAssert(0);
	//		contactsOut[count] = p0 - dp.Scale(side0 / t);
	//		count++;
	//	}
	//
	//	side0 = side1;
	//	p0 = p1;
	//}
	//
	//return count;
}
