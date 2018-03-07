// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformTLS.h"
#include "WindowsSystemIncludes.h"


/**
 * Windows implementation of the TLS OS functions.
 */
struct CORE_API FWindowsPlatformTLS
	: public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's identifier.
	 *
	 * @return The thread identifier.
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		return Windows::GetCurrentThreadId();
	}

	/**
	 * Allocates a thread local store slot.
	 *
	 * @return The index of the allocated slot.
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
		return Windows::TlsAlloc();
	}

	/**
	 * Sets a value in the specified TLS slot.
	 *
	 * @param SlotIndex the TLS index to store it in.
	 * @param Value the value to store in the slot.
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
		Windows::TlsSetValue(SlotIndex,Value);
	}

	/**
	 * Reads the value stored at the specified TLS slot.
	 *
	 * @param SlotIndex The index of the slot to read.
	 * @return The value stored in the slot.
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		return Windows::TlsGetValue(SlotIndex);
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		Windows::TlsFree(SlotIndex);
	}
};


typedef FWindowsPlatformTLS FPlatformTLS;
