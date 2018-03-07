///////////////////////////////////////////////////////////////////////
//	Utility.h
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
//		IDV, Inc.
//		Web: http://www.idvinc.com


///////////////////////////////////////////////////////////////////////
//  Preprocessor

#pragma once
#include <cstdio>
#include <cstdarg>
#if defined(SPEEDTREE_OPENGL) && !defined(NDEV)
	#define GLEW_STATIC
	//#define GLEW_NO_GLU
	#include "Utilities/glew.h"
    #ifdef __APPLE__
        #include <OpenGL/glu.h>
    #else
        #include <GL/glu.h>
    #endif
#endif
#if defined(_WIN32) && !defined(_XBOX)
	#include <windows.h>
	#pragma warning (disable : 4996)
#endif
#ifdef _XBOX
	#include <Xtl.h>
#endif
#if defined(NDEV)
	#include <errno.h>
#elif defined(__GNUC__)
	#include <errno.h>
	#ifndef __ORBIS__
		#include <dirent.h>
	#endif
#endif
#include "Core/Core.h"
#include "Core/String.h"


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////
//  INTERNAL_FORMAT_STRING macro
//  This code is used in the next few functions. Can't use a function call
//  because of the variable arguments.

#if !defined(__GNUC__) && !defined(__CELLOS_LV2__) && !defined(__APPLE__) && !defined(__SNC__) && !defined(NDEV) && !defined(vsnprintf)
	#define vsnprintf _vsnprintf
#endif

#define INTERNAL_FORMAT_STRING \
	const st_int32 c_nMaxStringSize = 2048; \
	st_char szBuffer[c_nMaxStringSize]; \
	va_list vlArgs; \
	va_start(vlArgs, pField); \
	(void) st_vsnprintf(szBuffer, c_nMaxStringSize - 1, pField, vlArgs); \
	szBuffer[c_nMaxStringSize - 1] = '\0'; \
	CBasicFixedString<c_nMaxStringSize> strValue(szBuffer);


