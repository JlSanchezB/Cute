//////////////////////////////////////////////////////////////////////////
// Cute engine - List of passes defined by default in the render pass system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_PASS_H_
#define RENDER_PASS_H_

#include <render/render.h>
#include <render/render_resource.h>
#include <display/display.h>
#include "render_system.h"

namespace render
{
	//A resource state, used for updates or preconditions
	struct ResourceStateSync
	{
		System::ResourceInfoReference resource;
		ResourceState state;

		ResourceStateSync(ResourceName _resource, ResourceState _state) :
			resource(_resource), state(_state)
		{
		}
	};

	//Resource barrier description
	struct ResourceBarrier
	{
		System::ResourceInfoReference resource;
		display::TranstitionState access;

		ResourceBarrier(ResourceName _resource, display::TranstitionState _access) :
			resource(_resource), access(_access)
		{
		}
	};

	//Resource pool dependency like depth buffers and render targets
	struct ResourcePoolDependency
	{
		ResourceName name;
		PoolResourceType type;
		bool needs_to_allocate; //It needs to be allocate this pass
		bool will_be_free; //It is free after this pass
		uint32_t width;
		uint32_t height;
		uint32_t size;
		uint16_t width_factor; //fixed point, 256 is factor 1
		uint16_t height_factor;
		uint16_t tile_size_width;
		uint16_t tile_size_height;
		display::Format format;
		float default_depth;
		uint8_t default_stencil;
		bool clear;
		bool not_alias;
		bool is_uav;

		ResourcePoolDependency(ResourceName _name, PoolResourceType& _type,  bool _needs_to_allocate, bool _will_be_free, uint32_t _width, uint32_t _height, uint32_t _size, float _width_factor, float _height_factor, uint16_t _tile_size_width, uint16_t _tile_size_height, const display::Format& _format, const float _default_depth, const uint8_t _default_stencil, const bool _clear, const bool _not_alias, const bool _is_uav) :
			name(_name), type(_type), needs_to_allocate(_needs_to_allocate), will_be_free(_will_be_free), width(_width), height(_height), size(_size), tile_size_width(_tile_size_width), tile_size_height(_tile_size_height), format(_format), default_depth(_default_depth), default_stencil(_default_stencil), clear(_clear), not_alias(_not_alias), is_uav(_is_uav)
		{
			width_factor = static_cast<uint16_t>(_width_factor * 256.f);
			height_factor = static_cast<uint16_t>(_height_factor * 256.f);
		}
	};

	//Context pass, list of commands to send to the GPU, it will use it own command list
	class ContextPass : public Pass
	{
		display::CommandListHandle m_command_list_handle;
		PassName m_name;

		std::vector<std::unique_ptr<Pass>> m_passes;

		//Conditions for the pass to run
		std::vector<ResourceStateSync> m_pre_resource_conditions;

		//State updates after the pass execution
		std::vector<ResourceStateSync> m_post_resource_updates;

		//Barrier needed to be ready for this pass
		std::vector<ResourceBarrier> m_resource_barriers;

		//Resource pool needed to get for this pass
		std::vector<ResourcePoolDependency> m_resource_pool_dependencies;

	public:
		DECLARE_RENDER_CLASS("Pass");

		void Destroy(display::Device* device) override;
		void Load(LoadContext& load_context) override;
		void InitPass(RenderContext& render_context, display::Device* device, ErrorContext& errors) override;
		void Render(RenderContext& render_context) const override;
		void Execute(RenderContext& render_context) const override;

		const std::vector<ResourceStateSync>& GetPreResourceCondition() const
		{
			return m_pre_resource_conditions;
		}

		const std::vector<ResourceStateSync>& GetPostUpdateCondition() const
		{
			return m_post_resource_updates;
		}

		const std::vector<ResourceBarrier>& GetResourceBarriers() const
		{
			return m_resource_barriers;
		}

		const std::vector<ResourcePoolDependency>& GetResourcePoolDependencies() const
		{
			return m_resource_pool_dependencies;
		}

		display::WeakCommandListHandle GetCommandList() const
		{
			return m_command_list_handle;
		}

		//The context pass need to be called passing the resource barriers, the normal render is not used
		void RootContextRender(RenderContext& render_context, const std::vector<display::ResourceBarrier>& resource_barriers) const;
	};

