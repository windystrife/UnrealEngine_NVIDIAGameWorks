/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoStringEscapeUtil.h"

#include "NvCoCharUtil.h"
#include "NvCoParseUtil.h"

namespace nvidia {
namespace Common {

Int StringEscapeUtil::extractEscaped(const SubString& in, Char& out)
{
	const Char* cur = in.begin();
	const Char* end = in.end();

	if (cur >= end || *cur != '\\')
	{
		return 0;
	}

	cur++;
	if (cur >= end)
	{
		return 0;
	}

	switch (CharUtil::toLower(*cur))
	{
		//        case 'e':
		//            out = '\e';
		//            return 2;
		case 'b':
			out = '\b';
			return 2;
		case 'f':
			out = '\f';
			return 2;
		case 'n':
			out = '\n';
			return 2;
		case 'r':
			out = '\r';
			return 2;
		case 'a':
			out = '\a';
			return 2;
		case 't':
			out = '\t';
			return 2;
		case 'v':
			out = '\v';
			return 2;
		case '\'':
			out = '\'';
			return 2;
		case '"':
			out = '"';
			return 2;
		case '\\':
		{
			out = '\\';
			return 2;
		}
		case 'x':
		{
			cur++;
			// 3 hex digits
			if (end - cur >= 3 &&
				CharUtil::isHexDigit(cur[0]) &&
				CharUtil::isHexDigit(cur[1]) &&
				CharUtil::isHexDigit(cur[2]))
			{
				Int value = SubString(cur, 3).toInt(16);
				out = (Char)value;
				return 5;
			}
			break;
		}
		default:
		{
			if (end - cur >= 3 &&
				CharUtil::isOctalDigit(cur[0]) &&
				CharUtil::isOctalDigit(cur[1]) &&
				CharUtil::isOctalDigit(cur[2]))
			{
				Int value = SubString(cur, 3).toInt(8);
				out = (Char)value;
				return 4;
			}
		}
	}
	return 0;
}

/* static */Void StringEscapeUtil::concatChar(Char in, String& out)
{
	const Char* esc = NV_NULL;
	switch (in)
	{
		case '\b': esc = "\\b"; break;
		case '\f': esc = "\\f"; break;
		case '\n': esc = "\\n"; break;
		case '\r': esc = "\\r"; break;
		case '\a': esc = "\\a"; break;
		case '\t': esc = "\\t"; break;
		case '\v': esc = "\\v"; break;
		case '\'': esc = "\\'"; break;
		case '"': esc = "\\\""; break;
		default: break;
	}

	if (esc)
	{
		// It's an escape char
		NV_CORE_ASSERT(esc[2] == 0);
		out.concat(esc, 2);
	}
	else if ((in < ' ') || (in > 126))
	{
		// Does it need hex
		Char hexBuf[5] = "\\x0";
		hexBuf[3] = CharUtil::toHexDigit((Int(in) >> 4) & 0xf);
		hexBuf[4] = CharUtil::toHexDigit(in & 0xf);
		out.concat(hexBuf, 5);
	}
	else
	{
		out.concat(in);
	}
}

/* static */Void StringEscapeUtil::concatEscaped(const SubString& in, String& out)
{
	const Char* cur = in.begin();
	const Char* end = in.end();

	for (; cur != end; cur++)
	{
		const Char c = *cur;
		if (needsEscape(c))
		{
			concatChar(c, out);
		}
		else
		{
			out.concat(c);
		}
	}
}

/* static */Void StringEscapeUtil::concatUnescaped(const SubString& in, String& out)
{
	const Char* cur = in.begin();
	const Char* end = in.end();

	while (cur < end)
	{
		Char c = *cur;
		if (c == '\\')
		{
			/** Work out what was escaped */
			Char unesc;
			Int size = extractEscaped(SubString(SubString::INIT_SPAN, cur, end), unesc);
			if (size > 0)
			{
				out.concat(unesc);
				cur += size;
				continue;
			}
		}
		// Otherwise just append
		out.concat(c);
		cur++;
	}
}

} // namespace Common 
} // namespace nvidia
