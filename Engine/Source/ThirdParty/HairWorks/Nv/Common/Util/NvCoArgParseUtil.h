/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_ARG_PARSE_UTIL_H
#define NV_CO_ARG_PARSE_UTIL_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/Container/NvCoArray.h>
#include <Nv/Common/NvCoString.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

class ArgParseUtil
{
	NV_CO_DECLARE_CLASS_BASE(ArgParseUtil);

	struct Arg
	{
		enum Type
		{
			TYPE_UNKNOWN,
			TYPE_STRING,
			TYPE_INT,
			TYPE_BOOL,
		};

			/// Set via single - prefix 
		Bool isSingleDash() const { return m_type == TYPE_BOOL; }
			/// Concat how the control works
		Void concatSwitch(String& out) const { out << (isSingleDash() ? SubString("-") : SubString("--")) << m_name; }
		
		Type m_type;			///< Defines the type of the parameter
		Int m_groupIndex;		///< The group index this parameter belongs to
		String m_name;			///< The name of the parameter
		String m_comment;		///< A comment about the meaning of the parameter
		Void* m_data;			///< Points to data of m_type
	};

		/// Parses the input and returns a string holding each of the delimited symbols
	static Result parse(const SubString& in, String& errorOut, Array<SubString>& out);
	static Result parse(const Char*const* in, Int size, String& errorOut, Array<SubString>& out);

		/// Parse args
	static Result parseArgs(Int groupIndex, const Arg* args, IndexT numArgs, Array<SubString>& paramsInOut, String& errorOut);

		/// Set the argument depending on the type
	static Result setArg(const Arg& arg, const SubString& value, String& errorOut, Void* dst = NV_NULL);

		/// Searches for first arg that has the name
	static IndexT findIndex(const SubString& name, const Arg* args, IndexT numArgs);
};

class ArgParseInfo
{
	NV_CO_DECLARE_CLASS_BASE(ArgParseInfo);

	typedef ArgParseUtil::Arg Arg;

	Int nextGroupIndex() { return ++m_groupIndex; }

	Void add(const SubString& name, const SubString& comment, Int& param) { return addArg(name, comment, Arg::TYPE_INT, &param); }
	Void add(const SubString& name, const SubString& comment, String& param) { return addArg(name, comment, Arg::TYPE_STRING, &param); }
	Void add(const SubString& name, const SubString& comment, Bool& param) { return addArg(name, comment, Arg::TYPE_BOOL, &param); }

	Void addArg(const SubString& name, const SubString& comment, Arg::Type type, Void* data);

		/// Parse
	Result parse(Int groupIndex = -1);

		/// 
	ArgParseInfo():m_result(NV_OK), m_groupIndex(0) {}

	Array<Arg> m_args;			/// All of the args
	Array<SubString> m_params;	///< The current active parameters

	Int m_groupIndex;
	String m_errorText;			///< Text describing error
	Result m_result;			///< Current result
};


} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_ARG_PARSE_UTIL_H
