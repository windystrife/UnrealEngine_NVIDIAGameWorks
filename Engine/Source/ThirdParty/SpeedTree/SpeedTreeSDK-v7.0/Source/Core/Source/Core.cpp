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

#define SPEEDTREE_FILESYSTEM_DECLARED
#include "Core/Types.h"
#include "Core/Memory.h"
#include "Core/FileSystem.h"
#include "Core/Core.h"
#include "SharedHeapBlock.h"
#include "ErrorHandler.h"
#include "Parser.h"
#include <cstdarg>
#include <cfloat>
#include "Core/Matrix.h"
#include "Core/Map.h"
#ifdef SPEEDTREE_EVALUATION_BUILD
	#include "Evaluation/Key.h"
#endif
#if defined(__CELLOS_LV2__) || defined(unix) || defined(__APPLE__) || defined(__GNUC__) || defined(__psp2__)
	#include <errno.h>
#endif
using namespace SpeedTree;
using namespace std;


///////////////////////////////////////////////////////////////////////
//  Constants

const st_int32				c_nNumSharedHeapBlocks = 16;
const size_t                c_siResourceMapReserveSize = 100;


///////////////////////////////////////////////////////////////////////
//  Structure SResourceEntry

struct SResourceEntry
{
	EGfxResourceType	m_eType;
	size_t			m_siSize;
};


///////////////////////////////////////////////////////////////////////
//  Global static variables

static	CSharedHeapBlock		g_acSharedHeapBlocks[c_nNumSharedHeapBlocks];
static	CErrorHandler			g_cErrorHandler;
static	CFixedString			g_strEvalKey;
static	st_float32				g_fClipSpaceDepthNear = 0.0f;
static	st_float32				g_fClipSpaceDepthFar = 1.0f;
static	CCore::SResourceSummary	g_sResourceSummary;
static	CMutex					g_cTmpMemoryMutex;

// resource map
typedef CMap<CFixedString, SResourceEntry, CLess<CFixedString>, false> TResourceMap;
static  TResourceMap g_mResourceMap(c_siResourceMapReserveSize);


///////////////////////////////////////////////////////////////////////
//  CAllocatorInterface::CAllocatorInterface

CAllocatorInterface::CAllocatorInterface(CAllocator* pAllocator)
{
	CHeapSystem::Allocator( ) = pAllocator;
}


///////////////////////////////////////////////////////////////////////
//  CFileSystemInterface::CFileSystemInterface

CFileSystemInterface::CFileSystemInterface(CFileSystem* pFileSystem)
{
	g_pFileSystem = pFileSystem;
}


///////////////////////////////////////////////////////////////////////
//  CFileSystemInterface::Get( )

SpeedTree::CFileSystem* CFileSystemInterface::Get(void)
{
	return g_pFileSystem;
}


///////////////////////////////////////////////////////////////////////
//  CCore::CCore

CCore::CCore( ) :
	m_pSrtBufferOwned(NULL),
	m_pSrtBufferExternal(NULL),
	m_bGrassModel(false),
	m_bTexCoordsFlipped(false),
	m_nNumCollisionObjects(0),
	m_pCollisionObjects(NULL),
	m_fAmbientImageScalar(1.0f),
	m_pUserData(NULL)
{
	memset(m_pUserStrings, 0, sizeof(m_pUserStrings));
	memset(m_asiSubSrtBufferOffsets, 0, sizeof(m_asiSubSrtBufferOffsets));
}


///////////////////////////////////////////////////////////////////////
//  CCore::~CCore

CCore::~CCore( )
{
	if (m_pSrtBufferOwned)
		st_delete_array<st_byte>(m_pSrtBufferOwned);
}


///////////////////////////////////////////////////////////////////////
//	CCore::FileSizeInBytes

size_t CCore::FileSizeInBytes(const st_char* pFilename)
{
	size_t siSize = 0;

	#ifdef _XBOX
		HANDLE hFile = CreateFile(pFilename,
								  GENERIC_READ,
								  0,
								  NULL,
								  OPEN_EXISTING,
								  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
								  NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			// determine file size
			siSize = size_t(GetFileSize(hFile, NULL));

			(void) XCloseHandle(hFile);
		}
	#else
		FILE* pFile = NULL;
		if (st_fopen(&pFile, pFilename, "rb"))
		{
			// go to the end of the file
			fseek(pFile, 0L, SEEK_END);

			// determine how large the file is
			siSize = size_t(ftell(pFile));

			fclose(pFile);
		}
	#endif

	return siSize;
}


///////////////////////////////////////////////////////////////////////
//	CCore::LoadFileIntoBuffer

