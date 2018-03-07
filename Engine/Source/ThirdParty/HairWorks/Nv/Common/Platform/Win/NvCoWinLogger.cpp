/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include <Nv/Common/Platform/Win/NvCoWinInclude.h>

#include "NvCoWinLogger.h"
#include <Nv/Common/NvCoString.h>

#include <stdlib.h>
#include <string.h>

namespace nvidia {
namespace Common {

/* static */WinLogger WinLogger::s_singleton;

void WinLogger::log(ELogSeverity severity, const Char* text, const Char* function, const Char* filename, Int lineNumber)
{
	String buf;

	buf << SubString(SubString::INIT_CSTR, Logger::getText(severity)) << " ";
	if (text)
	{
		buf << SubString(SubString::INIT_CSTR, text);
	}

	if (function)
	{
		buf << "\n" << SubString(SubString::INIT_CSTR, function) << " (" << lineNumber << ") ";
		if (filename)
		{
			buf << SubString(SubString::INIT_CSTR, filename);
		}
	}
	buf << "\n";

	OutputDebugStringA(buf.getCstr());
}

} // namespace Common 
} // namespace nvidia

