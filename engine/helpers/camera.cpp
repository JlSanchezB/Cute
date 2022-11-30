#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_ENABLE_EXPERIMENTAL

#include "camera.h"
#include <core/platform.h>
#include <ext/glm/vec4.hpp>
#include <ext/glm/vec2.hpp>
#include <ext/glm/gtx/vector_angle.hpp>
#include <ext/glm/gtx/rotate_vector.hpp>
#include <ext/glm/gtx/euler_angles.hpp>

//https://gist.github.com/podgorskiy/e698d18879588ada9014768e3e82a644

namespace helpers
{
	template<FlyCamera::Planes i, FlyCamera::Planes j>
	struct ij2k
	{
		enum { k = i * (9 - i) / 2 + j - 1 };
	};

	template<FlyCamera::Planes a, FlyCamera::Planes b, FlyCamera::Planes c>
	inline glm::vec3 intersection(const glm::vec4* planes, const glm::vec3* crosses)
	{
		float D = glm::dot(glm::vec3(planes[a]), crosses[ij2k<b, c>::k]);
		glm::vec3 res = glm::mat3(crosses[ij2k<b, c>::k], -crosses[ij2k<a, c>::k], crosses[ij2k<a, b>::k]) *
			glm::vec3(planes[a].w, planes[b].w, planes[c].w);
		return res * (-1.0f / D);
	}

	//Process input and update the position
	void FlyCamera::Update(platform::Game* game, float ellapsed_time)
	{
		if (game->IsFocus())
		{
			//Read inputs
			float forward_input = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftY);
			if (game->GetInputSlotState(platform::InputSlotState::Up) || game->GetInputSlotState(platform::InputSlotState::Key_W)) forward_input += 1.f * ellapsed_time;
			if (game->GetInputSlotState(platform::InputSlotState::Down) || game->GetInputSlotState(platform::InputSlotState::Key_S)) forward_input -= 1.f * ellapsed_time;
			float side_input = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftX);
			if (game->GetInputSlotState(platform::InputSlotState::Right) || game->GetInputSlotState(platform::InputSlotState::Key_D)) side_input += 1.f * ellapsed_time;
			if (game->GetInputSlotState(platform::InputSlotState::Left) || game->GetInputSlotState(platform::InputSlotState::Key_A)) side_input -= 1.f * ellapsed_time;
			float up_input = (game->GetInputSlotValue(platform::InputSlotValue::ControllerRightTrigger) - game->GetInputSlotValue(platform::InputSlotValue::ControllerLeftTrigger));
			if (game->GetInputSlotState(platform::InputSlotState::PageUp) || game->GetInputSlotState(platform::InputSlotState::Key_Z)) up_input += 1.f * ellapsed_time;
			if (game->GetInputSlotState(platform::InputSlotState::PageDown) || game->GetInputSlotState(platform::InputSlotState::Key_X)) up_input -= 1.f * ellapsed_time;

			if (game->GetInputSlotState(platform::InputSlotState::RightMouseButton))
			{
				side_input += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * m_mouse_move_factor;
				forward_input += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * m_mouse_move_factor;
			}

