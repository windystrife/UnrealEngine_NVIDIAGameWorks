// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformTLS.h: Apple platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformTLS.h"
#include "CoreTypes.h"
#if PLATFORM_MAC
#include "Mac/MacSystemIncludes.h"
#elif PLATFORM_IOS
#include "IOS/IOSSystemIncludes.h"
#endif

/**
* Apple implementation of the TLS OS functions
**/
struct CORE_API FApplePlatformTLS : public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
		return (uint32)pthread_mach_thread_np(pthread_self());
	}

	/**
	 * Allocates a thread local store slot
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
		// allocate a per-thread mem slot
		pthread_key_t SlotKey = 0;
		if (pthread_key_create(&SlotKey, NULL) != 0)
		{
			SlotKey = 0xFFFFFFFF;  // matches the Windows TlsAlloc() retval.
		}
		return SlotKey;
	}

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
		pthread_setspecific((pthread_key_t)SlotIndex, Value);
	}

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		return pthread_getspecific((pthread_key_t)SlotIndex);
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		pthread_key_delete((pthread_key_t)SlotIndex);
	}
};

typedef FApplePlatformTLS FPlatformTLS;
