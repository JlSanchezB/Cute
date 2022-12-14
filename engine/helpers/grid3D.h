#ifndef GRID3D_H_
#define GRID3D_H_

#include <array>

namespace helpers
{
	//A 3d grid with better cache access
	template<typename T, uint32_t DIM_X, uint32_t DIM_Y, uint32_t DIM_Z, uint32_t TILE_SIZE = 4>
	class Grid3D
	{
		static_assert(DIM_X % TILE_SIZE == 0);
		static_assert(DIM_Y % TILE_SIZE == 0);
		static_assert(DIM_Z % TILE_SIZE == 0);
	public:
		const T& Get(uint32_t x, uint32_t y, uint32_t z) const
		{
			return m_data[CalculateOffset(x, y, z)];
		}
		T& Get(uint32_t x, uint32_t y, uint32_t z)
		{
			return m_data[CalculateOffset(x, y, z)];
		}
	private:
		size_t CalculateOffset(uint32_t x, uint32_t y, uint32_t z) const
		{
			uint32_t tile_x = x / TILE_SIZE;
			uint32_t tile_y = y / TILE_SIZE;
			uint32_t tile_z = z / TILE_SIZE;

			uint32_t sub_x = x % TILE_SIZE;
			uint32_t sub_y = y % TILE_SIZE;
			uint32_t sub_z = z % TILE_SIZE;

			size_t tile_offset = tile_x + tile_y * DIM_X / TILE_SIZE + tile_z * (DIM_X / TILE_SIZE) * (DIM_Y / TILE_SIZE);
			size_t sub_offset = sub_x + sub_y * TILE_SIZE + sub_z * TILE_SIZE * TILE_SIZE;

			return tile_offset * TILE_SIZE * TILE_SIZE * TILE_SIZE + sub_offset;
		}

		std::array<T, DIM_X * DIM_Y * DIM_Z> m_data;
	};
}

#endif //GRID3D_H_