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
#include <stddef.h>

// uncomment to have the st_assert() macro function in release mode; errors will
// be reported through the CCore::SetError() function
#define SPEEDTREE_ASSERT_WARNS_IN_RELEASE


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


////////////////////////////////////////////////////////////
// ST_UNREF_PARAM

#define ST_UNREF_PARAM(x) (void)(x)


///////////////////////////////////////////////////////////////////////
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////
	//  Setup types based on platform, but nothing too complex

	typedef bool			st_bool;
	typedef char			st_int8;
	typedef char			st_char;
	typedef short			st_int16;
	typedef int				st_int32;
	typedef long long		st_int64;
	typedef unsigned char	st_uint8;
	typedef unsigned char	st_byte;
	typedef unsigned char	st_uchar;
	typedef unsigned short	st_uint16;
	typedef unsigned int	st_uint32;
	typedef float			st_float32;
	typedef double			st_float64;
	typedef size_t          st_sizet;


	///////////////////////////////////////////////////////////////////////
	//  Class st_float16 (half-float)

	class st_float16
	{
	public:
						st_float16( );
						st_float16(st_float32 fSinglePrecision);
						st_float16(const st_float16& hfCopy);

						operator st_float32(void) const;

	private:
			st_uint16	m_uiValue;
	};


	///////////////////////////////////////////////////////////////////////
	//  Compile time assertion

	#define ST_ASSERT_ON_COMPILE(expr) extern char AssertOnCompile[(expr) ? 1 : -1]

	ST_ASSERT_ON_COMPILE(sizeof(st_int8) == 1);
	ST_ASSERT_ON_COMPILE(sizeof(st_int16) == 2);
	ST_ASSERT_ON_COMPILE(sizeof(st_int32) == 4);
	ST_ASSERT_ON_COMPILE(sizeof(st_int64) == 8);
	ST_ASSERT_ON_COMPILE(sizeof(st_uint8) == 1);
	ST_ASSERT_ON_COMPILE(sizeof(st_uint16) == 2);
	ST_ASSERT_ON_COMPILE(sizeof(st_uint32) == 4);
	ST_ASSERT_ON_COMPILE(sizeof(st_float16) == 2);
	ST_ASSERT_ON_COMPILE(sizeof(st_float32) == 4);
	ST_ASSERT_ON_COMPILE(sizeof(st_float64) == 8);

	const st_int32 c_nSizeOfInt = 4;
	const st_int32 c_nSizeOfFloat = 4;
	const st_int32 c_nFixedStringLength = 256;

	// these types are still sometimes used internally with the following assumptions
	ST_ASSERT_ON_COMPILE(sizeof(int) == c_nSizeOfInt);
	ST_ASSERT_ON_COMPILE(sizeof(float) == c_nSizeOfFloat);


	///////////////////////////////////////////////////////////////////////
	//  Special SpeedTree assertion macro

	#ifndef NDEBUG
		#ifdef __psp2__
			#define st_assert(condition, explanation) assert((condition) && (explanation != 0))
		#else
			#define st_assert(condition, explanation) assert((condition) && (explanation))
		#endif
	#elif defined(SPEEDTREE_ASSERT_WARNS_IN_RELEASE)
		#define st_assert(condition, explanation)
	#else
		#define st_assert(condition, explanation)
	#endif


	///////////////////////////////////////////////////////////////////////
	//  Function equivalents of __min and __max macros

	template <class T> inline T st_min(const T& a, const T& b)
	{
		return (a < b) ? a : b;
	}

	template <class T> inline T st_max(const T& a, const T& b)
	{
		return (a > b) ? a : b;
	}


	///////////////////////////////////////////////////////////////////////
	//  Inline macro (we've found that MSDEV compiler ignores "inline" a bit too freely)

	#ifdef WIN32
		#define ST_INLINE __forceinline
	#else
		#define ST_INLINE inline
	#endif


	///////////////////////////////////////////////////////////////////////
	//  Structure Enumeration

	template <class E, class T>
	struct ST_DLL_LINK Enumeration
	{
			Enumeration( );
			Enumeration(E eValue);
			Enumeration(st_int32 nValue);

			operator E(void) const;

	private:
			T	m_eEnum;
	};


	///////////////////////////////////////////////////////////////////////
	//  Template for portably controlling the serialization of structure
	//	pointer member variables

	#ifdef _WIN32
		#pragma warning(push)
		#pragma warning (disable : 4311)
	#endif

	template <class T>
	class ST_DLL_LINK CPaddedPtr
	{
	public:
						CPaddedPtr(T* pPtr = 0) :
							m_pPtr(pPtr)
						#if !defined(_WIN64) && !defined(__LP64__)
						  , m_uiPad(0)
						#endif
						{
						}
						~CPaddedPtr( ) { }

			T*			operator->(void)				{ return m_pPtr; }
			const T*	operator->(void) const			{ return m_pPtr; }
			T&			operator*(void)					{ return *m_pPtr; }
						operator T*(void)				{ return m_pPtr; }
						operator const T*(void) const	{ return m_pPtr; }
						operator st_sizet(void)			{ return reinterpret_cast<st_sizet>(m_pPtr); }
			T*			operator=(T* pPtr)				{ m_pPtr = pPtr; return m_pPtr; }

	private:
			T*			m_pPtr;
			#if !defined(_WIN64) && !defined(__LP64__)
			st_uint32	m_uiPad;	// align 32-bit pointers with 64-bit pointers (helps with SRT serialization)
			#endif
	};

	#ifdef _WIN32
		#pragma warning(pop)
	#endif


	///////////////////////////////////////////////////////////////////////
	//  SDK-wide constants

	#if defined(_XBOX)
		static const st_char c_chFolderSeparator = '\\';
		static const st_char* c_szFolderSeparator = "\\";
	#else
		// strange way to specify this because GCC issues warnings about these being unused in Core
		static const st_char* c_szFolderSeparator = "/";
		static const st_char c_chFolderSeparator = c_szFolderSeparator[0];
	#endif


	// include inline functions
	#include "Core/Types_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"
