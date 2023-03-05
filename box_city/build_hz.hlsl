

//Indirect parameters for draw call
globallycoherent RWTexture2D<float> hz_texture : register(u0);
Texture2D<float> scene_depth : register(t0);
globallycoherent RWStructuredBuffer<uint> atomic_buffer : register(u1);

#define REDUCTION_METHOD

#ifdef REDUCTION_METHOD

groupshared uint group_dispatch_index;

uint2 GetMipOffset(uint mip_index)
{
	if (mip_index == 0)
	{
		return uint2(0, 0);
	}
	return uint2(512, 512 - 1024 / exp2(mip_index));
}

//The first reduction needs a lot of kernel size
[numthreads(8, 8, 1)]
void build_hz(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
	uint width, height, num_levels;
	scene_depth.GetDimensions(0, width, height, num_levels);

	{
		//Each thread builds one pixel in the 512x512 HiZ mip0
		float min_value = 1.f;

		uint2 source_min = group.xy * 8 + group_thread_id.xy;
		uint2 source_max = min(group.xy * 8 + group_thread_id.xy + uint2(1, 1), 511);

		uint2 dest_min = uint2(float2(source_min) * float2(float(width) / 512.f, float(height) / 512.f));
		uint2 dest_max = uint2(float2(source_max) * float2(float(width) / 512.f, float(height) / 512.f));

		uint2 range = uint2(dest_max - dest_min) + uint2(1, 1);

#define KERNEL(size_x, size_y) \
		[branch] if (range.x <= size_x && range.y <= size_y)\
		{\
			float2 step = (dest_max - dest_min) / float2(size_x, size_y);\
			[unroll] for (uint i = 0; i < size_x; ++i)\
				[unroll] for (uint j = 0; j < size_y; ++j)\
			{\
				float value = scene_depth[dest_min + uint2(i,j) * step];\
				min_value = min(min_value, value);\
			}\
		}

		//Have special unroll look for each kernel size
			KERNEL(4,3)
		else
			KERNEL(4,4)
		else
			KERNEL(5,4)
		else
			KERNEL(5,5)
		else
			KERNEL(6,5)
		else
			KERNEL(6, 6)
		else
			KERNEL(7, 7)
		else
			KERNEL(8, 7)
		else
			KERNEL(8, 8)

		//Write output
		hz_texture[source_min] = min_value;
	}
	DeviceMemoryBarrier();

	//The result is a 512x512
	//Mip 1 is 256x256 (32x8), 32x32 groups need to work
	//Mip 2 is 128x128 (16x8), 16x16 groups need to work
	//Mip 3 is 64x64, 8x8 groups working
	//Mip 4 is 32x32, 4x4 groups working
	//Mip 5 is 16x16, 2x2 groups working
	//Mip 6 is 8x8, 1 groups working, 8x8 threads
	//Mip 7 is 4x4, 1 group working, 4x4 threads
	//Mip 8 is 2x2, 1 group working, 2x2 threads
	//Mip 9 is 1x1, 1 group working, 1x1 threads

	//Now to reduce the rest of the mips

	uint2 group_reduced = group.xy / 2;
	uint mip_num_groups = 32;

	[unroll]
	for (uint mip_index = 1; mip_index < 10; ++mip_index)
	{
		const uint atomic_step = 0xFFFFFFFF / 4 + 1; //Adding this step, each 4 will go to zero
		//Reduce the groups as needed
		if (group_thread_id.x == 0 && group_thread_id.y == 0 && mip_num_groups != 0) //We only reduce when we have 2x2 groups or more
		{
			//Each group will increment the atomic to know how many groups have passed
			InterlockedAdd(atomic_buffer[group_reduced.x + group_reduced.y * mip_num_groups], atomic_step, group_dispatch_index);
		}
		GroupMemoryBarrierWithGroupSync();

		if (group_dispatch_index != atomic_step * 3) return; //Only the last one survive, that when the prev value was atomic_step * 3

		uint2 dest_tex_coords = group_reduced * 8 + group_thread_id.xy;

		//Read the 4 values
		float value0 = hz_texture[GetMipOffset(mip_index - 1) + dest_tex_coords.xy * 2 + uint2(0, 0)];
		float value1 = hz_texture[GetMipOffset(mip_index - 1) + dest_tex_coords.xy * 2 + uint2(0, 1)];
		float value2 = hz_texture[GetMipOffset(mip_index - 1) + dest_tex_coords.xy * 2 + uint2(1, 0)];
		float value3 = hz_texture[GetMipOffset(mip_index - 1) + dest_tex_coords.xy * 2 + uint2(1, 1)];

		float min_value = min(value0, min(value1, min(value2, value3)));

		//Write result in the correct offset
		hz_texture[GetMipOffset(mip_index) + dest_tex_coords.xy] = min_value;

		group_reduced = group_reduced / 2;
		mip_num_groups = mip_num_groups / 2;

		DeviceMemoryBarrier();
	}
}


