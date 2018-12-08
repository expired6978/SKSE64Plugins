#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <set>

typedef DirectX::XMMATRIX		CDXMatrix;
typedef unsigned short	CDXMeshIndex;
typedef DirectX::XMVECTOR		CDXVec;
typedef DirectX::XMFLOAT4		CDXVec4;
typedef DirectX::XMFLOAT3		CDXVec3;
typedef DirectX::XMFLOAT2		CDXVec2;
typedef DirectX::XMFLOAT3		CDXColor;

typedef std::set<CDXMeshIndex> CDXMeshIndexSet;
typedef std::map<CDXMeshIndex, float> CDXHitIndexMap;

struct CDXMeshEdge
{
	CDXMeshIndex	p1;
	CDXMeshIndex	p2;

	CDXMeshEdge(CDXMeshIndex _p1, CDXMeshIndex _p2)
	{
		p1 = _p1;
		p2 = _p2;
	}
};

struct CDXMeshFace
{
	CDXMeshIndex	v1;
	CDXMeshIndex	v2;
	CDXMeshIndex	v3;

	CDXMeshFace(CDXMeshIndex _v1, CDXMeshIndex _v2, CDXMeshIndex _v3)
	{
		v1 = _v1;
		v2 = _v2;
		v3 = _v3;
	}
};

namespace std {
	template<> struct hash < CDXMeshEdge >
	{
		std::size_t operator() (const CDXMeshEdge & t) const
		{
			return ((t.p2 << 16) | (t.p1 & 0xFFFF));
		}

	};

	template <> struct equal_to < CDXMeshEdge >
	{
		bool operator() (const CDXMeshEdge& t1, const CDXMeshEdge& t2) const
		{
			return ((t1.p1 == t2.p1) && (t1.p2 == t2.p2));
		}
	};

	template <> struct hash < CDXMeshFace > {
		std::size_t operator() (const CDXMeshFace& t) const {
			char* d = (char*)&t;
			size_t len = sizeof(CDXMeshFace);
			size_t hash, i;
			for (hash = i = 0; i < len; ++i)
			{
				hash += d[i];
				hash += (hash << 10);
				hash ^= (hash >> 6);
			}
			hash += (hash << 3);
			hash ^= (hash >> 11);
			hash += (hash << 15);
			return hash;
		}
	};
	template <> struct equal_to < CDXMeshFace >
	{
		bool operator() (const CDXMeshFace& t1, const CDXMeshFace& t2) const
		{
			return ((t1.v1 == t2.v1) && (t1.v2 == t2.v2) && (t1.v3 == t2.v3));
		}
	};
}

struct CDXMeshVert
{
	CDXVec3		Position;
	CDXVec3		Normal;
	CDXVec2		Tex;
	CDXColor	Color;
};