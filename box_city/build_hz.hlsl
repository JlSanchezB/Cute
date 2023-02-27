

//Indirect parameters for draw call
RWTexture2D<float> hz_texture : register(u0);
Texture2D<float> scene_depth : register(t0);

groupshared uint min_z_value;

//Naive implementation for culling
//Each group will get one instance list
[numthreads(8, 8, 1)]
void build_hz(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID, uint3 thread_id : SV_DispatchThreadID)
{
	if (group_thread_id.x == 0 && group_thread_id.y == 0)
	{
		min_z_value = asuint(1.f);
	}
	GroupMemoryBarrier();
	//Reduce from scene depth to first mip of the hz using 8x8 grid
	
	uint width, height, num_levels;
	scene_depth.GetDimensions(0, width, height, num_levels);

	if (thread_id.x < width && thread_id.y < height)
	{
		float value = scene_depth[thread_id.xy];
		uint value_before;
		InterlockedMin(min_z_value, asuint(value), value_before);
	}

	GroupMemoryBarrier();
	//Write result in the hz texture
	if (group_thread_id.x == 0 && group_thread_id.y == 0)
	{
		hz_texture[group.xy] = asfloat(min_z_value);
	}

	//Sync

	//Build the rest of the mips
};
