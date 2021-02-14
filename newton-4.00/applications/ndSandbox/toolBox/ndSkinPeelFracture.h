#ifndef __D_SKIN_PEEL_FRACTURE_H__
#define __D_SKIN_PEEL_FRACTURE_H__

#include "ndSandboxStdafx.h"

#define USE_SINGLE_MESH

class ndDemoMesh;
class ndDemoDebriEntity;
class ndDemoEntityManager;
class ndDemoDebriEntityRoot;

class ndSkinPeelFracture: public ndModel
{
	class ndAtom
	{
		public:
		ndAtom();
		ndAtom(const ndAtom& atom);
		~ndAtom();

		dVector m_centerOfMass;
		dVector m_momentOfInertia;
#ifdef USE_SINGLE_MESH
		ndDemoMesh* m_mesh;
#else	
		ndDemoDebriEntity* m_mesh;
#endif
		ndShapeInstance* m_collision;
		dFloat32 m_massFraction;
	};

	public:
	class ndDesc
	{
		public:
		ndDesc()
			:m_pointCloud()
			,m_outerShape(nullptr)
			,m_innerShape(nullptr)
			,m_outTexture(nullptr)
			,m_innerTexture(nullptr)
			,m_breakImpactSpeed(10.0f)
		{
		}

		dArray<dVector> m_pointCloud;
		ndShapeInstance* m_outerShape;
		ndShapeInstance* m_innerShape;
		const char* m_outTexture;
		const char* m_innerTexture;
		dFloat32 m_breakImpactSpeed;
	};

	class ndEffect : public dList<ndAtom>
	{
		public:
		ndEffect(ndSkinPeelFracture* const manager, const ndDesc& desc);
		ndEffect(const ndEffect& effect);
		~ndEffect();

		private:
		ndBodyKinematic* m_body;
		ndShapeInstance* m_shape;
		ndDemoMesh* m_visualMesh;
		dFloat32 m_breakImpactSpeed;
#ifndef USE_SINGLE_MESH
		ndDemoDebriEntityRoot* m_debriRootEnt;
#endif

		friend ndSkinPeelFracture;
	};

	public:
	ndSkinPeelFracture(ndDemoEntityManager* const scene);
	~ndSkinPeelFracture();

	void AddEffect(const ndEffect& effect, dFloat32 mass, const dMatrix& location);

	virtual void AppUpdate(ndWorld* const world);
	virtual void Update(const ndWorld* const world, dFloat32 timestep);

	void UpdateEffect(ndWorld* const world, ndEffect& effect);

	void ExplodeLocation(ndBodyDynamic* const body, const dMatrix& matrix, dFloat32 factor) const;

	dList<ndEffect> m_effectList;
	dList<ndEffect> m_pendingEffect;
	ndDemoEntityManager* m_scene;
	dSpinLock m_lock;
};

#endif
