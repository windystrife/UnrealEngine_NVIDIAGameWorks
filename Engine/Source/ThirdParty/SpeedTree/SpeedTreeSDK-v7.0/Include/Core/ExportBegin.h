///////////////////////////////////////////////////////////////////////  
//  ExportBegin.h
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

// storage-class specification
#ifndef ST_DLL_LINK
	#if (defined(_WIN32) || defined(_XBOX)) && defined(_USRDLL)
		#define ST_DLL_LINK __declspec(dllexport)
		#define ST_DLL_LINK_STATIC_MEMVAR __declspec(dllexport)
	#elif defined(SPEEDTREE_USE_SDK_AS_DLLS)
		#define ST_DLL_LINK
		#define ST_DLL_LINK_STATIC_MEMVAR __declspec(dllimport)
	#else
		#define ST_DLL_LINK
		#define ST_DLL_LINK_STATIC_MEMVAR
	#endif
#endif

// specify calling convention
#ifndef ST_CALL_CONV
	#if (defined(_WIN32) || defined(_XBOX)) && !defined(ST_CALL_CONV)
		#define ST_CALL_CONV   __cdecl
	#else
		#define ST_CALL_CONV
	#endif
#endif


///////////////////////////////////////////////////////////////////////  
//  Namespace management
//
//	We moved away from including the SDK version number in the namespace; it's
//	now simply "SpeedTree". The macros below should help with client code that
//	continue to use a namespace with a version number.

#define SpeedTree60 SpeedTree
#define SpeedTree61 SpeedTree
#define SpeedTree62 SpeedTree


// packing - certain SpeedTree data structures require particular packing, so we set it
// explicitly. Comment this line out to override, but be sure to set packing to 4 before
// including Core.h or other key header files.
#define ST_SETS_PACKING_INTERNALLY



