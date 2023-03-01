

//Indirect parameters for draw call
RWTexture2D<float> hz_texture : register(u0);
Texture2D<float> scene_depth : register(t0);

groupshared uint min_z_value_mip[8*8 + 4*4 + 2*2 + 1];
static const uint mip0_offset = 0;
static const uint mip1_offset = 8 * 8;
static const uint mip2_offset = 8 * 8 + 4 * 4;
static const uint mip3_offset = 8 * 8 + 4 * 4 + 2 * 2;
static const uint mip_size = 8 * 8 + 4 * 4 + 2 * 2 + 1;
//Each group will handle 64x64 pixels
//Number of threads is 16x16, 256
//Each thread will handle 4x4 texels from the source, 16 * 4 = 64, 16 * 16 * 4 * 4 = 64 * 64
//Each group will write 8x8 for mip 0, 4x4 for mip1, 2x2 for mip2 and 1x1 to mip3
[numthreads(256, 1, 1)]
void build_hz(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
	//Clear the groupshaded data
	if (group_thread_id.x < mip_size)
	{
		min_z_value_mip[group_thread_id.x] = asuint(1.f);
	}

	GroupMemoryBarrierWithGroupSync();

	
	uint width, height, num_levels;
	scene_depth.GetDimensions(0, width, height, num_levels);

	float min_value = 1.f;

	uint2 quad = uint2(group_thread_id.x % 16, group_thread_id.x / 16);
	uint2 texture_access;
	texture_access = group.xy * 64 + quad * 4;

	[[unroll]]
	for (uint i = 0; i < 4; ++i)
	{
		[[unroll]]
		for (uint j = 0; j < 4; ++j)
		{
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
	if (group_thread_id.x < mip1_offset)
	{
		uint2 coords = uint2(group_thread_id.x % 8, group_thread_id.x / 8);
		output_coords = group.xy * 8 + coords;
		output_value = asfloat(min_z_value_mip[coords.x + coords.y * 8]);
		write_out = true;
	}
	else if (group_thread_id.x < mip2_offset)
	{
		uint2 offset = uint2(512, 0);
		uint local_thread_id = group_thread_id.x - mip1_offset;
		uint2 coords = uint2(local_thread_id % 4, local_thread_id / 4);
		output_coords = offset + group.xy * 4 + coords;
		output_value = asfloat(min_z_value_mip[mip1_offset + coords.x + coords.y * 4]);
		write_out = true;
	}
	else if (group_thread_id.x < mip3_offset)
	{
		uint2 offset = uint2(512, 256);
		uint local_thread_id = group_thread_id.x - mip2_offset;
		uint2 coords = uint2(local_thread_id % 2, local_thread_id / 2);
		output_coords = offset + group.xy * 2 + coords;
		output_value = asfloat(min_z_value_mip[mip2_offset + coords.x + coords.y * 2]);
		write_out = true;
	}
	else if (group_thread_id.x < mip_size)
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
	//From 16x16 -> 4x4, mip1
/*	else if (group_thread_id.x < mip2_offset)
	{
		uint2 offset(512, 0);
		uint local_thread_id = group_thread_id.x - mip1_offset;
		uint2 coords = uint2(local_thread_id % 4, local_thread_id / 4);
		output_coords = offset uint2(group.xy * 8 + coords);
		output_value = asfloat(min_z_value_mip[coords.x + coords.y * 8]);
	}

	hz_texture[group.xy * 8 + group_thread_id.xy] = asfloat(min_z_value_mip0[group_thread_id.x][group_thread_id.y]);

	if (group_thread_id.x < 4 && group_thread_id.y < 4)
	{
		uint2 offset(512, 0);
		offset.x = 512;
		offset.y = 0;
		hz_texture[offset + group.xy * 4 + group_thread_id.xy] = asfloat(min_z_value_mip1[group_thread_id.x][group_thread_id.y]);
	}
	//from 16x16 to 2x2, mip2
	if (group_thread_id.x < 2 && group_thread_id.y < 2)
	{
		uint2 offset;
		offset.x = 512;
		offset.y = 256;
		hz_texture[offset + group.xy * 2 + group_thread_id.xy] = asfloat(min_z_value_mip2[group_thread_id.x][group_thread_id.y]);
	}
	//from 16x16 to 1x1, mip3
	if (group_thread_id.x < 1 && group_thread_id.y < 1)
	{
		uint2 offset;
		offset.x = 512;
		offset.y = 256 + 128;
		hz_texture[offset + group.xy + group_thread_id.xy] = asfloat(min_z_value_mip3);
	}
	*/
	//Just leave one group alive and
	//Build the rest of the mips, from width/64 x height/64 to 1x1
};
