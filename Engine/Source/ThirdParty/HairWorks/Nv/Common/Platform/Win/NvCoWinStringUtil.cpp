#include <Nv/Common/NvCoCommon.h>

#include <windows.h>

#include "NvCoWinStringUtil.h"

namespace nvidia {
namespace Common {

/* static */void WinStringUtil::appendToString(const wchar_t* in, String& out)
{
	Int outSize = ::WideCharToMultiByte(CP_UTF8, 0, in, -1, NV_NULL, 0, NV_NULL, false);
	if (outSize > 0)
	{
		Char* dst = out.requireSpace(outSize);
		::WideCharToMultiByte(CP_UTF8, 0, in, -1, dst, outSize, NV_NULL, false);
		out.changeSize(outSize);
	}
}

/* static */void WinStringUtil::appendWideChars(const SubString& in, Array<wchar_t>& out)
{
	const DWORD dwFlags = 0;
	int outSize = ::MultiByteToWideChar(CP_UTF8, dwFlags, in.begin(), int(in.getSize()), NV_NULL, 0);

	if (outSize > 0)
	{
		WCHAR* dst = out.expandBy(outSize + 1);
		::MultiByteToWideChar(CP_UTF8, dwFlags, in.begin(), int(in.getSize()), dst, outSize);
		// Make null terminated
		dst[outSize] = 0;
		// Remove terminating 0 from array
		out.popBack();
	}
}

} // namespace Common 
} // namespace nvidia
