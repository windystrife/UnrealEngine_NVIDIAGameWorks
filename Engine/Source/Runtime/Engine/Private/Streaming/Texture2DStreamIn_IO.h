// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Texture2DStreamIn.h: Stream in helper for 2D textures using texture streaming files.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Texture2DStreamIn.h"

// Base StreamIn framework exposing MipData
class FTexture2DStreamIn_IO : public FTexture2DStreamIn
{
public:

	FTexture2DStreamIn_IO(UTexture2D* InTexture, int32 InRequestedMips, bool InPrioritizedIORequest);
	~FTexture2DStreamIn_IO();

protected:

	// ****************************
	// ********* Helpers **********
	// ****************************

	// Set the IO filename for streaming the mips.
	void SetIOFilename(const FContext& Context);
	// Set the IO requests for streaming the mips.
	void SetIORequests(const FContext& Context);
	// Cancel / destroy each requests created in SetIORequests()
	void ClearIORequests(const FContext& Context);
	// Set the IO callback used for streaming the mips.
	void SetAsyncFileCallback(const FContext& Context);

	// Cancel all IO requests.
	void CancelIORequests();

	// Start an async task to cancel pending IO requests.
	void Abort() override;

private:

	class FCancelIORequestsTask : public FNonAbandonableTask
	{
	public:
		FCancelIORequestsTask(FTexture2DStreamIn_IO* InPendingUpdate) : PendingUpdate(InPendingUpdate) {}
		void DoWork();
		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FCancelIORequestsTask, STATGROUP_ThreadPoolAsyncTasks);
		}
	protected:
		FTexture2DStreamIn_IO* PendingUpdate;
	};

	typedef FAsyncTask<FCancelIORequestsTask> FAsyncCancelIORequestsTask;
	TUniquePtr<FAsyncCancelIORequestsTask> AsyncCancelIORequestsTask;
	friend class FCancelIORequestsTask;


	// Request for loading into each mip.
	IAsyncReadRequest* IORequests[MAX_TEXTURE_MIP_COUNT];

	bool bPrioritizedIORequest;

	// Async handle.
	FString IOFilename;
	int64 IOFileOffset;

	IAsyncReadFileHandle* IOFileHandle;
	FAsyncFileCallBack AsyncFileCallBack;


};

