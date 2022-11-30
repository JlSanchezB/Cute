#include "box_city_car_control.h"
#include <core/control_variables.h>
#include <core/platform.h>

//List of control variables

//Pitch input
CONTROL_VARIABLE(float, c_car_pitch_range, 0.f, 1.f, 0.9f, "Car", "Pitch Range");
CONTROL_VARIABLE(float, c_car_pitch_factor, 0.f, 1.f, 0.0025f, "Car", "Pitch Factor");
CONTROL_VARIABLE_BOOL(c_car_inverse_pitch, false, "Car", "Inverse Pitch");

//Roll input
CONTROL_VARIABLE(float, c_car_roll_range, 0.f, 1.f, 0.9f, "Car", "Roll Range");
CONTROL_VARIABLE(float, c_car_roll_factor, 0.f, 1.f, 0.0025f, "Car", "Roll Factor");
CONTROL_VARIABLE(float, c_car_roll_absorber, 0.f, 1.f, 0.05f, "Car", "Roll Absorber");

//Foward input
CONTROL_VARIABLE(float, c_car_foward_factor, 0.f, 1.f, 0.00025f, "Car", "Foward factor");

//Pitch control
CONTROL_VARIABLE(float, c_car_pitch_force, 0.f, 1.f, 0.0001f, "Car", "Pitch Force");

//Roll control
CONTROL_VARIABLE(float, c_car_roll_force, 0.f, 1.f, 0.0001f, "Car", "Roll Force");

//Forward
CONTROL_VARIABLE(float, c_car_foward_force, 0.f, 1.f, 0.0001f, "Car", "Foward Force");

//Friction
CONTROL_VARIABLE(float, c_car_friction_force, 0.f, 1.f, 0.1f, "Car", "Friction Force");


CONTROL_VARIABLE(float, c_car_camera_distance, 0.f, 100.f, 20.f, "Car", "Camera Distance");
CONTROL_VARIABLE(float, c_car_camera_up_offset, 0.f, 100.f, 1.f, "Car", "Camera Up Offset");
CONTROL_VARIABLE(float, c_car_camera_fov, 60.f, 180.f, 90.f, "Car", "Camera Fov");

namespace BoxCityCarControl
{
	void CarCamera::Update(platform::Game* game, Car& car, float ellapsed_time)
	{
		glm::vec3 camera_offset = glm::vec3(0.f, -c_car_camera_distance, c_car_camera_up_offset) * glm::toMat3(car.rotation);
		m_position = car.position + camera_offset;
		m_target = car.position;
		m_fov_y = glm::radians(c_car_camera_fov);

		UpdateInternalData();
	}

	void UpdatePlayerControl(platform::Game* game, CarControl& car_control, float ellapsed_time)
	{
		//Update pitch from the input
		car_control.pitch_target += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * c_car_pitch_factor * ((c_car_inverse_pitch) ? -1.f : 1.f) * ellapsed_time;
		car_control.pitch_target = glm::clamp(car_control.pitch_target, -c_car_pitch_range, c_car_pitch_range);

		//Apply absorber in the roll
		if (car_control.roll_target > 0.f)
		{
			car_control.roll_target -= c_car_roll_absorber * ellapsed_time;
			if (car_control.roll_target < 0.f) car_control.roll_target = 0.f;
		}
		else
		{
			car_control.roll_target += c_car_roll_absorber * ellapsed_time;
			if (car_control.roll_target > 0.f) car_control.roll_target = 0.f;
		}

		//Update roll from the input
		car_control.roll_target += game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * c_car_roll_factor * ellapsed_time;
		car_control.roll_target = glm::clamp(car_control.roll_target, -c_car_roll_range, c_car_roll_range);

		//Update forward using wheel control
		for (auto& input_event : game->GetInputEvents())
		{
			if (input_event.type == platform::EventType::MouseWheel)
			{
				car_control.foward += c_car_foward_factor * input_event.value * ellapsed_time;
			}
		}
		car_control.foward = glm::clamp(car_control.foward, 0.f, 1.f);
	}

	void CalculateForcesAndIntegrateCar(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float ellapsed_time)
	{
		glm::vec3 linear_forces (0.f, 0.f, 0.f);
		glm::vec3 angular_forces(0.f, 0.f, 0.f);

		glm::mat3x3 car_matrix = glm::mat3_cast(car.rotation);

		float car_yaw, car_pitch, car_roll;
		glm::extractEulerAngleXYZ(glm::mat4x4(car_matrix), car_yaw, car_pitch, car_roll);
		
		glm::vec3 pitch_vector = car_matrix[0];
		glm::vec3 roll_vector = car_matrix[1];
		glm::vec3 up_vector = car_matrix[2];

		//Apply pitch target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.pitch_target * glm::half_pi<float>();
			float diff_angle = target - car_pitch;

			//Generate pitch forces
			angular_forces.y += diff_angle * c_car_pitch_force * ellapsed_time;
		}
		//Apply roll target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.roll_target * glm::half_pi<float>();
			float diff_angle = target - car_roll;

			//Generate roll forces
			angular_forces += diff_angle * c_car_roll_force * ellapsed_time;
		}
		//Apply forward, it will allow to go up/down and left/right when rolling
		{
			linear_forces += car_control.foward * c_car_foward_force * ellapsed_time * roll_vector;
		}

		//Apply friction
		{
			linear_forces -= car_movement.lineal_velocity * glm::clamp(c_car_friction_force * ellapsed_time, 0.f, 1.f);

			float car_velocity_yaw, car_velocity_pitch, car_velocity_roll;
			glm::extractEulerAngleXYZ(glm::mat4x4(car_movement.rotation_velocity), car_velocity_yaw, car_velocity_pitch, car_velocity_roll);

			angular_forces -= glm::vec3(car_velocity_yaw, car_velocity_pitch, car_velocity_roll) * glm::clamp(c_car_friction_force * ellapsed_time, 0.f, 1.f);
		}


		//Integrate velocity
		car_movement.lineal_velocity +=  linear_forces * car_settings.inv_mass * ellapsed_time;

		//Calculate world inertia mass
		glm::mat3x3 inv_mass_intertia_matrix = glm::scale(car_settings.inv_mass_inertia);

		glm::mat3x3 world_inv_mass_inertial = glm::toMat3(car.rotation) * inv_mass_intertia_matrix * glm::toMat3(glm::inverse(car.rotation));

		car_movement.rotation_velocity *= glm::quat(angular_forces * world_inv_mass_inertial);

		//Integrate position
		car.position += car_movement.lineal_velocity * ellapsed_time;
		car.rotation += car_movement.rotation_velocity * ellapsed_time;
	}
}