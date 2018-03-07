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

#include "Core/Types.h"
#include <cstring>
#include "Parser.h"
#include "Core/Memory.h"
#include "Utilities/Utility.h"
using namespace SpeedTree;

#ifdef _WIN32
	#pragma warning (disable : 4311)
#endif


///////////////////////////////////////////////////////////////////////  
//  Constants

const st_int32 c_nSrtHeaderLength = 16;
const char* c_pSrtHeader = "SRT 07.0.0";


///////////////////////////////////////////////////////////////////////  
//  StringLookupHasOccurred

inline st_bool StringLookupHasOccurred(CStringPtr& strString)
{
	const st_int32 c_nIndexThreshhold = 1000;

	return (st_int32(strString) > c_nIndexThreshhold);
}


///////////////////////////////////////////////////////////////////////  
//  CParser::CParser

CParser::CParser( ) :
	m_pBuffer(NULL),
	m_uiBufferSize(0),
	m_uiBufferIndex(0),
	m_pTree(NULL),
	m_pGeometry(NULL),
	m_bSwapEndian(false),
	m_bFileIsBigEndian(false),
	m_nNumStringsInTable(0),
	m_pStringTable(NULL),
	m_pStringLengths(NULL)
{
}


///////////////////////////////////////////////////////////////////////  
//  CParser::~CParser

CParser::~CParser( )
{
}


///////////////////////////////////////////////////////////////////////  
//  CParser::Parse

st_bool CParser::Parse(const st_byte* pMemBlock, 
					   size_t asiSubBlockOffsets[2],
					   st_uint32 uiNumBytes, 
					   CCore* pTree, 
					   SGeometry* pGeometry)
{
	st_bool bSuccess = false;

	if (pMemBlock)
	{
		if (uiNumBytes > 0)
		{
			if (pGeometry)
			{
				// copy the parameters into member variables that will be accessed often
				m_pBuffer = pMemBlock;
				m_uiBufferSize = uiNumBytes;
				m_pTree = pTree;
				m_pGeometry = pGeometry;

				// set the index variable used by all parse routines to the beginning of the buffer
				m_uiBufferIndex = 0;

				if (ParseHeader( ) &&
					ParsePlatform( ) &&
					ParseExtents( ) &&
					ParseLOD( ) &&
					ParseWind( ))
				{
					asiSubBlockOffsets[0] = m_uiBufferIndex;

					if (ParseStringTable( ) &&
					    ParseCollisionObjects( ) &&
						ParseBillboards( ) &&
						ParseCustomData( ) &&
						ParseRenderStates( ) &&
						Parse3dGeometry( ))
					{
						asiSubBlockOffsets[1] = m_uiBufferIndex;

						if (ParseVertexAndIndexData( ))
						{
							if (m_bSwapEndian)
							{
								(void) ConvertEndianAllData( );
								CCore::SetError("Performance warning: SRT file was wrong endian format, added brief conversion time");
							}

							bSuccess = LookupStringsByIDs( );
						}
					}
				}
			}
			else
				CCore::SetError("CParser::Parse, pGeometry pointer was NULL");
		}
		else
			CCore::SetError("CParser::Parse, buffer passed in is too short (%d bytes)", uiNumBytes);
	}
	else
		CCore::SetError("CParser::Parse, pMemBlock parameter was NULL");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseHeader

st_bool CParser::ParseHeader(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(c_nSrtHeaderLength))
	{
		CFixedString cHeader;
		if (ParseString(cHeader, c_nSrtHeaderLength) && strcmp(c_pSrtHeader, cHeader.c_str( )) == 0)
		{
			bSuccess = true;
		}
		else
			CCore::SetError("CParser::ParseHeader, expected header [%s] but got [%s]\n", c_pSrtHeader, cHeader.c_str( ));
	}
	else
		CCore::SetError("CParser::ParseHeader, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseCustomData

st_bool CParser::ParseCustomData(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(CCore::USER_STRING_COUNT * c_nSizeOfInt))
	{
		bSuccess = true;

		for (st_int32 nUserString = st_int32(CCore::USER_STRING_0); nUserString < st_int32(CCore::USER_STRING_COUNT); ++nUserString)
			m_pTree->m_pUserStrings[nUserString] = GetStringFromTable(ParseInt( ));
	}
	else
		CCore::SetError("CParser::ParseCustomData, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParsePlatform

st_bool CParser::ParsePlatform(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(2 * c_nSizeOfInt))
	{
		m_bFileIsBigEndian = (ParseByte( ) != 0);

		#ifdef SPEEDTREE_BIG_ENDIAN
			m_bSwapEndian = !m_bFileIsBigEndian;
		#else
			m_bSwapEndian = m_bFileIsBigEndian;
		#endif

		// coordinate system
		st_int32 nCoordSysEnum = st_int32(ParseByte( ));
		CCoordSys::ECoordSysType eFileCoordSysType = CCoordSys::ECoordSysType(nCoordSysEnum);
		if (eFileCoordSysType != CCoordSys::GetCoordSysType( ))
			CCore::SetError("Warning: SRT compiled with [%s] coord system, but SDK is set to use [%s]",
				CCoordSys::CoordSysName(eFileCoordSysType), CCoordSys::CoordSysName(CCoordSys::GetCoordSysType( )));

        // texcoords flipped flag
        m_pTree->m_bTexCoordsFlipped = (ParseByte( ) == 1);

		(void) ParseByte( ); // reserved

		bSuccess = true;
	}
	else
		CCore::SetError("CParser::ParsePlatform, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseExtents

st_bool CParser::ParseExtents(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(6 * c_nSizeOfFloat))
	{
		// min
		m_pTree->m_cExtents[0] = ParseFloat( );
		m_pTree->m_cExtents[1] = ParseFloat( );
		m_pTree->m_cExtents[2] = ParseFloat( );

		// max
		m_pTree->m_cExtents[3] = ParseFloat( );
		m_pTree->m_cExtents[4] = ParseFloat( );
		m_pTree->m_cExtents[5] = ParseFloat( );

		// make sure the mins are <= max values; if not, swap them
		m_pTree->m_cExtents.Order( );

		bSuccess = true;
	}
	else
		CCore::SetError("CParser::ParseExtents, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseLOD

st_bool CParser::ParseLOD(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(1 * c_nSizeOfInt + 4 * c_nSizeOfFloat))
	{
		SLodProfile sLod;
		sLod.m_bLodIsPresent = (ParseInt( ) != 0);
		sLod.m_fHighDetail3dDistance = ParseFloat( );
		sLod.m_fLowDetail3dDistance = ParseFloat( );
		sLod.m_fBillboardStartDistance = ParseFloat( );
		sLod.m_fBillboardFinalDistance = ParseFloat( );
		m_pTree->SetLodProfile(sLod);

		bSuccess = true;
	}
	else
		CCore::SetError("CParser::ParseLOD, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseWind

st_bool CParser::ParseWind(void)
{
	st_bool bSuccess = false;

	CWind::SParams sWindParams;
	if (!EndOfFile(sizeof(sWindParams)))
	{
		CWind& cWind = m_pTree->GetWind( );

		memcpy(&sWindParams, m_pBuffer + m_uiBufferIndex, sizeof(sWindParams));
		m_uiBufferIndex += sizeof(sWindParams);
		cWind.SetParams(sWindParams);

		// parse array of option bools
		if (!EndOfFile(CWind::NUM_WIND_OPTIONS))
		{
			st_bool abOptions[CWind::NUM_WIND_OPTIONS];
			memcpy(abOptions, m_pBuffer + m_uiBufferIndex, CWind::NUM_WIND_OPTIONS);

			for (st_int32 i = 0; i < CWind::NUM_WIND_OPTIONS; ++i)
				cWind.SetOption(CWind::EOptions(i), abOptions[i]);

			m_uiBufferIndex += CWind::NUM_WIND_OPTIONS;
			ParseUntilAligned( );

			// grab tree-specific values
			if (!EndOfFile((3 + 1) * sizeof(st_float32)))
			{
				// branch anchor
				Vec3 vBranchAnchor;
				ParseFloat3(vBranchAnchor);

				// max branch length
				st_float32 fMaxBranchLength = ParseFloat( );

				// set values
				cWind.SetTreeValues(vBranchAnchor, fMaxBranchLength);

				bSuccess = true;
			}
		}
	}
	else
		CCore::SetError("CParser::ParseWind, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseStringTable

st_bool CParser::ParseStringTable(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(c_nSizeOfInt)) // should be enough for at least the # of strings
	{
		m_nNumStringsInTable = ParseInt( );

		if (!EndOfFile(sizeof(CPaddedPtr<st_int32>) * m_nNumStringsInTable)) // should be enough for the string lengths
		{
			// what's written here:
			//
			//	N = # strings (m_nNumStringsInTable above)
			//
			//	for each N
			//		<padded string length>
			//
			//	<padded string length> =
			//		<4-byte pad>
			//		<4-byte length of string>
			//
			//	for each N
			//		<characters of string>
			//		<padded for alignment>

			// get <4-byte length of string> of entry 0
			m_pStringLengths = (st_int32*) (m_pBuffer + m_uiBufferIndex + 4);

			// convert lengths endian if necessary (this is the only endian conversion not
			// done in ConvertEndianAllData(), after the file has been read)
			if (m_bSwapEndian)
			{
				st_int32* pConvert = (st_int32*) m_pStringLengths;
				for (st_int32 i = 0; i < m_nNumStringsInTable; ++i)
				{
					SwapEndian4Bytes<st_int32>(*pConvert);
					pConvert += 2; // to jump over padding, too
				}
			}

			// advance past all N <padded string length>s
			m_uiBufferIndex += sizeof(CPaddedPtr<st_int32>) * m_nNumStringsInTable;

			// point to first string in table
			m_pStringTable = (const char*) (m_pBuffer + m_uiBufferIndex);

			// advance through all strings (including pads)
			for (st_int32 i = 0; i < m_nNumStringsInTable; ++i)
			{
				st_int32 nLength = m_pStringLengths[i * 2]; // * 2 to work through the pads
				m_uiBufferIndex += nLength;
			}

			bSuccess = true;
		}
		else
			CCore::SetError("CParser::ParseStringTable, premature end-of-file\n");
	}
	else
		CCore::SetError("CParser::ParseStringTable, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseCollisionObjects

st_bool CParser::ParseCollisionObjects(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(c_nSizeOfInt))
	{
		m_pTree->m_nNumCollisionObjects = ParseInt( );
		
		if (!EndOfFile(sizeof(SCollisionObject) * m_pTree->m_nNumCollisionObjects))
		{
			// point directly in SRT memory for table
			m_pTree->m_pCollisionObjects = (SCollisionObject*) (m_pBuffer + m_uiBufferIndex);
			m_uiBufferIndex += sizeof(SCollisionObject) * m_pTree->m_nNumCollisionObjects;

			bSuccess = true;
		}
		else
			CCore::SetError("CParser::ParseCollisionObjects, premature end-of-file\n");
	}
	else
		CCore::SetError("CParser::ParseCollisionObjects, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseRenderStateBlock

st_bool CParser::ParseRenderStateBlock(SRenderState*& pDestBlock, st_int32 nNumStates)
{
	st_bool bSuccess = false;

	if (!EndOfFile(sizeof(SRenderState) * nNumStates))
	{
		pDestBlock = (SRenderState*) (m_pBuffer + m_uiBufferIndex);
		m_uiBufferIndex += nNumStates * sizeof(SRenderState);

		bSuccess = true;
	}
	else
		CCore::SetError("CParser::ParseRenderStateBlock, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseAndCopyRenderState

st_bool CParser::ParseAndCopyRenderState(SRenderState* pDest)
{
	st_bool bSuccess = false;

	if (!EndOfFile(sizeof(SRenderState)))
	{
		memcpy(pDest, m_pBuffer + m_uiBufferIndex, sizeof(SRenderState));
		m_uiBufferIndex += sizeof(SRenderState);

		bSuccess = true;
	}
	else
		CCore::SetError("CParser::ParseAndCopyRenderState, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseRenderStates

st_bool CParser::ParseRenderStates(void)
{
	st_bool bSuccess = false;

	if (!EndOfFile(c_nSizeOfInt * 3))
	{
		bSuccess = true;

		m_pGeometry->m_nNum3dRenderStates = ParseInt( );
		m_pGeometry->m_bDepthOnlyIncluded = (ParseInt( ) == 1);
		m_pGeometry->m_bShadowCastIncluded = (ParseInt( ) == 1);

		// parse shader path
		m_pGeometry->m_strShaderPath = GetStringFromTable(ParseInt( ));

		// parse 3d lighting render states
		bSuccess &= ParseRenderStateBlock(m_pGeometry->m_p3dRenderStates[RENDER_PASS_MAIN], m_pGeometry->m_nNum3dRenderStates);

		// parse 3d depth-only render states
		if (m_pGeometry->m_bDepthOnlyIncluded)
			bSuccess &= ParseRenderStateBlock(m_pGeometry->m_p3dRenderStates[RENDER_PASS_DEPTH_PREPASS], m_pGeometry->m_nNum3dRenderStates);

		// parse 3d shadow cast render states
		if (m_pGeometry->m_bShadowCastIncluded)
			bSuccess &= ParseRenderStateBlock(m_pGeometry->m_p3dRenderStates[RENDER_PASS_SHADOW_CAST], m_pGeometry->m_nNum3dRenderStates);

		// billboard lighting render state
		bSuccess &= ParseAndCopyRenderState(m_pGeometry->m_aBillboardRenderStates + RENDER_PASS_MAIN);

		// billboard depth-only render state
		if (m_pGeometry->m_bDepthOnlyIncluded)
			bSuccess &= ParseAndCopyRenderState(m_pGeometry->m_aBillboardRenderStates + RENDER_PASS_DEPTH_PREPASS);

		// billboard shadow cast render state
		if (m_pGeometry->m_bShadowCastIncluded)
			bSuccess &= ParseAndCopyRenderState(m_pGeometry->m_aBillboardRenderStates + RENDER_PASS_SHADOW_CAST);
	}
	else
		CCore::SetError("CParser::ParseRenderStates, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::Parse3dGeometry

st_bool CParser::Parse3dGeometry(void)
{
	st_bool bSuccess = false;

	// start with SLods
	if (!EndOfFile(c_nSizeOfInt))
	{
		m_pGeometry->m_nNumLods = ParseInt( );

		if (!EndOfFile(m_pGeometry->m_nNumLods * sizeof(SLod)))
		{
			// read SLod structs
			m_pGeometry->m_pLods = (SLod*) (m_pBuffer + m_uiBufferIndex);
			m_uiBufferIndex += sizeof(SLod) * m_pGeometry->m_nNumLods;

			// read SDrawCalls
			for (st_int32 nLod = 0; nLod < m_pGeometry->m_nNumLods; ++nLod)
			{
				SLod* pLod = (SLod*) m_pGeometry->m_pLods + nLod;

				// convert endian if necessary
				if (m_bSwapEndian)
				{
					SwapEndian4Bytes<st_int32>(pLod->m_nNumDrawCalls);
					SwapEndian4Bytes<st_int32>(pLod->m_nNumBones);
				}

				if (!EndOfFile(sizeof(SDrawCall) * pLod->m_nNumDrawCalls))
				{
					pLod->m_pDrawCalls = (SDrawCall*) (m_pBuffer + m_uiBufferIndex);
					m_uiBufferIndex += sizeof(SDrawCall) * pLod->m_nNumDrawCalls;

					// assign draw calls' render state pointers
					for (st_int32 nDrawCall = 0; nDrawCall < pLod->m_nNumDrawCalls; ++nDrawCall)
					{
						SDrawCall* pDrawCall = (SDrawCall*) pLod->m_pDrawCalls + nDrawCall;

						// convert endian if necessary
						if (m_bSwapEndian)
						{
							SwapEndian4Bytes<st_int32>(pDrawCall->m_nRenderStateIndex);
							SwapEndian4Bytes<st_int32>(pDrawCall->m_nNumVertices);
							SwapEndian4Bytes<st_int32>(pDrawCall->m_nNumIndices);
						}

						pDrawCall->m_pRenderState = m_pGeometry->m_p3dRenderStates[0] + pDrawCall->m_nRenderStateIndex;
					}

					// read bones
					if (pLod->m_nNumBones > 0)
					{
						pLod->m_pBones = (SBone*) (m_pBuffer + m_uiBufferIndex);
						m_uiBufferIndex += sizeof(SBone) * pLod->m_nNumBones;

						// convert endian if necessary
						if (m_bSwapEndian)
						{
							for (st_int32 nBone = 0; nBone < pLod->m_nNumBones; ++nBone)
							{
								SBone* pBone = (SBone*) pLod->m_pBones + nBone;

								SwapEndian4Bytes<st_int32>(pBone->m_nID);
								SwapEndian4Bytes<st_int32>(pBone->m_nParentID);
								SwapEndianVec3(pBone->m_vStart);
								SwapEndianVec3(pBone->m_vEnd);
								SwapEndian4Bytes<st_float32>(pBone->m_fRadius);
								SwapEndian4Bytes<st_float32>(pBone->m_fMass);
								SwapEndian4Bytes<st_float32>(pBone->m_fMassWithChildren);
							}
						}
					}

					bSuccess = true;
				}
			}
		}
		else
			CCore::SetError("CParser::Parse3dGeometry, premature end-of-file\n");
	}
	else
		CCore::SetError("CParser::Parse3dGeometry, premature end-of-file\n");

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseVertexAndIndexData

st_bool CParser::ParseVertexAndIndexData(void)
{
	st_bool bSuccess = false;

	assert(m_pGeometry);
	for (st_int32 nLod = 0; nLod < m_pGeometry->m_nNumLods; ++nLod)
	{
		assert(static_cast<SLod*>(m_pGeometry->m_pLods));
		SLod* pLod = (SLod*) m_pGeometry->m_pLods + nLod;

		for (st_int32 nDrawCall = 0; nDrawCall < pLod->m_nNumDrawCalls; ++nDrawCall)
		{
			assert(static_cast<SDrawCall*>(pLod->m_pDrawCalls));
			SDrawCall* pDrawCall = (SDrawCall*) pLod->m_pDrawCalls + nDrawCall;

			st_uint32 uiVertexDataSize = pDrawCall->m_nNumVertices * pDrawCall->m_pRenderState->m_sVertexDecl.m_uiVertexSize;
			st_uint32 uiIndexDataSize = pDrawCall->m_nNumIndices * (pDrawCall->m_b32BitIndices ? 4 : 2);
			if (!EndOfFile(uiVertexDataSize + uiIndexDataSize))
			{
				// vertex data
				assert(size_t(m_pBuffer + m_uiBufferIndex) % 4 == 0);
				pDrawCall->m_pVertexData = (st_byte*) (m_pBuffer + m_uiBufferIndex);
				m_uiBufferIndex += uiVertexDataSize;

				// index data
				assert(size_t(m_pBuffer + m_uiBufferIndex) % 4 == 0);
				pDrawCall->m_pIndexData = (st_byte*) (m_pBuffer + m_uiBufferIndex);
				m_uiBufferIndex += uiIndexDataSize;

				ParseUntilAligned( );

				bSuccess = true;
			}
			else
			{
				CCore::SetError("CParser::ParseVertexAndIndexData, premature end-of-file\n");
				bSuccess = false;
			}
		}
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseBillboards

st_bool CParser::ParseBillboards(void)
{
	st_bool bSuccess = false;

	// vertical billboards
	{
		if (!EndOfFile(2 * c_nSizeOfInt + 3 * c_nSizeOfFloat))
		{
			SVerticalBillboards& sBBs = m_pGeometry->m_sVertBBs;

			// parse dimensions
			sBBs.m_fWidth = ParseFloat( );
			sBBs.m_fTopPos = ParseFloat( );
			sBBs.m_fBottomPos = ParseFloat( );
			sBBs.m_nNumBillboards = ParseInt( );

			// texcoord table
			assert(size_t(m_pBuffer + m_uiBufferIndex) % 4 == 0); // check alignment
			sBBs.m_pTexCoords = (const st_float32*) (m_pBuffer + m_uiBufferIndex);
			m_uiBufferIndex += sBBs.m_nNumBillboards * sizeof(st_float32) * 4;

			// rotated flags
			sBBs.m_pRotated = (const st_byte*) (m_pBuffer + m_uiBufferIndex);
			m_uiBufferIndex += sBBs.m_nNumBillboards * sizeof(st_byte);
			ParseUntilAligned( );

			// cutout values
			sBBs.m_nNumCutoutVertices = ParseInt( );
			sBBs.m_nNumCutoutIndices = ParseInt( );
			if (sBBs.m_nNumCutoutVertices > 0 && sBBs.m_nNumCutoutIndices > 0)
			{
				// interp float pairs
				sBBs.m_pCutoutVertices = (const st_float32*) (m_pBuffer + m_uiBufferIndex);
				m_uiBufferIndex += 2 * sizeof(st_float32) * sBBs.m_nNumCutoutVertices;

				// interp indices
				sBBs.m_pCutoutIndices = (const st_uint16*) (m_pBuffer + m_uiBufferIndex);
				m_uiBufferIndex += sizeof(st_uint16) * sBBs.m_nNumCutoutIndices;
				ParseUntilAligned( );
			}

			// convert endian if need be
			if (m_bSwapEndian)
			{
				for (st_int32 i = 0; i < sBBs.m_nNumBillboards * 4; ++i)
					SwapEndian4Bytes<st_float32>((st_float32&) sBBs.m_pTexCoords[i]);

				// cutouts
				for (st_int32 i = 0; i < sBBs.m_nNumCutoutVertices * 2; ++i)
					SwapEndian4Bytes<st_float32>((st_float32&) sBBs.m_pCutoutVertices[i]);

				for (st_int32 i = 0; i < sBBs.m_nNumCutoutIndices; ++i)
					SwapEndian2Bytes<st_uint16>((st_uint16&) sBBs.m_pCutoutIndices[i]);
			}

			bSuccess = true;
		}
		else
			CCore::SetError("CParser::ParseBillboards, premature end-of-file\n");
	}

	// horizontal billboard
	if (bSuccess)
	{
		if (!EndOfFile(c_nSizeOfInt + (8 + 12) * c_nSizeOfFloat))
		{
			SHorizontalBillboard& sBB = m_pGeometry->m_sHorzBB;

			sBB.m_bPresent = (ParseInt( ) != 0);

			// texcoords
			for (st_int32 i = 0; i < 8; ++i)
				sBB.m_afTexCoords[i] = ParseFloat( );

			// positions
			for (st_int32 i = 0; i < 4; ++i)
				ParseFloat3(sBB.m_avPositions[i]);
		}
		else
		{
			CCore::SetError("CParser::ParseBillboards, premature end-of-file\n");
			bSuccess = false;
		}
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  Helper: SwapEndianStringPtr

void SwapEndianStringPtr(CStringPtr& cStringPtr)
{
	st_uint32 uiPtrConvert = 0;
	#if defined(__LP64__) || defined(_LP64) || defined(_WIN64)
		union 
		{
			const char* m_pPtr;
			st_uint32 	m_aData[2];
		} uSplit;
		
		uSplit.m_pPtr = cStringPtr;
		uiPtrConvert = uSplit.m_aData[0];
	#else
		const char* pPtr = cStringPtr;
		uiPtrConvert = st_uint32(pPtr);
	#endif
	
	SwapEndian4Bytes<st_uint32>(uiPtrConvert);
	// @fixme: this looks fishy (converting uint32 to a 64-bit pointer)
	cStringPtr = (const char*) uiPtrConvert;
}


///////////////////////////////////////////////////////////////////////  
//  Helper: SwapEndianRenderState

void SwapEndianRenderState(SRenderState& sRenderState)
{
	// textures
	for (st_int32 i = 0; i < TL_NUM_TEX_LAYERS; ++i)
		SwapEndianStringPtr(sRenderState.m_apTextures[i]);

	// lighting model
	SwapEndian4Bytes<ELightingModel>(sRenderState.m_eLightingModel);

	// ambient
	SwapEndianVec3(sRenderState.m_vAmbientColor);
	SwapEndian4Bytes<ELightingEffect>(sRenderState.m_eAmbientContrast);
	SwapEndian4Bytes<st_float32>(sRenderState.m_fAmbientContrastFactor);

	// diffuse
	SwapEndianVec3(sRenderState.m_vDiffuseColor);
	SwapEndian4Bytes<st_float32>(sRenderState.m_fDiffuseScalar);

	// detail
	SwapEndian4Bytes<ELightingEffect>(sRenderState.m_eDetailLayer);

	// specular
	SwapEndian4Bytes<ELightingEffect>(sRenderState.m_eSpecular);
	SwapEndian4Bytes<st_float32>(sRenderState.m_fShininess);
	SwapEndianVec3(sRenderState.m_vSpecularColor);

	// transmission
	SwapEndian4Bytes<ELightingEffect>(sRenderState.m_eTransmission);
	SwapEndianVec3(sRenderState.m_vTransmissionColor);
	SwapEndian4Bytes<st_float32>(sRenderState.m_fTransmissionShadowBrightness);
	SwapEndian4Bytes<st_float32>(sRenderState.m_fTransmissionViewDependency);

	// branch seam smoothing
	SwapEndian4Bytes<ELightingEffect>(sRenderState.m_eBranchSeamSmoothing);
	SwapEndian4Bytes<st_float32>(sRenderState.m_fBranchSeamWeight);

	// LOD
	SwapEndian4Bytes<ELodMethod>(sRenderState.m_eLodMethod);

	// render states
	SwapEndian4Bytes<EShaderGenerationMode>(sRenderState.m_eShaderGenerationMode);
	SwapEndian4Bytes<ECullType>(sRenderState.m_eFaceCulling);
	
	// fog
	SwapEndian4Bytes<EFogCurve>(sRenderState.m_eFogCurve);
	SwapEndian4Bytes<EFogColorType>(sRenderState.m_eFogColorStyle);

	// misc
	SwapEndian4Bytes<st_float32>(sRenderState.m_fAlphaScalar);
	SwapEndian4Bytes<EWindLod>(sRenderState.m_eWindLod);
	SwapEndian4Bytes<ERenderPass>(sRenderState.m_eRenderPass);
	SwapEndianStringPtr(sRenderState.m_pDescription);
	SwapEndianStringPtr(sRenderState.m_pUserData);
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ConvertEndianAllData

st_bool CParser::ConvertEndianAllData(void)
{
	assert(m_bSwapEndian);

	st_bool bSuccess = true;

	// wind parameters
	{
		// query incorrectly formatted wind parameters
		CWind::SParams sWindParams = m_pTree->GetWind( ).GetParams( );

		// endian swap
		st_float32* pDst = (st_float32*) &sWindParams;
		st_int32 c_nNumFloats = sizeof(sWindParams) / sizeof(st_float32);
		for (st_int32 i = 0; i < c_nNumFloats; ++i)
			SwapEndian4Bytes<st_float32>(*pDst++);

		// set corrected version back
		m_pTree->GetWind( ).SetParams(sWindParams);
	}

	// collision objects
	for (st_int32 i = 0; i < m_pTree->m_nNumCollisionObjects; ++i)
	{
		SCollisionObject* pObject = m_pTree->m_pCollisionObjects + i;

		SwapEndianVec3(pObject->m_vCenter1);
		SwapEndianVec3(pObject->m_vCenter2);
		SwapEndian4Bytes<st_float32>(pObject->m_fRadius);
		SwapEndianStringPtr(pObject->m_pUserString);
	}

	// render states
	{
		// 3d lighting
		for (st_int32 i = 0; i < m_pGeometry->m_nNum3dRenderStates; ++i)
			SwapEndianRenderState(m_pGeometry->m_p3dRenderStates[RENDER_PASS_MAIN][i]);

		// 3d depth-only
		if (m_pGeometry->m_bDepthOnlyIncluded)
			for (st_int32 i = 0; i < m_pGeometry->m_nNum3dRenderStates; ++i)
				SwapEndianRenderState(m_pGeometry->m_p3dRenderStates[RENDER_PASS_DEPTH_PREPASS][i]);

		// 3d shadow-cast
		if (m_pGeometry->m_bShadowCastIncluded)
			for (st_int32 i = 0; i < m_pGeometry->m_nNum3dRenderStates; ++i)
				SwapEndianRenderState(m_pGeometry->m_p3dRenderStates[RENDER_PASS_SHADOW_CAST][i]);

		// billboard lighting
		SwapEndianRenderState(m_pGeometry->m_aBillboardRenderStates[RENDER_PASS_MAIN]);

		// billboard depth-only
		if (m_pGeometry->m_bDepthOnlyIncluded)
			SwapEndianRenderState(m_pGeometry->m_aBillboardRenderStates[RENDER_PASS_DEPTH_PREPASS]);

		// billboard shadow cast
		if (m_pGeometry->m_bShadowCastIncluded)
			SwapEndianRenderState(m_pGeometry->m_aBillboardRenderStates[RENDER_PASS_SHADOW_CAST]);
	}

	// vertex data
	assert(m_pGeometry);
	for (st_int32 nLod = 0; nLod < m_pGeometry->m_nNumLods; ++nLod)
	{
		assert(static_cast<SLod*>(m_pGeometry->m_pLods));
		SLod& sLod = m_pGeometry->m_pLods[nLod];

		for (st_int32 nDrawCall = 0; nDrawCall < sLod.m_nNumDrawCalls; ++nDrawCall)
		{
			assert(static_cast<SDrawCall*>(sLod.m_pDrawCalls));
			SDrawCall& sDrawCall = sLod.m_pDrawCalls[nDrawCall];

			st_byte* pVertexData = static_cast<st_byte*>(sDrawCall.m_pVertexData);
			for (st_int32 nVertex = 0; nVertex < sDrawCall.m_nNumVertices; ++nVertex)
			{
				assert(static_cast<SRenderState*>(sDrawCall.m_pRenderState));
				const SVertexDecl& sVertexDecl = sDrawCall.m_pRenderState->m_sVertexDecl;
				for (st_int32 nAttrib = 0; nAttrib < VERTEX_ATTRIB_COUNT; ++nAttrib)
				{
					const SVertexDecl::SAttribute& sAttrib = sVertexDecl.m_asAttributes[nAttrib];
					if (sAttrib.IsUsed( ))
					{
						if (sAttrib.m_eFormat == VERTEX_FORMAT_FULL_FLOAT)
						{
							for (st_int32 i = 0; i < sAttrib.NumUsedComponents( ); ++i)
							{
								st_float32* pFloat32 = reinterpret_cast<st_float32*>(pVertexData);
								SwapEndian4Bytes<st_float32>(*pFloat32);
								pVertexData += SVertexDecl::FormatSize(sAttrib.m_eFormat);
							}
						}
						else if (sAttrib.m_eFormat == VERTEX_FORMAT_HALF_FLOAT)
						{
							for (st_int32 i = 0; i < sAttrib.NumUsedComponents( ); ++i)
							{
								// need to swap these two bytes, but using st_float16 results in
								// some compiler warnings on several platforms; using st_uint16 to
								// avoid warnings, but net effect is the same
								st_uint16* pFloat16 = reinterpret_cast<st_uint16*>(pVertexData);
								SwapEndian2Bytes<st_uint16>(*pFloat16);
								pVertexData += SVertexDecl::FormatSize(sAttrib.m_eFormat);
							}
						}
						else
							pVertexData += sAttrib.Size( );
					}
				}
			}
		}
	}

	// index data
	assert(m_pGeometry);
	for (st_int32 nLod = 0; nLod < m_pGeometry->m_nNumLods; ++nLod)
	{
		assert(static_cast<SLod*>(m_pGeometry->m_pLods));
		SLod& sLod = m_pGeometry->m_pLods[nLod];

		for (st_int32 nDrawCall = 0; nDrawCall < sLod.m_nNumDrawCalls; ++nDrawCall)
		{
			assert(static_cast<SDrawCall*>(sLod.m_pDrawCalls));
			SDrawCall& sDrawCall = sLod.m_pDrawCalls[nDrawCall];

			if (sDrawCall.m_b32BitIndices)
			{
				st_uint32* pIndex32 = reinterpret_cast<st_uint32*>(static_cast<st_byte*>(sDrawCall.m_pIndexData));
				for (st_int32 i = 0; i < sDrawCall.m_nNumIndices; ++i)
					SwapEndian4Bytes<st_uint32>(*pIndex32++);
			}
			else
			{
				st_uint16* pIndex16 = reinterpret_cast<st_uint16*>(static_cast<st_byte*>(sDrawCall.m_pIndexData));
				for (st_int32 i = 0; i < sDrawCall.m_nNumIndices; ++i)
					SwapEndian2Bytes<st_uint16>(*pIndex16++);
			}
		}
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::LookupStringsByIDs

st_bool CParser::LookupStringsByIDs(void)
{
	st_bool bSuccess = true;

	// collision objects
	for (st_int32 i = 0; i < m_pTree->m_nNumCollisionObjects; ++i)
	{
		SCollisionObject* pObject = m_pTree->m_pCollisionObjects + i;
		if (!StringLookupHasOccurred(pObject->m_pUserString))
			pObject->m_pUserString = GetStringFromTable(st_int32(pObject->m_pUserString));
	}

	// render states
	for (st_int32 i = 0; i < m_pGeometry->m_nNum3dRenderStates; ++i)
		LookupRenderStateStrings(m_pGeometry->m_p3dRenderStates[RENDER_PASS_MAIN][i]);
	LookupRenderStateStrings(m_pGeometry->m_aBillboardRenderStates[RENDER_PASS_MAIN]);

	if (m_pGeometry->m_bDepthOnlyIncluded)
	{
		for (st_int32 i = 0; i < m_pGeometry->m_nNum3dRenderStates; ++i)
			LookupRenderStateStrings(m_pGeometry->m_p3dRenderStates[RENDER_PASS_DEPTH_PREPASS][i]);
		LookupRenderStateStrings(m_pGeometry->m_aBillboardRenderStates[RENDER_PASS_DEPTH_PREPASS]);
	}

	if (m_pGeometry->m_bShadowCastIncluded)
	{
		for (st_int32 i = 0; i < m_pGeometry->m_nNum3dRenderStates; ++i)
			LookupRenderStateStrings(m_pGeometry->m_p3dRenderStates[RENDER_PASS_SHADOW_CAST][i]);
		LookupRenderStateStrings(m_pGeometry->m_aBillboardRenderStates[RENDER_PASS_SHADOW_CAST]);
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseByte

inline st_byte CParser::ParseByte(void)
{
	return m_pBuffer[m_uiBufferIndex++];
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseInt

inline st_int32 CParser::ParseInt(void)
{
	if (m_bSwapEndian)
	{
		union
		{
			st_byte		m_acBytes[4];
			st_int32	m_nInt32;
		} uUnion;

		uUnion.m_acBytes[3] = m_pBuffer[m_uiBufferIndex++];
		uUnion.m_acBytes[2] = m_pBuffer[m_uiBufferIndex++];
		uUnion.m_acBytes[1] = m_pBuffer[m_uiBufferIndex++];
		uUnion.m_acBytes[0] = m_pBuffer[m_uiBufferIndex++];

		return uUnion.m_nInt32;
	}
	else
	{
		st_int32* pReturn = (st_int32*) (m_pBuffer + m_uiBufferIndex);
		m_uiBufferIndex += c_nSizeOfInt;

		return *pReturn;
	}
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseFloat

inline st_float32 CParser::ParseFloat(void)
{
	if (m_bSwapEndian)
	{
		union
		{
			st_byte		m_acBytes[4];
			st_float32	m_fFloat32;
		} uUnion;

		uUnion.m_acBytes[3] = m_pBuffer[m_uiBufferIndex++];
		uUnion.m_acBytes[2] = m_pBuffer[m_uiBufferIndex++];
		uUnion.m_acBytes[1] = m_pBuffer[m_uiBufferIndex++];
		uUnion.m_acBytes[0] = m_pBuffer[m_uiBufferIndex++];

		return uUnion.m_fFloat32;
	}
	else
	{
		st_float32* pReturn = (st_float32*) (m_pBuffer + m_uiBufferIndex);
		m_uiBufferIndex += c_nSizeOfFloat;

		return *pReturn;
	}
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseFloat3

inline void CParser::ParseFloat3(st_float32 afValues[3])
{
	afValues[0] = ParseFloat( );
	afValues[1] = ParseFloat( );
	afValues[2] = ParseFloat( );
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseInt

inline st_bool CParser::ParseString(CFixedString& cValue, st_int32 nMaxLength)
{
	st_bool bSuccess = false;

	if (!EndOfFile(nMaxLength))
	{
		cValue = CFixedString((const char*) m_pBuffer + m_uiBufferIndex);
		m_uiBufferIndex += nMaxLength;

		bSuccess = true;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::ParseUntilAligned

inline void CParser::ParseUntilAligned(void)
{
	st_uint32 uiPadSize = 4 - m_uiBufferIndex % 4;

	if (uiPadSize < 4)
		m_uiBufferIndex += uiPadSize;
}


///////////////////////////////////////////////////////////////////////  
//  CParser::LookupRenderStateStrings

void CParser::LookupRenderStateStrings(SRenderState& sRenderState) const
{
	for (st_int32 i = 0; i < TL_NUM_TEX_LAYERS; ++i)
		if (!StringLookupHasOccurred(sRenderState.m_apTextures[i]))
			sRenderState.m_apTextures[i] = GetStringFromTable(st_int32(sRenderState.m_apTextures[i]));

	if (!StringLookupHasOccurred(sRenderState.m_pDescription))
		sRenderState.m_pDescription = GetStringFromTable(st_int32(sRenderState.m_pDescription));
	if (!StringLookupHasOccurred(sRenderState.m_pUserData))
		sRenderState.m_pUserData = GetStringFromTable(st_int32(sRenderState.m_pUserData));
}


///////////////////////////////////////////////////////////////////////  
//  CParser::GetStringFromTable

inline const st_char* CParser::GetStringFromTable(st_int32 nStringIndex) const
{
	assert(nStringIndex < m_nNumStringsInTable);
	assert(m_pStringTable);
	assert(m_pStringLengths);

	st_int32 nTableIndex = 0;
	for (st_int32 i = 0; i < nStringIndex; ++i)
	{
		nTableIndex += m_pStringLengths[i * 2]; // * 2 for padding
	}

	return m_pStringTable + nTableIndex;
}
