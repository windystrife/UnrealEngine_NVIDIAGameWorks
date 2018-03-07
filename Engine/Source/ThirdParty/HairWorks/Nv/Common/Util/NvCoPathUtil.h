/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_PATH_UTIL_H
#define NV_CO_PATH_UTIL_H

#include <Nv/Common/NvCoCommon.h>

#ifdef NV_WINDOWS_FAMILY
#	include <Nv/Common/Platform/Win/NvCoWinPathUtil.h>
#endif

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

#ifdef NV_WINDOWS_FAMILY
typedef WinPathUtil PathUtil;
#else
#	error "Not implemented"
#endif

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_PATH_UTIL_H
