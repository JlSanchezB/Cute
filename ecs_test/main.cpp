#include "ecs/entity_component_desc.h"
#include "ecs/entity_component_system.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif
#include <windows.h>

struct PositionComponent
{
	float position[2];
};

struct VelocityComponent
{
	float velocity[2];
};

struct OrientationComponent
{
	float angle;
};

struct TriangleShapeComponent
{
	float size;
};

struct CircleShapeComponent
{
	float radius;
};

struct SquareShapeComponent
{
	float side;
};

DECLARE_COMPONENT(PositionComponent);
DECLARE_COMPONENT(VelocityComponent);
DECLARE_COMPONENT(OrientationComponent);
DECLARE_COMPONENT(TriangleShapeComponent);
DECLARE_COMPONENT(CircleShapeComponent);
DECLARE_COMPONENT(SquareShapeComponent);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	//Create ecs database

	ecs::DatabaseDesc database_desc;
	database_desc.Add<PositionComponent>();
	database_desc.Add<VelocityComponent>();
	database_desc.Add<OrientationComponent>();
	database_desc.Add<TriangleShapeComponent>();
	database_desc.Add<CircleShapeComponent>();
	database_desc.Add<SquareShapeComponent>();

	ecs::Database* database = ecs::CreateDatabase(database_desc);
}