#ifndef BOX_CITY_COMPONENTS_H
#define BOX_CITY_COMPONENTS_H

#include <helpers/collision.h>
#include <render/render.h>
#include <ecs/entity_component_system.h>
#include <render_module/render_module_gpu_memory.h>
#include <ext/glm/ext.hpp>
#include <core/platform.h>
#include <core/slot_pool.h>

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
	BoxGPUHandle()
	{
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

	BoxListHandle()
	{
	}
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

struct InstanceIndex
{
	inline static core::SlotPool<uint32_t, 500000> slot_pool;
	core::Slot<uint32_t> slot;

	InstanceIndex()
	{
	}
	~InstanceIndex()
	{
		slot_pool.Free(slot);
	}
	InstanceIndex(InstanceIndex&& instance_index)
	{
		slot = std::move(instance_index.slot);
	}
	InstanceIndex& operator=(InstanceIndex&& instance_index)
	{
		slot = std::move(instance_index.slot);
		return *this;
	}
	InstanceIndex(core::Slot<uint32_t>&& slot) : slot(std::move(slot))
	{
	}
};

//ECS definition
using BoxType = ecs::EntityType<BoxGPUHandle, OBBBox, RangeAABB, FlagBox, BoxListHandle, InstanceIndex>;
using AnimatedBoxType = ecs::EntityType<InterpolatedPosition, BoxGPUHandle, RangeAABB, OBBBox, AnimationBox, FlagBox, BoxListHandle, InstanceIndex>;
using CarType = ecs::EntityType<OBBBox, Car, CarMovement, CarSettings, CarTarget, CarControl, CarGPUIndex, CarBuildingsCache, FlagBox, CarBoxListOffset, InstanceIndex>;

using GameComponents = ecs::ComponentList<InterpolatedPosition, BoxGPUHandle, OBBBox, RangeAABB, AnimationBox, BoxListHandle, FlagBox, Car, CarMovement, CarSettings, CarTarget, CarGPUIndex, CarControl, CarBuildingsCache, CarBoxListOffset, InstanceIndex>;
using GameEntityTypes = ecs::EntityTypeList<BoxType, AnimatedBoxType, CarType>;

using GameDatabase = ecs::DatabaseDeclaration<GameComponents, GameEntityTypes>;
using Instance = ecs::Instance<GameDatabase>;
using InstanceReference = ecs::InstanceReference;

//Set friendly names
template<> inline const char* ecs::GetTypeDebugName<InterpolatedPosition>() { return "InterpolatedPosition"; };
template<> inline const char* ecs::GetTypeDebugName<BoxGPUHandle>() { return "BoxGPUHandle"; };
template<> inline const char* ecs::GetTypeDebugName<OBBBox>() { return "OBBBox"; };
template<> inline const char* ecs::GetTypeDebugName<RangeAABB>() { return "RangeAABB"; };
template<> inline const char* ecs::GetTypeDebugName<AnimationBox>() { return "AnimationBox"; };
template<> inline const char* ecs::GetTypeDebugName<BoxListHandle>() { return "BoxListHandle"; };
template<> inline const char* ecs::GetTypeDebugName<FlagBox>() { return "FlagBox"; };
template<> inline const char* ecs::GetTypeDebugName<Car>() { return "Car"; };
template<> inline const char* ecs::GetTypeDebugName<CarMovement>() { return "CarMovement"; };
template<> inline const char* ecs::GetTypeDebugName<CarSettings>() { return "CarSettings"; };
template<> inline const char* ecs::GetTypeDebugName<CarGPUIndex>() { return "CarGPUIndex"; };
template<> inline const char* ecs::GetTypeDebugName<CarControl>() { return "CarControl"; };
template<> inline const char* ecs::GetTypeDebugName<CarBuildingsCache>() { return "CarBuildingsCache"; };
template<> inline const char* ecs::GetTypeDebugName<CarBoxListOffset>() { return "CarBoxListOffset"; };
template<> inline const char* ecs::GetTypeDebugName<InstanceIndex>() { return "InstanceIndex"; };

template<> inline const char* ecs::GetTypeDebugName<BoxType>() { return "BoxType"; };
template<> inline const char* ecs::GetTypeDebugName<AnimatedBoxType>() { return "AnimatedBoxType"; };
template<> inline const char* ecs::GetTypeDebugName<CarType>() { return "CarType"; };

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
	uint32_t gap_1;
	glm::vec3 extent; //3
	uint32_t gap_2;
	glm::vec3 colour; //3
	uint32_t flags; //1

	void Fill(const glm::vec3& _position, const glm::vec3 _extent, const glm::vec3& _colour, const uint32_t& _flags)
	{
		position = _position;
		extent = _extent;
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