/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoApiHandle.h"

#include <Nv/Common/NvCoLogger.h>

// For sprintf...
#include <stdio.h>

namespace nvidia {
namespace Common {

/* static */const Char* ApiHandle::getApiText(EApiType apiType)
{
	switch (apiType)
	{
		case ApiType::DX11:		return "Dx11";
		case ApiType::DX12:		return "Dx12";
		case ApiType::VULCAN:	return "Vulcan";
		case ApiType::METAL:	return "Metal";
		case ApiType::OPEN_GL:	return "OpenGl";
		default: return "Unknown";
	}
}

/* static */Bool ApiHandle::isGenericCastFailure(Int fromType, Int toType, EApiType apiType)
{
	EApiType fromApiType = getApiType(fromType);
	EApiType toApiType = getApiType(toType);
	return (fromApiType != toApiType || fromApiType != apiType);
}

/// Log that there is a cast failure
/* static */Void ApiHandle::logCastFailure(Int fromType, Int toType, EApiType apiType)
{
	EApiType fromApiType = getApiType(fromType);
	EApiType toApiType = getApiType(toType);

	char buffer[1024];
	if (fromApiType != toApiType)
	{
		sprintf_s(buffer, NV_COUNT_OF(buffer), "Cannot convert type - different apis %s->%s for API expected is %s", getApiText(getApiType(fromType)), getApiText(getApiType(toType)), getApiText(apiType));

		NV_CO_LOG_WARN(buffer);
		return;
	}
	if (fromApiType != apiType)
	{
		sprintf_s(buffer, NV_COUNT_OF(buffer), "Expecting something in api %s, but have %s", getApiText(getApiType(fromType)), getApiText(apiType));
		NV_CO_LOG_WARN(buffer);
		return;
	}

	// Handle the generic situation, where all we know is the cast can't happen
	{
		sprintf_s(buffer, NV_COUNT_OF(buffer), "Cannot cast %s %d to %s %d", getApiText(fromApiType), getSubType(fromType), getApiText(toApiType), getSubType(toType));
		NV_CO_LOG_WARN(buffer);
	}
}

/* static */Void ApiHandle::logSubTypeCastFailure(const Char* fromSubText, const Char* toSubText, EApiType apiType)
{
	char buffer[1024];
	sprintf_s(buffer, NV_COUNT_OF(buffer), "Cannot cast %s to %s on api %s", fromSubText, toSubText, getApiText(apiType));
	NV_CO_LOG_WARN(buffer);
}

} // namespace Common 
} // namespace nvidia
