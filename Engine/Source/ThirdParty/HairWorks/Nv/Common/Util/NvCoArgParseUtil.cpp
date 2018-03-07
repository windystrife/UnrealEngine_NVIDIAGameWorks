/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoArgParseUtil.h"

#include <Nv/Common/NvCoLogger.h>

#include "NvCoCharUtil.h"
#include "NvCoParseUtil.h"
#include "NvCoStringEscapeUtil.h"

namespace nvidia {
namespace Common {

/* !!!!!!!!!!!!!!!!!!!!!!!!!!! ArgParseInfo !!!!!!!!!!!!!!!!!!!!!!!!!!!! */


Void ArgParseInfo::addArg(const SubString& name, const SubString& comment, Arg::Type type, Void* data)
{
	if (ArgParseUtil::findIndex(name, m_args.begin(), m_args.getSize()) >= 0)
	{
		String msg;
		msg << "Parameter '" << name << "' is already defined";
		NV_CO_LOG_ERROR(msg.getCstr());
		return;
	}

	Arg& arg = m_args.expandOne();
	arg.m_name = name;
	arg.m_comment = comment;
	arg.m_type = type;
	arg.m_data = data;
	arg.m_groupIndex = m_groupIndex;
}

Result ArgParseInfo::parse(Int groupIndex)
{
	if (NV_SUCCEEDED(m_result))
	{
		m_result = ArgParseUtil::parseArgs(groupIndex, m_args.begin(), m_args.getSize(), m_params, m_errorText);
	}
	return m_result;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!! ArgParseUtil !!!!!!!!!!!!!!!!!!!!!!!!!!!! */

static const Char* _parseOther(const SubString& in)
{
	const Char* cur = in.begin();
	const Char* end = in.end();
	// Looking for white space to delimit end
	for (; cur < end; cur++)
	{
		const Char c = *cur;
		switch (c)
		{
			case ' ':
			case '\t':
			{
				return cur;
			}
			case '"':
			{
				return NV_NULL;
			}
		}
	}
	return cur;
}

static const Char* _parseSwitch(const SubString& in, SubString& keyOut, SubString& valueOut)
{
	const Char* end = in.end();
	const Char* cur = in.begin();

	Bool isBool = true;

	if (cur >= end || *cur != '-')
	{
		return NV_NULL;
	}
	cur++;
	if (cur < end && *cur == '-')
	{
		isBool = false;
		cur++;
	}

	{
		const Char* tokEnd = ParseUtil::parseSymbol(SubString(SubString::INIT_SPAN, cur, end));
		if (!tokEnd)
		{
			return NV_NULL;
		}
		keyOut = SubString(SubString::INIT_SPAN, cur, tokEnd);
		cur = tokEnd;
	}
	
	if (cur >= end || (*cur == ' ') || (*cur == '\t'))
	{
		if (isBool)
		{
			valueOut = "1";
		}
		return cur;
	}

	if (*cur != '=')
	{
		return NV_NULL;
	}
	cur++;

	{
		const Char* start = cur;
		const Char* tokEnd = start;
		if (cur < end)
		{
			if (*cur == '\"')
			{
				tokEnd = ParseUtil::parseString(SubString(SubString::INIT_SPAN, start, end));
			}
			else
			{
				tokEnd = _parseOther(SubString(SubString::INIT_SPAN, start, end));
			}
		}
		if (tokEnd == NV_NULL)
		{
			return NV_NULL;
		}
		valueOut = SubString(SubString::INIT_SPAN, start, tokEnd);
		return tokEnd;
	}
}

static const Char* _parse(const SubString& in)
{
	if (in.getSize() > 0)
	{
		switch (in[0])
		{
			case '-':
			{
				SubString key, value;
				return _parseSwitch(in, key, value);
			}
			case '"':
			{
				return ParseUtil::parseString(in);
			}
			default:
			{
				return _parseOther(in);
			}
		}
	}
	return NV_NULL;
}

/* static */Result ArgParseUtil::parse(const Char*const* in, Int size, String& errorOut, Array<SubString>& out)
{
	for (Int i = 0; i < size; i++)
	{
		SubString param(SubString::INIT_CSTR, in[i]);
		const Char* end = _parse(param);

		if (param.end() != end)
		{
			errorOut << "Unable to parse '" << param << "'";
			return NV_FAIL;
		}

		out.pushBack(param);
	}
	return NV_OK;
}

Result ArgParseUtil::parse(const SubString& in, String& errorOut, Array<SubString>& out)
{
	const Char* end = in.end();
	// Consume any whitespace at the start
	const Char* cur = ParseUtil::parseWhiteSpace(in);

	while (cur < end)
	{
		// Consume the parameter
		{
			const Char* start = cur;
			const Char* tokEnd = _parse(SubString(SubString::INIT_SPAN, cur, end));
			if (tokEnd == NV_NULL)
			{
				errorOut << "Unable to parse '" << SubString(SubString::INIT_SPAN, start, end) << "'";
				return NV_FAIL;
			}
			out.pushBack(SubString(SubString::INIT_SPAN, start, tokEnd));
			cur = tokEnd;
		}

		// Must be white space before next symbol or it's the end
		if (cur < end)
		{
			const Char* endWhite = ParseUtil::parseWhiteSpace(SubString(SubString::INIT_SPAN, cur, end));
			if (endWhite <= cur)
			{
				errorOut << "Unable to parse '" << SubString(SubString::INIT_SPAN, cur, end) << "'";
				return NV_FAIL;
			}
			cur = endWhite;
		} 
	}

	return NV_OK;
}

/* static */IndexT ArgParseUtil::findIndex(const SubString& name, const Arg* args, IndexT numArgs)
{
	for (IndexT i = 0; i < numArgs; i++)
	{
		if (args[i].m_name == name)
		{
			return i;
		} 
	}
	return -1;
}

/* static */Result ArgParseUtil::setArg(const Arg& arg, const SubString& value, String& errorOut, Void* dst)
{
	dst = dst ? dst : arg.m_data;
	NV_CORE_ASSERT(dst);

	switch (arg.m_type)
	{
		case Arg::TYPE_BOOL:
		{
			Bool& dstBool = *(Bool*)dst;
			if (value == "" || value == "1" || value.equalsI("true"))
			{
				dstBool = true;
				return NV_OK;
			}			
			if (value == "0" || value.equalsI("false") || value.equalsI("off"))
			{
				dstBool = false;
				return NV_OK;
			}
			break;
		}
		case Arg::TYPE_STRING:
		{
			String& dstString = *(String*)dst;

			if (value.getSize() > 0 && value[0] == '"')
			{
				// We need to unescape
				if (value.tail(-1) != "\"")
				{
					errorOut << "Badly formed string " << value;
					return NV_FAIL;
				}
				SubString contents = value.subStringWithEnd(1, -1);

				dstString.clear();
				StringEscapeUtil::concatUnescaped(contents, dstString);
				return NV_OK;
			}
			else
			{
				dstString = value;
				return NV_OK;
			}
			break;
		}
		case Arg::TYPE_INT:
		{
			Int& dstInt = *(Int*)dst;

			const Char* end = ParseUtil::parseIntegral(value);
			if (end != value.end())
			{
				errorOut << "Unable to parse integral '" << value << "'";
				return NV_FAIL;
			}

			dstInt = SubString(SubString::INIT_SPAN, value.begin(), end).toInt();
			return NV_OK;
		}
		default: break;
	}

	// 
	errorOut << "'" << arg.m_name << "' inappropriately set";
	return NV_FAIL;
}

/* static */Result ArgParseUtil::parseArgs(Int groupIndex, const Arg* args, IndexT numArgs, Array<SubString>& paramsInOut, String& errorOut)
{
	IndexT numParams = paramsInOut.getSize();
	for (IndexT i = 0; i < numParams; i++)
	{
		const SubString& param = paramsInOut[i];
		if (param.getSize() > 0 && param[0] == '-')
		{
			SubString key, value;
			const Char* end = _parseSwitch(param, key, value);
			if (!end)
			{
				return NV_FAIL;
			}

			IndexT argIndex = findIndex(key, args, numArgs);
			if (argIndex >= 0)
			{
				// Only do args in a specific group
				const Arg& arg = args[argIndex];
				if (groupIndex >= 0 && arg.m_groupIndex != groupIndex)
				{
					continue;
				}

				// Parse it
				NV_RETURN_ON_FAIL(setArg(args[argIndex], value, errorOut));

				// Remove from the params cos it was correctly set
				paramsInOut.removeAtCopyBack(i);
				numParams--;
				i--;
			}
		}
	}
	return NV_OK;
}

} // namespace Common 
} // namespace nvidia
