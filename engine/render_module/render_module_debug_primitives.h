// Cute engine - Implementation for the debug primitives rendering
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_MODULE_DEGUB_PRIMITIVES_H_
#define RENDER_MODULE_DEGUB_PRIMITIVES_H_

#include <render/render.h>
#include <display/display_handle.h>
#include <ext/glm/glm.hpp>
#include <job/job_helper.h>
#include <render_module/render_module_gpu_memory.h>

namespace display
{
	struct Device;
	struct Context;
}

namespace render
{
	//Debug primitives Render Module
	class DebugPrimitivesRenderModule : public render::Module
	{
		DebugPrimitivesRenderModule(GPUMemoryRenderModule* gpu_memory_render_module);

		void Init(display::Device* device, System* system) override;
		void Shutdown(display::Device* device, System* system) override;

		//Draw a line
		void AddLine(const glm::vec3& a, const glm::vec3& b, uint32_t colour);
		void AddLine(const glm::vec3& a, const glm::vec3& b, uint32_t colour_a, uint32_t colour_b);

		//Set view projection matrix
		void SetViewProjectionMatrix(const glm::mat4x4& view_projection_matrix);
	private:

		void Render(display::Device* device, render::System* render_system, display::Context* context);

		struct GPULine
		{
			glm::vec3 a;
			glm::vec3 b;
			uint32_t colour_a;
			uint32_t colour_b;
		};

		//Debug primitives associated to this worker thread
		struct DebugPrimitives
		{
			//Vector of segments with debug primitives
			std::vector<GPULine*> segment_vector;

			//Last segment line index
			size_t last_segment_line_index = 0;
		};

		//Thread local storage with the collect debug primitives
		job::ThreadData<DebugPrimitives> m_debug_primitives;

		//View projection matrix
		glm::mat4x4 m_view_projection_matrix[2];

		GPUMemoryRenderModule* m_gpu_memory_render_module;
		size_t m_gpu_memory_segment_size;
		display::Device* m_device = nullptr;
		render::System* m_render_system = nullptr;

		display::RootSignatureHandle m_root_signature;
		display::PipelineStateHandle m_pipeline_state;
		display::BufferHandle m_constant_buffer;

		friend class RenderDebugPrimitivesPass;
	};

	class RenderDebugPrimitivesPass : public Pass
	{
		mutable DebugPrimitivesRenderModule* m_debug_primitives_render_module = nullptr;
	public:
		DECLARE_RENDER_CLASS("RenderDebugPrimitives");

		void Render(RenderContext& render_context) const override;
	};
}

#endif //RENDER_MODULE_DEGUB_PRIMITIVES_H_