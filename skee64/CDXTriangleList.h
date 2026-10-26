#pragma once

#include "CDXTypes.h"
#include <span>
#include <vector>

// Sculpt picking, adjacency and normals all consume triangle lists. Expand
// separate legacy strips without connecting their ends or losing winding.
inline bool ExpandTriangleStrips(std::span<const std::uint16_t> lengths,
	std::span<const CDXMeshIndex> strips, std::uint32_t triangleCount,
	std::vector<CDXMeshIndex>& output)
{
	std::size_t points = 0, triangles = 0;
	for (auto length : lengths) {
		if (length < 3 || points > strips.size() || length > strips.size() - points) return false;
		points += length;
		triangles += length - 2;
	}
	if (points != strips.size() || triangles != triangleCount) return false;
	std::vector<CDXMeshIndex> result;
	result.reserve(triangles * 3);
	std::size_t offset = 0;
	for (auto length : lengths) {
		for (std::uint32_t i = 0; i + 2 < length; ++i) {
			result.push_back(strips[offset + i + (i % 2)]);
			result.push_back(strips[offset + i + 1 - (i % 2)]);
			result.push_back(strips[offset + i + 2]);
		}
		offset += length;
	}
	output = std::move(result);
	return true;
}
