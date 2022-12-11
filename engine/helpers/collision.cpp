#include "collision.h"

namespace helpers
{
	bool CollisionFrustumVsAABB(const Frustum& frustum, const AABB& bounding_box)
	{
		const glm::vec4* planes = frustum.planes;
		const glm::vec3* points = frustum.points;
		// check box outside/inside of frustum
		for (int i = 0; i < Frustum::Count; i++)
		{
			if ((glm::dot(planes[i], glm::vec4(bounding_box.min.x, bounding_box.min.y, bounding_box.min.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.max.x, bounding_box.min.y, bounding_box.min.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.min.x, bounding_box.max.y, bounding_box.min.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.max.x, bounding_box.max.y, bounding_box.min.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.min.x, bounding_box.min.y, bounding_box.max.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.max.x, bounding_box.min.y, bounding_box.max.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.min.x, bounding_box.max.y, bounding_box.max.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(bounding_box.max.x, bounding_box.max.y, bounding_box.max.z, 1.0f)) < 0.0))
			{
				return false;
			}
		}

		// check frustum outside/inside box
		int out;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x > bounding_box.max.x) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x < bounding_box.min.x) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y > bounding_box.max.y) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y < bounding_box.min.y) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z > bounding_box.max.z) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z < bounding_box.min.z) ? 1 : 0); if (out == 8) return false;

		return true;
	}
	bool CollisionOBBVsOBB(const OBB& a, const OBB& b)
	{
		float ra, rb;
		glm::mat3x3 R, AbsR;
		// Compute rotation matrix expressing b in a’s coordinate frame
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				R[i][j] = glm::dot(glm::row(a.rotation, i), glm::row(b.rotation, j));

		// Compute translation vector t
		glm::vec3 t = b.position - a.position;
		// Bring translation into a’s coordinate frame
		t = glm::vec3(glm::dot(t, glm::row(a.rotation, 0)), glm::dot(t, glm::row(a.rotation, 1)), glm::dot(t, glm::row(a.rotation, 2)));
		// Compute common subexpressions. Add in an epsilon term to
		// counteract arithmetic errors when two edges are parallel and
		// their cross product is (near) null (see text for details)
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				AbsR[i][j] = fabsf(R[i][j]) + 0.00001f;

		// Test axes L = A0, L = A1, L = A2
		for (int i = 0; i < 3; i++) {
			ra = a.extents[i];
			rb = b.extents[0] * AbsR[i][0] + b.extents[1] * AbsR[i][1] + b.extents[2] * AbsR[i][2];
			if (fabsf(t[i]) > ra + rb) return false;
		}
		// Test axes L = B0, L = B1, L = B2
		for (int i = 0; i < 3; i++) {
			ra = a.extents[0] * AbsR[0][i] + a.extents[1] * AbsR[1][i] + a.extents[2] * AbsR[2][i];
			rb = b.extents[i];
			if (fabsf(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]) > ra + rb) return false;
		}
		// Test axis L = A0 x B0
		ra = a.extents[1] * AbsR[2][0] + a.extents[2] * AbsR[1][0];
		rb = b.extents[1] * AbsR[0][2] + b.extents[2] * AbsR[0][1];
		if (fabsf(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;
		// Test axis L = A0 x B1
		ra = a.extents[1] * AbsR[2][1] + a.extents[2] * AbsR[1][1];
		rb = b.extents[0] * AbsR[0][2] + b.extents[2] * AbsR[0][0];
		if (fabsf(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;
		// Test axis L = A0 x B2
		ra = a.extents[1] * AbsR[2][2] + a.extents[2] * AbsR[1][2];
		rb = b.extents[0] * AbsR[0][1] + b.extents[1] * AbsR[0][0];
		if (fabsf(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;
		// Test axis L = A1 x B0
		ra = a.extents[0] * AbsR[2][0] + a.extents[2] * AbsR[0][0];
		rb = b.extents[1] * AbsR[1][2] + b.extents[2] * AbsR[1][1];
		if (fabsf(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;

		// Test axis L = A1 x B1
		ra = a.extents[0] * AbsR[2][1] + a.extents[2] * AbsR[0][1];
		rb = b.extents[0] * AbsR[1][2] + b.extents[2] * AbsR[1][0];
		if (fabsf(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;
		// Test axis L = A1 x B2
		ra = a.extents[0] * AbsR[2][2] + a.extents[2] * AbsR[0][2];
		rb = b.extents[0] * AbsR[1][1] + b.extents[1] * AbsR[1][0];
		if (fabsf(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;
		// Test axis L = A2 x B0
		ra = a.extents[0] * AbsR[1][0] + a.extents[1] * AbsR[0][0];
		rb = b.extents[1] * AbsR[2][2] + b.extents[2] * AbsR[2][1];
		if (fabsf(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;
		// Test axis L = A2 x B1
		ra = a.extents[0] * AbsR[1][1] + a.extents[1] * AbsR[0][1];
		rb = b.extents[0] * AbsR[2][2] + b.extents[2] * AbsR[2][0];
		if (fabsf(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;
		// Test axis L = A2 x B2
		ra = a.extents[0] * AbsR[1][2] + a.extents[1] * AbsR[0][2];
		rb = b.extents[0] * AbsR[2][1] + b.extents[1] * AbsR[2][0];
		if (fabsf(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;
		// Since no separating axis is found, the OBBs must be intersecting
		return true;
	}

	//https://github.com/gszauer/GamePhysicsCookbook/blob/master/Code/Geometry3D.cpp
	struct Interval
	{
		float min;
		float max;
	};
	struct Line
	{
		glm::vec3 start;
		glm::vec3 end;

		inline Line() {}
		inline Line(const glm::vec3& s, const glm::vec3& e) :
			start(s), end(e) { }
	};
	struct Plane
	{
		glm::vec3 normal;
		float distance;

		inline Plane() : normal(1, 0, 0) { }
		inline Plane(const glm::vec3& n, float d) :
			normal(n), distance(d) { }
	};

	inline std::array<float, 9> GetRotationArray(const OBB& obb)
	{
		std::array<float, 9> ret;
		ret[0] = obb.rotation[0][0];
		ret[1] = obb.rotation[1][0];
		ret[2] = obb.rotation[2][0];
		ret[3] = obb.rotation[0][1];
		ret[4] = obb.rotation[1][1];
		ret[5] = obb.rotation[2][1];
		ret[6] = obb.rotation[0][2];
		ret[7] = obb.rotation[1][2];
		ret[8] = obb.rotation[2][2];
		
		return ret;
	}

	inline Interval GetInterval(const OBB& obb, const glm::vec3& axis) {
		glm::vec3 vertex[8];

		glm::vec3 C = obb.position;	// OBB Center
		glm::vec3 E = obb.extents;		// OBB Extents
		std::array<float, 9> o = GetRotationArray(obb);
		glm::vec3 A[] = {			// OBB Axis
			glm::vec3(o[0], o[1], o[2]),
			glm::vec3(o[3], o[4], o[5]),
			glm::vec3(o[6], o[7], o[8]),
		};

		vertex[0] = C + A[0] * E[0] + A[1] * E[1] + A[2] * E[2];
		vertex[1] = C - A[0] * E[0] + A[1] * E[1] + A[2] * E[2];
		vertex[2] = C + A[0] * E[0] - A[1] * E[1] + A[2] * E[2];
		vertex[3] = C + A[0] * E[0] + A[1] * E[1] - A[2] * E[2];
		vertex[4] = C - A[0] * E[0] - A[1] * E[1] - A[2] * E[2];
		vertex[5] = C + A[0] * E[0] - A[1] * E[1] - A[2] * E[2];
		vertex[6] = C - A[0] * E[0] + A[1] * E[1] - A[2] * E[2];
		vertex[7] = C - A[0] * E[0] - A[1] * E[1] + A[2] * E[2];

		Interval result;
		result.min = result.max = glm::dot(axis, vertex[0]);

		for (int i = 1; i < 8; ++i) {
			float projection = glm::dot(axis, vertex[i]);
			result.min = (projection < result.min) ? projection : result.min;
			result.max = (projection > result.max) ? projection : result.max;
		}

		return result;
	}

	inline float PenetrationDepth(const OBB& o1, const OBB& o2, const glm::vec3& axis, bool& outShouldFlip)
	{
		Interval i1 = GetInterval(o1, glm::normalize(axis));
		Interval i2 = GetInterval(o2, glm::normalize(axis));

		if (!((i2.min <= i1.max) && (i1.min <= i2.max))) {
			return 0.0f; // No penerattion
		}

		float len1 = i1.max - i1.min;
		float len2 = i2.max - i2.min;
		float min = glm::min(i1.min, i2.min);
		float max = glm::max(i1.max, i2.max);
		float length = max - min;

		outShouldFlip = (i2.min < i1.min);

		return (len1 + len2) - length;
	}

	inline std::array<Plane, 6> GetPlanes(const OBB& obb) {
		glm::vec3 c = obb.position;	// OBB Center
		glm::vec3 e = obb.extents;		// OBB Extents
		std::array<float, 9> o = GetRotationArray(obb);
		glm::vec3 a[] = {			// OBB Axis
			glm::vec3(o[0], o[1], o[2]),
			glm::vec3(o[3], o[4], o[5]),
			glm::vec3(o[6], o[7], o[8]),
		};

		std::array<Plane, 6> result;

		result[0] = Plane(a[0], glm::dot(a[0], (c + a[0] * e.x)));
		result[1] = Plane(a[0] * -1.0f, -glm::dot(a[0], (c - a[0] * e.x)));
		result[2] = Plane(a[1], glm::dot(a[1], (c + a[1] * e.y)));
		result[3] = Plane(a[1] * -1.0f, -glm::dot(a[1], (c - a[1] * e.y)));
		result[4] = Plane(a[2], glm::dot(a[2], (c + a[2] * e.z)));
		result[5] = Plane(a[2] * -1.0f, -glm::dot(a[2], (c - a[2] * e.z)));

		return result;
	}

	inline bool ClipToPlane(const Plane& plane, const Line& line, glm::vec3* outPoint) {
		glm::vec3 ab = line.end - line.start;

		float nA = glm::dot(plane.normal, line.start);
		float nAB = glm::dot(plane.normal, ab);

		auto CMP = [](float x, float y) -> bool {return (glm::abs(x - y) <= FLT_EPSILON * glm::max(1.0f, glm::max(glm::abs(x), glm::abs(y)))); };

		if (CMP(nAB, 0.f)) {
			return false;
		}

		float t = (plane.distance - nA) / nAB;
		if (t >= 0.0f && t <= 1.0f) {
			if (outPoint != 0) {
				*outPoint = line.start + ab * t;
			}
			return true;
		}

		return false;
	}
	inline bool PointInOBB(const glm::vec3& point, const OBB& obb) {
		glm::vec3 dir = point - obb.position;

		for (int i = 0; i < 3; ++i) {
			std::array<float, 9> o = GetRotationArray(obb);
			const float* orientation = &o[i * 3];
			glm::vec3 axis(orientation[0], orientation[1], orientation[2]);

			float distance = glm::dot(dir, axis);

			if (distance > obb.extents[i]) {
				return false;
			}
			if (distance < -obb.extents[i]) {
				return false;
			}
		}

		return true;
	}
	inline std::vector<glm::vec3> ClipEdgesToOBB(const std::array<Line, 12>& edges, const OBB& obb) {
		std::vector<glm::vec3> result;
		result.reserve(edges.size() * 3);
		glm::vec3 intersection;

		std::array<Plane, 6> planes = GetPlanes(obb);

		for (int i = 0; i < planes.size(); ++i) {
			for (int j = 0; j < edges.size(); ++j) {
				if (ClipToPlane(planes[i], edges[j], &intersection)) {
					if (PointInOBB(intersection, obb)) {
						result.push_back(intersection);
					}
				}
			}
		}

		return result;
	}

	inline std::array<glm::vec3, 8> GetVertices(const OBB& obb) {
		std::array<glm::vec3, 8> v;

		glm::vec3 C = obb.position;	// OBB Center
		glm::vec3 E = obb.extents;		// OBB Extents
		std::array<float, 9> o = GetRotationArray(obb);
		glm::vec3 A[] = {			// OBB Axis
			glm::vec3(o[0], o[1], o[2]),
			glm::vec3(o[3], o[4], o[5]),
			glm::vec3(o[6], o[7], o[8]),
		};

		v[0] = C + A[0] * E[0] + A[1] * E[1] + A[2] * E[2];
		v[1] = C - A[0] * E[0] + A[1] * E[1] + A[2] * E[2];
		v[2] = C + A[0] * E[0] - A[1] * E[1] + A[2] * E[2];
		v[3] = C + A[0] * E[0] + A[1] * E[1] - A[2] * E[2];
		v[4] = C - A[0] * E[0] - A[1] * E[1] - A[2] * E[2];
		v[5] = C + A[0] * E[0] - A[1] * E[1] - A[2] * E[2];
		v[6] = C - A[0] * E[0] + A[1] * E[1] - A[2] * E[2];
		v[7] = C - A[0] * E[0] - A[1] * E[1] + A[2] * E[2];

		return v;
	}

	inline std::array<Line, 12> GetEdges(const OBB& obb) {
		std::array<Line, 12>  result;
		std::array<glm::vec3, 8> v = GetVertices(obb);

		int index[][2] = { // Indices of edges
			{ 6, 1 },{ 6, 3 },{ 6, 4 },{ 2, 7 },{ 2, 5 },{ 2, 0 },
			{ 0, 1 },{ 0, 3 },{ 7, 1 },{ 7, 4 },{ 4, 5 },{ 5, 3 }
		};

		for (int j = 0; j < 12; ++j) {
			result[j] = Line(v[index[j][0]], v[index[j][1]]);
		}

		return result;
	}

	bool CollisionFeaturesOBBvsOBB(const OBB& obb1, const OBB& obb2, CollisionReturn& collision_return)
	{
		CollisionReturn ret;

		if (glm::distance(obb1.position, obb2.position) > (glm::length(obb1.extents) + glm::length(obb2.extents)))
		{
			return false;
		}

		std::array<float, 9> o1 = GetRotationArray(obb1);
		std::array<float, 9> o2 = GetRotationArray(obb2);

		glm::vec3 test[15] = {
			glm::vec3(o1[0], o1[1], o1[2]),
			glm::vec3(o1[3], o1[4], o1[5]),
			glm::vec3(o1[6], o1[7], o1[8]),
			glm::vec3(o2[0], o2[1], o2[2]),
			glm::vec3(o2[3], o2[4], o2[5]),
			glm::vec3(o2[6], o2[7], o2[8])
		};

		for (int i = 0; i < 3; ++i) { // Fill out rest of axis
			test[6 + i * 3 + 0] = glm::cross(test[i], test[0]);
			test[6 + i * 3 + 1] = glm::cross(test[i], test[1]);
			test[6 + i * 3 + 2] = glm::cross(test[i], test[2]);
		}

		glm::vec3* hitNormal = nullptr;
		bool shouldFlip;

		for (int i = 0; i < 15; ++i) {
			if (test[i].x < 0.000001f) test[i].x = 0.0f;
			if (test[i].y < 0.000001f) test[i].y = 0.0f;
			if (test[i].z < 0.000001f) test[i].z = 0.0f;
			if (glm::length2(test[i]) < 0.001f) {
				continue;
			}

			float depth = PenetrationDepth(obb1, obb2, test[i], shouldFlip);
			if (depth <= 0.0f) {
				return false;
			}
			else if (depth < ret.depth) {
				if (shouldFlip) {
					test[i] = test[i] * -1.0f;
				}
				ret.depth = depth;
				hitNormal = &test[i];
			}
		}

		if (hitNormal == nullptr) {
			return false;
		}
		glm::vec3 axis = glm::normalize(*hitNormal);

		std::vector<glm::vec3> c1 = ClipEdgesToOBB(GetEdges(obb2), obb1);
		std::vector<glm::vec3> c2 = ClipEdgesToOBB(GetEdges(obb1), obb2);
		ret.contacts.reserve(c1.size() + c2.size());
		ret.contacts.insert(ret.contacts.end(), c1.begin(), c1.end());
		ret.contacts.insert(ret.contacts.end(), c2.begin(), c2.end());

		Interval i = GetInterval(obb1, axis);
		float distance = (i.max - i.min) * 0.5f - ret.depth * 0.5f;
		glm::vec3 pointOnPlane = obb1.position + axis * distance;

		for (int32_t i = (int32_t)(ret.contacts.size() - 1); i >= 0; --i) {
			glm::vec3 contact = ret.contacts[i];
			ret.contacts[i] = contact + (axis * glm::dot(axis, pointOnPlane - contact));

			// This bit is in the "There is more" section of the book
			for (int32_t j = (int32_t)(ret.contacts.size() - 1); j > i; --j) {
				if (glm::length2(ret.contacts[j] - ret.contacts[i]) < 0.0001f) {
					ret.contacts.erase(ret.contacts.begin() + j);
					break;
				}
			}
		}

		ret.normal = axis;

		return true;
	}
}