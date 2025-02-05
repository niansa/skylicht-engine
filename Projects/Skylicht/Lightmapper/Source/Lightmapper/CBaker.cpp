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
#include "CBaker.h"
#include "CBakeUtils.h"
#include "GameObject/CGameObject.h"
#include "RenderPipeline/CBaseRP.h"

#include "Lightmapper/CLightmapper.h"

namespace Skylicht
{
	namespace Lightmapper
	{
		CBaker::CBaker()
		{
			IVideoDriver *driver = getVideoDriver();

			u32 bakeSize = CLightmapper::getHemisphereBakeSize();
			core::dimension2du s(bakeSize * NUM_FACES, bakeSize);

			m_radiance = driver->addRenderTargetTexture(s, "lmrt", video::ECF_A8R8G8B8);
		}

		CBaker::~CBaker()
		{
			IVideoDriver *driver = getVideoDriver();
			driver->removeTexture(m_radiance);
			m_radiance = NULL;
		}

		const CSH9& CBaker::bake(CCamera *camera,
			IRenderPipeline* rp,
			CEntityManager* entityMgr,
			const core::vector3df& position,
			const core::vector3df& normal,
			const core::vector3df& tangent,
			const core::vector3df& binormal,
			int numFace)
		{
			IVideoDriver *driver = getVideoDriver();

			// apply projection
			camera->setAspect(1.0f);
			camera->setFOV(90.0f);

			// clear rtt
			getVideoDriver()->setRenderTarget(m_radiance, true, true);

			core::matrix4 toTangentSpace[NUM_FACES];

			u32 rtSize = CLightmapper::getHemisphereBakeSize();

			for (int face = 0; face < numFace; face++)
			{
				core::matrix4 cameraWorld;
				core::matrix4 viewToWorld;

				core::recti viewport;

				getWorldView(normal, tangent, binormal, position, face, cameraWorld);

				// to tangent space
				toTangentSpace[face] = cameraWorld;
				setRow(toTangentSpace[face], 3, core::vector3df(0.0f, 0.0f, 0.0f), 1.0f);

				// camera world
				camera->getGameObject()->getTransform()->setRelativeTransform(cameraWorld);

				// view matrix is world inverse
				cameraWorld.makeInverse();
				camera->setViewMatrix(cameraWorld, position);

				// apply viewport
				u32 offsetX = face * rtSize;
				u32 offsetY = 0;
				viewport.UpperLeftCorner.set(offsetX, offsetY);
				viewport.LowerRightCorner.set(offsetX + rtSize, offsetY + rtSize);

				// render
				CBaseRP::setBakeMode(true);
				rp->render(m_radiance, camera, entityMgr, viewport);
				CBaseRP::setBakeMode(false);
			}

			driver->setRenderTarget(NULL, false, false);

			// Cubemap to SH		
			u8 *imageData = (u8*)m_radiance->lock(video::ETLM_READ_ONLY);
			u32 bpp = 4;
			u32 rowSize = rtSize * NUM_FACES * bpp;
			float c = 1.0f / 255.0f;

			core::vector3df color;
			core::vector3df dirTS;

			bool isBGR = false;

			// Note: DX11 & OpenGLES is RGB after read texture data
			if (getVideoDriver()->getDriverType() == video::EDT_OPENGL)
				isBGR = true;

			m_sh.zero();

			// Compute the final weight for integration
			float weightSum = 0.0f;

			// Compute SH by radiance
			for (int face = 0; face < numFace; face++)
			{
				// offset to face data
				u8 *faceData = imageData + rtSize * face * bpp;

				// scan pixels
				for (u32 y = 0; y < rtSize; y++)
				{
					u8* data = faceData;

					for (u32 x = 0; x < rtSize; x++)
					{
						// Calculate the location in [-1, 1] texture space
						float u = ((x / float(rtSize)) * 2.0f - 1.0f);
						float v = -((y / float(rtSize)) * 2.0f - 1.0f);

						float temp = 1.0f + u * u + v * v;
						float weight = 4.0f / (sqrt(temp) * temp);
						weightSum = weightSum + weight;

						if (isBGR == true)
						{
							color.X = data[2] * c * weight; // b
							color.Y = data[1] * c * weight; // g
							color.Z = data[0] * c * weight; // r
						}
						else
						{
							color.X = data[0] * c * weight; // r
							color.Y = data[1] * c * weight; // g
							color.Z = data[2] * c * weight; // b
						}

						dirTS.X = u;
						dirTS.Y = v;
						dirTS.Z = 1.0f;
						toTangentSpace[face].rotateVect(dirTS);
						dirTS.normalize();

						CSH9 sh;
						sh.projectOntoSH(dirTS, color);
						m_sh += sh;

						data += bpp;
					}

					faceData += rowSize;
				}
			}

			// finalWeight is weight for 1 pixel on Sphere
			// S = 4 * PI * R^2
			float finalWeight = (4.0f * 3.14159f) / weightSum;
			m_sh *= finalWeight;

			m_radiance->unlock();

			// CBaseRP::saveFBOToFile(m_radiance, "D:\\test.png");
			return m_sh;
		}
	}
}