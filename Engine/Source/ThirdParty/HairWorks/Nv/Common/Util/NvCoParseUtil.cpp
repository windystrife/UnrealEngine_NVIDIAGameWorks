
#include "NvCoParseUtil.h"

#include "NvCoCharUtil.h"

namespace nvidia {
namespace Common {

/* static */const Char* ParseUtil::parseString(const SubString& in)
{
	if (in.getSize() < 2 || in[0] != '"')
	{
		return NV_NULL;
	}

	const Char* end = in.end();
	const Char* cur = in.begin() + 1;

	Bool isEsc = false;
	while (cur < end)
	{
		const Char c = *cur;
		if (c == 0)
		{
			// Badly formed
			return NV_NULL;
		}
		if (isEsc)
		{
			isEsc = false;
		}
		else
		{
			if (c == '\\')
			{
				isEsc = true;
			}
			else if (c == '"')
			{
				return cur + 1;
			}
		}
		cur++;
	}
	// Didn't hit closing "
	return NV_NULL;
}

/* static */const Char* ParseUtil::parseSymbol(const SubString& in)
{	
	const Char* end = in.end();
	const Char* cur = in.begin();
	// Parse first char
	if (!(cur < end && CharUtil::isSymbolFirstChar(*cur)))
	{
		return NV_NULL;
	}
	// Parse the rest
	for (cur++; cur < end && CharUtil::isSymbolChar(*cur); cur++);
	return cur;
}

/* static */const Char* ParseUtil::parseIntegral(const SubString& in)
{
	const Char* cur = in.begin();
	const Char* end = in.end();
	if (cur < end && *cur == '-')
	{
		cur++;
	}
	const Char* start = cur;
	while (cur < end && CharUtil::isNumeric(*cur)) cur++;
	// Must be one or move characters
	return (cur > start) ? cur : NV_NULL;
}

/* static */const Char* ParseUtil::parseWhiteSpace(const SubString& in)
{
	const Char* cur = in.begin();
	const Char* end = in.end();
	// Looking for white space to delimit end
	for (; cur < end; cur++)
	{
		const Char c = *cur;
		if (!(c == ' ' || c == '\t'))
		{
			return cur;
		}
	}
	return cur;
}

} // namespace Common 
} // namespace nvidia
