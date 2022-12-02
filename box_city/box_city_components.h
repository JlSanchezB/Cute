#ifndef BOX_CITY_COMPONENTS_H
#define BOX_CITY_COMPONENTS_H

#include <helpers/collision.h>
#include <render/render.h>
#include <ecs/entity_component_system.h>
#include <render_module/render_module_gpu_memory.h>
#include <ext/glm/ext.hpp>

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
	//Offset in the GPU handle
	uint32_t offset_gpu_allocator : 24;
	uint32_t lod_group : 8;

	static constexpr uint32_t kInvalidOffset = 0xFFFFFF;
	bool IsValid() const
	{
		return offset_gpu_allocator != kInvalidOffset;
	}

	BoxGPUHandle(const uint32_t _offset_gpu_allocator, const uint32_t _lod_group) : offset_gpu_allocator(_offset_gpu_allocator), lod_group(_lod_group)
	{
	}
};

struct BoxRender
{
	glm::vec4 colour;
};

struct Panel
{
};

struct Car
{
	glm::vec3 position= glm::vec3(0.f, 0.f, 0.f);
	glm::quat rotation = glm::quat(glm::vec3(0.f, 0.f, 0.f));

	Car()
	{
	};
	Car(const glm::vec3& _position, const glm::quat& _rotation) : position(_position), rotation(_rotation)
	{
	};
};

struct CarMovement
{
	glm::vec3 lineal_velocity = glm::vec3(0.f, 0.f, 0.f);
	glm::vec3 rotation_velocity = glm::vec3(0.f, 0.f, 0.f);

	CarMovement()
	{
	};
	CarMovement(const glm::vec3& _lineal_velocity, const glm::vec3& _rotation_velocity) : lineal_velocity(_lineal_velocity), rotation_velocity(_rotation_velocity)
	{
	};
};

struct CarSettings
{
	glm::vec3 size;
	float inv_mass;
	glm::vec3 inv_mass_inertia;

	CarSettings()
	{
	};
	CarSettings(const glm::vec3& _size, const float mass, const glm::vec3& mass_inertia) : size(_size), inv_mass(1.f / mass), inv_mass_inertia(1.f / mass_inertia.x, 1.f / mass_inertia.y, 1.f / mass_inertia.z)
	{
	}
};

struct CarControl
{
	float Y_target;
	float X_target;
	float foward;
	CarControl()
	{
	}
};

struct CarTarget
{
	glm::vec3 target;

	CarTarget()
	{
	};
	CarTarget(const glm::vec3& _target) : target(_target)
	{
	};
};

struct CarGPUIndex
{
	static constexpr uint16_t kInvalidSlot = 0xFFFF;

	uint16_t gpu_slot = kInvalidSlot;

	CarGPUIndex()
	{
	}

	CarGPUIndex(uint16_t _gpu_slot) : gpu_slot(_gpu_slot)
	{
	};
	bool IsValid() const
	{
		return gpu_slot != kInvalidSlot;
	}
};

//ECS definition
using BoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox>;
using AnimatedBoxType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox, FlagBox>;
using AttachedPanelType = ecs::EntityType< BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox, Attachment, Panel>;
using PanelType = ecs::EntityType<BoxRender, BoxGPUHandle, OBBBox, AABBBox, FlagBox, Panel>;
using CarType = ecs::EntityType<OBBBox, AABBBox, Car, CarMovement, CarSettings, CarTarget, CarControl, CarGPUIndex>;

using GameComponents = ecs::ComponentList<BoxRender, BoxGPUHandle, OBBBox, AABBBox, AnimationBox, FlagBox, Attachment, Panel, Car, CarMovement, CarSettings, CarTarget, CarGPUIndex, CarControl>;
using GameEntityTypes = ecs::EntityTypeList<BoxType, AnimatedBoxType, AttachedPanelType, PanelType, CarType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;
using InstanceReference = ecs::InstanceReference;

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