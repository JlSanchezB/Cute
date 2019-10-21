//////////////////////////////////////////////////////////////////////////
// Cute engine - List of resources defined by default in the render pass system
//////////////////////////////////////////////////////////////////////////
#ifndef RENDER_RESOURCE_H_
#define RENDER_RESOURCE_H_

#include "render.h"
#include <display/display.h>

namespace render
{
	//Resource reference, used to save in a pass and can be recovered using the render context
	template<class RESOURCE>
	class ResourceReference
	{
		ResourceName m_resource;
	public:
		void Set(const ResourceName& resource_name)
		{
			m_resource = resource_name;
		}
		ResourceName GetResourceName() const
		{
			return m_resource;
		}
		RESOURCE* Get(RenderContext& render_context) const
		{
			return render_context.GetResource<RESOURCE>(m_resource);
		}
	};


	//Resource that can be created outside the render pass system, just a display handle
	//If a handle is valid, means that can be deleted by the render system, if it is a weak point means that the countrol outside
	template<typename HANDLE>
	class DisplayHandleResource : public Resource
	{
		HANDLE m_handle;
		using WEAKHANDLE = typename HANDLE::WeakHandleVersion;
		WEAKHANDLE m_weak_handle;
	public:
		void Init(HANDLE& handle)
		{
			//It is a self destroy handle
			m_handle = std::move(handle);
			m_weak_handle = m_handle;
		}
		void Init(WEAKHANDLE& handle)
		{
			//It is a reference, the live is not controlled by the render system
			m_handle = HANDLE();
			m_weak_handle = handle;
		}
		void Destroy(display::Device* device) override
		{
			if (m_handle.IsValid())
			{
				display::DestroyHandle(device, m_handle);
			}
		}
		WEAKHANDLE& GetHandle()
		{
			return m_weak_handle;
		}
	};

	//Bool resource
	class BoolResource : public Resource
	{
		bool m_value;
	public:
		DECLARE_RENDER_CLASS("Bool");

		void Load(LoadContext& load_context) override;
	};

	//Texture resource
	class TextureResource : public DisplayHandleResource<display::ShaderResourceHandle>
	{
	public:
		DECLARE_RENDER_CLASS("Texture");

		void Load(LoadContext& load_context) override;
	};

	//Constant buffer resource
	class ConstantBufferResource : public DisplayHandleResource<display::ConstantBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ConstantBuffer");

		void Load(LoadContext& load_context) override;
	};

	//Unordered access buffer resource
	class UnorderedAccessBufferResource : public DisplayHandleResource<display::UnorderedAccessBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("UnorderedAccessBuffer");

		void Load(LoadContext& load_context) override;
	};

	//Vertex buffer resource
	class VertexBufferResource : public DisplayHandleResource<display::VertexBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("VertexBuffer");

		void Load(LoadContext& load_context) override {};
	};


	//Root signature resource
	class RootSignatureResource : public DisplayHandleResource<display::RootSignatureHandle>
	{
	public:
		DECLARE_RENDER_CLASS("RootSignature");

		void Load(LoadContext& load_context) override;
	};

	//Render target resource
	class RenderTargetResource : public DisplayHandleResource<display::RenderTargetHandle>
	{
	public:
		DECLARE_RENDER_CLASS("RenderTarget");

		void Load(LoadContext& load_context) override;
	};

	//Depth buffer
	class DepthBufferResource : public DisplayHandleResource<display::DepthBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("DepthBuffer");

		void Load(LoadContext& load_context) override;
	};

	//Graphics pipeline state resource
	class GraphicsPipelineStateResource : public DisplayHandleResource<display::PipelineStateHandle>
	{
	public:
		DECLARE_RENDER_CLASS("GraphicsPipelineState");

		void Load(LoadContext& load_context) override;
	};

	//Compute pipeline state resource
	class ComputePipelineStateResource : public DisplayHandleResource<display::PipelineStateHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ComputePipelineState");

		void Load(LoadContext& load_context) override;
	};

	//Descriptor table resource
	class DescriptorTableResource : public DisplayHandleResource<display::DescriptorTableHandle>
	{
	public:
		DECLARE_RENDER_CLASS("DescriptorTable");

		void Load(LoadContext& load_context) override;
	};
}
#endif //RENDER_RESOURCE_H_
