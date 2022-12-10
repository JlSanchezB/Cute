#include "box_city_car_control.h"
#include <core/control_variables.h>
#include <core/platform.h>
#include "box_city_tile_manager.h"

//List of control variables

//Pitch input
CONTROL_VARIABLE(float, c_car_Y_range, 0.f, 1.f, 0.7f, "Car", "Y Range");
CONTROL_VARIABLE(float, c_car_Y_mouse_factor, 0.f, 10.f, 0.2f, "Car", "Y Mouse Factor");
CONTROL_VARIABLE(float, c_car_Y_keyboard_factor, 0.f, 10.f, 2.0f, "Car", "Y Keyboard Factor");
CONTROL_VARIABLE_BOOL(c_car_inverse_Y, false, "Car", "Y Inverse");
CONTROL_VARIABLE(float, c_car_Y_absorber, 0.f, 1.f, 0.15f, "Car", "Y Absorber");

//Roll input
CONTROL_VARIABLE(float, c_car_X_range, 0.f, 1.f, 0.5f, "Car", "X Range");
CONTROL_VARIABLE(float, c_car_X_mouse_factor, 0.f, 10.f, 0.2f, "Car", "X Mouse Factor");
CONTROL_VARIABLE(float, c_car_X_keyboard_factor, 0.f, 10.f, 2.f, "Car", "X Keyboard Factor");
CONTROL_VARIABLE(float, c_car_X_absorber, 0.f, 1.f, 0.15f, "Car", "X Absorber");

//Foward input
CONTROL_VARIABLE(float, c_car_foward_mouse_factor, 0.f, 10.f, 2.25f, "Car", "Foward Mouse Factor");
CONTROL_VARIABLE(float, c_car_foward_keyboard_factor, 0.f, 10.f, 1.25f, "Car", "Foward Keybard Factor");

//Pitch control
CONTROL_VARIABLE(float, c_car_Y_pitch_force, 0.f, 10.f, 0.02f, "Car", "Y Pitch Force");
CONTROL_VARIABLE(float, c_car_Y_pitch_linear_force, 0.f, 10.f, 0.01f, "Car", "Y Pitch Linear Force");

//Roll control
CONTROL_VARIABLE(float, c_car_X_roll_angular_force, 0.f, 10.f, 0.02f, "Car", "X Roll Angular Force");
CONTROL_VARIABLE(float, c_car_X_jaw_angular_force, 0.f, 10.f, 0.05f, "Car", "X Jaw Angular Force");
CONTROL_VARIABLE(float, c_car_X_linear_force, 0.f, 10.f, 0.01f, "Car", "X Linear Force");

//Forward
CONTROL_VARIABLE(float, c_car_foward_force, 0.f, 10000.f, 300.0f, "Car", "Foward Force");
CONTROL_VARIABLE(float, c_car_foward_kill_height_force, 0.f, 100.f, 2.0f, "Car", "Foward Kill Heigth Force");

//Friction
CONTROL_VARIABLE(float, c_car_friction_linear_force, 0.f, 10.f, 1.8f, "Car", "Linear Friction Force");
CONTROL_VARIABLE(float, c_car_friction_angular_force, 0.f, 10.f, 1.8f, "Car", "Angular Friction Force");


CONTROL_VARIABLE(float, c_car_camera_distance, 0.f, 100.f, 4.5f, "Car", "Camera Distance");
CONTROL_VARIABLE(float, c_car_camera_up_offset, 0.f, 100.f, 1.f, "Car", "Camera Up Offset");
CONTROL_VARIABLE(float, c_car_camera_fov, 60.f, 180.f, 100.f, "Car", "Camera Fov");

CONTROL_VARIABLE(float, c_car_ai_forward, 0.f, 1.f, 0.8f, "Car", "Camera AI foward");
CONTROL_VARIABLE(float, c_car_ai_min_forward, 0.f, 1.f, 0.3f, "Car", "Camera AI min foward");
CONTROL_VARIABLE(float, c_car_ai_target_speed, 0.f, 1.f, 5.f, "Car", "Camera AI target speed");
CONTROL_VARIABLE(float, c_car_ai_avoidance_calculation_distance, 0.f, 10000.f, 1000.f, "Car", "Camera AI avoidance calculation distance");
CONTROL_VARIABLE(float, c_car_ai_visibility_distance, 0.f, 10.f, 100.f, "Car", "Camera AI visibility distance");
CONTROL_VARIABLE(float, c_car_ai_avoidance_extra_distance, 0.f, 1000.f, 20.f, "Car", "Camera AI avoidance extra distance with building");
CONTROL_VARIABLE(float, c_car_ai_avoidance_distance_expansion, 0.f, 1000.f, 50.f, "Car", "Camera AI avoidance extra expansion apply to buildings when far");
CONTROL_VARIABLE(float, c_car_ai_avoidance_slow_factor, 0.f, 1.f, 0.3f, "Car", "Car AI avoidance slow factor");
CONTROL_VARIABLE(float, c_car_ai_target_range, 1.f, 10000.f, 2000.f, "Car", "Car AI target range");
CONTROL_VARIABLE(float, c_car_ai_min_target_range, 1.f, 10000.f, 500.f, "Car", "Car AI min target range");
CONTROL_VARIABLE(float, c_car_ai_min_target_distance, 1.f, 10000.f, 100.f, "Car", "Car AI min target distance");
namespace BoxCityCarControl
{
	static bool NeedsUpdate(uint32_t instance_index, uint32_t frame_index, uint32_t frame_rate)
	{
		return ((frame_index + instance_index) % frame_rate) == 0;
	}

