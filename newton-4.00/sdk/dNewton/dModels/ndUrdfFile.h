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

#ifndef _ND_URDF_FILE_H_
#define _ND_URDF_FILE_H_

#include "ndModelArticulation.h"

class ndUrdfBodyNotify : public ndBodyNotify
{
	public:
	ndUrdfBodyNotify(ndMeshEffect* const mesh)
		:ndBodyNotify(ndVector::m_zero)
		,m_mesh(mesh)
	{
	}
	
	ndSharedPtr<ndMeshEffect> m_mesh;
};

class ndUrdfFile : public ndClassAlloc
{
	class Material
	{
		public:
		Material()
			:m_color(1.0f, 1.0f, 1.0f, 1.0f)
		{
		}
		ndVector m_color;
	};

	public:
	D_NEWTON_API ndUrdfFile();
	D_NEWTON_API virtual ~ndUrdfFile();

	D_NEWTON_API virtual ndModelArticulation* Import(const char* const fileName);
	D_NEWTON_API virtual void Export(const char* const fileName, ndModelArticulation* const model);

	private:
	//void CheckUniqueNames(ndModelArticulation* const model);
	//void AddLinks(nd::TiXmlElement* const rootNode, const ndModelArticulation* const model);
	//void AddJoints(nd::TiXmlElement* const rootNode, const ndModelArticulation* const model);
	//
	//void AddMaterials(nd::TiXmlElement* const rootNode, const ndModelArticulation* const model);
	//void AddLink(nd::TiXmlElement* const rootNode, const ndModelArticulation::ndNode* const link);
	//void AddJoint(nd::TiXmlElement* const rootNode, const ndModelArticulation::ndNode* const link);
	//
	//void AddInertia(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link);
	//void AddGeometry(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link);
	//void AddCollision(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link);
	//
	//void AddPose(nd::TiXmlElement* const linkNode, const ndMatrix& pose);
	//void AddCollision(nd::TiXmlElement* const linkNode, const ndModelArticulation::ndNode* const link, const ndShapeInstance& collision);


	class Hierarchy
	{
		public:
		Hierarchy(const nd::TiXmlNode* const link)
			:m_parent(nullptr)
			,m_link(link)
			,m_joint(link)
			,m_parentLink(nullptr)
			,m_articulation(nullptr)
		{
		}

		Hierarchy* m_parent;
		const nd::TiXmlNode* m_link;
		const nd::TiXmlNode* m_joint;
		const nd::TiXmlNode* m_parentLink;
		ndModelArticulation::ndNode* m_articulation;
	};

	void LoadMaterials(const nd::TiXmlNode* const rootNode);
	ndMatrix GetMatrix(const nd::TiXmlNode* const parentNode) const;
	
	ndBodyDynamic* CreateBody(const nd::TiXmlNode* const linkNode);
	ndJointBilateralConstraint* CreateJoint(const nd::TiXmlNode* const jointNode, ndBodyDynamic* const child, ndBodyDynamic* const parent);

	void ApplyRotation(const ndMatrix& rotation, ndModelArticulation* const model);

	void LoadStlMesh(const char* const pathName, ndMeshEffect* const meshEffect) const;
	ndString m_path;
	ndArray<Material> m_materials;
	ndTree<Hierarchy, ndString> m_bodyLinks;
	ndTree<ndInt32, ndString> m_materialMap;
};

#endif