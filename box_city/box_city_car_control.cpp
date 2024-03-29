#include "box_city_car_control.h"
#include <core/control_variables.h>
#include <core/counters.h>
#include <core/platform.h>
#include "box_city_tile_manager.h"
#include "box_city_traffic_manager.h"
#include <render/render_debug_primitives.h>

//List of control variables

CONTROL_VARIABLE_BOOL(c_car_ai_avoidance_enable, true, "Car AI", "Car AI avoidance enabled");
CONTROL_VARIABLE_BOOL(c_car_ai_targeting_enable, true, "Car AI", "Car AI targeting enabled");
CONTROL_VARIABLE_BOOL(c_car_collision_enable, true, "Car Collision", "Car collision enabled");

//Pitch input
CONTROL_VARIABLE(float, c_car_Y_range, 0.f, 1.f, 1.0f, "Car Control", "Y Range");
CONTROL_VARIABLE(float, c_car_Y_mouse_factor, 0.f, 10.f, 0.2f, "Car Control", "Y Mouse Factor");
CONTROL_VARIABLE(float, c_car_Y_keyboard_factor, 0.f, 10.f, 2.0f, "Car Control", "Y Keyboard Factor");
CONTROL_VARIABLE_BOOL(c_car_inverse_Y, false, "Car Control", "Y Inverse");
CONTROL_VARIABLE(float, c_car_Y_absorber, 0.f, 1.f, 0.15f, "Car Control", "Y Absorber");

//Roll input
CONTROL_VARIABLE(float, c_car_X_range, 0.f, 1.f, 0.8f, "Car Control", "X Range");
CONTROL_VARIABLE(float, c_car_X_mouse_factor, 0.f, 10.f, 0.2f, "Car Control", "X Mouse Factor");
CONTROL_VARIABLE(float, c_car_X_keyboard_factor, 0.f, 10.f, 2.f, "Car Control", "X Keyboard Factor");
CONTROL_VARIABLE(float, c_car_X_absorber, 0.f, 1.f, 0.15f, "Car Control", "X Absorber");

//Foward input
CONTROL_VARIABLE(float, c_car_foward_mouse_factor, 0.f, 10.f, 2.25f, "Car Control", "Foward Mouse Factor");
CONTROL_VARIABLE(float, c_car_foward_keyboard_factor, 0.f, 10.f, 1.25f, "Car Control", "Foward Keybard Factor");

//Pitch control
CONTROL_VARIABLE(float, c_car_Y_pitch_force, 0.f, 10.f, 0.02f, "Car Control", "Y Pitch Force");
CONTROL_VARIABLE(float, c_car_Y_pitch_linear_force, 0.f, 10.f, 0.00f, "Car Control", "Y Pitch Linear Force");

//Roll control
CONTROL_VARIABLE(float, c_car_X_roll_angular_force, 0.f, 10.f, 0.02f, "Car Control", "X Roll Angular Force");
CONTROL_VARIABLE(float, c_car_X_jaw_angular_force, 0.f, 10.f, 0.05f, "Car Control", "X Jaw Angular Force");
CONTROL_VARIABLE(float, c_car_X_linear_force, 0.f, 10.f, 0.00f, "Car Control", "X Linear Force");

//Forward
CONTROL_VARIABLE(float, c_car_foward_force, 0.f, 10000.f, 200.0f, "Car Control", "Foward Force");
CONTROL_VARIABLE(float, c_car_foward_kill_height_force, 0.f, 100.f, 2.0f, "Car Control", "Foward Kill Heigth Force");

//Friction
CONTROL_VARIABLE(float, c_car_friction_linear_force, 0.f, 10.f, 1.4f, "Car Control", "Linear Friction Force");
CONTROL_VARIABLE(float, c_car_friction_angular_force, 0.f, 10.f, 1.8f, "Car Control", "Angular Friction Force");

//Collision
CONTROL_VARIABLE(float, c_car_collision_lost, 0.f, 1.f, 1.0f, "Car Collision", "Energy lost during collision");

//Aerodynamic forces
CONTROL_VARIABLE(float, c_car_aerodynamic_linear_force, 0.f, 10.f, 1.5f, "Car Control", "Linear Aerodynamic Force");

