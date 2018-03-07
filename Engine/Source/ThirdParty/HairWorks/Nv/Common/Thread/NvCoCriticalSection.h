/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_CRITICAL_SECTION_H
#define NV_CO_CRITICAL_SECTION_H

#include <Nv/Common/NvCoCommon.h>

#ifdef NV_MICROSOFT_FAMILY
#	include <Nv/Common/Platform/Win/NvCoWinCriticalSection.h>
#endif

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

#ifdef NV_MICROSOFT_FAMILY
typedef WinCriticalSection CriticalSection;
#endif

///////////////////////////////////////////////////////////////////////////////////////
class ScopeCriticalSection
{
public:
	ScopeCriticalSection(CriticalSection& criticalSection):
		m_criticalSection(criticalSection)
	{
		m_criticalSection.lock();
	}
	~ScopeCriticalSection()
	{
		m_criticalSection.unlock();
	}
private:
	// Disable
	ScopeCriticalSection();
	void operator=(const ScopeCriticalSection& rhs);

	CriticalSection& m_criticalSection;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_CRITICAL_SECTION_H
