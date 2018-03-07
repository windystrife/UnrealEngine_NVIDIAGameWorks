///////////////////////////////////////////////////////////////////////  
//	Comparators.h
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


///////////////////////////////////////////////////////////////////////
// Preprocessor

#pragma once
#include "ExportBegin.h"


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
	// class CLess

	template <class T>
	class CLess
	{
	public:
		bool operator( )(const T& cL, const T& cR)
		{
			return (cL < cR);
		}
	};


	///////////////////////////////////////////////////////////////////////
	// class CGreater

	template <class T>
	class CGreater
	{
	public:
		bool operator( )(const T& cL, const T& cR)
		{
			return (cL > cR);
		}
	};

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif

#include "ExportEnd.h"


