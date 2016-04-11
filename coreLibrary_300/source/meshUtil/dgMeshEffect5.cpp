/* Copyright (c) <2003-2011> <Julio Jerez, Newton Game Dynamics>
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

#include "dgPhysicsStdafx.h"
#include "dgBody.h"
#include "dgWorld.h"
#include "dgMeshEffect.h"
#include "dgCollisionConvexHull.h"



bool dgMeshEffect::PlaneClip(const dgMeshEffect& convexMesh, const dgEdge* const convexFace)
{
	dgAssert (convexFace->m_incidentFace > 0);

	dgBigVector normal (convexMesh.FaceNormal(convexFace, &convexMesh.m_points[0].m_x, sizeof(dgBigVector)));
	dgFloat64 mag2 = normal % normal;
	if (mag2 < dgFloat64 (1.0e-30)) {
		dgAssert (0);
		return true;
	}  

	normal = normal.Scale3(dgFloat64 (1.0f) / sqrt (mag2));
	dgBigVector origin (convexMesh.m_points[convexFace->m_incidentVertex]);
	dgBigPlane plane (normal, - (origin % normal));

	dgAssert (!HasOpenEdges());

	dgInt32 pointCount = GetVertexCount();
	dgStack <dgFloat64> testPool (2 * pointCount + 1024);
	dgFloat64* const test = &testPool[0];
	for (dgInt32 i = 0; i < pointCount; i ++) {
		test[i] = plane.Evalue (m_points[i]);
		if (fabs (test[i]) < dgFloat32 (1.0e-5f)) {
			test[i] = dgFloat32 (0.0f);
		}
	}

	dgInt32 positive = 0;
	dgInt32 negative = 0;
	dgPolyhedra::Iterator iter (*this); 
	for (iter.Begin(); iter && !(positive && negative); iter ++){
		dgEdge* const edge = &(*iter);
		positive += test[edge->m_incidentVertex] > dgFloat32 (0.0f);
		negative += test[edge->m_incidentVertex] < dgFloat32 (0.0f);
	}
	if (positive  && !negative) {
		return false;
	}

	if (positive && negative) {

		const dgEdge* e0 = convexFace;
		const dgEdge* e1 = e0->m_next;
		const dgEdge* e2 = e1->m_next;

		dgMatrix matrix;
		dgBigVector p1 (convexMesh.m_points[e1->m_incidentVertex]);

		dgBigVector xDir (p1 - origin);
		dgAssert ((xDir % xDir) > dgFloat32 (0.0f));
		matrix[2] = dgVector (normal);
		matrix[0] = dgVector(xDir.Scale3(dgFloat64 (1.0f) / sqrt (xDir% xDir)));
		matrix[1] = matrix[2] * matrix[0];
		matrix[3] = dgVector (origin);
		matrix[3][3] = dgFloat32 (1.0f);

		dgVector q0 (matrix.UntransformVector(dgVector(convexMesh.m_points[e0->m_incidentVertex])));
		dgVector q1 (matrix.UntransformVector(dgVector(convexMesh.m_points[e1->m_incidentVertex])));
		dgVector q2 (matrix.UntransformVector(dgVector(convexMesh.m_points[e2->m_incidentVertex])));

		dgVector p10 (q1 - q0);
		dgVector p20 (q2 - q0);
		dgVector faceNormal (matrix.UnrotateVector (dgVector(normal)));
		dgFloat32 areaInv = (p10 * p20) % faceNormal;
		if (e2->m_next != e0) {
			const dgEdge* edge = e2;
			dgVector r1 (q2);
			dgVector p10 (p20);
			do {
				dgVector r2 (matrix.UntransformVector(dgVector(convexMesh.m_points[edge->m_next->m_incidentVertex])));
				dgVector p20 (r2 - q0);
				dgFloat32 areaInv1 = (p10 * p20) % faceNormal;
				if (areaInv1 > areaInv) {
					e1 = edge;
					e2 = edge->m_next;
					q1 = r1;
					q2 = r2;
					areaInv = areaInv1;
				}
				r1 = r2;
				p10 = p20;
				edge = edge->m_next;
			} while (edge->m_next != e0);
		}

		dgAssert (areaInv > dgFloat32 (0.0f));
		areaInv = dgFloat32 (1.0f) / areaInv;

		dgVector uv0_0 (dgFloat32 (convexMesh.m_attrib[e0->m_userData].m_u0), dgFloat32 (convexMesh.m_attrib[e0->m_userData].m_v0), dgFloat32 (0.0f), dgFloat32 (0.0f));
		dgVector uv0_1 (dgFloat32 (convexMesh.m_attrib[e1->m_userData].m_u0), dgFloat32 (convexMesh.m_attrib[e1->m_userData].m_v0), dgFloat32 (0.0f), dgFloat32 (0.0f));
		dgVector uv0_2 (dgFloat32 (convexMesh.m_attrib[e2->m_userData].m_u0), dgFloat32 (convexMesh.m_attrib[e2->m_userData].m_v0), dgFloat32 (0.0f), dgFloat32 (0.0f));

		dgVector uv1_0 (dgFloat32 (convexMesh.m_attrib[e0->m_userData].m_u1), dgFloat32 (convexMesh.m_attrib[e0->m_userData].m_v1), dgFloat32 (0.0f), dgFloat32 (0.0f));
		dgVector uv1_1 (dgFloat32 (convexMesh.m_attrib[e1->m_userData].m_u1), dgFloat32 (convexMesh.m_attrib[e1->m_userData].m_v1), dgFloat32 (0.0f), dgFloat32 (0.0f));
		dgVector uv1_2 (dgFloat32 (convexMesh.m_attrib[e2->m_userData].m_u1), dgFloat32 (convexMesh.m_attrib[e2->m_userData].m_v1), dgFloat32 (0.0f), dgFloat32 (0.0f));

		for (iter.Begin(); iter; iter ++){
			dgEdge* const edge = &(*iter);

			dgFloat64 side0 = test[edge->m_prev->m_incidentVertex];
			dgFloat64 side1 = test[edge->m_incidentVertex];

			if ((side0 < dgFloat32 (0.0f)) && (side1 > dgFloat64 (0.0f))) {
				dgBigVector dp (m_points[edge->m_incidentVertex] - m_points[edge->m_prev->m_incidentVertex]);
				dgFloat64 param = - side0 / (plane % dp);

				dgEdge* const splitEdge = InsertEdgeVertex (edge->m_prev, param);
				test[splitEdge->m_next->m_incidentVertex] = dgFloat64 (0.0f);
			} 
		}

		dgInt32 colorMark = IncLRU();
		for (iter.Begin(); iter; iter ++) {
			dgEdge* const edge = &(*iter);
			dgFloat64 side0 = test[edge->m_incidentVertex];
			dgFloat64 side1 = test[edge->m_next->m_incidentVertex];

			if ((side0 > dgFloat32 (0.0f)) || (side1 > dgFloat64 (0.0f))) {
				edge->m_mark = colorMark;
			}
		}

		for (iter.Begin(); iter; iter ++) {
			dgEdge* const edge = &(*iter);
			dgFloat64 side0 = test[edge->m_incidentVertex];
			dgFloat64 side1 = test[edge->m_next->m_incidentVertex];
			if ((side0 == dgFloat32 (0.0f)) && (side1 == dgFloat64 (0.0f))) {
				dgEdge* ptr = edge->m_next;
				do {
					if (ptr->m_mark == colorMark) {
						edge->m_mark = colorMark;
						break;
					}
					ptr = ptr->m_next;
				} while (ptr != edge);
			}
		}


		for (iter.Begin(); iter; iter ++) {
			dgEdge* const edge = &(*iter);
			if ((edge->m_mark == colorMark) && (edge->m_next->m_mark < colorMark)) {
				dgEdge* const startEdge = edge->m_next;
				dgEdge* end = startEdge;
				do {
					if (end->m_mark == colorMark) {
						break;
					}

					end = end->m_next;
				} while (end != startEdge);
				dgAssert (end != startEdge);
				dgEdge* const devideEdge = ConnectVertex (startEdge, end);
				dgAssert (devideEdge);
				dgAssert (devideEdge->m_next->m_mark != colorMark);
				dgAssert (devideEdge->m_prev->m_mark != colorMark);
				dgAssert (devideEdge->m_twin->m_next->m_mark == colorMark);
				dgAssert (devideEdge->m_twin->m_prev->m_mark == colorMark);
				devideEdge->m_mark = colorMark - 1;
				devideEdge->m_twin->m_mark = colorMark;
			}
		}

		dgInt32 mark = IncLRU();
		dgList<dgEdge*> faceList (GetAllocator());
		for (iter.Begin(); iter; iter ++){
			dgEdge* const face = &(*iter);
			if ((face->m_mark >= colorMark) && (face->m_mark != mark)) {
				faceList.Append(face);
				dgEdge* edge = face;
				do {
					edge->m_mark = mark;
					edge = edge->m_next;
				} while (edge != face);
			}
		}

		for (dgList<dgEdge*>::dgListNode* node = faceList.GetFirst(); node; node = node->GetNext()) {
			dgEdge* const face = node->GetInfo();
			DeleteFace(face);
		}

		mark = IncLRU();
		faceList.RemoveAll();
		for (iter.Begin(); iter; iter ++){
			dgEdge* const face = &(*iter);
			if ((face->m_mark != mark) && (face->m_incidentFace < 0)) {
				faceList.Append(face);
				dgEdge* edge = face;
				do {
					edge->m_mark = mark;
					edge = edge->m_next;
				} while (edge != face);
			}
		}

		const dgFloat64 capAttribute = convexMesh.m_attrib[convexFace->m_userData].m_material;
		for (dgList<dgEdge*>::dgListNode* node = faceList.GetFirst(); node; node = node->GetNext()) {
			dgEdge* const face = node->GetInfo();

			dgEdge* edge = face;
			do {
				dgVertexAtribute attibute;
				attibute.m_vertex = m_points[edge->m_incidentVertex];
				attibute.m_normal_x = normal.m_x;
				attibute.m_normal_y = normal.m_y;
				attibute.m_normal_z = normal.m_z;
				attibute.m_material = capAttribute;

				dgVector p (matrix.UntransformVector (attibute.m_vertex));

				dgVector p_p0 (p - q0);
				dgVector p_p1 (p - q1);
				dgVector p_p2 (p - q2);
				//dgFloat32 alpha1 = p10 % p_p0;
				//dgFloat32 alpha2 = p20 % p_p0;
				//dgFloat32 alpha3 = p10 % p_p1;
				//dgFloat32 alpha4 = p20 % p_p1;
				//dgFloat32 alpha5 = p10 % p_p2;
				//dgFloat32 alpha6 = p20 % p_p2;
				//dgFloat32 vc = alpha1 * alpha4 - alpha3 * alpha2;
				//dgFloat32 vb = alpha5 * alpha2 - alpha1 * alpha6;
				//dgFloat32 va = alpha3 * alpha6 - alpha5 * alpha4;
				//dgFloat32 den = va + vb + vc;
				//dgAssert (den > 0.0f);
				//den = dgFloat32 (1.0f) / (va + vb + vc);
				//dgFloat32 alpha0 = dgFloat32 (va * den);
				//alpha1 = dgFloat32 (vb * den);
				//alpha2 = dgFloat32 (vc * den);

				dgFloat32 alpha0 = ((p_p1 * p_p2) % faceNormal) * areaInv;
				dgFloat32 alpha1 = ((p_p2 * p_p0) % faceNormal) * areaInv;
				dgFloat32 alpha2 = ((p_p0 * p_p1) % faceNormal) * areaInv;

				attibute.m_u0 = uv0_0.m_x * alpha0 + uv0_1.m_x * alpha1 + uv0_2.m_x * alpha2; 
				attibute.m_v0 = uv0_0.m_y * alpha0 + uv0_1.m_y * alpha1 + uv0_2.m_y * alpha2; 
				attibute.m_u1 = uv1_0.m_x * alpha0 + uv1_1.m_x * alpha1 + uv1_2.m_x * alpha2; 
				attibute.m_v1 = uv1_0.m_y * alpha0 + uv1_1.m_y * alpha1 + uv1_2.m_y * alpha2; 

				AddAtribute (attibute);
				edge->m_incidentFace = 1;
				edge->m_userData = m_atribCount - 1;

				//faceIndices[indexCount] = edge->m_incidentVertex;
				//indexCount ++;
				//dgAssert (indexCount < sizeof (faceIndices) / sizeof (faceIndices[0]));

				edge = edge->m_next;
			} while (edge != face);

			//facePolygedra.AddFace(indexCount, faceIndices);
			//facePolygedra.EndFace();

			//dgPolyhedra leftOversOut(GetAllocator());
			//facePolygedra.ConvexPartition (&m_points[0].m_x, sizeof (dgBigVector), &leftOversOut);
			//dgAssert (leftOversOut.GetCount() == 0);
		}
	}

	return true;
}


dgMeshEffect* dgMeshEffect::ConvexMeshIntersection (const dgMeshEffect* const convexMeshSrc) const
{
	dgMeshEffect convexMesh (*convexMeshSrc);
	convexMesh.ConvertToPolygons();
	//return new (GetAllocator()) dgMeshEffect (*convexMesh);

	dgMeshEffect* const convexIntersection = new (GetAllocator()) dgMeshEffect (*this);
	//convexIntersection->ConvertToPolygons();
	//convexIntersection->Triangulate();
	convexIntersection->RemoveUnusedVertices(NULL);

	dgInt32 mark = convexMesh.IncLRU();
	dgPolyhedra::Iterator iter (convexMesh);

	for (iter.Begin(); iter; iter ++){
		dgEdge* const convexFace = &(*iter);
		if ((convexFace->m_incidentFace > 0) && (convexFace->m_mark != mark)) {
			dgEdge* ptr = convexFace;
			do {
				ptr->m_mark = mark;
				ptr = ptr->m_next;
			} while (ptr != convexFace);
			if (!convexIntersection->PlaneClip(convexMesh, convexFace)) {
				delete convexIntersection;
				return NULL;
			}
		}
	}

	convexIntersection->RemoveUnusedVertices(NULL);
	if (!convexIntersection->GetVertexCount()) {
		delete convexIntersection;
		return NULL;
	}

	convexIntersection->RemoveUnusedVertices(NULL);

	return convexIntersection;
}


void dgMeshEffect::ClipMesh (const dgMatrix& matrix, const dgMeshEffect* const clipMesh, dgMeshEffect** const back, dgMeshEffect** const front) const
{
	dgAssert (0);
/*
	dgMeshEffect clipper (*clipMesh);
	clipper.TransformMesh (matrix);

	dgMeshEffect* backMeshSource = NULL;
	dgMeshEffect* frontMeshSource = NULL;
	dgMeshEffect* backMeshClipper = NULL;
	dgMeshEffect* frontMeshClipper = NULL;

	ClipMesh (&clipper, &backMeshSource, &frontMeshSource);
	if (backMeshSource && frontMeshSource) {
		clipper.ClipMesh (this, &backMeshClipper, &frontMeshClipper);
		if (backMeshSource && frontMeshSource) {

			dgMeshEffect* backMesh;
			dgMeshEffect* frontMesh;

			backMesh = new (GetAllocator()) dgMeshEffect (GetAllocator(), true);
			frontMesh = new (GetAllocator()) dgMeshEffect (GetAllocator(), true);

			backMesh->BeginPolygon();
			frontMesh->BeginPolygon();

			backMesh->MergeFaces(backMeshSource);
			backMesh->MergeFaces(backMeshClipper);

			frontMesh->MergeFaces(frontMeshSource);
			frontMesh->ReverseMergeFaces(backMeshClipper);

			backMesh->EndPolygon(dgFloat64 (1.0e-5f));
			frontMesh->EndPolygon(dgFloat64 (1.0e-5f));

			*back = backMesh;
			*front = frontMesh;
		}
	}

	if (backMeshClipper) {
		delete backMeshClipper;
	}

	if (frontMeshClipper) {
		delete frontMeshClipper;
	}

	if (backMeshSource) {
		delete backMeshSource;
	}

	if (frontMeshSource) {
		delete frontMeshSource;
	}
*/
}




dgMeshEffect* dgMeshEffect::Union (const dgMatrix& matrix, const dgMeshEffect* const clipperMesh) const
{
	dgAssert (0);
	return NULL;
/*
	dgMeshEffect copy (*this);
	dgMeshEffect clipper (*clipperMesh);
	clipper.TransformMesh (matrix);

	dgBooleanMeshClipper::ClipMeshesAndColorize (&copy, &clipper);

	dgMeshEffect* const mesh = new (GetAllocator()) dgMeshEffect (GetAllocator());
	mesh->BeginFace();
	dgBooleanMeshClipper::CopyPoints(mesh, &copy);
	dgBooleanMeshClipper::AddExteriorFaces (mesh, &copy);

	dgBooleanMeshClipper::AddExteriorFaces (mesh, &clipper);
	mesh->EndFace ();
	mesh->RepairTJoints();
	mesh->RemoveUnusedVertices(NULL);
	return mesh;
*/
}

dgMeshEffect* dgMeshEffect::Difference (const dgMatrix& matrix, const dgMeshEffect* const clipperMesh) const
{
/*
	dgMeshEffect copy (*this);
	dgMeshEffect clipper (*clipperMesh);
	clipper.TransformMesh (matrix);

	dgBooleanMeshClipper::ClipMeshesAndColorize (&copy, &clipper);

	dgMeshEffect* const mesh = new (GetAllocator()) dgMeshEffect (GetAllocator());
	mesh->BeginFace();
	dgBooleanMeshClipper::CopyPoints(mesh, &copy);
	dgBooleanMeshClipper::AddExteriorFaces (mesh, &copy);
	dgBooleanMeshClipper::AddInteriorFacesInvertWinding (mesh, &clipper);
	mesh->EndFace ();
	mesh->RepairTJoints();
	mesh->RemoveUnusedVertices(NULL);
	return mesh;
*/

	dgAssert (0);
	return NULL;
}


