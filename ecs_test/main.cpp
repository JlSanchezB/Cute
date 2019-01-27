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
	OrientationComponent() {};
	OrientationComponent(float _angle) : angle(_angle)
	{
	}
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

using TriangleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, OrientationComponent, TriangleShapeComponent>;
using CircleEntityType = ecs::EntityType<PositionComponent, VelocityComponent, OrientationComponent, CircleShapeComponent>;
using SquareEntityType = ecs::EntityType<PositionComponent, VelocityComponent, OrientationComponent, SquareShapeComponent>;

using GameComponents = core::TypeList<PositionComponent, VelocityComponent, OrientationComponent, TriangleShapeComponent, CircleShapeComponent, SquareShapeComponent>;
using GameEntityTypes = core::TypeList<TriangleEntityType, CircleEntityType, SquareEntityType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;


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
	ecs::Database* database = ecs::CreateDatabase<GameDatabase>(database_desc);

	Instance instance = ecs::AllocInstance<GameDatabase, TriangleEntityType>().InitDefault().Init<OrientationComponent>(3.5f);
}