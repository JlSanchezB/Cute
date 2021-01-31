//////////////////////////////////////////////////////////////////////////
// Cute engine - resources for the ECS test
//////////////////////////////////////////////////////////////////////////
#ifndef ECS_RESOURCES_h
#define ECS_RESOURCES_h

struct DisplayResource
{
	display::VertexBufferHandle m_box_vertex_buffer;
	display::IndexBufferHandle m_box_index_buffer;


	void Load(display::Device* device)
	{
		//Box Vertex buffer
		{
			struct VertexData
			{
				float position[3];
			};

			VertexData vertex_data[8] =
			{
				{-1.f, -1.f, -1.f},
				{1.f, -1.f, -1.f},
				{-1.f, 1.f, -1.f},
				{1.f, 1.f, -1.f},
				{1.f, 1.f, 1.f},
				{1.f, 1.f, 1.f},
				{1.f, 1.f, 1.f},
				{1.f, 1.f, 1.f}
			};

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_data;
			vertex_buffer_desc.size = sizeof(vertex_data);
			vertex_buffer_desc.stride = sizeof(VertexData);

			m_box_vertex_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "box_vertex_buffer");
		}

		//Quad Index buffer
		{
			uint16_t index_buffer_data[36] = {	0, 4, 1, 1, 4, 5,
												1, 5, 3, 3, 5, 7,
												3, 7, 2, 2, 7, 6,
												2, 6, 0, 0, 6, 4,
												4, 5, 6, 5, 6, 7,
												0, 2, 1, 1, 3, 2
			};
			display::IndexBufferDesc index_buffer_desc;
			index_buffer_desc.init_data = index_buffer_data;
			index_buffer_desc.size = sizeof(index_buffer_data);

			m_box_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "box_index_buffer");
		}
	}

	void Unload(display::Device* device)
	{
		display::DestroyHandle(device, m_box_vertex_buffer);
		display::DestroyHandle(device, m_box_index_buffer);
	}
};

#endif
