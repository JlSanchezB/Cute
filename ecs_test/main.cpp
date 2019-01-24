#include "ecs/entity_component_desc.h"
#include "ecs/entity_component_system.h"
#include "core/virtual_buffer.h"

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

using Component_PositionComponent = ecs::ComponentDesc<PositionComponent>;
using Component_VelocityComponent = ecs::ComponentDesc<VelocityComponent>;
using Component_OrientationComponent = ecs::ComponentDesc<OrientationComponent>;
using Component_TriangleShapeComponent = ecs::ComponentDesc<TriangleShapeComponent>;
using Component_CircleShapeComponent = ecs::ComponentDesc<CircleShapeComponent>;
using Component_SquareShapeComponent = ecs::ComponentDesc<SquareShapeComponent>;

using TriangleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, TriangleShapeComponent>;
using CircleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, CircleShapeComponent>;
using SquareEntityType = ecs::EntityType<PositionComponent, VelocityComponent, SquareShapeComponent>;

struct Game;
using Instance = ecs::Instance<Game>;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	core::VirtualBuffer virtual_buffer(1024 * 1024 * 8);

	virtual_buffer.SetCommitedSize(300);
	static_cast<char*>(virtual_buffer.GetPtr())[299] = 1;
	virtual_buffer.SetCommitedSize(30000);
	static_cast<char*>(virtual_buffer.GetPtr())[30000 - 1] = 1;
//	static_cast<char*>(virtual_buffer.GetPtr())[50000] = 1;
	virtual_buffer.SetCommitedSize(2000);
	virtual_buffer.SetCommitedSize(231231);
	virtual_buffer.SetCommitedSize(0);
//	static_cast<char*>(virtual_buffer.GetPtr())[1] = 1;
	virtual_buffer.SetCommitedSize(1024 * 1024 * 8);


	//Create ecs database
	ecs::DatabaseDesc database_desc;
	database_desc.AddComponent<PositionComponent>();
	database_desc.AddComponent<VelocityComponent>();
	database_desc.AddComponent<OrientationComponent>();
	database_desc.AddComponent<TriangleShapeComponent>();
	database_desc.AddComponent<CircleShapeComponent>();
	database_desc.AddComponent<SquareShapeComponent>();

	database_desc.AddEntityType<TriangleEntityType>();
	database_desc.AddEntityType<CircleEntityType>();
	database_desc.AddEntityType<SquareEntityType>();

	ecs::Database* database = ecs::CreateDatabase<Game>(database_desc);

	Instance instance = ecs::AllocInstance<Game, TriangleEntityType>();
}