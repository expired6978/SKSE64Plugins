#pragma once

#include <vector>
#include <stdint.h>

const double EPSILON = 0.0001;

const float PI = 3.141592f;
const float DEG2RAD = PI / 180.0f;

namespace Morpher
{
struct Vector2 {
	float u;
	float v;

	Vector2() {
		u = v = 0.0f;
	}
	Vector2(float U, float V) {
		u = U;
		v = V;
	}

	bool operator == (const Vector2& other) {
		if (u == other.u && v == other.v)
			return true;
		return false;
	}
	bool operator != (const Vector2& other) {
		if (u != other.u && v != other.v)
			return true;
		return false;
	}

	Vector2& operator -= (const Vector2& other) {
		u -= other.u;
		v -= other.v;
		return (*this);
	}
	Vector2 operator - (const Vector2& other) const {
		Vector2 tmp = (*this);
		tmp -= other;
		return tmp;
	}

	Vector2& operator += (const Vector2& other) {
		u += other.u;
		v += other.v;
		return (*this);
	}
	Vector2 operator + (const Vector2& other) const {
		Vector2 tmp = (*this);
		tmp += other;
		return tmp;
	}

	Vector2& operator *= (float val) {
		u *= val;
		v *= val;
		return(*this);
	}
	Vector2 operator * (float val) const {
		Vector2 tmp = (*this);
		tmp *= val;
		return tmp;
	}

	Vector2& operator /= (float val) {
		u /= val;
		v /= val;
		return (*this);
	}
	Vector2 operator / (float val) const {
		Vector2 tmp = (*this);
		tmp /= val;
		return tmp;
	}
};

struct Vector3 {
	float x;
	float y;
	float z;

	Vector3() {
		x = y = z = 0.0f;
	}
	Vector3(float X, float Y, float Z) {
		x = X;
		y = Y;
		z = Z;
	}

	void Zero() {
		x = y = z = 0.0f;
	}

	bool IsZero(bool bUseEpsilon = false) {
		if (bUseEpsilon) {
			if (fabs(x) < EPSILON && fabs(y) < EPSILON && fabs(z) < EPSILON)
				return true;
		}
		else {
			if (x == 0.0f && y == 0.0f && z == 0.0f)
				return true;
		}

		return false;
	}

	void Normalize() {
		float d = sqrt(x*x + y*y + z*z);
		if (d == 0)
			d = 1.0f;

		x /= d;
		y /= d;
		z /= d;
	}

	uint32_t hash() {
		uint32_t *h = (uint32_t*)this;
		uint32_t f = (h[0] + h[1] * 11 - h[2] * 17) & 0x7fffffff;
		return (f >> 22) ^ (f >> 12) ^ (f);
	}

	bool operator == (const Vector3& other) {
		if (x == other.x && y == other.y && z == other.z)
			return true;
		return false;
	}
	bool operator != (const Vector3& other) {
		if (x != other.x && y != other.y && z != other.z)
			return true;
		return false;
	}

	Vector3& operator -= (const Vector3& other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return (*this);
	}
	Vector3 operator - (const Vector3& other) const {
		Vector3 tmp = (*this);
		tmp -= other;
		return tmp;
	}
	Vector3& operator += (const Vector3& other) {
		x += other.x;
		y += other.y;
		z += other.z;
		return (*this);
	}
	Vector3 operator + (const Vector3& other) const {
		Vector3 tmp = (*this);
		tmp += other;
		return tmp;
	}
	Vector3& operator *= (float val) {
		x *= val;
		y *= val;
		z *= val;
		return(*this);
	}
	Vector3 operator * (float val) const {
		Vector3 tmp = (*this);
		tmp *= val;
		return tmp;
	}
	Vector3& operator /= (float val) {
		x /= val;
		y /= val;
		z /= val;
		return (*this);
	}
	Vector3 operator / (float val) const {
		Vector3 tmp = (*this);
		tmp /= val;
		return tmp;
	}

	Vector3 cross(const Vector3& other) const {
		Vector3 tmp;
		tmp.x = y*other.z - z*other.y;
		tmp.y = z*other.x - x*other.z;
		tmp.z = x*other.y - y*other.x;
		return tmp;
	}

	float dot(const Vector3& other) const {
		return x*other.x + y*other.y + z*other.z;
	}

	float DistanceTo(const Vector3& target) {
		float dx = target.x - x;
		float dy = target.y - y;
		float dz = target.z - z;
		return (float)sqrt(dx*dx + dy*dy + dz*dz);
	}

	float DistanceSquaredTo(const Vector3& target) {
		float dx = target.x - x;
		float dy = target.y - y;
		float dz = target.z - z;
		return (float)dx*dx + dy*dy + dz*dz;
	}

	float angle(const Vector3& other) const {
		Vector3 A(x, y, z);
		Vector3 B(other.x, other.y, other.z);
		A.Normalize();
		B.Normalize();

		float dot = A.dot(B);
		if (dot > 1.0)
			return 0.0f;
		else if (dot < -1.0f)
			return PI;
		else if (dot == 0.0f)
			return PI / 2.0f;

		return acosf(dot);
	}

	void clampEpsilon() {
		if (fabs(x) < EPSILON)
			x = 0.0f;
		if (fabs(y) < EPSILON)
			y = 0.0f;
		if (fabs(z) < EPSILON)
			z = 0.0f;
	}
};

struct Triangle {
	uint16_t p1;
	uint16_t p2;
	uint16_t p3;

