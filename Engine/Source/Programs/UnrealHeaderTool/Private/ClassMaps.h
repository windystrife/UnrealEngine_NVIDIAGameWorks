// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Stack.h"
#include "UnderlyingEnumType.h"

#include "UnrealSourceFile.h"

class UField;
class UClass;
class UProperty;
class UPackage;
class UEnum;
class FClassDeclarationMetaData;
class FArchive;
struct FManifestModule;
class FUnrealSourceFile;
class FUnrealTypeDefinitionInfo;

extern TMap<FString, TSharedRef<FUnrealSourceFile> > GUnrealSourceFilesMap;
extern TMap<UField*, TSharedRef<FUnrealTypeDefinitionInfo> > GTypeDefinitionInfoMap;
extern TMap<UClass*, FString> GClassStrippedHeaderTextMap;
extern TMap<UClass*, FString> GClassHeaderNameWithNoPathMap;
extern TSet<FUnrealSourceFile*> GPublicSourceFileSet;
extern TMap<UProperty*, FString> GArrayDimensions;
extern TMap<UPackage*,  const FManifestModule*> GPackageToManifestModuleMap;
extern TMap<UField*, uint32> GGeneratedCodeCRCs;
extern TMap<UEnum*, EUnderlyingEnumType> GEnumUnderlyingTypes;
extern TMap<FName, TSharedRef<FClassDeclarationMetaData> > GClassDeclarations;
extern TSet<UProperty*> GUnsizedProperties;
extern TSet<UField*> GEditorOnlyDataTypes;
extern TMap<UStruct*, TTuple<TSharedRef<FUnrealSourceFile>, int32>> GStructToSourceLine;

/** Types access specifiers. */
enum EAccessSpecifier
{
	ACCESS_NotAnAccessSpecifier = 0,
	ACCESS_Public,
	ACCESS_Private,
	ACCESS_Protected,
	ACCESS_Num,
};

inline FArchive& operator<<(FArchive& Ar, EAccessSpecifier& ObjectType)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		ObjectType = EAccessSpecifier(Value);
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)ObjectType;
		Ar << Value;
	}

	return Ar;
}
