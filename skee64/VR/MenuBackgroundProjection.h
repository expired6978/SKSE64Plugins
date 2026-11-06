#pragma once
#include <array>
#include <cmath>

namespace SKEE::MenuAppearance
{
	struct SurfaceVertex { double x, y, z, u, v; };

	// Measure world length per unit texture U/V, not screen-space perspective.
	// Only rectangular, affine four-vertex surfaces are supported. Reject curved,
	// sheared or degenerate geometry rather than silently distorting the image.
	inline double SurfaceAspect(const std::array<SurfaceVertex, 4>& vertices)
	{
		double horizontal = 0, vertical = 0;
		std::array<double, 3> uAxis{}, vAxis{};
		int uEdges = 0, vEdges = 0;
		for (const auto& p : vertices)
			if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
				!std::isfinite(p.u) || !std::isfinite(p.v)) return 0;
		for (std::size_t i = 0; i < 4; ++i) for (std::size_t j = i + 1; j < 4; ++j) {
			const auto& a = vertices[i]; const auto& b = vertices[j];
			const auto du = b.u - a.u, dv = b.v - a.v;
			const std::array<double, 3> d{b.x - a.x, b.y - a.y, b.z - a.z};
			const auto length = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
			if (std::abs(dv) < 1e-6 && std::abs(du) > 1e-6) {
				const auto value = length / std::abs(du);
				if (uEdges && std::abs(value - horizontal) > horizontal * 1e-4) return 0;
				horizontal = value; ++uEdges;
				for (int k = 0; k < 3; ++k) {
					if (uEdges > 1 && std::abs(d[k]/du-uAxis[k]) > horizontal*1e-4) return 0;
					uAxis[k] = d[k]/du;
				}
			} else if (std::abs(du) < 1e-6 && std::abs(dv) > 1e-6) {
				const auto value = length / std::abs(dv);
				if (vEdges && std::abs(value - vertical) > vertical * 1e-4) return 0;
				vertical = value; ++vEdges;
				for (int k = 0; k < 3; ++k) {
					if (vEdges > 1 && std::abs(d[k]/dv-vAxis[k]) > vertical*1e-4) return 0;
					vAxis[k] = d[k]/dv;
				}
			}
		}
		if (uEdges != 2 || vEdges != 2 || horizontal <= 0 || vertical <= 0) return 0;
		const auto dot = uAxis[0]*vAxis[0] + uAxis[1]*vAxis[1] + uAxis[2]*vAxis[2];
		if (std::abs(dot) > horizontal*vertical*1e-4) return 0;
		return horizontal / vertical;
	}
}
