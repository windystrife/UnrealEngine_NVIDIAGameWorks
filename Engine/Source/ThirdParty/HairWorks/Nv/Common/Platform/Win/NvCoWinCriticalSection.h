/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_WIN_CRITICAL_SECTION_H
#define NV_CO_WIN_CRITICAL_SECTION_H

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/Platform/Win/NvCoWinInclude.h>

/** \addtogroup common 
@{ 
*/

namespace nvidia {
namespace Common {

class WinCriticalSection
{
public:
	NV_FORCE_INLINE WinCriticalSection()
	{
		::InitializeCriticalSection(&m_criticalSection);
	}
	NV_FORCE_INLINE ~WinCriticalSection()
	{
		::DeleteCriticalSection(&m_criticalSection);
	}
	NV_FORCE_INLINE void lock()
	{
		::EnterCriticalSection(&m_criticalSection);
	}
	NV_FORCE_INLINE void unlock()
	{
		::LeaveCriticalSection(&m_criticalSection);
	}
private:
	CRITICAL_SECTION m_criticalSection;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_WIN_CRITICAL_SECTION_H
