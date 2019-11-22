#include "Morpher.h"
#include <cmath>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "half.hpp"
#include "skse64/NiGeometry.h"

float round_v(float num)
{
	return (num > 0.0) ? floor(num + 0.5) : ceil(num - 0.5);
}

NormalApplicator::NormalApplicator(NiPointer<BSGeometry> _geometry, NiPointer<NiSkinPartition> _skinPartition) : geometry(_geometry)
{
	BSDynamicTriShape * dynamicTriShape = ni_cast(geometry, BSDynamicTriShape);
	BSTriShape * triShape = ni_cast(geometry, BSTriShape);

	if (dynamicTriShape) dynamicTriShape->lock.Lock();

	UInt64 vertexDesc = geometry->vertexDesc;
	UInt32 numVertices = triShape ? triShape->numVertices : 0;
	UInt8* vertexBlock = nullptr;

	bool hasNormals = (NiSkinPartition::GetVertexFlags(vertexDesc) & VertexFlags::VF_NORMAL) == VertexFlags::VF_NORMAL;
	bool hasTangents = (NiSkinPartition::GetVertexFlags(vertexDesc) & VertexFlags::VF_TANGENT) == VertexFlags::VF_TANGENT;
	bool hasUV = (NiSkinPartition::GetVertexFlags(vertexDesc) & VertexFlags::VF_UV) == VertexFlags::VF_UV;

	const NiSkinInstance * skinInstance = geometry->m_spSkinInstance.m_pObject;
	if (skinInstance && (hasNormals || hasTangents)) {
		const NiSkinPartition * skinPartition = skinInstance->m_spSkinPartition.m_pObject;
		if (skinPartition) {
			numVertices = numVertices ? numVertices : skinPartition->vertexCount;

			// Pull the base data from the vertex block
			rawVertices.resize(numVertices);
			if (hasUV)
				rawUV.resize(numVertices);


			if (hasNormals) {
				rawNormals.resize(numVertices);
				if (hasTangents) {
					rawTangents.resize(numVertices);
					rawBitangents.resize(numVertices);
				}
			}

			UInt32 vertexSize = NiSkinPartition::GetVertexSize(vertexDesc);

			vertexBlock = dynamicTriShape ? reinterpret_cast<UInt8*>(dynamicTriShape->pDynamicData) : reinterpret_cast<UInt8*>(skinPartition->m_pkPartitions[0].shapeData->m_RawVertexData);

			for (UInt32 i = 0; i < numVertices; i++)
			{
				UInt8 * vBegin = &vertexBlock[i * vertexSize];

				rawVertices[i].x = (*(float *)vBegin); vBegin += 4;
				rawVertices[i].y = (*(float *)vBegin); vBegin += 4;
				rawVertices[i].z = (*(float *)vBegin); vBegin += 4;

				vBegin += 4; // Skip BitangetX

				if (NiSkinPartition::GetVertexFlags(vertexDesc) & VertexFlags::VF_UV)
				{
					rawUV[i].u = (*(half_float::half *)vBegin); vBegin += 2;
					rawUV[i].v = (*(half_float::half *)vBegin); vBegin += 2;
				}
			}

			std::vector<uint16_t> indices;
			for (UInt32 p = 0; p < skinPartition->m_uiPartitions; ++p)
			{
				for (UInt32 t = 0; t < skinPartition->m_pkPartitions[p].m_usTriangles * 3; ++t)
				{
					indices.push_back(skinPartition->m_pkPartitions[p].m_pusTriList[t]);
				}
			}

			RecalcNormals(indices.size() / 3, reinterpret_cast<Morpher::Triangle*>(&indices.at(0)));
			CalcTangentSpace(indices.size() / 3, reinterpret_cast<Morpher::Triangle*>(&indices.at(0)));

			for (UInt32 i = 0; i < numVertices; i++)
			{
				UInt8 * vBegin = &vertexBlock[i * vertexSize];

				// X,Y,Z,BX
				vBegin += 4;
				vBegin += 4;
				vBegin += 4;

				// No need to write bitangentX
				*(float *)vBegin = rawBitangents[i].x; vBegin += 4;

				// Skip UV write
				if (hasUV)
				{
					vBegin += 4;
				}

				if (hasNormals)
				{
					*(SInt8*)vBegin = (UInt8)round_v((((rawNormals[i].x + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;
					*(SInt8*)vBegin = (UInt8)round_v((((rawNormals[i].y + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;
					*(SInt8*)vBegin = (UInt8)round_v((((rawNormals[i].z + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;

					*(SInt8*)vBegin = (UInt8)round_v((((rawBitangents[i].y + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;

					if (hasTangents)
					{
						*(SInt8*)vBegin = (UInt8)round_v((((rawTangents[i].x + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;
						*(SInt8*)vBegin = (UInt8)round_v((((rawTangents[i].y + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;
						*(SInt8*)vBegin = (UInt8)round_v((((rawTangents[i].z + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;

						*(SInt8*)vBegin = (UInt8)round_v((((rawBitangents[i].z + 1.0f) / 2.0f) * 255.0f)); vBegin += 1;
					}
				}
			}
		}
	}

	if (dynamicTriShape) dynamicTriShape->lock.Release();
}

void NormalApplicator::RecalcNormals(UInt32 numTriangles, Morpher::Triangle* triangles, const bool smooth, const float smoothThresh)
{
	UInt32 numVertices = rawVertices.size();

	std::vector<Morpher::Vector3> verts(numVertices);
	std::vector<Morpher::Vector3> norms(numVertices);

	for (UInt32 i = 0; i < numVertices; i++)
	{
		verts[i].x = rawVertices[i].x * -0.1f;
		verts[i].z = rawVertices[i].y * 0.1f;
		verts[i].y = rawVertices[i].z * 0.1f;
	}

	// Face normals
	Morpher::Vector3 tn;
	for (UInt32 t = 0; t < numTriangles; t++)
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

	for (UInt32 i = 0; i < numVertices; i++)
	{
		rawNormals[i].x = -norms[i].x;
		rawNormals[i].y = norms[i].z;
		rawNormals[i].z = norms[i].y;
	}
}

void NormalApplicator::CalcTangentSpace(UInt32 numTriangles, Morpher::Triangle * triangles)
{
	UInt32 numVertices = rawVertices.size();

	std::vector<Morpher::Vector3> tan1;
	std::vector<Morpher::Vector3> tan2;
	tan1.resize(numVertices);
	tan2.resize(numVertices);

	for (UInt32 i = 0; i < numTriangles; i++)
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

	for (UInt32 i = 0; i < numVertices; i++)
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