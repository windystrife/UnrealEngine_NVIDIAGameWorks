// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "WindowsSystemIncludes.h"
#include <intrin.h>

/**
 * Windows implementation of the Atomics OS functions
 */
struct CORE_API FWindowsPlatformAtomics
	: public FGenericPlatformAtomics
{
	static FORCEINLINE int32 InterlockedIncrement( volatile int32* Value )
	{
		return (int32)_InterlockedIncrement((long*)Value);
	}

#if PLATFORM_HAS_64BIT_ATOMICS
	static FORCEINLINE int64 InterlockedIncrement( volatile int64* Value )
	{
		#if PLATFORM_64BITS
			return (int64)::_InterlockedIncrement64((int64*)Value);
		#else
			// No explicit instruction for 64-bit atomic increment on 32-bit processors; has to be implemented in terms of CMPXCHG8B
			for (;;)
			{
				int64 OldValue = *Value;
				if (_InterlockedCompareExchange64(Value, OldValue + 1, OldValue) == OldValue)
				{
					return OldValue + 1;
				}
			}
		#endif
	}
#endif

	static FORCEINLINE int32 InterlockedDecrement( volatile int32* Value )
	{
		return (int32)::_InterlockedDecrement((long*)Value);
	}

#if PLATFORM_HAS_64BIT_ATOMICS
	static FORCEINLINE int64 InterlockedDecrement( volatile int64* Value )
	{
		#if PLATFORM_64BITS
			return (int64)::_InterlockedDecrement64((int64*)Value);
		#else
			// No explicit instruction for 64-bit atomic decrement on 32-bit processors; has to be implemented in terms of CMPXCHG8B
			for (;;)
			{
				int64 OldValue = *Value;
				if (_InterlockedCompareExchange64(Value, OldValue - 1, OldValue) == OldValue)
				{
					return OldValue - 1;
				}
			}
		#endif
	}
#endif

	static FORCEINLINE int32 InterlockedAdd( volatile int32* Value, int32 Amount )
	{
		return (int32)::_InterlockedExchangeAdd((long*)Value, (long)Amount);
	}

#if PLATFORM_HAS_64BIT_ATOMICS
	static FORCEINLINE int64 InterlockedAdd( volatile int64* Value, int64 Amount )
	{
		#if PLATFORM_64BITS
			return (int64)::_InterlockedExchangeAdd64((int64*)Value, (int64)Amount);
		#else
			// No explicit instruction for 64-bit atomic add on 32-bit processors; has to be implemented in terms of CMPXCHG8B
			for (;;)
			{
				int64 OldValue = *Value;
				if (_InterlockedCompareExchange64(Value, OldValue + Amount, OldValue) == OldValue)
				{
					return OldValue + Amount;
				}
			}
		#endif
	}
#endif

	static FORCEINLINE int32 InterlockedExchange( volatile int32* Value, int32 Exchange )
	{
		return (int32)::_InterlockedExchange((long*)Value, (long)Exchange);
	}

	static FORCEINLINE int64 InterlockedExchange( volatile int64* Value, int64 Exchange )
	{
		#if PLATFORM_64BITS
			return ::_InterlockedExchange64(Value, Exchange);
		#else
			// No explicit instruction for 64-bit atomic exchange on 32-bit processors; has to be implemented in terms of CMPXCHG8B
			for (;;)
			{
				int64 OldValue = *Value;
				if (_InterlockedCompareExchange64(Value, Exchange, OldValue) == OldValue)
				{
					return OldValue;
				}
			}
		#endif
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
			if (IsAligned(Dest) == false)
			{
				HandleAtomicsFailure(TEXT("InterlockedExchangePointer requires Dest pointer to be aligned to %d bytes"), sizeof(void*));
			}
		#endif

		#if PLATFORM_64BITS
			return (void*)::_InterlockedExchange64((int64*)(Dest), (int64)(Exchange));
		#else
			return (void*)::_InterlockedExchange((long*)(Dest), (long)(Exchange));
		#endif
	}

	static FORCEINLINE int32 InterlockedCompareExchange( volatile int32* Dest, int32 Exchange, int32 Comparand )
	{
		return (int32)::_InterlockedCompareExchange((long*)Dest, (long)Exchange, (long)Comparand);
	}

#if PLATFORM_HAS_64BIT_ATOMICS
	static FORCEINLINE int64 InterlockedCompareExchange( volatile int64* Dest, int64 Exchange, int64 Comparand )
	{
		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (IsAligned(Dest) == false)
			{
				HandleAtomicsFailure(TEXT("InterlockedCompareExchangePointer requires Dest pointer to be aligned to %d bytes"), sizeof(void*));
			}
		#endif

		return (int64)::_InterlockedCompareExchange64(Dest, Exchange, Comparand);
	}
	static FORCEINLINE int64 AtomicRead64(volatile const int64* Src)
	{
		return InterlockedCompareExchange((volatile int64*)Src, 0, 0);
	}

#endif

	/**
	 *	The function compares the Destination value with the Comparand value:
	 *		- If the Destination value is equal to the Comparand value, the Exchange value is stored in the address specified by Destination, 
	 *		- Otherwise, the initial value of the Destination parameter is stored in the address specified specified by Comparand.
	 *	
	 *	@return true if Comparand equals the original value of the Destination parameter, or false if Comparand does not equal the original value of the Destination parameter.
	 *	
	 *	Early AMD64 processors lacked the CMPXCHG16B instruction.
	 *	To determine whether the processor supports this operation, call the IsProcessorFeaturePresent function with PF_COMPARE64_EXCHANGE128.
	 */
#if	PLATFORM_HAS_128BIT_ATOMICS
	static FORCEINLINE bool InterlockedCompareExchange128( volatile FInt128* Dest, const FInt128& Exchange, FInt128* Comparand )
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (IsAligned(Dest,16) == false)
		{
			HandleAtomicsFailure(TEXT("InterlockedCompareExchangePointer requires Dest pointer to be aligned to 16 bytes") );
		}
		if (IsAligned(Comparand,16) == false)
		{
			HandleAtomicsFailure(TEXT("InterlockedCompareExchangePointer requires Comparand pointer to be aligned to 16 bytes") );
		}
#endif

		return ::_InterlockedCompareExchange128((int64 volatile *)Dest, Exchange.High, Exchange.Low, (int64*)Comparand) == 1;
	}
	/**
	* Atomic read of 128 bit value with a memory barrier
	*/
	static FORCEINLINE void AtomicRead128(const volatile FInt128* Src, FInt128* OutResult)
	{
		FInt128 Zero;
		Zero.High = 0;
		Zero.Low = 0;
		*OutResult = Zero;
		InterlockedCompareExchange128((volatile FInt128*)Src, Zero, OutResult);
	}

