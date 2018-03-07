/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_DX_DEBUG_UTIL_H
#define NV_CO_DX_DEBUG_UTIL_H

#include <Nv/Common/NvCoComPtr.h>

#include <DXGIDebug.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common { 

struct DxDebugUtil
{
		/// Get the debug interface
	static Result getDebugInterface(NvCo::ComPtr<IDXGIDebug>& debugOut);
};

} // namespace Common
} // namespace nvidia

/** @} */

#endif // NV_CO_DX12_DEBUG_UTIL_H
