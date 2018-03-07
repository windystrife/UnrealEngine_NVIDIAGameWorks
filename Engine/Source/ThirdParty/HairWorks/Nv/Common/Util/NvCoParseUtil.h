/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_PARSE_UTIL_H
#define NV_CO_PARSE_UTIL_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/Container/NvCoArray.h>
#include <Nv/Common/NvCoString.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

class ParseUtil
{
	NV_CO_DECLARE_CLASS_BASE(ParseUtil);

		/// Parses a quoted string. Takes into account \ escapes.  
		/// Returns NV_NULL on error, or the end of the successful parse
	static const Char* parseString(const SubString& in);
		/// Parses a symbol [a-zA-Z_]([a-zA-Z0-0_])*
		/// Returns NV_NULL on error, or the end of the successful parse
	static const Char* parseSymbol(const SubString& in);
		/// Parse an integral -?[0-9]+
		/// Returns NV_NULL on error, or the end of the successful parse
	static const Char* parseIntegral(const SubString& in);
		/// Parse white space (spacees and tabs and carrage returns etc)
	static const Char* parseWhiteSpace(const SubString& in);
};


} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_ARG_PARSE_UTIL_H
