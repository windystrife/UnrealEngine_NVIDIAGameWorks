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

#if defined(NDEBUG) && defined(_WIN32)
	// uncomment to activate tags compatible with Intel's Graphics
	// Performance Analyzer
	//#define SPEEDTREE_INTEL_GPA
#endif

#ifndef SPEEDTREE_INTEL_GPA
	// uncomment if not using GPA but want some rough idea of timing 
	// in critical sections (CScopeTrace::~CScopeTrace is incomplete)
	//#define SPEEDTREE_FALLBACK_TIMING
#endif

#ifdef SPEEDTREE_INTEL_GPA
	#include <ittnotify.h>
	#include "Core/Types.h"
#else
    #include "Core/Timer.h"
    #include "Core/Array.h"
    #include "Core/String.h"
#endif
#include <cassert>


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
	//  ScopeTrace macro

	#define ScopeTrace(FunctionName) CScopeTrace __task__(FunctionName)


	///////////////////////////////////////////////////////////////////////  
	//  Class CScopeTrace

	class ST_DLL_LINK CScopeTrace
	{
	public:
		struct SNode
        {
                                    SNode( );
                                    ~SNode( );

            const char*             m_pName;
            st_float32              m_fTime;
            CArray<SNode*>          m_aChildren;
        };

								    CScopeTrace(const char* pName);
								    ~CScopeTrace( );

	static	void       ST_CALL_CONV Init(void);
    static  void       ST_CALL_CONV Start(void);
    static  st_bool    ST_CALL_CONV IsActive(void);
    static  void       ST_CALL_CONV Stop(void);

    enum EReportFormat
    {
        FORMAT_PRINT,
        FORMAT_CSV_FILE
    };
    static  void       ST_CALL_CONV Report(EReportFormat eFormat, CString& strReport, SNode* pStart = NULL);

	private:
	#ifdef SPEEDTREE_INTEL_GPA
		static	__itt_domain*	    m_pDomain;
    #elif defined(SPEEDTREE_FALLBACK_TIMING)
        CTimer                      m_cTimer;

        SNode*                      m_pParent;

        static SNode                m_sRootNode;
        static SNode*               m_pActiveNode;
        static st_bool              m_bActive;
	#endif
	};

	// include inline functions
	#include "ScopeTrace_inl.h"

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif
