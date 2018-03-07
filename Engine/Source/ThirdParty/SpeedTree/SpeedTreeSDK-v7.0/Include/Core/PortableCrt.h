///////////////////////////////////////////////////////////////////////  
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
//  Preprocessor

#pragma once
#include "Core/ExportBegin.h"
#include "Core/Types.h"
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <cassert>
#include <cstring>


///////////////////////////////////////////////////////////////////////  
//  Using the Windows version of the runtime?

#if !defined(__GNUC__) && !defined(__CELLOS_LV2__) && !defined(__APPLE__) && !defined(__SNC__) && !defined(NDEV)
	#define WINDOWS_SECURE_CRT
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  st_localtime

	ST_INLINE void st_localtime(struct tm* _tm, const time_t* time)
	{
		assert(_tm);
		assert(time);

		#ifdef WINDOWS_SECURE_CRT
			localtime_s(_tm, time);
		#else
			*_tm = *localtime(time);
		#endif
	}


	///////////////////////////////////////////////////////////////////////  
	//  st_vsnprintf

	#ifdef WINDOWS_SECURE_CRT
		#define st_vsnprintf(pBuffer, size, format, args) vsnprintf_s((pBuffer), (size), (size) - 1, (format), (args));
	#else
		#define st_vsnprintf(pBuffer, size, format, args) vsnprintf((pBuffer), (size) - 1, (format), (args)); pBuffer[(size) - 1] = '\0';
	#endif


	///////////////////////////////////////////////////////////////////////  
	//  st_sprintf

	inline void st_sprintf(st_char* pBuffer, size_t siSizeOfBuffer, const st_char* szFormat, ...)
	{
		va_list args;
		va_start(args, szFormat);
		st_vsnprintf(pBuffer, siSizeOfBuffer, szFormat, args);
		va_end(args);
	}


	///////////////////////////////////////////////////////////////////////  
	//  st_fopen

	inline st_bool st_fopen(FILE** pFile, const st_char* szFilename, const st_char* szMode) 
	{
		st_bool bSuccess = false;

		#ifdef WINDOWS_SECURE_CRT
			bSuccess = (fopen_s(pFile, szFilename, szMode) == 0);
		#else
			*pFile = fopen(szFilename, szMode);
			bSuccess = (*pFile != NULL);
		#endif

		return bSuccess;
	}


	///////////////////////////////////////////////////////////////////////  
	//  st_strerror

	inline void st_strerror(st_char* pBuffer, size_t siSizeOfBuffer, st_int32 nErrorNo)
	{
		#ifdef WINDOWS_SECURE_CRT
			(void) strerror_s(pBuffer, siSizeOfBuffer, nErrorNo);
		#else
			const st_char* pErrorString = strerror(nErrorNo);
			strncpy(pBuffer, pErrorString, st_min(strlen(pErrorString) + 1, siSizeOfBuffer));
		#endif
	}

} // end namespace SpeedTree

#include "Core/ExportEnd.h"
