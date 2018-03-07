///////////////////////////////////////////////////////////////////////  
//	FixedString.h
//
//	This file is part of IDV's lightweight STL-like container classes.
//	Like STL, these containers are templated and portable, but are thin
//	enough not to have some of the portability issues of STL nor will
//	it conflict with an application's similar libraries.  These containers
//	also contain some mechanisms for mitigating the inordinate number of
//	of heap allocations associated with the standard STL.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		http://www.idvinc.com


/////////////////////////////////////////////////////////////////////
// Preprocessor

#pragma once
#include "Core/ExportBegin.h"
#include "Core/Types.h"
#include "PortableCrt.h"
#include <cstdio>
#include <cassert>
#if defined(__GNUC__) || defined(__psp2__)
	#include <string.h>
#else
	#include <cstring>
#endif


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	//////////////////////////////////////////////////////////////////////
	// class CBasicString

	template <size_t uiDataSize = 256>
	class ST_DLL_LINK CBasicFixedString
	{
	public:
											CBasicFixedString(void);
											CBasicFixedString(const CBasicFixedString& cCopy);
											CBasicFixedString(const char* pchData);
	virtual									~CBasicFixedString(void);

			// typical functions			
			void 							clear(void);
			bool							empty(void) const;
			bool							resize(size_t uiSize);
			const char*						c_str(void) const;
			size_t							size(void) const;
			size_t							length(void) const;
			CBasicFixedString				substr(size_t uiStart, size_t uiCount = npos);
			size_t							find(char chFind, size_t uiStart = 0) const;
			void							erase_all(const char chErase);

			// operator overloads
			void							operator += (const char* pchData);
			CBasicFixedString				operator + (const char* pchData) const;
			void							operator += (const CBasicFixedString& strRight);
			CBasicFixedString				operator + (const CBasicFixedString& strRight) const;
			void							operator += (const char& chRight);
			CBasicFixedString				operator + (const char& chRight) const;
			CBasicFixedString&				operator = (const CBasicFixedString& strRight);
			CBasicFixedString&				operator = (const char* pchData);
			bool							operator == (const CBasicFixedString& strRight) const;
			bool							operator != (const CBasicFixedString& strRight) const;
			bool							operator < (const CBasicFixedString& strRight) const;
			bool							operator > (const CBasicFixedString& strRight) const;
			char&							operator [] (size_t uiIndex);	
			const char&						operator [] (size_t uiIndex) const;

			// formatting
	static	CBasicFixedString  ST_CALL_CONV	Format(const char* pchFormat, ...);
			CBasicFixedString				Extension(char chExtensionChar = '.') const;
			CBasicFixedString				NoExtension(char chExtensionChar = '.') const;
			CBasicFixedString				Path(CBasicFixedString strDelimiters = "/\\") const;
			CBasicFixedString				NoPath(CBasicFixedString strDelimiters = "/\\") const;
			CBasicFixedString				MakePlatformCompliantPath(void) const;

			enum { npos = st_uint32(-1) };

	protected:
			size_t							m_uiSize;
			char							m_aData[uiDataSize];
	};

	const size_t c_siFixedStringDefaultLength = 256;
	typedef CBasicFixedString<c_siFixedStringDefaultLength> CFixedString;

	// include inline functions
	#include "Core/FixedString_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"