	Triangle() {
		p1 = p2 = p3 = 0.0f;
	}
	Triangle(uint16_t P1, uint16_t P2, uint16_t P3) {
		p1 = P1;
		p2 = P2;
		p3 = P3;
	}

	void set(uint16_t P1, uint16_t P2, uint16_t P3) {
		p1 = P1; p2 = P2; p3 = P3;
	}

	void trinormal(Vector3* vertref, Vector3* outNormal) {
		outNormal->x = (vertref[p2].y - vertref[p1].y) * (vertref[p3].z - vertref[p1].z) - (vertref[p2].z - vertref[p1].z) * (vertref[p3].y - vertref[p1].y);
		outNormal->y = (vertref[p2].z - vertref[p1].z) * (vertref[p3].x - vertref[p1].x) - (vertref[p2].x - vertref[p1].x) * (vertref[p3].z - vertref[p1].z);
		outNormal->z = (vertref[p2].x - vertref[p1].x) * (vertref[p3].y - vertref[p1].y) - (vertref[p2].y - vertref[p1].y) * (vertref[p3].x - vertref[p1].x);
	}

	void trinormal(const std::vector<Vector3>& vertref, Vector3* outNormal) {
		outNormal->x = (vertref[p2].y - vertref[p1].y) * (vertref[p3].z - vertref[p1].z) - (vertref[p2].z - vertref[p1].z) * (vertref[p3].y - vertref[p1].y);
		outNormal->y = (vertref[p2].z - vertref[p1].z) * (vertref[p3].x - vertref[p1].x) - (vertref[p2].x - vertref[p1].x) * (vertref[p3].z - vertref[p1].z);
		outNormal->z = (vertref[p2].x - vertref[p1].x) * (vertref[p3].y - vertref[p1].y) - (vertref[p2].y - vertref[p1].y) * (vertref[p3].x - vertref[p1].x);
	}

	void midpoint(Vector3* vertref, Vector3& outPoint) {
		outPoint = vertref[p1];
		outPoint += vertref[p2];
		outPoint += vertref[p3];
		outPoint /= 3;
	}

	float AxisMidPointY(Vector3* vertref) {
		return (vertref[p1].y + vertref[p2].y + vertref[p3].y) / 3.0f;
	}

	float AxisMidPointX(Vector3* vertref) {
		return (vertref[p1].x + vertref[p2].x + vertref[p3].x) / 3.0f;
	}

	float AxisMidPointZ(Vector3* vertref) {
		return (vertref[p1].z + vertref[p2].z + vertref[p3].z) / 3.0f;
	}

	bool operator < (const Triangle& other) const {
		int d = 0;
		if (d == 0) d = p1 - other.p1;
		if (d == 0) d = p2 - other.p2;
		if (d == 0) d = p3 - other.p3;
		return d < 0;
	}

	bool operator == (const Triangle& other) const {
		return (p1 == other.p1 && p2 == other.p2 && p3 == other.p3);
	}

	bool CompareIndices(const Triangle& other) const {
		return ((p1 == other.p1 || p1 == other.p2 || p1 == other.p3)
			&& (p2 == other.p1 || p2 == other.p2 || p2 == other.p3)
			&& (p3 == other.p1 || p3 == other.p2 || p3 == other.p3));
	}

	void rot() {
		if (p2 < p1 && p2 < p3) {
			set(p2, p3, p1);
		}
		else if (p3 < p1) {
			set(p3, p1, p2);
		}
	}
};

struct Face {
	uint8_t nPoints;
	uint16_t p1;
	uint16_t uv1;
	uint16_t p2;
	uint16_t uv2;
	uint16_t p3;
	uint16_t uv3;
	uint16_t p4;
	uint16_t uv4;

	Face(int npts = 0, int* points = nullptr, int* tc = nullptr) {
		nPoints = npts;
		if (npts < 3)
			return;

		p1 = points[0]; p2 = points[1];  p3 = points[2];
		uv1 = tc[0]; uv2 = tc[1]; uv3 = tc[2];
		if (npts == 4) {
			p4 = points[3];
			uv4 = tc[3];
		}
	}
};

struct Edge {
	uint16_t p1;
	uint16_t p2;

	Edge() {
		p1 = p2 = 0;
	}
	Edge(uint16_t P1, uint16_t P2) {
		p1 = P1; p2 = P2;
	}
};

};

namespace std {
	template<> struct hash < Morpher::Edge > {
		std::size_t operator() (const Morpher::Edge& t) const {
			return ((t.p2 << 16) | (t.p1 & 0xFFFF));
		}
	};

	template <> struct equal_to < Morpher::Edge > {
		bool operator() (const Morpher::Edge& t1, const Morpher::Edge& t2) const {
			return ((t1.p1 == t2.p1) && (t1.p2 == t2.p2));
		}
	};

	template <> struct hash < Morpher::Triangle > {
		std::size_t operator() (const Morpher::Triangle& t) const {
			char* d = (char*)&t;
			size_t len = sizeof(Morpher::Triangle);
			size_t hash, i;
			for (hash = i = 0; i < len; ++i) {
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

	template <> struct equal_to < Morpher::Triangle > {
		bool operator() (const Morpher::Triangle& t1, const Morpher::Triangle& t2) const {
			return ((t1.p1 == t2.p1) && (t1.p2 == t2.p2) && (t1.p3 == t2.p3));
		}
	};
}
