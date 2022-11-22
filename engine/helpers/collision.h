#ifndef COLLISION_H_
#define COLLISION_H_

#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/mat4x4.hpp>
#include <ext/glm/gtc/constants.hpp>
#include <ext/glm/gtc/matrix_access.hpp>
#include <utility>
#include "camera.h"

namespace helpers
{
	struct AABB
	{
		glm::vec3 min = glm::vec3(FLT_MIN, FLT_MIN, FLT_MIN);
		glm::vec3 max = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);

		bool IsValid() const
		{
			return min.x < max.x;
		}

		void Add(const AABB& b)
		{
			if (IsValid())
			{
				min = glm::min(min, b.min);
				max = glm::max(max, b.max);
			}
			else
			{
				min = b.min;
				max = b.max;
			}
		}
		void Add(const glm::vec3& point)
		{
			if (IsValid())
			{
				min = glm::min(min, point);
				max = glm::max(max, point);
			}
			else
			{
				min = point;
				max = point;
			}
		}
	};

	struct OBB
	{
		glm::vec3 position;
		glm::mat3x3 rotation;
		glm::vec3 extents;
	};

	inline bool CollisionFrustumVsAABB(const Frustum& frustum, const AABB& bounding_box)
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

	inline bool CollisionOBBVsOBB(const OBB& a, const OBB& b)
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
		return 1;
	}

	inline bool CollisionAABBVsAABB(const AABB& a, const AABB& b)
	{
		// Exit with no intersection if separated along an axis
		if (a.max[0] < b.min[0] || a.min[0] > b.max[0]) return false;
		if (a.max[1] < b.min[1] || a.min[1] > b.max[1]) return false;
		if (a.max[2] < b.min[2] || a.min[2] > b.max[2]) return false;
		// Overlapping
		return true;
	}

	inline void CalculateAABBFromOBB(AABB& output, const OBB& source)
	{
		//Calculate the max distance
		glm::vec3 half_distance = glm::abs(glm::row(source.rotation, 0) * source.extents[0]) + glm::abs(glm::row(source.rotation, 1) * source.extents[1]) + glm::abs(glm::row(source.rotation, 2) * source.extents[2]);
		
		output.min = source.position - half_distance;
		output.max = source.position + half_distance;
	}
}

#endif //CAMERA_H