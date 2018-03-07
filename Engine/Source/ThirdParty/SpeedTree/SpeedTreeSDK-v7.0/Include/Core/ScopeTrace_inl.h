///////////////////////////////////////////////////////////////////////  
//  ScopeTrace.inl
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


#ifdef SPEEDTREE_INTEL_GPA

	///////////////////////////////////////////////////////////////////////
	//  CScopeTrace::CScopeTrace

	ST_INLINE CScopeTrace::CScopeTrace(const char* pName)
	{
		assert(pName);

		__itt_string_handle* pTaskHandle = __itt_string_handle_createA(pName);
		assert(pTaskHandle);

		assert(m_pDomain);
		__itt_task_begin(m_pDomain, __itt_null, __itt_null, pTaskHandle);
	}


	///////////////////////////////////////////////////////////////////////
	//  CScopeTrace::~CScopeTrace

	ST_INLINE CScopeTrace::~CScopeTrace( )
	{
		__itt_task_end(m_pDomain);
	}


	///////////////////////////////////////////////////////////////////////
	//  CScopeTrace::Init

	ST_INLINE void CScopeTrace::Init(void)
	{
		// create domain
		m_pDomain = __itt_domain_createA("IDV.SpeedTree");
		assert(m_pDomain);
	}

#else // SPEEDTREE_INTEL_GPA

	///////////////////////////////////////////////////////////////////////
	//  CScopeTrace::CScopeTrace

	#ifdef SPEEDTREE_FALLBACK_TIMING
		ST_INLINE CScopeTrace::CScopeTrace(const char* pName) :
            m_pParent(m_pActiveNode)
		{
            if (!IsActive( ))
                return;

            assert(pName);

            // assign parent node to currently active (current will change shortly)
            m_pParent = m_pActiveNode;

            // find this node in the active node's children, or add it
            SNode* pThisNode = NULL;
            for (size_t i = 0; i < m_pActiveNode->m_aChildren.size( ); ++i)
            {
                assert(m_pActiveNode->m_aChildren[i]->m_pName);
                if (strcmp(pName, m_pActiveNode->m_aChildren[i]->m_pName) == 0)
                {
                    pThisNode = m_pActiveNode->m_aChildren[i];
                    break;
                }
            }

            // if not found, allocate a new one
            if (!pThisNode)
            {
                pThisNode = st_new(SNode, "CScopeTrace::SNode");
                pThisNode->m_pName = pName;
                m_pActiveNode->m_aChildren.push_back(pThisNode);
            }
            assert(pThisNode);

            // this node will now act as parent node for all scope traces that
            // occur before this scope ends
            m_pActiveNode = pThisNode;

			m_cTimer.Start( );
		}
	#else
		ST_INLINE CScopeTrace::CScopeTrace(const char*) { }
	#endif


	///////////////////////////////////////////////////////////////////////
	//  CScopeTrace::~CScopeTrace

	ST_INLINE CScopeTrace::~CScopeTrace( )
	{
		#ifdef SPEEDTREE_FALLBACK_TIMING
            if (!IsActive( ))
                return;

			m_cTimer.Stop( );

            // record data
            m_pActiveNode->m_fTime += m_cTimer.GetMilliSec( );

            // return control back to parent
            m_pActiveNode = m_pParent;
		#endif
	}


	///////////////////////////////////////////////////////////////////////
	//  CScopeTrace::Init

	ST_INLINE void CScopeTrace::Init(void)
	{
	}


    ///////////////////////////////////////////////////////////////////////
    //  CScopeTrace::Start

    ST_INLINE void CScopeTrace::Start(void)
    {
        #ifdef SPEEDTREE_FALLBACK_TIMING
            for (size_t i = 0; i < m_sRootNode.m_aChildren.size( ); ++i)
                st_delete<SNode>(m_sRootNode.m_aChildren[i]);
            m_sRootNode.m_aChildren.clear( );
            m_pActiveNode = &m_sRootNode;
            
            m_bActive = true;
        #endif
    }


    ///////////////////////////////////////////////////////////////////////
    //  CScopeTrace::IsActive

    ST_INLINE st_bool CScopeTrace::IsActive(void)
    {
        #ifdef SPEEDTREE_FALLBACK_TIMING
            return m_bActive;
        #else
            return false;
        #endif
    }


    ///////////////////////////////////////////////////////////////////////
    //  CScopeTrace::Stop

    ST_INLINE void CScopeTrace::Stop(void)
    {
        #ifdef SPEEDTREE_FALLBACK_TIMING
            m_bActive = false;
        #endif
    }


    #ifdef SPEEDTREE_FALLBACK_TIMING
        ///////////////////////////////////////////////////////////////////////
        //  Function: SNode* comparison functor

        class SNodeCellSorter
        {
        public:
            bool operator( )(const CScopeTrace::SNode* pLeft, const CScopeTrace::SNode* pRight) const
            {
                // descending order
                return (pLeft->m_fTime > pRight->m_fTime);
            }
        };
    #endif


    ///////////////////////////////////////////////////////////////////////
    //  CScopeTrace::Report

    inline void CScopeTrace::Report(EReportFormat eFormat, CString& strReport, SNode* pStart)
    {
        #ifdef SPEEDTREE_FALLBACK_TIMING
            static st_int32 nIndent = 0;

            st_float32 fScopeSum = 0.0f;
            if (pStart)
                fScopeSum = pStart->m_fTime;
            else
            {
                for (size_t i = 0; i < m_sRootNode.m_aChildren.size( ); ++i)
                    fScopeSum += m_sRootNode.m_aChildren[i]->m_fTime;

                if (eFormat == FORMAT_PRINT)
                    strReport += CFixedString::Format("TOTAL: %g ms\n", fScopeSum).c_str( );
                else
                    strReport += CFixedString::Format("Task,Time (ms),%% of Parent Task\n").c_str( );
            }

            if (!pStart)
                pStart = &m_sRootNode;
            assert(pStart);

            // sort the children by descending times
            pStart->m_aChildren.sort(SNodeCellSorter( ));

            // sum the children's times
            for (size_t i = 0; i < pStart->m_aChildren.size( ); ++i)
            {
                SNode* pChild = pStart->m_aChildren[i];
                assert(pChild);

                const st_int32 c_nIndentSize = 2;
                const st_int32 c_nLabelWidth = 40;
                for (st_int32 j = 0; j < nIndent * c_nIndentSize; ++j)
                    strReport += " ";

                st_char szFormat[256];
                if (eFormat == FORMAT_PRINT)
                    sprintf(szFormat, "%%-%ds  %%6.4f ms  %%5.1f%%%%\n", c_nLabelWidth);
                else
                    sprintf(szFormat, "%%-%ds,%%6.4f,%%5.1f%%%%\n", c_nLabelWidth);
                strReport += CFixedString::Format(szFormat, pChild->m_pName, pChild->m_fTime, 100.0f * pChild->m_fTime / fScopeSum).c_str( );

                ++nIndent;
                Report(eFormat, strReport, pChild);
                --nIndent;
            }
        #else
            (void) (eFormat);
            (void) (strReport);
            (void) (pStart);
        #endif
    }


    ///////////////////////////////////////////////////////////////////////
    //  CScopeTrace::SNode:SNode

    const st_float32 c_fBadTiming = -1.0f;

    ST_INLINE CScopeTrace::SNode::SNode( ) :
        m_pName("Root"),
        m_fTime(0.0)
    {
    }


    ///////////////////////////////////////////////////////////////////////
    //  CScopeTrace::SNode:~SNode

    ST_INLINE CScopeTrace::SNode::~SNode( )
    {
        for (size_t i = 0; i < m_aChildren.size( ); ++i)
            st_delete<SNode>(m_aChildren[i]);
    }

#endif // SPEEDTREE_INTEL_GPA