class dgBooleanMeshClipper: public dgMeshEffect::dgMeshBVH
{

	class dgNodeKey
	{
		public:
		dgNodeKey()
		{
		}

		dgNodeKey(const dgEdge* const edge, dgFloat64 param)
			:m_param (param)
			,m_edgeKey((edge->m_incidentVertex < edge->m_twin->m_incidentVertex) ? edge : edge->m_twin)
		{
			if (m_edgeKey != edge) {
				m_param = 1.0f - m_param;
			}
		}

		dgInt32 operator> (const dgNodeKey& key) const
		{
			if (key.m_edgeKey > m_edgeKey) {
				return 1;
			} else if (key.m_edgeKey == m_edgeKey) {
				return (key.m_param > m_param) ? 1 : 0;
			}
			return 0;
		}

		dgInt32 operator< (const dgNodeKey& key) const
		{
			if (key.m_edgeKey < m_edgeKey) {
				return 1;
			} else if (key.m_edgeKey == m_edgeKey) {
				return (key.m_param < m_param) ? 1 : 0;
			}
			return 0;
		}

		dgFloat64 m_param;
		const dgEdge* m_edgeKey;
	};

	class dgPoint
	{
		public:
		dgPoint()
			:m_links(NULL)
		{
			dgAssert (0);
		}

