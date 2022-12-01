#include "box_city_car_control.h"
#include <core/control_variables.h>
#include <core/platform.h>

#pragma optimize( "", off )
//List of control variables

//Pitch input
CONTROL_VARIABLE(float, c_car_pitch_range, 0.f, 1.f, 0.5f, "Car", "Pitch Range");
CONTROL_VARIABLE(float, c_car_pitch_mouse_factor, 0.f, 10.f, 0.0f, "Car", "Pitch Mouse Factor");
CONTROL_VARIABLE(float, c_car_pitch_keyboard_factor, 0.f, 10.f, 1.0f, "Car", "Pitch Keyboard Factor");
CONTROL_VARIABLE_BOOL(c_car_inverse_pitch, false, "Car", "Inverse Pitch");

//Roll input
CONTROL_VARIABLE(float, c_car_roll_range, 0.f, 1.f, 0.5f, "Car", "Roll Range");
CONTROL_VARIABLE(float, c_car_roll_mouse_factor, 0.f, 10.f, 0.0f, "Car", "Roll Mouse Factor");
CONTROL_VARIABLE(float, c_car_roll_keyboard_factor, 0.f, 10.f, 1.f, "Car", "Roll Keyboard Factor");
CONTROL_VARIABLE(float, c_car_roll_absorber, 0.f, 1.f, 0.15f, "Car", "Roll Absorber");

//Foward input
CONTROL_VARIABLE(float, c_car_foward_mouse_factor, 0.f, 10.f, 2.25f, "Car", "Foward Mouse Factor");
CONTROL_VARIABLE(float, c_car_foward_keyboard_factor, 0.f, 10.f, 1.25f, "Car", "Foward Keybard Factor");

//Pitch control
CONTROL_VARIABLE(float, c_car_pitch_force, 0.f, 10.f, 0.02f, "Car", "Pitch Force");
CONTROL_VARIABLE(float, c_car_pitch_max_force, 0.f, 10.f, 0.01f, "Car", "Pitch Max Force");

//Roll control
CONTROL_VARIABLE(float, c_car_roll_force, 0.f, 10.f, 0.02f, "Car", "Roll Force");
CONTROL_VARIABLE(float, c_car_roll_max_force, 0.f, 10.f, 0.01f, "Car", "Roll MaxForce");

//Forward
CONTROL_VARIABLE(float, c_car_foward_force, 0.f, 10000.f, 100.0f, "Car", "Foward Force");

//Friction
CONTROL_VARIABLE(float, c_car_friction_linear_force, 0.f, 10.f, 0.8f, "Car", "Linear Friction Force");
CONTROL_VARIABLE(float, c_car_friction_angular_force, 0.f, 10.f, 0.8f, "Car", "Angular Friction Force");


CONTROL_VARIABLE(float, c_car_camera_distance, 0.f, 100.f, 20.f, "Car", "Camera Distance");
CONTROL_VARIABLE(float, c_car_camera_up_offset, 0.f, 100.f, 2.f, "Car", "Camera Up Offset");
CONTROL_VARIABLE(float, c_car_camera_fov, 60.f, 180.f, 90.f, "Car", "Camera Fov");

namespace BoxCityCarControl
{
	void CarCamera::Update(platform::Game* game, Car& car, float elapsed_time)
	{
		glm::mat3x3 car_matrix = glm::toMat3(car.rotation);
		m_position = car.position - glm::row(car_matrix, 1) * c_car_camera_distance + glm::vec3(0.f, 0.f, c_car_camera_up_offset);
		m_target = car.position;
		m_fov_y = glm::radians(c_car_camera_fov);

		UpdateInternalData();
	}

