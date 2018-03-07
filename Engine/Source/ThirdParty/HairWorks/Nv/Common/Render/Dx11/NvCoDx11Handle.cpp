/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoLogger.h>

#include "NvCoDx11Handle.h"

namespace nvidia {
namespace Common {

/* static */const Char* Dx11Type::getSubTypeText(EDx11SubType subType)
{
	switch (subType)
	{
		// Make sure all of your subtypes are represented here...
		case Dx11SubType::COUNT_OF: break;
		case Dx11SubType::UNKNOWN:	return "Unknown";
		case Dx11SubType::CONTEXT:	return "ID3D11DeviceContext";
		case Dx11SubType::DEVICE:	return "ID3D11Device";
		case Dx11SubType::BUFFER:	return "ID3D11Buffer";
		case Dx11SubType::FLOAT32:	return "Float32";
		case Dx11SubType::DEPTH_STENCIL_VIEW: return "ID3D11DepthStencilView";
		case Dx11SubType::SHADER_RESOURCE_VIEW: return "ID3D11ShaderResourceView";
	}
	NV_CORE_ALWAYS_ASSERT("Unknown type");
	return "Unknown";
}

/* static */Void* Dx11Type::handlePtrCast(Int fromType, Int toType)
{
	// Handle null type
	if (fromType == 0)
	{
		return NV_NULL;
	}
	castFailure(fromType, toType);
	return NV_NULL;
}

/* static */Void* Dx11Type::handleCast(Int fromType, Int toType)
{
	// Handle null type
	if (fromType == 0)
	{
		return NV_NULL;
	}
	castFailure(fromType, toType);
	return NV_NULL;
}

/* static */Void Dx11Type::logCastFailure(Int fromType, Int toType)
{
	// Handles all the classic cast failures
	if (!ApiHandle::isGenericCastFailure(fromType, toType, ApiType::DX11))
	{
		// Must be the right api, but wrong subType
		EDx11SubType fromSubType = (EDx11SubType)ApiHandle::getSubType(fromType);
		EDx11SubType toSubType = (EDx11SubType)ApiHandle::getSubType(toType);
		return ApiHandle::logSubTypeCastFailure(getSubTypeText(fromSubType), getSubTypeText(toSubType), ApiType::DX11);
	}
	return ApiHandle::logCastFailure(fromType, toType, ApiType::DX11);
}

/* static */Void Dx11Type::castFailure(Int fromType, Int toType)
{
	logCastFailure(fromType, toType);
	// Make it assert on a debug build
	NV_CORE_ALWAYS_ASSERT("Cast failed");
}

} // namespace Common 
} // namespace nvidia