CONTROL_VARIABLE(float, c_car_camera_distance, 0.f, 100.f, 4.5f, "Car Camera", "Camera Distance");
CONTROL_VARIABLE(float, c_car_camera_up_offset, 0.f, 100.f, 1.f, "Car Camera", "Camera Up Offset");
CONTROL_VARIABLE(float, c_car_camera_fov, 60.f, 180.f, 100.f, "Car Camera", "Camera Fov");
CONTROL_VARIABLE(float, c_car_camera_speed, 0.f, 200.f, 30.f, "Car Camera", "Camera Speed");
CONTROL_VARIABLE(float, c_car_camera_car_rotation_min, 0.f, 10.f, 0.4f, "Car Camera", "Camera Car Rotation Min");
CONTROL_VARIABLE(float, c_car_camera_car_rotation_factor, 0.f, 10.f, 2.f, "Car Camera", "Camera Car Rotation Factor");

CONTROL_VARIABLE(float, c_car_ai_forward, 0.f, 1.f, 0.25f, "Car AI", "Camera AI foward");
CONTROL_VARIABLE(float, c_car_ai_min_forward, 0.f, 1.f, 0.05f, "Car AI", "Camera AI min foward");
CONTROL_VARIABLE(float, c_car_ai_avoidance_calculation_distance, 0.f, 10000.f, 1000.f, "Car AI", "Camera AI avoidance calculation distance");
CONTROL_VARIABLE(float, c_car_ai_visibility_distance, 0.f, 10.f, 80.f, "Car AI", "Camera AI visibility distance");
CONTROL_VARIABLE(float, c_car_ai_visibility_side_distance, 0.f, 10.f, 20.f, "Car AI", "Camera AI visibility side distance");
CONTROL_VARIABLE(float, c_car_ai_avoidance_extra_distance, 0.f, 1000.f, 5.f, "Car AI", "Camera AI avoidance extra distance with building");
CONTROL_VARIABLE(float, c_car_ai_avoidance_distance_expansion, 0.f, 1000.f, 2.f, "Car AI", "Camera AI avoidance extra expansion apply to buildings when far");
CONTROL_VARIABLE(float, c_car_ai_avoidance_reaction_factor, 0.f, 10.f, 8.0f, "Car AI", "Car AI avoidance reaction factor");
CONTROL_VARIABLE(float, c_car_ai_avoidance_reaction_power, 0.f, 10.f, 1.0f, "Car AI", "Car AI avoidance reaction power");
CONTROL_VARIABLE(float, c_car_ai_avoidance_slow_factor, 0.f, 1.f, 0.0f, "Car AI", "Car AI avoidance slow factor");
CONTROL_VARIABLE(float, c_car_ai_target_range, 1.f, 10000.f, 2000.f, "Car AI", "Car AI target range");
CONTROL_VARIABLE(float, c_car_ai_target_reaction_factor, 1.f, 10.f, 4.f, "Car AI", "Car AI target reaction factor");
CONTROL_VARIABLE(float, c_car_ai_min_target_range, 1.f, 10000.f, 500.f, "Car AI", "Car AI min target range");
CONTROL_VARIABLE(float, c_car_ai_min_target_distance, 1.f, 10000.f, 20.f, "Car AI", "Car AI min target distance");
CONTROL_VARIABLE(float, c_car_ai_close_target_distance, 1.f, 10000.f, 50.f, "Car AI", "Car AI close target distance");
CONTROL_VARIABLE(float, c_car_ai_close_target_distance_slow, 0.f, 1.f, 0.6f, "Car AI", "Car AI close target distance slow");
CONTROL_VARIABLE(float, c_car_ai_lane_size, 0.f, 10.f, 0.0f, "Car AI", "Car AI lane size");

CONTROL_VARIABLE(float, c_car_gyroscope_collision_control, 0.f, 1.f, 0.05f, "Car Control", "Car gyroscope collision control");
CONTROL_VARIABLE(float, c_car_gyroscope_control_min, 0.f, 10.f, 1.0f, "Car Control", "Car gyroscope control min");
CONTROL_VARIABLE(float, c_car_gyroscope_control_factor, 0.f, 10.f, 1.0f, "Car Control", "Car gyroscope control factor");

CONTROL_VARIABLE_BOOL(c_car_ai_debug_render, false, "Car Debug", "Car AI debug render");

