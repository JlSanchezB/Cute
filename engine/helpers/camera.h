#ifndef CAMERA_H_
#define CAMERA_H_

#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/mat4x4.hpp>
#include <ext/glm/gtc/constants.hpp>

namespace platform
{
	class Game;
}

namespace helpers
{
	class Frustum
	{
	public:
		enum Planes
		{
			Left = 0,
			Right,
			Bottom,
			Top,
			Near,
			Far,
			Count,
			Combinations = Count * (Count - 1) / 2
		};

		void Init(const glm::mat4x4& view_projection_matrix);

		//Frustum planes
		glm::vec4   planes[Planes::Count];

		//Frustum points
		glm::vec3   points[8];
	};

	class Camera : public Frustum
	{
	public:
		enum class ZRange
		{
			ZeroOne,
			OneZero
		};

		enum class Type
		{
			Rotation,
			Target
		};

		void UpdateAspectRatio(float aspect_ratio)
		{
			m_aspect_ratio = aspect_ratio;
		}

		glm::mat4x4 GetViewProjectionMatrix() const;

		glm::vec3 GetPosition() const
		{
			return m_position;
		}
		float GetNearPlane() const
		{
			return m_near;
		}

		float GetFarPlane() const
		{
			return m_far;
		}


		//Set near/far
		void SetNearFar(const float near, const float far)
		{
			m_near = near;
			m_far = far;
		}

		Camera(const Type type, const ZRange z_range) : m_type(type), m_z_range(z_range)
		{
		}
	protected:

		void UpdateInternalData();

		//Setup
		Type m_type;
		ZRange m_z_range;
		glm::vec3 m_position = glm::vec3(0.f, 0.f, 0.f);
		glm::vec2 m_rotation = glm::vec2(0.f, 0.f);
		glm::vec3 m_target = glm::vec3(0.f, 0.f, 0.f);
		glm::vec3 m_up_vector = glm::vec3(0.f, 0.f, 1.f);
		float m_fov_y = glm::half_pi<float>();
		float m_aspect_ratio = 1.f;
		float m_far = 10000.f;
		float m_near = 0.1f;
	
	private:

		//Matrices
		glm::mat4x4 m_world_to_view_matrix = glm::mat4x4();
		glm::mat4x4 m_projection_matrix = glm::mat4x4();
		glm::mat4x4 m_view_projection_matrix = glm::mat4x4();

	};

	class FlyCamera : public Camera
	{
	public:
		//Process input and update the position
		void Update(platform::Game* game, float ellapsed_time);

		FlyCamera(const Camera::ZRange z_range) : Camera(Type::Rotation, z_range)
		{
		}
	private: 
		//State
		glm::vec3 m_move_speed = glm::vec3(0.f, 0.f, 0.f);
		glm::vec2 m_rotation_speed = glm::vec2(0.f, 0.f);
		float m_move_speed_factor = 1.f;

		//Setup
		float m_mouse_rotate_factor = 0.0025f;
		float m_mouse_move_factor = 0.0025f;
		float m_damp_factor = 5.0f;
		float m_move_factor = 50.0f;
		float m_rotation_factor = 20.f;
		float m_wheel_factor = 0.1f;
	};
}

#endif //CAMERA_H