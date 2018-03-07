// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"
#include "Serialization/Archive.h"
#include "Containers/Array.h"
#include "HAL/CriticalSection.h"

/**
 * Memory output device. Logs only into pre-allocated memory buffer.
 */
class CORE_API FOutputDeviceMemory : public FOutputDevice
{
	class FOutputDeviceMemoryProxyArchive : public FArchive
	{
		FOutputDeviceMemory& OutputDevice;
	public:
		FOutputDeviceMemoryProxyArchive(FOutputDeviceMemory& InOutputDevice)
			: OutputDevice(InOutputDevice)
		{}
		virtual void Serialize(void* V, int64 Length) override
		{
			OutputDevice.SerializeToBuffer((ANSICHAR*)V, Length / sizeof(ANSICHAR));
		}
	} ArchiveProxy;

public:
	/** 
	 * Constructor, initializing member variables.
	 *
	 * @param InPreserveSize	Bytes of the rung buffer not to overwrite (startup info etc)
	 * @param InBufferSize		Maximum size of the memory ring buffer
	 */
	FOutputDeviceMemory(int32 InPreserveSize = 256 * 1024, int32 InBufferSize = 2048 * 1024);

	/** Dumps the contents of the buffer to an archive */
	virtual void Dump(FArchive& Ar) override;

	//~ Begin FOutputDevice Interface.
	/**
	 * Closes output device and cleans up. This can't happen in the destructor
	 * as we have to call "delete" which cannot be done for static/ global
	 * objects.
	 */
	virtual void TearDown() override;

	/**
	 * Flush the write cache so the file isn't truncated in case we crash right
	 * after calling this function.
	 */
	virtual void Flush() override;

	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time ) override;
	virtual void Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category) override;
	virtual bool CanBeUsedOnAnyThread() const override
	{
		return true;
	}
	virtual bool IsMemoryOnly() const override
	{
		return true;
	}
	//~ End FOutputDevice Interface.

private:
	
	/** Serialize cast data to the actual memory buffer */
	void SerializeToBuffer(ANSICHAR* Data, int32 Length);

	/** Ring buffer */
	TArray<ANSICHAR> Buffer;
	/** Position where data starts in the buffer */
	int32 BufferStartPos;
	/** Used data size */
	int32 BufferLength;
	/** Amount of data not to overwrite */
	int32 PreserveSize;
	/** Sync object for the buffer pos */
	FCriticalSection BufferPosCritical;
};