		dgPoint(dgMeshEffect* const edgeOwnerMesh, dgEdge* const edge, dgFloat64 param, dgMeshEffect* const faceOwnerMesh, dgEdge* const face)
			:m_links(edgeOwnerMesh->GetAllocator())
//			:m_edge(edge)
//			,m_face(face)
//			,m_edgeOwnerMesh(edgeOwnerMesh)
//			,m_faceOwnerMesh(faceOwnerMesh)
//			,m_indexA(0)
//			,m_indexB(0)
//			,m_lru(0)
		{
//			dgBigVector p0(m_edgeOwnerMesh->GetVertex(m_edge->m_incidentVertex));
//			dgBigVector p1(m_edgeOwnerMesh->GetVertex(m_edge->m_twin->m_incidentVertex));
//			m_posit = p0 + (p1 - p0).Scale3 (param);
		}

		dgBigVector m_posit;
		dgList<dgTree<dgPoint, dgNodeKey>::dgTreeNode*> m_links;
//		dgEdge* m_edge;
//		dgEdge* m_face;
//		dgMeshEffect* m_edgeOwnerMesh;
//		dgMeshEffect* m_faceOwnerMesh;
//		dgInt32 m_indexA;
//		dgInt32 m_indexB;
//		dgInt32 m_lru;
	};

