/* Copyright (c) <2003-2022> <Julio Jerez, Newton Game Dynamics>
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

#include "ndCoreStdafx.h"
#include "ndNewtonStdafx.h"
#include "ndWorld.h"
#include "ndModel.h"
#include "ndBodyDynamic.h"
#include "ndModelArticulation.h"

ndModelArticulation::ndNode::ndNode(const ndSharedPtr<ndBody>& body, const ndSharedPtr<ndJointBilateralConstraint>& joint, ndNode* const parent)
	:ndNodeHierarchy<ndNode>()
	,m_body(body)
	,m_joint(joint)
	,m_name("")
{
	if (parent)
	{
		Attach(parent);
	}
}

ndModelArticulation::ndNode::~ndNode()
{
}

ndModelArticulation::ndModelArticulation()
	:ndModel()
	,m_rootNode(nullptr)
	//,m_invDynamicsSolver()
	,m_closeLoops()
	,m_name("")
{
}

ndModelArticulation::ndModelArticulation(const ndModelArticulation& src)
	:ndModel(src)
	,m_rootNode(nullptr)
	//,m_invDynamicsSolver()
	,m_closeLoops()
	,m_name(src.m_name)
{
	ndAssert(0);
	ndAssert(src.GetRoot()->m_body->GetAsBodyDynamic());

	ndFixSizeArray<ndNode*, 256> stack;
	stack.PushBack(src.GetRoot());
	while (stack.GetCount())
	{
		ndNode* const node = stack.Pop();
		//ndBodyDynamic* xxx = node->m_body->GetAsBodyDynamic();
		//ndBodyDynamic* xxx1 = new ndBodyDynamic(*xxx);
		AddRootBody(new ndBodyDynamic(*node->m_body->GetAsBodyDynamic()));
		if (*node->m_joint)
		{

		}

		for (ndNode* child = node->GetFirstChild(); child; child = child->GetNext())
		{
			stack.PushBack(child);
		}
	}
}

ndModelArticulation::~ndModelArticulation()
{
	if (m_rootNode)
	{
		delete m_rootNode;
	}
}

ndModel* ndModelArticulation::Clone() const
{
	return new ndModelArticulation(*this);
}

ndModelArticulation* ndModelArticulation::GetAsModelArticulation()
{
	return this;
}

const ndString& ndModelArticulation::GetName() const
{
	return m_name;
}

void ndModelArticulation::SetName(const ndString& name)
{
	m_name = name;
}

ndModelArticulation::ndNode* ndModelArticulation::GetRoot() const
{
	return m_rootNode;
}

ndModelArticulation::ndNode* ndModelArticulation::AddRootBody(const ndSharedPtr<ndBody>& rootBody)
{
	ndAssert(!m_rootNode);
	ndSharedPtr <ndJointBilateralConstraint> dommyJoint;
	m_rootNode = new ndNode(rootBody, dommyJoint, nullptr);
	return m_rootNode;
}

ndModelArticulation::ndNode* ndModelArticulation::AddLimb(ndNode* const parent, const ndSharedPtr<ndBody>& body, const ndSharedPtr<ndJointBilateralConstraint>& joint)
{
	ndAssert(m_rootNode);
	ndAssert(joint->GetBody0() == body->GetAsBodyKinematic());
	ndAssert(joint->GetBody1() == parent->m_body->GetAsBodyKinematic());
	return new ndNode(body, joint, parent);
}

void ndModelArticulation::OnAddToWorld()
{
	ndAssert(m_world);
	ndFixSizeArray<ndNode*, 256> stack;
	if (m_rootNode)
	{
		stack.PushBack(m_rootNode);
		while (stack.GetCount())
		{
			ndInt32 index = stack.GetCount() - 1;
			ndNode* const node = stack[index];
			stack.SetCount(index);
			m_world->AddBody(node->m_body);
			if (node->m_joint)
			{
				m_world->AddJoint(node->m_joint);
			}

			for (ndNode* child = node->GetFirstChild(); child; child = child->GetNext())
			{
				stack.PushBack(child);
			}
		}
	}

	for (ndSharedList<ndJointBilateralConstraint>::ndNode* node = m_closeLoops.GetFirst(); node; node = node->GetNext())
	{
		m_world->AddJoint(node->GetInfo());
	}
}

void ndModelArticulation::OnRemoveFromToWorld()
{
	ndAssert(m_world);
	ndFixSizeArray<ndNode*, 256> stack;
	if (m_rootNode)
	{
		for (ndSharedList<ndJointBilateralConstraint>::ndNode* node = m_closeLoops.GetFirst(); node; node = node->GetNext())
		{
			if (node->GetInfo()->m_worldNode)
			{
				m_world->RemoveJoint(node->GetInfo());
			}
		}

		stack.PushBack(m_rootNode);
		while (stack.GetCount())
		{
			ndInt32 index = stack.GetCount() - 1;
			ndNode* const node = stack[index];
			stack.SetCount(index);
			if (node->m_joint)
			{
				if (node->m_joint->m_worldNode)
				{
					m_world->RemoveJoint(node->m_joint);
				}
			}
			if (node->m_body->GetAsBodyKinematic()->m_sceneNode)
			{
				m_world->RemoveBody(node->m_body);
			}

			for (ndNode* child = node->GetFirstChild(); child; child = child->GetNext())
			{
				stack.PushBack(child);
			}
		}
	}
}

void ndModelArticulation::AddCloseLoop(const ndSharedPtr<ndJointBilateralConstraint>& joint)
{
	#ifdef _DEBUG

	auto Check = [&](const ndBodyKinematic* const body)
	{
		if (body->GetInvMass() == ndFloat32(0.0f))
		{
			return false;
		}
		ndFixSizeArray<ndNode*, 256> stack;
		stack.PushBack(m_rootNode);
		while (stack.GetCount())
		{
			ndInt32 index = stack.GetCount() - 1;
			ndNode* const node = stack[index];
			stack.SetCount(index);
			if (node->m_body->GetAsBodyKinematic() == body)
			{
				return true;
			}

			for (ndNode* child = node->GetFirstChild(); child; child = child->GetNext())
			{
				stack.PushBack(child);
			}
		}

		return false;
	};

	ndAssert(Check(joint->GetBody0()) || Check(joint->GetBody1()));
	#endif

	for (ndSharedList<ndJointBilateralConstraint>::ndNode* node = m_closeLoops.GetFirst(); node; node = node->GetNext())
	{
		if (*node->GetInfo() == *joint)
		{
			return;
		}
	}

	m_closeLoops.Append(joint);
}

void ndModelArticulation::AddToWorld(ndWorld* const world)
{
	for (ndModelArticulation::ndNode* node = m_rootNode->GetFirstIterator(); node; node = node->GetNextIterator())
	{
		world->AddBody(node->m_body);
		if (node->m_joint)
		{
			world->AddJoint(node->m_joint);
		}
	}
	world->AddModel(this);
}

ndModelArticulation::ndNode* ndModelArticulation::FindByName(const char* const name) const
{
	for (ndModelArticulation::ndNode* node = m_rootNode->GetFirstIterator(); node; node = node->GetNextIterator())
	{
		if (strcmp(node->m_name.GetStr(), name) == 0)
		{
			return node;
		}
	}

	return nullptr;
}

void ndModelArticulation::SetTransform(const ndMatrix& matrix)
{
	const ndMatrix offset(m_rootNode->m_body->GetMatrix().OrthoInverse() * matrix);
	for (ndModelArticulation::ndNode* node = m_rootNode->GetFirstIterator(); node; node = node->GetNextIterator())
	{
		ndSharedPtr<ndBody> body(node->m_body);
		body->SetMatrix(body->GetMatrix() * offset);
	}
}

void ndModelArticulation::ConvertToUrdf()
{
	struct BodyInfo
	{
		ndVector m_centerOfMass;
		ndMatrix m_bodyMatrix;
		ndMatrix m_collisionMatrix;
		ndMatrix m_jointMatrix0;
		ndMatrix m_jointMatrix1;
		ndJointBilateralConstraint* m_joint;
	};

	ndMatrix rootMatrix(m_rootNode->m_body->GetMatrix());
	//SetTransform(ndGetIdentityMatrix());
	SetTransform(ndPitchMatrix(ndPi * 0.5f));

	ndTree<BodyInfo, ndModelArticulation::ndNode*> map;
	for (ndModelArticulation::ndNode* node = m_rootNode->GetFirstIterator(); node; node = node->GetNextIterator())
	{
		if (*node->m_joint)
		{
			BodyInfo info;
			ndBodyKinematic* const body = node->m_body->GetAsBodyKinematic();
			info.m_bodyMatrix = body->GetMatrix();
			info.m_centerOfMass = info.m_bodyMatrix.TransformVector(body->GetCentreOfMass());
			info.m_collisionMatrix = body->GetCollisionShape().GetLocalMatrix() * info.m_bodyMatrix;
			info.m_joint = *node->m_joint;
			info.m_joint->CalculateGlobalMatrix(info.m_jointMatrix0, info.m_jointMatrix1);
			map.Insert(info, node);
		}
	}

	ndFixSizeArray<BodyInfo, 512> saved;
	for (ndModelArticulation::ndNode* child = m_rootNode->GetFirstChild(); child; child = child->GetNext())
	{
		BodyInfo info;
		ndBodyKinematic* const body = child->m_body->GetAsBodyKinematic();
		info.m_bodyMatrix = body->GetMatrix();
		info.m_centerOfMass = info.m_bodyMatrix.TransformVector(body->GetCentreOfMass());
		info.m_collisionMatrix = body->GetCollisionShape().GetLocalMatrix() * info.m_bodyMatrix;
		info.m_joint = *child->m_joint;
		info.m_joint->CalculateGlobalMatrix(info.m_jointMatrix0, info.m_jointMatrix1);
		saved.PushBack(info);
	}

	BodyInfo rootBodyInfo;
	ndBodyKinematic* const rootBody = m_rootNode->m_body->GetAsBodyKinematic();
	rootBodyInfo.m_bodyMatrix = rootBody->GetMatrix();
	rootBodyInfo.m_centerOfMass = rootBodyInfo.m_bodyMatrix.TransformVector(rootBody->GetCentreOfMass());
	rootBodyInfo.m_collisionMatrix = rootBody->GetCollisionShape().GetLocalMatrix() * rootBodyInfo.m_bodyMatrix;

	rootBody->SetMatrix(ndGetIdentityMatrix());
	rootBody->GetCollisionShape().SetLocalMatrix(rootBodyInfo.m_collisionMatrix);
	rootBody->SetCentreOfMass(rootBodyInfo.m_centerOfMass);
	for (ndInt32 i = 0; i < saved.GetCount(); ++i)
	{
		const BodyInfo& info = saved[i];
		info.m_joint->SetLocalMatrix1(info.m_jointMatrix1);
	}

	ndFixSizeArray<ndModelArticulation::ndNode*, 256> stack;
	stack.PushBack(m_rootNode);
	while (stack.GetCount())
	{
		ndModelArticulation::ndNode* const node = stack.Pop();
		if (*node->m_joint)
		{
			const BodyInfo& info = map.Find(node)->GetInfo();
			ndBodyKinematic* const body = node->m_body->GetAsBodyKinematic();
			
			body->SetMatrix(info.m_jointMatrix0);
			ndMatrix localMatrix(info.m_collisionMatrix * info.m_jointMatrix0.OrthoInverse());
			body->GetCollisionShape().SetLocalMatrix(localMatrix);
			body->SetCentreOfMass(info.m_jointMatrix0.UntransformVector(info.m_centerOfMass));
		}

		for (ndModelArticulation::ndNode* child = node->GetFirstChild(); child; child = child->GetNext())
		{
			stack.PushBack(child);
		}
	}

	stack.PushBack(m_rootNode);
	while (stack.GetCount())
	{
		ndModelArticulation::ndNode* const node = stack.Pop();
		ndJointBilateralConstraint* const joint = *node->m_joint;
		if (joint)
		{
			const BodyInfo& info = map.Find(node)->GetInfo();
			ndBodyKinematic* const body0 = joint->GetBody0();
			ndBodyKinematic* const body1 = joint->GetBody1();

			ndMatrix localMatrix0(info.m_jointMatrix0 * body0->GetMatrix().OrthoInverse());
			ndMatrix localMatrix1(info.m_jointMatrix1 * body1->GetMatrix().OrthoInverse());
			joint->SetLocalMatrix0(localMatrix0);
			joint->SetLocalMatrix1(localMatrix1);
		}

		for (ndModelArticulation::ndNode* child = node->GetFirstChild(); child; child = child->GetNext())
		{
			stack.PushBack(child);
		}
	}

	SetTransform(rootMatrix);
}