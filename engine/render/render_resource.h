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

		//Cached pointer to fast access
		mutable RESOURCE* m_resource_ptr = nullptr;
	public:
		ResourceReference()
		{
		}

		ResourceReference(ResourceName resource_name)
		{
			m_resource = resource_name;
		}

		void UpdateName(const ResourceName& resource_name)
		{
			m_resource = resource_name;
			m_resource_ptr = nullptr;
		}
		ResourceName GetResourceName() const
		{
			return m_resource;
		}
		RESOURCE* Get(RenderContext& render_context) const
		{
			if (m_resource_ptr == nullptr)
			{
				bool can_not_be_cached;
				RESOURCE* resource = render_context.GetResource<RESOURCE>(m_resource, can_not_be_cached);

				//If the resource is a pass resource, do not cache, as it changes depends of the render context
				if (!can_not_be_cached)
				{
					m_resource_ptr = resource;
				}

				return resource;
			}
			else
			{
				return m_resource_ptr;
			}
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
		WEAKHANDLE GetHandle()
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
		display::TranstitionState GetDefaultAccess() const override  { return display::TranstitionState::AllShaderResource; };
	};

	//Constant buffer resource
	class ConstantBufferResource : public DisplayHandleResource<display::ConstantBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ConstantBuffer");

		void Load(LoadContext& load_context) override;
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::VertexAndConstantBuffer; };
	};

	//Constant buffer resource
	class ConstantBuffer2Resource : public DisplayHandleResource<display::ResourceHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ConstantBuffer2");

		void Load(LoadContext& load_context) override;
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::VertexAndConstantBuffer; };
	};

	//Unordered access buffer resource
	class UnorderedAccessBufferResource : public DisplayHandleResource<display::UnorderedAccessBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("UnorderedAccessBuffer");

		void Load(LoadContext& load_context) override;
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::AllShaderResource; };
		DisplayHandle GetDisplayHandle() override
		{
			return DisplayHandleResource<display::UnorderedAccessBufferHandle>::GetHandle();
		}
	};

	//Shader Resource resource
	class ShaderResourceResource : public DisplayHandleResource<display::ShaderResourceHandle>
	{
	public:
		DECLARE_RENDER_CLASS("ShaderResource");

		void Load(LoadContext& load_context) override;
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::AllShaderResource; };
	};

	//Vertex buffer resource
	class VertexBufferResource : public DisplayHandleResource<display::VertexBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("VertexBuffer");

		void Load(LoadContext& load_context) override {};
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::VertexAndConstantBuffer; };
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
		uint32_t m_width = 0;
		uint32_t m_heigth = 0;

	public:
		DECLARE_RENDER_CLASS("RenderTarget");

		RenderTargetResource()
		{
		}
		RenderTargetResource(uint32_t width, uint32_t heigth) :
			m_width(width),
			m_heigth(heigth)
		{
		}

		void Load(LoadContext& load_context) override;

		DisplayHandle GetDisplayHandle() override
		{
			return DisplayHandleResource<display::RenderTargetHandle>::GetHandle();
		}
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::RenderTarget; };

		void UpdateInfo(uint32_t width, uint32_t heigth)
		{
			m_width = width;
			m_heigth = heigth;
		}

		void GetInfo(uint32_t& width, uint32_t& heigth) const
		{
			width = m_width;
			heigth = m_heigth;
		}
	};

	//Depth buffer
	class DepthBufferResource : public DisplayHandleResource<display::DepthBufferHandle>
	{
	public:
		DECLARE_RENDER_CLASS("DepthBuffer");

		void Load(LoadContext& load_context) override;
		DisplayHandle GetDisplayHandle() override
		{
			return DisplayHandleResource<display::DepthBufferHandle>::GetHandle();
		}
		display::TranstitionState GetDefaultAccess() const override { return display::TranstitionState::Depth; };
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
