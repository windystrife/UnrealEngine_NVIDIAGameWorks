// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/IsEnum.h"
#include "Templates/Tuple.h"
#include "Misc/Crc.h"

/**
 * Combines two hash values to get a third.
 * Note - this function is not commutative.
 */
inline uint32 HashCombine(uint32 A, uint32 C)
{
	uint32 B = 0x9e3779b9;
	A += B;

	A -= B; A -= C; A ^= (C>>13);
	B -= C; B -= A; B ^= (A<<8);
	C -= A; C -= B; C ^= (B>>13);
	A -= B; A -= C; A ^= (C>>12);
	B -= C; B -= A; B ^= (A<<16);
	C -= A; C -= B; C ^= (B>>5);
	A -= B; A -= C; A ^= (C>>3);
	B -= C; B -= A; B ^= (A<<10);
	C -= A; C -= B; C ^= (B>>15);

	return C;
}


inline uint32 PointerHash(const void* Key,uint32 C = 0)
{
	// Avoid LHS stalls on PS3 and Xbox 360
#if PLATFORM_64BITS
	// Ignoring the lower 4 bits since they are likely zero anyway.
	// Higher bits are more significant in 64 bit builds.
	auto PtrInt = reinterpret_cast<UPTRINT>(Key) >> 4;
#else
	auto PtrInt = reinterpret_cast<UPTRINT>(Key);
#endif

	return HashCombine(PtrInt, C);
}


//
// Hash functions for common types.
//

inline uint32 GetTypeHash( const uint8 A )
{
	return A;
}

inline uint32 GetTypeHash( const int8 A )
{
	return A;
}

inline uint32 GetTypeHash( const uint16 A )
{
	return A;
}

inline uint32 GetTypeHash( const int16 A )
{
	return A;
}

inline uint32 GetTypeHash( const int32 A )
{
	return A;
}

inline uint32 GetTypeHash( const uint32 A )
{
	return A;
}

inline uint32 GetTypeHash( const uint64 A )
{
	return (uint32)A+((uint32)(A>>32) * 23);
}

inline uint32 GetTypeHash( const int64 A )
{
	return (uint32)A+((uint32)(A>>32) * 23);
}

#if PLATFORM_MAC
inline uint32 GetTypeHash( const __uint128_t A )
{
	uint64 Low = (uint64)A;
	uint64 High = (uint64)(A >> 64);
	return GetTypeHash(Low) ^ GetTypeHash(High);
}
#endif

inline uint32 GetTypeHash( float Value )
{
	return *(uint32*)&Value;
}

inline uint32 GetTypeHash( double Value )
{
	return GetTypeHash(*(uint64*)&Value);
}

inline uint32 GetTypeHash( const TCHAR* S )
{
	return FCrc::Strihash_DEPRECATED(S);
}

inline uint32 GetTypeHash( const void* A )
{
	return PointerHash(A);
}

inline uint32 GetTypeHash( void* A )
{
	return PointerHash(A);
}

template <typename EnumType>
FORCEINLINE  typename TEnableIf<TIsEnum<EnumType>::Value, uint32>::Type GetTypeHash(EnumType E)
{
	return GetTypeHash((__underlying_type(EnumType))E);
}

namespace UE4TypeHash_Private
{
	template <uint32 ArgToCombine, uint32 ArgCount>
	struct TGetTupleHashHelper
	{
		template <typename TupleType>
		FORCEINLINE static uint32 Do(uint32 Hash, const TupleType& Tuple)
		{
			return TGetTupleHashHelper<ArgToCombine + 1, ArgCount>::Do(HashCombine(Hash, GetTypeHash(Tuple.template Get<ArgToCombine>())), Tuple);
		}
	};

	template <uint32 ArgIndex>
	struct TGetTupleHashHelper<ArgIndex, ArgIndex>
	{
		template <typename TupleType>
		FORCEINLINE static uint32 Do(uint32 Hash, const TupleType& Tuple)
		{
			return Hash;
		}
	};
}

template <typename... Types>
FORCEINLINE uint32 GetTypeHash(const TTuple<Types...>& Tuple)
{
	return UE4TypeHash_Private::TGetTupleHashHelper<1u, sizeof...(Types)>::Do(GetTypeHash(Tuple.template Get<0>()), Tuple);
}

FORCEINLINE uint32 GetTypeHash(const TTuple<>& Tuple)
{
	return 0;
}
