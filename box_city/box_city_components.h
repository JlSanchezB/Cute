#ifndef BOX_CITY_COMPONENTS_H
#define BOX_CITY_COMPONENTS_H

#include <helpers/collision.h>
#include <render/render.h>
#include <ecs/entity_component_system.h>

//Components
struct FlagBox
{
	bool gpu_updated : 1;

	FlagBox()
	{
		gpu_updated = false;
	}
};

struct AABBBox : helpers::AABB
{
};

struct OBBBox : helpers::OBB
{
};

struct AnimationBox
{
	glm::vec3 original_position;
	float range; //Distance to navigate in axis Z
	float offset; //Start offset
	float frecuency; //Speed
};

struct BoxGPUHandle
{
	//Access to gpu buffer memory
	render::AllocHandle gpu_memory;
};

struct BoxRender
{
	//Index in the material array
	uint32_t material;
};

//ECS definition
using BoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox>;
using AnimatedBoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox, FlagBox>;

using GameComponents = ecs::ComponentList<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox, FlagBox>;
using GameEntityTypes = ecs::EntityTypeList<BoxType, AnimatedBoxType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;

#endif //BOX_CITY_COMPONENTS_H