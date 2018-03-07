// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "StructSerializerTestTypes.generated.h"

/**
 * Test structure for numeric properties.
 */
USTRUCT()
struct FStructSerializerNumericTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int8 Int8;

	UPROPERTY()
	int16 Int16;

	UPROPERTY()
	int32 Int32;

	UPROPERTY()
	int64 Int64;

	UPROPERTY()
	uint8 UInt8;

	UPROPERTY()
	uint16 UInt16;

	UPROPERTY()
	uint32 UInt32;

	UPROPERTY()
	uint64 UInt64;

	UPROPERTY()
	float Float;

	UPROPERTY()
	double Double;

	/** Default constructor. */
	FStructSerializerNumericTestStruct()
		: Int8(-127)
		, Int16(-32767)
		, Int32(-2147483647)
		, Int64(-92233720368547/*75807*/)
		, UInt8(255)
		, UInt16(65535)
		, UInt32(4294967295)
		, UInt64(18446744073709/*551615*/)
		, Float(4.125)
		, Double(1.03125)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerNumericTestStruct( ENoInit ) { }
};


/**
 * Test structure for boolean properties.
 */
USTRUCT()
struct FStructSerializerBooleanTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool BoolFalse;

	UPROPERTY()
	bool BoolTrue;

	UPROPERTY()
	uint32 Bitfield;

	/** Default constructor. */
	FStructSerializerBooleanTestStruct()
		: BoolFalse(false)
		, BoolTrue(true)
		, Bitfield(1)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerBooleanTestStruct( ENoInit ) { }
};


/**
 * Test structure for UObject properties.
 */
USTRUCT()
struct FStructSerializerObjectTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TSubclassOf<class UObject> Class;

	UPROPERTY()
	class UObject* ObjectPtr;

	/** Default constructor. */
	FStructSerializerObjectTestStruct()
		: ObjectPtr(nullptr)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerObjectTestStruct( ENoInit ) { }
};


/**
 * Test structure for properties of various built-in types.
 */
USTRUCT()
struct FStructSerializerBuiltinTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FGuid Guid;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FString String;

	UPROPERTY()
	FRotator Rotator;

	UPROPERTY()
	FText Text;

	UPROPERTY()
	FVector Vector;

	/** Default constructor. */
	FStructSerializerBuiltinTestStruct()
		: Guid(FGuid::NewGuid())
		, String("Test String")
		, Rotator(4096, 8192, 16384)
		, Text(FText::FromString("Test Text"))
		, Vector(1.0f, 2.0f, 3.0f)
	{ }

	/** Creates an uninitialized instance. */
	FStructSerializerBuiltinTestStruct( ENoInit ) { }
};


/**
 * Test structure for array properties.
 */
USTRUCT()
struct FStructSerializerArrayTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<int32> Int32Array;

	UPROPERTY()
	int32 StaticSingleElement[1];

	UPROPERTY()
	int32 StaticInt32Array[3];

	UPROPERTY()
	float StaticFloatArray[3];

	UPROPERTY()
	TArray<FVector> VectorArray;

	/** Default constructor. */
	FStructSerializerArrayTestStruct()
	{
		Int32Array.Add(-1);
		Int32Array.Add(0);
		Int32Array.Add(1);

		StaticSingleElement[0] = 42;

		StaticInt32Array[0] = -1;
		StaticInt32Array[1] = 0;
		StaticInt32Array[2] = 1;

		StaticFloatArray[0] = -1.0f;
		StaticFloatArray[1] = 0.0f;
		StaticFloatArray[2] = 1.0f;

		VectorArray.Add(FVector(1.0f, 2.0f, 3.0f));
		VectorArray.Add(FVector(-1.0f, -2.0f, -3.0f));
	}

	/** Creates an uninitialized instance. */
	FStructSerializerArrayTestStruct( ENoInit ) { }
};


/**
 * Test structure for map properties.
 */
USTRUCT()
struct FStructSerializerMapTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TMap<int32, FString> IntToStr;

	UPROPERTY()
	TMap<FString, FString> StrToStr;

	UPROPERTY()
	TMap<FString, FVector> StrToVec;

	/** Default constructor. */
	FStructSerializerMapTestStruct()
	{
		IntToStr.Add(1, TEXT("One"));
		IntToStr.Add(2, TEXT("Two"));
		IntToStr.Add(3, TEXT("Three"));

		StrToStr.Add(TEXT("StrAll"), TEXT("All"));
		StrToStr.Add(TEXT("StrYour"), TEXT("Your"));
		StrToStr.Add(TEXT("StrBase"), TEXT("Base"));

		StrToVec.Add(TEXT("V000"), FVector(0.0f, 0.0f, 0.0f));
		StrToVec.Add(TEXT("V123"), FVector(1.0f, 2.0f, 3.0f));
		StrToVec.Add(TEXT("V666"), FVector(6.0f, 6.0f, 6.0f));
	}

	/** Creates an uninitialized instance. */
	FStructSerializerMapTestStruct( ENoInit ) { }
};


/**
 * Test structure for all supported types.
 */
USTRUCT()
struct FStructSerializerTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FStructSerializerNumericTestStruct Numerics;

	UPROPERTY()
	FStructSerializerBooleanTestStruct Booleans;

	UPROPERTY()
	FStructSerializerObjectTestStruct Objects;

	UPROPERTY()
	FStructSerializerBuiltinTestStruct Builtins;

	UPROPERTY()
	FStructSerializerArrayTestStruct Arrays;

	UPROPERTY()
	FStructSerializerMapTestStruct Maps;

	/** Default constructor. */
	FStructSerializerTestStruct() { }

	/** Creates an uninitialized instance. */
	FStructSerializerTestStruct( ENoInit )
		: Numerics(NoInit)
		, Booleans(NoInit)
		, Objects(NoInit)
		, Builtins(NoInit)
		, Arrays(NoInit)
		, Maps(NoInit)
	{ }
};
