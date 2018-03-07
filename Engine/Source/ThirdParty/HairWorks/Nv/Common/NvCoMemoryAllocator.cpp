/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoMemoryAllocator.h"

#ifdef NV_WINDOWS_FAMILY
#	include <Nv/Common/Platform/Win/NvCoWinMemoryAllocator.h>
#endif

namespace nvidia {
namespace Common {

/* static */MemoryAllocator* MemoryAllocator::s_instance = getDefault();

/* static */MemoryAllocator* MemoryAllocator::getDefault()
{
#ifdef NV_WINDOWS_FAMILY
	return WinMemoryAllocator::getSingleton();
#else
	return NV_NULL;
#endif
}

} // namespace Common 
} // namespace nvidia