//Counters
COUNTER(c_Car_Collisions, "Cars", "Cars collision", true);
COUNTER(c_Car_Retargets, "Cars", "Cars retargetting", true);
COUNTER(c_Car_Caching_Buildings, "Cars", "Cars Caching Buildings", true);

//#pragma optimize("", off)

namespace BoxCityCarControl
{
	static bool NeedsUpdate(uint32_t instance_index, uint32_t frame_index, uint32_t frame_rate)
	{
		//We divide the instance index by 8 to improve the timeslice cache access, so closest instances will skip frames

		return ((frame_index + instance_index/8) % frame_rate) == 0;
	}

	static bool NeedsUpdate(uint32_t instance_index, uint32_t frame_index, uint32_t max_frame_rate, float min_range, float max_range, float factor)
	{
		float t = glm::clamp((factor - min_range) / (max_range - min_range), 0.f, 1.f);
		uint32_t frame_rate = static_cast<uint32_t>(glm::ceil(t * max_frame_rate));
		frame_rate = glm::clamp<uint32_t>(frame_rate, 1, max_frame_rate);
		return NeedsUpdate(instance_index, frame_index, frame_rate);
	}

	void CarCamera::Update(platform::Game* game, const Car& car, const CarMovement& car_movement, float elapsed_time)
	{
		glm::mat3x3 car_matrix = glm::toMat3(*car.rotation);
		glm::vec3 car_vector = glm::row(car_matrix, 1);
		glm::vec3 camera_vector = glm::normalize(*car.position - *m_position);
		float car_rotation_velocity = glm::length(car_movement.rotation_velocity);
		//Now we choose the best one, depends of the car rotation speed
		glm::vec3 vector = glm::normalize(glm::mix(car_vector, camera_vector, glm::clamp((car_rotation_velocity - c_car_camera_car_rotation_min) * c_car_camera_car_rotation_factor, 0.f, 1.f)));

		*m_position = glm::mix(*m_position, *car.position - vector * c_car_camera_distance + glm::vec3(0.f, 0.f, c_car_camera_up_offset), glm::clamp(elapsed_time * c_car_camera_speed, 0.f, 1.f));
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

	void SetupCarTarget(std::mt19937& random, BoxCityTileSystem::Manager* manager, const Car& car, CarTarget& car_target, bool reset)
	{
		glm::vec3 last_target;
		if (reset)
		{
			last_target = *car.position;
		}
		else
		{
			last_target = car_target.target;
		}
		car_target.target_valid = manager->GetNextTrafficTarget(random, *car.position, car_target.target);
		if (car_target.target_valid)
		{
			car_target.last_target = last_target;

			if (car_target.target != last_target)
			{
				//Move the target with a lane size
				glm::vec3 direction = glm::normalize(car_target.target - last_target);

				glm::vec3 offset;
				if (direction != glm::vec3(0.f, 0.f, 1.f))
				{
					offset = glm::normalize(glm::cross(direction, glm::vec3(0.f, 0.f, 1.f)));
				}
				else
				{
					offset = glm::normalize(glm::cross(direction, glm::vec3(0.f, 1.f, 0.f)));
				}

				car_target.target = car_target.target + c_car_ai_lane_size * offset;
			}
		}
		assert(glm::all(glm::isfinite(car_target.target)));
		assert(glm::all(glm::isfinite(car_target.last_target)));
	}
	void UpdateAIControl(std::mt19937& random, uint32_t instance_index, CarControl& car_control, const Car& car, const CarMovement& car_movement, const CarSettings& car_settings, CarTarget& car_target, CarBuildingsCache& car_buildings_cache, uint32_t frame_index, float elapsed_time, BoxCityTileSystem::Manager* tile_manager, BoxCityTrafficSystem::Manager* traffic_manager, const glm::vec3& camera_pos, bool is_player_car)
	{
		const glm::vec3 car_position = *car.position;
		float camera_distance2 = glm::distance2(camera_pos, car_position);

		//We timeslice the update by the distance to the camera
		if (!NeedsUpdate(instance_index, frame_index, 8, 500.f, 3000.f, glm::fastSqrt(camera_distance2))) return;

		const glm::mat3x3 car_matrix = glm::toMat3(*car.rotation);
		const glm::vec3 car_left = glm::row(car_matrix, 0);
		const glm::vec3 car_front = glm::row(car_matrix, 1);
		const glm::vec3 car_top = glm::row(car_matrix, 2);
		const glm::vec3 car_left_flat = glm::normalize(glm::vec3(car_left.x, car_left.y, 0.f));
		const float car_radius = glm::fastLength(car_settings.size);
		const glm::vec3 car_top_flat = car_top - glm::dot(car_left, car_top) * car_left;
		//Calculate X and Y control for the car
		car_control.foward = c_car_ai_forward;

		//Calculate avoidance
		glm::vec2 avoidance_target(0.f, 0.f);
		float avoidance_factor = 0.f;
		if (c_car_ai_avoidance_enable && camera_distance2 < c_car_ai_avoidance_calculation_distance * c_car_ai_avoidance_calculation_distance)
		{
			const glm::vec3 car_direction = glm::normalize(car_movement.linear_velocity);

			//Only update the buildings if needed, each 4 frames
			if (NeedsUpdate(instance_index, frame_index, 4))
			{
				COUNTER_INC(c_Car_Caching_Buildings);

				//Calculate visibility AABB
				helpers::AABB car_frustum;
				car_frustum.Add(car_position - car_direction * c_car_ai_visibility_distance * 0.05f);

				car_frustum.Add(car_position + glm::vec3(0.f, 0.f, 1.f) * c_car_ai_visibility_side_distance + car_left_flat * c_car_ai_visibility_side_distance + car_direction * c_car_ai_visibility_distance);
				car_frustum.Add(car_position + glm::vec3(0.f, 0.f, 1.f) * c_car_ai_visibility_side_distance - car_left_flat * c_car_ai_visibility_side_distance + car_direction * c_car_ai_visibility_distance);
				car_frustum.Add(car_position - glm::vec3(0.f, 0.f, 1.f) * c_car_ai_visibility_side_distance + car_left_flat * c_car_ai_visibility_side_distance + car_direction * c_car_ai_visibility_distance);
				car_frustum.Add(car_position - glm::vec3(0.f, 0.f, 1.f) * c_car_ai_visibility_side_distance - car_left_flat * c_car_ai_visibility_side_distance + car_direction * c_car_ai_visibility_distance);

				//Collect the top 4 building close the car
				float building_distances[CarBuildingsCache::kNumCachedBuildings] = {FLT_MAX};
				for (auto& it : car_buildings_cache.buildings) it.size = 0.f;

				//List all buldings colliding with this visibility AABB
				tile_manager->VisitBuildings(car_frustum, [&](const InstanceReference& building)
					{
						OBBBox& avoid_box = building.Get<GameDatabase>().Get<OBBBox>();

						const glm::vec3 extent = glm::row(avoid_box.rotation, 2) * (avoid_box.extents.z);
						const glm::vec3 building_bottom = avoid_box.position - extent;
						const glm::vec3 building_top = avoid_box.position + extent;

						glm::vec3 closest_point = helpers::CalculateClosestPointToSegment(car_position, building_bottom, building_top);

						float distance = glm::distance2(car_position, closest_point);

						//Now check if it needs to be added
						for (uint32_t i = 0; i < CarBuildingsCache::kNumCachedBuildings; ++i)
						{
							if (distance < building_distances[i])
							{
								//Needs to be inserted here and displace the rest
								for (uint32_t j = i; j < CarBuildingsCache::kNumCachedBuildings - 1; ++j)
								{
									car_buildings_cache.buildings[j + 1] = car_buildings_cache.buildings[j];
									building_distances[j + 1] = building_distances[j];
								}

								//Update the current slot
								car_buildings_cache.buildings[i].position = avoid_box.position;
								car_buildings_cache.buildings[i].extent = extent;
								car_buildings_cache.buildings[i].size = glm::fastLength(glm::vec2(avoid_box.extents.x, avoid_box.extents.y));
								building_distances[i] = distance;

								break;
							}
						}
					}
				);
			}

			//Calculate avoidance with the cached buildings
			for (auto& building : car_buildings_cache.buildings)
			{
				if (building.size > 0.f)
				{
					glm::vec3 box_point, car_point;
					float box_t, car_t;

					//Building top and bottom points
					const glm::vec3 building_bottom = building.position - building.extent;
					const glm::vec3 building_top = building.position + building.extent;

					//Avoid buildings in front
					{
						helpers::CalculateClosestPointsInTwoSegments(car_position, car_position + car_direction * (c_car_ai_visibility_distance),
							building_bottom, building_top,
							car_point, box_point, car_t, box_t);

						float expansion = car_t * c_car_ai_avoidance_distance_expansion;
						//Calculate distance between the points and check with the wide of the box, that is good for the caps as well
						if (glm::length2(car_point - box_point) < glm::pow2(building.size + expansion + c_car_ai_avoidance_extra_distance + car_radius))
						{
							//It is going to collide
							//You need to avoid box_point
							glm::vec3 car_avoid_direction = glm::normalize(box_point - car_position);

							float xx = glm::dot(car_avoid_direction, car_left_flat);
							xx = (glm::sign<float>(xx) - xx) * c_car_ai_avoidance_reaction_factor;
							xx = glm::sign(xx) * glm::pow(glm::abs(xx), c_car_ai_avoidance_reaction_power);
							avoidance_target.x += xx;
							float yy = glm::dot(car_avoid_direction, car_top_flat);
							yy = (glm::sign<float>(yy) - yy) * c_car_ai_avoidance_reaction_factor;
							yy = glm::sign(yy) * glm::pow(glm::abs(yy), c_car_ai_avoidance_reaction_power);
							avoidance_target.y += yy;

							car_control.foward -= c_car_ai_avoidance_slow_factor * (1.f - car_t);

							avoidance_factor = glm::max(avoidance_factor, 1.f - car_t);

							if (c_car_ai_debug_render && is_player_car)
							{
								render::debug_primitives::DrawLine(building_bottom, building_top, render::debug_primitives::kRed);
							}
						}
						else
						{
							if (c_car_ai_debug_render && is_player_car)
							{
								render::debug_primitives::DrawLine(building_bottom, building_top, render::debug_primitives::kGreen);
							}
						}
					}
				}
			}
		}
		
		float target_x = avoidance_target.x;
		float target_y = avoidance_target.y;

		//Calculate if it needs retargetting
		float target_distance2 = glm::length2(*car.position - car_target.target);
		//assert(!car_target.target_valid || target_distance2 < 2000.f * 2000.f);

		if (target_distance2 < c_car_ai_min_target_distance * c_car_ai_min_target_distance || !car_target.target_valid)
		{
			//Retarget
			SetupCarTarget(random, tile_manager, car, car_target);

			COUNTER_INC(c_Car_Retargets);
		}

		if (c_car_ai_targeting_enable && car_target.target_valid)
		{
			float avoidance_adjusted = (1.f - avoidance_factor);

			float target_pos = glm::clamp(glm::length(car_target.target - car_position) / glm::length(car_target.target - car_target.last_target), 0.f, 1.f);
			//float lerp_factor = 1.f;// glm::smoothstep(0.4f, 0.9f, target_pos);
			//Calculate the angles between the car direction and they target
			//glm::vec3 car_in_target_line = helpers::CalculateClosestPointToSegment(car_position, car_target.last_target, car_target.target);
			//glm::vec3 car_target_direction = glm::normalize(glm::mix(car_target.target, car_in_target_line, glm::mix(0.85f, 0.f, lerp_factor)) - car_position);
			glm::vec3 car_target_direction = glm::normalize(car_target.target - car_position);
			//assert(glm::all(glm::isfinite(car_in_target_line)));
			
			/*if (glm::dot(car_front, car_target_direction) < 0.f)
			{
				//It is behind, needs to rotate
				target_x += ((glm::dot(car_target_direction, car_left_flat)) > 0.f ? -1.f : 1.f) * avoidance_adjusted * 10.f;
				car_control.foward -= c_car_ai_close_target_distance_slow;
			}
			else*/
			{
				target_x += -glm::dot(car_target_direction, car_left_flat) * avoidance_adjusted * c_car_ai_target_reaction_factor;
			}
			target_y += -glm::dot(car_target_direction, car_top_flat) * avoidance_adjusted * c_car_ai_target_reaction_factor;

			if (target_distance2 < c_car_ai_close_target_distance * c_car_ai_close_target_distance)
			{
				//Reduce speed for improving targeting
				car_control.foward -= c_car_ai_close_target_distance_slow * (1.f - glm::pow2(glm::clamp(target_distance2 / c_car_ai_close_target_distance * c_car_ai_close_target_distance, 0.f, 1.f)));
			}

			if (c_car_ai_debug_render && is_player_car)
			{
				render::debug_primitives::DrawStar(car_target.target, 5.f, render::debug_primitives::kGreen);
			}
		}
		car_control.foward = glm::max(car_control.foward * car_settings.speed_factor, c_car_ai_min_forward);

		//Update targets
		car_control.X_target = glm::clamp(target_x, -c_car_X_range, c_car_X_range);
		car_control.Y_target = glm::clamp(target_y, -c_car_Y_range, c_car_Y_range);
		car_control.foward = glm::clamp(car_control.foward, 0.f, 1.f);

		if (c_car_ai_debug_render && is_player_car)
		{
			render::debug_primitives::DrawLine(car_position, car_position + car_left * car_control.X_target, render::debug_primitives::kYellow);
			render::debug_primitives::DrawLine(car_position, car_position + car_top * car_control.Y_target, render::debug_primitives::kYellow);
			render::debug_primitives::DrawLine(car_position, car_position + car_front * car_control.foward, render::debug_primitives::kYellow);
		}
	}
	void CalculateControlForces(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float elapsed_time, glm::vec3& linear_forces, glm::vec3& angular_forces)
	{
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
			linear_forces -= car_movement.linear_velocity * glm::clamp(c_car_friction_linear_force * elapsed_time, 0.f, 1.f) / elapsed_time;
			angular_forces -= car_movement.rotation_velocity * glm::clamp(c_car_friction_angular_force * elapsed_time, 0.f, 1.f) / elapsed_time;
		}

		//Apply aerodynamic forces
		if (glm::length2(car_movement.linear_velocity) > 0.001f)
		{
			float aerodynamic_factor = glm::abs(glm::dot(car_front_vector, car_movement.linear_velocity));

			//Remove linear velocity in the direction of the car
			linear_forces -= glm::normalize(car_movement.linear_velocity) * glm::clamp(c_car_aerodynamic_linear_force * elapsed_time, 0.f, 1.f) / elapsed_time;
			//Add same force to the direction of the car
			linear_forces += car_front_vector * glm::clamp(c_car_aerodynamic_linear_force * elapsed_time, 0.f, 1.f) / elapsed_time;
		}

		//Apply gyroscope control
		float rotation_moment = glm::length(car_movement.rotation_velocity);
		if (rotation_moment > c_car_gyroscope_control_min)
		{
			float force_factor = glm::clamp((rotation_moment - c_car_gyroscope_control_min) * c_car_gyroscope_control_factor * elapsed_time, 0.f, 1.f) / elapsed_time;

			//We need to apply forces to kill the rotation velocity
			angular_forces -= car_movement.rotation_velocity * force_factor;

			//We want always to force it to loop up
			float diff_angle = 0.f - (glm::angle(car_front_vector, up_vector) - glm::half_pi<float>());
			angular_forces += car_left_flat * diff_angle * force_factor;
			diff_angle = 0.f - (glm::angle(car_left_vector, -up_vector) - glm::half_pi<float>());
			angular_forces += car_front_vector * diff_angle * force_factor;
		}


		assert(glm::all(glm::isfinite(linear_forces)));
		assert(glm::all(glm::isfinite(angular_forces)));
	}
	void CalculateCollisionForces(BoxCityTileSystem::Manager* manager, const float elapsed_time, const glm::vec3& camera_pos, OBBBox& obb, glm::vec3& linear_forces, glm::vec3& angular_forces, CarMovement& car_movement, CarSettings& car_settings, glm::vec3& position_offset)
	{
		if (c_car_collision_enable && glm::distance2(obb.position, camera_pos) < c_car_ai_avoidance_calculation_distance * c_car_ai_avoidance_calculation_distance)
		{
			helpers::AABB aabb;
			helpers::CalculateAABBFromOBB(aabb, obb);

			manager->VisitBuildings(aabb, [&](const InstanceReference& building)
				{
					assert(building.IsValid());
					assert(building.Get<GameDatabase>().Is<BoxType>() || building.Get<GameDatabase>().Is<AnimatedBoxType>());

					const OBBBox& building_box = building.Get<GameDatabase>().Get<OBBBox>();

					helpers::CollisionReturn collision_return;
					if (helpers::CollisionFeaturesOBBvsOBB(obb, building_box, collision_return))
					{	
						//Calculate the speed of the building, it is only linear
						glm::vec3 building_velocity = glm::vec3(0.f, 0.f, 0.f);
						if (building.Get<GameDatabase>().Is<AnimatedBoxType>())
						{
							platform::Interpolated<glm::vec3> position = building.Get<GameDatabase>().Get<InterpolatedPosition>().position;

							building_velocity = (*position - position.Last()) * elapsed_time;
						}
			

						//Bounce
						for (auto& contact : collision_return.contacts)
						{	
							glm::vec3 contact_vector = contact.position - obb.position;

							if (glm::length2(contact_vector) > 0.f)
							{
								//Calculate bounce back force in the contact point from velocity
								glm::vec3 contact_force = -building_velocity + car_movement.linear_velocity + glm::cross(car_movement.rotation_velocity, contact_vector);
								if (glm::dot(contact_force, contact.normal) < 0.f)
								{
									glm::vec3 bounce_back_force = -(1.f + c_car_collision_lost) * glm::dot(contact_force, contact.normal) * contact.normal;
									bounce_back_force /= (car_settings.inv_mass + glm::dot(contact.normal, glm::cross(glm::cross(contact_vector, contact.normal) * car_settings.inv_mass_inertia, contact_vector)));

									bounce_back_force *= (1.f / static_cast<float>(collision_return.contacts.size()));

									//Apply the bounce back force to the linear and angular velocity
									car_movement.linear_velocity += bounce_back_force * car_settings.inv_mass;
									car_movement.rotation_velocity -= c_car_gyroscope_collision_control * glm::cross(contact_vector, bounce_back_force) * car_settings.inv_mass_inertia;

									/*
									//Friction
									glm::vec3 contact_velocity_tangent = contact_force - glm::dot(contact_force, contact.normal) * contact.normal;
									//float friction_factor = glm::dot(contact_velocity_tangent, contact_force);

									//friction_factor *= (1.f / static_cast<float>(collision_return.contacts.size()));

									//Really simple friction
									glm::vec3 friction_force = -contact_velocity_tangent * 0.25f; //sticky

									//Apply the friction velocity to the linear and angular velocity
									car_movement.linear_velocity += friction_force * car_settings.inv_mass;
									car_movement.rotation_velocity -= glm::cross(contact_vector, friction_force) * car_settings.inv_mass_inertia;
									*/
								}
							}
						}
						
						//Readjust position
						position_offset -= collision_return.normal * collision_return.depth;

						COUNTER_INC(c_Car_Collisions);
					}
				});
		}
	}
	void IntegrateCar(Car& car, CarMovement& car_movement, const CarSettings& car_settings, const glm::vec3& linear_forces, const glm::vec3& angular_forces, const glm::vec3& position_offset, float elapsed_time)
	{
		assert(glm::all(glm::isfinite(car.position.Last())));

		const glm::mat3x3 car_matrix = glm::toMat3(*car.rotation);

		//Integrate velocity
		car_movement.linear_velocity += linear_forces * car_settings.inv_mass * elapsed_time;
		assert(glm::all(glm::isfinite(car_movement.linear_velocity)));

		//Calculate world inertia mass
		glm::mat3x3 inertia_matrix = glm::scale(car_settings.inv_mass_inertia);
		glm::mat3x3 world_inv_mass_inertial = car_matrix * inertia_matrix * glm::inverse(car_matrix);
		car_movement.rotation_velocity += angular_forces * elapsed_time * world_inv_mass_inertial;
		assert(glm::all(glm::isfinite(car_movement.rotation_velocity)));

		//Integrate position
		*car.position = car.position.Last() + car_movement.linear_velocity * elapsed_time + position_offset;
		float rotation_angle = glm::length(car_movement.rotation_velocity * elapsed_time);
		if (rotation_angle > 0.000001f)
		{
			*car.rotation = glm::normalize(car.rotation.Last() * glm::angleAxis(rotation_angle, car_movement.rotation_velocity / rotation_angle));
		}

		assert(glm::all(glm::isfinite(*car.position)));
	}
}