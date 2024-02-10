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

		bool Inside(const glm::vec3& point, float offset = 0.f) const
		{
			return (min.x - offset) <= point.x && (min.y - offset) <= point.y && (min.z - offset) <= point.z &&
				(max.x + offset) >= point.x && (max.y + offset) >= point.y && (max.z + offset) >= point.z;
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

	inline glm::vec3 CalculateClosestPointToSegment(const glm::vec3& point,
		const glm::vec3& segment_begin, const glm::vec3& segment_end)
	{
		glm::vec3 lVec = segment_end - segment_begin; // Line Vector
		if (glm::length2(lVec) < FLT_EPSILON) return segment_begin;

		// Project "point" onto the "Line Vector", computing:
		// closest(t) = start + t * (end - start)
		// T is how far along the line the projected point is
		float t = glm::dot(point - segment_begin, lVec) / glm::dot(lVec, lVec);
		// Clamp t to the 0 to 1 range
		t = glm::clamp(t, 0.f, 1.f);
		// Return projected position of t
		return segment_begin + lVec * t;
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
	struct CollisionContact
	{
		glm::vec3 position;
		glm::vec3 normal;
		float depth = FLT_MAX;

		CollisionContact(const glm::vec3 _position, const glm::vec3 _normal, const float _depth):
			position(_position), normal(_normal), depth(_depth)
		{
		}
	};
	struct CollisionReturn
	{
		uint32_t code;
		glm::vec3 normal;
		float depth = FLT_MAX;
		std::vector<CollisionContact> contacts = {};
	};
	bool CollisionFeaturesOBBvsOBB(const OBB& obb1, const OBB& obb2, CollisionReturn& collision_return);

	glm::vec3 CalculateClosestPointToOBB(const glm::vec3& point, const OBB& obb, bool& inside);
}

#endif //COLLISION_H_