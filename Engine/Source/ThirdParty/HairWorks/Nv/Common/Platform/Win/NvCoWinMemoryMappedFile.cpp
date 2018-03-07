/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/NvCoCommon.h>

#include "NvCoWinMemoryMappedFile.h"

namespace nvidia {
namespace Common {

Result WinMemoryMappedFile::init(const char* name, SizeT size)
{
#ifndef NV_WINMODERN
   	m_mapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
	if (m_mapFile == NV_NULL )
	{
		m_mapFile = CreateFileMappingA(
      				INVALID_HANDLE_VALUE,    // use paging file
       				NV_NULL,                    // default security
       				PAGE_READWRITE,          // read/write access
       				0,                       // maximum object size (high-order DWORD)
       				DWORD(size),                // maximum object size (low-order DWORD)
       				name);
	}
#else
	// convert name to unicode
	const static int BUFFER_SIZE = 256;
	WCHAR buffer[BUFFER_SIZE];
	int succ = MultiByteToWideChar(CP_ACP, 0, name, -1, buffer, BUFFER_SIZE);
	// validate
	if (succ < 0)
		succ = 0;
	if (succ < BUFFER_SIZE)
		buffer[succ] = 0;
	else if (buffer[BUFFER_SIZE - 1])
		buffer[0] = 0;

	m_mapFile = (succ > 0) ? CreateFileMappingFromApp(
		INVALID_HANDLE_VALUE,    // use paging file
       	NULL,                    // default security
       	PAGE_READWRITE,          // read/write access
       	ULONG64(mapSize),        // maximum object size (low-order DWORD)
       	buffer) 
		: NV_NULL;
#endif
	if (!m_mapFile)
	{
		return NV_FAIL;
	}
	m_baseAddress = MapViewOfFile(m_mapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);
	if (!m_baseAddress)
	{
		CloseHandle(m_mapFile);
		m_mapFile = NV_NULL;
		return NV_FAIL;
	}
	return NV_OK;
}

WinMemoryMappedFile::~WinMemoryMappedFile()
{
	if (m_baseAddress )
   	{
   		UnmapViewOfFile(m_baseAddress);
		CloseHandle(m_mapFile);
	}
}

} // namespace Common 
} // namespace nvidia