st_byte* CCore::LoadFileIntoBuffer(const st_char* pFilename, size_t& siBufferSize, st_byte* pClientSideBuffer)
{
	using namespace SpeedTree;

	st_byte* pBuffer = pClientSideBuffer;
	siBufferSize = 0;

	#ifdef _XBOX
		HANDLE hFile = CreateFile(pFilename,
								  GENERIC_READ,
								  0,
								  NULL,
								  OPEN_EXISTING,
								  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
								  NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			// determine file size
			DWORD dwFileSize = GetFileSize(hFile, NULL);
			if (dwFileSize > 0)
			{
				// allocate a buffer and read the contents of the file into it
				if (!pBuffer)
					pBuffer = st_new_array<st_byte>(dwFileSize, "CCore::LoadFileIntoBuffer");

				// read entire file into heap buffer
				DWORD dwBytesRead = 0;
				if (ReadFile(hFile, pBuffer, dwFileSize, &dwBytesRead, NULL) &&
					(dwBytesRead == dwFileSize))
				{
					siBufferSize = size_t(dwBytesRead);
				}
				else
				{
					// failure, so dump the buffer since a partial read is a total failure
					if (!pClientSideBuffer)
						st_delete_array<st_byte>(pBuffer);
					CCore::SetError("Only read %d of %d bytes from %s: [Xbox error code: %d]", dwBytesRead, dwFileSize, pFilename, GetLastError( ));
				}
			}
			else
				CCore::SetError("File [%s] is empty, or GetFileSize() failed", pFilename);

			(void) XCloseHandle(hFile);
		}
		else
			CCore::SetError("Failed to open [%s]: [Xbox error code: %d]\n", pFilename, GetLastError( ));
	#else
		FILE* pFile = NULL;
		if (st_fopen(&pFile, pFilename, "rb"))
		{
			// go to the end of the file
			fseek(pFile, 0L, SEEK_END);

			// determine how large the file is
			long lFileSizeInBytes = ftell(pFile);

			if (lFileSizeInBytes > 0)
			{
				// return to the beginning of the file
				st_int32 nErrorCode = fseek(pFile, 0, SEEK_SET);

				// did the calls perform without error?
				if (nErrorCode >= 0)
				{
					// allocate a buffer and read the contents of the file into it
					if (!pBuffer)
						pBuffer = st_new_array<st_byte>(lFileSizeInBytes, "CCore::LoadFileIntoBuffer");
					size_t siBytesRead = fread(pBuffer, 1, lFileSizeInBytes, pFile);

					if (siBytesRead == size_t(lFileSizeInBytes))
					{
						siBufferSize = size_t(lFileSizeInBytes);
					}
					else
					{
						if (!pClientSideBuffer)
							st_delete_array<st_byte>(pBuffer);

						char szError[512];
						st_strerror(szError, 512, errno);
						CCore::SetError("Only read %d of %d bytes from %s: [%s]", siBytesRead, lFileSizeInBytes, pFilename, szError);
					}
				}
				else
				{
					char szError[512];
					st_strerror(szError, 512, errno);
					CCore::SetError("fseek() failed to return to the beginning of the file [%s]: [%s]\n", pFilename, szError);
				}
			}
			else
				CCore::SetError("File [%s] is empty, or ftell() failed", pFilename);

			(void) fclose(pFile);
		}
		else
		{
			char szError[512];
			st_strerror(szError, 512, errno);
			CCore::SetError("Failed to open [%s]: [%s]\n", pFilename, szError);
		}
	#endif

	return pBuffer;
}


///////////////////////////////////////////////////////////////////////
//	CCore::IsCompiledForDeferred

st_bool CCore::IsCompiledForDeferred(void) const
{
	st_bool bDeferred = false;

	// all SRenderState entries will be either deferred rendering or per-vertex/per-pixel; so
	// just check the first
	if (m_sGeometry.m_nNum3dRenderStates > 0 && m_sGeometry.m_p3dRenderStates[RENDER_PASS_MAIN])
	{
		bDeferred = (m_sGeometry.m_p3dRenderStates[RENDER_PASS_MAIN][0].m_eLightingModel == LIGHTING_MODEL_DEFERRED);
	}

	return bDeferred;
}


///////////////////////////////////////////////////////////////////////
//	CCore::IsCompiledAsGrass

st_bool CCore::IsCompiledAsGrass(void) const
{
	st_bool bGrass = false;

	// all SRenderState::m_bUsedAsGrass entries will be all true or all false for a given model,
	// so just check the first
	if (m_sGeometry.m_nNum3dRenderStates > 0 && m_sGeometry.m_p3dRenderStates[RENDER_PASS_MAIN])
	{
		bGrass = m_sGeometry.m_p3dRenderStates[RENDER_PASS_MAIN][0].m_bUsedAsGrass;
	}

	return bGrass;
}


