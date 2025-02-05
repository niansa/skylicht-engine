#pragma once

#include "CGridPlaneData.h"
#include "Transform/CWorldTransformData.h"
#include "Entity/IRenderSystem.h"

namespace Skylicht
{
	class CGridPlaneRender : public IRenderSystem
	{
	protected:
		core::array<CGridPlaneData*> m_gridPlanes;
		core::array<CWorldTransformData*> m_transforms;

	public:
		virtual void beginQuery(CEntityManager* entityManager);

		virtual void onQuery(CEntityManager *entityManager, CEntity *entity);

		virtual void init(CEntityManager *entityManager);

		virtual void update(CEntityManager *entityManager);

		virtual void render(CEntityManager *entityManager);
	};
}