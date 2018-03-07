/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */


#include <Nv/Common/Util/NvCoPathUtil.h>
#include <Nv/Common/NvCoLogger.h>

// this
#include "NvCoPathFinder.h"

namespace nvidia {
namespace Common {

Bool SimpleParentPathFinder::findPath(const SubString& pathIn, String& pathOut)
{
	if (PathUtil::exists(pathIn))
	{
		pathOut = pathIn;
		return true;
	}
	if (PathUtil::isAbsolutePath(pathIn))
	{
		return false;
	}
	// Retry with its parent folders
	pathOut = pathIn;
	for (Int i = 0; i < m_maxDepth; ++i)
	{
		pathOut.insert(0, "..\\");

		if (PathUtil::exists(pathOut))
		{
			String work;
			// Make absolute
			PathUtil::absolutePath(pathOut, work);
			work.swap(pathOut);
			// Found path
			return true;
		}
	}

#ifdef NV_DEBUG
	{
		String buf;
		buf << "Path '" << pathIn << "' not found.";
		NV_CO_LOG_WARN(buf.getCstr());
	}
#endif

	// Didn't find the path
	return false;
}

} // namespace Common 
} // namespace nvidia

