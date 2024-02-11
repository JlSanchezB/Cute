#include "render_module_debug_primitives.h"

namespace render
{
	DebugPrimitivesRenderModule::DebugPrimitivesRenderModule(GPUMemoryRenderModule* gpu_memory_render_module)
	{
		m_gpu_memory_render_module = gpu_memory_render_module;
		m_gpu_memory_segment_size = m_gpu_memory_render_module->GetDynamicSegmentSize();
		assert(m_gpu_memory_segment_size % sizeof(GPULine) == 0);
	}

	void DebugPrimitivesRenderModule::Init(display::Device* device, System* system)
	{
		m_device = device;
		m_render_system = system;

		//Register pass
		render::RegisterPassFactory<RenderDebugPrimitivesPass>(system);

		//Create root signature

		//Create pipeline state

	}

	void DebugPrimitivesRenderModule::Shutdown(display::Device* device, System* system)
	{
		
	}
	void DebugPrimitivesRenderModule::AddLine(const glm::vec3& a, const glm::vec3& b, uint32_t colour)
	{
		AddLine(a, b, colour, colour);
	}
	void DebugPrimitivesRenderModule::AddLine(const glm::vec3& a, const glm::vec3& b, uint32_t colour_a, uint32_t colour_b)
	{
		//Check if there is space
		auto& debug_primitives = m_debug_primitives.Get();

		if (debug_primitives.segment_vector.empty() || debug_primitives.last_segment_used == m_gpu_memory_segment_size / sizeof(GPULine))
		{
			//Needs a new segment
			debug_primitives.segment_vector.push_back(reinterpret_cast<GPULine*>(m_gpu_memory_render_module->AllocDynamicSegmentGPUMemory(m_device, render::GetRenderFrameIndex(m_render_system))));
			debug_primitives.last_segment_used = 0;
		}

		//Add line (do not read)
		GPULine& line = debug_primitives.segment_vector.back()[debug_primitives.last_segment_used];

		line.a = a;
		line.b = b;
		line.colour_a = colour_a;
		line.colour_b = colour_b;

		debug_primitives.last_segment_used++;
	}

	void DebugPrimitivesRenderModule::Render(display::Device* device, render::System* render_system, display::Context* context)
	{
		//Generate a draw call for each segment filled in each worker thread
		m_debug_primitives.Visit([&](DebugPrimitives& debug_primitives)
			{
				for (auto& segment : debug_primitives.segment_vector)
				{
					uint32_t num_lines = (segment == debug_primitives.segment_vector.back()) ? debug_primitives.last_segment_used : m_gpu_memory_segment_size / sizeof(GPULine);

					//Add draw primitive
				}

				debug_primitives.segment_vector.clear();
				debug_primitives.last_segment_used = 0;
			}
		);
	}

	void RenderDebugPrimitivesPass::Render(RenderContext& render_context) const
	{
		if (m_debug_primitives_render_module == nullptr)
		{
			m_debug_primitives_render_module = render::GetModule<DebugPrimitivesRenderModule>(render_context.GetRenderSystem(), "DebugPrimitives"_sh32);
		}

		m_debug_primitives_render_module->Render(render_context.GetDevice(), render_context.GetRenderSystem(), render_context.GetContext());
	}
}