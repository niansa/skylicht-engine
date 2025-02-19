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
#include "CRenderMeshData.h"
#include "Material/Shader/CShaderManager.h"
#include "TextureManager/CTextureManager.h"
#include "Utils/CPath.h"
#include "Importer/IMeshImporter.h"

namespace Skylicht
{
	ACTIVATOR_REGISTER(CRenderMeshData);

	CRenderMeshData::CRenderMeshData() :
		RenderMesh(NULL),
		OriginalMesh(NULL),
		SoftwareSkinning(false),
		IsSkinnedMesh(false)
	{

	}

	CRenderMeshData::~CRenderMeshData()
	{
		if (RenderMesh != NULL)
			RenderMesh->drop();

		if (OriginalMesh != NULL)
			OriginalMesh->drop();
	}

	void CRenderMeshData::setMesh(CMesh* mesh)
	{
		if (RenderMesh)
		{
			RenderMesh->drop();
			RenderMesh = NULL;
		}

		RenderMesh = mesh->clone();
	}

	void CRenderMeshData::setMaterial(CMaterial* material)
	{
		CMesh* mesh = RenderMesh;
		const char* name = material->getName();

		int bufferID = 0;
		for (std::string& materialName : mesh->MaterialName)
		{
			if (materialName == name)
			{
				material->addAffectMesh(mesh->getMeshBuffer(bufferID));
				mesh->Material[bufferID] = material;
			}

			bufferID++;
		}
	}

	void CRenderMeshData::initSoftwareSkinning()
	{
		CSkinnedMesh* mesh = new CSkinnedMesh();

		for (int i = 0, n = RenderMesh->getMeshBufferCount(); i < n; i++)
		{
			// alloc new mesh buffer
			CMeshBuffer<video::S3DVertex>* meshBuffer = new CMeshBuffer<video::S3DVertex>(getVideoDriver()->getVertexDescriptor(video::EVT_STANDARD), video::EIT_16BIT);

			// skinned mesh buffer
			IMeshBuffer* originalMeshBuffer = RenderMesh->getMeshBuffer(i);


			// get new index & new vertex buffer
			CVertexBuffer<video::S3DVertex>* vertexBuffer = dynamic_cast<CVertexBuffer<video::S3DVertex>*>(meshBuffer->getVertexBuffer(0));
			CIndexBuffer* indexBuffer = dynamic_cast<CIndexBuffer*>(meshBuffer->getIndexBuffer());

			if (originalMeshBuffer->getVertexDescriptor()->getID() == video::EVT_SKIN_TANGENTS)
			{
				// SKIN TANGENT
				CVertexBuffer<video::S3DVertexSkinTangents>* originalTangentVertexBuffer = (CVertexBuffer<video::S3DVertexSkinTangents>*)originalMeshBuffer->getVertexBuffer(0);
				int numVertex = originalTangentVertexBuffer->getVertexCount();

				vertexBuffer->set_used(numVertex);
				for (int i = 0; i < numVertex; i++)
				{
					video::S3DVertex& dest = vertexBuffer->getVertex(i);
					video::S3DVertexSkinTangents& src = originalTangentVertexBuffer->getVertex(i);

					dest.Pos = src.Pos;
					dest.Normal = src.Normal;
					dest.Color = src.Color;
					dest.TCoords = src.TCoords;
				}
			}
			else
			{
				// SKIN
				CVertexBuffer<video::S3DVertexSkin>* originalVertexBuffer = (CVertexBuffer<video::S3DVertexSkin>*)originalMeshBuffer->getVertexBuffer(0);
				int numVertex = originalVertexBuffer->getVertexCount();

				vertexBuffer->set_used(numVertex);
				for (int i = 0; i < numVertex; i++)
				{
					video::S3DVertex& dest = vertexBuffer->getVertex(i);
					video::S3DVertexSkin& src = originalVertexBuffer->getVertex(i);

					dest.Pos = src.Pos;
					dest.Normal = src.Normal;
					dest.Color = src.Color;
					dest.TCoords = src.TCoords;
				}
			}

			// copy indices
			int numIndex = originalMeshBuffer->getIndexBuffer()->getIndexCount();
			indexBuffer->set_used(numIndex);
			for (int i = 0; i < numIndex; i++)
			{
				indexBuffer->setIndex(i, originalMeshBuffer->getIndexBuffer()->getIndex(i));
			}

			// copy material
			meshBuffer->getMaterial() = originalMeshBuffer->getMaterial();

			// apply static material
			CShaderManager* shaderMgr = CShaderManager::getInstance();

			if (meshBuffer->getMaterial().getTexture(0) != NULL)
				meshBuffer->getMaterial().MaterialType = shaderMgr->getShaderIDByName("TextureColor");
			else
				meshBuffer->getMaterial().MaterialType = shaderMgr->getShaderIDByName("VertexColor");

			meshBuffer->setHardwareMappingHint(EHM_STREAM);

			// add to mesh
			mesh->addMeshBuffer(meshBuffer);
			meshBuffer->recalculateBoundingBox();
			meshBuffer->drop();
		}

		mesh->recalculateBoundingBox();

		// swap default render mesh to dynamic stream mesh
		// see CSoftwareSkinningSystem todo next
		OriginalMesh = RenderMesh;
		RenderMesh = mesh;
	}

