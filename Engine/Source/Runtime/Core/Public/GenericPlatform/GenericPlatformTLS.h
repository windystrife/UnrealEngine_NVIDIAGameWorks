// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"


/**
 * It should be possible to provide a generic implementation as long as a threadID is provided. We don't do that yet.
 */
struct FGenericPlatformTLS
{
	/**
	 * Return false if this is an invalid TLS slot
	 * @param SlotIndex the TLS index to check
	 * @return true if this looks like a valid slot
	 */
	static FORCEINLINE bool IsValidTlsSlot(uint32 SlotIndex)
	{
		return SlotIndex != 0xFFFFFFFF;
	}

#if 0 // provided for reference
	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
	}

	/**
	 * Allocates a thread local store slot
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
	}

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
	}

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
	}
#endif
};
