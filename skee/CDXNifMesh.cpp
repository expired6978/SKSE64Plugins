#include "CDXNifMesh.h"
#include "CDXNifMaterial.h"
#include "CDXScene.h"
#include "CDXShader.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiRenderer.h"

#include "NifUtils.h"

#include <thread>
#include <mutex>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "half.hpp"

#include <d3d11_3.h>

using namespace DirectX;

CDXNifMesh::CDXNifMesh() : CDXEditableMesh()
{
	m_material = nullptr;
	m_morphable = false;
}

CDXNifMesh::~CDXNifMesh()
{
	
}

CDXMeshVert * CDXNifMesh::LockVertices(const LockMode type)
{
	EnterCriticalSection(&g_renderManager->lock);
	return CDXMesh::LockVertices(type);
}

CDXMeshIndex * CDXNifMesh::LockIndices()
{
	EnterCriticalSection(&g_renderManager->lock);
	return CDXMesh::LockIndices();
}

void CDXNifMesh::UnlockVertices(const LockMode type)
{
	CDXMesh::UnlockVertices(type);
	LeaveCriticalSection(&g_renderManager->lock);
}
void CDXNifMesh::UnlockIndices(bool write)
{
	CDXMesh::UnlockIndices(write);
	LeaveCriticalSection(&g_renderManager->lock);
}

CDXBSTriShapeMesh::CDXBSTriShapeMesh()
{
	m_geometry = nullptr;
}

CDXBSTriShapeMesh::~CDXBSTriShapeMesh()
{
	
}