	class dgCurvesNetwork: public dgTree<dgPoint, dgNodeKey>
	{
		public:
		dgCurvesNetwork ()
			:dgTree<dgPoint, dgNodeKey>(NULL)
		{
			dgAssert (0);
		}

		dgCurvesNetwork (dgMemoryAllocator* const allocator)
			:dgTree<dgPoint, dgNodeKey>(allocator)
		{
		}

		dgCurvesNetwork(dgBooleanMeshClipper* const BVHmeshA, dgBooleanMeshClipper* const BVHmeshB)
			:dgTree<dgPoint, dgNodeKey>(BVHmeshA->m_mesh->GetAllocator())
//			,m_meshA(BVHmeshA->m_mesh)
//			,m_meshB(BVHmeshB->m_mesh)
//			,m_pointBaseA(m_meshA->GetVertexCount()) 
//			,m_pointBaseB(m_meshB->GetVertexCount()) 
//			,m_lru(0)
		{
		}
/*
		dgHugeVector CalculateFaceNormal (const dgMeshEffect* const mesh, dgEdge* const face)
		{
			dgHugeVector plane(dgGoogol::m_zero, dgGoogol::m_zero, dgGoogol::m_zero, dgGoogol::m_zero);
			dgEdge* edge = face;
			dgHugeVector p0(mesh->GetVertex(edge->m_incidentVertex));
			edge = edge->m_next;
			dgHugeVector p1(mesh->GetVertex(edge->m_incidentVertex));
			dgHugeVector p1p0(p1 - p0);
			edge = edge->m_next;
			do {
				dgHugeVector p2(mesh->GetVertex(edge->m_incidentVertex));
				dgHugeVector p2p0(p2 - p0);
				plane += p2p0 * p1p0;
				p1p0 = p2p0;
				edge = edge->m_next;
			} while (edge != face);

			plane.m_w = dgGoogol::m_zero - (plane % p0);
			return plane;
		}


		bool IsPointInFace (const dgHugeVector& point, const dgMeshEffect* const mesh, dgEdge* const face, const dgHugeVector& normal) const 
		{
			dgEdge* edge = face;

			dgTrace (("%f %f %f\n", dgFloat64 (point.m_x), dgFloat64 (point.m_y), dgFloat64 (point.m_z)));
			do {
				dgBigVector p1(mesh->GetVertex(edge->m_incidentVertex));
				dgTrace (("%f %f %f\n", dgFloat64 (p1.m_x), dgFloat64 (p1.m_y), dgFloat64 (p1.m_z)));
				edge = edge->m_next;
			} while (edge != face);

			dgHugeVector p0(mesh->GetVertex(face->m_incidentVertex));
			do {
				dgHugeVector p1(mesh->GetVertex(edge->m_twin->m_incidentVertex));
				dgHugeVector p1p0(p1 - p0);
				dgHugeVector q1p0(point - p0);
				dgGoogol side (q1p0 % (normal * p1p0));
				if (side >= dgGoogol::m_zero) {
					return false;
				}
				p0 = p1;
				edge = edge->m_next;
			} while (edge != face);

			return true;
		}

		dgFloat64 ClipEdgeFace(const dgMeshEffect* const meshEdge, dgEdge* const edge, const dgMeshEffect* const meshFace, dgEdge* const face, const dgHugeVector& plane)
		{
			dgHugeVector p0 (meshEdge->GetVertex(edge->m_incidentVertex));
			dgHugeVector p1 (meshEdge->GetVertex(edge->m_twin->m_incidentVertex));
			
			dgGoogol test0 (plane.EvaluePlane(p0));
			dgGoogol test1 (plane.EvaluePlane(p1));

			if ((test0 * test1) > dgGoogol::m_zero) {
				// both point are in one side
				return -1.0f;
			}

			if ((test0 * test1) < dgGoogol::m_zero) {
				//point on different size, clip the line
				dgHugeVector p1p0 (p1 - p0);
				dgGoogol param = dgGoogol::m_zero - plane.EvaluePlane(p0) / (plane % p1p0);
				dgHugeVector p (p0 + p1p0.Scale3 (param));
				if (IsPointInFace (p, meshFace, face, plane)) {
					return param;
				}
				return -1.0f;
			} else {
				dgAssert (0);
				//special cases;
			}
			
			return -1.0f;
		}

		void AddPoint (dgMeshEffect* const edgeOwnerMesh, dgEdge* const edgeStart, dgMeshEffect* const faceOwnerMesh, dgEdge* const face, const dgHugeVector& plane, dgTreeNode** nodes, dgInt32& index)
		{
			dgEdge* edge = edgeStart;
			do {
				dgFloat64 param = ClipEdgeFace(edgeOwnerMesh, edge, faceOwnerMesh, face, plane);
				if (param > 0.0f) {
					dgPoint point(edgeOwnerMesh, edge, param, faceOwnerMesh, face);
					dgTreeNode* node = Find(dgNodeKey(edge, param));
					if (!node) {
						node = Insert(point, dgNodeKey(edge, param));
					}
					nodes[index] = node;
					index ++;
				}
				edge = edge->m_next;
			} while (edge != edgeStart);

		}

		void ClipMeshesFaces(dgEdge* const faceA, dgEdge* const faceB)
		{
			dgAssert (m_meshA->FindEdge(faceA->m_incidentVertex, faceA->m_twin->m_incidentVertex) == faceA);
			dgAssert (m_meshB->FindEdge(faceB->m_incidentVertex, faceB->m_twin->m_incidentVertex) == faceB);

			dgHugeVector planeA (CalculateFaceNormal (m_meshA, faceA));
			dgHugeVector planeB (CalculateFaceNormal (m_meshB, faceB));

			dgInt32 index = 0;
			dgTreeNode* nodes[16];
			AddPoint (m_meshA, faceA, m_meshB, faceB, planeB, nodes, index);
			AddPoint (m_meshB, faceB, m_meshA, faceA, planeA, nodes, index);
			dgAssert ((index == 0) || (index == 2));
			if (index == 2) {
				dgPoint& pointA = nodes[0]->GetInfo();
				dgPoint& pointB = nodes[1]->GetInfo();
				pointA.m_links.Append(nodes[1]);
				pointB.m_links.Append(nodes[0]);
			}
		}

		void GetCurve (dgList<dgTreeNode*>& curve, dgTreeNode* const node)
		{
			dgInt32 stack = 1;
			dgTreeNode* pool[64];

			pool[0] = node;
			while (stack) {
				stack --;
				dgTreeNode* const ptr = pool[stack];
				dgPoint& point = ptr->GetInfo();
				if (point.m_lru != m_lru) {
					point.m_lru = m_lru;
					curve.Append(ptr);
					for (dgList<dgTree<dgPoint, dgNodeKey>::dgTreeNode*>::dgListNode* ptrPoint = point.m_links.GetFirst(); ptrPoint; ptrPoint = ptrPoint->GetNext()) {
						dgTreeNode* const nextnode = ptrPoint->GetInfo();
						dgPoint& nextPoint = nextnode->GetInfo();
						if (nextPoint.m_lru != m_lru) {
							pool[stack] = nextnode;
							stack ++;
						}
					}
				}
			}
		}

		void EmbedCurveToSingleFace (dgList<dgTreeNode*>& curve, dgMeshEffect* const mesh)
		{
			dgEdge* const face = curve.GetFirst()->GetInfo()->GetInfo().m_face;

			dgInt32 indexBase = mesh->GetVertexCount();
			dgInt32 indexAttribBase = mesh->GetPropertiesCount();

			for (dgList<dgTreeNode*>::dgListNode* node = curve.GetFirst(); node; node = node->GetNext()) {
				dgPoint& point = node->GetInfo()->GetInfo();
				dgAssert (point.m_face == face);
				dgMeshEffect::dgVertexAtribute attribute(mesh->InterpolateVertex(point.m_posit, face));
				mesh->AddVertex(point.m_posit);
				mesh->AddAtribute(attribute);
			}

			dgList<dgEdge*> list(GetAllocator());
			dgInt32 i0 = curve.GetCount() - 1;
			for (dgInt32 i = 0; i < curve.GetCount(); i++) {
				dgEdge* const edge = mesh->AddHalfEdge(indexBase + i0, indexBase + i);
				dgEdge* const twin = mesh->AddHalfEdge(indexBase + i, indexBase + i0);

				edge->m_incidentFace = 1;
				twin->m_incidentFace = 1;
				edge->m_userData = indexAttribBase + i0;
				twin->m_userData = indexAttribBase + i;
				twin->m_twin = edge;
				edge->m_twin = twin;
				i0 = i;
				list.Append(edge);
			}

			dgEdge* closestEdge = NULL;
			dgFloat64 dist2 = dgFloat64 (1.0e10f);
			dgBigVector p(mesh->GetVertex(face->m_incidentVertex));

			list.Append(list.GetFirst()->GetInfo());
			list.Addtop(list.GetLast()->GetInfo());
			for (dgList<dgEdge*>::dgListNode* node = list.GetFirst()->GetNext(); node != list.GetLast(); node = node->GetNext()) {
				dgEdge* const edge = node->GetInfo();

				dgEdge* const prev = node->GetPrev()->GetInfo();
				edge->m_prev = prev;
				prev->m_next = edge;
				edge->m_twin->m_next = prev->m_twin;
				prev->m_twin->m_prev = edge->m_twin;

				dgEdge* const next = node->GetNext()->GetInfo();
				edge->m_next = next;
				next->m_prev = edge;
				edge->m_twin->m_prev = next->m_twin;
				next->m_twin->m_next = edge->m_twin;

				dgBigVector dist(mesh->GetVertex(edge->m_incidentVertex) - p);
				dgFloat64 err2 = dist % dist;
				if (err2 < dist2) {
					closestEdge = edge;
					dist2 = err2;
				}
			}

			dgBigVector faceNormal (mesh->FaceNormal(face, mesh->GetVertexPool(), mesh->GetVertexStrideInByte()));
			dgBigVector clipNormal (mesh->FaceNormal(closestEdge, mesh->GetVertexPool(), mesh->GetVertexStrideInByte()));
			if ((clipNormal % faceNormal) > dgFloat64(0.0f)) {
				closestEdge = closestEdge->m_twin->m_next;
			}
			dgEdge* const glueEdge = mesh->ConnectVertex (closestEdge, face);
			dgAssert (glueEdge);
			mesh->PolygonizeFace(glueEdge, mesh->GetVertexPool(), sizeof (dgBigVector));
		}

		void EmbedCurveToMulipleFaces (dgList<dgTreeNode*>& curve, dgMeshEffect* const mesh)
		{
			for (dgList<dgTreeNode*>::dgListNode* node = curve.GetFirst(); node; node = node->GetNext()) {
				dgPoint& point = node->GetInfo()->GetInfo();
				if (point.m_edgeOwnerMesh == mesh) {
					dgEdge* const edge = point.m_edge;
					dgBigVector p0 (mesh->GetVertex(edge->m_incidentVertex));
					dgBigVector p1 (mesh->GetVertex(edge->m_twin->m_incidentVertex));
					dgVector p1p0 (p1 - p0);
					dgVector qp0 (point.m_posit - p0);
					dgFloat64 param = (qp0 % p1p0) / (p1p0 % p1p0);
					dgAssert (param >= dgFloat64 (0.0f));
					dgAssert (param <= dgFloat64 (1.0f));
					dgEdge* const newEdge = mesh->InsertEdgeVertex (edge, param);
				}
//				mesh->AddVertex(point.m_posit);
//				mesh->AddAtribute(attribute);
			}
		}


		void AddCurveToMesh (dgList<dgTreeNode*>& curve, dgMeshEffect* const mesh)
		{
			bool isIscribedInFace = true; 
			dgEdge* const face = curve.GetFirst()->GetInfo()->GetInfo().m_face;
			for (dgList<dgTreeNode*>::dgListNode* node = curve.GetFirst(); isIscribedInFace && node; node = node->GetNext()) {
				dgPoint& point = node->GetInfo()->GetInfo();
				isIscribedInFace = isIscribedInFace && (point.m_face == face);
				isIscribedInFace = isIscribedInFace && (point.m_faceOwnerMesh == mesh);
			}

			if (isIscribedInFace) {
				EmbedCurveToSingleFace (curve, mesh);
			} else {
				EmbedCurveToMulipleFaces (curve, mesh);
			}
		}

		void Colorize()
		{
			m_lru ++;
			Iterator iter (*this);
			for (iter.Begin(); iter; iter ++) {
				dgPoint& point = iter.GetNode()->GetInfo();
				if (point.m_lru != m_lru) {
					dgList<dgTreeNode*> curve (GetAllocator());
					GetCurve (curve, iter.GetNode());
					AddCurveToMesh (curve, m_meshB);
					AddCurveToMesh (curve, m_meshA);
				}
			}

			m_meshA->SaveOFF("xxxA0.off");
			m_meshB->SaveOFF("xxxB0.off");
		}

		dgMeshEffect* m_meshA;
		dgMeshEffect* m_meshB;
		dgInt32 m_pointBaseA;
		dgInt32 m_pointBaseB;
		dgInt32 m_lru;
*/
	};


