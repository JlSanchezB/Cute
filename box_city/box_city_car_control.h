#ifndef BOX_CITY_CAR_CONTROL_H
#define BOX_CITY_CAR_CONTROL_H

#include "box_city_components.h"
#include <core/platform.h>
#include <helpers/camera.h>

namespace BoxCityTileSystem
{
	class Manager;
}

namespace BoxCityTrafficSystem
{
	class Manager;
}

namespace BoxCityCarControl
{
	//Camera that follows a car
	class CarCamera : public helpers::Camera
	{
	public:
		//Process input and update the position during the render, so interpolated
		void Update(platform::Game* game, Car& car, float ellapsed_time);

		CarCamera(const Camera::ZRange z_range) : Camera(Type::Target, z_range)
		{
		}
	private:
	};

	//Read the player input and update the car
	void UpdatePlayerControl(platform::Game* game, CarControl& car_control, float elapsed_time);

	//Calculate AI car control
	void UpdateAIControl(std::mt19937& random, uint32_t instance_index, CarControl& car_control, const Car& car, const CarMovement& car_movement, const CarSettings& car_settings, CarTarget& car_target, CarBuildingsCache& car_buildings_cache, uint32_t frame_index, float elapsed_time, BoxCityTileSystem::Manager* tile_manager, BoxCityTrafficSystem::Manager* traffic_manager, const glm::vec3& camera_pos);

	//Setup new car target
	void SetupCarTarget(std::mt19937& random, BoxCityTileSystem::Manager* manager, const Car& car, CarTarget& car_target, bool reset = false);

	//Calculate car forces from the control system
	void CalculateControlForces(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float elapsed_time, glm::vec3& linear_forces, glm::vec3& angular_forces);

	//Calculate car forces from collision
	void CalculateCollisionForces(BoxCityTileSystem::Manager* manager, const glm::vec3& camera_pos, AABBBox& aabb, OBBBox& obb, glm::vec3& linear_forces, glm::vec3& angular_forces, glm::vec3& position_offset);
	
	//Integrate forces
	void IntegrateCar(Car& car, CarMovement& car_movement, const CarSettings& car_settings, const glm::vec3& linear_forces, const glm::vec3& angular_forces, const glm::vec3& position_offset, float elapsed_time);
}

#endif //BOX_CITY_CAR_CONTROL_H