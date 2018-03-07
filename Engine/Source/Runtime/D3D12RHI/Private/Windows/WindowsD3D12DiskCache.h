// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Disk caching functions to preserve state across runs

#pragma once

#define IL_MAX_SEMANTIC_NAME 255

class FDiskCacheInterface
{
	// Increment this if changes are made to the
	// disk caches so stale caches get updated correctly
	static const uint32 mCurrentHeaderVersion = 5;
	struct FDiskCacheHeader
	{
		uint32 mHeaderVersion;
		uint32 mNumPsos;
		uint32 mSizeInBytes; // The number of bytes after the header
		bool   mUsesAPILibraries;
	};

private:
	FString mFileName;
	byte*   mFileStart;
	HANDLE  mFile;
	HANDLE  mMemoryMap;
	HANDLE  mMapAddress;
	SIZE_T  mCurrentFileMapSize;
	SIZE_T  mCurrentOffset;
	bool    mCacheExists;
	bool    mInErrorState;
	FDiskCacheHeader mHeader;

	// There is the potential for the file mapping to grow
	// in that case all of the pointers will be invalid. Back
	// some of the pointers we might read again (i.e. shade byte
	// code for PSO mapping) in persitent system memory.
	TArray<void*> mBackedMemory;

	static const SIZE_T mFileGrowSize = (1024 * 1024); // 1 megabyte;
	static int32 GEnablePSOCache;
	static FAutoConsoleVariableRef CVarEnablePSOCache;

	void GrowMapping(SIZE_T size, bool firstrun);

public:
	enum RESET_TYPE
	{
		RESET_TO_FIRST_OBJECT,
		RESET_TO_AFTER_LAST_OBJECT
	};

	bool AppendData(const void* pData, size_t size);
	bool SetPointerAndAdvanceFilePosition(void** pDest, size_t size, bool backWithSystemMemory = false);
	void Reset(RESET_TYPE type);
	void Init(FString &filename);
	// NVCHANGE_BEGIN: Add VXGI
	// Number of PSOs should be tracked inside the cache handler.
	// With NV pipeline state extensions, not every PSO is written to the disk cache,
	// so using LowLevelGraphicsPipelineStateCache.Count() for the disk cache is no longer correct.
	void BeginAppendPSO();
	void Close();
	void Flush();
	// NVCHANGE_END: Add VXGI
	void ClearDiskCache();
	uint32 GetNumPSOs() const
	{
		return mHeader.mNumPsos;
	}
	uint32 GetSizeInBytes() const
	{
		return mHeader.mSizeInBytes;
	}
	
	FORCEINLINE_DEBUGGABLE bool IsInErrorState() const
	{
		return !GEnablePSOCache || mInErrorState;
	}

	SIZE_T GetCurrentOffset() const { return mCurrentOffset; }

	void* GetDataAt(SIZE_T Offset) const;
	void* GetDataAtStart() const;

	~FDiskCacheInterface()
	{
		for (void* memory : mBackedMemory)
		{
			if (memory)
			{
				FMemory::Free(memory);
			}
		}
	}
};