	class dgClippedFace: public dgMeshEffect
	{
		public:
		dgClippedFace ()
			:dgMeshEffect()
			,m_curveNetwork()
		{
			dgAssert (0);
		}

		dgClippedFace (dgMemoryAllocator* const allocator)
			:dgMeshEffect(allocator)
			,m_curveNetwork(allocator)
		{
		}

		dgClippedFace (const dgClippedFace& copy)
			:dgMeshEffect(copy)
			,m_curveNetwork(copy.m_curveNetwork)
		{
		}

		void InitFace(dgMeshEffect* const mesh, dgEdge* const face)
		{
			dgInt32 indexCount = 0;
			dgInt32 faceIndex[256];
			dgInt64 faceDataIndex[256];
			BeginFace ();
			dgEdge* ptr = face;
			do {
				const dgMeshEffect::dgVertexAtribute& point =  mesh->GetAttribute(dgInt32 (ptr->m_userData));
				AddPoint (&point.m_vertex.m_x, dgInt32 (point.m_material));
				faceIndex[indexCount] = indexCount;
				faceDataIndex[indexCount] = indexCount;
				indexCount ++;
				ptr = ptr->m_next;
			} while (ptr != face);
			AddFace (indexCount, faceIndex, faceDataIndex);
			EndFace ();
		}

