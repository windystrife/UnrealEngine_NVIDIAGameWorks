// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformNamedPipe.h"
#include "WindowsSystemIncludes.h"


#if PLATFORM_SUPPORTS_NAMED_PIPES

class FString;


// Windows wrapper for named pipe communications
class CORE_API FWindowsPlatformNamedPipe
	: public FGenericPlatformNamedPipe
{
public:

	FWindowsPlatformNamedPipe();
	virtual ~FWindowsPlatformNamedPipe();

public:

	// FGenericPlatformNamedPipe overrides

	virtual bool Create(const FString& PipeName, bool bAsServer, bool bAsync) override;
	virtual bool Destroy() override;
	virtual bool OpenConnection() override;
	virtual bool BlockForAsyncIO() override;
	virtual bool IsReadyForRW() const override;
	virtual bool UpdateAsyncStatus() override;
	virtual bool WriteBytes(int32 NumBytes, const void* Data) override;
	virtual bool ReadBytes(int32 NumBytes, void* OutData) override;
	virtual bool IsCreated() const override;
	virtual bool HasFailed() const override;

private:

	void*		Pipe;
	Windows::OVERLAPPED	Overlapped;
	double		LastWaitingTime;
	bool		bUseOverlapped : 1;
	bool		bIsServer : 1;

	enum EState
	{
		State_Uninitialized,
		State_Created,
		State_Connecting,
		State_ReadyForRW,
		State_WaitingForRW,

		State_ErrorPipeClosedUnexpectedly,
	};

	EState State;

	bool UpdateAsyncStatusAfterRW();
};


typedef FWindowsPlatformNamedPipe FPlatformNamedPipe;

#endif	// PLATFORM_SUPPORTS_NAMED_PIPES
