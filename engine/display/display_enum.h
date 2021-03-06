//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_ENUM_H_
#define DISPLAY_ENUM_H_

namespace display
{
	enum class RootSignatureParameterType : uint8_t
	{
		Constants,
		ConstantBuffer,
		UnorderedAccessBuffer,
		ShaderResource,
		DescriptorTable
	};

	enum class DescriptorTableParameterType : uint8_t
	{
		ConstantBuffer,
		UnorderedAccessBuffer,
		ShaderResource,
		Sampler
	};

	enum class ShaderVisibility : uint8_t
	{
		All,
		Vertex,
		Hull,
		Domain,
		Geometry,
		Pixel
	};

	enum class Format : uint8_t
	{
		UNKNOWN, //Used for structure buffers
		R32G32_FLOAT,
		R32G32B32_FLOAT,
		R32G32B32A32_FLOAT,
		R8G8B8A8_UNORM,
		R8G8B8A8_UNORM_SRGB,
		R32_UINT,
		R16_UINT,
		D32_FLOAT
	};

	enum class FillMode : uint8_t
	{
		Wireframe,
		Solid
	};

	enum class CullMode : uint8_t
	{
		None,
		Front,
		Back
	};

	enum class Blend : uint8_t
	{
		Zero,
		One,
		SrcAlpha,
		InvSrcAlpha
	};

	enum class ComparationFunction : uint8_t
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

	enum class Topology : uint8_t
	{
		Triangle
	};

	enum class PrimitiveTopology : uint8_t
	{
		TriangleList
	};

	enum class BlendOp : uint8_t
	{
		Add,
		Substract
	};

	enum class InputType : uint8_t
	{
		Vertex,
		Instance
	};

	enum class TextureAddressMode : uint8_t
	{
		Wrap,
		Mirror,
		Clamp
	};

	enum class Filter : uint8_t
	{
		Point,
		Linear,
		Anisotropic
	};

	enum class Access : uint8_t
	{
		Static, //Only change in the initialization, never touch again
		Dynamic, //Upload from the CPU to the GPU one per frame, duplicates the resource so it needs to upload everyframe
		Upload //It is a resource that lives in the upload heap, user needs to sync CPU and GPU
	};

	enum class ShaderResourceType : uint8_t
	{
		Buffer,
		Texture2D
	};

	enum class RenderTargetType : uint8_t
	{
		Texture2D
	};

	//Type of resource barrier
	enum class ResourceBarrierType : uint8_t
	{
		Transition,
		UnorderAccess
	};

	enum class TranstitionState : uint8_t
	{
		Common,
		VertexAndConstantBuffer,
		UnorderedAccess,
		RenderTarget,
		PixelShaderResource,
		NonPixelShaderResource,
		AllShaderResource,
		Depth,
		DepthRead,
		Present
	};
}

#endif
