#include "Morpher.h"
#include "NifUtils.h"
#include <cmath>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "half.hpp"
#include <DirectXMath.h>
#include "RE/N/NiGeometry.h"
#include "RE/N/NiExtraData.h"
#include "RE/B/BSAtomic.h"
#include "RE/B/BSDynamicTriShape.h"
#include <cstdint>
#include <optional>

float round_v(float num)
{
	return (num > 0.0) ? floor(num + 0.5) : ceil(num - 0.5);
}

NormalApplicator::NormalApplicator(RE::NiPointer<RE::BSGeometry> _geometry, RE::NiPointer<RE::NiSkinPartition> _skinPartition) 
	: geometry(_geometry)
	, skinPartition(_skinPartition)
{
	
}

static std::uint8_t PackUnitFloat(float value)
{
	return static_cast<std::uint8_t>(round_v(((value + 1.0f) / 2.0f) * 255.0f));
}

void NormalApplicator::Apply()
{
	using RE::BSGraphics::Vertex;

	if (!geometry || !skinPartition || skinPartition->numPartitions == 0)
		return;

	auto& partition = skinPartition->partitions[0];
	if (!partition.buffData || !partition.buffData->rawVertexData)
		return;

	// The layout of the vertex block being rewritten. Any attribute can be missing,
	// so every one is located through its offset rather than assuming the order.
	auto vertexDesc = partition.vertexDesc;
	const std::uint32_t vertexSize = vertexDesc.GetSize();

	const bool hasVertices = vertexDesc.HasFlag(Vertex::VF_VERTEX);
	const bool hasUV = vertexDesc.HasFlag(Vertex::VF_UV);
	const bool hasNormals = vertexDesc.HasFlag(Vertex::VF_NORMAL);
	const bool hasTangents = hasNormals && vertexDesc.HasFlag(Vertex::VF_TANGENT); // Tangents are only stored next to normals

	if (!hasNormals || vertexSize == 0)
		return;

	RE::BSDynamicTriShape* dynamicTriShape = geometry->AsDynamicTriShape();
	RE::BSTriShape* triShape = geometry->AsTriShape();

	// A BSDynamicTriShape keeps its positions (and bitangent X) in dynamicData, one float4 per vertex
	DirectX::XMFLOAT4* dynamicVertices = nullptr;
	std::uint32_t numVertices = skinPartition->vertexCount;
	if (triShape && triShape->vertexCount && (numVertices == 0 || triShape->vertexCount < numVertices))
		numVertices = triShape->vertexCount;

	if (dynamicTriShape) {
		auto& dynamicData = dynamicTriShape->GetDynamicTrishapeRuntimeData();
		dynamicVertices = static_cast<DirectX::XMFLOAT4*>(dynamicData.dynamicData);
		if (dynamicVertices && dynamicData.dataSize)
			numVertices = (std::min)(numVertices, dynamicData.dataSize / static_cast<std::uint32_t>(sizeof(DirectX::XMFLOAT4)));
	}

	// Without positions there is nothing to calculate normals from
	if (!dynamicVertices && !hasVertices)
		return;

	if (numVertices == 0)
		return;

	std::vector<std::uint16_t> indices;
	for (std::uint32_t p = 0; p < skinPartition->numPartitions; ++p)
	{
		auto& part = skinPartition->partitions[p];
		if (!part.triList)
			continue;

		for (std::uint32_t t = 0; t < part.triangles; ++t)
		{
			const std::uint16_t* tri = &part.triList[t * 3];
			if (tri[0] < numVertices && tri[1] < numVertices && tri[2] < numVertices)
				indices.insert(indices.end(), tri, tri + 3);
		}
	}

	if (indices.empty())
		return;

	std::optional<RE::BSSpinLockGuard> locker;
	if (dynamicTriShape)
		locker.emplace(dynamicTriShape->GetDynamicTrishapeRuntimeData().lock);

	RE::NiIntegersExtraData* extraData = static_cast<RE::NiIntegersExtraData*>(geometry->GetExtraData("LOCKEDNORM"));
	if (extraData)
	{
		for (std::uint32_t i = 0; i < extraData->size; ++i)
		{
			lockedVertices.insert(static_cast<std::uint16_t>(extraData->value[i]));
		}
	}

	const std::uint32_t positionOffset = vertexDesc.GetAttributeOffset(Vertex::VA_POSITION);
	const std::uint32_t uvOffset = vertexDesc.GetAttributeOffset(Vertex::VA_TEXCOORD0);
	const std::uint32_t normalOffset = vertexDesc.GetAttributeOffset(Vertex::VA_NORMAL);
	const std::uint32_t tangentOffset = vertexDesc.GetAttributeOffset(Vertex::VA_BINORMAL);

	std::uint8_t* vertexBlock = partition.buffData->rawVertexData;

	// Pull the base data from the vertex block
	rawVertices.resize(numVertices);
	rawNormals.resize(numVertices);
	if (hasUV)
		rawUV.resize(numVertices);
	if (hasTangents) {
		rawTangents.resize(numVertices);
		rawBitangents.resize(numVertices);
	}

	for (std::uint32_t i = 0; i < numVertices; i++)
	{
		const std::uint8_t* vBegin = &vertexBlock[i * vertexSize];

		if (dynamicVertices)
		{
			rawVertices[i].x = dynamicVertices[i].x;
			rawVertices[i].y = dynamicVertices[i].y;
			rawVertices[i].z = dynamicVertices[i].z;
		}
		else
		{
			const float* position = reinterpret_cast<const float*>(vBegin + positionOffset);
			rawVertices[i].x = position[0];
			rawVertices[i].y = position[1];
			rawVertices[i].z = position[2];
		}

		if (hasUV)
		{
			const half_float::half* uv = reinterpret_cast<const half_float::half*>(vBegin + uvOffset);
			rawUV[i].u = uv[0];
			rawUV[i].v = uv[1];
		}
	}

	const std::uint32_t numTriangles = static_cast<std::uint32_t>(indices.size() / 3);
	Morpher::Triangle* triangles = reinterpret_cast<Morpher::Triangle*>(indices.data());

	RecalcNormals(numTriangles, triangles);
	if (hasTangents)
		CalcTangentSpace(numTriangles, triangles);

	for (std::uint32_t i = 0; i < numVertices; i++)
	{
		if (lockedVertices.count(static_cast<std::uint16_t>(i)))
			continue;

		std::uint8_t* vBegin = &vertexBlock[i * vertexSize];

		// Normal XYZ, bitangent Y
		std::uint8_t* normal = vBegin + normalOffset;
		normal[0] = PackUnitFloat(rawNormals[i].x);
		normal[1] = PackUnitFloat(rawNormals[i].y);
		normal[2] = PackUnitFloat(rawNormals[i].z);

		// The bitangent is split over the position, normal and tangent; leave it alone without tangents
		if (!hasTangents)
			continue;

		normal[3] = PackUnitFloat(rawBitangents[i].y);

		// Tangent XYZ, bitangent Z
		std::uint8_t* tangent = vBegin + tangentOffset;
		tangent[0] = PackUnitFloat(rawTangents[i].x);
		tangent[1] = PackUnitFloat(rawTangents[i].y);
		tangent[2] = PackUnitFloat(rawTangents[i].z);
		tangent[3] = PackUnitFloat(rawBitangents[i].z);

		// Bitangent X
		if (dynamicVertices)
			dynamicVertices[i].w = rawBitangents[i].x;
		else
			*reinterpret_cast<float*>(vBegin + positionOffset + 12) = rawBitangents[i].x;
	}
}