	bool CRenderMeshData::serializable(CMemoryStream* stream, IMeshExporter* exporter)
	{
		stream->writeChar(IsSkinnedMesh ? 1 : 0);

		CMesh* mesh = RenderMesh;
		if (SoftwareSkinning == true)
			mesh = OriginalMesh;

		if (IsSkinnedMesh == true)
		{
			CSkinnedMesh* smesh = dynamic_cast<CSkinnedMesh*>(mesh);

			// write joint data
			u32 numJoint = smesh->Joints.size();
			stream->writeUInt(numJoint);

			for (u32 i = 0; i < numJoint; i++)
			{
				CSkinnedMesh::SJoint& j = smesh->Joints[i];

				stream->writeString(j.Name);
				stream->writeInt(j.EntityIndex);
				stream->writeFloatArray(j.BindPoseMatrix.pointer(), 16);
			}
		}

		// write number buffer
		u32 numMB = mesh->getMeshBufferCount();
		stream->writeUInt(numMB);

		for (u32 i = 0; i < numMB; i++)
		{
			// write bind material name
			stream->writeString(mesh->MaterialName[i]);

			IMeshBuffer* mb = mesh->getMeshBuffer(i);
			IVertexBuffer* vb = mb->getVertexBuffer(0);
			IIndexBuffer* ib = mb->getIndexBuffer();

			// write texture name
			SMaterial& material = mb->getMaterial();
			for (int t = 0; t < 3; t++)
			{
				ITexture* texture = material.TextureLayer[t].Texture;
				if (texture != NULL)
				{
					std::string path = texture->getName().getInternalName().c_str();
					stream->writeString(CPath::getFileName(path).c_str());
				}
				else
					stream->writeString("");
			}

			// write bounding box
			stream->writeFloatArray(&mb->getBoundingBox().MinEdge.X, 3);
			stream->writeFloatArray(&mb->getBoundingBox().MaxEdge.X, 3);

			// write vertices data
			u32 vtxCount = vb->getVertexCount();
			u32 vtxSize = vb->getVertexSize();
			u32 vtxBufferSize = vtxCount * vtxSize;

			u32 idxCount = ib->getIndexCount();
			u32 idxSize = ib->getIndexSize();
			u32 idxBufferSize = idxCount * idxSize;

			stream->writeUInt(vtxCount);
			stream->writeUInt(vtxSize);

			stream->writeUInt(idxCount);
			stream->writeUInt(idxSize);

			// write attribute
			int vertexType = (int)mb->getVertexType();
			stream->writeString(video::sBuiltInVertexTypeNames[vertexType]);

			video::IVertexDescriptor* vtxInfo = mb->getVertexDescriptor();
			u32 numAttribute = vtxInfo->getAttributeCount();
			stream->writeUInt(numAttribute);
			for (u32 j = 0; j < numAttribute; j++)
			{
				IVertexAttribute* attribute = vtxInfo->getAttribute(j);
				stream->writeString(attribute->getName().c_str());
				stream->writeShort(attribute->getOffset());
				stream->writeShort(attribute->getElementCount());
				stream->writeShort(attribute->getTypeSize());
			}

			// write vertex data
			stream->writeData(vb->getVertices(), vtxBufferSize);

			// write indices data
			stream->writeData(ib->getIndices(), idxBufferSize);
		}

		return true;
	}

