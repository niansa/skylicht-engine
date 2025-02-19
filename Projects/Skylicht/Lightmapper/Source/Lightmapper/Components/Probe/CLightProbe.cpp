/*
!@
MIT License

Copyright (c) 2020 Skylicht Technology CO., LTD

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

This file is part of the "Skylicht Engine".
https://github.com/skylicht-lab/skylicht-engine
!#
*/

#include "pch.h"
#include "CLightProbe.h"
#include "GameObject/CGameObject.h"

#include "Entity/CEntity.h"
#include "Entity/CEntityManager.h"

#include "Lightmapper/CLightmapper.h"

namespace Skylicht
{
	namespace Lightmapper
	{
		CLightProbe::CLightProbe() :
			m_probeData(NULL)
		{

		}

		CLightProbe::~CLightProbe()
		{

		}

		void CLightProbe::initComponent()
		{
			m_probeData = m_gameObject->getEntity()->addData<CLightProbeData>();
			m_gameObject->getEntityManager()->addRenderSystem<CLightProbeRender>();
		}

		void CLightProbe::updateComponent()
		{

		}

		void CLightProbe::bakeIrradiance(CCamera *camera, IRenderPipeline *rp, CEntityManager *entityMgr)
		{
			core::vector3df position = m_gameObject->getPosition();

			core::vector3df n = CTransform::s_oy;
			core::vector3df t = CTransform::s_ox;
			core::vector3df b = n.crossProduct(t);
			b.normalize();

			m_probeData->SH = CLightmapper::getInstance()->bakeAtPosition(
				camera,
				rp,
				entityMgr,
				position,
				n,
				t,
				b);
		}
	}
}