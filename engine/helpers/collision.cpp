#include "collision.h"

//#pragma optimize("", off)

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

	glm::vec3 CalculateClosestPointToOBB(const glm::vec3& point, const OBB& obb, bool& inside)
	{
		glm::vec3 result = obb.position;
		glm::vec3 dir = point - obb.position;

		inside = true;
		for (int i = 0; i < 3; ++i) {
			std::array<float, 9> o = GetRotationArray(obb);
			const float* orientation = &o[i * 3];
			glm::vec3 axis(orientation[0], orientation[1], orientation[2]);

			float distance = glm::dot(dir, axis);

			if (distance > obb.extents[i])
			{
				inside = false;
				distance = obb.extents[i];
			}
			if (distance < -obb.extents[i])
			{
				inside = false;
				distance = -obb.extents[i];
			}

			result = result + (axis * distance);
		}

		return result;
	}

//Integrated from bulletphysics engine
//https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/CollisionDispatch/btBoxBoxDetector.cpp

#define btScalar float
#define btVector3 glm::vec3
#define btFabs fabsf
#define btAtan2 atan2f
#define btSqrt sqrtf
#define SIMD_EPSILON   FLT_EPSILON
#define BT_LARGE_FLOAT   1e18f
// given two boxes (p1,R1,side1) and (p2,R2,side2), collide them together and
// generate contact points. this returns 0 if there is no contact otherwise
// it returns the number of contacts generated.
// `normal' returns the contact normal.
// `depth' returns the maximum penetration depth along that normal.
// `return_code' returns a number indicating the type of contact that was
// detected:
//        1,2,3 = box 2 intersects with a face of box 1
//        4,5,6 = box 1 intersects with a face of box 2
//        7..15 = edge-edge contact
// `maxc' is the maximum number of contacts allowed to be generated, i.e.
// the size of the `contact' array.
// `contact' and `skip' are the contact array information provided to the
// collision functions. this function only fills in the position and depth
// fields.
	struct dContactGeom;
#define dDOTpq(a, b, p, q) ((a)[0] * (b)[0] + (a)[p] * (b)[q] + (a)[2 * (p)] * (b)[2 * (q)])
#define dInfinity FLT_MAX

	/*PURE_INLINE btScalar dDOT   (const btScalar *a, const btScalar *b) { return dDOTpq(a,b,1,1); }
	PURE_INLINE btScalar dDOT13 (const btScalar *a, const btScalar *b) { return dDOTpq(a,b,1,3); }
	PURE_INLINE btScalar dDOT31 (const btScalar *a, const btScalar *b) { return dDOTpq(a,b,3,1); }
	PURE_INLINE btScalar dDOT33 (const btScalar *a, const btScalar *b) { return dDOTpq(a,b,3,3); }
	*/
#define dDOT(a,b) dDOTpq(a, b, 1, 1)
#define dDOT44(a,b) dDOTpq(a, b, 4, 4)
#define dDOT41(a,b) dDOTpq(a, b, 4, 1)
#define dDOT14(a,b) dDOTpq(a, b, 1, 4)
#define dMULTIPLYOP1_331(A, op, B, C)   \
	{                                   \
		(A)[0] op dDOT41((B), (C));     \
		(A)[1] op dDOT41((B + 1), (C)); \
		(A)[2] op dDOT41((B + 2), (C)); \
	}

#define dMULTIPLYOP0_331(A, op, B, C) \
	{                                 \
		(A)[0] op dDOT((B), (C));     \
		(A)[1] op dDOT((B + 4), (C)); \
		(A)[2] op dDOT((B + 8), (C)); \
	}

