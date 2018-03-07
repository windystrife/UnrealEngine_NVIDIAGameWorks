/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_PATH_FINDER_H
#define NV_CO_PATH_FINDER_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

#include <Nv/Common/NvCoString.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

class PathFinder
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(PathFinder)

	virtual Bool findPath(const SubString& in, String& pathOut) = 0;
};

class SimpleParentPathFinder: public PathFinder
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS(SimpleParentPathFinder, PathFinder)
	
	// PathFinder
	virtual Bool findPath(const SubString& in, String& pathOut) NV_OVERRIDE;
	
		/// Ctor 
	SimpleParentPathFinder(Int maxDepth = 7):
		m_maxDepth(maxDepth)
	{
	}

	Int m_maxDepth;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_PATH_FINDER_H