CDXBSTriShapeMesh * CDXBSTriShapeMesh::Create(CDXD3DDevice * pDevice, BSTriShape * geometry)
{
	UInt32 vertCount = 0;
	UInt32 triangleCount = 0;

	UInt16 alphaFlags = 0;
	UInt8 alphaThreshold = 0;
	UInt32 shaderFlags1 = 0;
	UInt32 shaderFlags2 = 0;

	CDXBSTriShapeMesh * nifMesh = new CDXBSTriShapeMesh;
	nifMesh->m_geometry = geometry;
	BSShaderMaterial * material = nullptr;

	if (geometry)
	{
		// Pre-transform
		NiTransform localTransform = GetGeometryTransform(geometry);
		const BSLightingShaderProperty * shaderProperty = ni_cast(geometry->m_spEffectState, BSLightingShaderProperty);
		if (shaderProperty) {
			material = shaderProperty->material;
			shaderFlags1 = shaderProperty->shaderFlags1;
			shaderFlags2 = shaderProperty->shaderFlags2;
		}

		const NiAlphaProperty * alphaProperty = ni_cast(geometry->m_spPropertyState, NiAlphaProperty);
		if (alphaProperty) {
			alphaFlags = alphaProperty->alphaFlags;
			alphaThreshold = alphaProperty->alphaThreshold;
		}

		const NiSkinInstance * skinInstance = geometry->m_spSkinInstance.m_pObject;
		if (!skinInstance) {
			delete nifMesh;
			return nullptr;
		}

		const NiSkinPartition * skinPartition = skinInstance->m_spSkinPartition.m_pObject;
		if (!skinPartition) {
			delete nifMesh;
			return nullptr;
		}

		std::vector<CDXMeshIndex> indices;
		for (UInt32 p = 0; p < skinPartition->m_uiPartitions; ++p)
		{
			for (UInt32 t = 0; t < skinPartition->m_pkPartitions[p].m_usTriangles * 3; ++t)
			{
				indices.push_back(skinPartition->m_pkPartitions[p].m_pusTriList[t]);
			}
		}

		vertCount = geometry->numVertices;
		triangleCount = indices.size();

		nifMesh->m_vertCount = vertCount;
		nifMesh->m_indexCount = triangleCount;

		BSFaceGenBaseMorphExtraData * morphData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
		if (morphData) {
			nifMesh->m_morphable = true;
		}

		nifMesh->InitializeBuffers(pDevice, nifMesh->m_vertCount, nifMesh->m_indexCount, [&](CDXMeshVert* pVertices, CDXMeshIndex* pIndices)
		{
			nifMesh->m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			memcpy(pIndices, &indices.at(0), indices.size() * sizeof(CDXMeshIndex));

			BSDynamicTriShape * dynamicTriShape = ni_cast(geometry, BSDynamicTriShape);
			UInt32 vertexSize = NiSkinPartition::GetVertexSize(geometry->vertexDesc);
			UInt32 uvOffset = NiSkinPartition::GetVertexAttributeOffset(geometry->vertexDesc, VertexAttribute::VA_TEXCOORD0);

			for (UInt32 i = 0; i < vertCount; i++) {
				NiPoint3 * vertex = dynamicTriShape ? reinterpret_cast<NiPoint3*>(&reinterpret_cast<DirectX::XMFLOAT4*>(dynamicTriShape->diffBlock)[i]) : reinterpret_cast<NiPoint3*>(&skinPartition->m_pkPartitions[0].shapeData->m_RawVertexData[i * vertexSize]);
				NiPoint3 xformed = localTransform * (*vertex);
				struct UVCoord
				{
					half_float::half u;
					half_float::half v;
				};
				UVCoord * texCoord = reinterpret_cast<UVCoord*>(&skinPartition->m_pkPartitions[0].shapeData->m_RawVertexData[i * vertexSize + uvOffset]);
				DirectX::XMFLOAT2 uv{ texCoord->u, texCoord->v };
				pVertices[i].Position = *(DirectX::XMFLOAT3*)&xformed;
				pVertices[i].Normal = DirectX::XMFLOAT3(0,0,0);
				pVertices[i].Tex = uv;
				XMStoreFloat3(&pVertices[i].Color, COLOR_UNSELECTED);
			}
		});

		nifMesh->BuildAdjacency();
		if (nifMesh->IsMorphable()) {
			nifMesh->BuildFacemap();
			nifMesh->BuildNormals();
		}

		CDXNifMaterial * meshMaterial = new CDXNifMaterial;
		meshMaterial->SetWireframeColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		meshMaterial->SetShaderFlags1(shaderFlags1);
		meshMaterial->SetShaderFlags2(shaderFlags2);
		if (alphaFlags != 0) {
			meshMaterial->SetFlags(alphaFlags);
			meshMaterial->SetAlphaThreshold(alphaThreshold);
		}

		const BSLightingShaderProperty * lightingShaderProperty = ni_cast(geometry->m_spEffectState, BSLightingShaderProperty);
		if (lightingShaderProperty) {
			BSLightingShaderMaterial * lightingMaterial = static_cast<BSLightingShaderMaterial*>(material);
			NiTexture * textures[] = { lightingMaterial->texture1, lightingMaterial->texture2, lightingMaterial->texture3 };
			for (UInt32 i = 0; i < sizeof(textures) / sizeof(NiTexture*); ++i)
			{
				if (textures[i]) {
					meshMaterial->SetNiTexture(i, textures[i]);
				}
			}
		}

		if (material) {
			switch(material->GetShaderType())
			{
				case BSShaderMaterial::kShaderType_FaceGen:
				{
					const BSLightingShaderMaterialFacegen * tintMaterial = static_cast<BSLightingShaderMaterialFacegen*>(material);
					if (tintMaterial->renderedTexture) {
						meshMaterial->SetNiTexture(4, tintMaterial->renderedTexture);
					}
					break;
				}
				case BSShaderMaterial::kShaderType_FaceGenRGBTint:
				{
					const BSLightingShaderMaterialFacegenTint * tintMaterial = static_cast<BSLightingShaderMaterialFacegenTint*>(material);
					meshMaterial->SetTintColor(DirectX::XMFLOAT4(tintMaterial->tintColor.r, tintMaterial->tintColor.g, tintMaterial->tintColor.b, 1.0f));
					break;
				}
				case BSShaderMaterial::kShaderType_HairTint:
				{
					const BSLightingShaderMaterialHairTint * tintMaterial = static_cast<BSLightingShaderMaterialHairTint*>(material);
					meshMaterial->SetTintColor(DirectX::XMFLOAT4(tintMaterial->tintColor.r, tintMaterial->tintColor.g, tintMaterial->tintColor.b, 1.0f));
					break;
				}
			}
		}

		nifMesh->SetMaterial(meshMaterial);
	}

	if (!nifMesh->IsMorphable())
		nifMesh->SetLocked(true);

	return nifMesh;
}

