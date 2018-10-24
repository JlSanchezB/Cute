//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_HANDLE_H_
#define DISPLAY_HANDLE_H_

namespace display
{
	using CommandListHandle = core::Handle<struct CommandList, uint16_t>;
	using WeakCommandListHandle = core::WeakHandle<struct CommandList, uint16_t>;

	using TextureHandle = core::Handle<struct Texture, uint16_t>;
	using weakTextureHandle = core::WeakHandle<struct Texture, uint16_t>;

	using RenderTargetHandle = core::Handle<struct RenderTarget, uint16_t>;
	using WeakRenderTargetHandle = core::WeakHandle<struct RenderTarget, uint16_t>;

	using RootSignatureHandle = core::Handle<struct RootSignature, uint16_t>;
	using WeakRootSignatureHandle = core::WeakHandle<struct RootSignature, uint16_t>;

	using PipelineStateHandle = core::Handle<struct PipelineState, uint16_t>;
	using WeakPipelineStateHandle = core::WeakHandle<struct PipelineState, uint16_t>;

	using VertexBufferHandle = core::Handle<struct VertexBuffer, uint16_t>;
	using WeakVertexBufferHandle = core::WeakHandle<struct VertexBuffer, uint16_t>;

	using IndexBufferHandle = core::Handle<struct IndexBuffer, uint16_t>;
	using WeakIndexBufferOHandle = core::WeakHandle<struct IndexBuffer, uint16_t>;
}

#endif //DISPLAY_HANDLE_H_
