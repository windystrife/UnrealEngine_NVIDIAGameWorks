// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClassMaps.h"
#include "UnrealHeaderTool.h"
#include "UnrealTypeDefinitionInfo.h"

TMap<FString, TSharedRef<FUnrealSourceFile> > GUnrealSourceFilesMap;
TMap<UField*, TSharedRef<FUnrealTypeDefinitionInfo> > GTypeDefinitionInfoMap;
TMap<UClass*, FString> GClassStrippedHeaderTextMap;
TMap<UClass*, FString> GClassHeaderNameWithNoPathMap;
TSet<FUnrealSourceFile*> GPublicSourceFileSet;
TMap<UProperty*, FString> GArrayDimensions;
TMap<UPackage*,  const FManifestModule*> GPackageToManifestModuleMap;
TMap<UField*, uint32> GGeneratedCodeCRCs;
TMap<UEnum*,  EUnderlyingEnumType> GEnumUnderlyingTypes;
TMap<FName, TSharedRef<FClassDeclarationMetaData> > GClassDeclarations;
TSet<UProperty*> GUnsizedProperties;
TSet<UField*> GEditorOnlyDataTypes;
TMap<UStruct*, TTuple<TSharedRef<FUnrealSourceFile>, int32>> GStructToSourceLine;
