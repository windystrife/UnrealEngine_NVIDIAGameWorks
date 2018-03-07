// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Disk caching functions to preserve state across runs

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"


int32 FDiskCacheInterface::GEnablePSOCache = 1;
FAutoConsoleVariableRef FDiskCacheInterface::CVarEnablePSOCache(
	TEXT("D3D12.EnablePSOCache"),
	FDiskCacheInterface::GEnablePSOCache,
	TEXT("Enables a disk cache for PipelineState Objects."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

void FDiskCacheInterface::Init(FString &filename)
{
	mFileStart = nullptr;
	mFile = 0;
	mMemoryMap = 0;
	mMapAddress = 0;
	mCurrentFileMapSize = 0;
	mCurrentOffset = 0;
	mInErrorState = false;

	mFileName = filename;
	mCacheExists = true;
	if (!GEnablePSOCache)
	{
		mCacheExists = false;
	}
	else
	{
		WIN32_FIND_DATA fileData;
		HANDLE Handle = FindFirstFile(mFileName.GetCharArray().GetData(), &fileData);
		if (Handle == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == ERROR_FILE_NOT_FOUND)
			{
				mCacheExists = false;
			}
		}
		else
		{
			FindClose(Handle);
		}
	}
	bool fileFound = mCacheExists;
	mCurrentFileMapSize = 1;
	GrowMapping(64 * 1024, true);

	if (fileFound && mFileStart)
	{
		mHeader = *(FDiskCacheHeader*)mFileStart;
		if (mHeader.mHeaderVersion != mCurrentHeaderVersion || (!mHeader.mUsesAPILibraries && FD3D12PipelineStateCache::bUseAPILibaries))
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("Disk cache is stale. Disk Cache version: %d App version: %d"), mHeader.mHeaderVersion, mCurrentHeaderVersion);
			ClearDiskCache();
			Init(filename);
		}
	}
	else
	{
		mHeader.mHeaderVersion = mCurrentHeaderVersion;
		mHeader.mNumPsos = 0;
		mHeader.mSizeInBytes = 0;
		mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;
	}
}

