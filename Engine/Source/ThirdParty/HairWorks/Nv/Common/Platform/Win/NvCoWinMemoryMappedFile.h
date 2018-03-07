/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_WIN_MEMORY_MAPPED_FILE_H
#define NV_CO_WIN_MEMORY_MAPPED_FILE_H

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoMemoryMappedFile.h>

#include <Nv/Common/Platform/Win/NvCoWinMinimalInclude.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common{

/// Windows implementation of 'memory mapped file'.
class WinMemoryMappedFile: public MemoryMappedFile
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS(WinMemoryMappedFile, MemoryMappedFile)
	
		/// Initialize, returns NV_OK on success
	Result init(const char* name, SizeT size);
		/// Ctor
	WinMemoryMappedFile():m_mapFile(NV_NULL) {}

		/// Dtor
	virtual ~WinMemoryMappedFile();

private:
	HANDLE m_mapFile;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_WIN_MEMORY_MAPPED_FILE_H