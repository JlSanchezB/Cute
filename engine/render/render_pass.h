//////////////////////////////////////////////////////////////////////////
// Cute engine - List of passes defined by default in the render pass system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_PASS_H_
#define RENDER_PASS_H_

#include "render.h"
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
		void InitPass(RenderContext& render_context, display::Device* device) override;
	};

	class SetRenderTargetPass : public Pass
	{
		std::array<std::string, display::kMaxNumRenderTargets> m_render_target_name;

	public:
		DECLARE_RENDER_CLASS("SetRenderTarget");

		void Load(LoadContext& load_context) override;
	};

	class ClearRenderTargetPass : public Pass
	{
		float colour[4];
	public:
		DECLARE_RENDER_CLASS("ClearRenderTarget");

		void Load(LoadContext& load_context) override;
	};

	class SetRootSignaturePass : public Pass
	{
		std::string m_rootsignature_name;

	public:
		DECLARE_RENDER_CLASS("SetRootSignature");

		void Load(LoadContext& load_context) override;
	};

	class SetPipelineStatePass : public Pass
	{
		std::string m_pipeline_state_name;

	public:
		DECLARE_RENDER_CLASS("SetPipelineState");

		void Load(LoadContext& load_context) override;
	};

	class SetDescriptorTablePass : public Pass
	{
		//Static resource used
		std::string m_descriptor_table_static_name;
		//If there is a descriptor table, that resource will be build the first time that it is executed, as it knows the constants buffer
		std::vector<std::string> m_descriptor_table;
	
	public:
		DECLARE_RENDER_CLASS("SetDescriptorTable");

		void Load(LoadContext& load_context) override;
	};

	class DrawFullScreenQuadPass : public Pass
	{
	public:
		DECLARE_RENDER_CLASS("DrawFullScreenQuad");

		void Load(LoadContext& load_context) override;
	};
}

#endif //RENDER_PASS_H_