#define dMULTIPLY1_331(A, B, C) dMULTIPLYOP1_331(A, =, B, C)
#define dMULTIPLY0_331(A, B, C) dMULTIPLYOP0_331(A, =, B, C)

	typedef btScalar dMatrix3[4 * 3];

	void dLineClosestApproach(const btVector3& pa, const btVector3& ua,
		const btVector3& pb, const btVector3& ub,
		btScalar* alpha, btScalar* beta);
	void dLineClosestApproach(const btVector3& pa, const btVector3& ua,
		const btVector3& pb, const btVector3& ub,
		btScalar* alpha, btScalar* beta)
	{
		btVector3 p;
		p[0] = pb[0] - pa[0];
		p[1] = pb[1] - pa[1];
		p[2] = pb[2] - pa[2];
		btScalar uaub = dDOT(ua, ub);
		btScalar q1 = dDOT(ua, p);
		btScalar q2 = -dDOT(ub, p);
		btScalar d = 1 - uaub * uaub;
		if (d <= btScalar(0.0001f))
		{
			// @@@ this needs to be made more robust
			*alpha = 0;
			*beta = 0;
		}
		else
		{
			d = 1.f / d;
			*alpha = (q1 + uaub * q2) * d;
			*beta = (uaub * q1 + q2) * d;
		}
	}

	// find all the intersection points between the 2D rectangle with vertices
	// at (+/-h[0],+/-h[1]) and the 2D quadrilateral with vertices (p[0],p[1]),
	// (p[2],p[3]),(p[4],p[5]),(p[6],p[7]).
	//
	// the intersection points are returned as x,y pairs in the 'ret' array.
	// the number of intersection points is returned by the function (this will
	// be in the range 0 to 8).

	static int intersectRectQuad2(btScalar h[2], btScalar p[8], btScalar ret[16])
	{
		// q (and r) contain nq (and nr) coordinate points for the current (and
		// chopped) polygons
		int nq = 4, nr = 0;
		btScalar buffer[16];
		btScalar* q = p;
		btScalar* r = ret;
		for (int dir = 0; dir <= 1; dir++)
		{
			// direction notation: xy[0] = x axis, xy[1] = y axis
			for (int sign = -1; sign <= 1; sign += 2)
			{
				// chop q along the line xy[dir] = sign*h[dir]
				btScalar* pq = q;
				btScalar* pr = r;
				nr = 0;
				for (int i = nq; i > 0; i--)
				{
					// go through all points in q and all lines between adjacent points
					if (sign * pq[dir] < h[dir])
					{
						// this point is inside the chopping line
						pr[0] = pq[0];
						pr[1] = pq[1];
						pr += 2;
						nr++;
						if (nr & 8)
						{
							q = r;
							goto done;
						}
					}
					btScalar* nextq = (i > 1) ? pq + 2 : q;
					if ((sign * pq[dir] < h[dir]) ^ (sign * nextq[dir] < h[dir]))
					{
						// this line crosses the chopping line
						pr[1 - dir] = pq[1 - dir] + (nextq[1 - dir] - pq[1 - dir]) /
							(nextq[dir] - pq[dir]) * (sign * h[dir] - pq[dir]);
						pr[dir] = sign * h[dir];
						pr += 2;
						nr++;
						if (nr & 8)
						{
							q = r;
							goto done;
						}
					}
					pq += 2;
				}
				q = r;
				r = (q == ret) ? buffer : ret;
				nq = nr;
			}
		}
	done:
		if (q != ret) memcpy(ret, q, nr * 2 * sizeof(btScalar));
		return nr;
	}

