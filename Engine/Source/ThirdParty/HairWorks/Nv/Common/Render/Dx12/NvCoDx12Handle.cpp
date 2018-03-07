/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoLogger.h>

#include "NvCoDx12Handle.h"

namespace nvidia {
namespace Common {

/* static */const Char* Dx12Type::getSubTypeText(Dx12SubType::Enum subType)
{
	switch (subType)
	{
		// Make sure all of your subtypes are represented here...
		case Dx12SubType::COUNT_OF: break;
		case Dx12SubType::UNKNOWN:	return "Unknown";
		case Dx12SubType::CONTEXT:	return "ID3D12CommandList";
		case Dx12SubType::DEVICE:	return "ID3D12Device";
		case Dx12SubType::BUFFER:	return "ID3D12Resource";
		case Dx12SubType::FLOAT32:	return "Float32";
		case Dx12SubType::CPU_DESCRIPTOR_HANDLE: return "D3D12_CPU_DESCRIPTOR_HANDLE";
		case Dx12SubType::COMMAND_QUEUE: return "ID3D12CommandQueue";
	}
	NV_CORE_ALWAYS_ASSERT("Unknown subtype");
	return "Unknown";
}

/* static */Void* Dx12Type::handlePtrCast(Int fromType, Int toType)
{
	// Handle null type
	if (fromType == 0)
	{
		return NV_NULL;
	}
	castFailure(fromType, toType);
	return NV_NULL;
}

/* static */Void* Dx12Type::handleCast(Int fromType, Int toType)
{
	// Handle null type
	if (fromType == 0)
	{
		return NV_NULL;
	}
	castFailure(fromType, toType);
	return NV_NULL;
}

/* static */Void Dx12Type::logCastFailure(Int fromType, Int toType)
{
	// Handles all the classic cast failures
	if (!ApiHandle::isGenericCastFailure(fromType, toType, ApiType::DX11))
	{
		// Must be the right api, but wrong subType
		Dx12SubType::Enum fromSubType = (Dx12SubType::Enum)ApiHandle::getSubType(fromType);
		Dx12SubType::Enum toSubType = (Dx12SubType::Enum)ApiHandle::getSubType(toType);
		return ApiHandle::logSubTypeCastFailure(getSubTypeText(fromSubType), getSubTypeText(toSubType), ApiType::DX11);
	}
	return ApiHandle::logCastFailure(fromType, toType, ApiType::DX11);
}

/* static */Void Dx12Type::castFailure(Int fromType, Int toType)
{
	logCastFailure(fromType, toType);
	// Make it assert on a debug build
	NV_CORE_ALWAYS_ASSERT("Cast failure");
}

} // namespace Common
} // namespace nvidia