///////////////////////////////////////////////////////////////////////
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	////////////////////////////////////////////////////////////
	// Error

	inline void Error(const char* pField, ...)
	{
		INTERNAL_FORMAT_STRING;

		#ifdef _DURANGO
			OutputDebugStringA("[Error]: ");
			OutputDebugStringA(strValue.c_str( ));
			OutputDebugStringA("\n");
		#elif defined(_WIN32) && !defined(_XBOX)
			(void) MessageBoxA(NULL, strValue.c_str( ), "SpeedTree ERROR", MB_ICONERROR | MB_OK);
		#else
			printf("\n[Error]: %s\n", strValue.c_str( ));
		#endif
	}


	////////////////////////////////////////////////////////////
	// Warning

	inline void Warning(const char* pField, ...)
	{
		INTERNAL_FORMAT_STRING;

		// display the warning
		#ifdef _DURANGO
			OutputDebugStringA("[Warning]: ");
			OutputDebugStringA(strValue.c_str( ));
			OutputDebugStringA("\n");
		#else
			printf("[Warning]: %s\n", strValue.c_str( ));
		#endif
	}


	////////////////////////////////////////////////////////////
	// Internal

	inline void Internal(const char* pField, ...)
	{
		INTERNAL_FORMAT_STRING;

		// display the fatal error
		#ifdef _DURANGO
			OutputDebugStringA("SpeedTree INTERNAL ERROR: ");
			OutputDebugStringA(strValue.c_str( ));
			OutputDebugStringA("\n");
		#elif defined(_WIN32) && !defined(_XBOX)
			(void) MessageBoxA(NULL, strValue.c_str( ), "SpeedTree INTERNAL ERROR", MB_ICONERROR | MB_OK);
		#else
			printf("SpeedTree INTERNAL ERROR: %s\n", strValue.c_str( ));
		#endif
	}


	////////////////////////////////////////////////////////////
	// Report

	inline void Report(const char* pField, ...)
	{
		INTERNAL_FORMAT_STRING;

		#ifdef _DURANGO
			OutputDebugStringA(strValue.c_str( ));
			OutputDebugStringA("\n");
		#else
			printf("%s\n", strValue.c_str( ));
		#endif
	}


	////////////////////////////////////////////////////////////
	// Interpolate

	template <class T> inline T Interpolate(const T& tStart, const T& tEnd, st_float32 fPercent)
	{
		return static_cast<T>((tStart + (tEnd - tStart) * fPercent));
	}


	/////////////////////////////////////////////////////////////////////////////
	//	ColorToUInt

	inline st_uint32 ColorToUInt(st_float32 fR, st_float32 fG, st_float32 fB, st_float32 fA = 1.0f)
	{
		st_uint32 uiRed = (unsigned int) (fR * 255.0f);
		st_uint32 uiGreen = (unsigned int) (fG * 255.0f);
		st_uint32 uiBlue = (unsigned int) (fB * 255.0f);
		st_uint32 uiAlpha = (unsigned int) (fA * 255.0f);

		return ((uiRed << 0) + (uiGreen << 8) + (uiBlue << 16) + (uiAlpha << 24));
	}


	/////////////////////////////////////////////////////////////////////////////
	//	ColorToUInt

	inline st_uint32 ColorToUInt(const st_float32 afColor[4])
	{
		return ColorToUInt(afColor[0], afColor[1], afColor[2], afColor[3]);
	}


	/////////////////////////////////////////////////////////////////////////////
	//	ColorToFloats

	inline void ColorToFloats(st_uint32 uiColor, st_float32 afColor[4])
	{
		afColor[0] = ((uiColor & 0x000000FF) >> 0) / 255.0f;
		afColor[1] = ((uiColor & 0x0000FF00) >> 8) / 255.0f;
		afColor[2] = ((uiColor & 0x00FF0000) >> 16) / 255.0f;
		afColor[3] = ((uiColor & 0xFF000000) >> 24) / 255.0f;
	}


	////////////////////////////////////////////////////////////
	// Clamp

	template <class T> inline T Clamp(T tValue, T tMinValue, T tMaxValue)
	{
		T tReturn = st_min(tValue, tMaxValue);

		return st_max(tReturn, tMinValue);
	}


	////////////////////////////////////////////////////////////
	// Swap

	template <class T> inline void Swap(T& tA, T& tB)
	{
		T tTemp = tA;
		tA = tB;
		tB = tTemp;
	}


	////////////////////////////////////////////////////////////
	// Frac

	inline SpeedTree::st_float32 Frac(SpeedTree::st_float32 x)
	{
		return x - floor(x);
	}


	////////////////////////////////////////////////////////////
	// SwapEndian4Bytes

	template <class T> inline void SwapEndian4Bytes(T& tValue)
	{
		assert(sizeof(T) == 4);

		union UUnion4Bytes
		{
			st_byte	m_acBytes[4];
			T		m_tValue;
		};

		UUnion4Bytes uOriginal;
		uOriginal.m_tValue = tValue;

		UUnion4Bytes uSwapped;
		uSwapped.m_acBytes[0] = uOriginal.m_acBytes[3];
		uSwapped.m_acBytes[1] = uOriginal.m_acBytes[2];
		uSwapped.m_acBytes[2] = uOriginal.m_acBytes[1];
		uSwapped.m_acBytes[3] = uOriginal.m_acBytes[0];

		tValue = uSwapped.m_tValue;
	}


	////////////////////////////////////////////////////////////
	// SwapEndian2Bytes

	template <class T> inline void SwapEndian2Bytes(T& tValue)
	{
		assert(sizeof(T) == 2);

		union UUnion2Bytes
		{
			st_byte	m_acBytes[2];
			T		m_tValue;
		};

		UUnion2Bytes uOriginal;
		uOriginal.m_tValue = tValue;

		UUnion2Bytes uSwapped;
		uSwapped.m_acBytes[0] = uOriginal.m_acBytes[1];
		uSwapped.m_acBytes[1] = uOriginal.m_acBytes[0];

		tValue = uSwapped.m_tValue;
	}


	////////////////////////////////////////////////////////////
	// SwapEndianVec3

	inline void SwapEndianVec3(SpeedTree::Vec3& vVec3)
	{
		SwapEndian4Bytes<st_float32>(vVec3.x);
		SwapEndian4Bytes<st_float32>(vVec3.y);
		SwapEndian4Bytes<st_float32>(vVec3.z);
	}


	////////////////////////////////////////////////////////////
	// OrderPair
	//
	// Swaps min and max if min is greater than max

	template <class T> inline void OrderPair(T& tMin, T& tMax)
	{
		if (tMin > tMax)
		{
			T tTmp = tMin;
			tMin = tMax;
			tMax = tTmp;
		}
	}


	///////////////////////////////////////////////////////////////////////
	//  PrintSpeedTreeError

	inline void PrintSpeedTreeErrors(const char* pLocation = NULL)
	{
		const char* pError = SpeedTree::CCore::GetError( );
		while (pError)
		{
			if (pLocation)
				Warning("(%s): %s", pLocation, pError);
			else
				Warning("%s", pError);

			pError = SpeedTree::CCore::GetError( );
		}
	}


	#if defined(SPEEDTREE_OPENGL) && !defined(NDEV)
		///////////////////////////////////////////////////////////////////////
		//  PrintOpenGLErrors

		inline void PrintOpenGLErrors(const char* pLocation = NULL)
		{
			// query and print opengl errors
			GLenum eError = glGetError( );
			while (eError != GL_NO_ERROR)
			{
				const char* pErrorString = (const char*) gluErrorString(eError);
				CFixedString strErrorCode;
				if (!pErrorString)
					strErrorCode = CFixedString::Format("Unknown code %d", eError);

				// sometimes the error code yields a NULL string from gluErrorString
				if (pLocation)
					Warning("OpenGL error (%s): [%s]\n", pLocation, pErrorString ? pErrorString : strErrorCode.c_str( ));
				else
					Warning("OpenGL error: [%s]\n", pErrorString ? pErrorString : strErrorCode.c_str( ));

				// continue to retrieve until empty
				eError = glGetError( );
			}
		}
	#endif


	///////////////////////////////////////////////////////////////////////
	//  Strcmpi

	inline bool Strcmpi(const SpeedTree::CFixedString& strA, const SpeedTree::CFixedString& strB)
	{
		#if defined(__GNUC__) || defined(__SNC__) || defined(NDEV)
			return strcasecmp(strA.c_str( ), strB.c_str( )) == 0;
		#else
			return _strcmpi(strA.c_str( ), strB.c_str( )) == 0;
		#endif
	}


	///////////////////////////////////////////////////////////////////////
	//  IsAbsolutePath

	inline st_bool IsAbsolutePath(const SpeedTree::CFixedString& strPath)
	{
		st_bool bAbsolutePath = false;

		if (!strPath.empty( ))
		{
			char szLeadChar = strPath[0];

			if (szLeadChar == '/' || szLeadChar == '\\')
				bAbsolutePath = true;
			else
			{
				// does a colon appear anywhere in the filename?
				size_t siPos = strPath.find(':');
				if (siPos != (size_t)SpeedTree::CFixedString::npos)
					bAbsolutePath = true;
			}
		}

		return bAbsolutePath;
	}


	///////////////////////////////////////////////////////////////////////
	//  ScanFolder

	inline void ScanFolder(const char* pFolder, const char* pExtension, bool bRecurse, CArray<CFixedString>& vFiles)
	{
		CFixedString strFolder(pFolder);
		CFixedString strExtension(pExtension);

		#if defined(_WIN32) || defined(_XBOX) || defined(_DURANGO)
			WIN32_FIND_DATAA sDirEntry;
			CFixedString strSearch = CFixedString(strFolder) + "\\*";

			HANDLE hDirHandle = FindFirstFileA(strSearch.c_str( ), &sDirEntry);
			if (hDirHandle != INVALID_HANDLE_VALUE)
			{
				// check "../data" for any folder
				do
				{
					CFixedString strFilename(sDirEntry.cFileName);
					if (Strcmpi(strFilename.Extension( ), strExtension))
						vFiles.push_back(strFolder + "\\" + strFilename);
					else if (bRecurse &&
							 sDirEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
							 strcmp(sDirEntry.cFileName, ".") != 0 &&
							 strcmp(sDirEntry.cFileName, "..") != 0)
					{
						ScanFolder((strFolder + "\\" + CFixedString(sDirEntry.cFileName)).c_str( ), pExtension, bRecurse, vFiles);
					}

				} while (FindNextFileA(hDirHandle, &sDirEntry));

				(void) FindClose(hDirHandle);
			}
			else
				Warning("Failed to scan folder [%s] with wildcard [%s]\n", strFolder.c_str( ), strSearch.c_str( ));

		#elif defined(__GNUC__) && !defined(NDEV) && !defined(__ORBIS__)

			DIR* pDir = opendir(strFolder.c_str( ));
			if (pDir)
			{
				struct dirent* pEntry = readdir(pDir);
				while (pEntry)
				{
					CFixedString strFilename(pEntry->d_name);
					if (Strcmpi(strFilename.Extension( ), strExtension))
					{
						strFilename = strFolder + "/" + strFilename;
						vFiles.push_back(strFilename);
					}
					else if (bRecurse &&
							 pEntry->d_type == DT_DIR &&
							 strcmp(pEntry->d_name, ".") &&
							 strcmp(pEntry->d_name, ".."))
					{
						ScanFolder(pEntry->d_name, pExtension, bRecurse, vFiles);
					}

					pEntry = readdir(pDir);
				}
				closedir(pDir);
			}
			else
			{
				char szError[512];
				st_strerror(szError, 512, errno);
				Warning("Failed to scan folder [%s] - %s\n", strFolder.c_str( ), szError);
			}
		#endif
	}

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif
