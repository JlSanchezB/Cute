//////////////////////////////////////////////////////////////////////////
// Cute engine - Implementation of basic zone systems for the ECS
//////////////////////////////////////////////////////////////////////////

#include <bitset>
#include <ecs/entity_component_common.h>

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
			float range_x = (x - world_left) / (world_right - world_left);
			float range_y = (x - world_bottom) / (world_top - world_bottom);

			//Convert to grid index
			uint16_t index_x = std::min(static_cast<uint16_t>(range_x * side_count), side_count);
			uint16_t index_y = std::min(static_cast<uint16_t>(range_x * side_count), side_count);

			//Return index
			return std::make_pair<uint16_t, uint16_t>(index_x, index_y);
		}

		//Get zone
		static ZoneType GetZone(float x, float y, float radius)
		{
			if (radius < object_zero_zone_max_size)
			{
				auto& index = GetIndex(x, y);
				return 1 + index.first + index.second * side_count;
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
			auto& begin = GetIndex(x - radius - object_zero_zone_max_size, y - radius - object_zero_zone_max_size);
			auto& end = GetIndex(x + radius + object_zero_zone_max_size, y + radius + object_zero_zone_max_size);

			//Set the range
			for (uint16_t i = begin.first; i <= end.first; ++i)
			{
				for (uint16_t j = begin.second; j <= end.second; ++j)
				{
					bit_set.set(1 + i + j, true);
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