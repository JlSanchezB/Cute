#ifndef BOX_CITY_COMPONENTS_H
#define BOX_CITY_COMPONENTS_H

#include <helpers/collision.h>
#include <render/render.h>
#include <ecs/entity_component_system.h>
#include <render_module/render_module_gpu_memory.h>
#include <ext/glm/ext.hpp>
#include <core/platform.h>

//Components
struct FlagBox
{
	bool moved : 1;

	FlagBox()
	{
		moved = false;
	}
};

using OBBBox = helpers::OBB;
using RangeAABB = helpers::AABB; //It includes as far it can move

struct InterpolatedPosition
{
	platform::Interpolated<glm::vec3> position;
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

struct Car
{
	platform::Interpolated<glm::vec3> position;
	platform::Interpolated<glm::quat> rotation;

	Car()
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
	float size;
	float inv_mass;
	glm::vec3 inv_mass_inertia;
	uint32_t car_type;

	CarSettings()
	{
	};
	CarSettings(const float _size, const float mass, const glm::vec3& mass_inertia) : size(_size), inv_mass(1.f / mass), inv_mass_inertia(1.f / mass_inertia.x, 1.f / mass_inertia.y, 1.f / mass_inertia.z)
	{
	}
};

struct CarBoxListOffset
{
	uint32_t car_box_list_offset;

	CarBoxListOffset()
	{
	}
	CarBoxListOffset(uint32_t _car_box_list_offset) : car_box_list_offset(_car_box_list_offset)
	{
	}
};

struct CarControl
{
	float Y_target;
	float X_target;
	float foward;
	CarControl() : Y_target(0.f), X_target(0.f), foward(0.f)
	{
	}
};

struct CarTarget
{
	glm::vec3 target;
	glm::vec3 last_target;
	bool target_valid = false;
	CarTarget()
	{
	};
	CarTarget(const glm::vec3& _target) : target(_target), last_target(_target)
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

struct BoxListHandle
{
	render::AllocHandle box_list_handle;

	BoxListHandle(render::AllocHandle& handle)
	{
		box_list_handle = std::move(handle);
	}
};

struct CarBuildingsCache
{
	struct CachedBuilding
	{
		glm::vec3 position;
		glm::vec3 extent;
		float size;
	};

	static constexpr uint32_t kNumCachedBuildings = 4;
	std::array<CachedBuilding, kNumCachedBuildings> buildings;
};

//ECS definition
using BoxType = ecs::EntityType<BoxGPUHandle, OBBBox, RangeAABB, FlagBox, BoxListHandle>;
using AnimatedBoxType = ecs::EntityType<InterpolatedPosition, BoxGPUHandle, RangeAABB, OBBBox, AnimationBox, FlagBox, BoxListHandle>;
using CarType = ecs::EntityType<OBBBox, Car, CarMovement, CarSettings, CarTarget, CarControl, CarGPUIndex, CarBuildingsCache, FlagBox, CarBoxListOffset>;

using GameComponents = ecs::ComponentList<InterpolatedPosition, BoxGPUHandle, OBBBox, RangeAABB, AnimationBox, BoxListHandle, FlagBox, Car, CarMovement, CarSettings, CarTarget, CarGPUIndex, CarControl, CarBuildingsCache, CarBoxListOffset>;
using GameEntityTypes = ecs::EntityTypeList<BoxType, AnimatedBoxType, CarType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;
using InstanceReference = ecs::InstanceReference;


//GPUBoxInstance
//GPU instance represent a 1 size box
struct GPUBoxInstance
{
	glm::vec3 position; //3
	uint32_t box_list_offset; //1
	
	glm::vec3 extents; //3
	uint32_t instance_id; //1

	glm::quat rotation; //4

	void FillForUpdatePosition(const helpers::OBB& obb_box, uint32_t _box_list_offset)
	{
		position = obb_box.position;
		box_list_offset = _box_list_offset;
	}

	void Fill(const glm::vec3& _position, const glm::vec3& _extents, const glm::quat& _rotation, uint32_t _box_list_offset)
	{
		position = _position;
		box_list_offset = _box_list_offset;
		extents = _extents;
		rotation = _rotation;
	}

	void Fill(const helpers::OBB& obb_box, uint32_t _box_list_offset)
	{
		position = obb_box.position;
		box_list_offset = _box_list_offset;
		extents = obb_box.extents;
		rotation = glm::toQuat(obb_box.rotation);
	}
};

//GPUBox
//The Box are axis oriented boxes and the range is -1 to 1
struct GPUBox
{
	glm::vec3 position; //3
	uint32_t gap;
	glm::vec4 rotation_extent[3]; //12
	glm::vec3 colour; //3
	uint32_t flags; //1

	void Fill(const helpers::OBB& obb_box, const glm::vec3& _colour, const uint32_t& _flags)
	{
		position = obb_box.position;
		rotation_extent[0] =  glm::vec4(obb_box.rotation[0][0], obb_box.rotation[0][1], obb_box.rotation[0][2], obb_box.extents.x);
		rotation_extent[1] =  glm::vec4(obb_box.rotation[1][0], obb_box.rotation[1][1], obb_box.rotation[1][2], obb_box.extents.y);
		rotation_extent[2] =  glm::vec4(obb_box.rotation[2][0], obb_box.rotation[2][1], obb_box.rotation[2][2], obb_box.extents.z);

		colour = _colour;
		flags = _flags;
	}
};



//Point of view custom data associated to box city
struct BoxCityCustomPointOfViewData
{
	uint32_t instance_lists_offset;
	uint32_t num_instance_lists;
};

#endif //BOX_CITY_COMPONENTS_H