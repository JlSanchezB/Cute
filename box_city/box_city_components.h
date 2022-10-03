#ifndef BOX_CITY_COMPONENTS_H
#define BOX_CITY_COMPONENTS_H

#include <helpers/collision.h>
#include <render/render.h>
#include <ecs/entity_component_system.h>
#include <render_module/render_module_gpu_memory.h>

//Components
struct FlagBox
{
	bool gpu_updated : 1;

	FlagBox()
	{
		gpu_updated = false;
	}
};

using AABBBox = helpers::AABB;

using OBBBox = helpers::OBB;

struct AnimationBox
{
	glm::vec3 original_position;
	float range; //Distance to navigate in axis Z
	float offset; //Start offset
	float frecuency; //Speed
};

struct Attachment
{
	ecs::InstanceReference parent;
	glm::mat4x4 parent_to_child;
};

struct BoxGPUHandle
{
	//Access to gpu buffer memory
	render::AllocHandle gpu_memory;
};

struct BoxRender
{
	glm::vec4 colour;
};

//ECS definition
using BoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox>;
using AnimatedBoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox, FlagBox>;
using AttachedPanelType = ecs::EntityType< BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox, Attachment>;
using PanelType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox>;

using GameComponents = ecs::ComponentList<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox, FlagBox, Attachment>;
using GameEntityTypes = ecs::EntityTypeList<BoxType, AnimatedBoxType, AttachedPanelType, PanelType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;

//GPU memory structs
struct GPUBoxInstance
{
	glm::vec4 local_matrix[3];
	glm::vec4 colour;
	void Fill(const helpers::OBB& obb_box)
	{
		local_matrix[0] = glm::vec4(obb_box.rotation[0][0] * obb_box.extents[0], obb_box.rotation[0][1] * obb_box.extents[1], obb_box.rotation[0][2] * obb_box.extents[2], obb_box.position.x);
		local_matrix[1] = glm::vec4(obb_box.rotation[1][0] * obb_box.extents[0], obb_box.rotation[1][1] * obb_box.extents[1], obb_box.rotation[1][2] * obb_box.extents[2], obb_box.position.y);
		local_matrix[2] = glm::vec4(obb_box.rotation[2][0] * obb_box.extents[0], obb_box.rotation[2][1] * obb_box.extents[1], obb_box.rotation[2][2] * obb_box.extents[2], obb_box.position.z);
	}
	void Fill(const BoxRender& box_render)
	{
		colour = box_render.colour;
	}
};

#endif //BOX_CITY_COMPONENTS_H