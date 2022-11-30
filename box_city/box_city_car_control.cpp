#include "box_city_car_control.h"
#include <core/control_variables.h>
#include <core/platform.h>

#pragma optimize( "", off )
//List of control variables

//Pitch input
CONTROL_VARIABLE(float, c_car_pitch_range, 0.f, 1.f, 0.5f, "Car", "Pitch Range");
CONTROL_VARIABLE(float, c_car_pitch_factor, 0.f, 10.f, 0.25f, "Car", "Pitch Factor");
CONTROL_VARIABLE_BOOL(c_car_inverse_pitch, false, "Car", "Inverse Pitch");

//Roll input
CONTROL_VARIABLE(float, c_car_roll_range, 0.f, 1.f, 0.5f, "Car", "Roll Range");
CONTROL_VARIABLE(float, c_car_roll_factor, 0.f, 10.f, 0.25f, "Car", "Roll Factor");
CONTROL_VARIABLE(float, c_car_roll_absorber, 0.f, 1.f, 0.05f, "Car", "Roll Absorber");

//Foward input
CONTROL_VARIABLE(float, c_car_foward_factor, 0.f, 10.f, 2.25f, "Car", "Foward factor");

//Pitch control
CONTROL_VARIABLE(float, c_car_pitch_force, 0.f, 10.f, 0.5f, "Car", "Pitch Force");

//Roll control
CONTROL_VARIABLE(float, c_car_roll_force, 0.f, 10.f, 0.5f, "Car", "Roll Force");

//Forward
CONTROL_VARIABLE(float, c_car_foward_force, 0.f, 10000.f, 100.0f, "Car", "Foward Force");

//Friction
CONTROL_VARIABLE(float, c_car_friction_linear_force, 0.f, 1.f, 0.2f, "Car", "Linear Friction Force");
CONTROL_VARIABLE(float, c_car_friction_angular_force, 0.f, 1.f, 0.8f, "Car", "Angular Friction Force");


CONTROL_VARIABLE(float, c_car_camera_distance, 0.f, 100.f, 20.f, "Car", "Camera Distance");
CONTROL_VARIABLE(float, c_car_camera_up_offset, 0.f, 100.f, 1.f, "Car", "Camera Up Offset");
CONTROL_VARIABLE(float, c_car_camera_fov, 60.f, 180.f, 90.f, "Car", "Camera Fov");

namespace BoxCityCarControl
{
	void CarCamera::Update(platform::Game* game, Car& car, float elapsed_time)
	{
		glm::vec3 camera_offset = glm::vec3(0.f, -c_car_camera_distance, c_car_camera_up_offset) * glm::toMat3(car.rotation);
		m_position = car.position + camera_offset;
		m_target = car.position;
		m_fov_y = glm::radians(c_car_camera_fov);

		UpdateInternalData();
	}

	void UpdatePlayerControl(platform::Game* game, CarControl& car_control, float elapsed_time)
	{
		if (game->IsFocus())
		{
			//Update pitch from the input
			car_control.pitch_target += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * c_car_pitch_factor * ((c_car_inverse_pitch) ? -1.f : 1.f) * elapsed_time;
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
			car_control.roll_target += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * c_car_roll_factor * elapsed_time;
			car_control.roll_target = glm::clamp(car_control.roll_target, -c_car_roll_range, c_car_roll_range);

			//Update forward using wheel control
			for (auto& input_event : game->GetInputEvents())
			{
				if (input_event.type == platform::EventType::MouseWheel)
				{
					car_control.foward += c_car_foward_factor * input_event.value * elapsed_time;
				}
			}
			car_control.foward = glm::clamp(car_control.foward, 0.f, 1.f);
		}
	}

	void CalculateForcesAndIntegrateCar(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float elapsed_time)
	{
		glm::vec3 linear_forces (0.f, 0.f, 0.f);
		glm::vec3 angular_forces(0.f, 0.f, 0.f);

		glm::mat3x3 car_matrix = glm::toMat3(car.rotation);

		float car_pitch, car_roll, car_yaw;
		glm::extractEulerAngleXYZ(glm::mat4x4(car_matrix), car_pitch, car_roll, car_yaw);
		
		glm::vec3 front_vector = car_matrix[1];

		//Apply pitch target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.pitch_target * glm::half_pi<float>();
			float diff_angle = target - car_pitch;

			//Generate pitch forces
			angular_forces.x += diff_angle * c_car_pitch_force * elapsed_time;
		}
		//Apply roll target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.roll_target * glm::half_pi<float>();
			float diff_angle = target - car_roll;

			//Generate roll forces
			angular_forces.y += diff_angle * c_car_roll_force * elapsed_time;
		}
		//Apply forward, it will allow to go up/down and left/right when rolling
		{
			linear_forces += car_control.foward * c_car_foward_force * elapsed_time * front_vector;
		}

		//Apply friction
		{
			linear_forces -= car_movement.lineal_velocity * glm::clamp(c_car_friction_linear_force * elapsed_time, 0.f, 1.f);

			float car_velocity_pitch, car_velocity_roll, car_velocity_yaw;
			glm::extractEulerAngleXYZ(glm::mat4x4(car_movement.rotation_velocity), car_velocity_pitch, car_velocity_roll, car_velocity_yaw);

			angular_forces -= glm::vec3(car_velocity_pitch, car_velocity_roll, car_velocity_yaw) * glm::clamp(c_car_friction_angular_force * elapsed_time, 0.f, 1.f);
		}


		//Integrate velocity
		car_movement.lineal_velocity +=  linear_forces * car_settings.inv_mass;

		//Calculate world inertia mass
		glm::mat3x3 inv_mass_intertia_matrix = glm::scale(car_settings.inv_mass_inertia);

		glm::mat3x3 world_inv_mass_inertial = inv_mass_intertia_matrix * glm::toMat3(car.rotation);
		glm::quat rotation_force = glm::quat(angular_forces * world_inv_mass_inertial);
		car_movement.rotation_velocity *= rotation_force;

		//Integrate position
		car.position += car_movement.lineal_velocity * elapsed_time;
		float velocity_pitch, velocity_roll, velocity_yaw;
		glm::extractEulerAngleXYZ(glm::mat4x4(car_movement.rotation_velocity), velocity_pitch, velocity_roll, velocity_yaw);
		car.rotation *= glm::quat(glm::vec3(velocity_pitch, velocity_roll, velocity_yaw) * elapsed_time);
	}
}

#pragma optimize( "", on )