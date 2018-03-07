/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_LOGGER_H
#define NV_CO_LOGGER_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! The log severities available */
class LogSeverity { LogSeverity(); public: enum Enum
{
	DEBUG_INFO,			///< Debugging info - only available on debug builds.
	INFO,				///< Informational - noting is wrong.
	WARNING,			///< Warning - something may be 100% correct or optimal. Something might need fixing.
	NON_FATAL_ERROR,			///< Something is wrong and needs fixing, but execution can continue.
	FATAL_ERROR,				///< Something is seriously wrong, execution cannot continue.
	COUNT_OF, 
}; };
typedef LogSeverity::Enum ELogSeverity;

/*! Flags that can be used in Logger implementation to filter results */
class LogSeverityFlag { LogSeverityFlag(); public: enum Enum
{
	DEBUG_INFO		= 1 << Int(LogSeverity::DEBUG_INFO),			
	INFO			= 1 << Int(LogSeverity::INFO),				
	WARNING			= 1 << Int(LogSeverity::WARNING),			
	NON_FATAL_ERROR	= 1 << Int(LogSeverity::NON_FATAL_ERROR),	
	FATAL_ERROR		= 1 << Int(LogSeverity::FATAL_ERROR),				
}; };
typedef LogSeverityFlag::Enum ELogSeverityFlag;

#ifdef NV_DEBUG

#	define NV_CO_LOG_LOCATION NV_FUNCTION_NAME, __FILE__, __LINE__
#	define NV_CO_LOG_ERROR(text)	::NvCo::Logger::doLog(::NvCo::LogSeverity::NON_FATAL_ERROR, text, NV_CO_LOG_LOCATION);
#	define NV_CO_LOG_WARN(text)	::NvCo::Logger::doLog(::NvCo::LogSeverity::WARNING, text, NV_CO_LOG_LOCATION);
#	define NV_CO_LOG_INFO(text)	::NvCo::Logger::doLog(::NvCo::LogSeverity::INFO, text, NV_CO_LOG_LOCATION);
#	define NV_CO_LOG_FATAL(text)  ::NvCo::Logger::doLog(::NvCo::LogSeverity::FATAL_ERROR, text, NV_CO_LOG_LOCATION);
#	define NV_CO_LOG_DEBUG(text)  ::NvCo::Logger::doLog(::NvCo::LogSeverity::DEBUG_INFO, text, NV_CO_LOG_LOCATION);

#	define NV_CO_LOG_ERROR_FORMAT(format, ...)	::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::NON_FATAL_ERROR, NV_CO_LOG_LOCATION, format, __VA_ARGS__);
#	define NV_CO_LOG_WARN_FORMAT(format, ...)	::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::WARNING, NV_CO_LOG_LOCATION, format, __VA_ARGS__);
#	define NV_CO_LOG_INFO_FORMAT(format, ...)	::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::INFO, NV_CO_LOG_LOCATION, format, __VA_ARGS__);
#	define NV_CO_LOG_FATAL_FORMAT(format, ...)  ::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::FATAL_ERROR, NV_CO_LOG_LOCATION, format, __VA_ARGS__);

#	define NV_CO_LOG_DEBUG_FORMAT(format, ...)  ::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::DEBUG_INFO, NV_CO_LOG_LOCATION, format, __VA_ARGS__);

#else

#	define NV_CO_LOG_ERROR(text)	::NvCo::Logger::doLog(::Nv::LogSeverity::NON_FATAL_ERROR, text);
#	define NV_CO_LOG_WARN(text)	::NvCo::Logger::doLog(::Nv::LogSeverity::WARNING, text);
#	define NV_CO_LOG_INFO(text)	::NvCo::Logger::doLog(::Nv::LogSeverity::INFO, text);
#	define NV_CO_LOG_FATAL(text)  ::NvCo::Logger::doLog(::Nv::LogSeverity::FATAL_ERROR, text);
#	define NV_CO_LOG_DEBUG(text)  

#	define NV_CO_LOG_ERROR_FORMAT(format, ...)	::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::NON_FATAL_ERROR, format, __VA_ARGS__);
#	define NV_CO_LOG_WARN_FORMAT(format, ...)	::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::WARNING, format, __VA_ARGS__);
#	define NV_CO_LOG_INFO_FORMAT(format, ...)	::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::INFO, format, __VA_ARGS__);
#	define NV_CO_LOG_FATAL_FORMAT(format, ...)  ::NvCo::Logger::doLogWithFormat(::NvCo::LogSeverity::FATAL_ERROR, format, __VA_ARGS__);

#	define NV_CO_LOG_DEBUG_FORMAT(format, ...) 

#endif

/// Logger interface
class Logger
{
	NV_CO_DECLARE_POLYMORPHIC_CLASS_BASE(Logger)

		/// Report a message.
	virtual void log(ELogSeverity severity, const Char* text, const Char* function, const Char* filename, Int lineNumber) = 0;
		/// Flush the contents to storage
	virtual void flush() {}

		/// Log an error (without function, filename or line number)
	void logError(const Char* text);
	void logErrorWithFormat(const Char* format, ...);

		/// Log with format 
	static Void doLogWithFormat(ELogSeverity severity, const Char* function, const Char* filename, int lineNumber, const Char* format, ...);
	static Void doLog(ELogSeverity severity, const Char* msg, const Char* function, const Char* filename, int lineNumber);

	static Void doLogWithFormat(ELogSeverity severity, const Char* format, ...);
	static Void doLog(ELogSeverity severity, const Char* msg);

		/// Get the log severity as text
	static const Char* getText(ELogSeverity severity);

		// Get the current global logging instance
	NV_FORCE_INLINE static Logger* getInstance() { return s_instance; }
		/// Set the global logging instance
	static Void setInstance(Logger* logger) { s_instance = logger; }

		/// Anything sent to the ignore logger will be thrown away
	static Logger* getIgnoreLogger() { return s_ignoreLogger; }

private:
	static Logger* s_instance;
	static Logger* s_ignoreLogger;
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_LOGGER_H