	static bool NeedsUpdate(uint32_t instance_index, uint32_t frame_index, uint32_t max_frame_rate, float min_range, float max_range, float factor)
	{
		float t = glm::clamp((factor - min_range) / (max_range - min_range), 0.f, 1.f);
		uint32_t frame_rate = static_cast<uint32_t>(t * max_frame_rate);
		frame_rate = glm::clamp<uint32_t>(frame_rate, 1, max_frame_rate);
		return NeedsUpdate(instance_index, frame_index, frame_rate);
	}

	void CarCamera::Update(platform::Game* game, Car& car, float elapsed_time)
	{
		glm::mat3x3 car_matrix = glm::toMat3(*car.rotation);
		*m_position = *car.position - glm::row(car_matrix, 1) * c_car_camera_distance + glm::vec3(0.f, 0.f, c_car_camera_up_offset);
		*m_target = *car.position;
		m_fov_y = glm::radians(c_car_camera_fov);
	}

	void UpdatePlayerControl(platform::Game* game, CarControl& car_control, float elapsed_time)
	{
		if (game->IsFocus())
		{
			//Apply absorber in the pitch
			if (car_control.Y_target > 0.f)
			{
				car_control.Y_target -= c_car_Y_absorber * elapsed_time;
				if (car_control.Y_target < 0.f) car_control.Y_target = 0.f;
			}
			else
			{
				car_control.Y_target += c_car_Y_absorber * elapsed_time;
				if (car_control.Y_target > 0.f) car_control.Y_target = 0.f;
			}

			//Update pitch from the input
			float pitch_offset = game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionY) * c_car_Y_mouse_factor;
			pitch_offset += (game->GetInputSlotState(platform::InputSlotState::Key_Q)) ? c_car_Y_keyboard_factor : 0.f;
			pitch_offset -= (game->GetInputSlotState(platform::InputSlotState::Key_E)) ? c_car_Y_keyboard_factor : 0.f;
			car_control.Y_target += pitch_offset * ((c_car_inverse_Y) ? -1.f : 1.f) * elapsed_time;
			car_control.Y_target = glm::clamp(car_control.Y_target, -c_car_Y_range, c_car_Y_range);

			//Apply absorber in the roll
			if (car_control.X_target > 0.f)
			{
				car_control.X_target -= c_car_X_absorber * elapsed_time;
				if (car_control.X_target < 0.f) car_control.X_target = 0.f;
			}
			else
			{
				car_control.X_target += c_car_X_absorber * elapsed_time;
				if (car_control.X_target > 0.f) car_control.X_target = 0.f;
			}

			//Update roll from the input
			float roll_offset = game->GetInputSlotValue(platform::InputSlotValue::MouseRelativePositionX) * c_car_X_mouse_factor;
			roll_offset += (game->GetInputSlotState(platform::InputSlotState::Key_D)) ? c_car_X_keyboard_factor : 0.f;
			roll_offset -= (game->GetInputSlotState(platform::InputSlotState::Key_A)) ? c_car_X_keyboard_factor : 0.f;
			car_control.X_target += roll_offset * elapsed_time;
			car_control.X_target = glm::clamp(car_control.X_target, -c_car_X_range, c_car_X_range);

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

	void SetupCarTarget(std::mt19937& random, const Car& car, CarTarget& car_target)
	{
		//Calculate a new position as target
		std::uniform_real_distribution<float> position_range(-c_car_ai_target_range + c_car_ai_min_target_range, c_car_ai_target_range - c_car_ai_min_target_range);
		std::uniform_real_distribution<float> position_range_z(BoxCityTileSystem::kTileHeightBottom, BoxCityTileSystem::kTileHeightTop);

		float range_x = position_range(random);
		float range_y = position_range(random);

		//Add the min target distance
		range_x += glm::sign(range_x) * c_car_ai_min_target_range;
		range_y += glm::sign(range_x) * c_car_ai_min_target_range;

		car_target.target = glm::vec3((*car.position).x + range_x, (*car.position).y + range_y, position_range_z(random));

	}

	void UpdateAIControl(std::mt19937& random, uint32_t instance_index, CarControl& car_control, const Car& car, const CarMovement& car_movement, const CarSettings& car_settings, CarTarget& car_target, uint32_t frame_index, float elapsed_time, BoxCityTileSystem::Manager* manager, const glm::vec3& camera_pos)
	{
		const glm::vec3 car_position = *car.position;
		float camera_distance = glm::distance(camera_pos, car_position);

		//We timeslice the update by the distance to the camera
		if (!NeedsUpdate(instance_index, frame_index, 8, 500.f, 3000.f, camera_distance)) return;

		const glm::mat3x3 car_matrix = glm::toMat3(*car.rotation);
		const glm::vec3 car_left = glm::row(car_matrix, 0);
		const glm::vec3 car_front = glm::row(car_matrix, 1);
		const glm::vec3 car_top = glm::row(car_matrix, 2);
		const glm::vec3 car_left_flat = glm::normalize(glm::vec3(car_left.x, car_left.y, 0.f));
		const float car_radius = glm::length2(car_settings.size);

		//Calculate X and Y control for the car
		car_control.foward = c_car_ai_forward;

		//Calculate avoidance
		glm::vec2 avoidance_target(0.f, 0.f);

		if (glm::distance2(camera_pos, car_position) < c_car_ai_avoidance_calculation_distance * c_car_ai_avoidance_calculation_distance)
		{
			const glm::vec3 car_direction = glm::normalize(car_movement.lineal_velocity);

			//Calculate visibility AABB
			helpers::AABB car_frustum;
			car_frustum.Add(car_position - car_direction * c_car_ai_visibility_distance * 0.05f);

			car_frustum.Add(car_position + car_left_flat * c_car_ai_visibility_distance + car_direction * c_car_ai_visibility_distance);
			car_frustum.Add(car_position - car_left_flat * c_car_ai_visibility_distance + car_direction * c_car_ai_visibility_distance);
			car_frustum.Add(car_position + glm::vec3(0.f, 0.f, 1.f) * c_car_ai_visibility_distance + car_direction * c_car_ai_visibility_distance);
			car_frustum.Add(car_position - glm::vec3(0.f, 0.f, 1.f) * c_car_ai_visibility_distance + car_direction * c_car_ai_visibility_distance);

			//List all buldings colliding with this visibility AABB
			manager->VisitBuildings(car_frustum, [&](const InstanceReference& building)
				{
					OBBBox& avoid_box = building.Get<GameDatabase>().Get<OBBBox>();
					
					glm::vec3 box_point, car_point;
					float box_t, car_t;

					//Building top and bottom points
					const glm::vec3 building_bottom = avoid_box.position - glm::row(avoid_box.rotation, 2) * (avoid_box.extents.z );
					const glm::vec3 building_top = avoid_box.position + glm::row(avoid_box.rotation, 2) * (avoid_box.extents.z);

					//Avoid buildings in front
					{
						helpers::CalculateClosestPointsInTwoSegments(car_position - car_direction * (c_car_ai_visibility_distance * 0.05f), car_position + car_direction * (c_car_ai_visibility_distance),
							building_bottom, building_top,
							car_point, box_point, car_t, box_t);

						float expansion = car_t * c_car_ai_avoidance_distance_expansion;
						//Calculate distance between the points and check with the wide of the box, that is good for the caps as well
						if (glm::length(car_point - box_point) < (glm::sqrt(glm::pow2(avoid_box.extents.x) + glm::pow2(avoid_box.extents.y)) +  expansion + car_radius + c_car_ai_avoidance_extra_distance))
						{
							//It is going to collide
							//You need to avoid box_point
							glm::vec3 car_avoid_direction = glm::normalize(box_point - car_position);

							float xx = glm::dot(car_avoid_direction, car_left_flat);
							avoidance_target.x += (glm::sign<float>(xx) - xx) * (100.f);
							float yy = car_avoid_direction.z;
							avoidance_target.y += (glm::sign<float>(yy) - yy) * (100.f);

							car_control.foward -= c_car_ai_avoidance_slow_factor * (1.f - car_t);
						}
					}	
				}
			);

			car_control.foward = glm::max(car_control.foward, c_car_ai_min_forward);
		}

		float target_x = avoidance_target.x;
		float target_y = avoidance_target.y;

		//Calculate if it needs retargetting
		if (glm::length2(*car.position - car_target.target) < c_car_ai_min_target_distance * c_car_ai_min_target_distance)
		{
			//Retarget
			SetupCarTarget(random, car, car_target);
		}


		//Calculate the angles between the car direction and they target
		glm::vec3 car_target_direction = glm::normalize(car_target.target - car_position);

		if (glm::dot(car_front, car_target_direction) < 0.f)
		{
			//It is behind, needs to rotate
			target_x += (glm::dot(car_target_direction, car_left)) > 0.f ? -1.f : 1.f;
		}
		else
		{
			target_x += -glm::dot(car_target_direction, car_left);
		}
		target_y += -car_target_direction.z;

		car_control.X_target = glm::mix(car_control.X_target, target_x, glm::clamp(c_car_ai_target_speed * elapsed_time, 0.f, 1.f));
		car_control.Y_target = glm::mix(car_control.Y_target, target_y, glm::clamp(c_car_ai_target_speed * elapsed_time, 0.f, 1.f));

		car_control.X_target = glm::clamp(target_x, -c_car_X_range, c_car_X_range);
		car_control.Y_target = glm::clamp(target_y, -c_car_Y_range, c_car_Y_range);
		car_control.foward = glm::clamp(car_control.foward, 0.f, 1.f);
	}

	void CalculateForcesAndIntegrateCar(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float elapsed_time)
	{
		glm::vec3 linear_forces (0.f, 0.f, 0.f);
		glm::vec3 angular_forces(0.f, 0.f, 0.f);

		const glm::mat3x3 car_matrix = glm::toMat3(*car.rotation);

		const glm::vec3 car_left_vector = glm::row(car_matrix, 0);
		const glm::vec3 car_front_vector = glm::row(car_matrix, 1);
		const glm::vec3 car_up_vector = glm::row(car_matrix, 2);
		const glm::vec3 up_vector(0.f, 0.f, 1.f);
		const glm::vec3 car_left_flat = glm::normalize(glm::vec3(car_left_vector.x, car_left_vector.y, 0.f));

		//Apply car Y target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.Y_target * glm::half_pi<float>();
			float diff_angle = target - (glm::angle(car_front_vector, up_vector) - glm::half_pi<float>());

			//Convert it into angular force
			angular_forces += car_left_flat * diff_angle * c_car_Y_pitch_force;
			linear_forces += car_up_vector * c_car_Y_pitch_linear_force * car_control.Y_target;
		}
		//Apply car X target forces
		{
			//The target pitch represent the angle that needs to be
			float target = car_control.X_target * glm::half_pi<float>();
			float diff_angle = target - (glm::angle(car_left_vector, -up_vector) - glm::half_pi<float>());

			//Generate roll forces
			angular_forces += car_front_vector * diff_angle * c_car_X_roll_angular_force;
			angular_forces -= up_vector * c_car_X_jaw_angular_force * car_control.X_target;
			linear_forces += car_left_flat * c_car_X_linear_force * car_control.X_target;
		}
		//Apply forward, it will allow to go up/down and left/right when rolling
		{
			glm::vec3 foward_force = car_control.foward * c_car_foward_force * car_front_vector;

			//Will up force if it is over the world
			float distance_top = (*car.position).z - BoxCityTileSystem::kTileHeightTop;
			if (distance_top > 0.f)
			{
				foward_force.z -= distance_top * c_car_foward_kill_height_force;
			}

			float distance_bottom = (*car.position).z - BoxCityTileSystem::kTileHeightBottom;
			if (distance_bottom < 0.f)
			{
				foward_force.z -= distance_bottom * c_car_foward_kill_height_force;
			}
			
			linear_forces += foward_force;
		}

		//Apply friction
		{
			linear_forces -= car_movement.lineal_velocity * glm::clamp(c_car_friction_linear_force * elapsed_time, 0.f, 1.f) / elapsed_time;
			angular_forces -= car_movement.rotation_velocity * glm::clamp(c_car_friction_angular_force * elapsed_time, 0.f, 1.f) / elapsed_time;
		}


		//Integrate velocity
		car_movement.lineal_velocity +=  linear_forces * car_settings.inv_mass * elapsed_time;

		//Calculate world inertia mass
		glm::mat3x3 inertia_matrix = glm::scale(car_settings.inv_mass_inertia);
		glm::mat3x3 world_inv_mass_inertial = car_matrix * inertia_matrix * glm::inverse(car_matrix);
		car_movement.rotation_velocity += angular_forces * elapsed_time * world_inv_mass_inertial;

		//Integrate position
		*car.position = car.position.Last() + car_movement.lineal_velocity * elapsed_time;
		float rotation_angle = glm::length(car_movement.rotation_velocity * elapsed_time);
		if (rotation_angle > 0.000001f)
		{
			*car.rotation = glm::normalize(car.rotation.Last() * glm::angleAxis(rotation_angle, car_movement.rotation_velocity / rotation_angle));
		}
	}
}