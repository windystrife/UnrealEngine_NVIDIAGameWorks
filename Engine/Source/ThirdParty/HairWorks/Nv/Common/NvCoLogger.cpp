/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoLogger.h"

// Needed for var args
#include <stdarg.h>
#include <stdio.h>

namespace nvidia {
namespace Common {

class IgnoreLogger: public Logger
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS(IgnoreLogger, Logger);
	void log(ELogSeverity severity, const Char* text, const Char* function, const Char* filename, Int lineNumber) NV_OVERRIDE 
	{
		NV_UNUSED(severity)
		NV_UNUSED(text)
		NV_UNUSED(function)
		NV_UNUSED(filename)
		NV_UNUSED(lineNumber)
	}
	void flush() NV_OVERRIDE {}
};

static IgnoreLogger s_ignoreLoggerInstance;

/* static */Logger* Logger::s_instance = NV_NULL;
/* static */Logger* Logger::s_ignoreLogger = &s_ignoreLoggerInstance;

void Logger::logError(const Char* text)
{
	log(LogSeverity::NON_FATAL_ERROR, text, NV_NULL, NV_NULL, 0);
}

void Logger::logErrorWithFormat(const Char* format, ...)
{
	va_list va;
	va_start(va, format);
	char msg[1024];
	vsprintf_s(msg, NV_COUNT_OF(msg), format, va);
	va_end(va);

	log(LogSeverity::NON_FATAL_ERROR, msg, NV_NULL, NV_NULL, 0);
}

/* static */Void Logger::doLogWithFormat(ELogSeverity severity, const Char* funcName, const Char* filename, int lineNumber, const Char* format, ...)
{
	ThisType* instance = getInstance();
	if (instance)
	{
		va_list va;
		va_start(va, format);
		char msg[1024];
		vsprintf_s(msg, NV_COUNT_OF(msg), format, va);
		va_end(va);

		instance->log(severity, msg, funcName, filename, lineNumber);
	}
}

/* static */ Void Logger::doLog(ELogSeverity severity, const Char* msg, const Char* funcName, const Char* filename, int lineNumber)
{
	ThisType* instance = getInstance();
	if (instance)
	{
		instance->log(severity, msg, funcName, filename, lineNumber);
	}
}

/* static */ Void Logger::doLogWithFormat(ELogSeverity severity, const Char* format, ...)
{
	ThisType* instance = getInstance();
	if (instance)
	{
		va_list va;
		va_start(va, format);
		char msg[1024];
		vsprintf_s(msg, NV_COUNT_OF(msg), format, va);
		va_end(va);

		instance->log(severity, msg, NV_NULL, NV_NULL, 0);
	}
}

/* static */ Void Logger::doLog(ELogSeverity severity, const Char* msg)
{
	ThisType* instance = getInstance();
	if (instance)
	{
		instance->log(severity, msg, NV_NULL, NV_NULL, 0);
	}
}

/* static */ const Char* Logger::getText(ELogSeverity severity)
{
	switch (severity)
	{
		case LogSeverity::DEBUG_INFO:	return "DEBUG";
		case LogSeverity::INFO:			return "INFO";
		case LogSeverity::WARNING:		return "WARN";
		case LogSeverity::NON_FATAL_ERROR: return "ERROR";
		case LogSeverity::FATAL_ERROR:	return "FATAL";
		default: break;
	}
	return "?";
}

} // namespace Common 
} // namespace nvidia

