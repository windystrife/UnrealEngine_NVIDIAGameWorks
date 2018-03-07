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
//  SRT 7.0.0 File Format (binary)
//
//	This file format was designed with several goals in mind:
//
//	  1. To be read as quickly as possible. While not compressed, it is
//		 set up for direct serialization so that once loaded, the parser
//		 would simply assign numerous data structure pointers into the
//		 read block.
//
//	  2. Minimal heap allocations will be needed, which is particularly
//		 important for consoles. An entire set of SRT files may reside
//	     in a single pre-allocated block and be processed through the
//		 serialization procedures such that no heap allocations are made.
//		 Typically, however, on PCs the procedure is to read the SRT file
//		 from the disk one at a time, requiring a heap allocation per file.
//
//	Due to serialization, endian-ness matters. The parser will automatically 
//  convert a file of opposite endian on load and will caution that a time 
//	penalty was incurred, as well as how long the conversion took.
//
//	6.0 SRT files use the header string "SRT 06.0.0"; SDK versions 6.0, 6.1,
//	and 6.2 all read the same file format.


///////////////////////////////////////////////////////////////////////  
//  Preprocessor

#pragma once
#include "Core/Core.h"
#include "Core/Array.h"
#include "Core/String.h"


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
	//  Forward references

	class CGeometry;


	///////////////////////////////////////////////////////////////////////  
	//  Class CParser

	class CParser
	{
	public:
										CParser( );
										~CParser( );

			st_bool						Parse(const st_byte* pMemBlock,
											  size_t asiSubBlockOffsets[2],
											  st_uint32 uiNumBytes, 
											  CCore* pTree, 
											  SGeometry* pGeometry);

	private:
			st_bool						ParseHeader(void);
			st_bool						ParseCustomData(void);
			st_bool						ParsePlatform(void);
			st_bool						ParseExtents(void);
			st_bool						ParseLOD(void);
			st_bool						ParseWind(void);
			st_bool						ParseStringTable(void);
			st_bool						ParseCollisionObjects(void);
			st_bool						ParseRenderStates(void);
			st_bool						Parse3dGeometry(void);
			st_bool						ParseVertexAndIndexData(void);
			st_bool						ParseBillboards(void);

			// endian conversion utilities
			st_bool						ConvertEndianAllData(void);
			st_bool						LookupStringsByIDs(void);

			// utility functions
			st_byte						ParseByte(void);
			st_int32					ParseInt(void);
			st_float32					ParseFloat(void);
			void						ParseFloat3(st_float32 afValues[3]);
			st_bool						ParseString(CFixedString& szValue, st_int32 nMaxLength = c_nFixedStringLength);
			void						ParseUntilAligned(void);
			void						LookupRenderStateStrings(SRenderState& sRenderState) const;
			st_bool						ParseRenderStateBlock(SRenderState*& pDestBlock, st_int32 nNumStates);
			st_bool						ParseAndCopyRenderState(SRenderState* pDest);
			const st_char*				GetStringFromTable(st_int32 nStringIndex) const;
			st_bool						EndOfFile(st_uint32 uiBytes = 0) const { return m_uiBufferIndex + uiBytes > m_uiBufferSize; }

			// entire binary file is read into memory 
			const st_byte*				m_pBuffer;
			st_uint32					m_uiBufferSize;
			st_uint32					m_uiBufferIndex;

			// SRT file values are stored here
			CCore*						m_pTree;
			SGeometry*					m_pGeometry;

			// SRT file format parameters
			st_bool						m_bSwapEndian;
			st_bool						m_bFileIsBigEndian;

			// string table
			st_int32					m_nNumStringsInTable;
			const char*					m_pStringTable;
			const st_int32*				m_pStringLengths;
	};

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif
