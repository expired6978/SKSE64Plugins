#include "CDXNifMesh.h"
#include "CDXNifMaterial.h"
#include "CDXScene.h"
#include "CDXShader.h"
#include "CDXTriangleList.h"

#include "RE/N/NiGeometry.h"
#include "RE/N/NiRTTI.h"
#include "RE/N/NiExtraData.h"

#include "NifUtils.h"

#include <thread>
#include <mutex>
#include <vector>
#include <stdexcept>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "half.hpp"

#include "REX/W32/D3D11_3.h"
#include <cstdint>



using namespace DirectX;

CDXNifMesh::CDXNifMesh() : CDXEditableMesh()
{
	m_material = nullptr;
	m_morphable = false;
}

CDXNifMesh::~CDXNifMesh()
{
	
}

CDXBSTriShapeMesh::CDXBSTriShapeMesh()
{
	m_geometry = nullptr;
}

CDXBSTriShapeMesh::~CDXBSTriShapeMesh()
{
	
}

CDXBSTriShapeMesh * CDXBSTriShapeMesh::Create(CDXD3DDevice * pDevice, RE::BSTriShape * geometry)
{
	std::uint32_t vertCount = 0;
	std::uint32_t triangleCount = 0;

	std::uint16_t alphaFlags = 0;
	std::uint8_t alphaThreshold = 0;
	std::uint32_t shaderFlags1 = 0;
	std::uint32_t shaderFlags2 = 0;

	if (!geometry) return nullptr;
	auto candidate = std::make_unique<CDXBSTriShapeMesh>();
	auto* nifMesh = candidate.get();
	nifMesh->m_geometry.reset(geometry);
	RE::BSShaderMaterial * material = nullptr;

	if (geometry)
	{
		// Pre-transform
		RE::NiTransform localTransform = GetGeometryTransform(geometry);
		const RE::BSLightingShaderProperty * shaderProperty = netimmerse_cast<RE::BSLightingShaderProperty*>(geometry->shaderProperty.get());
		if (shaderProperty) {
			material = shaderProperty->material;
			std::uint64_t sf = shaderProperty->flags.underlying();
			shaderFlags1 = static_cast<std::uint32_t>(sf & 0xFFFFFFFF);
			shaderFlags2 = static_cast<std::uint32_t>(sf >> 32);
		}

		const RE::NiAlphaProperty * alphaProperty = geometry->alphaProperty.get();
		if (alphaProperty) {
			alphaFlags = alphaProperty->alphaFlags;
			alphaThreshold = alphaProperty->alphaThreshold;
		}

		const RE::NiSkinInstance * skinInstance = geometry->skinInstance.get();
		if (!skinInstance) {
			return nullptr;
		}

		const RE::NiSkinPartition * skinPartition = skinInstance->skinPartition.get();
		if (!skinPartition || !skinPartition->numPartitions || !skinPartition->partitions ||
			!skinPartition->partitions[0].buffData || !skinPartition->partitions[0].buffData->rawVertexData) {
			return nullptr;
		}

		std::vector<CDXMeshIndex> indices;
		for (std::uint32_t p = 0; p < skinPartition->numPartitions; ++p)
		{
			if (skinPartition->partitions[p].triangles && !skinPartition->partitions[p].triList) return nullptr;
			for (std::uint32_t t = 0; t < skinPartition->partitions[p].triangles * 3; ++t)
			{
				indices.push_back(skinPartition->partitions[p].triList[t]);
			}
		}

		vertCount = geometry->vertexCount ? geometry->vertexCount : skinPartition->vertexCount;
		triangleCount = indices.size();

		RE::BSFaceGenBaseMorphExtraData * morphData = (RE::BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
		if (morphData) {
			nifMesh->m_morphable = true;
		}

		if (!nifMesh->InitializeBuffers(pDevice, vertCount, triangleCount, [&](CDXMeshVert* pVertices, CDXMeshIndex* pIndices)
		{
			nifMesh->m_topology = REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			memcpy(pIndices, &indices.at(0), indices.size() * sizeof(CDXMeshIndex));

			RE::BSDynamicTriShape * dynamicTriShape = geometry ? geometry->AsDynamicTriShape() : nullptr;
			std::uint32_t vertexSize = geometry->vertexDesc.GetSize();
			std::uint32_t vertOffset = geometry->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);
			std::uint32_t uvOffset = geometry->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_TEXCOORD0);

			if (dynamicTriShape && !dynamicTriShape->dynamicData) throw std::runtime_error("Missing dynamic vertex data");
			// Lock ownership is lexical even if the fill later throws.
			struct DynamicReadGuard {
				RE::BSDynamicTriShape* shape;
				explicit DynamicReadGuard(RE::BSDynamicTriShape* value) : shape(value) { if (shape) shape->lock.Lock(); }
				~DynamicReadGuard() { if (shape) shape->lock.Unlock(); }
				DynamicReadGuard(const DynamicReadGuard&) = delete;
				DynamicReadGuard& operator=(const DynamicReadGuard&) = delete;
			} dynamicLock(dynamicTriShape);
			for (std::uint32_t i = 0; i < vertCount; i++) {
				RE::NiPoint3 * vertex = dynamicTriShape ? reinterpret_cast<RE::NiPoint3*>(&reinterpret_cast<DirectX::XMFLOAT4*>(dynamicTriShape->dynamicData)[i]) : reinterpret_cast<RE::NiPoint3*>(&skinPartition->partitions[0].buffData->rawVertexData[i * vertexSize + vertOffset]);
				RE::NiPoint3 xformed = localTransform * (*vertex);
				struct UVCoord
				{
					half_float::half u;
					half_float::half v;
				};
				UVCoord * texCoord = reinterpret_cast<UVCoord*>(&skinPartition->partitions[0].buffData->rawVertexData[i * vertexSize + uvOffset]);
				DirectX::XMFLOAT2 uv{ texCoord->u, texCoord->v };
				pVertices[i].Position = *(DirectX::XMFLOAT3*)&xformed;
				pVertices[i].Normal = DirectX::XMFLOAT3(0,0,0);
				pVertices[i].Tex = uv;
				XMStoreFloat3(&pVertices[i].Color, COLOR_UNSELECTED);
			}
		})) return nullptr;

		nifMesh->BuildAdjacency();
		if (nifMesh->IsMorphable()) {
			nifMesh->BuildFacemap();
			nifMesh->BuildNormals();
		}

		std::shared_ptr<CDXNifMaterial> meshMaterial = std::make_shared<CDXNifMaterial>();
		meshMaterial->SetWireframeColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		meshMaterial->SetShaderFlags1(shaderFlags1);
		meshMaterial->SetShaderFlags2(shaderFlags2);
		if (alphaFlags != 0) {
			meshMaterial->SetFlags(alphaFlags);
			meshMaterial->SetAlphaThreshold(alphaThreshold);
		}

		const RE::BSLightingShaderProperty * lightingShaderProperty = netimmerse_cast<RE::BSLightingShaderProperty*>(geometry->shaderProperty.get());
		if (lightingShaderProperty) {
			RE::BSLightingShaderMaterial * lightingMaterial = static_cast<RE::BSLightingShaderMaterial*>(material);
			RE::NiTexture * textures[] = { lightingMaterial->diffuseTexture.get(), lightingMaterial->normalTexture.get(), lightingMaterial->rimSoftLightingTexture.get() };
			for (std::uint32_t i = 0; i < sizeof(textures) / sizeof(RE::NiTexture*); ++i)
			{
				if (textures[i]) {
					meshMaterial->SetNiTexture(i, textures[i]);
				}
			}
		}

		if (material) {
			switch(material->GetFeature())
			{
				case RE::BSShaderMaterial::Feature::kFaceGen:
				{
					const RE::BSLightingShaderMaterialFacegen * tintMaterial = static_cast<RE::BSLightingShaderMaterialFacegen*>(material);
					if (tintMaterial->tintTexture) {
						meshMaterial->SetNiTexture(4, tintMaterial->tintTexture.get());
					}
					break;
				}
				case RE::BSShaderMaterial::Feature::kFaceGenRGBTint:
				{
					const RE::BSLightingShaderMaterialFacegenTint * tintMaterial = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(material);
					meshMaterial->SetTintColor(DirectX::XMFLOAT4(tintMaterial->tintColor.red, tintMaterial->tintColor.green, tintMaterial->tintColor.blue, 1.0f));
					break;
				}
				case RE::BSShaderMaterial::Feature::kHairTint:
				{
					const RE::BSLightingShaderMaterialHairTint * tintMaterial = static_cast<RE::BSLightingShaderMaterialHairTint*>(material);
					meshMaterial->SetTintColor(DirectX::XMFLOAT4(tintMaterial->tintColor.red, tintMaterial->tintColor.green, tintMaterial->tintColor.blue, 1.0f));
					break;
				}
			}
		}

		nifMesh->SetMaterial(meshMaterial);
	}

	if (!nifMesh->IsMorphable())
		nifMesh->SetLocked(true);

	return candidate.release();
}

