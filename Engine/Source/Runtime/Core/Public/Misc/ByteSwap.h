// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

// These macros are not safe to use unless data is UNSIGNED!
#define BYTESWAP_ORDER16_unsigned(x) ((((x) >> 8) & 0xff) + (((x) << 8) & 0xff00))
#define BYTESWAP_ORDER32_unsigned(x) (((x) >> 24) + (((x) >> 8) & 0xff00) + (((x) << 8) & 0xff0000) + ((x) << 24))


static FORCEINLINE uint16 BYTESWAP_ORDER16 (uint16 val)
{
	return(BYTESWAP_ORDER16_unsigned(val));
}

static FORCEINLINE int16 BYTESWAP_ORDER16 (int16 val)
{
	uint16 uval = *((uint16*)&val);
	uval = BYTESWAP_ORDER16_unsigned(uval);

	return *((int16*) &uval);
}

static FORCEINLINE uint32 BYTESWAP_ORDER32 (uint32 val)
{
	return (BYTESWAP_ORDER32_unsigned(val));
}

static FORCEINLINE int32 BYTESWAP_ORDER32 (int32 val)
{
	uint32 uval = *((uint32*)&val);
	uval = BYTESWAP_ORDER32_unsigned(uval);

	return *((int32*)&uval);
}

static FORCEINLINE float BYTESWAP_ORDERF (float val)
{
	uint32 uval = *((uint32*)&val);
	uval = BYTESWAP_ORDER32_unsigned(uval);

	return *((float*) &uval);
}

static FORCEINLINE uint64 BYTESWAP_ORDER64 (uint64 Value)
{
	uint64 Swapped = 0;
	uint8* Forward = (uint8*)&Value;
	uint8* Reverse = (uint8*)&Swapped + 7;
	for( int32 i=0; i<8; i++ )
	{
		*(Reverse--) = *(Forward++); // copy into Swapped
	}
	return Swapped;
}

static FORCEINLINE int64 BYTESWAP_ORDER64 (int64 Value)
{
	int64 Swapped = 0;
	uint8* Forward = (uint8*)&Value;
	uint8* Reverse = (uint8*)&Swapped + 7;

	for (int32 i = 0; i < 8; i++)
	{
		*(Reverse--) = *(Forward++); // copy into Swapped
	}

	return Swapped;
}

static FORCEINLINE void BYTESWAP_ORDER_TCHARARRAY (TCHAR* str)
{
	for (TCHAR* c = str; *c; ++c)
	{
		*c = BYTESWAP_ORDER16_unsigned(*c);
	}
}


// General byte swapping.
#if PLATFORM_LITTLE_ENDIAN
	#define INTEL_ORDER16(x)   (x)
	#define INTEL_ORDER32(x)   (x)
	#define INTEL_ORDERF(x)    (x)
	#define INTEL_ORDER64(x)   (x)
	#define INTEL_ORDER_TCHARARRAY(x)
#else
	#define INTEL_ORDER16(x)			BYTESWAP_ORDER16(x)
	#define INTEL_ORDER32(x)			BYTESWAP_ORDER32(x)
	#define INTEL_ORDERF(x)				BYTESWAP_ORDERF(x)
	#define INTEL_ORDER64(x)			BYTESWAP_ORDER64(x)
	#define INTEL_ORDER_TCHARARRAY(x)	BYTESWAP_ORDER_TCHARARRAY(x)
#endif
