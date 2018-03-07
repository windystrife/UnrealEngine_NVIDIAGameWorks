// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/Function.h"
#include "HAL/PlatformTLS.h"
#include "HAL/TlsAutoCleanup.h"

template <typename FuncType> class TFunctionRef;

/**
 * Thread singleton initializer.
 */
class FThreadSingletonInitializer
{
public:

	/**
	* @return an instance of a singleton for the current thread.
	*/
	static CORE_API FTlsAutoCleanup* Get( TFunctionRef<FTlsAutoCleanup*()> CreateInstance, uint32& TlsSlot );
};


/**
 * This a special version of singleton. It means that there is created only one instance for each thread.
 * Calling Get() method is thread-safe.
 */
template < class T >
class TThreadSingleton : public FTlsAutoCleanup
{
	/**
	 * @return TLS slot that holds a TThreadSingleton.
	 */
	static uint32& GetTlsSlot()
	{
		static uint32 TlsSlot = 0xFFFFFFFF;
		return TlsSlot;
	}

protected:

	/** Default constructor. */
	TThreadSingleton()
		: ThreadId(FPlatformTLS::GetCurrentThreadId())
	{}

	/**
	 * @return a new instance of the thread singleton.
	 */
	static FTlsAutoCleanup* CreateInstance()
	{
		return new T();
	}

	/** Thread ID of this thread singleton. */
	const uint32 ThreadId;

public:

	/**
	 *	@return an instance of a singleton for the current thread.
	 */
	FORCEINLINE static T& Get()
	{
		return *(T*)FThreadSingletonInitializer::Get( [](){ return (FTlsAutoCleanup*)new T(); }, T::GetTlsSlot() ); //-V572
	}
};