		dgCurvesNetwork m_curveNetwork;
	};

	class dgClipppedFaces: public dgTree<dgClippedFace, dgEdge*>
	{
		public:
		dgClipppedFaces(dgMeshEffect* const mesh)
			:dgTree<dgClippedFace, dgEdge*>(mesh->GetAllocator())
			,m_parentMesh (mesh)
		{
		}

		void ClipMeshesFaces(dgMeshEffect* const meshA, dgEdge* const faceA, dgMeshEffect* const meshB, dgEdge* const faceB)
		{
			dgAssert (meshA == m_parentMesh);
			dgTreeNode* node = Find (faceA);
			if (!node) {
				dgClippedFace tmp (m_parentMesh->GetAllocator());
				node = Insert (tmp, faceA);
				dgClippedFace& faceHead = node->GetInfo();
				faceHead.InitFace (m_parentMesh, faceA);
			}
		}

		dgMeshEffect* m_parentMesh;
	};
	

	public:
	dgBooleanMeshClipper(dgMeshEffect* const mesh)
		:dgMeshBVH(mesh)
		,m_clippedFaces(mesh)
	{
		dgMeshBVH::Build();
	}

	~dgBooleanMeshClipper()
	{
	}

	static void ClipMeshesAndColorize(dgMeshEffect* const meshA, dgMeshEffect* const meshB)
	{
		dgBooleanMeshClipper BVHmeshA(meshA);
		dgBooleanMeshClipper BVHmeshB(meshB);

//		dgCurvesNetwork network(&BVHmeshA, &BVHmeshB);

		int stack = 1;
		dgMeshBVHNode* stackPool[2 * DG_MESH_EFFECT_BVH_STACK_DEPTH][2];

		stackPool[0][0] = BVHmeshA.m_rootNode;
		stackPool[0][1] = BVHmeshB.m_rootNode;
		while (stack) {
			stack --;
			dgMeshBVHNode* const nodeA = stackPool[stack][0];
			dgMeshBVHNode* const nodeB = stackPool[stack][1];
			if (dgOverlapTest (nodeA->m_p0, nodeA->m_p1, nodeB->m_p0, nodeB->m_p1)) {
				if (nodeA->m_face && nodeB->m_face) {
					BVHmeshA.m_clippedFaces.ClipMeshesFaces(meshA, nodeA->m_face, meshB, nodeB->m_face);
					BVHmeshB.m_clippedFaces.ClipMeshesFaces(meshB, nodeB->m_face, meshA, nodeA->m_face);
				} else if (nodeA->m_face) {
					stackPool[stack][0] = nodeA;
					stackPool[stack][1] = nodeB->m_left;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));

					stackPool[stack][0] = nodeA;
					stackPool[stack][1] = nodeB->m_right;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));
					
				} else if (nodeB->m_face) {
					stackPool[stack][0] = nodeA->m_left;
					stackPool[stack][1] = nodeB;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));

					stackPool[stack][0] = nodeA->m_right;
					stackPool[stack][1] = nodeB;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));

				} else {
					stackPool[stack][0] = nodeA->m_left;
					stackPool[stack][1] = nodeB->m_left;
					stack ++;
					dgAssert (stack < sizeof (stackPool) / sizeof (stackPool[0]));

					stackPool[stack][0] = nodeA->m_left;
					stackPool[stack][1] = nodeB->m_right;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));

					stackPool[stack][0] = nodeA->m_right;
					stackPool[stack][1] = nodeB->m_left;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));

					stackPool[stack][0] = nodeA->m_right;
					stackPool[stack][1] = nodeB->m_right;
					stack++;
					dgAssert(stack < sizeof (stackPool) / sizeof (stackPool[0]));
				}
			}
		}

		dgAssert (0);
