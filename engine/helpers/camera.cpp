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
		//Read inputs
		float forward_input = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftY);
		if (game->GetInputSlotState(platform::InputSlotState::Up) || game->GetInputSlotState(platform::InputSlotState::Key_W)) forward_input += 1.f;
		if (game->GetInputSlotState(platform::InputSlotState::Down) || game->GetInputSlotState(platform::InputSlotState::Key_S)) forward_input -= 1.f;
		float side_input = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbLeftX);
		if (game->GetInputSlotState(platform::InputSlotState::Right) || game->GetInputSlotState(platform::InputSlotState::Key_D)) side_input += 1.f;
		if (game->GetInputSlotState(platform::InputSlotState::Left) || game->GetInputSlotState(platform::InputSlotState::Key_A)) side_input -= 1.f;
		float up_input = (game->GetInputSlotValue(platform::InputSlotValue::ControllerRightTrigger) - game->GetInputSlotValue(platform::InputSlotValue::ControllerLeftTrigger));
		if (game->GetInputSlotState(platform::InputSlotState::PageUp) || game->GetInputSlotState(platform::InputSlotState::Key_Z)) up_input += 1.f;
		if (game->GetInputSlotState(platform::InputSlotState::PageDown) || game->GetInputSlotState(platform::InputSlotState::Key_X)) up_input -= 1.f;

		if (game->GetInputSlotState(platform::InputSlotState::RightMouseButton))
		{
			side_input += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * m_mouse_move_factor;
			forward_input += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * m_mouse_move_factor;
		}

		forward_input = glm::clamp(forward_input, -1.f, 1.f);
		side_input = glm::clamp(side_input, -1.f, 1.f);
		up_input = glm::clamp(up_input, -1.f, 1.f);

		glm::vec2 rotation_input;
		rotation_input.x = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightX);
		rotation_input.y = game->GetInputSlotValue(platform::InputSlotValue::ControllerThumbRightY);
		if (game->GetInputSlotState(platform::InputSlotState::LeftMouseButton))
		{
			rotation_input.x += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * m_mouse_rotate_factor;
			rotation_input.y -= game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * m_mouse_rotate_factor;
		}
		rotation_input.x = glm::clamp(rotation_input.x, -1.f, 1.f);
		rotation_input.y = glm::clamp(rotation_input.y, -1.f, 1.f);

		//Apply damp
		m_move_speed -= m_move_speed * glm::clamp((m_damp_factor * ellapsed_time), 0.f, 1.f);
		m_rotation_speed -= m_rotation_speed * glm::clamp((m_damp_factor * ellapsed_time), 0.f, 1.f);

		//Calculate position movement
		glm::vec3 move_speed;

		glm::mat3x3 rot = glm::rotate(m_rotation.y, glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(m_rotation.x, glm::vec3(0.f, 0.f, 1.f));
		//Define forward and side
		glm::vec3 forward = glm::vec3(0.f, 1.f, 0.f) * rot;
		glm::vec3 side = glm::vec3(1.f, 0.f, 0.f) * rot;

		move_speed = -side_input * m_move_factor * ellapsed_time * side;
		move_speed += forward_input * m_move_factor * ellapsed_time * forward;
		//Up/Down doesn't depend of the rotation
		move_speed.z += up_input * m_move_factor * ellapsed_time;


		m_move_speed += move_speed;

		//Calculate direction movement
		glm::vec2 angles;
		angles.x = -rotation_input.x * m_rotation_factor * ellapsed_time;
		angles.y = -rotation_input.y * m_rotation_factor * ellapsed_time;

		m_rotation_speed += angles;

		//Apply
		m_position += m_move_speed * ellapsed_time;
		m_rotation += m_rotation_speed * ellapsed_time;

		if (m_rotation.y > glm::radians(85.f)) m_rotation.y = glm::radians(85.f);
		if (m_rotation.y < glm::radians(-85.f)) m_rotation.y = glm::radians(-85.f);

		UpdateInternalData();
	}

	void Camera::UpdateInternalData()
	{
		//Calculate view to world
		glm::mat3x3 rot = glm::rotate(m_rotation.y, glm::vec3(1.f, 0.f, 0.f)) * glm::rotate(m_rotation.x, glm::vec3(0.f, 0.f, 1.f));
		glm::mat4x4 world_to_view_matrix = glm::lookAt(m_position, m_position + glm::vec3(0.f, 1.f, 0.f) * rot, m_up_vector);

		//Calculate projection matrix
		glm::mat4x4 projection_matrix = glm::perspective(m_fov_y, m_aspect_ratio, m_near, m_far);

		UpdateFrustum(world_to_view_matrix, projection_matrix);
	}
	
	void Frustum::UpdateFrustum(const glm::mat4x4& world_to_view_matrix, const glm::mat4x4& projection_matrix)
	{
		m_world_to_view_matrix = world_to_view_matrix;
		m_projection_matrix = projection_matrix;

		//Calculate view projection
		m_view_projection_matrix = m_projection_matrix * m_world_to_view_matrix;

		//Calculate frustum planes
		glm::mat4x4 frustum_matrix = glm::transpose(m_view_projection_matrix);

		m_planes[Planes::Left] = frustum_matrix[3] + frustum_matrix[0];
		m_planes[Planes::Right] = frustum_matrix[3] - frustum_matrix[0];
		m_planes[Planes::Bottom] = frustum_matrix[3] + frustum_matrix[1];
		m_planes[Planes::Top] = frustum_matrix[3] - frustum_matrix[1];
		m_planes[Planes::Near] = frustum_matrix[3] + frustum_matrix[2];
		m_planes[Planes::Far] = frustum_matrix[3] - frustum_matrix[2];

		//Calculate points
		glm::vec3 crosses[Combinations] = {
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Right])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Bottom])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Top])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Left]),   glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Bottom])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Top])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Right]),  glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Bottom]), glm::vec3(m_planes[Top])),
			glm::cross(glm::vec3(m_planes[Bottom]), glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Bottom]), glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Top]),    glm::vec3(m_planes[Near])),
			glm::cross(glm::vec3(m_planes[Top]),    glm::vec3(m_planes[Far])),
			glm::cross(glm::vec3(m_planes[Near]),   glm::vec3(m_planes[Far]))
		};

		m_points[0] = intersection<Left, Bottom, Near>(m_planes, crosses);
		m_points[1] = intersection<Left, Top, Near>(m_planes, crosses);
		m_points[2] = intersection<Right, Bottom, Near>(m_planes, crosses);
		m_points[3] = intersection<Right, Top, Near>(m_planes, crosses);
		m_points[4] = intersection<Left, Bottom, Far>(m_planes, crosses);
		m_points[5] = intersection<Left, Top, Far>(m_planes, crosses);
		m_points[6] = intersection<Right, Bottom, Far>(m_planes, crosses);
		m_points[7] = intersection<Right, Top, Far>(m_planes, crosses);
	}

	glm::mat4x4 Frustum::GetViewProjectionMatrix() const
	{
		return m_view_projection_matrix;
	}	
}