	bool CRenderMeshData::deserializable(CMemoryStream* stream, IMeshImporter* importer)
	{
		CShaderManager* shaderMgr = CShaderManager::getInstance();

		IsSkinnedMesh = stream->readChar() == 1 ? true : false;

		if (IsSkinnedMesh == true)
		{
			CSkinnedMesh* smesh = new CSkinnedMesh();

			u32 numJoint = stream->readUInt();

			for (u32 i = 0; i < numJoint; i++)
			{
				smesh->Joints.push_back(CSkinnedMesh::SJoint());
				CSkinnedMesh::SJoint& j = smesh->Joints[i];

				j.Name = stream->readString();
				j.EntityIndex = stream->readInt();
				stream->readFloatArray(j.BindPoseMatrix.pointer(), 16);
			}

			RenderMesh = smesh;
		}
		else
		{
			RenderMesh = new CMesh();
		}

		u32 numMB = stream->readUInt();

		for (u32 i = 0; i < numMB; i++)
		{
			std::string material = stream->readString();

			std::vector<std::string> textures;

			for (int t = 0; t < 3; t++)
				textures.push_back(stream->readString());

			core::aabbox3df bbox;
			stream->readFloatArray(&bbox.MinEdge.X, 3);
			stream->readFloatArray(&bbox.MaxEdge.X, 3);

			u32 vtxCount = stream->readUInt();
			u32 vtxSize = stream->readUInt();
			u32 vtxBufferSize = vtxCount * vtxSize;

			u32 idxCount = stream->readUInt();
			u32 idxSize = stream->readUInt();
			u32 idxBufferSize = idxCount * idxSize;

			E_INDEX_TYPE indexType = video::EIT_16BIT;
			if (idxSize == 4)
				indexType = video::EIT_32BIT;

			std::string vertexTypeName = stream->readString();

			IMeshBuffer* mb = NULL;

			bool vertexCompatible = true;

			IVertexDescriptor* vertexDes = getVideoDriver()->getVertexDescriptor(vertexTypeName.c_str());
			if (vertexDes != NULL)
			{
				if (vertexDes->getVertexSize(0) == vtxSize)
				{
					E_VERTEX_TYPE vtxType = (E_VERTEX_TYPE)vertexDes->getID();
					switch (vtxType)
					{
					case EVT_STANDARD:
						mb = new CMeshBuffer<S3DVertex>(vertexDes, indexType);
						break;
					case EVT_2TCOORDS:
						mb = new CMeshBuffer<S3DVertex2TCoords>(vertexDes, indexType);
						break;
					case EVT_TANGENTS:
						mb = new CMeshBuffer<S3DVertexTangents>(vertexDes, indexType);
						break;
					case EVT_SKIN:
						mb = new CMeshBuffer<S3DVertexSkin>(vertexDes, indexType);
						break;
					case EVT_SKIN_TANGENTS:
						mb = new CMeshBuffer<S3DVertexSkinTangents>(vertexDes, indexType);
						break;
					case EVT_2TCOORDS_TANGENTS:
						mb = new CMeshBuffer<S3DVertex2TCoordsTangents>(vertexDes, indexType);
						break;
					}
				}
				else
				{
					os::Printer::log("[CRenderMeshData::deserializable] Vertex is not compatible");
					vertexCompatible = false;
				}
			}

			u32 numAttribute = stream->readUInt();
			for (u32 j = 0; j < numAttribute; j++)
			{
				std::string attributeName = stream->readString();
				short offset = stream->readShort();
				short elementCount = stream->readShort();
				short typeSize = stream->readShort();
			}

			if (mb != NULL)
			{
				IVertexBuffer* vtxBuffer = mb->getVertexBuffer();
				IIndexBuffer* idxBuffer = mb->getIndexBuffer();

				vtxBuffer->set_used(vtxCount);
				stream->readData(vtxBuffer->getVertices(), vtxBufferSize);

				idxBuffer->set_used(idxCount);
				stream->readData(idxBuffer->getIndices(), idxBufferSize);

				mb->getBoundingBox() = bbox;
				mb->setHardwareMappingHint(EHM_STATIC);

				video::SMaterial& irrMaterial = mb->getMaterial();

				for (int t = 0; t < 3; t++)
				{
					if (textures[t].empty() == false)
					{
						ITexture* texture = CTextureManager::getInstance()->getTexture(textures[t].c_str(), importer->getTextureFolder());
						if (texture != NULL)
							irrMaterial.setTexture(t, texture);

						if (t == 0 && texture != NULL)
						{
							if (IsSkinnedMesh == false)
								irrMaterial.MaterialType = shaderMgr->getShaderIDByName("TextureColor");
							else
								irrMaterial.MaterialType = shaderMgr->getShaderIDByName("Skin");
						}
					}
				}

				RenderMesh->addMeshBuffer(mb, material.c_str());
				RenderMesh->recalculateBoundingBox();

				mb->drop();
			}
		}

		return true;
	}
}