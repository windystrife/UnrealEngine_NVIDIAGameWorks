// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#ifndef WINDOWS_PLATFORM_ATOMICS_GUARD
	#define WINDOWS_PLATFORM_ATOMICS_GUARD
#else
	#error Nesting AllowWindowsPlatformAtomics.h is not allowed!
#endif

#define InterlockedIncrement _InterlockedIncrement
#define InterlockedDecrement _InterlockedDecrement
#define InterlockedAdd _InterlockedAdd
#define InterlockedExchange _InterlockedExchange
#define InterlockedExchangeAdd _InterlockedExchangeAdd
#define InterlockedCompareExchange _InterlockedCompareExchange

#if PLATFORM_64BITS
	#define InterlockedCompareExchangePointer _InterlockedCompareExchangePointer
#else
	#define InterlockedCompareExchangePointer __InlineInterlockedCompareExchangePointer
#endif

#if PLATFORM_64BITS
	#define InterlockedExchange64 _InterlockedExchange64
	#define InterlockedExchangeAdd64 _InterlockedExchangeAdd64
	#define InterlockedCompareExchange64 _InterlockedCompareExchange64
	#define InterlockedIncrement64 _InterlockedIncrement64
	#define InterlockedDecrement64 _InterlockedDecrement64
#endif
