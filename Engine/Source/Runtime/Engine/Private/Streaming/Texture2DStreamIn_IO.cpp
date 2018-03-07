// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn.cpp: Stream in helper for 2D textures using texture streaming files.
=============================================================================*/

#include "Streaming/Texture2DStreamIn_IO.h"
#include "RenderUtils.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"

FTexture2DStreamIn_IO::FTexture2DStreamIn_IO(UTexture2D* InTexture, int32 InRequestedMips, bool InPrioritizedIORequest)
	: FTexture2DStreamIn(InTexture, InRequestedMips)
	, bPrioritizedIORequest(InPrioritizedIORequest)
	, IOFileOffset(0)
	, IOFileHandle(nullptr)
{
	FMemory::Memzero(IORequests, sizeof(IORequests));
}

FTexture2DStreamIn_IO::~FTexture2DStreamIn_IO()
{
	// Work must be done here because derived destructors have been called now and so derived members are invalid.

	check(!IOFileHandle);

	if (AsyncCancelIORequestsTask)
	{
		ensure(AsyncCancelIORequestsTask->IsWorkDone());
		AsyncCancelIORequestsTask->EnsureCompletion();
	}

#if DO_CHECK
	for (IAsyncReadRequest* IORequest : IORequests)
	{
		check(!IORequest);
	}
#endif
}

void FTexture2DStreamIn_IO::SetIOFilename(const FContext& Context)
{
	const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();

	const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();
	for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip; ++MipIndex)
	{
		const FTexture2DMipMap& MipMap = OwnerMips[MipIndex];
		if (MipMap.BulkData.IsStoredCompressedOnDisk())
		{
			UE_LOG(LogTexture, Error, TEXT("Compression at the package level is no longer supported."));
			IOFilename.Reset();
			break;
		}
		else if (MipMap.BulkData.GetBulkDataSize() <= 0)
		{
			UE_LOG(LogTexture, Error, TEXT("Invalid bulk data size."));
			IOFilename.Reset();
			break;
		}

		if (MipIndex == PendingFirstMip)
		{
			IOFilename = MipMap.BulkData.GetFilename();

			if (GEventDrivenLoaderEnabled)
			{
				if (IOFilename.EndsWith(TEXT(".uasset")) || IOFilename.EndsWith(TEXT(".umap")))
				{
					IOFileOffset = -IFileManager::Get().FileSize(*IOFilename);
					check(IOFileOffset < 0);
					IOFilename = FPaths::GetBaseFilename(IOFilename, false) + TEXT(".uexp");
					UE_LOG(LogTexture, Error, TEXT("Streaming from the .uexp file '%s' this MUST be in a ubulk instead for best performance."), *IOFilename);
				}
			}
		}
		else if (IOFilename != MipMap.BulkData.GetFilename())
		{
			UE_LOG(LogTexture, Error, TEXT("All of the streaming mips must be stored in the same file."), *IOFilename);
			IOFilename.Reset();
			break;
		}
	}

	if (IOFilename.IsEmpty())
	{
		MarkAsCancelled();
	}
}

void FTexture2DStreamIn_IO::SetIORequests(const FContext& Context)
{
	SetAsyncFileCallback(Context);

	check(!IOFileHandle);
	IOFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*IOFilename);
	if (IOFileHandle)
	{
		const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();

		for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip && !IsCancelled(); ++MipIndex)
		{
			const FTexture2DMipMap& MipMap = OwnerMips[MipIndex];

			check(MipData[MipIndex]);

			// Increment as we push the requests. If a requests complete immediately, then it will call the callback
			// but that won't do anything because the tick would not try to acquire the lock since it is already locked.
			TaskSynchronization.Increment();

			IORequests[MipIndex] = IOFileHandle->ReadRequest(
				MipMap.BulkData.GetBulkDataOffsetInFile() + IOFileOffset, 
				MipMap.BulkData.GetBulkDataSize(), 
				bPrioritizedIORequest ? AIOP_BelowNormal : AIOP_Low, 
				&AsyncFileCallBack, 
				(uint8*)MipData[MipIndex]);
		}
	}
	else
	{
		MarkAsCancelled();
	}
}

void FTexture2DStreamIn_IO::CancelIORequests()
{
	for (int32 MipIndex = 0; MipIndex < MAX_TEXTURE_MIP_COUNT; ++MipIndex)
	{
		IAsyncReadRequest* IORequest = IORequests[MipIndex];
		if (IORequest)
		{
			// Calling cancel will trigger the SetAsyncFileCallback() which will also try a tick but will fail.
			IORequest->Cancel();
		}
	}
}

void FTexture2DStreamIn_IO::ClearIORequests(const FContext& Context)
{
	if (IOFileHandle)
	{
		const TIndirectArray<FTexture2DMipMap>& OwnerMips = Context.Texture->GetPlatformMips();
		const int32 CurrentFirstMip = Context.Resource->GetCurrentFirstMip();

		for (int32 MipIndex = PendingFirstMip; MipIndex < CurrentFirstMip; ++MipIndex)
		{
			IAsyncReadRequest* IORequest = IORequests[MipIndex];
			IORequests[MipIndex] = nullptr;

			if (IORequest)
			{
				// If clearing requests not yet completed, cancel and wait.
				if (!IORequest->PollCompletion())
				{
					IORequest->Cancel();
					IORequest->WaitCompletion();
				}
				delete IORequest;
			}
		}

		delete IOFileHandle;
		IOFileHandle = nullptr;
	}
}

void FTexture2DStreamIn_IO::SetAsyncFileCallback(const FContext& Context)
{
	AsyncFileCallBack = [this, Context](bool bWasCancelled, IAsyncReadRequest* Req)
	{
		// At this point task synchronization would hold the number of pending requests.
		TaskSynchronization.Decrement();
		
		if (bWasCancelled)
		{
			MarkAsCancelled();
		}

		// The tick here is intended to schedule the success or cancel callback.
		// Using TT_None ensure gets which could create a dead lock.
		Tick(Context.Texture, FTexture2DUpdate::TT_None);
	};
}

void FTexture2DStreamIn_IO::Abort()
{
	if (!IsCancelled())
	{
		FTexture2DStreamIn::Abort();

		// IO requests can only exist in the lifetime of IOFileHandle.
		if (IOFileHandle)
		{
			// Prevent the update from being considered done before this is finished.
			// By checking that it was not already cancelled, we make sure this doesn't get called twice.
			FPlatformAtomics::InterlockedIncrement(&ScheduledTaskCount);
			if (AsyncCancelIORequestsTask)
			{
				AsyncCancelIORequestsTask->EnsureCompletion();
			}
			AsyncCancelIORequestsTask = MakeUnique<FAsyncCancelIORequestsTask>(this);
			AsyncCancelIORequestsTask->StartBackgroundTask();
		}
	}
}

void FTexture2DStreamIn_IO::FCancelIORequestsTask::DoWork()
{
	check(PendingUpdate);
	// Acquire the lock of this object in order to cancel any pending IO.
	// If the object is currently being ticked, wait.
	PendingUpdate->DoLock();
	PendingUpdate->CancelIORequests();
	PendingUpdate->DoUnlock();
	FPlatformAtomics::InterlockedDecrement(&PendingUpdate->ScheduledTaskCount);
}
