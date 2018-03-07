// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.

#ifndef NV_HW_WIN_LOAD_SDK_H
#define NV_HW_WIN_LOAD_SDK_H

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoMemoryAllocator.h>
#include <Nv/Common/NvCoLogger.h>

#include <Nv/Common/Platform/Win/NvCoWinInclude.h>

namespace nvidia {
namespace HairWorks { 

/* !\brief Use this function to load HairSDK from dll
	\param [in] dllPath Path to the NvHairWorksxxx.*.dll file
	\param [in] version Version should match between this header and the dll
	\param [in] allocator If not NV_NULL, HairWorks will use this allocator for all internal CPU memory allocation
	\param [in] logHandler If not NV_NULL, HairWorks will use this log handler to process the log messages
	\param [in] debugMode HairWorks internally use only, this value must be zero
	\return The HairWorks SDK instance pointer will be returned. */
inline Sdk* loadSdk(const Char* dllPath, UInt32 version = NV_HAIR_VERSION, NvCo::MemoryAllocator* allocator = NV_NULL, NvCo::Logger* logger = NV_NULL, Int debugMode = 0)
{
	typedef Sdk* (__cdecl * Func)(UInt32, NvCo::MemoryAllocator*, NvCo::Logger*, Int);
	HMODULE hairDllModule = LoadLibraryA(dllPath);
	if (hairDllModule)
	{
		Func createProc = (Func)::GetProcAddress(hairDllModule, "NvHairWorks_Create");
		if (!createProc)
		{
			FreeLibrary(hairDllModule);
			hairDllModule = NV_NULL;
			if (logger)
			{
				logger->log(NvCo::LogSeverity::NON_FATAL_ERROR, "HairWorks dll file not found", NV_FUNCTION_NAME, __FILE__, __LINE__);
			}
		}
		else
		{
			return createProc(version, allocator, logger, debugMode);
		}
	}
	else
	{
		::DWORD lastErr = ::GetLastError();
		char* msgBuf;
		::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NV_NULL, lastErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			(LPSTR)&msgBuf, 0, NV_NULL);
		if (logger)
		{
			logger->log(NvCo::LogSeverity::NON_FATAL_ERROR, "HairWorks dll file not found", NV_FUNCTION_NAME, __FILE__, __LINE__);
		}
		::LocalFree(msgBuf);
	}

	return NV_NULL;
}

} // namespace HairWorks
} // namespace nvidia

#endif // NV_HW_WIN_LOAD_SDK_H