///////////////////////////////////////////////////////////////////////  
//  FileSystem.h
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
#include <stdlib.h>
#include "Core/ExportBegin.h"
#include "Core/Types.h"
#include "Core/FixedString.h"


///////////////////////////////////////////////////////////////////////  
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////  
//  All SpeedTree SDK classes and variables are under the namespace "SpeedTree"

namespace SpeedTree
{

	///////////////////////////////////////////////////////////////////////  
	//  class CFileSystem

	class ST_DLL_LINK CFileSystem
	{
	public:
			enum ETermHint
			{
				SHORT_TERM,
				LONG_TERM
			};

			enum ETimeCompare
			{
				EQUAL,
				FIRST_OLDER,
				SECOND_OLDER,
				FILE_ERROR
			};

	virtual					~CFileSystem( ) { }

	virtual	st_bool			FileExists(const st_char* pFilename);
	virtual size_t			FileSize(const st_char* pFilename);	// returns size in bytes
	virtual st_byte*		LoadFile(const st_char* pFilename, ETermHint eTermHint = SHORT_TERM);
	virtual	void			Release(st_byte* pBuffer);

    static	CFixedString	CleanPlatformFilename(const st_char* pFilename);
    static	CFixedString	CleanPlatformFilename(const CFixedString& strFilename) { return CleanPlatformFilename(strFilename.c_str( )); }

	virtual ETimeCompare	CompareFileTimes(const st_char* pFilenameOne, const st_char* pFilenameTwo);
	};


	#ifdef SPEEDTREE_FILESYSTEM_DECLARED
		ST_DLL_LINK SpeedTree::CFileSystem g_cFileSystem;
		ST_DLL_LINK SpeedTree::CFileSystem *g_pFileSystem = &g_cFileSystem;
	#endif // SPEEDTREE_FILESYSTEM_DECLARED

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "Core/ExportEnd.h"
