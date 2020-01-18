/*
!@
MIT License

Copyright (c) 2019 Skylicht Technology CO., LTD

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
#include "CBaseRP.h"
#include "Material/CMaterial.h"
#include "Material/Shader/CShaderManager.h"
#include "TextureManager/CTextureManager.h"

namespace Skylicht
{
	CBaseRP::CBaseRP() :
		m_next(NULL)
	{
		const core::dimension2du &size = getVideoDriver()->getCurrentRenderTargetSize();
		m_viewport2DW = (float)size.Width;
		m_viewport2DH = (float)size.Height;

		m_drawBuffer = new scene::CMeshBuffer<video::S3DVertex2TCoords>(getVideoDriver()->getVertexDescriptor(video::EVT_2TCOORDS), video::EIT_16BIT);
		m_drawBuffer->setHardwareMappingHint(EHM_STREAM);

		m_verticesImage = m_drawBuffer->getVertexBuffer();
		m_indicesImage = m_drawBuffer->getIndexBuffer();

		// init indices buffer
		m_indicesImage->set_used(6);
		u16 *index = (u16*)m_indicesImage->getIndices();
		index[0] = 0;
		index[1] = 1;
		index[2] = 2;
		index[3] = 0;
		index[4] = 2;
		index[5] = 3;

		// unbind material
		m_unbindMaterial.ZBuffer = video::ECFN_DISABLED;
		m_unbindMaterial.ZWriteEnable = false;
		m_unbindMaterial.MaterialType = CShaderManager::getInstance()->getShaderIDByName("TextureColor");
		ITexture *nullTexture = CTextureManager::getInstance()->getNullTexture();
		for (u32 i = 0; i < MATERIAL_MAX_TEXTURES; ++i)
			m_unbindMaterial.setTexture(i, nullTexture);
	}

	CBaseRP::~CBaseRP()
	{
		m_drawBuffer->drop();
		m_verticesImage = NULL;
		m_indicesImage = NULL;
	}

	bool CBaseRP::canRenderMaterial(CMaterial *m)
	{
		// default dont render deferred material
		if (m->isDeferred() == true)
			return false;

		return true;
	}

	void CBaseRP::setCamera(CCamera *camera)
	{
		const SViewFrustum& viewArea = camera->getViewFrustum();
		video::IVideoDriver* driver = getVideoDriver();
		driver->setTransform(video::ETS_PROJECTION, viewArea.getTransform(video::ETS_PROJECTION));
		driver->setTransform(video::ETS_VIEW, viewArea.getTransform(video::ETS_VIEW));
	}

	void CBaseRP::setNextPipeLine(IRenderPipeline *next)
	{
		m_next = next;
	}

	void CBaseRP::onNext(ITexture *target, CCamera *camera, CEntityManager* entity)
	{
		if (m_next != NULL)
			m_next->render(target, camera, entity);
	}

	void CBaseRP::beginRender2D(float w, float h)
	{
		core::matrix4 orthoMatrix;
		orthoMatrix.makeIdentity();
		orthoMatrix.buildProjectionMatrixOrthoLH(w, -h, -1.0f, 1.0f);
		orthoMatrix.setTranslation(core::vector3df(-1, 1, 0));

		IVideoDriver *driver = getVideoDriver();

		driver->setTransform(video::ETS_PROJECTION, orthoMatrix);
		driver->setTransform(video::ETS_VIEW, core::IdentityMatrix);
		driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);

		m_viewport2DW = w;
		m_viewport2DH = h;
	}

	void CBaseRP::unbindRTT()
	{
		IVideoDriver *driver = getVideoDriver();

		int numVerticesUse = 4;
		m_verticesImage->set_used(numVerticesUse);
		S3DVertex2TCoords *vertices = (S3DVertex2TCoords*)m_verticesImage->getVertices();
		SColor color(255, 255, 255, 255);

		float x = -1.0f;
		float y = -1.0f;
		float w = 1.0f;
		float h = 1.0f;

		// add vertices
		vertices[0] = S3DVertex2TCoords(x, y, 0.0f, 0.0f, 0.0f, 1.0f, color, 0, 0, 1, 1);
		vertices[1] = S3DVertex2TCoords(x + w, y, 0.0f, 0.0f, 0.0f, 1.0f, color, 0, 0, 1, 1);
		vertices[2] = S3DVertex2TCoords(x + w, y + h, 0.0f, 0.0f, 0.0f, 1.0f, color, 0, 0, 1, 1);
		vertices[3] = S3DVertex2TCoords(x, y + h, 0.0f, 0.0f, 0.0f, 1.0f, color, 0, 0, 1, 1);

		// notify change vertex buffer
		m_drawBuffer->setDirty(scene::EBT_VERTEX);

		// draw buffer
		driver->setMaterial(m_unbindMaterial);
		driver->drawMeshBuffer(m_drawBuffer);
	}

	void CBaseRP::renderBufferToTarget(float sx, float sy, float sw, float sh, SMaterial& material, bool flipY, bool flipX)
	{
		ITexture *tex = material.getTexture(0);
		if (tex == NULL)
			return;

		IVideoDriver *driver = getVideoDriver();

		int numVerticesUse = 4;

		m_verticesImage->set_used(numVerticesUse);
		S3DVertex2TCoords *vertices = (S3DVertex2TCoords*)m_verticesImage->getVertices();
		SColor color(255, 255, 255, 255);

		float x = 0;
		float y = 0;
		float w = m_viewport2DW;
		float h = m_viewport2DH;

		float texW = (float)tex->getSize().Width;
		float texH = (float)tex->getSize().Height;

		const f32 invW = 1.f / static_cast<f32>(texW);
		const f32 invH = 1.f / static_cast<f32>(texH);

		float tx1 = sx * invW;
		float ty1 = sy * invH;
		float tw1 = tx1 + sw * invW;
		float th1 = ty1 + sh * invH;

		float tx2 = 0.0f;
		float ty2 = 0.0f;
		float tw2 = 1.0f;
		float th2 = 1.0f;

		if (driver->getDriverType() != EDT_DIRECT3D11)
		{
			if (flipY)
			{
				ty1 = 1.0f - ty1;
				th1 = 1.0f - th1;

				ty2 = 1.0f - ty2;
				th2 = 1.0f - th2;
			}

			if (flipX)
			{
				tx1 = 1.0f - tx1;
				tw1 = 1.0f - tw1;

				tx2 = 1.0f - tx2;
				tw2 = 1.0f - tw2;
			}
		}

		// add vertices
		vertices[0] = S3DVertex2TCoords(x, y, 0.0f, 0.0f, 0.0f, 1.0f, color, tx1, ty1, tx2, ty2);
		vertices[1] = S3DVertex2TCoords(x + w, y, 0.0f, 0.0f, 0.0f, 1.0f, color, tw1, ty1, tw2, ty2);
		vertices[2] = S3DVertex2TCoords(x + w, y + h, 0.0f, 0.0f, 0.0f, 1.0f, color, tw1, th1, tw2, th2);
		vertices[3] = S3DVertex2TCoords(x, y + h, 0.0f, 0.0f, 0.0f, 1.0f, color, tx1, th1, tx2, th2);

		// notify change vertex buffer
		m_drawBuffer->setDirty(scene::EBT_VERTEX);

		// no depth test
		material.ZBuffer = video::ECFN_DISABLED;
		material.ZWriteEnable = false;

		// draw buffer
		driver->setMaterial(material);
		driver->drawMeshBuffer(m_drawBuffer);
	}
}