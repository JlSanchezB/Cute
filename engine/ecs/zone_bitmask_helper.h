//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation of basic zone systems for the ECS
//////////////////////////////////////////////////////////////////////////

#include <bitset>
#include <ecs/entity_component_common.h>
#include <algorithm>

namespace ecs
{
	//2D grid, big objects will use zero zone
	//DESCRIPTOR is a class with the parameters of the grid
	template<typename DESCRIPTOR>
	struct GridOneLevel
	{
		static constexpr uint16_t side_count = DESCRIPTOR::side_count;
		static constexpr uint16_t zone_count = 1 + side_count * side_count;
		static constexpr float world_top = DESCRIPTOR::world_top;
		static constexpr float world_bottom = DESCRIPTOR::world_bottom;
		static constexpr float world_left = DESCRIPTOR::world_left;
		static constexpr float world_right = DESCRIPTOR::world_right;
		static constexpr float object_zero_zone_max_size = DESCRIPTOR::object_zero_zone_max_size;

		using BitSet = std::bitset<zone_count>;

		//Get index in the grid for this position
		static std::pair<uint16_t, uint16_t> GetIndex(float x, float y)
		{
			float range_x = std::clamp((x - world_left) / (world_right - world_left), 0.f, 1.f);
			float range_y = std::clamp((y - world_bottom) / (world_top - world_bottom), 0.f, 1.f);

			//Convert to grid index
			constexpr uint16_t max_index = side_count - 1;
			uint16_t index_x = std::clamp(static_cast<uint16_t>(floorf(range_x * side_count)), static_cast<uint16_t>(0), max_index);
			uint16_t index_y = std::clamp(static_cast<uint16_t>(floorf(range_y * side_count)), static_cast<uint16_t>(0), max_index);

			//Return index
			return std::pair<uint16_t, uint16_t>(index_x, index_y);
		}

		//Get zone index
		static uint16_t GetZoneLinealIndex(uint16_t index_x, uint16_t index_y)
		{
			return 1 + index_x + index_y * side_count;
		}

		//Get zone
		static ZoneType GetZone(float x, float y, float radius)
		{
			if (radius < object_zero_zone_max_size)
			{
				const auto& index = GetIndex(x, y);
				return GetZoneLinealIndex(index.first, index.second);
			}
			else
			{
				//Use the zero zone for big objects
				return 0;
			}
		}

		//Calculate BitSet influence
		static BitSet CalculateInfluence(float x, float y, float radius)
		{
			BitSet bit_set;
			bit_set.set(0, true);

			//Calculate incluence
			const auto& begin = GetIndex(x - radius - object_zero_zone_max_size, y - radius - object_zero_zone_max_size);
			const auto& end = GetIndex(x + radius + object_zero_zone_max_size, y + radius + object_zero_zone_max_size);

			//Set the range
			for (uint16_t i = begin.first; i <= end.first; ++i)
			{
				for (uint16_t j = begin.second; j <= end.second; ++j)
				{
					bit_set.set(GetZoneLinealIndex(i, j), true);
				}
			}

			return bit_set;
		}

		static BitSet All()
		{
			BitSet bit_set;
			bit_set.set();
			return bit_set;
		}
	};
}