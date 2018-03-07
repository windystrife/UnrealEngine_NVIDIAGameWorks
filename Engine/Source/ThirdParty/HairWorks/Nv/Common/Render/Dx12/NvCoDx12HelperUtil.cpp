/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

// Make the DXGI_DEBUG_D3D12 symbol defined in this compilation unit, to avoid link problems
#define INITGUID

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoString.h>

#include "NvCoDx12HelperUtil.h"

#include <Nv/Common/NvCoLogger.h>

#include <Nv/Common/Render/Dx/NvCoDxFormatUtil.h>
#include <Nv/Common/Render/Dx/NvCoDxDebugUtil.h>

#include <d3d12.h>

namespace nvidia {
namespace Common {

Void Dx12HelperUtil::calcSrvDesc(ID3D12Resource* resource, DXGI_FORMAT pixelFormat, D3D12_SHADER_RESOURCE_VIEW_DESC& descOut)
{
	// Get the desc
	return calcSrvDesc(resource->GetDesc(), pixelFormat, descOut);
}

Void Dx12HelperUtil::calcSrvDesc(const D3D12_RESOURCE_DESC& desc, DXGI_FORMAT pixelFormat, D3D12_SHADER_RESOURCE_VIEW_DESC& descOut)
{
	
	// create SRV
	descOut = D3D12_SHADER_RESOURCE_VIEW_DESC();
	
	descOut.Format = (pixelFormat == DXGI_FORMAT_UNKNOWN) ? DxFormatUtil::calcFormat(DxFormatUtil::USAGE_SRV, desc.Format) : pixelFormat;
	descOut.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	if (desc.DepthOrArraySize == 1)
	{
		descOut.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

		descOut.Texture2D.MipLevels = desc.MipLevels;
		descOut.Texture2D.MostDetailedMip = 0;
		descOut.Texture2D.PlaneSlice = 0;
		descOut.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	else if (desc.DepthOrArraySize == 6)
	{
		descOut.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;

		descOut.TextureCube.MipLevels = desc.MipLevels;
		descOut.TextureCube.MostDetailedMip = 0;
		descOut.TextureCube.ResourceMinLODClamp = 0;
	}
	else
	{
		descOut.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

		descOut.Texture2DArray.ArraySize = desc.DepthOrArraySize;
		descOut.Texture2DArray.MostDetailedMip = 0;
		descOut.Texture2DArray.MipLevels = desc.MipLevels;
		descOut.Texture2DArray.FirstArraySlice = 0;
		descOut.Texture2DArray.PlaneSlice = 0;
		descOut.Texture2DArray.ResourceMinLODClamp = 0;
	}
}

/* static */Void Dx12HelperUtil::reportLiveObjects()
{
	ComPtr<IDXGIDebug> dxgiDebug;
	Result res = DxDebugUtil::getDebugInterface(dxgiDebug);

	if (NV_FAILED(res) || !dxgiDebug)
	{
		NV_CO_LOG_WARN("Unable to access debug interface -> can't report live objects");
		return;
	}
	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_DETAIL);
}

/* static */Result Dx12HelperUtil::serializeRootSigniture(const D3D12_ROOT_SIGNATURE_DESC& desc, D3D_ROOT_SIGNATURE_VERSION signitureVersion, ComPtr<ID3DBlob>& sigBlobOut)
{
	ComPtr<ID3DBlob> error;
	Result res = D3D12SerializeRootSignature(&desc, signitureVersion, sigBlobOut.writeRef(), error.writeRef());
	if (NV_SUCCEEDED(res))
	{
		return res;
	}

	char* errorText = (char*)error->GetBufferPointer();

	String buf;
	buf << "Unable to serialize Dx12 signature: " << SubString(SubString::INIT_CSTR, errorText);

	NV_CO_LOG_WARN(buf.getCstr());
	return res;
}

} // namespace Common
} // namespace nvidia