//		network.Colorize();
	

/*
		dgInt32 baseAttibuteCountB = BVHmeshB.m_mesh->GetPropertiesCount();

		BVHmeshA.m_mesh->SaveOFF("xxxA0.off");
		BVHmeshB.m_mesh->SaveOFF("xxxB0.off");

		// edge-face, edge-edge and edge-vertex intersections until not more intersections are found 
		for (bool intersectionFound = true; intersectionFound;) {
			intersectionFound = false;

			intersectionFound |= BVHmeshA.CalculateEdgeFacesIntersetions(BVHmeshB);
			intersectionFound |= BVHmeshB.CalculateEdgeFacesIntersetions(BVHmeshA);

			intersectionFound |= BVHmeshA.CalculateVertexFacesIntersetions(BVHmeshB);
			intersectionFound |= BVHmeshB.CalculateVertexFacesIntersetions(BVHmeshA);


			BVHmeshA.m_mesh->SaveOFF("xxxA1.off");
			BVHmeshB.m_mesh->SaveOFF("xxxB1.off");

			intersectionFound |= BVHmeshA.CalculateEdgeEdgeIntersetions(BVHmeshB);

			BVHmeshA.m_mesh->SaveOFF("xxxA2.off");
			BVHmeshB.m_mesh->SaveOFF("xxxB2.off");

			intersectionFound |= BVHmeshA.CalculateEdgeVertexIntersetions(BVHmeshB);
			intersectionFound |= BVHmeshB.CalculateEdgeVertexIntersetions(BVHmeshA);

			BVHmeshA.m_mesh->SaveOFF("xxxA3.off");
			BVHmeshB.m_mesh->SaveOFF("xxxB3.off");
		}
*/		
	}

	dgClipppedFaces m_clippedFaces;
};



dgMeshEffect* dgMeshEffect::Intersection (const dgMatrix& matrix, const dgMeshEffect* const clipperMesh) const
{
	dgMeshEffect copy (*this);
	dgMeshEffect clipper (*clipperMesh);
	clipper.TransformMesh (matrix);

	dgBooleanMeshClipper::ClipMeshesAndColorize (&copy, &clipper);
/*
	dgMeshEffect* const mesh = new (GetAllocator()) dgMeshEffect (GetAllocator());
	mesh->BeginFace();
	dgBooleanMeshClipper::CopyPoints(mesh, &copy);
	dgBooleanMeshClipper::AddInteriorFaces (mesh, &copy);
	dgBooleanMeshClipper::AddInteriorFaces (mesh, &clipper);
	mesh->EndFace ();
	mesh->RepairTJoints();
	mesh->RemoveUnusedVertices(NULL);

	return mesh;
*/

	dgAssert (0);
	return NULL;
}


