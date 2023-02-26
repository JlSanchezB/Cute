

//Indirect parameters for draw call
RWTexture2D<float> hz_texture : register(u0);
Texture2D<float> scene_depth : register(t0);

//Naive implementation for culling
//Each group will get one instance list
[numthreads(8, 8, 1)]
void build_hz(uint3 group : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{

	//Reduce from scene depth to first mip of the hz using 8x8 grid


	//Write result in the hz texture
	if (group_thread_id.x == 0 && group_thread_id.y == 0)
	{
		hz_texture[group.xy] = 1.f;
	}

	//Sync

	//Build the rest of the mips
};