///////////////////////////////////////////////////////////////////////
//  CCore::LoadTree

st_bool CCore::LoadTree(const st_char* pFilename, st_bool bGrassModel, st_float32 fScalar)
{
	st_bool bSuccess = false;

	#ifndef NDEBUG
		#ifdef SPEEDTREE_BIG_ENDIAN
			assert(IsRunTimeBigEndian( ));
		#else
			assert(!IsRunTimeBigEndian( ));
		#endif
	#endif

	#include "Evaluation/LicenseTest_inl.h"

	// if not first load on this CCore object
	if (m_pSrtBufferOwned)
	{
		g_pFileSystem->Release(m_pSrtBufferOwned);
		m_pSrtBufferOwned = NULL;
	}

	const size_t c_siBufferSize = g_pFileSystem->FileSize(pFilename);
	if (c_siBufferSize > 0)
	{
		m_pSrtBufferOwned = g_pFileSystem->LoadFile(pFilename, CFileSystem::LONG_TERM);
		if (m_pSrtBufferOwned)
		{
			m_bGrassModel = bGrassModel;

			CParser cParser;
			bSuccess = cParser.Parse(m_pSrtBufferOwned, m_asiSubSrtBufferOffsets, st_uint32(c_siBufferSize), this, &m_sGeometry);

			if (bSuccess)
			{
				if (fScalar != 1.0f)
					ApplyScale(fScalar);

				m_strFilename = pFilename;
			}
			else
			{
				g_pFileSystem->Release(m_pSrtBufferOwned);
				m_pSrtBufferOwned = NULL;
			}
		}
		else
		{
			char szError[512];
			st_strerror(szError, 512, errno);
			CCore::SetError("CCore::LoadTree, failed to open [%s] : [%s]\n", pFilename, szError);
		}
	}
	else
	{
		char szError[512];
		st_strerror(szError, 512, errno);
		CCore::SetError("CCore::LoadTree, failed to open [%s] : [%s]\n", pFilename, szError);
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::LoadTree

st_bool CCore::LoadTree(const st_byte* pMemBlock, st_uint32 uiNumBytes, st_bool bCopyBuffer, st_bool bGrassModel, st_float32 fScalar)
{
	st_bool bSuccess = false;

	#ifndef NDEBUG
		#ifdef SPEEDTREE_BIG_ENDIAN
			assert(IsRunTimeBigEndian( ));
		#else
			assert(!IsRunTimeBigEndian( ));
		#endif
	#endif

	#include "Evaluation/LicenseTest_inl.h"

	// if not first load on this CCore object
	if (m_pSrtBufferOwned)
		st_delete_array<st_byte>(m_pSrtBufferOwned);

	m_bGrassModel = bGrassModel;
	if (bCopyBuffer)
	{
		m_pSrtBufferOwned = st_new_array<st_byte>(uiNumBytes, "CCore::LoadTree");
		memcpy(m_pSrtBufferOwned, pMemBlock, uiNumBytes);
	}
	else
	{
		m_pSrtBufferExternal = pMemBlock;
	}

	CParser cParser;
	bSuccess = cParser.Parse(SrtBuffer( ), m_asiSubSrtBufferOffsets, uiNumBytes, this, &m_sGeometry);

	if (bSuccess)
	{
		if (fScalar != 1.0f)
			ApplyScale(fScalar);

		// need some sort of unique string representation even though this tree was loaded
		// through a memory block; LoadTree() that takes a filename will overwrite this
		// value if necessary
		m_strFilename = CFixedString::Format("%x", pMemBlock);
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::SetClipSpaceDepthRange

void CCore::SetClipSpaceDepthRange(st_float32 fNear, st_float32 fFar)
{
	g_fClipSpaceDepthNear = fNear;
	g_fClipSpaceDepthFar = fFar;
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetClipSpaceDepthRange

void CCore::GetClipSpaceDepthRange(st_float32& fNear, st_float32& fFar)
{
	fNear = g_fClipSpaceDepthNear;
	fFar = g_fClipSpaceDepthFar;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ReassignPointer

void CCore::ReassignPointer(st_byte*& pPointer, const st_byte* pRefBlock) const
{
	const size_t c_siOffsetFromOriginalBlock = pPointer - pRefBlock;

	assert(m_asiSubSrtBufferOffsets[0] < c_siOffsetFromOriginalBlock);
	const size_t c_siOffsetFromNewBlock = c_siOffsetFromOriginalBlock - m_asiSubSrtBufferOffsets[0];

	pPointer = m_pSrtBufferOwned + c_siOffsetFromNewBlock;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ReassignRenderState

void CCore::ReassignRenderState(SRenderState& sRenderState, const st_byte* pOriginalSrtBuffer) const
{
	for (st_int32 nLayer = 0; nLayer < TL_NUM_TEX_LAYERS; ++nLayer)
		ReassignPointer((st_byte*&) sRenderState.m_apTextures[nLayer], pOriginalSrtBuffer);

	ReassignPointer((st_byte*&) sRenderState.m_pDescription, pOriginalSrtBuffer);
	ReassignPointer((st_byte*&) sRenderState.m_pUserData, pOriginalSrtBuffer);
}


///////////////////////////////////////////////////////////////////////
//  CCore::DeleteGeometry

void CCore::DeleteGeometry(void)
{
	if (m_asiSubSrtBufferOffsets[0] > 0 && m_asiSubSrtBufferOffsets[1] > 0)
	{
		assert(m_asiSubSrtBufferOffsets[1] > m_asiSubSrtBufferOffsets[0]);

		// make a copy of part of the m_pSrtBuffer
		st_bool bOldBufferWasOwned = (m_pSrtBufferOwned != NULL);
		const st_byte* pOriginalSrtBuffer = SrtBuffer( );
		const size_t c_siSubBufferSize = m_asiSubSrtBufferOffsets[1] - m_asiSubSrtBufferOffsets[0];
		m_pSrtBufferOwned = st_new_array<st_byte>(c_siSubBufferSize, "CCore::DeleteGeometry");
		memcpy(m_pSrtBufferOwned, pOriginalSrtBuffer + m_asiSubSrtBufferOffsets[0], c_siSubBufferSize);

		// reassign collision object pointers
		ReassignPointer((st_byte*&) m_pCollisionObjects, pOriginalSrtBuffer);
		for (st_int32 i = 0; i < m_nNumCollisionObjects; ++i)
		{
			SCollisionObject* pObject = m_pCollisionObjects + i;
			ReassignPointer((st_byte*&) pObject->m_pUserString, pOriginalSrtBuffer);
		}

		// reassign billboard pointers
		ReassignPointer((st_byte*&) m_sGeometry.m_sVertBBs.m_pTexCoords, pOriginalSrtBuffer);
		ReassignPointer((st_byte*&) m_sGeometry.m_sVertBBs.m_pRotated, pOriginalSrtBuffer);
		ReassignPointer((st_byte*&) m_sGeometry.m_sVertBBs.m_pCutoutVertices, pOriginalSrtBuffer);
		ReassignPointer((st_byte*&) m_sGeometry.m_sVertBBs.m_pCutoutIndices, pOriginalSrtBuffer);

		// reassign custom data pointers
		for (st_int32 i = 0; i < USER_STRING_COUNT; ++i)
			ReassignPointer((st_byte*&) m_pUserStrings[i], pOriginalSrtBuffer);
		ReassignPointer((st_byte*&) m_pUserData, pOriginalSrtBuffer);

		// reassign render state pointers to new, smaller, block
		for (st_int32 nPass = 0; nPass < RENDER_PASS_COUNT; ++nPass)
		{
			if (nPass == RENDER_PASS_SHADOW_CAST && !m_sGeometry.m_bShadowCastIncluded)
				continue;
			if (nPass == RENDER_PASS_DEPTH_PREPASS && !m_sGeometry.m_bDepthOnlyIncluded)
				continue;

			// main array of 3d render states
			ReassignPointer((st_byte*&) m_sGeometry.m_p3dRenderStates[nPass], pOriginalSrtBuffer);

			// 3d render states
			for (st_int32 nState = 0; nState < m_sGeometry.m_nNum3dRenderStates; ++nState)
				ReassignRenderState(m_sGeometry.m_p3dRenderStates[nPass][nState], pOriginalSrtBuffer);

			// billboard render states
			ReassignRenderState(m_sGeometry.m_aBillboardRenderStates[nPass], pOriginalSrtBuffer);
		}

		// reassigned SGeometry pointers
		ReassignPointer((st_byte*&) m_sGeometry.m_pLods, pOriginalSrtBuffer);
		for (st_int32 i = 0; i < m_sGeometry.m_nNumLods; ++i)
		{
			SLod* pLod = (SLod*) m_sGeometry.m_pLods + i;
			ReassignPointer((st_byte*&) pLod->m_pDrawCalls, pOriginalSrtBuffer);

			for (st_int32 j = 0; j < pLod->m_nNumDrawCalls; ++j)
			{
				SDrawCall* pDrawCall = (SDrawCall*) pLod->m_pDrawCalls + j;

				// clear vertex & index data since it's being deleted
				pDrawCall->m_pVertexData = NULL;
				pDrawCall->m_pIndexData = NULL;

				// reassign m_pRenderState pointer
				ReassignPointer((st_byte*&) pDrawCall->m_pRenderState, pOriginalSrtBuffer);
			}
		}

		// if this object owns m_pSrtBuffer, delete the original
		if (bOldBufferWasOwned)
		{
			st_byte* pDeleteThis = (st_byte*) pOriginalSrtBuffer;
			st_delete_array<st_byte>(pDeleteThis);
		}

		// keep DeleteGeometry() from deleting twice
		m_asiSubSrtBufferOffsets[0] = m_asiSubSrtBufferOffsets[1] = 0;
	}
}


///////////////////////////////////////////////////////////////////////
//  CCore::SResourceStats::SResourceStats

CCore::SResourceStats::SResourceStats( ) :
	m_siCurrentUsage(0),
	m_siPeakUsage(0),
	m_siCurrentQuantity(0),
	m_siPeakQuantity(0)
{
}


///////////////////////////////////////////////////////////////////////
//  CCore::ShutDown( )

void CCore::ShutDown(void)
{
	TmpHeapBlockDeleteAll( );
	
	#ifndef NDEBUG
		for (TResourceMap::iterator i = g_mResourceMap.begin( ); i != g_mResourceMap.end( ); ++i)
		{
			fprintf(stderr, "g_mResourceMap still contains [%s]\n", i->first.c_str( ));
		}
	#endif
	assert(g_mResourceMap.empty( ));
}


///////////////////////////////////////////////////////////////////////
//  CCore::SetError

void CCore::SetError(const st_char* pError, ...)
{
	const st_int32 c_nMaxErrSize = 1024;
	st_char szBuffer[c_nMaxErrSize];

	va_list vlArgs;
	va_start(vlArgs, pError);
	(void) st_vsnprintf(szBuffer, c_nMaxErrSize - 1, pError, vlArgs);
	szBuffer[c_nMaxErrSize - 1] = '\0';

	g_cErrorHandler.SetError(szBuffer);
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetError

const st_char* CCore::GetError(void)
{
	return g_cErrorHandler.GetError( );
}


///////////////////////////////////////////////////////////////////////
//  CCore::IsRunTimeBigEndian

st_bool CCore::IsRunTimeBigEndian(void)
{
	union
	{
		st_byte		m_bBytes[4];
		st_uint32	m_uiUint;
	} uTest;

	uTest.m_uiUint = 1;

	return (uTest.m_bBytes[3] == 1);
}


///////////////////////////////////////////////////////////////////////
//  CCore::Authorize

st_bool CCore::Authorize(const st_char* pKey)
{
	st_bool bSuccess = false;

	#ifdef SPEEDTREE_EVALUATION_BUILD
		if (pKey)
		{
			g_strEvalKey = pKey;

			CFixedString strFailureCause;
			st_bool bValidKey = CEvalKey::KeyIsValid(g_strEvalKey, strFailureCause);
			if (bValidKey)
			{
				bSuccess = true;

				static st_bool bOneWarning = true;
				if (bOneWarning)
				{
					const char* pMsg = "You are using the SpeedTree evaluation libraries that will soon cease to function; if using the full libraries, no need to invoke CCore::Authorize";
					SetError(pMsg);
					bOneWarning = false;
				}
			}
			else
				SetError("CCore::Authorize() failed: %s", strFailureCause.c_str( ));
		}
		else
			SetError("CCore::Authorize() failed: key string passed in was NULL");
	#else
		ST_UNREF_PARAM(pKey);

		// full builds are always authorized
		bSuccess = true;
	#endif

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::IsAuthorized

st_bool CCore::IsAuthorized(void)
{
	#include "Evaluation/LicenseTest_inl.h"

	return true;
}

///////////////////////////////////////////////////////////////////////
//  CCore::Version

const st_char* CCore::Version(st_bool bShort)
{
	if (bShort)
		return SPEEDTREE_VERSION_STRING;

	static const st_char* pVersion = "SpeedTree SDK v" SPEEDTREE_VERSION_STRING

	// debug/release
	#ifdef NDEBUG
		" Release"
	#else
		" Debug"
	#endif

	// platform
	#ifdef _XBOX
		" (Xbox 360)"
	#elif _DURANGO
		" (Xbox One/Durango)"
	#elif defined(_WIN32)
		" (Windows PC)"
	#elif defined(__CELLOS_LV2__)
		" (PlayStation 3)"
	#elif defined(__ORBIS__)
		" (PlayStation 4/Orbis)"
	#elif defined(__psp2__)
		" (PlayStation Vita)"
	#elif defined(NDEV)
		" (WiiU)"
	#elif defined(__APPLE__)
		" (MacOSX)"
	#elif defined(__linux__)
		" (Linux)"
	#else
		#ifdef SPEEDTREE_BIG_ENDIAN
			" (Unsupported Platform / Big-Endian Architecture)"
		#else
			" (Unsupported Platform / Little-Endian Architecture)"
		#endif
	#endif

	// licensed or evaluation?
	#ifdef SPEEDTREE_EVALUATION_BUILD
		", Evaluation Build"
	#else
		", Fully Licensed Build"
	#endif
	;

	return pVersion;
}


///////////////////////////////////////////////////////////////////////
//  CCore::TmpHeapBlockLock

//#define REPORT_TMP_BLOCK_STATUS

st_byte* CCore::TmpHeapBlockLock(size_t siSizeInBytes, const char* pOwner, st_int32& nHandle)
{
	st_byte* pBlock = NULL;

	g_cTmpMemoryMutex.Lock( );

	#ifdef USE_SDK_TMP_HEAP_RING_BUFFER
		// first, scan for available block that's already big enough for the job
		for (st_int32 i = 0; i < c_nNumSharedHeapBlocks; ++i)
		{
			if (g_acSharedHeapBlocks[i].IsAvailable( ) &&
				g_acSharedHeapBlocks[i].Size( ) >= siSizeInBytes)
			{
				nHandle = i;

				pBlock = g_acSharedHeapBlocks[i].Lock(siSizeInBytes, pOwner);
				assert(pBlock);
				break;
			}
		}

		// if no pre-allocated block is available, scan for first available
		if (!pBlock)
		{
			for (st_int32 i = 0; i < c_nNumSharedHeapBlocks; ++i)
			{
				if (g_acSharedHeapBlocks[i].IsAvailable( ))
				{
					nHandle = i;

					#ifdef REPORT_TMP_BLOCK_STATUS
						Report("Allocating new tmp block (need %d bytes):\n", siSizeInBytes);
						for (st_int32 j = 0; j < c_nNumSharedHeapBlocks; ++j)
						{
							Report("  block[%d] -- locked: %d, size: %7d %s\n",
								j, !g_acSharedHeapBlocks[j].IsAvailable( ),
								g_acSharedHeapBlocks[j].m_siSize, j == i ? "***" : "");
						}
					#endif

					pBlock = g_acSharedHeapBlocks[i].Lock(siSizeInBytes, pOwner);
					assert(pBlock);
					break;
				}
			}
		}
	#else
		ST_UNREF_PARAM(pOwner);

		nHandle = 0; // need a valid handle even though it won't be used in this mode
		pBlock = st_new_array<st_byte>(siSizeInBytes, "Temporary", ALLOC_TYPE_TEMPORARY);
	#endif

	g_cTmpMemoryMutex.Unlock( );

	// this is an internal error - there should always be a block available; if not,
	// look for overlapping use or increase c_nNumSharedHeapBlocks
	assert(pBlock);

	return pBlock;
}


///////////////////////////////////////////////////////////////////////
//  CCore::TmpHeapBlockUnlock

st_bool CCore::TmpHeapBlockUnlock(st_int32 nHandle)
{
	st_bool bSuccess = false;

	g_cTmpMemoryMutex.Lock( );

	#ifdef USE_SDK_TMP_HEAP_RING_BUFFER
		assert(nHandle > -1 && nHandle < c_nNumSharedHeapBlocks);

		bSuccess = g_acSharedHeapBlocks[nHandle].Unlock( );
	#else
		ST_UNREF_PARAM(nHandle);
		bSuccess = true;
	#endif

	g_cTmpMemoryMutex.Unlock( );

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::TmpHeapBlockFindHandle

st_int32 CCore::TmpHeapBlockFindHandle(const st_byte* pBlock)
{
	st_int32 nHandle = -1;

	#ifdef USE_SDK_TMP_HEAP_RING_BUFFER
		for (st_int32 i = 0; i < c_nNumSharedHeapBlocks; ++i)
		{
			if (g_acSharedHeapBlocks[i].m_pBuffer == pBlock)
			{
				nHandle = i;
				break;
			}
		}
	#else
		ST_UNREF_PARAM(pBlock);
		nHandle = 0;
	#endif

	return nHandle;
}


///////////////////////////////////////////////////////////////////////
//  CCore::TmpHeapBlockDelete

st_bool CCore::TmpHeapBlockDelete(st_int32 nHandle, size_t siSizeThreshold)
{
	st_bool bSuccess = false;

	#ifdef USE_SDK_TMP_HEAP_RING_BUFFER
		assert(nHandle > -1 && nHandle < c_nNumSharedHeapBlocks);

		if (g_acSharedHeapBlocks[nHandle].Size( ) >= siSizeThreshold)
			bSuccess = g_acSharedHeapBlocks[nHandle].Delete( );
	#else
		ST_UNREF_PARAM(nHandle);
		ST_UNREF_PARAM(siSizeThreshold);
		bSuccess = true;
	#endif

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::TmpHeapBlockDeleteAll

st_bool CCore::TmpHeapBlockDeleteAll(size_t siSizeThreshold)
{
	st_bool bSuccess = true;

	#ifdef USE_SDK_TMP_HEAP_RING_BUFFER
		for (st_int32 i = 0; i < c_nNumSharedHeapBlocks; ++i)
		{
			if (g_acSharedHeapBlocks[i].IsAvailable( ))
				bSuccess &= TmpHeapBlockDelete(i, siSizeThreshold);
			else
			{
				const char* pOwner = g_acSharedHeapBlocks[i].GetOwner( );

				CFixedString strError = CFixedString::Format("CCore::TmpHeapBlockDeleteAll, unable to delete block with handle [%d], still in use by [%s]",
					i, pOwner ? pOwner : "UNKNOWN");
				SetError(strError.c_str( ));

				bSuccess = false;
			}
		}
	#else
		ST_UNREF_PARAM(siSizeThreshold);
	#endif

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ResourceAllocated

void CCore::ResourceAllocated(EGfxResourceType eType, const CFixedString& strResourceKey, size_t siSize)
{
	// look for resource to prevent duplicate
	TResourceMap::const_iterator iFind = g_mResourceMap.find(strResourceKey);
	if (iFind == g_mResourceMap.end( ))
	{
		// not present, so add it
		SResourceEntry sEntry;
		sEntry.m_eType = eType;
		sEntry.m_siSize = siSize;
		g_mResourceMap[strResourceKey] = sEntry;

		// adjust resource summary values
		g_sResourceSummary.m_asGfxResources[eType].m_siCurrentUsage += siSize;
		g_sResourceSummary.m_asGfxResources[eType].m_siPeakUsage = st_max(g_sResourceSummary.m_asGfxResources[eType].m_siPeakUsage, g_sResourceSummary.m_asGfxResources[eType].m_siCurrentUsage);
		g_sResourceSummary.m_asGfxResources[eType].m_siCurrentQuantity++;
		g_sResourceSummary.m_asGfxResources[eType].m_siPeakQuantity = st_max(g_sResourceSummary.m_asGfxResources[eType].m_siPeakQuantity, g_sResourceSummary.m_asGfxResources[eType].m_siCurrentQuantity);
	}
	else
		SetError("CCore::ResourceAllocated(), resource [%s], size %d, already logged\n", strResourceKey.c_str( ), siSize);
}


///////////////////////////////////////////////////////////////////////
//  CCore::ResourceReleased

void CCore::ResourceReleased(const CFixedString& strResourceKey)
{
	// look for resource to prevent duplicate
	TResourceMap::iterator iFind = g_mResourceMap.find(strResourceKey);
	if (iFind == g_mResourceMap.end( ))
	{
		SetError("CCore::ResourceReleased(), cannot find resource [%s]\n", strResourceKey.c_str( ));
	}
	else
	{
		g_mResourceMap.erase(iFind);

		// adjust resource summary values
		g_sResourceSummary.m_asGfxResources[iFind->second.m_eType].m_siCurrentUsage -= iFind->second.m_siSize;
		g_sResourceSummary.m_asGfxResources[iFind->second.m_eType].m_siCurrentQuantity--;
	}
}


///////////////////////////////////////////////////////////////////////
//  CCore::GetSdkResourceUsage

const CCore::SResourceSummary& CCore::GetSdkResourceUsage(void)
{
	// heap
	g_sResourceSummary.m_sHeap.m_siCurrentQuantity = g_sResourceSummary.m_sHeap.m_siPeakQuantity = CHeapSystem::NumAllocs( );
	g_sResourceSummary.m_sHeap.m_siCurrentUsage = CHeapSystem::CurrentUse( );
	g_sResourceSummary.m_sHeap.m_siPeakUsage = CHeapSystem::PeakUse( );

	return g_sResourceSummary;
}


///////////////////////////////////////////////////////////////////////
//  Scale3dGeometry

st_bool Scale3dGeometry(SLod& sLod, st_float32 fScalar)
{
	st_bool bSuccess = true;

	// 3D geometry
	assert(static_cast<SDrawCall*>(sLod.m_pDrawCalls));
	for (st_int32 nDrawCall = 0; nDrawCall < sLod.m_nNumDrawCalls; ++nDrawCall)
	{
		SDrawCall& sDrawCall = (SDrawCall&) sLod.m_pDrawCalls[nDrawCall];
		assert(static_cast<SRenderState*>(sDrawCall.m_pRenderState));

		for (st_int32 nVertex = 0; nVertex < sDrawCall.m_nNumVertices; ++nVertex)
		{
			EVertexProperty aePropertiesToScale[ ] =
			{
				VERTEX_PROPERTY_POSITION,
				VERTEX_PROPERTY_LOD_POSITION,
				VERTEX_PROPERTY_LEAF_CARD_CORNER,
				VERTEX_PROPERTY_LEAF_CARD_SELF_SHADOW_OFFSET,
				VERTEX_PROPERTY_LEAF_ANCHOR_POINT
			};

			for (size_t nProperty = 0; nProperty < sizeof(aePropertiesToScale) / sizeof(EVertexProperty); ++nProperty)
			{
				EVertexProperty eProperty = aePropertiesToScale[nProperty];

				// not every SRT file or geometry group will have every property, so we check for it
				if (sDrawCall.m_pRenderState->m_sVertexDecl.m_asProperties[eProperty].IsPresent( ))
				{
					if (sDrawCall.m_pRenderState->m_sVertexDecl.m_asProperties[eProperty].m_eFormat == VERTEX_FORMAT_FULL_FLOAT)
					{
						st_float32 afValue[4] = { 0.0f };
						if (sDrawCall.GetProperty(eProperty, nVertex, afValue))
						{
							afValue[0] *= fScalar;
							afValue[1] *= fScalar;
							afValue[2] *= fScalar;

							bSuccess &= sDrawCall.SetProperty(eProperty, nVertex, afValue);
						}
					}
					else if (sDrawCall.m_pRenderState->m_sVertexDecl.m_asProperties[eProperty].m_eFormat == VERTEX_FORMAT_HALF_FLOAT)
					{
						st_float16 afValue16[4] = { st_float16(0.0f) };
						if (sDrawCall.GetProperty(eProperty, nVertex, afValue16))
						{
							// convert to 32-bit so we can scale, then convert back to 16
							afValue16[0] = st_float16(st_float32(afValue16[0]) * fScalar);
							afValue16[1] = st_float16(st_float32(afValue16[1]) * fScalar);
							afValue16[2] = st_float16(st_float32(afValue16[2]) * fScalar);

							bSuccess &= sDrawCall.SetProperty(eProperty, nVertex, afValue16);
						}
						else
							bSuccess = false;
					}
				}
			}
		}
	}

	// bones
	assert(sLod.m_nNumBones == 0 || static_cast<SBone*>(sLod.m_pBones));
	for (st_int32 nBone = 0; nBone < sLod.m_nNumBones; ++nBone)
	{
		SBone& sBone= (SBone&) sLod.m_pBones[nBone];

		sBone.m_vStart *= fScalar;
		sBone.m_vEnd *= fScalar;
		sBone.m_fRadius *= fScalar;
	}

	return bSuccess;
}


///////////////////////////////////////////////////////////////////////
//  CCore::ApplyScale

void CCore::ApplyScale(st_float32 fScalar)
{
	if (fScalar != 1.0f)
	{
		// 3D geometry
		assert(static_cast<SLod*>(m_sGeometry.m_pLods));
		for (st_int32 i = 0; i < m_sGeometry.m_nNumLods; ++i)
			Scale3dGeometry((SLod&) m_sGeometry.m_pLods[i], fScalar);

		// vertical billboards
		SVerticalBillboards& sVBBs = m_sGeometry.m_sVertBBs;
		sVBBs.m_fWidth *= fScalar;
		sVBBs.m_fTopPos *= fScalar;
		sVBBs.m_fBottomPos *= fScalar;

		// horizontal billboard
		SHorizontalBillboard& sHBB = m_sGeometry.m_sHorzBB;
		if (sHBB.m_bPresent)
			for (st_int32 i = 0; i < 4; ++i)
				sHBB.m_avPositions[i] *= fScalar;

		// wind object
		m_cWind.Scale(fScalar);

		// collision objects
		assert(m_pCollisionObjects);
		for (st_int32 i = 0; i < m_nNumCollisionObjects; ++i)
		{
			SCollisionObject* pObject = m_pCollisionObjects + i;

			pObject->m_vCenter1 *= fScalar;
			pObject->m_vCenter2 *= fScalar;
			pObject->m_fRadius *= fScalar;
		}

		// extents
		m_cExtents.Scale(fScalar);

		// LOD
		m_sLodProfile.Scale(fScalar);
		m_sLodProfile.Square(m_sLodProfileSquared);
	}
}