void FDiskCacheInterface::GrowMapping(SIZE_T size, bool firstrun)
{
	if (!GEnablePSOCache || mInErrorState)
	{
		return;
	}

	if (mCurrentFileMapSize - mCurrentOffset < size)
	{
		while ((mCurrentFileMapSize - mCurrentOffset) < size)
		{
			mCurrentFileMapSize += mFileGrowSize;
		}
	}
	else
	{
		return;
	}

	if (mMapAddress)
	{
		FlushViewOfFile(mMapAddress, mCurrentOffset);
		UnmapViewOfFile(mMapAddress);
	}
	if (mMemoryMap)
	{
		CloseHandle(mMemoryMap);
	}
	if (mFile)
	{
		CloseHandle(mFile);
	}

	uint32 flag = (mCacheExists) ? OPEN_EXISTING : CREATE_NEW;
	// open the shader cache file
	mFile = CreateFile(mFileName.GetCharArray().GetData(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flag, FILE_ATTRIBUTE_NORMAL, NULL);
	if (mFile == INVALID_HANDLE_VALUE)
	{
		//error state!
		mInErrorState = true;
		return;
	}

	mCacheExists = true;

	uint32 fileSize = GetFileSize(mFile, NULL);
	if (fileSize == 0)
	{
		byte data[64];
		FMemory::Memset(data, NULL, _countof(data));
		//It's invalide to map a zero sized file so write some junk data in that case
		WriteFile(mFile, data, _countof(data), NULL, NULL);
	}
	else if (firstrun)
	{
		mCurrentFileMapSize = fileSize;
	}

	mMemoryMap = CreateFileMapping(mFile, NULL, PAGE_READWRITE, 0, (uint32)mCurrentFileMapSize, NULL);
	if (mMemoryMap == (HANDLE)nullptr)
	{
		//error state!
		mInErrorState = true;
		ClearDiskCache();
		return;
	}

	mMapAddress = MapViewOfFile(mMemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, mCurrentFileMapSize);
	if (mMapAddress == (HANDLE)nullptr)
	{
		//error state!
		mInErrorState = true;
		ClearDiskCache();
		return;
	}

	mFileStart = (byte*)mMapAddress;
}

void FDiskCacheInterface::BeginAppendPSO()
{
	mHeader.mNumPsos += 1;
}

bool FDiskCacheInterface::AppendData(const void* pData, size_t size)
{
	GrowMapping(size, false);
	if (IsInErrorState())
	{
		return false;
	}

	memcpy((mFileStart + mCurrentOffset), pData, size);
	mCurrentOffset += size;

	return true;
}

bool FDiskCacheInterface::SetPointerAndAdvanceFilePosition(void** pDest, size_t size, bool backWithSystemMemory)
{
	GrowMapping(size, false);
	if (IsInErrorState())
	{
		return false;
	}

	// Back persistent objects in system memory to avoid
	// troubles when re-mapping the file.
	if (backWithSystemMemory)
	{
		// Optimization: most (all?) of the shader input layout semantic names are "ATTRIBUTE"...
		// instead of making 1000's of attribute strings, just set it to a static one.
		static const char attribute[] = "ATTRIBUTE";
		if (size == sizeof(attribute) && FMemory::Memcmp((mFileStart + mCurrentOffset), attribute, sizeof(attribute)) == 0)
		{
			*pDest = (void*)attribute;
		}
		else
		{
			void* newMemory = FMemory::Malloc(size);
			if (newMemory)
			{
				FMemory::Memcpy(newMemory, (mFileStart + mCurrentOffset), size);
				mBackedMemory.Add(newMemory);
				*pDest = newMemory;
			}
			else
			{
				check(false);
				return false;
			}
		}
	}
	else
	{
		*pDest = mFileStart + mCurrentOffset;
	}

	mCurrentOffset += size;

	return true;
}

void FDiskCacheInterface::Reset(RESET_TYPE type)
{
	mCurrentOffset = sizeof(FDiskCacheHeader);

	if (type == RESET_TO_AFTER_LAST_OBJECT)
	{
		mCurrentOffset += mHeader.mSizeInBytes;
	}
}

void FDiskCacheInterface::Close()
{
	mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;

	check(mCurrentOffset >= sizeof(FDiskCacheHeader));
	mHeader.mSizeInBytes = mCurrentOffset - sizeof(FDiskCacheHeader);

	if (!IsInErrorState())
	{
		if (mMapAddress)
		{
			*(FDiskCacheHeader*)mFileStart = mHeader;
			FlushViewOfFile(mMapAddress, mCurrentOffset);
			UnmapViewOfFile(mMapAddress);
		}
		if (mMemoryMap)
		{
			CloseHandle(mMemoryMap);
		}
		if (mFile)
		{
			CloseHandle(mFile);
		}
	}
}

void FDiskCacheInterface::ClearDiskCache()
{
	// Prevent reads/writes 
	mInErrorState = true;
	mHeader.mHeaderVersion = mCurrentHeaderVersion;
	mHeader.mNumPsos = 0;
	mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;

	if (!GEnablePSOCache)
	{
		return;
	}

	if (mMapAddress)
	{
		UnmapViewOfFile(mMapAddress);
	}
	if (mMemoryMap)
	{
		CloseHandle(mMemoryMap);
	}
	if (mFile)
	{
		CloseHandle(mFile);
	}
#ifdef UNICODE
	BOOL result = DeleteFileW(mFileName.GetCharArray().GetData());
#else
	bool result = DeleteFileA(mFileName.GetCharArray().GetData());
#endif
	UE_LOG(LogD3D12RHI, Warning, TEXT("Deleted PSO Cache with result %d"), result);
}

void FDiskCacheInterface::Flush()
{
	mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;

	check(mCurrentOffset >= sizeof(FDiskCacheHeader));
	mHeader.mSizeInBytes = mCurrentOffset - sizeof(FDiskCacheHeader);

	if (mMapAddress && !IsInErrorState())
	{
		*(FDiskCacheHeader*)mFileStart = mHeader;
		FlushViewOfFile(mMapAddress, mCurrentOffset);
	}
}

void* FDiskCacheInterface::GetDataAt(SIZE_T Offset) const
{
	void* data = mFileStart + Offset;

	check(data <= (mFileStart + mCurrentFileMapSize));
	return data;
}

void* FDiskCacheInterface::GetDataAtStart() const
{
	return GetDataAt(sizeof(FDiskCacheHeader));
}