#endif // PLATFORM_HAS_128BIT_ATOMICS

	static FORCEINLINE void* InterlockedCompareExchangePointer( void** Dest, void* Exchange, void* Comparand )
	{
		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (IsAligned(Dest) == false)
			{
				HandleAtomicsFailure(TEXT("InterlockedCompareExchangePointer requires Dest pointer to be aligned to %d bytes"), sizeof(void*));
			}
		#endif

		#if PLATFORM_64BITS
			return (void*)::_InterlockedCompareExchange64((int64 volatile *)Dest, (int64)Exchange, (int64)Comparand);
		#else
			return (void*)::_InterlockedCompareExchange((long volatile *)Dest, (long)Exchange, (long)Comparand);
		#endif
	}

	/**
	* @return true, if the processor we are running on can execute compare and exchange 128-bit operation.
	* @see cmpxchg16b, early AMD64 processors don't support this operation.
	*/
	static FORCEINLINE bool CanUseCompareExchange128()
	{
		return !!Windows::IsProcessorFeaturePresent( WINDOWS_PF_COMPARE_EXCHANGE128 );
	}

protected:
	/**
	 * Handles atomics function failure.
	 *
	 * Since 'check' has not yet been declared here we need to call external function to use it.
	 *
	 * @param InFormat - The string format string.
	 */
	static void HandleAtomicsFailure( const TCHAR* InFormat, ... );
};



typedef FWindowsPlatformAtomics FPlatformAtomics;
