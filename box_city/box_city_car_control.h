#ifndef BOX_CITY_CAR_CONTROL_H
#define BOX_CITY_CAR_CONTROL_H

#include "box_city_components.h"
#include <core/platform.h>
#include <helpers/camera.h>

namespace BoxCityCarControl
{
	//Camera that follows a car
	class CarCamera : public helpers::Camera
	{
	public:
		//Process input and update the position
		void Update(platform::Game* game, Car& car, float ellapsed_time);

		CarCamera(const Camera::ZRange z_range) : Camera(Type::Target, z_range)
		{
		}
	private:
	};

	//Read the player input and update the car
	void UpdatePlayerControl(platform::Game* game, CarControl& car_control, float ellapsed_time);

	//Calculate car forces and apply
	void CalculateForcesAndIntegrateCar(Car& car, CarMovement& car_movement, CarSettings& car_settings, CarControl& car_control);
}

#endif //BOX_CITY_CAR_CONTROL_H