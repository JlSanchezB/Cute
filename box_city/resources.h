//////////////////////////////////////////////////////////////////////////
// Cute engine - resources for the ECS test
//////////////////////////////////////////////////////////////////////////
#ifndef ECS_RESOURCES_h
#define ECS_RESOURCES_h

struct DisplayResource
{
	display::VertexBufferHandle m_box_vertex_position_buffer;
	display::VertexBufferHandle m_box_vertex_normal_buffer;
	display::IndexBufferHandle m_box_index_buffer;


	void Load(display::Device* device)
	{
		//Box Vertex buffer
		{

			glm::vec3 vertex_position_data[4 * 6];
			glm::vec3 vertex_normal_data[4 * 6];

			//Make the top face
			vertex_position_data[0] = glm::vec3( -1.f, 1.f, 1.f );
			vertex_position_data[1] = glm::vec3(1.f, 1.f, 1.f);
			vertex_position_data[2] = glm::vec3(-1.f, -1.f, 1.f);
			vertex_position_data[3] = glm::vec3(1.f, -1.f, 1.f);
			vertex_normal_data[0] = vertex_normal_data[1] = vertex_normal_data[2] = vertex_normal_data[3] = glm::vec3(0.f, 0.f, 1.f);

			//Make the rest rotating
			for (size_t i = 1; i < 6; ++i)
			{
				glm::mat3x3 rot;
				switch (i)
				{
				case 1:
					rot = glm::rotate(glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));
					break;
				case 2:
					rot = glm::rotate(glm::half_pi<float>(), glm::vec3(0.f, 1.f, 0.f));
					break;
				case 3:
					rot = glm::rotate(-glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));
					break;
				case 4:
					rot = glm::rotate(-glm::half_pi<float>(), glm::vec3(0.f, 1.f, 0.f));
					break;
				case 5:
					rot = glm::rotate(glm::pi<float>(), glm::vec3(1.f, 0.f, 0.f));
					break;
				}

				//Apply rotation
				vertex_position_data[i * 4 + 0] = rot * vertex_position_data[0];
				vertex_position_data[i * 4 + 1] = rot * vertex_position_data[1];
				vertex_position_data[i * 4 + 2] = rot * vertex_position_data[2];
				vertex_position_data[i * 4 + 3] = rot * vertex_position_data[3];
				vertex_normal_data[i * 4 + 0] = vertex_normal_data[i * 4 + 1] = vertex_normal_data[i * 4 + 2] = vertex_normal_data[i * 4 + 3] = rot * vertex_normal_data[0];
			}

			display::VertexBufferDesc vertex_buffer_desc;
			vertex_buffer_desc.init_data = vertex_position_data;
			vertex_buffer_desc.size = sizeof(vertex_position_data);
			vertex_buffer_desc.stride = sizeof(glm::vec3);

			m_box_vertex_position_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "box_position_vertex_buffer");

			vertex_buffer_desc.init_data = vertex_normal_data;

			m_box_vertex_normal_buffer = display::CreateVertexBuffer(device, vertex_buffer_desc, "box_normal_vertex_buffer");
		}

		//Quad Index buffer
		{
			uint16_t index_buffer_data[36] = {  0 + 4 * 0, 3 + 4 * 0, 1 + 4 * 0, 0 + 4 * 0, 2 + 4 * 0, 3 + 4 * 0,
												0 + 4 * 1, 3 + 4 * 1, 1 + 4 * 1, 0 + 4 * 1, 2 + 4 * 1, 3 + 4 * 1,
												0 + 4 * 2, 3 + 4 * 2, 1 + 4 * 2, 0 + 4 * 2, 2 + 4 * 2, 3 + 4 * 2,
												0 + 4 * 3, 3 + 4 * 3, 1 + 4 * 3, 0 + 4 * 3, 2 + 4 * 3, 3 + 4 * 3,
												0 + 4 * 4, 3 + 4 * 4, 1 + 4 * 4, 0 + 4 * 4, 2 + 4 * 4, 3 + 4 * 4,
												0 + 4 * 5, 3 + 4 * 5, 1 + 4 * 5, 0 + 4 * 5, 2 + 4 * 5, 3 + 4 * 5};
			display::IndexBufferDesc index_buffer_desc;
			index_buffer_desc.init_data = index_buffer_data;
			index_buffer_desc.size = sizeof(index_buffer_data);

			m_box_index_buffer = display::CreateIndexBuffer(device, index_buffer_desc, "box_index_buffer");
		}
	}

	void Unload(display::Device* device)
	{
		display::DestroyHandle(device, m_box_vertex_position_buffer);
		display::DestroyHandle(device, m_box_vertex_normal_buffer);
		display::DestroyHandle(device, m_box_index_buffer);
	}
};

#endif
