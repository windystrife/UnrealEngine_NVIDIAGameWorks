/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_CHAR_UTIL_H
#define NV_CO_CHAR_UTIL_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

class CharUtil
{
	NV_CO_DECLARE_CLASS_BASE(CharUtil);

		/// True if numeric
	NV_FORCE_INLINE static Bool isNumeric(Char c) { return (c >= '0' && c <= '9'); }
		/// True if it's alpha
	NV_FORCE_INLINE static Bool isAlpha(Char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

		/// True if it's alpha or numeric
	NV_FORCE_INLINE static Bool isAlphaNumeric(Char c) { return isAlpha(c) || isNumeric(c); }

		/// A symbol can be made of a-z/A-Z/0-9/_
	NV_FORCE_INLINE static Bool isSymbolChar(Char c) { return isAlpha(c) || isNumeric(c) || c == '_'; }
		/// First character of a symbol can be a-z/A-Z/_
	NV_FORCE_INLINE static Bool isSymbolFirstChar(Char c) { return isAlpha(c) || c == '_'; }

		/// Make a character lower case
	NV_FORCE_INLINE static Char toLower(Char c) { return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c; }
		/// make a character upper case
	NV_FORCE_INLINE static Char toUpper(Char c) { return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c; }
		/// True if it's a hex digit
	NV_FORCE_INLINE static Bool isHexDigit(Char c) { return (c >= 'a' && c <= 'f') || (c >='A' && c <= 'F') || (c >= '0' && c <= '9'); }

		/// True if it's an octal digit
	NV_FORCE_INLINE static Bool isOctalDigit(Char c) { return c >= '0' && c <= '7'; }

		/// Convert a value 0-15 to a hex digit
	static Char toHexDigit(Int i) { return (i >= 0 && i <= 9) ? Char('0' + i) : ((i >= 10 && i <= 15) ? Char(i - 10 + 'A') : '0'); } 
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_CHAR_UTIL_H