	class SetRenderTargetPass : public Pass
	{
		std::array<ResourceReference<RenderTargetResource>, display::kMaxNumRenderTargets> m_render_target;
		ResourceReference<DepthBufferResource> m_depth_buffer;
		uint8_t m_num_render_targets;
	public:
		DECLARE_RENDER_CLASS("SetRenderTarget");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class ClearRenderTargetPass : public Pass
	{
		ResourceReference<RenderTargetResource> m_render_target;
		float colour[4];
	public:
		DECLARE_RENDER_CLASS("ClearRenderTarget");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class ClearDepthStencilPass : public Pass
	{
		ResourceReference<DepthBufferResource> m_depth_stencil_buffer;
		display::ClearType clear_type;
		std::optional<float> depth_value;
		std::optional<uint8_t> stencil_value;
	public:
		DECLARE_RENDER_CLASS("ClearDepthStencil");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetRootSignaturePass : public Pass
	{
		//Pipe
		display::Pipe m_pipe = display::Pipe::Graphics;
		//Root signature name
		ResourceReference<RootSignatureResource> m_rootsignature;

	public:
		DECLARE_RENDER_CLASS("SetRootSignature");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetRootConstantBufferPass : public Pass
	{
		//Pipe
		display::Pipe m_pipe = display::Pipe::Graphics;
		//Root parameter
		uint8_t m_root_parameter = 0;
		//Root signature name
		ResourceReference<BufferResource> m_constant_buffer;

	public:
		DECLARE_RENDER_CLASS("SetRootConstantBuffer");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetRootUnorderedAccessBufferPass : public Pass
	{
		//Pipe
		display::Pipe m_pipe = display::Pipe::Graphics;
		//Root parameter
		uint8_t m_root_parameter = 0;
		//Root signature name
		ResourceReference<BufferResource> m_unordered_access_buffer;

	public:
		DECLARE_RENDER_CLASS("SetRootUnorderedAccessBuffer");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetRootShaderResourcePass : public Pass
	{
		//Pipe
		display::Pipe m_pipe = display::Pipe::Graphics;
		//Root parameter
		uint8_t m_root_parameter = 0;
		//Root signature name
		ResourceReference<BufferResource> m_shader_resource;

	public:
		DECLARE_RENDER_CLASS("SetRootShaderResource");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetPipelineStatePass : public Pass
	{
		ResourceReference<GraphicsPipelineStateResource> m_pipeline_state;

	public:
		DECLARE_RENDER_CLASS("SetPipelineState");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetComputePipelineStatePass : public Pass
	{
		ResourceReference<ComputePipelineStateResource> m_pipeline_state;

	public:
		DECLARE_RENDER_CLASS("SetComputePipelineState");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class SetDescriptorTablePass : public Pass
	{
		//Root parameter
		uint8_t m_root_parameter = 0;
		//Pipe
		display::Pipe m_pipe = display::Pipe::Graphics;
		//If there is a descriptor table, that resource will be build the first time that it is executed, as it knows the constants buffer
		std::vector<std::pair<std::string, display::DescriptorTableParameterType>> m_descriptor_table_names;
		//Descriptor has a pool resource and needs to be updated each frame
		bool m_update_each_frame = false;

		//Static resource used
		ResourceReference<DescriptorTableResource> m_descriptor_table;
	
		//Just a global count to handle unique names for the resources
		inline static uint32_t m_resource_id_count = 0;
	
		bool FillDescriptorTableDesc(RenderContext& render_context, display::DescriptorTableDesc& descriptor_table_desc) const;

	public:
		DECLARE_RENDER_CLASS("SetDescriptorTable");

		void Load(LoadContext& load_context) override;
		void InitPass(RenderContext& render_context, display::Device* device, ErrorContext& errors) override;
		void Render(RenderContext& render_context) const override;
	};

	class DrawFullScreenQuadPass : public Pass
	{
	public:
		DECLARE_RENDER_CLASS("DrawFullScreenQuad");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class DispatchViewComputePass : public Pass
	{
		uint32_t m_tile_width;
		uint32_t m_tile_height;
		uint32_t m_scale_down = 1;

	public:
		DECLARE_RENDER_CLASS("DispatchViewCompute");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class DispatchComputePass : public Pass
	{
		uint32_t m_group_count_x = 1;
		uint32_t m_group_count_y = 1;
		uint32_t m_group_count_z = 1;

	public:
		DECLARE_RENDER_CLASS("DispatchCompute");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class DispatchComputeFilterPass : public Pass
	{
		uint32_t m_tile_size_x = 8;
		uint32_t m_tile_size_y = 8;

		ResourceReference<TextureResource> m_texture;
	public:
		DECLARE_RENDER_CLASS("DispatchComputeFilter");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};

	class DrawRenderItemsPass : public Pass
	{
		uint8_t m_priority;
	public:
		DECLARE_RENDER_CLASS("DrawRenderItems");

		void Load(LoadContext& load_context) override;
		void Render(RenderContext& render_context) const override;
	};
}

#endif //RENDER_PASS_H_