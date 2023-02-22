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

#include "ndFileFormatStdafx.h"
#include "ndFileFormat.h"
#include "ndFileFormatShapeStaticMesh_bvh.h"

ndFileFormatShapeStaticMesh_bvh::ndFileFormatShapeStaticMesh_bvh()
	:ndFileFormatShapeStaticMesh(ndShapeStatic_bvh::StaticClassName())
{
}

ndFileFormatShapeStaticMesh_bvh::ndFileFormatShapeStaticMesh_bvh(const char* const className)
	:ndFileFormatShapeStaticMesh(className)
{
}

ndInt32 ndFileFormatShapeStaticMesh_bvh::SaveShape(ndFileFormat* const scene, nd::TiXmlElement* const parentNode, const ndShape* const shape)
{
	nd::TiXmlElement* const classNode = xmlCreateClassNode(parentNode, "ndShape", ndShapeStatic_bvh::StaticClassName());
	ndFileFormatShapeStaticMesh::SaveShape(scene, classNode, shape);

	char fileName[1024];
	ndShapeStatic_bvh* const staticMesh = (ndShapeStatic_bvh*)shape;
	sprintf(fileName, "%s", scene->m_fileName.GetStr());
	char* const ptr = strrchr(fileName, '.');
	if (ptr)
	{
		ndInt32 nodeId = xmlGetNodeId(classNode);
		sprintf(ptr, "_%d.bin", nodeId);
	}
	xmlSaveParam(classNode, "assetName", "string", fileName);
	staticMesh->Serialize(fileName);
	return xmlGetNodeId(classNode);
}