/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_WIN_LOGGER_H
#define NV_CO_WIN_LOGGER_H

#include <Nv/Common/NvCoCommon.h>

#include <Nv/Common/NvCoLogger.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/// Windows implementation of a logger. Writes errors to the debug stream
class WinLogger: public Logger
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS(WinLogger, Logger);

	// Logger
	void log(ELogSeverity severity, const Char* text, const Char* function, const Char* filename, Int lineNumber) NV_OVERRIDE;
	
		/// Get the singleton
	NV_FORCE_INLINE static WinLogger* getSingleton() { return &s_singleton; }

private:
	static WinLogger s_singleton;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_WIN_LOGGER_H