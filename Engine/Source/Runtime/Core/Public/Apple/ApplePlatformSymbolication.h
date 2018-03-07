// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	ApplePlatformSymbolication.h: Apple platform implementation of symbolication
==============================================================================================*/

#pragma once

#include "GenericPlatformSymbolication.h"
#include "ApplePlatformStackWalk.h"
#include "Serialization/Archive.h"
#include "SharedPointer.h"

/**
 * Opaque symbol cache for improved symbolisation performance.
 */
struct FApplePlatformSymbolCache
{
	void* Buffer0;
	void* Buffer1;
};

/**
 * Apple symbol database.
 */
struct FApplePlatformSymbolDatabase
{
	FApplePlatformSymbolDatabase();
	FApplePlatformSymbolDatabase(FApplePlatformSymbolDatabase const& Other);
	~FApplePlatformSymbolDatabase();
	FApplePlatformSymbolDatabase& operator=(FApplePlatformSymbolDatabase const& Other);
	
	TSharedPtr<FGenericPlatformSymbolDatabase> GenericDB;
	FApplePlatformSymbolCache AppleDB;
};

/**
 * Apple symbol database hash.
 */
struct FApplePlatformSymbolDatabaseKeyFuncs
{
	enum { bAllowDuplicateKeys = 0 };
	typedef typename TCallTraits<FString>::ParamType KeyInitType;
	typedef typename TCallTraits<FApplePlatformSymbolDatabase>::ParamType ElementInitType;

	/**
	 * @return The key used to index the given element.
	 */
	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.GenericDB->Signature;
	}

	/**
	 * @return True if the keys match.
	 */
	static FORCEINLINE bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}

	/** Calculates a hash index for a key. */
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

typedef TSet<FApplePlatformSymbolDatabase, FApplePlatformSymbolDatabaseKeyFuncs> FApplePlatformSymbolDatabaseSet;

/**
 * Apple platform implementation of symbolication - not async. handler safe, so don't call during crash reporting!
 */
struct CORE_API FApplePlatformSymbolication
{
	static void EnableCoreSymbolication(bool const bEnable);
	
	static bool LoadSymbolDatabaseForBinary(FString SourceFolder, FString Binary, FString BinarySignature, FApplePlatformSymbolDatabase& OutDatabase);
	static bool SaveSymbolDatabaseForBinary(FString TargetFolder, FString Name, FString BinarySignature, FApplePlatformSymbolDatabase& Database);
	
	static bool SymbolInfoForStrippedSymbol(FApplePlatformSymbolDatabase const& Database, uint64 ProgramCounter, uint64 ModuleOffset, FString ModuleSignature, FProgramCounterSymbolInfo& Info);
	
	static bool SymbolInfoForAddress(uint64 ProgramCounter, FProgramCounterSymbolInfo& Info);
};

typedef FApplePlatformSymbolDatabase FPlatformSymbolDatabase;
typedef FApplePlatformSymbolication FPlatformSymbolication;
typedef TSet<FApplePlatformSymbolDatabase, FApplePlatformSymbolDatabaseKeyFuncs> FPlatformSymbolDatabaseSet;
