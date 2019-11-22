#pragma once

#include "shape.hpp"
#include "kd_matcher.hpp"

#include <vector>
#include <functional>

#include "skse64/NiTypes.h"

class BSGeometry;
class NiSkinPartition;

class NormalApplicator
{
public:
	NormalApplicator(NiPointer<BSGeometry> _geometry, NiPointer<NiSkinPartition> _skinPartition);

	void RecalcNormals(UInt32 numTriangles, Morpher::Triangle* triangles, const bool smooth = true, const float smoothThres = 60.0f);
	void CalcTangentSpace(UInt32 numTriangles, Morpher::Triangle * triangles);

protected:
	NiPointer<BSGeometry> geometry;
	NiPointer<NiSkinPartition> skinPartition;
	std::vector<Morpher::Vector3> rawVertices;
	std::vector<Morpher::Vector3> rawNormals;
	std::vector<Morpher::Vector2> rawUV;
	std::vector<Morpher::Vector3> rawTangents;
	std::vector<Morpher::Vector3> rawBitangents;
};
