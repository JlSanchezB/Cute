#ifndef COLLISION_H_
#define COLLISION_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL

#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/mat4x4.hpp>
#include <ext/glm/gtc/constants.hpp>
#include <ext/glm/gtc/matrix_access.hpp>
#include "camera.h"

namespace helpers
{
	bool CollisionFrustumVsAABB(Frustum& frustum, const glm::vec3& min_box, const glm::vec3& max_box)
	{
		const glm::vec4* planes = frustum.GetPlanes();
		const glm::vec3* points = frustum.GetPoints();
		// check box outside/inside of frustum
		for (int i = 0; i < Frustum::Count; i++)
		{
			if ((glm::dot(planes[i], glm::vec4(min_box.x, min_box.y, min_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(max_box.x, min_box.y, min_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(min_box.x, max_box.y, min_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(max_box.x, max_box.y, min_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(min_box.x, min_box.y, max_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(max_box.x, min_box.y, max_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(min_box.x, max_box.y, max_box.z, 1.0f)) < 0.0) &&
				(glm::dot(planes[i], glm::vec4(max_box.x, max_box.y, max_box.z, 1.0f)) < 0.0))
			{
				return false;
			}
		}

		// check frustum outside/inside box
		int out;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x > max_box.x) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x < min_box.x) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y > max_box.y) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y < min_box.y) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z > max_box.z) ? 1 : 0); if (out == 8) return false;
		out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z < min_box.z) ? 1 : 0); if (out == 8) return false;

		return true;
	}

	bool CollisionOBBVsOBB(const glm::vec3& position_a, const glm::mat3x3& rotation_a, const glm::vec3 extents_a,
		const glm::vec3& position_b, const glm::mat3x3& rotation_b, const glm::vec3 extents_b)
	{
		float ra, rb;
		glm::mat3x3 R, AbsR;
		// Compute rotation matrix expressing b in a’s coordinate frame
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				R[i][j] = glm::dot(glm::row(rotation_a, i), glm::row(rotation_b, j));

		// Compute translation vector t
		glm::vec3 t = position_b - position_a;
		// Bring translation into a’s coordinate frame
		t = glm::vec3(glm::dot(t, glm::row(rotation_a, 0)), glm::dot(t, glm::row(rotation_a, 1)), glm::dot(t, glm::row(rotation_a, 2)));
		// Compute common subexpressions. Add in an epsilon term to
		// counteract arithmetic errors when two edges are parallel and
		// their cross product is (near) null (see text for details)
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				AbsR[i][j] = fabsf(R[i][j]) + 0.00001f;

		// Test axes L = A0, L = A1, L = A2
		for (int i = 0; i < 3; i++) {
			ra = extents_a[i];
			rb = extents_b[0] * AbsR[i][0] + extents_b[1] * AbsR[i][1] + extents_b[2] * AbsR[i][2];
			if (fabsf(t[i]) > ra + rb) return false;
		}
		// Test axes L = B0, L = B1, L = B2
		for (int i = 0; i < 3; i++) {
			ra = extents_a[0] * AbsR[0][i] + extents_a[1] * AbsR[1][i] + extents_a[2] * AbsR[2][i];
			rb = extents_b[i];
			if (fabsf(t[0] * R[0][i] + t[1] * R[1][i] + t[2] * R[2][i]) > ra + rb) return false;
		}
		// Test axis L = A0 x B0
		ra = extents_a[1] * AbsR[2][0] + extents_a[2] * AbsR[1][0];
		rb = extents_b[1] * AbsR[0][2] + extents_b[2] * AbsR[0][1];
		if (fabsf(t[2] * R[1][0] - t[1] * R[2][0]) > ra + rb) return false;
		// Test axis L = A0 x B1
		ra = extents_a[1] * AbsR[2][1] + extents_a[2] * AbsR[1][1];
		rb = extents_b[0] * AbsR[0][2] + extents_b[2] * AbsR[0][0];
		if (fabsf(t[2] * R[1][1] - t[1] * R[2][1]) > ra + rb) return false;
		// Test axis L = A0 x B2
		ra = extents_a[1] * AbsR[2][2] + extents_a[2] * AbsR[1][2];
		rb = extents_b[0] * AbsR[0][1] + extents_b[1] * AbsR[0][0];
		if (fabsf(t[2] * R[1][2] - t[1] * R[2][2]) > ra + rb) return false;
		// Test axis L = A1 x B0
		ra = extents_a[0] * AbsR[2][0] + extents_a[2] * AbsR[0][0];
		rb = extents_b[1] * AbsR[1][2] + extents_b[2] * AbsR[1][1];

		if (fabsf(t[0] * R[2][0] - t[2] * R[0][0]) > ra + rb) return false;
		// Test axis L = A1 x B1
		ra = extents_a[0] * AbsR[2][1] + extents_a[2] * AbsR[0][1];
		rb = extents_b[0] * AbsR[1][2] + extents_b[2] * AbsR[1][0];
		if (fabsf(t[0] * R[2][1] - t[2] * R[0][1]) > ra + rb) return false;
		// Test axis L = A1 x B2
		ra = extents_a[0] * AbsR[2][2] + extents_a[2] * AbsR[0][2];
		rb = extents_b[0] * AbsR[1][1] + extents_b[1] * AbsR[1][0];
		if (fabsf(t[0] * R[2][2] - t[2] * R[0][2]) > ra + rb) return false;
		// Test axis L = A2 x B0
		ra = extents_a[0] * AbsR[1][0] + extents_a[1] * AbsR[0][0];
		rb = extents_b[1] * AbsR[2][2] + extents_b[2] * AbsR[2][1];
		if (fabsf(t[1] * R[0][0] - t[0] * R[1][0]) > ra + rb) return false;
		// Test axis L = A2 x B1
		ra = extents_a[0] * AbsR[1][1] + extents_a[1] * AbsR[0][1];
		rb = extents_b[0] * AbsR[2][2] + extents_b[2] * AbsR[2][0];
		if (fabsf(t[1] * R[0][1] - t[0] * R[1][1]) > ra + rb) return false;
		// Test axis L = A2 x B2
		ra = extents_a[0] * AbsR[1][2] + extents_a[1] * AbsR[0][2];
		rb = extents_b[0] * AbsR[2][1] + extents_b[1] * AbsR[2][0];
		if (fabsf(t[1] * R[0][2] - t[0] * R[1][2]) > ra + rb) return false;
		// Since no separating axis is found, the OBBs must be intersecting
		return 1;
	}

	void CalculateAABBFromOBB(glm::vec3& min_position, glm::vec3& max_position, const glm::vec3& position_source, const glm::mat3x3& rotation_source, const glm::vec3 extents_source)
	{
		//Calculate the max distance
		glm::vec3 half_distance = glm::abs(glm::row(rotation_source, 0) * extents_source[0]) + glm::abs(glm::row(rotation_source, 1) * extents_source[1]) + glm::abs(glm::row(rotation_source, 2) * extents_source[2]);
		
		min_position = position_source - half_distance;
		max_position = position_source + half_distance;
	}
}

#endif //CAMERA_H