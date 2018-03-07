// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Function.h"
#include "Stats/Stats.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Function.h"

class IAsyncReadRequest;

DECLARE_MEMORY_STAT_EXTERN(TEXT("Async File Handle Memory"), STAT_AsyncFileMemory, STATGROUP_Memory, CORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Async File Handles"), STAT_AsyncFileHandles, STATGROUP_Memory, CORE_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Async File Requests"), STAT_AsyncFileRequests, STATGROUP_Memory, CORE_API);

// Note on threading. Like the rest of the filesystem platform abstraction, these methods are threadsafe, but it is expected you are not concurrently _using_ these data structures. 

class IAsyncReadRequest;
typedef TFunction<void(bool bWasCancelled, IAsyncReadRequest*)> FAsyncFileCallBack;

class CORE_API IAsyncReadRequest
{
protected:
	union
	{
		PTRINT Size;
		uint8* Memory;
	};
	FAsyncFileCallBack Callback;
	bool bDataIsReady;
	bool bCompleteAndCallbackCalled;
	bool bCompleteSync;
	bool bCanceled;
	const bool bSizeRequest;
	const bool bUserSuppliedMemory;
public:

	FORCEINLINE IAsyncReadRequest(FAsyncFileCallBack* InCallback, bool bInSizeRequest, uint8* UserSuppliedMemory)
		: bDataIsReady(false)
		, bCompleteAndCallbackCalled(false)
		, bCompleteSync(false)
		, bCanceled(false)
		, bSizeRequest(bInSizeRequest)
		, bUserSuppliedMemory(!!UserSuppliedMemory)
	{
		if (InCallback)
		{
			Callback = *InCallback;
		}
		if (bSizeRequest)
		{
			Size = -1;
			check(!UserSuppliedMemory && !bUserSuppliedMemory); // size requests don't do memory
		}
		else
		{
			Memory = UserSuppliedMemory;
			check((!!Memory) == bUserSuppliedMemory);
		}
		INC_DWORD_STAT(STAT_AsyncFileRequests);
	}

	/* Not legal to destroy the request until it is complete. */
	virtual ~IAsyncReadRequest()
	{
		check(bCompleteAndCallbackCalled && (bSizeRequest || !Memory)); // must be complete, and if it was a read request, the memory should be gone
		DEC_DWORD_STAT(STAT_AsyncFileRequests);
	}

	/**
	* Nonblocking poll of the state of completion.
	* @return true if the request is complete
	**/
	FORCEINLINE bool PollCompletion() TSAN_SAFE
	{
		return bCompleteAndCallbackCalled;
	}

	/**
	* Waits for the request to complete, but not longer than the given time limit
	* @param TimeLimitSeconds	Zero to wait forever, otherwise the maximum amount of time to wait.
	* @return true if the request is complete
	**/
	FORCEINLINE bool WaitCompletion(float TimeLimitSeconds = 0.0f)
	{
		if (PollCompletion())
		{
			return true;
		}
		WaitCompletionImpl(TimeLimitSeconds);
		return PollCompletion();
	}

	/** Cancel the request. This is a non-blocking async call and so does not ensure completion! **/
	FORCEINLINE void Cancel()
	{
		if (!bCanceled)
		{
			bCanceled = true;
			bDataIsReady = true;
			FPlatformMisc::MemoryBarrier();
			if (!PollCompletion())
			{
				return CancelImpl();
			}
		}
	}

	/**
	* Return the size of a completed size request. Not legal to call unless the request is complete.
	* @return Returned size of the file or -1 if the file was not found, the request was canceled or other error.
	**/
	FORCEINLINE int64 GetSizeResults()
	{
		check(bDataIsReady && bSizeRequest);
		return bCanceled ? -1 : Size;
	}

	/**
	* Return the bytes of a completed read request. Not legal to call unless the request is complete.
	* @return Returned memory block which if non-null contains the bytes read. Caller owns the memory block and must call FMemory::Free on it when done. Can be null if the file was not found or could not be read or the request was cancelled, or the request priority was AIOP_Precache.
	**/
	FORCEINLINE uint8* GetReadResults() TSAN_SAFE
	{
		check(bDataIsReady && !bSizeRequest);
		uint8* Result = Memory;
		if (bCanceled && Result)
		{
			Result = nullptr;
		}
		else
		{
			Memory = nullptr;
		}
		return Result;
	}

protected:
	/**
	* Waits for the request to complete, but not longer than the given time limit
	* @param TimeLimitSeconds	Zero to wait forever, otherwise the maximum amount of time to wait.
	* @return true if the request is complete
	**/
	virtual void WaitCompletionImpl(float TimeLimitSeconds) = 0;

	/** Cancel the request. This is a non-blocking async call and so does not ensure completion! **/
	virtual void CancelImpl() = 0;

	void SetDataComplete()
	{
		bDataIsReady = true;
		FPlatformMisc::MemoryBarrier();
		if (Callback)
		{
			Callback(bCanceled, this);
		}
		FPlatformMisc::MemoryBarrier();
	}

	void SetAllComplete()
	{
		bCompleteAndCallbackCalled = true;
		FPlatformMisc::MemoryBarrier();
	}

	void SetComplete() TSAN_SAFE
	{
		SetDataComplete();
		SetAllComplete();
	}
};

class CORE_API IAsyncReadFileHandle
{
public:
	IAsyncReadFileHandle()
	{
		INC_DWORD_STAT(STAT_AsyncFileHandles);
	}
	/** Destructor, also the only way to close the file handle. It is not legal to delete an async file with outstanding requests. You must always call WaitCompletion before deleting a request. **/
	virtual ~IAsyncReadFileHandle()
	{
		DEC_DWORD_STAT(STAT_AsyncFileHandles);
	}

	/**
	* Request the size of the file. This is also essentially the existence check.
	* @param CompleteCallback		Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	* @return A request for the size. This is owned by the caller and must be deleted by the caller.
	**/
	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) = 0;

	/**
	* Submit an async request and/or wait for an async request
	* @param Offset					Offset into the file to start reading.
	* @param BytesToRead			number of bytes to read. If this request is AIOP_Preache, the size can be anything, even MAX_int64, otherwise the size and offset must be fully contained in the file.
	* @PAram Priority				Priority of the request. If this is AIOP_Precache, then memory will never be returned. The request should always be canceled and waited for, even for a precache request.
	* @param CompleteCallback		Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	* @return A request for the read. This is owned by the caller and must be deleted by the caller.
	**/
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr, uint8* UserSuppliedMemory = nullptr) = 0;

	// Non-copyable
	IAsyncReadFileHandle(const IAsyncReadFileHandle&) = delete;
	IAsyncReadFileHandle& operator=(const IAsyncReadFileHandle&) = delete;
};