void NormalApplicator::RecalcNormals(std::uint32_t numTriangles, Morpher::Triangle* triangles, const bool smooth, const float smoothThresh)
{
	std::uint32_t numVertices = rawVertices.size();

	std::vector<Morpher::Vector3> verts(numVertices);
	std::vector<Morpher::Vector3> norms(numVertices);

	for (std::uint32_t i = 0; i < numVertices; i++)
	{
		verts[i].x = rawVertices[i].x * -0.1f;
		verts[i].z = rawVertices[i].y * 0.1f;
		verts[i].y = rawVertices[i].z * 0.1f;
	}

	// Face normals
	Morpher::Vector3 tn;
	for (std::uint32_t t = 0; t < numTriangles; t++)
	{
		triangles[t].trinormal(verts, &tn);
		norms[triangles[t].p1] += tn;
		norms[triangles[t].p2] += tn;
		norms[triangles[t].p3] += tn;
	}

	for (auto &n : norms)
		n.Normalize();

	// Smooth normals
	if (smooth) {
		kd_matcher matcher(verts.data(), numVertices);
		for (int i = 0; i < matcher.matches.size(); i++)
		{
			std::pair<Morpher::Vector3*, int>& a = matcher.matches[i].first;
			std::pair<Morpher::Vector3*, int>& b = matcher.matches[i].second;

			Morpher::Vector3& an = norms[a.second];
			Morpher::Vector3& bn = norms[b.second];
			if (an.angle(bn) < smoothThresh * DEG2RAD) {
				Morpher::Vector3 anT = an;
				an += bn;
				bn += anT;
			}
		}

		for (auto &n : norms)
			n.Normalize();
	}

	for (std::uint32_t i = 0; i < numVertices; i++)
	{
		rawNormals[i].x = -norms[i].x;
		rawNormals[i].y = norms[i].z;
		rawNormals[i].z = norms[i].y;
	}
}

