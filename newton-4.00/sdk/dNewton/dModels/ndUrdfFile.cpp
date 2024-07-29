/* Copyright (c) <2003-2022> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

#include "ndCoreStdafx.h"
#include "ndNewtonStdafx.h"
#include "ndUrdfFile.h"


ndUrdfFile::ndUrdfFile()
	:ndClassAlloc()
{
}

ndUrdfFile::~ndUrdfFile()
{
}

//ndModelArticulation* ndUrdfFile::Import(const char* const fileName)
ndModelArticulation* ndUrdfFile::Import(const char* const)
{
	ndAssert(0);
	return nullptr;
}

void ndUrdfFile::Export(const char* const filePathName, ndModelArticulation* const model)
{
	ndAssert(strstr(filePathName, ".urdf"));

	nd::TiXmlDocument* const doc = new nd::TiXmlDocument("");
	nd::TiXmlDeclaration* const decl = new nd::TiXmlDeclaration("1.0", "", "");
	doc->LinkEndChild(decl);
	ndString oldloc(setlocale(LC_ALL, 0));

	nd::TiXmlElement* const rootNode = new nd::TiXmlElement("robot");
	doc->LinkEndChild(rootNode);
	if (model->GetName() == "")
	{
		rootNode->SetAttribute("name", "robot_model");
	}
	else
	{
		rootNode->SetAttribute("name", model->GetName().GetStr());
	}

	CheckUniqueNames(model);
	//AddMaterials(rootNode, model);
	AddLinks(rootNode, model);
	//AddJoints(rootNode, model);

	doc->SaveFile(filePathName);
	setlocale(LC_ALL, oldloc.GetStr());
	delete doc;
}

void ndUrdfFile::CheckUniqueNames(ndModelArticulation* const model)
{
	ndTree < ndModelArticulation::ndNode*, ndString> filter;
	
	ndString baseName("link_");
	for (ndModelArticulation::ndNode* childNode = model->GetRoot()->GetFirstIterator(); childNode; childNode = childNode->GetNextIterator())
	{
		if (childNode->m_name == "")
		{
			childNode->m_name = baseName + "0";
		}
		ndInt32 count = 1;
		while (filter.Find(childNode->m_name))
		{
			childNode->m_name = baseName + count;
			count++;
		}
		filter.Insert(childNode, childNode->m_name);
	}
}

//void ndUrdfFile::AddMaterials(nd::TiXmlElement* const rootNode, const ndModelArticulation* const model)
void ndUrdfFile::AddMaterials(nd::TiXmlElement* const rootNode, const ndModelArticulation* const)
{
	nd::TiXmlElement* const material = new nd::TiXmlElement("material");
	rootNode->LinkEndChild(material);
	material->SetAttribute("name", "material_0");

	nd::TiXmlElement* const color = new nd::TiXmlElement("color");
	material->LinkEndChild(color);
	color->SetAttribute("rgba", "0.5 0.6 0.2, 1.0");
}

void ndUrdfFile::AddLinks(nd::TiXmlElement* const rootNode, const ndModelArticulation* const model)
{
	ndFixSizeArray<ndModelArticulation::ndNode*, 256> stack;
	stack.PushBack(model->GetRoot());

	while (stack.GetCount())
	{
		ndModelArticulation::ndNode* const node = stack[stack.GetCount() - 1];
		stack.SetCount(stack.GetCount() - 1);

		AddLink(rootNode, node);

		for (ndModelArticulation::ndNode* childNode = node->GetFirstChild(); childNode; childNode = childNode->GetNext())
		{
			stack.PushBack(childNode);
		}
	}
}

void ndUrdfFile::AddLink(nd::TiXmlElement* const rootNode, const ndModelArticulation::ndNode* const link)
{
	nd::TiXmlElement* const linkNode = new nd::TiXmlElement("link");
	rootNode->LinkEndChild(linkNode);

	linkNode->SetAttribute("name", link->m_name.GetStr());

	AddInertia(linkNode, link);
	AddGeometry(linkNode, link);
	AddCollision(linkNode, link);
}

void ndUrdfFile::AddInertia(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link)
{
	char buffer[256];
	nd::TiXmlElement* const inertial = new nd::TiXmlElement("inertial");
	linkNode->LinkEndChild(inertial);

	const ndBodyKinematic* const body = link->m_body->GetAsBodyKinematic();
	ndMatrix matrix(body->GetMatrix());
	ndVector com(matrix.TransformVector(body->GetCentreOfMass()));
	AddPose(inertial, matrix);

	nd::TiXmlElement* const mass = new nd::TiXmlElement("mass");
	inertial->LinkEndChild(mass);
	ndVector massMatrix(body->GetMassMatrix());
	sprintf(buffer, "%g", massMatrix.m_w);
	mass->SetAttribute("value", buffer);

	nd::TiXmlElement* const inertia = new nd::TiXmlElement("inertia");
	inertial->LinkEndChild(inertia);

	ndMatrix inertiaMatrix(body->CalculateInertiaMatrix());
	sprintf(buffer, "%g", inertiaMatrix[0][0]);
	inertia->SetAttribute("xx", buffer);

	sprintf(buffer, "%g", inertiaMatrix[0][1]);
	inertia->SetAttribute("xy", buffer);

	sprintf(buffer, "%g", inertiaMatrix[0][2]);
	inertia->SetAttribute("xz", buffer);

	sprintf(buffer, "%g", inertiaMatrix[1][1]);
	inertia->SetAttribute("yy", buffer);

	sprintf(buffer, "%g", inertiaMatrix[1][2]);
	inertia->SetAttribute("yz", buffer);

	sprintf(buffer, "%g", inertiaMatrix[2][2]);
	inertia->SetAttribute("zz", buffer);
}

void ndUrdfFile::AddPose(nd::TiXmlElement* const parentNode, const ndMatrix& pose)
{
	char buffer[256];
	nd::TiXmlElement* const origin = new nd::TiXmlElement("origin");
	parentNode->LinkEndChild(origin);
	sprintf(buffer, "%g %g %g", pose.m_posit.m_x, pose.m_posit.m_y, pose.m_posit.m_z);
	origin->SetAttribute("xyz", buffer);

	ndVector euler1;
	ndVector euler(pose.CalcPitchYawRoll(euler1));
	sprintf(buffer, "%g %g %g", euler.m_z, euler.m_x, euler.m_y);
	origin->SetAttribute("rpy", buffer);
}

void ndUrdfFile::AddGeometry(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link)
{
	char buffer[256];

	nd::TiXmlElement* const visualNode = new nd::TiXmlElement("visual");
	linkNode->LinkEndChild(visualNode);

	// add position and orientation
	const ndBodyKinematic* const body = link->m_body->GetAsBodyKinematic();
	const ndShapeInstance& collision = body->GetCollisionShape();
	ndMatrix matrix(collision.GetScaledTransform(body->GetMatrix()));

	AddPose(visualNode, matrix);

	// add geometry node
	nd::TiXmlElement* const geometry = new nd::TiXmlElement("geometry");
	visualNode->LinkEndChild(geometry);
	const ndShape* const collisionShape = collision.GetShape();
	const char* const className = collisionShape->ClassName();
	if (!strcmp(className, "ndShapeBox"))
	{
		ndShapeInfo info (collisionShape->GetShapeInfo());
		sprintf(buffer, "%g %g %g", info.m_box.m_x, info.m_box.m_y, info.m_box.m_z);

		nd::TiXmlElement* const shape = new nd::TiXmlElement("box");
		geometry->LinkEndChild(shape);
		shape->SetAttribute("size", buffer);
	}
	else if (!strcmp(className, "ndShapeCapsule"))
	{
		ndShapeInfo info(collisionShape->GetShapeInfo());

		nd::TiXmlElement* const shape = new nd::TiXmlElement("cylinder");
		geometry->LinkEndChild(shape);

		sprintf(buffer, "%g", info.m_capsule.m_radio0);
		shape->SetAttribute("radius", buffer);

		sprintf(buffer, "%g", info.m_capsule.m_height + info.m_capsule.m_radio0 * 2.0f);
		shape->SetAttribute("length", buffer);
	}
	else if (!strcmp(className, "ndShapeSphere"))
	{
		ndShapeInfo info(collisionShape->GetShapeInfo());

		nd::TiXmlElement* const shape = new nd::TiXmlElement("sphere");
		geometry->LinkEndChild(shape);

		sprintf(buffer, "%g", info.m_sphere.m_radius);
		shape->SetAttribute("radius", buffer);
	}
	else
	{
		ndAssert(0);
	}

	// add material node
	nd::TiXmlElement* const material = new nd::TiXmlElement("material");
	visualNode->LinkEndChild(material);
	material->SetAttribute("name", "material_0");

	nd::TiXmlElement* const color = new nd::TiXmlElement("color");
	material->LinkEndChild(color);
	color->SetAttribute("rgba", "0.5 0.6 0.2, 1.0");
}

void ndUrdfFile::AddCollision(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link, const ndShapeInstance& collision)
{
	char buffer[256];
	const ndShape* const collisionShape = collision.GetShape();
	const char* const className = collisionShape->ClassName();

	nd::TiXmlElement* const collisionNode = new nd::TiXmlElement("collision");
	linkNode->LinkEndChild(collisionNode);

	const ndBodyKinematic* const body = link->m_body->GetAsBodyKinematic();
	ndMatrix matrix(collision.GetScaledTransform(body->GetMatrix()));
	AddPose(collisionNode, matrix);

	nd::TiXmlElement* const geometry = new nd::TiXmlElement("geometry");
	collisionNode->LinkEndChild(geometry);

	if (!strcmp(className, "ndShapeBox"))
	{
		ndShapeInfo info(collisionShape->GetShapeInfo());
		sprintf(buffer, "%g %g %g", info.m_box.m_x, info.m_box.m_y, info.m_box.m_z);
		
		nd::TiXmlElement* const shape = new nd::TiXmlElement("box");
		geometry->LinkEndChild(shape);
		shape->SetAttribute("size", buffer);
	}
	else if (!strcmp(className, "ndShapeCapsule"))
	{
		ndShapeInfo info(collisionShape->GetShapeInfo());
		
		nd::TiXmlElement* const shape = new nd::TiXmlElement("cylinder");
		geometry->LinkEndChild(shape);
		
		sprintf(buffer, "%g", info.m_capsule.m_radio0);
		shape->SetAttribute("radius", buffer);
		
		sprintf(buffer, "%g", info.m_capsule.m_height + info.m_capsule.m_radio0 * 2.0f);
		shape->SetAttribute("length", buffer);
	}
	else if (!strcmp(className, "ndShapeSphere"))
	{
		ndShapeInfo info(collisionShape->GetShapeInfo());
		
		nd::TiXmlElement* const shape = new nd::TiXmlElement("sphere");
		geometry->LinkEndChild(shape);
		
		sprintf(buffer, "%g", info.m_sphere.m_radius);
		shape->SetAttribute("radius", buffer);
	}
	else
	{
		ndAssert(0);
	}
}

void ndUrdfFile::AddCollision(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link)
{
	const ndBodyKinematic* const body = link->m_body->GetAsBodyKinematic();
	const ndShapeInstance& collision = body->GetCollisionShape();

	const ndShape* const collisionShape = collision.GetShape();
	const char* const className = collisionShape->ClassName();

	if (!strcmp(className, "ndShapeCompound"))
	{
		ndAssert(0);
	}
	else
	{
		AddCollision(linkNode, link, collision);
	}
}

void ndUrdfFile::AddJoints(nd::TiXmlElement* const rootNode, const ndModelArticulation* const model)
{
	ndAssert(0);
	ndFixSizeArray<ndModelArticulation::ndNode*, 256> stack;
	for (ndModelArticulation::ndNode* child = model->GetRoot()->GetFirstChild(); child; child = child->GetNext())
	{
		stack.PushBack(child);
	}

	while (stack.GetCount())
	{
		ndModelArticulation::ndNode* const node = stack[stack.GetCount() - 1];
		stack.SetCount(stack.GetCount() - 1);

		nd::TiXmlElement* const joint = new nd::TiXmlElement("joint");
		rootNode->LinkEndChild(joint);


		nd::TiXmlElement* const child = new nd::TiXmlElement("child");
		joint->LinkEndChild(child);
		child->SetAttribute("link", node->m_name.GetStr());

		for (ndModelArticulation::ndNode* childNode = node->GetFirstChild(); childNode; childNode = childNode->GetNext())
		{
			stack.PushBack(childNode);
		}
	}
}

void ndUrdfFile::AddJoint(nd::TiXmlElement* const joint, const ndModelArticulation::ndNode* const link)
{
	ndString name(link->GetParent()->m_name + "_" + link->m_name);
	joint->SetAttribute("name", name.GetStr());
	joint->SetAttribute("type", "xxxxxxxx");

	nd::TiXmlElement* const parent = new nd::TiXmlElement("parent");
	joint->LinkEndChild(parent);
	parent->SetAttribute("link", link->GetParent()->m_name.GetStr());
}