//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_ENUM_H_
#define DISPLAY_ENUM_H_

namespace display
{
	enum class RootSignatureParameterType
	{
		ConstantBuffer,
		UnorderAccessBuffer,
		ShaderResource,
		DescriptorTable
	};

	enum class DescriptorTableParameterType
	{
		ConstantBuffer,
		UnorderAccessBuffer,
		ShaderResource,
		Sampler
	};

	enum class ShaderVisibility
	{
		All,
		Vertex,
		Hull,
		Domain,
		Geometry,
		Pixel
	};

	enum class Format
	{
		R32G32_FLOAT,
		R32G32B32_FLOAT,
		R32G32B32A32_FLOAT,
		R8G8B8A8_UNORM,
		R8G8B8A8_UNORM_SRGB,
		R32_UINT,
		R16_UINT
	};

	enum class FillMode
	{
		Wireframe,
		Solid
	};

	enum class CullMode
	{
		None,
		Front,
		Back
	};

	enum class Blend
	{
		Zero,
		One,
		SrcAlpha,
		InvSrcAlpha
	};

	enum class ComparationFunction
	{
		Never,
		Less,
		Equal,
		Less_Equal,
		Greater,
		NotEqual,
		Greater_Equal,
		Always
	};

	enum class Topology
	{
		Triangle
	};

	enum class PrimitiveTopology
	{
		TriangleList
	};

	enum class BlendOp
	{
		Add,
		Substract
	};

	enum class InputType
	{
		Vertex,
		Instance
	};

	enum class TextureAddressMode
	{
		Wrap,
		Mirror,
		Clamp
	};

	enum class Filter
	{
		Point,
		Linear,
		Anisotropic
	};

	enum class Access
	{
		Static, //Only change in the initialization, never touch again
		Dynamic, //Upload from the CPU to the GPU one per frame
	};

	enum class ShaderResourceType
	{
		Buffer,
		Texture2D
	};

	enum class RenderTargetType
	{
		Texture2D
	};

	enum class ResourceState
	{
		RenderTarget,
		PixelShaderResource,
		NonPixelShaderResource
	};
}

#endif