#else

groupshared uint min_z_value_mip[8 * 8 + 4 * 4 + 2 * 2 + 1];
static const uint mip0_offset = 0;
static const uint mip1_offset = 8 * 8;
static const uint mip2_offset = 8 * 8 + 4 * 4;
static const uint mip3_offset = 8 * 8 + 4 * 4 + 2 * 2;
static const uint mip_size = 8 * 8 + 4 * 4 + 2 * 2 + 1;
groupshared uint group_dispatch_index;

//Each group will handle 64x64 pixels
//Number of threads is 16x16, 256
//Each thread will handle 4x4 texels from the source, 16 * 4 = 64, 16 * 16 * 4 * 4 = 64 * 64
//Each group will write 8x8 for mip 0, 4x4 for mip1, 2x2 for mip2 and 1x1 to mip3
[numthreads(256, 1, 1)]
void build_hz(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
	uint thread_index = group_thread_id.x;

	//Clear the groupshaded data
	if (thread_index < mip_size)
	{
		min_z_value_mip[thread_index] = asuint(1.f);
	}

	uint2 quad = uint2(thread_index % 16, thread_index / 16);

	{
		uint width, height, num_levels;
		scene_depth.GetDimensions(0, width, height, num_levels);

		float min_value = 1.f;

		uint2 source_min = group.xy * 64 + quad * 4;
		uint2 source_max = group.xy * 64 + quad * 4 + uint2(1, 1);

		uint2 dest_min = uint2(float2(source_min) * float2(float(width) / 4096.f, float(height) / 4096.f));
		uint2 dest_max = uint2(float2(source_max) * float2(float(width) / 4096.f, float(height) / 4096.f));

		uint2 range = dest_max - dest_min + uint2(1, 1);

		float2 step = (dest_max - dest_min) / float2(4.f, 3.f);

		[unroll] for (uint i = 0; i < 4; ++i)
			[unroll] for (uint j = 0; j < 3; ++j)
		{
			//Calculate the index
			uint2 thread_texel;
			thread_texel.x = dest_min.x + i * step.x;
			thread_texel.y = dest_min.y + j * step.y;

			thread_texel.x = min(thread_texel.x, width - 1);
			thread_texel.y = min(thread_texel.y, height - 1);

			float value = scene_depth[thread_texel.xy];
			min_value = min(min_value, value);
		}

		GroupMemoryBarrierWithGroupSync();

		{
			//Calculate the min value for all the mipmaps levels that is generating at the same time
			uint value_before;
			InterlockedMin(min_z_value_mip[mip0_offset + (quad.x / 2) + (quad.y / 2) * 8], asuint(min_value), value_before);
			uint value_before2;
			InterlockedMin(min_z_value_mip[mip1_offset + (quad.x / 4) + (quad.y / 4) * 4], asuint(min_value), value_before2);
			uint value_before3;
			InterlockedMin(min_z_value_mip[mip2_offset + (quad.x / 8) + (quad.y / 8) * 2], asuint(min_value), value_before3);
			uint value_before4;
			InterlockedMin(min_z_value_mip[mip3_offset], asuint(min_value), value_before4);
		}

		GroupMemoryBarrierWithGroupSync();

		//Write result in the hz texture

		uint2 output_coords;
		float output_value;
		bool write_out = false;
		//From 16x16 -> 8x8 the top mip
		[flatten] if (thread_index < mip1_offset)
		{
			uint2 coords;
			coords.x = (((thread_index >> 2) & 0x0007) & 0xFFFE) | (thread_index & 0x0001);
			coords.y = ((thread_index >> 1) & 0x0003) | (((thread_index >> 3) & 0x0007) & 0xFFFC);
			output_coords = group.xy * 8 + coords;
			output_value = asfloat(min_z_value_mip[coords.x + coords.y * 8]);
			write_out = true;
		}
		else[flatten] if (thread_index < mip2_offset)
		{
			uint2 offset = uint2(512, 0);
			uint local_thread_id = thread_index - mip1_offset;
			uint2 coords;
			coords.x = (((local_thread_id >> 2) & 0x0007) & 0xFFFE) | (local_thread_id & 0x0001);
			coords.y = ((local_thread_id >> 1) & 0x0003) | (((local_thread_id >> 3) & 0x0007) & 0xFFFC);
			output_coords = offset + group.xy * 4 + coords;
			output_value = asfloat(min_z_value_mip[mip1_offset + coords.x + coords.y * 4]);
			write_out = true;
		}
		else[flatten] if (thread_index < mip3_offset)
		{
			uint2 offset = uint2(512, 256);
			uint local_thread_id = thread_index - mip2_offset;
			uint2 coords;
			coords.x = (((local_thread_id >> 2) & 0x0007) & 0xFFFE) | (local_thread_id & 0x0001);
			coords.y = ((local_thread_id >> 1) & 0x0003) | (((local_thread_id >> 3) & 0x0007) & 0xFFFC);
			output_coords = offset + group.xy * 2 + coords;
			output_value = asfloat(min_z_value_mip[mip2_offset + coords.x + coords.y * 2]);
			write_out = true;
		}
		else[flatten] if (thread_index < mip_size)
		{
			uint2 offset = uint2(512, 256 + 128);
			output_coords = offset + group.xy;
			output_value = asfloat(min_z_value_mip[mip3_offset]);
			write_out = true;
		}

		if (write_out)
		{
			hz_texture[output_coords] = output_value;
		}
	}

	//Only the last thread will survive
	if (thread_index == 0)
	{
		//Each group will increment the atomic to know how many groups have passed
		uint value;
		InterlockedAdd(atomic_buffer[0], 1, value);
		group_dispatch_index = value; //We need to share it with the rest of the threads in this group
	}
	GroupMemoryBarrierWithGroupSync();

	//The rest of the groups/threads can die here, only the last 16x16 threads of the last group will survive
	if (group_dispatch_index < 64 * 64 - 1) return;


	{
		//Just leave the last group alive to create the last mips
		//Build the rest of the mips, from width/64 x height/64 to 1x1
		//In this point, only the last one can be alive
		if (thread_index == 0)
		{
			atomic_buffer[0] = 0; //Needs to be clean for the next dispatch
		}

		float thread_local_mip0[2][2];
		float thread_local_mip1 = 1.f;

		thread_local_mip0[0][0] = 1.f;
		thread_local_mip0[0][1] = 1.f;
		thread_local_mip0[1][0] = 1.f;
		thread_local_mip0[1][1] = 1.f;

		//Clear the groupshaded data
		if (thread_index < mip_size)
		{
			min_z_value_mip[thread_index] = asuint(1.f);
		}

		//Each thread, 16x16 reads 4x4
		[unroll]
		for (uint texel_index = 0; texel_index < 16; ++texel_index)
		{
			//Morton codes
			uint i = (((texel_index >> 2) & 0x0007) & 0xFFFE) | (texel_index & 0x0001);
			uint j = ((texel_index >> 1) & 0x0003) | (((texel_index >> 3) & 0x0007) & 0xFFFC);

			//Calculate the index
			uint2 thread_texel;
			thread_texel.x = 512 + quad.x * 4 + i;
			thread_texel.y = 256 + 128 + quad.y * 4 + j;

			float value = hz_texture[thread_texel.xy];

			thread_local_mip0[i / 2][j / 2] = min(thread_local_mip0[i / 2][j / 2], value);
			thread_local_mip1 = min(thread_local_mip1, value);
		}

		//Each thread can write the mip0 and mip1
		//Mip0 is 32x32
		{
			uint2 offset = uint2(512, 256 + 128 + 64);
			hz_texture[offset + quad.xy * 2 + uint2(0, 0)] = thread_local_mip0[0][0];
			hz_texture[offset + quad.xy * 2 + uint2(0, 1)] = thread_local_mip0[0][1];
			hz_texture[offset + quad.xy * 2 + uint2(1, 0)] = thread_local_mip0[1][0];
			hz_texture[offset + quad.xy * 2 + uint2(1, 1)] = thread_local_mip0[1][1];
		}

		GroupMemoryBarrierWithGroupSync();
		{
			//Mip1, 16x16 and build the mip2,3,4,5 in the group shared memory
			uint2 offset = uint2(512, 256 + 128 + 64 + 32);
			hz_texture[offset + quad.xy] = thread_local_mip1;

			//Use the group shared for building mip2, mip3, mip4, mip5
			uint value_before;
			InterlockedMin(min_z_value_mip[mip0_offset + (quad.x / 2) + (quad.y / 2) * 8], asuint(thread_local_mip1), value_before);
			uint value_before2;
			InterlockedMin(min_z_value_mip[mip1_offset + (quad.x / 4) + (quad.y / 4) * 4], asuint(thread_local_mip1), value_before2);
			uint value_before3;
			InterlockedMin(min_z_value_mip[mip2_offset + (quad.x / 8) + (quad.y / 8) * 2], asuint(thread_local_mip1), value_before3);
			uint value_before4;
			InterlockedMin(min_z_value_mip[mip3_offset], asuint(thread_local_mip1), value_before4);
		}

		GroupMemoryBarrierWithGroupSync();

		//We distribute the job between the threads to write out the rest of the mips
		uint2 output_coords;
		float output_value;
		bool write_out = false;
		//8x8 the top mip
		[flatten] if (thread_index < mip1_offset)
		{
			uint2 offset = uint2(512, 256 + 128 + 64 + 32 + 16);
			uint2 coords;
			coords.x = (((thread_index >> 2) & 0x0007) & 0xFFFE) | (thread_index & 0x0001);
			coords.y = ((thread_index >> 1) & 0x0003) | (((thread_index >> 3) & 0x0007) & 0xFFFC);
			output_coords = offset + coords;
			output_value = asfloat(min_z_value_mip[coords.x + coords.y * 8]);
			write_out = true;
		}
		//4x4
		else[flatten] if (thread_index < mip2_offset)
		{
			uint2 offset = uint2(512, 256 + 128 + 64 + 32 + 16 + 8);
			uint local_thread_id = thread_index - mip1_offset;
			uint2 coords;
			coords.x = (((local_thread_id >> 2) & 0x0007) & 0xFFFE) | (local_thread_id & 0x0001);
			coords.y = ((local_thread_id >> 1) & 0x0003) | (((local_thread_id >> 3) & 0x0007) & 0xFFFC);
			output_coords = offset + coords;
			output_value = asfloat(min_z_value_mip[mip1_offset + coords.x + coords.y * 4]);
			write_out = true;
		}
		//2x2
		else[flatten] if (thread_index < mip3_offset)
		{
			uint2 offset = uint2(512, 256 + 128 + 64 + 32 + 16 + 8 + 4);
			uint local_thread_id = thread_index - mip2_offset;
			uint2 coords;
			coords.x = (((local_thread_id >> 2) & 0x0007) & 0xFFFE) | (local_thread_id & 0x0001);
			coords.y = ((local_thread_id >> 1) & 0x0003) | (((local_thread_id >> 3) & 0x0007) & 0xFFFC);
			output_coords = offset + coords;
			output_value = asfloat(min_z_value_mip[mip2_offset + coords.x + coords.y * 2]);
			write_out = true;
		}
		//1x1
		else[flatten] if (thread_index < mip_size)
		{
			uint2 offset = uint2(512, 256 + 128 + 64 + 32 + 16 + 8 + 4 + 2);
			output_coords = offset;
			output_value = asfloat(min_z_value_mip[mip3_offset]);
			write_out = true;
		}

		if (write_out)
		{
			hz_texture[output_coords] = output_value;
		}
	}
};

#endif
