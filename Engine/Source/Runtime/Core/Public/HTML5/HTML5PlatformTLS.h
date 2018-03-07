// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5TLS.h: HTML5 platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTLS.h"

/**
 * HTML5 implementation of the TLS OS functions
 */
struct CORE_API FHTML5TLS : public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		return 0;
	}

	/**
	 * Allocates a thread local store slot
	 */
	static uint32 AllocTlsSlot(void);

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static void SetTlsValue(uint32 SlotIndex,void* Value);

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static void* GetTlsValue(uint32 SlotIndex);

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		// nothing to do, just grow the array forever
		// @todo if this done a lot, we can make a TMap
	}

protected:

	// @todo html5 threads: without threads, we can don't need real TLS, just a map
};

typedef FHTML5TLS FPlatformTLS;
