// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CoreNative.h: Native function lookup table.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/Object.h"

struct FFrame;

/** The type of a native function callable by script */
typedef void (UObject::*Native)(FFrame& TheStack, RESULT_DECL);

// This class is deliberately simple (i.e. POD) to keep generated code size down.
struct FNameNativePtrPair
{
	const char* NameUTF8;
	Native      Pointer;
};

extern COREUOBJECT_API Native GCasts[];
uint8 COREUOBJECT_API GRegisterCast( int32 CastCode, const Native& Func );


/** A struct that maps a string name to a native function */
struct FNativeFunctionRegistrar
{
	FNativeFunctionRegistrar(class UClass* Class, const ANSICHAR* InName, Native InPointer)
	{
		RegisterFunction(Class, InName, InPointer);
	}
	static COREUOBJECT_API void RegisterFunction(class UClass* Class, const ANSICHAR* InName, Native InPointer);
	// overload for types generated from blueprints, which can have unicode names:
	static COREUOBJECT_API void RegisterFunction(class UClass* Class, const WIDECHAR* InName, Native InPointer);

	static COREUOBJECT_API void RegisterFunctions(class UClass* Class, const FNameNativePtrPair* InArray, int32 NumFunctions);
};
