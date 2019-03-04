//////////////////////////////////////////////////////////////////////////
// Cute engine - List of passes defined by default in the render pass system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_PASS_H_
#define RENDER_PASS_H_

#include <render/render.h>
#include <render/render_resource.h>
#include <display/display.h>

namespace render
{
	//Context pass, list of commands to send to the GPU, it will use it own command list
	class ContextPass : public Pass
	{
		display::CommandListHandle m_command_list_handle;
		std::vector<std::unique_ptr<Pass>> m_passes;

	public:
		DECLARE_RENDER_CLASS("Pass");

		void Destroy(display::Device* device) override;
		void Load(LoadContext& load_context) override;
		void InitPass(RenderContext& render_context, display::Device* device, ErrorContext& errors) override;
		void Render(RenderContext& render_context) const override;
		void Execute(RenderContext& render_context) const override;
	};

	class SetRenderTargetPass : public Pass
	{
		std::array<ResourceReference<RenderTargetResource>, display::kMaxNumRenderTargets> m_render_target;
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
		ResourceReference<ConstantBufferResource> m_constant_buffer;

	public:
		DECLARE_RENDER_CLASS("SetRootConstantBuffer");

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

	class SetDescriptorTablePass : public Pass
	{
		//Root parameter
		uint8_t m_root_parameter = 0;
		//Pipe
		display::Pipe m_pipe = display::Pipe::Graphics;
		//If there is a descriptor table, that resource will be build the first time that it is executed, as it knows the constants buffer
		std::vector<std::string> m_descriptor_table_names;

		//Static resource used
		ResourceReference<DescriptorTableResource> m_descriptor_table;
		
	
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