			glm::vec2 rotation_input;
			rotation_input.x = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightX) * ellapsed_time;
			rotation_input.y = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightY) * ellapsed_time;
			if (game->GetInputSlotState(platform::InputSlotState::LeftMouseButton))
			{
				rotation_input.x += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * m_mouse_rotate_factor;
				rotation_input.y -= game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * m_mouse_rotate_factor;
			}

			//Wheel control
			for (auto& input_event : game->GetInputEvents())
			{
				if (input_event.type == platform::EventType::MouseWheel)
				{
					m_move_speed_factor += m_wheel_factor * input_event.value;
				}
			}
			m_move_speed_factor = glm::clamp(m_move_speed_factor, 0.2f, 5.f);

			//Calculate position movement
			glm::vec3 move_speed;

			glm::mat3x3 rot = glm::rotate(m_rotation.y, glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(m_rotation.x, glm::vec3(0.f, 0.f, 1.f));
			//Define forward and side
			glm::vec3 forward = glm::vec3(0.f, 1.f, 0.f) * rot;
			glm::vec3 side = glm::vec3(1.f, 0.f, 0.f) * rot;

			move_speed = -side_input * m_move_factor * side * m_move_speed_factor * m_move_speed_factor;
			move_speed += forward_input * m_move_factor * forward * m_move_speed_factor * m_move_speed_factor;
			//Up/Down doesn't depend of the rotation
			move_speed.z += up_input * m_move_factor;


			m_move_speed += move_speed;

			//Calculate direction movement
			glm::vec2 angles;
			angles.x = -rotation_input.x * m_rotation_factor;
			angles.y = -rotation_input.y * m_rotation_factor;

			m_rotation_speed += angles;
		}

		//Apply
		m_position += m_move_speed * ellapsed_time;
		m_rotation += m_rotation_speed * ellapsed_time;

		if (m_rotation.y > glm::radians(85.f)) m_rotation.y = glm::radians(85.f);
		if (m_rotation.y < glm::radians(-85.f)) m_rotation.y = glm::radians(-85.f);

		//Apply damp
		m_move_speed -= m_move_speed * glm::clamp((m_damp_factor * ellapsed_time), 0.f, 1.f);
		m_rotation_speed -= m_rotation_speed * glm::clamp((m_damp_factor * ellapsed_time), 0.f, 1.f);

		UpdateInternalData();
	}

	void Camera::UpdateInternalData()
	{
		//Calculate view to world
		glm::mat4x4 world_to_view_matrix;
		switch (m_type)
		{
		case Type::Rotation:
			glm::mat3x3 rot = glm::rotate(m_rotation.y, glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(m_rotation.x, glm::vec3(0.f, 0.f, 1.f));
			world_to_view_matrix =glm::lookAt(m_position, m_position + glm::vec3(0.f, 1.f, 0.f) * rot, m_up_vector);
			break;
		case Type::Target:
			world_to_view_matrix = glm::lookAt(m_position, m_target, m_up_vector);
			break;
		}	

		//Calculate projection matrix
		glm::mat4x4 projection_matrix;
		switch (m_z_range)
		{
		case ZRange::ZeroOne:
			projection_matrix = glm::perspective(m_fov_y, m_aspect_ratio, m_near, m_far);
			break;
		case ZRange::OneZero:
			//Inverse near - far
			projection_matrix = glm::perspective(m_fov_y, m_aspect_ratio, m_far, m_near);
			break;
		}

		m_world_to_view_matrix = world_to_view_matrix;
		m_projection_matrix = projection_matrix;

		//Calculate view projection
		m_view_projection_matrix = m_projection_matrix * m_world_to_view_matrix;

		//If it is the reverseZ, the FrustumInit only works with a normal 0-1, we could calculate the differences or just recalculate it to 0-1
		glm::mat4x4 frustum_view_projection_matrix = m_view_projection_matrix;

		if (m_z_range == ZRange::OneZero)
		{
			frustum_view_projection_matrix = glm::perspective(m_fov_y, m_aspect_ratio, m_near, m_far) * m_world_to_view_matrix;
		}

		Frustum::Init(frustum_view_projection_matrix);
	}

	glm::mat4x4 Camera::GetViewProjectionMatrix() const
	{
		return m_view_projection_matrix;
	}
	
	void Frustum::Init(const glm::mat4x4& view_projection_matrix)
	{
		//Calculate frustum planes
		glm::mat4x4 frustum_matrix = glm::transpose(view_projection_matrix);

		planes[Planes::Left] = frustum_matrix[3] + frustum_matrix[0];
		planes[Planes::Right] = frustum_matrix[3] - frustum_matrix[0];
		planes[Planes::Bottom] = frustum_matrix[3] + frustum_matrix[1];
		planes[Planes::Top] = frustum_matrix[3] - frustum_matrix[1];
		planes[Planes::Near] = frustum_matrix[3] + frustum_matrix[2];
		planes[Planes::Far] = frustum_matrix[3] - frustum_matrix[2];

		//Calculate points
		glm::vec3 crosses[Combinations] = {
			glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Right])),
			glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Bottom])),
			glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Top])),
			glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Near])),
			glm::cross(glm::vec3(planes[Left]),   glm::vec3(planes[Far])),
			glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Bottom])),
			glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Top])),
			glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Near])),
			glm::cross(glm::vec3(planes[Right]),  glm::vec3(planes[Far])),
			glm::cross(glm::vec3(planes[Bottom]), glm::vec3(planes[Top])),
			glm::cross(glm::vec3(planes[Bottom]), glm::vec3(planes[Near])),
			glm::cross(glm::vec3(planes[Bottom]), glm::vec3(planes[Far])),
			glm::cross(glm::vec3(planes[Top]),    glm::vec3(planes[Near])),
			glm::cross(glm::vec3(planes[Top]),    glm::vec3(planes[Far])),
			glm::cross(glm::vec3(planes[Near]),   glm::vec3(planes[Far]))
		};

		points[0] = intersection<Left, Bottom, Near>(planes, crosses);
		points[1] = intersection<Left, Top, Near>(planes, crosses);
		points[2] = intersection<Right, Bottom, Near>(planes, crosses);
		points[3] = intersection<Right, Top, Near>(planes, crosses);
		points[4] = intersection<Left, Bottom, Far>(planes, crosses);
		points[5] = intersection<Left, Top, Far>(planes, crosses);
		points[6] = intersection<Right, Bottom, Far>(planes, crosses);
		points[7] = intersection<Right, Top, Far>(planes, crosses);
	}
}
