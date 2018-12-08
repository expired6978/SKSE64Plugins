#ifdef FIXME

#include "CDXNifMesh.h"
#include "CDXScene.h"
#include "CDXShader.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiRTTI.h"
#include "skse64/NiExtraData.h"

#include "NifUtils.h"

#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <time.h>

CDXNifMesh::CDXNifMesh() : CDXEditableMesh()
{
	m_material = NULL;
	m_geometry = NULL;
	m_morphable = false;
}

CDXNifMesh::~CDXNifMesh()
{
	if (m_geometry) {
		m_geometry->DecRef();
	}
}

NiGeometry * CDXNifMesh::GetNifGeometry()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_geometry;
}

bool CDXNifMesh::IsMorphable()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_morphable;
}

CDXNifMesh * CDXNifMesh::Create(ID3D11Device * pDevice, NiGeometry * geometry)
{
	UInt32 vertCount = 0;
	UInt32 triangleCount = 0;
	ID3D11Buffer * vertexBuffer = NULL;
	ID3D11Buffer * indexBuffer = NULL;
	LPDIRECT3DBASETEXTURE9 diffuseTexture = NULL;
	UInt16 alphaFlags = 0;
	UInt8 alphaThreshold = 0;
	UInt32 shaderFlags1 = 0;
	UInt32 shaderFlags2 = 0;

	CDXNifMesh * nifMesh = new CDXNifMesh;

	if (geometry) {
		geometry->IncRef();
		nifMesh->m_geometry = geometry;
	}

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
				NiTransform localTransform = GetGeometryTransform(geometry);
				BSLightingShaderProperty * shaderProperty = niptr_cast<BSLightingShaderProperty>(geometry->m_spEffectState);
				if (shaderProperty) {
					BSLightingShaderMaterial * material = shaderProperty->material;
					if (material) {
						NiTexture * diffuse = niptr_cast<NiTexture>(material->diffuse);
						if (diffuse) {
							NiTexture::RendererData * rendererData = diffuse->rendererData;
							if (rendererData) {
								NiTexture::NiDX9TextureData * dx9RendererData = (NiTexture::NiDX9TextureData *)rendererData;
								diffuseTexture = dx9RendererData->texture;
							}
						}
					}

					shaderFlags1 = shaderProperty->shaderFlags1;
					shaderFlags2 = shaderProperty->shaderFlags2;
				}

				NiAlphaProperty * alphaProperty = niptr_cast<NiAlphaProperty>(geometry->m_spPropertyState);
				if (alphaProperty) {
					alphaFlags = alphaProperty->alphaFlags;
					alphaThreshold = alphaProperty->alphaThreshold;
				}

				vertCount = geometryData->m_usVertices;
				triangleCount = geometryData->m_usTriangles;

				nifMesh->m_vertCount = vertCount;
				nifMesh->m_primitiveCount = triangleCount;

				BSFaceGenBaseMorphExtraData * morphData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
				if (morphData) {
					nifMesh->m_morphable = true;
				}

				pDevice->CreateVertexBuffer(geometryData->m_usVertices*sizeof(CDXMeshVert), 0, 0, D3DPOOL_MANAGED, &vertexBuffer, NULL);

				CDXMeshVert* pVertices = NULL;
				CDXMeshIndex* pIndices = NULL;

				if (triShapeData)
					pDevice->CreateIndexBuffer(triangleCount*sizeof(CDXMeshFace), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indexBuffer, NULL);
				else if (triStripsData)
					pDevice->CreateIndexBuffer(GetStripLengthSum(triStripsData)*sizeof(CDXMeshIndex), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indexBuffer, NULL);

				// lock i_buffer and load the indices into it
				indexBuffer->Lock(0, 0, (void**)&pIndices, 0);
				if (triShapeData) {
					memcpy(pIndices, triShapeData->m_pusTriList, triShapeData->m_uiTriListLength*sizeof(CDXMeshIndex));
					nifMesh->m_primitiveType = D3DPT_TRIANGLELIST;
				}
				else if (triStripsData) {
					memcpy(pIndices, triStripsData->m_pusStripLists, GetStripLengthSum(triStripsData)*sizeof(CDXMeshIndex));
					nifMesh->m_primitiveCount = GetStripLengthSum(triStripsData);
					nifMesh->m_primitiveType = D3DPT_TRIANGLESTRIP;
				}
				nifMesh->m_indexBuffer = indexBuffer;
				indexBuffer->Unlock();

				vertexBuffer->Lock(0, 0, (void**)&pVertices, 0);
				for (UInt32 i = 0; i < vertCount; i++) {
					NiPoint3 xformed = localTransform * geometryData->m_pkVertex[i];
					NiPoint2 uv = geometryData->m_pkTexture[i];
					pVertices[i].Position = *(DirectX::XMFLOAT3*)&xformed;
					DirectX::XMFLOAT3 vNormal(0, 0, 0);
					pVertices[i].Normal = vNormal;
					pVertices[i].Tex = *(DirectX::XMFLOAT2*)&uv;
					pVertices[i].Color = COLOR_UNSELECTED;

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

				nifMesh->m_vertexBuffer = vertexBuffer;

				// Don't need edge table if not editable
				if (nifMesh->m_morphable) {
					CDXEdgeMap edges;
					for (UInt32 f = 0; f < triangleCount; f++) {

						if (triShapeData) {
							CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
							auto it = edges.emplace(CDXMeshEdge(min(face->v1, face->v2), max(face->v1, face->v2)), 1);
							if (it.second == false)
								it.first->second++;
							it = edges.emplace(CDXMeshEdge(min(face->v2, face->v3), max(face->v2, face->v3)), 1);
							if (it.second == false)
								it.first->second++;
							it = edges.emplace(CDXMeshEdge(min(face->v3, face->v1), max(face->v3, face->v1)), 1);
							if (it.second == false)
								it.first->second++;
						}
						else if (triStripsData) {
							UInt16 v1 = 0, v2 = 0, v3 = 0;
							GetTriangleIndices(triStripsData, f, v1, v2, v3);
							auto it = edges.emplace(CDXMeshEdge(min(v1, v2), max(v1, v2)), 1);
							if (it.second == false)
								it.first->second++;
							it = edges.emplace(CDXMeshEdge(min(v2, v3), max(v2, v3)), 1);
							if (it.second == false)
								it.first->second++;
							it = edges.emplace(CDXMeshEdge(min(v3, v1), max(v3, v1)), 1);
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
						DirectX::XMFLOAT3 vNormal(0, 0, 0);
						if (!geometryData->m_pkNormal)
							vNormal = nifMesh->CalculateVertexNormal(i);
						else
							vNormal = *(DirectX::XMFLOAT3*)&geometryData->m_pkNormal[i];

						pVertices[i].Normal = vNormal;
					}
				}

				vertexBuffer->Unlock();

				CDXMaterial * material = new CDXMaterial;
				material->SetDiffuseTexture(diffuseTexture);
				material->SetSpecularColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
				material->SetAmbientColor(DirectX::XMFLOAT3(0.2f, 0.2f, 0.2f));
				material->SetDiffuseColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
				material->SetWireframeColor(DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f));
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

void CDXNifMesh::Pass(ID3D11Device * pDevice, UInt32 iPass, CDXShader * shader)
{
	ID3DXEffect * effect = shader->GetEffect();
	if (m_material) {
		effect->SetValue(shader->m_hSpecular, m_material->GetSpecularColor(), sizeof(CDXVec3));
		effect->SetValue(shader->m_hAmbient, m_material->GetAmbientColor(), sizeof(CDXVec3));
		effect->SetValue(shader->m_hDiffuse, m_material->GetDiffuseColor(), sizeof(CDXVec3));
		effect->SetValue(shader->m_hWireframeColor, m_material->GetWireframeColor(), sizeof(CDXVec3));

		UInt32 alphaFunc = mappedTestFunctions[m_material->GetTestMode()];
		UInt32 srcBlend = mappedAlphaFunctions[m_material->GetSrcBlendMode()];
		UInt32 destBlend = mappedAlphaFunctions[m_material->GetDestBlendMode()];

		bool isDoubleSided = (m_material->GetShaderFlags2() & BSShaderProperty::kSLSF2_Double_Sided) == BSShaderProperty::kSLSF2_Double_Sided;
		bool zBufferTest = (m_material->GetShaderFlags1() & BSShaderProperty::kSLSF1_ZBuffer_Test) == BSShaderProperty::kSLSF1_ZBuffer_Test;
		bool zBufferWrite = (m_material->GetShaderFlags2() & BSShaderProperty::kSLSF2_ZBuffer_Write) == BSShaderProperty::kSLSF2_ZBuffer_Write;

		pDevice->SetRenderState(D3DRS_ZENABLE, zBufferTest ? TRUE : FALSE);
		pDevice->SetRenderState(D3DRS_ZWRITEENABLE, zBufferWrite ? TRUE : FALSE);
		pDevice->SetRenderState(D3DRS_CULLMODE, isDoubleSided ? D3DCULL_NONE : D3DCULL_CW);
		pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, m_material->GetAlphaBlending() ? TRUE : FALSE);
		pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, m_material->GetAlphaTesting() ? TRUE : FALSE);
		pDevice->SetRenderState(D3DRS_ALPHAREF, m_material->GetAlphaThreshold());
		pDevice->SetRenderState(D3DRS_ALPHAFUNC, alphaFunc);
		pDevice->SetRenderState(D3DRS_SRCBLEND, srcBlend);
		pDevice->SetRenderState(D3DRS_DESTBLEND, destBlend);
	}
	effect->CommitChanges();

	CDXMesh::Pass(pDevice, iPass, shader);
}

#endif