#ifndef BOX_CITY_CAR_CONTROL_H
#define BOX_CITY_CAR_CONTROL_H

#include "box_city_components.h"
#include <core/platform.h>
#include <helpers/camera.h>

namespace BoxCityTileSystem
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
	void UpdateAIControl(std::mt19937& random, CarControl& car_control, const Car& car, const CarMovement& car_movement, const CarSettings& car_settings, CarTarget& car_target, float elapsed_time, BoxCityTileSystem::Manager* manager, const glm::vec3& camera_pos);

	//Setup new car target
	void SetupCarTarget(std::mt19937& random, const Car& car, CarTarget& car_target);

	//Calculate car forces and apply
	void CalculateForcesAndIntegrateCar(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control, float elapsed_time);
}

#endif //BOX_CITY_CAR_CONTROL_H