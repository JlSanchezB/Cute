//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_HANDLE_H_
#define DISPLAY_HANDLE_H_

#include <core/handle_pool.h>

namespace display
{
	using CommandListHandle = core::Handle<struct CommandList, uint16_t>;
	using WeakCommandListHandle = core::WeakHandle<struct CommandList, uint16_t>;

	using RootSignatureHandle = core::Handle<struct RootSignature, uint16_t>;
	using WeakRootSignatureHandle = core::WeakHandle<struct RootSignature, uint16_t>;

	using PipelineStateHandle = core::Handle<struct PipelineState, uint16_t>;
	using WeakPipelineStateHandle = core::WeakHandle<struct PipelineState, uint16_t>;

	using DescriptorTableHandle = core::Handle<struct DescriptorTable, uint16_t>;
	using WeakDescriptorTableHandle = core::WeakHandle<struct DescriptorTable, uint16_t>;

	using SamplerDescriptorTableHandle = core::Handle<struct SamplerDescriptorTable, uint16_t>;
	using WeakSamplerDescriptorTableHandle = core::WeakHandle<struct SamplerDescriptorTable, uint16_t>;

	using BufferHandle = core::Handle<struct Buffer, uint16_t>;
	using WeakBufferHandle = core::WeakHandle<struct Buffer, uint16_t>;

	using Texture2DHandle = core::Handle<struct Texture2D, uint16_t>;
	using WeakTexture2DHandle = core::WeakHandle<struct Texture2D, uint16_t>;
}

#endif //DISPLAY_HANDLE_H_
