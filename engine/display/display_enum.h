//////////////////////////////////////////////////////////////////////////
// Cute engine - Display layer
//////////////////////////////////////////////////////////////////////////
#ifndef DISPLAY_ENUM_H_
#define DISPLAY_ENUM_H_

namespace display
{
	enum class Format
	{
		R32G32B32_FLOAT,
		R32G32B32A32_FLOAT,
		R8G8B8A8_UNORM,
		R8G8B8A8_UNORM_SRGB
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
}

#endif
