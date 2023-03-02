

//Indirect parameters for draw call
RWTexture2D<float> hz_texture : register(u0);
Texture2D<float> scene_depth : register(t0);
RWStructuredBuffer<uint> atomic_buffer : register(u1);

groupshared uint min_z_value_mip[8*8 + 4*4 + 2*2 + 1];
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

	GroupMemoryBarrierWithGroupSync();

	uint2 quad = uint2(thread_index % 16, thread_index / 16);

	{
		uint width, height, num_levels;
		scene_depth.GetDimensions(0, width, height, num_levels);

		float min_value = 1.f;

		uint2 texture_access;
		texture_access = group.xy * 64 + quad * 4;

		[unroll]
		for (uint texel_index = 0; texel_index < 16; ++texel_index)
		{
			//Morton codes
			uint i = (((texel_index >> 2) & 0x0007) & 0xFFFE) | (texel_index & 0x0001);
			uint j = ((texel_index >> 1) & 0x0003) | (((texel_index >> 3) & 0x0007) & 0xFFFC);

			//Calculate the index
			uint2 thread_texel;
			thread_texel.x = texture_access.x + i;
			thread_texel.y = texture_access.y + j;

			//Adjust to the current resolution, it is design for the worst case 4096x4096
			thread_texel.x = thread_texel.x * width / 4096;
			thread_texel.y = thread_texel.y * height / 4096;

			thread_texel.x = min(thread_texel.x, width - 1);
			thread_texel.y = min(thread_texel.y, height - 1);

			float value = scene_depth[thread_texel.xy];
			min_value = min(min_value, value);
		}

		{
			//Calculate the min value for all the mipmaps levels that is generating at the same time
			uint value_before;
			InterlockedMin(min_z_value_mip[mip0_offset + (quad.x / 2) + (quad.y / 2) * 8], asuint(min_value), value_before);
			InterlockedMin(min_z_value_mip[mip1_offset + (quad.x / 4) + (quad.y / 4) * 4], asuint(min_value), value_before);
			InterlockedMin(min_z_value_mip[mip2_offset + (quad.x / 8) + (quad.y / 8) * 2], asuint(min_value), value_before);
			InterlockedMin(min_z_value_mip[mip3_offset], asuint(min_value), value_before);
		}

		GroupMemoryBarrierWithGroupSync();

		//Write result in the hz texture

		uint2 output_coords;
		float output_value;
		bool write_out = false;
		//From 16x16 -> 8x8 the top mip
		[flatten] if (thread_index < mip1_offset)
		{
			uint2 coords = uint2(thread_index % 8, thread_index / 8);
			output_coords = group.xy * 8 + coords;
			output_value = asfloat(min_z_value_mip[coords.x + coords.y * 8]);
			write_out = true;
		}
		else [flatten] if (thread_index < mip2_offset)
		{
			uint2 offset = uint2(512, 0);
			uint local_thread_id = thread_index - mip1_offset;
			uint2 coords = uint2(local_thread_id % 4, local_thread_id / 4);
			output_coords = offset + group.xy * 4 + coords;
			output_value = asfloat(min_z_value_mip[mip1_offset + coords.x + coords.y * 4]);
			write_out = true;
		}
		else [flatten] if (thread_index < mip3_offset)
		{
			uint2 offset = uint2(512, 256);
			uint local_thread_id = thread_index - mip2_offset;
			uint2 coords = uint2(local_thread_id % 2, local_thread_id / 2);
			output_coords = offset + group.xy * 2 + coords;
			output_value = asfloat(min_z_value_mip[mip2_offset + coords.x + coords.y * 2]);
			write_out = true;
		}
		else [flatten] if (thread_index < mip_size)
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
		atomic_buffer[0] = 0; //Needs to be clean for the next dispatch

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

		GroupMemoryBarrierWithGroupSync();

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
		{
			//Mip1, 16x16 and build the mip2,3,4,5 in the group shared memory
			uint2 offset = uint2(512, 256 + 128 + 64 + 32);
			hz_texture[offset + quad.xy] = thread_local_mip1;

			//Use the group shared for building mip2, mip3, mip4, mip5
			uint value_before;
			InterlockedMin(min_z_value_mip[mip0_offset + (quad.x / 2) + (quad.y / 2) * 8], asuint(thread_local_mip1), value_before);
			InterlockedMin(min_z_value_mip[mip1_offset + (quad.x / 4) + (quad.y / 4) * 4], asuint(thread_local_mip1), value_before);
			InterlockedMin(min_z_value_mip[mip2_offset + (quad.x / 8) + (quad.y / 8) * 2], asuint(thread_local_mip1), value_before);
			InterlockedMin(min_z_value_mip[mip3_offset], asuint(thread_local_mip1), value_before);
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
			uint2 coords = uint2(thread_index % 8, thread_index / 8);
			output_coords = offset + coords;
			output_value = asfloat(min_z_value_mip[coords.x + coords.y * 8]);
			write_out = true;
		}
		//4x4
		else [flatten] if (thread_index < mip2_offset)
		{
			uint2 offset = uint2(512, 256 + 128 + 64 + 32 + 16 + 8);
			uint local_thread_id = thread_index - mip1_offset;
			uint2 coords = uint2(local_thread_id % 4, local_thread_id / 4);
			output_coords = offset + coords;
			output_value = asfloat(min_z_value_mip[mip1_offset + coords.x + coords.y * 4]);
			write_out = true;
		}
		//2x2
		else[flatten] if (thread_index < mip3_offset)
		{
			uint2 offset = uint2(512, 256 + 128 + 64 + 32 + 16 + 8 + 4);
			uint local_thread_id = thread_index - mip2_offset;
			uint2 coords = uint2(local_thread_id % 2, local_thread_id / 2);
			output_coords = offset + coords;
			output_value = asfloat(min_z_value_mip[mip2_offset + coords.x + coords.y * 2]);
			write_out = true;
		}
		//1x1
		else [flatten] if (thread_index < mip_size)
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
