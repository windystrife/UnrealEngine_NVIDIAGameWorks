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

#include "Core/FileSystem.h"
#include "Core/Core.h"
#include "Utilities/Utility.h"
#if !defined(__psp2__) && !defined(NDEV)
	#include <sys/stat.h>
#endif
using namespace SpeedTree;


///////////////////////////////////////////////////////////////////////  
//  CFileSystem::FileExists

st_bool CFileSystem::FileExists(const char* pFilename)
{
	FILE* pFile = fopen(pFilename, "r");
	st_bool bExists = (pFile != NULL);

	if (pFile)
		fclose(pFile);

	return bExists;
}


///////////////////////////////////////////////////////////////////////  
//  CFileSystem::FileSize

size_t CFileSystem::FileSize(const char* pFilename)
{
	return CCore::FileSizeInBytes(pFilename);
}


///////////////////////////////////////////////////////////////////////  
//  CFileSystem::LoadFile

st_byte* CFileSystem::LoadFile(const char* pFilename, ETermHint eTermHint)
{
	st_byte* pFileBlock = NULL;

	if (pFilename)
	{
		size_t siObjFileSize = CCore::FileSizeInBytes(pFilename);
		if (siObjFileSize > 0)
		{
			CFixedString strDesc = CFixedString::Format("CFileSystem::LoadFile(%s)", pFilename);

			// if short term, use the SDK's temporary heap block system to keep heap allocations low
			if (eTermHint == SHORT_TERM)
			{
				// get temporary heap block
				st_int32 nHeapBlockHandle = 0;
				pFileBlock = CCore::TmpHeapBlockLock(siObjFileSize, strDesc.c_str( ), nHeapBlockHandle);

				// if heap block successful, load file contents into it
				if (pFileBlock)
				{
					if (!CCore::LoadFileIntoBuffer(pFilename, siObjFileSize, pFileBlock))
					{
						// if failed to load, we should free the block
						CCore::TmpHeapBlockUnlock(nHeapBlockHandle);
						pFileBlock = NULL;
					}
				}
			}
			// if long term, allocate a heap block to be freed later
			else
			{
				pFileBlock = st_new_array<st_byte>(siObjFileSize, strDesc.c_str( ));
				if (pFileBlock)
				{
					if (!CCore::LoadFileIntoBuffer(pFilename, siObjFileSize, pFileBlock))
					{
						// if failed to load, we should free the block
						st_delete_array<st_byte>(pFileBlock);
						pFileBlock = NULL;
					}
				}
			}
		}
	}

	return pFileBlock;
}


///////////////////////////////////////////////////////////////////////  
//  CFileSystem::Release

void CFileSystem::Release(st_byte* pBuffer)
{
	if (pBuffer)
	{
		// check for short term usage
		st_int32 nHandle = CCore::TmpHeapBlockFindHandle(pBuffer);
		if (nHandle > -1)
			CCore::TmpHeapBlockUnlock(nHandle);
		else
		{
			// otherwise, must be long term
			st_delete_array<st_byte>(pBuffer);
		}
	}
}


///////////////////////////////////////////////////////////////////////  
//  CFileSystem::CleanPlatformFilename

CFixedString CFileSystem::CleanPlatformFilename(const st_char* pFilename)
{
	CFixedString strCleanFilename;

	#if defined(__CELLOS_LV2__)
		// the PS3 seems to deal well with less-than-clean filenames
		strCleanFilename = pFilename;
	#else
		#ifdef __ORBIS__
			strCleanFilename += '/';
		#endif

		const char* c_szDelimiters = "/\\";

		if (pFilename)
		{
			st_uint32 uiSize = st_uint32(strlen(pFilename));
			if (uiSize > 0)
			{
				st_bool bNetworkDrive = (uiSize > 1) && (pFilename[0] == '\\' || pFilename[0] == '/') && (pFilename[1] == '\\' || pFilename[1] == '/');

				// make local copy that strtok can use
				assert(uiSize < c_siFixedStringDefaultLength);
				CFixedString strFilenameCopy = pFilename;

				const st_int32 c_nMaxFolders = 100;
				CStaticArray<CFixedString> aFilenameTokens(c_nMaxFolders + 1, "CFileSystem::CleanPlatformFilename", false);

				if (!bNetworkDrive && (pFilename[0] == '\\' || pFilename[0] == '/')) //path begins with delimiter
					aFilenameTokens.push_back(""); // insert null token to recreate delimiter at beginning of path

				// find first token and start token-scanning loop
				const char* pToken = strtok((char*) strFilenameCopy.c_str( ), c_szDelimiters);
				while (pToken)
				{
					bool bKeepToken = true;
					#ifdef _XBOX
						// Xbox 360 doesn't support in a path, but it can occur in our pipeline
						if (strcmp(pToken, "..") == 0 && !aFilenameTokens.empty( ))
						{
							aFilenameTokens.pop_back( );
							bKeepToken = false;
						}
					#endif

					if (bKeepToken)
						aFilenameTokens.push_back(pToken);

					// continue scanning
					pToken = strtok(NULL, c_szDelimiters);
				}

				// build return string by reassembling the tokens with a single folder divider symbol between each
				if (bNetworkDrive)
					strCleanFilename = "//";
				for (size_t i = 0; i < aFilenameTokens.size( ); ++i)
				{
					strCleanFilename += aFilenameTokens[i];
					if (i < aFilenameTokens.size( ) - 1)
						strCleanFilename += c_szFolderSeparator;
				}
			}
		}
	#endif

	return strCleanFilename;
}


///////////////////////////////////////////////////////////////////////  
//  CFileSystem::CompareFileTimes
//
//	This function is mostly used by the GLSL shader loader, hence it
//	does not function correctly for PS3/PSVita.

CFileSystem::ETimeCompare CFileSystem::CompareFileTimes(const st_char* pFilenameOne, const st_char* pFilenameTwo)
{
	#if defined(__GNUC__) || defined(__psp2__) || defined(NDEV) || defined(__ORBIS__)
		ST_UNREF_PARAM(pFilenameOne);
		ST_UNREF_PARAM(pFilenameTwo);
		return FILE_ERROR;
	#else
		ETimeCompare eReturn = FILE_ERROR;

		#if defined(WIN32) || defined(_XBOX)
			struct _stat sStatsOne;
			struct _stat sStatsTwo;
			st_int32 iResultOne = _stat(pFilenameOne, &sStatsOne);
			st_int32 iResultTwo = _stat(pFilenameTwo, &sStatsTwo);
		#else
			struct stat sStatsOne;
			struct stat sStatsTwo;
			st_int32 iResultOne = stat(pFilenameOne, &sStatsOne);
			st_int32 iResultTwo = stat(pFilenameTwo, &sStatsTwo);
		#endif	

		if (iResultOne == 0 && iResultTwo == 0)
		{
			eReturn = EQUAL;
			if (sStatsOne.st_mtime < sStatsTwo.st_mtime)
				eReturn = FIRST_OLDER;
			else if (sStatsOne.st_mtime > sStatsTwo.st_mtime)
				eReturn = SECOND_OLDER;
		}

		return eReturn;
	#endif
}
