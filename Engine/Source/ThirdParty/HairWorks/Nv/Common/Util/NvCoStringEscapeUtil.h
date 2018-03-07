/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_STRING_ESCAPE_UTIL_H
#define NV_CO_STRING_ESCAPE_UTIL_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoString.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

class StringEscapeUtil
{
	NV_CO_DECLARE_CLASS_BASE(StringEscapeUtil);


	/* Map escape sequences into their equivalent symbols. Return the equivalent
	* ASCII character. *s is advanced past the escape sequence. If no escape
	* sequence is present, the current character is returned and the string
	* is advanced by one. The following are recognized:
	*
	*  \b  backspace
	*  \f  formfeed
	*  \n  newline
	*  \r  carriage return
	*  \s  space
	*  \t  tab
	*  \e  ASCII ESC character ('\033')
	*  \DDD    number formed of 1-3 octal digits
	*  \xDDD   number formed of 1-3 hex digits
	*  \^C C = any letter. Control code
	*/


		/// Extract a single character
	static Int extractEscaped(const SubString& in, Char& out);
		/// Concat single char, escaped as necessary
	static Void concatChar(Char in, String& out);
		/// Concat in escaped to out
	static Void concatEscaped(const SubString& in, String& out);
		/// Concat in unescaped to out
	static Void concatUnescaped(const SubString& in, String& out);
		/// Returns true if a character will need escaping
	static Bool needsEscape(Char c) { return (c < ' ') || (c == '\'') || (c == '"') || (c > 126); }

};



} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_STRING_ESCAPE_UTIL_H
