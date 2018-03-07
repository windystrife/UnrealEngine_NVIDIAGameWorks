/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoMemoryMappedFile.h"

#ifdef NV_WINDOWS_FAMILY
#	include <Nv/Common/Platform/Win/NvCoWinMemoryMappedFile.h>
#endif

namespace nvidia {
namespace Common {

/* static */MemoryMappedFile* MemoryMappedFile::create(const char* name, SizeT size)
{
#ifdef NV_WINDOWS_FAMILY
	WinMemoryMappedFile* impl = new WinMemoryMappedFile;
	if (NV_FAILED(impl->init(name, size)))
	{
		delete impl;
		return NV_NULL;
	}
	return impl;
#else
	return NV_NULL;
#endif
}

} // namespace Common 
} // namespace nvidia

