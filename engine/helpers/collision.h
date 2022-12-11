#ifndef COLLISION_H_
#define COLLISION_H_

#include <ext/glm/glm.hpp>
#include <ext/glm/ext.hpp>
#include <ext/glm/gtx/norm.hpp>
#include <utility>
#include "camera.h"

namespace helpers
{
	struct AABB
	{
		glm::vec3 min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
		glm::vec3 max = glm::vec3(FLT_MIN, FLT_MIN, FLT_MIN);

		bool IsValid() const
		{
			return min.x <= max.x;
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

	bool CollisionFrustumVsAABB(const Frustum& frustum, const AABB& bounding_box);

	bool CollisionOBBVsOBB(const OBB& a, const OBB& b);

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

	inline void CalculateProjectionPointToSegment(const glm::vec3& point,
		const glm::vec3& segment_begin, const glm::vec3& segment_end,
		float& projected_delta)
	{
		glm::vec3 AB = segment_end - segment_begin;
		float AB_squared = glm::dot(AB, AB);
		if (AB_squared == 0.f)
		{
			projected_delta = 0.f;
		}
		else
		{
			glm::vec3 Ap = point - segment_begin;
			float t = glm::dot(Ap, AB) / AB_squared;
			projected_delta = glm::clamp(projected_delta, 0.f, 1.f);
		}
	}

	inline void CalculateClosestPointsInTwoSegments(const glm::vec3& segment_a_begin, const glm::vec3& segment_a_end,
		const glm::vec3& segment_b_begin, const glm::vec3& segment_b_end,
		glm::vec3& segment_a_point, glm::vec3& segment_b_point,
		float& t_a, float& t_b)
	{
		glm::vec3 P1 = segment_a_begin;
		glm::vec3 P2 = segment_b_begin;
		glm::vec3 V1 = segment_a_end - segment_a_begin;
		glm::vec3 V2 = segment_b_end - segment_b_begin;
		glm::vec3 V21 = P2 - P1;

		float v22 = glm::dot(V2, V2);
		float v11 = glm::dot(V1, V1);
		float v21 = glm::dot(V2, V1);
		float v21_1 = glm::dot(V21, V1);
		float v21_2 = glm::dot(V21, V2);
		float denom = v21 * v21 - v22 * v11;

		float s, t;
		if (glm::abs<float>(denom) < 0.0001f)
		{
			s = 0.f;
			t = (v11 * s - v21_1) / v21;
		}
		else
		{
			s = (v21_2 * v21 - v22 * v21_1) / denom;
			t = (-v21_1 * v21 + v11 * v21_2) / denom;
		}

		t_a = glm::clamp(s, 0.f, 1.f);
		t_b = glm::clamp(t, 0.f, 1.f);

		segment_a_point = P1 + t_a * V1;
		segment_b_point = P2 + t_b * V2;
	}
	
	struct CollisionReturn
	{
		glm::vec3 normal = glm::vec3(0.f, 0.f, 0.f);
		float depth = 0.f;
		std::vector<glm::vec3> contacts = {};
	};
	bool CollisionFeaturesOBBvsOBB(const OBB& obb1, const OBB& obb2, CollisionReturn& collision_return);
}

#endif //COLLISION_H_