//////////////////////////////////////////////////////////////////////////
// Cute engine - DDS LOADER
//////////////////////////////////////////////////////////////////////////
#ifndef DDS_LOADER_H_
#define DDS_LOADER_H_

#include <vector>
#include <d3d12.h>

namespace dds_loader
{
	enum DDS_ALPHA_MODE
	{
		DDS_ALPHA_MODE_UNKNOWN = 0,
		DDS_ALPHA_MODE_STRAIGHT = 1,
		DDS_ALPHA_MODE_PREMULTIPLIED = 2,
		DDS_ALPHA_MODE_OPAQUE = 3,
		DDS_ALPHA_MODE_CUSTOM = 4,
	};

	enum DDS_LOADER_FLAGS
	{
		DDS_LOADER_DEFAULT = 0,
		DDS_LOADER_FORCE_SRGB = 0x1,
		DDS_LOADER_MIP_RESERVE = 0x8,
	};

	HRESULT CalculateD12Loader(ID3D12Device* device, const uint8_t* dds_file, size_t size, DDS_LOADER_FLAGS flags, D3D12_RESOURCE_DESC& d12_resource_desc,
		std::vector<D3D12_SUBRESOURCE_DATA>& sub_resources);

	DDS_ALPHA_MODE CalculateAlphaMode(const uint8_t* dds_file);
}

#endif //DDS_LOADER_H_