const char * CDXBSTriShapeMesh::GetName() const
{
	return m_geometry ? m_geometry->name.c_str() : "";
}

CDXLegacyNifMesh::CDXLegacyNifMesh()
{
	m_geometry = nullptr;
}

CDXLegacyNifMesh::~CDXLegacyNifMesh()
{
	
}

CDXLegacyNifMesh * CDXLegacyNifMesh::Create(CDXD3DDevice * pDevice, RE::NiGeometry * geometry)
{
	std::uint32_t vertCount = 0;
	std::uint32_t triangleCount = 0;

	REX::W32::ID3D11ShaderResourceView * diffuseTexture = nullptr;

	std::uint16_t alphaFlags = 0;
	std::uint8_t alphaThreshold = 0;
	std::uint32_t shaderFlags1 = 0;
	std::uint32_t shaderFlags2 = 0;

	if (!geometry) return nullptr;
	auto candidate = std::make_unique<CDXLegacyNifMesh>();
	auto* nifMesh = candidate.get();
	nifMesh->m_geometry.reset(geometry);

	if (geometry)
	{
		RE::NiGeometryData * geomDataBase = geometry->spModelData.get();
		RE::NiTriBasedGeomData * geometryData = netimmerse_cast<RE::NiTriBasedGeomData*>(geomDataBase);
		if (geometryData)
		{
			RE::NiTriShapeData * triShapeData = netimmerse_cast<RE::NiTriShapeData*>(geometryData);
			RE::NiTriStripsData * triStripsData = netimmerse_cast<RE::NiTriStripsData*>(geometryData);
			if (triShapeData || triStripsData)
			{
				// Pre-transform
				RE::NiTransform localTransform = GetLegacyGeometryTransform(geometry);
				RE::BSLightingShaderProperty * shaderProperty = netimmerse_cast<RE::BSLightingShaderProperty*>(geometry->spEffectState.get());
				if (shaderProperty) {
					RE::BSLightingShaderMaterial * material = static_cast<RE::BSLightingShaderMaterial*>(shaderProperty->material);
					if (material) {
						RE::NiTexture * diffuse = material->diffuseTexture.get();
						if (diffuse) {
							auto srcTex = static_cast<RE::NiSourceTexture*>(diffuse);
							RE::BSGraphics::Texture * rendererData = srcTex ? srcTex->rendererTexture : nullptr;
							if (rendererData) {
								diffuseTexture = rendererData->resourceView;
							}
						}
					}

					std::uint64_t sf = shaderProperty->flags.underlying();
					shaderFlags1 = static_cast<std::uint32_t>(sf & 0xFFFFFFFF);
					shaderFlags2 = static_cast<std::uint32_t>(sf >> 32);
				}

				RE::NiAlphaProperty * alphaProperty = netimmerse_cast<RE::NiAlphaProperty*>(geometry->spPropertyState.get());
				if (alphaProperty) {
					alphaFlags = alphaProperty->alphaFlags;
					alphaThreshold = alphaProperty->alphaThreshold;
				}

				vertCount = geometryData->vertices;
				triangleCount = geometryData->numTriangles;

				if (!geometryData->vertex || !geometryData->texture || !triangleCount) return nullptr;
				std::vector<CDXMeshIndex> indices;
				if (triShapeData) {
					if (!triShapeData->triList || triShapeData->triListLength != triangleCount * 3) return nullptr;
					indices.assign(triShapeData->triList, triShapeData->triList + triShapeData->triListLength);
				} else {
					if (!triStripsData->stripLists || !triStripsData->stripLengths) return nullptr;
					const std::span<const std::uint16_t> lengths(triStripsData->stripLengths, triStripsData->numStrips);
					std::size_t points = 0;
					for (auto length : lengths) points += length;
					if (!ExpandTriangleStrips(lengths, {triStripsData->stripLists, points}, triangleCount, indices)) return nullptr;
				}
				const auto indexCount = static_cast<std::uint32_t>(indices.size());

				RE::BSFaceGenBaseMorphExtraData * morphData = (RE::BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
				if (morphData) {
					nifMesh->m_morphable = true;
				}

				if (!nifMesh->InitializeBuffers(pDevice, vertCount, indexCount, [&](CDXMeshVert* pVertices, CDXMeshIndex* pIndices)
				{
					memcpy(pIndices, indices.data(), indices.size() * sizeof(CDXMeshIndex));

					for (std::uint32_t i = 0; i < vertCount; i++) {
						RE::NiPoint3 xformed = localTransform * geometryData->vertex[i];
						RE::NiPoint2 uv = geometryData->texture[i];
						pVertices[i].Position = *(DirectX::XMFLOAT3*)&xformed;
						DirectX::XMFLOAT3 vNormal(0, 0, 0);
						pVertices[i].Normal = vNormal;
						pVertices[i].Tex = *(DirectX::XMFLOAT2*)&uv;
						XMStoreFloat3(&pVertices[i].Color, COLOR_UNSELECTED);
						if (nifMesh->m_morphable && geometryData->normal)
							XMStoreFloat3(&pVertices[i].Normal, XMLoadFloat3((XMFLOAT3*)&geometryData->normal[i]));

					}
				})) return nullptr;
				// All sculpt consumers now share the normalized triangle list.
				// Build derived data only after the buffer transaction commits.
				nifMesh->BuildAdjacency();
				if (nifMesh->m_morphable) {
					nifMesh->BuildFacemap();
					if (!geometryData->normal) nifMesh->BuildNormals();
				}
				
				std::shared_ptr<CDXMaterial> material = std::make_shared<CDXMaterial>();
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

	if (!nifMesh->GetVertexBuffer().Get() || !nifMesh->GetIndexBuffer().Get()) return nullptr;
	if (!nifMesh->m_morphable)
		nifMesh->SetLocked(true);

	return candidate.release();
}

const char * CDXLegacyNifMesh::GetName() const
{
	return m_geometry ? m_geometry->name.c_str() : "";
}