	void UpdatePlayerControl(platform::Game* game, CarControl& car_control, float elapsed_time)
	{
		if (game->IsFocus())
		{
			//Update pitch from the input
			float pitch_offset = game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * c_car_pitch_mouse_factor;
			pitch_offset += (game->GetInputSlotState(platform::InputSlotState::Key_Q)) ? c_car_pitch_keyboard_factor : 0.f;
			pitch_offset -= (game->GetInputSlotState(platform::InputSlotState::Key_E)) ? c_car_pitch_keyboard_factor : 0.f;
			car_control.pitch_target += pitch_offset * ((c_car_inverse_pitch) ? -1.f : 1.f) * elapsed_time;
			car_control.pitch_target = glm::clamp(car_control.pitch_target, -c_car_pitch_range, c_car_pitch_range);

			//Apply absorber in the roll
			if (car_control.roll_target > 0.f)
			{
				car_control.roll_target -= c_car_roll_absorber * elapsed_time;
				if (car_control.roll_target < 0.f) car_control.roll_target = 0.f;
			}
			else
			{
				car_control.roll_target += c_car_roll_absorber * elapsed_time;
				if (car_control.roll_target > 0.f) car_control.roll_target = 0.f;
			}

			//Update roll from the input
			float roll_offset = game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * c_car_roll_mouse_factor;
			roll_offset += (game->GetInputSlotState(platform::InputSlotState::Key_D)) ? c_car_roll_keyboard_factor : 0.f;
			roll_offset -= (game->GetInputSlotState(platform::InputSlotState::Key_A)) ? c_car_roll_keyboard_factor : 0.f;
			car_control.roll_target += roll_offset * elapsed_time;
			car_control.roll_target = glm::clamp(car_control.roll_target, -c_car_roll_range, c_car_roll_range);

			//Update forward using wheel control
			float foward_offset = 0.f;
			for (auto& input_event : game->GetInputEvents())
			{
				if (input_event.type == platform::EventType::MouseWheel)
				{
					foward_offset += c_car_foward_mouse_factor * input_event.value;
				}
			}
			foward_offset += (game->GetInputSlotState(platform::InputSlotState::Key_W)) ? c_car_foward_keyboard_factor : 0.f;
			foward_offset -= (game->GetInputSlotState(platform::InputSlotState::Key_S)) ? c_car_foward_keyboard_factor : 0.f;

			car_control.foward += foward_offset * elapsed_time;
			car_control.foward = glm::clamp(car_control.foward, 0.f, 1.f);
		}
	}

	void CalculateForcesAndIntegrateCar(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float elapsed_time)
	{
		glm::vec3 linear_forces (0.f, 0.f, 0.f);
		glm::vec3 angular_forces(0.f, 0.f, 0.f);

		glm::mat3x3 car_matrix = glm::toMat3(car.rotation);

		glm::vec3 car_left_vector = glm::row(car_matrix, 0);
		glm::vec3 car_front_vector = glm::row(car_matrix, 1);
		glm::vec3 up_vector(0.f, 0.f, 1.f);

		//Apply pitch target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.pitch_target * glm::half_pi<float>();
			float diff_angle = target - (glm::angle(car_front_vector, up_vector) - glm::half_pi<float>());

			//Convert it into angular force
			angular_forces += glm::row(car_matrix, 0) * glm::max(-c_car_pitch_max_force, glm::min(c_car_pitch_max_force, diff_angle * c_car_pitch_force));
		}
		//Apply roll target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.roll_target * glm::half_pi<float>();
			float diff_angle = target - (glm::angle(car_left_vector, -up_vector) - glm::half_pi<float>());

			//Generate roll forces
			angular_forces += glm::row(car_matrix, 1) * glm::max(-c_car_roll_max_force, glm::min(c_car_roll_max_force, diff_angle * c_car_roll_force));
		}
		//Apply forward, it will allow to go up/down and left/right when rolling
		{
			linear_forces += car_control.foward * c_car_foward_force * car_front_vector;
		}

		//Apply friction
		{
			linear_forces -= car_movement.lineal_velocity * glm::clamp(c_car_friction_linear_force * elapsed_time, 0.f, 1.f) / elapsed_time;
			angular_forces -= car_movement.rotation_velocity * glm::clamp(c_car_friction_angular_force * elapsed_time, 0.f, 1.f) / elapsed_time;
		}


		//Integrate velocity
		car_movement.lineal_velocity +=  linear_forces * car_settings.inv_mass * elapsed_time;

		//Calculate world inertia mass
		glm::mat3x3 inv_mass_intertia_matrix = glm::scale(car_settings.inv_mass_inertia);

		glm::mat3x3 world_inv_mass_inertial = inv_mass_intertia_matrix * glm::toMat3(car.rotation);
		car_movement.rotation_velocity += angular_forces * elapsed_time * inv_mass_intertia_matrix;

		//Integrate position
		car.position += car_movement.lineal_velocity * elapsed_time;
		float rotation_angle = glm::length(car_movement.rotation_velocity * elapsed_time);
		if (rotation_angle > 0.000001f)
		{
			car.rotation = glm::normalize(car.rotation * glm::angleAxis(rotation_angle, car_movement.rotation_velocity / rotation_angle));
		}
	}
}

#pragma optimize( "", on )