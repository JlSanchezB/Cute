

//Indirect parameters for draw call
RWTexture2D<float> hz_texture : register(u0);
Texture2D<float> scene_depth : register(t0);

//groupshared uint min_z_value_mip2;
groupshared uint min_z_value_mip3;
groupshared uint min_z_value_mip2[2][2];
groupshared uint min_z_value_mip1[4][4];
groupshared uint min_z_value_mip0[8][8];

//Each group will handle 64x64 pixels
//Number of threads is 16x16, 256
//Each thread will handle 4x4 texels from the source, 16 * 4 = 64, 16 * 16 * 4 * 4 = 64 * 64
//Each group will write 8x8 for mip 0, 4x4 for mip1, 2x2 for mip2 and 1x1 to mip3
[numthreads(16, 16, 1)]
void build_hz(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID, uint3 thread_id : SV_DispatchThreadID)
{
	if (group_thread_id.x < 8 && group_thread_id.y < 8)
	{
		min_z_value_mip0[group_thread_id.x][group_thread_id.y] = asuint(1.f);
	}
	if (group_thread_id.x < 4 && group_thread_id.y < 4)
	{
		min_z_value_mip1[group_thread_id.x][group_thread_id.y] = asuint(1.f);
	}
	if (group_thread_id.x < 2 && group_thread_id.y < 2)
	{
		min_z_value_mip2[group_thread_id.x][group_thread_id.y] = asuint(1.f);
	}
	if (group_thread_id.x == 0 && group_thread_id.y == 0)
	{
		min_z_value_mip3 = asuint(1.f);
	}
	GroupMemoryBarrierWithGroupSync();

	
	uint width, height, num_levels;
	scene_depth.GetDimensions(0, width, height, num_levels);

	float min_value = 1.f;
	[[unroll]]
	for (uint i = 0; i < 4; ++i)
	{
		[[unroll]]
		for (uint j = 0; j < 4; ++j)
		{
			//Calculate the index
			uint2 thread_texel;
			thread_texel.x = thread_id.x * 4 + i;
			thread_texel.y = thread_id.y * 4 + j;

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
		InterlockedMin(min_z_value_mip0[group_thread_id.x / 2][group_thread_id.y / 2], asuint(min_value), value_before);
		InterlockedMin(min_z_value_mip1[group_thread_id.x / 4][group_thread_id.y / 4], asuint(min_value), value_before);
		InterlockedMin(min_z_value_mip2[group_thread_id.x / 8][group_thread_id.y / 8], asuint(min_value), value_before);
		InterlockedMin(min_z_value_mip3, asuint(min_value), value_before);
	}

	GroupMemoryBarrierWithGroupSync();
	
	//Write result in the hz texture

	//from 16x16 -> 8x8 the top mip
	if (group_thread_id.x < 8 && group_thread_id.y < 8)
	{
		hz_texture[group.xy * 8 + group_thread_id.xy] = asfloat(min_z_value_mip0[group_thread_id.x][group_thread_id.y]);
	}
	//From 16x16 -> 4x4, mip1
	if (group_thread_id.x < 4 && group_thread_id.y < 4)
	{
		uint2 offset;
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

	//Just leave one group alive and
	//Build the rest of the mips, from width/64 x height/64 to 1x1
};