#define M__PI 3.14159265f

	// given n points in the plane (array p, of size 2*n), generate m points that
	// best represent the whole set. the definition of 'best' here is not
	// predetermined - the idea is to select points that give good box-box
	// collision detection behavior. the chosen point indexes are returned in the
	// array iret (of size m). 'i0' is always the first entry in the array.
	// n must be in the range [1..8]. m must be in the range [1..n]. i0 must be
	// in the range [0..n-1].

	void cullPoints2(int n, btScalar p[], int m, int i0, int iret[]);
	void cullPoints2(int n, btScalar p[], int m, int i0, int iret[])
	{
		// compute the centroid of the polygon in cx,cy
		int i, j;
		btScalar a, cx, cy, q;
		if (n == 1)
		{
			cx = p[0];
			cy = p[1];
		}
		else if (n == 2)
		{
			cx = btScalar(0.5) * (p[0] + p[2]);
			cy = btScalar(0.5) * (p[1] + p[3]);
		}
		else
		{
			a = 0;
			cx = 0;
			cy = 0;
			for (i = 0; i < (n - 1); i++)
			{
				q = p[i * 2] * p[i * 2 + 3] - p[i * 2 + 2] * p[i * 2 + 1];
				a += q;
				cx += q * (p[i * 2] + p[i * 2 + 2]);
				cy += q * (p[i * 2 + 1] + p[i * 2 + 3]);
			}
			q = p[n * 2 - 2] * p[1] - p[0] * p[n * 2 - 1];
			if (btFabs(a + q) > SIMD_EPSILON)
			{
				a = 1.f / (btScalar(3.0) * (a + q));
			}
			else
			{
				a = BT_LARGE_FLOAT;
			}
			cx = a * (cx + q * (p[n * 2 - 2] + p[0]));
			cy = a * (cy + q * (p[n * 2 - 1] + p[1]));
		}

		// compute the angle of each point w.r.t. the centroid
		btScalar A[8];
		for (i = 0; i < n; i++) A[i] = btAtan2(p[i * 2 + 1] - cy, p[i * 2] - cx);

		// search for points that have angles closest to A[i0] + i*(2*pi/m).
		int avail[8];
		for (i = 0; i < n; i++) avail[i] = 1;
		avail[i0] = 0;
		iret[0] = i0;
		iret++;
		for (j = 1; j < m; j++)
		{
			a = btScalar(j) * (2 * M__PI / m) + A[i0];
			if (a > M__PI) a -= 2 * M__PI;
			btScalar maxdiff = 1e9, diff;

			*iret = i0;  // iret is not allowed to keep this value, but it sometimes does, when diff=#QNAN0

			for (i = 0; i < n; i++)
			{
				if (avail[i])
				{
					diff = btFabs(A[i] - a);
					if (diff > M__PI) diff = 2 * M__PI - diff;
					if (diff < maxdiff)
					{
						maxdiff = diff;
						*iret = i;
					}
				}
			}
#if defined(DEBUG) || defined(_DEBUG)
			btAssert(*iret != i0);  // ensure iret got set
#endif
			avail[*iret] = 0;
			iret++;
		}
	}

	//int dBoxBox2(const btVector3& p1, const dMatrix3 R1,
	//	const btVector3& side1, const btVector3& p2,
	//	const dMatrix3 R2, const btVector3& side2,
	//	btVector3& normal, btScalar* depth, int* return_code,
	//	int maxc, dContactGeom* /*contact*/, int /*skip*/, btDiscreteCollisionDetectorInterface::Result& output)

	bool CollisionFeaturesOBBvsOBB(const OBB& obb1, const OBB& obb2, CollisionReturn& collision_return)
	{
		btVector3& normal = collision_return.normal;
		btScalar* depth = &collision_return.depth;
		const btVector3 p1 = obb1.position;
		dMatrix3 R1 = {};
		R1[0] = obb1.rotation[0][0];
		R1[1] = obb1.rotation[0][1];
		R1[2] = obb1.rotation[0][2];
		R1[3] = 0.f;
		R1[4] = obb1.rotation[1][0];
		R1[5] = obb1.rotation[1][1];
		R1[6] = obb1.rotation[1][2];
		R1[7] = 0.f;
		R1[8] = obb1.rotation[2][0];
		R1[9] = obb1.rotation[2][1];
		R1[10] = obb1.rotation[2][2];
		R1[11] = 0.f;
		

		const btVector3 side1 = obb1.extents * 2.f;

		const btVector3 p2 = obb2.position;
		dMatrix3 R2 = {};
		R2[0] = obb2.rotation[0][0];
		R2[1] = obb2.rotation[0][1];
		R2[2] = obb2.rotation[0][2];
		R2[3] = 0.f;
		R2[4] = obb2.rotation[1][0];
		R2[5] = obb2.rotation[1][1];
		R2[6] = obb2.rotation[1][2];
		R2[7] = 0.f;
		R2[8] = obb2.rotation[2][0];
		R2[9] = obb2.rotation[2][1];
		R2[10] = obb2.rotation[2][2];
		R2[11] = 0.f;

		const btVector3 side2 = obb2.extents * 2.f;

		const btScalar fudge_factor = btScalar(1.05);
		btVector3 p, pp, normalC(0.f, 0.f, 0.f);
		const btScalar* normalR = 0;
		btScalar A[3], B[3], R11, R12, R13, R21, R22, R23, R31, R32, R33,
			Q11, Q12, Q13, Q21, Q22, Q23, Q31, Q32, Q33, s, s2, l;
		int i, j, invert_normal, code;

		// get vector from centers of box 1 to box 2, relative to box 1
		p = p2 - p1;
		dMULTIPLY1_331(pp, R1, p);  // get pp = p relative to body 1

		// get side lengths / 2
		A[0] = side1[0] * btScalar(0.5);
		A[1] = side1[1] * btScalar(0.5);
		A[2] = side1[2] * btScalar(0.5);
		B[0] = side2[0] * btScalar(0.5);
		B[1] = side2[1] * btScalar(0.5);
		B[2] = side2[2] * btScalar(0.5);

		// Rij is R1'*R2, i.e. the relative rotation between R1 and R2
		R11 = dDOT44(R1 + 0, R2 + 0);
		R12 = dDOT44(R1 + 0, R2 + 1);
		R13 = dDOT44(R1 + 0, R2 + 2);
		R21 = dDOT44(R1 + 1, R2 + 0);
		R22 = dDOT44(R1 + 1, R2 + 1);
		R23 = dDOT44(R1 + 1, R2 + 2);
		R31 = dDOT44(R1 + 2, R2 + 0);
		R32 = dDOT44(R1 + 2, R2 + 1);
		R33 = dDOT44(R1 + 2, R2 + 2);

		Q11 = btFabs(R11);
		Q12 = btFabs(R12);
		Q13 = btFabs(R13);
		Q21 = btFabs(R21);
		Q22 = btFabs(R22);
		Q23 = btFabs(R23);
		Q31 = btFabs(R31);
		Q32 = btFabs(R32);
		Q33 = btFabs(R33);

		// for all 15 possible separating axes:
		//   * see if the axis separates the boxes. if so, return 0.
		//   * find the depth of the penetration along the separating axis (s2)
		//   * if this is the largest depth so far, record it.
		// the normal vector will be set to the separating axis with the smallest
		// depth. note: normalR is set to point to a column of R1 or R2 if that is
		// the smallest depth normal so far. otherwise normalR is 0 and normalC is
		// set to a vector relative to body 1. invert_normal is 1 if the sign of
		// the normal should be flipped.

#define TST(expr1, expr2, norm, cc)    \
	s2 = btFabs(expr1) - (expr2);      \
	if (s2 > 0) return 0;              \
	if (s2 > s)                        \
	{                                  \
		s = s2;                        \
		normalR = norm;                \
		invert_normal = ((expr1) < 0); \
		code = (cc);                   \
	}

		s = -dInfinity;
		invert_normal = 0;
		code = 0;

		// separating axis = u1,u2,u3
		TST(pp[0], (A[0] + B[0] * Q11 + B[1] * Q12 + B[2] * Q13), R1 + 0, 1);
		TST(pp[1], (A[1] + B[0] * Q21 + B[1] * Q22 + B[2] * Q23), R1 + 1, 2);
		TST(pp[2], (A[2] + B[0] * Q31 + B[1] * Q32 + B[2] * Q33), R1 + 2, 3);

		// separating axis = v1,v2,v3
		TST(dDOT41(R2 + 0, p), (A[0] * Q11 + A[1] * Q21 + A[2] * Q31 + B[0]), R2 + 0, 4);
		TST(dDOT41(R2 + 1, p), (A[0] * Q12 + A[1] * Q22 + A[2] * Q32 + B[1]), R2 + 1, 5);
		TST(dDOT41(R2 + 2, p), (A[0] * Q13 + A[1] * Q23 + A[2] * Q33 + B[2]), R2 + 2, 6);

		// note: cross product axes need to be scaled when s is computed.
		// normal (n1,n2,n3) is relative to box 1.
#undef TST
#define TST(expr1, expr2, n1, n2, n3, cc)                \
	s2 = btFabs(expr1) - (expr2);                        \
	if (s2 > SIMD_EPSILON) return 0;                     \
	l = btSqrt((n1) * (n1) + (n2) * (n2) + (n3) * (n3)); \
	if (l > SIMD_EPSILON)                                \
	{                                                    \
		s2 /= l;                                         \
		if (s2 * fudge_factor > s)                       \
		{                                                \
			s = s2;                                      \
			normalR = 0;                                 \
			normalC[0] = (n1) / l;                       \
			normalC[1] = (n2) / l;                       \
			normalC[2] = (n3) / l;                       \
			invert_normal = ((expr1) < 0);               \
			code = (cc);                                 \
		}                                                \
	}

		btScalar fudge2(1.0e-5f);

		Q11 += fudge2;
		Q12 += fudge2;
		Q13 += fudge2;

		Q21 += fudge2;
		Q22 += fudge2;
		Q23 += fudge2;

		Q31 += fudge2;
		Q32 += fudge2;
		Q33 += fudge2;

		// separating axis = u1 x (v1,v2,v3)
		TST(pp[2] * R21 - pp[1] * R31, (A[1] * Q31 + A[2] * Q21 + B[1] * Q13 + B[2] * Q12), 0, -R31, R21, 7);
		TST(pp[2] * R22 - pp[1] * R32, (A[1] * Q32 + A[2] * Q22 + B[0] * Q13 + B[2] * Q11), 0, -R32, R22, 8);
		TST(pp[2] * R23 - pp[1] * R33, (A[1] * Q33 + A[2] * Q23 + B[0] * Q12 + B[1] * Q11), 0, -R33, R23, 9);

		// separating axis = u2 x (v1,v2,v3)
		TST(pp[0] * R31 - pp[2] * R11, (A[0] * Q31 + A[2] * Q11 + B[1] * Q23 + B[2] * Q22), R31, 0, -R11, 10);
		TST(pp[0] * R32 - pp[2] * R12, (A[0] * Q32 + A[2] * Q12 + B[0] * Q23 + B[2] * Q21), R32, 0, -R12, 11);
		TST(pp[0] * R33 - pp[2] * R13, (A[0] * Q33 + A[2] * Q13 + B[0] * Q22 + B[1] * Q21), R33, 0, -R13, 12);

		// separating axis = u3 x (v1,v2,v3)
		TST(pp[1] * R11 - pp[0] * R21, (A[0] * Q21 + A[1] * Q11 + B[1] * Q33 + B[2] * Q32), -R21, R11, 0, 13);
		TST(pp[1] * R12 - pp[0] * R22, (A[0] * Q22 + A[1] * Q12 + B[0] * Q33 + B[2] * Q31), -R22, R12, 0, 14);
		TST(pp[1] * R13 - pp[0] * R23, (A[0] * Q23 + A[1] * Q13 + B[0] * Q32 + B[1] * Q31), -R23, R13, 0, 15);

#undef TST

		if (!code) return false;

		// if we get to this point, the boxes interpenetrate. compute the normal
		// in global coordinates.
		if (normalR)
		{
			normal[0] = normalR[0];
			normal[1] = normalR[4];
			normal[2] = normalR[8];
		}
		else
		{
			dMULTIPLY0_331(normal, R1, normalC);
		}
		if (invert_normal)
		{
			normal[0] = -normal[0];
			normal[1] = -normal[1];
			normal[2] = -normal[2];
		}
		*depth = -s;

		// compute contact point(s)

		if (code > 6)
		{
			// an edge from box 1 touches an edge from box 2.
			// find a point pa on the intersecting edge of box 1
			btVector3 pa;
			btScalar sign;
			for (i = 0; i < 3; i++) pa[i] = p1[i];
			for (j = 0; j < 3; j++)
			{
				sign = (dDOT14(normal, R1 + j) > 0) ? btScalar(1.0) : btScalar(-1.0);
				for (i = 0; i < 3; i++) pa[i] += sign * A[j] * R1[i * 4 + j];
			}

			// find a point pb on the intersecting edge of box 2
			btVector3 pb;
			for (i = 0; i < 3; i++) pb[i] = p2[i];
			for (j = 0; j < 3; j++)
			{
				sign = (dDOT14(normal, R2 + j) > 0) ? btScalar(-1.0) : btScalar(1.0);
				for (i = 0; i < 3; i++) pb[i] += sign * B[j] * R2[i * 4 + j];
			}

			btScalar alpha, beta;
			btVector3 ua, ub;
			for (i = 0; i < 3; i++) ua[i] = R1[((code)-7) / 3 + i * 4];
			for (i = 0; i < 3; i++) ub[i] = R2[((code)-7) % 3 + i * 4];

			dLineClosestApproach(pa, ua, pb, ub, &alpha, &beta);
			for (i = 0; i < 3; i++) pa[i] += ua[i] * alpha;
			for (i = 0; i < 3; i++) pb[i] += ub[i] * beta;

			{
				//contact[0].pos[i] = btScalar(0.5)*(pa[i]+pb[i]);
				//contact[0].depth = *depth;
				
#ifdef USE_CENTER_POINT
				btVector3 pointInWorld;
				for (i = 0; i < 3; i++)
					pointInWorld[i] = (pa[i] + pb[i]) * btScalar(0.5);
				collision_return.contacts.emplace_back(pointInWorld, -normal, -*depth);
#else
				collision_return.contacts.emplace_back(pb , -normal, -*depth);

#endif  //
				collision_return.code = code;
			}
			return true;
		}

		// okay, we have a face-something intersection (because the separating
		// axis is perpendicular to a face). define face 'a' to be the reference
		// face (i.e. the normal vector is perpendicular to this) and face 'b' to be
		// the incident face (the closest face of the other box).

		const btScalar* Ra, * Rb, * Sa, * Sb;
		btVector3 pa, pb;
		if (code <= 3)
		{
			Ra = R1;
			Rb = R2;
			pa = p1;
			pb = p2;
			Sa = A;
			Sb = B;
		}
		else
		{
			Ra = R2;
			Rb = R1;
			pa = p2;
			pb = p1;
			Sa = B;
			Sb = A;
		}

		// nr = normal vector of reference face dotted with axes of incident box.
		// anr = absolute values of nr.
		btVector3 normal2, nr, anr;
		if (code <= 3)
		{
			normal2[0] = normal[0];
			normal2[1] = normal[1];
			normal2[2] = normal[2];
		}
		else
		{
			normal2[0] = -normal[0];
			normal2[1] = -normal[1];
			normal2[2] = -normal[2];
		}
		dMULTIPLY1_331(nr, Rb, normal2);
		anr[0] = btFabs(nr[0]);
		anr[1] = btFabs(nr[1]);
		anr[2] = btFabs(nr[2]);

		// find the largest compontent of anr: this corresponds to the normal
		// for the indident face. the other axis numbers of the indicent face
		// are stored in a1,a2.
		int lanr, a1, a2;
		if (anr[1] > anr[0])
		{
			if (anr[1] > anr[2])
			{
				a1 = 0;
				lanr = 1;
				a2 = 2;
			}
			else
			{
				a1 = 0;
				a2 = 1;
				lanr = 2;
			}
		}
		else
		{
			if (anr[0] > anr[2])
			{
				lanr = 0;
				a1 = 1;
				a2 = 2;
			}
			else
			{
				a1 = 0;
				a2 = 1;
				lanr = 2;
			}
		}

		// compute center point of incident face, in reference-face coordinates
		btVector3 center;
		if (nr[lanr] < 0)
		{
			for (i = 0; i < 3; i++) center[i] = pb[i] - pa[i] + Sb[lanr] * Rb[i * 4 + lanr];
		}
		else
		{
			for (i = 0; i < 3; i++) center[i] = pb[i] - pa[i] - Sb[lanr] * Rb[i * 4 + lanr];
		}

		// find the normal and non-normal axis numbers of the reference box
		int codeN, code1, code2;
		if (code <= 3)
			codeN = code - 1;
		else
			codeN = code - 4;
		if (codeN == 0)
		{
			code1 = 1;
			code2 = 2;
		}
		else if (codeN == 1)
		{
			code1 = 0;
			code2 = 2;
		}
		else
		{
			code1 = 0;
			code2 = 1;
		}

		// find the four corners of the incident face, in reference-face coordinates
		btScalar quad[8];  // 2D coordinate of incident face (x,y pairs)
		btScalar c1, c2, m11, m12, m21, m22;
		c1 = dDOT14(center, Ra + code1);
		c2 = dDOT14(center, Ra + code2);
		// optimize this? - we have already computed this data above, but it is not
		// stored in an easy-to-index format. for now it's quicker just to recompute
		// the four dot products.
		m11 = dDOT44(Ra + code1, Rb + a1);
		m12 = dDOT44(Ra + code1, Rb + a2);
		m21 = dDOT44(Ra + code2, Rb + a1);
		m22 = dDOT44(Ra + code2, Rb + a2);
		{
			btScalar k1 = m11 * Sb[a1];
			btScalar k2 = m21 * Sb[a1];
			btScalar k3 = m12 * Sb[a2];
			btScalar k4 = m22 * Sb[a2];
			quad[0] = c1 - k1 - k3;
			quad[1] = c2 - k2 - k4;
			quad[2] = c1 - k1 + k3;
			quad[3] = c2 - k2 + k4;
			quad[4] = c1 + k1 + k3;
			quad[5] = c2 + k2 + k4;
			quad[6] = c1 + k1 - k3;
			quad[7] = c2 + k2 - k4;
		}

		// find the size of the reference face
		btScalar rect[2];
		rect[0] = Sa[code1];
		rect[1] = Sa[code2];

		// intersect the incident and reference faces
		btScalar ret[16];
		int n = intersectRectQuad2(rect, quad, ret);
		if (n < 1) return 0;  // this should never happen

		// convert the intersection points into reference-face coordinates,
		// and compute the contact position and depth for each point. only keep
		// those points that have a positive (penetrating) depth. delete points in
		// the 'ret' array as necessary so that 'point' and 'ret' correspond.
		btScalar point[3 * 8];  // penetrating contact points
		btScalar dep[8];        // depths for those points
		btScalar det1 = 1.f / (m11 * m22 - m12 * m21);
		m11 *= det1;
		m12 *= det1;
		m21 *= det1;
		m22 *= det1;
		int cnum = 0;  // number of penetrating contact points found
		for (j = 0; j < n; j++)
		{
			btScalar k1 = m22 * (ret[j * 2] - c1) - m12 * (ret[j * 2 + 1] - c2);
			btScalar k2 = -m21 * (ret[j * 2] - c1) + m11 * (ret[j * 2 + 1] - c2);
			for (i = 0; i < 3; i++) point[cnum * 3 + i] =
				center[i] + k1 * Rb[i * 4 + a1] + k2 * Rb[i * 4 + a2];
			dep[cnum] = Sa[codeN] - dDOT(normal2, point + cnum * 3);
			if (dep[cnum] >= 0)
			{
				ret[cnum * 2] = ret[j * 2];
				ret[cnum * 2 + 1] = ret[j * 2 + 1];
				cnum++;
			}
		}
		//if (cnum < 1) return 0;  // this should never happen

		// we can't generate more contacts than we actually have
		//if (maxc > cnum) maxc = cnum;
		//if (maxc < 1) maxc = 1;

		//if (cnum <= maxc)
		{
			if (code < 4)
			{
				// we have less contacts than we need, so we use them all
				for (j = 0; j < cnum; j++)
				{
					btVector3 pointInWorld;
					for (i = 0; i < 3; i++)
						pointInWorld[i] = point[j * 3 + i] + pa[i];
					collision_return.contacts.emplace_back(pointInWorld, -normal, -dep[j]);
				}
			}
			else
			{
				// we have less contacts than we need, so we use them all
				for (j = 0; j < cnum; j++)
				{
					btVector3 pointInWorld;
					for (i = 0; i < 3; i++)
						pointInWorld[i] = point[j * 3 + i] + pa[i] - normal[i] * dep[j];
					//pointInWorld[i] = point[j*3+i] + pa[i];
					collision_return.contacts.emplace_back(pointInWorld, -normal, -dep[j]);
				}
			}
		}
		/*else
		{
			// we have more contacts than are wanted, some of them must be culled.
			// find the deepest point, it is always the first contact.
			int i1 = 0;
			btScalar maxdepth = dep[0];
			for (i = 1; i < cnum; i++)
			{
				if (dep[i] > maxdepth)
				{
					maxdepth = dep[i];
					i1 = i;
				}
			}

			int iret[8];
			cullPoints2(cnum, ret, maxc, i1, iret);

			for (j = 0; j < maxc; j++)
			{
				//      dContactGeom *con = CONTACT(contact,skip*j);
				//    for (i=0; i<3; i++) con->pos[i] = point[iret[j]*3+i] + pa[i];
				//  con->depth = dep[iret[j]];

				btVector3 posInWorld;
				for (i = 0; i < 3; i++)
					posInWorld[i] = point[iret[j] * 3 + i] + pa[i];
				if (code < 4)
				{
					output.addContactPoint(-normal, posInWorld, -dep[iret[j]]);
				}
				else
				{
					output.addContactPoint(-normal, posInWorld - normal * dep[iret[j]], -dep[iret[j]]);
				}
			}
			cnum = maxc;
		}*/

		collision_return.code = code;
		return true;
	}
}