void NormalApplicator::CalcTangentSpace(std::uint32_t numTriangles, Morpher::Triangle * triangles)
{
	std::uint32_t numVertices = rawVertices.size();

	std::vector<Morpher::Vector3> tan1;
	std::vector<Morpher::Vector3> tan2;
	tan1.resize(numVertices);
	tan2.resize(numVertices);

	rawTangents.resize(numVertices);
	rawBitangents.resize(numVertices);

	// Without UVs there is no texture direction, every vertex gets a tangent frame around its normal
	const bool hasUV = rawUV.size() == numVertices;

	for (std::uint32_t i = 0; hasUV && i < numTriangles; i++)
	{
		int i1 = triangles[i].p1;
		int i2 = triangles[i].p2;
		int i3 = triangles[i].p3;

		Morpher::Vector3 v1 = rawVertices[i1];
		Morpher::Vector3 v2 = rawVertices[i2];
		Morpher::Vector3 v3 = rawVertices[i3];

		Morpher::Vector2 w1 = rawUV[i1];
		Morpher::Vector2 w2 = rawUV[i2];
		Morpher::Vector2 w3 = rawUV[i3];

		float x1 = v2.x - v1.x;
		float x2 = v3.x - v1.x;
		float y1 = v2.y - v1.y;
		float y2 = v3.y - v1.y;
		float z1 = v2.z - v1.z;
		float z2 = v3.z - v1.z;

		float s1 = w2.u - w1.u;
		float s2 = w3.u - w1.u;
		float t1 = w2.v - w1.v;
		float t2 = w3.v - w1.v;

		float r = (s1 * t2 - s2 * t1);
		r = (r >= 0.0f ? +1.0f : -1.0f);

		Morpher::Vector3 sdir = Morpher::Vector3((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
		Morpher::Vector3 tdir = Morpher::Vector3((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

		sdir.Normalize();
		tdir.Normalize();

		tan1[i1] += tdir;
		tan1[i2] += tdir;
		tan1[i3] += tdir;

		tan2[i1] += sdir;
		tan2[i2] += sdir;
		tan2[i3] += sdir;
	}

	for (std::uint32_t i = 0; i < numVertices; i++)
	{
		rawTangents[i] = tan1[i];
		rawBitangents[i] = tan2[i];

		if (rawTangents[i].IsZero() || rawBitangents[i].IsZero())
		{
			rawTangents[i].x = rawNormals[i].y;
			rawTangents[i].y = rawNormals[i].z;
			rawTangents[i].z = rawNormals[i].x;
			rawBitangents[i] = rawNormals[i].cross(rawTangents[i]);
		}
		else
		{
			rawTangents[i].Normalize();
			rawTangents[i] = (rawTangents[i] - rawNormals[i] * rawNormals[i].dot(rawTangents[i]));
			rawTangents[i].Normalize();

			rawBitangents[i].Normalize();

			rawBitangents[i] = (rawBitangents[i] - rawNormals[i] * rawNormals[i].dot(rawBitangents[i]));
			rawBitangents[i] = (rawBitangents[i] - rawTangents[i] * rawTangents[i].dot(rawBitangents[i]));

			rawBitangents[i].Normalize();
		}
	}
}