const char * CDXBSTriShapeMesh::GetName() const
{
	return m_geometry ? m_geometry->m_name : "";
}

CDXLegacyNifMesh::CDXLegacyNifMesh()
{
	m_geometry = nullptr;
}

CDXLegacyNifMesh::~CDXLegacyNifMesh()
{
	
}

CDXLegacyNifMesh * CDXLegacyNifMesh::Create(CDXD3DDevice * pDevice, NiGeometry * geometry)
{
	UInt32 vertCount = 0;
	UInt32 triangleCount = 0;

	ID3D11ShaderResourceView * diffuseTexture = nullptr;

	UInt16 alphaFlags = 0;
	UInt8 alphaThreshold = 0;
	UInt32 shaderFlags1 = 0;
	UInt32 shaderFlags2 = 0;

	CDXLegacyNifMesh * nifMesh = new CDXLegacyNifMesh;
	nifMesh->m_geometry = geometry;

	if (geometry)
	{
		NiTriBasedGeomData * geometryData = niptr_cast<NiTriBasedGeomData>(geometry->m_spModelData);
		if (geometryData)
		{
			NiTriShapeData * triShapeData = ni_cast(geometryData, NiTriShapeData);
			NiTriStripsData * triStripsData = ni_cast(geometryData, NiTriStripsData);
			if (triShapeData || triStripsData)
			{
				// Pre-transform
				NiTransform localTransform = GetLegacyGeometryTransform(geometry);
				BSLightingShaderProperty * shaderProperty = ni_cast(geometry->m_spEffectState, BSLightingShaderProperty);
				if (shaderProperty) {
					BSLightingShaderMaterial * material = shaderProperty->material;
					if (material) {
						NiTexture * diffuse = material->texture1;
						if (diffuse) {
							NiTexture::RendererData * rendererData = diffuse->rendererData;
							if (rendererData) {
								diffuseTexture = rendererData->resourceView;
							}
						}
					}

					shaderFlags1 = shaderProperty->shaderFlags1;
					shaderFlags2 = shaderProperty->shaderFlags2;
				}

				NiAlphaProperty * alphaProperty = ni_cast(geometry->m_spPropertyState, NiAlphaProperty);
				if (alphaProperty) {
					alphaFlags = alphaProperty->alphaFlags;
					alphaThreshold = alphaProperty->alphaThreshold;
				}

				vertCount = geometryData->m_usVertices;
				triangleCount = geometryData->m_usTriangles;

				nifMesh->m_vertCount = vertCount;
				nifMesh->m_indexCount = triangleCount;

				BSFaceGenBaseMorphExtraData * morphData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
				if (morphData) {
					nifMesh->m_morphable = true;
				}

				nifMesh->InitializeBuffers(pDevice, nifMesh->m_vertCount, nifMesh->m_indexCount, [&](CDXMeshVert* pVertices, CDXMeshIndex* pIndices)
				{
					if (triShapeData)
					{
						nifMesh->m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
						memcpy(pIndices, triShapeData->m_pusTriList, triShapeData->m_uiTriListLength * sizeof(CDXMeshIndex));
					}
					else if (triStripsData)
					{
						nifMesh->m_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
						memcpy(pIndices, triStripsData->m_pusStripLists, GetStripLengthSum(triStripsData) * sizeof(CDXMeshIndex));
					}

					for (UInt32 i = 0; i < vertCount; i++) {
						NiPoint3 xformed = localTransform * geometryData->m_pkVertex[i];
						NiPoint2 uv = geometryData->m_pkTexture[i];
						pVertices[i].Position = *(DirectX::XMFLOAT3*)&xformed;
						DirectX::XMFLOAT3 vNormal(0, 0, 0);
						pVertices[i].Normal = vNormal;
						pVertices[i].Tex = *(DirectX::XMFLOAT2*)&uv;
						XMStoreFloat3(&pVertices[i].Color, COLOR_UNSELECTED);

						// Build adjacency table
						if (nifMesh->m_morphable) {
							for (UInt32 f = 0; f < triangleCount; f++) {
								if (triShapeData) {
									CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
									if (i == face->v1 || i == face->v2 || i == face->v3)
										nifMesh->m_adjacency[i].push_back(*face);
								}
								else if (triStripsData) {
									UInt16 v1 = 0, v2 = 0, v3 = 0;
									GetTriangleIndices(triStripsData, f, v1, v2, v3);
									if (i == v1 || i == v2 || i == v3)
										nifMesh->m_adjacency[i].push_back(CDXMeshFace(v1, v2, v3));
								}
							}
						}
					}

					// Don't need edge table if not editable
					if (nifMesh->m_morphable) {
						CDXEdgeMap edges;
						for (UInt32 f = 0; f < triangleCount; f++) {

							if (triShapeData) {
								CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
								auto it = edges.emplace(CDXMeshEdge(std::min(face->v1, face->v2), std::max(face->v1, face->v2)), 1);
								if (it.second == false)
									it.first->second++;
								it = edges.emplace(CDXMeshEdge(std::min(face->v2, face->v3), std::max(face->v2, face->v3)), 1);
								if (it.second == false)
									it.first->second++;
								it = edges.emplace(CDXMeshEdge(std::min(face->v3, face->v1), std::max(face->v3, face->v1)), 1);
								if (it.second == false)
									it.first->second++;
							}
							else if (triStripsData) {
								UInt16 v1 = 0, v2 = 0, v3 = 0;
								GetTriangleIndices(triStripsData, f, v1, v2, v3);
								auto it = edges.emplace(CDXMeshEdge(std::min(v1, v2), std::max(v1, v2)), 1);
								if (it.second == false)
									it.first->second++;
								it = edges.emplace(CDXMeshEdge(std::min(v2, v3), std::max(v2, v3)), 1);
								if (it.second == false)
									it.first->second++;
								it = edges.emplace(CDXMeshEdge(std::min(v3, v1), std::max(v3, v1)), 1);
								if (it.second == false)
									it.first->second++;
							}
						}

						for (auto e : edges) {
							if (e.second == 1) {
								nifMesh->m_vertexEdges.insert(e.first.p1);
								nifMesh->m_vertexEdges.insert(e.first.p2);
							}
						}
					}

					// Only need vertex normals when it's editable
					if (nifMesh->m_morphable) {
						for (UInt32 i = 0; i < vertCount; i++) {
							// Setup normals
							CDXVec vNormal = XMVectorSet(0, 0, 0, 0);
							if (!geometryData->m_pkNormal)
								XMStoreFloat3(&pVertices[i].Normal, nifMesh->CalculateVertexNormal(i));
							else
								XMStoreFloat3(&pVertices[i].Normal, XMLoadFloat3((XMFLOAT3*)&geometryData->m_pkNormal[i]));
						}
					}
				});
				
				CDXMaterial * material = new CDXMaterial;
				material->SetTexture(0, diffuseTexture);
				material->SetWireframeColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
				material->SetShaderFlags1(shaderFlags1);
				material->SetShaderFlags2(shaderFlags2);
				if (alphaFlags != 0) {
					material->SetFlags(alphaFlags);
					material->SetAlphaThreshold(alphaThreshold);
				}

				nifMesh->m_material = material;
			}
		}
	}

	if (!nifMesh->m_morphable)
		nifMesh->SetLocked(true);

	return nifMesh;
}

const char * CDXLegacyNifMesh::GetName() const
{
	return m_geometry ? m_geometry->m_name : "";
}
