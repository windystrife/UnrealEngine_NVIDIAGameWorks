#ifndef NV_CO_WIN_STRING_UTIL_H
#define NV_CO_WIN_STRING_UTIL_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoString.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

struct WinStringUtil
{	
		/// Append the wide chars to the string
	static void appendToString(const wchar_t* in, String& out);
		/// Append in as wide chars
	static void appendWideChars(const SubString& in, Array<wchar_t>& out);
};

} // namespace Common 
} // namespace nvidia

 /** @} */

#endif // NV_CO_WIN_STRING_UTIL_H