// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/Blueprint.h"

class UBlueprintGeneratedClass;
class UUserDefinedEnum;
class UUserDefinedStruct;

/** The struct gathers dependencies of a converted BPGC */
struct BLUEPRINTCOMPILERCPPBACKEND_API FGatherConvertedClassDependencies
{
protected:
	UStruct* OriginalStruct;

public:
	// Dependencies:
	TArray<UObject*> Assets; 

	TSet<UBlueprintGeneratedClass*> ConvertedClasses;
	TSet<UUserDefinedStruct*> ConvertedStructs;
	TSet<UUserDefinedEnum*> ConvertedEnum;

	// What to include/declare in the generated code:
	TSet<UField*> IncludeInHeader;
	TSet<UField*> DeclareInHeader;
	TSet<UField*> IncludeInBody;

	TSet<TSoftObjectPtr<UPackage>> RequiredModuleNames;

	FCompilerNativizationOptions NativizationOptions;

public:
	FGatherConvertedClassDependencies(UStruct* InStruct, const FCompilerNativizationOptions& InNativizationOptions);

	UStruct* GetActualStruct() const
	{
		return OriginalStruct;
	}

	UClass* FindOriginalClass(const UClass* InClass) const;

	UClass* GetFirstNativeOrConvertedClass(UClass* InClass) const;

	TSet<const UObject*> AllDependencies() const;

public:
	bool WillClassBeConverted(const UBlueprintGeneratedClass* InClass) const;

	static void GatherAssetReferencedByUDSDefaultValue(TSet<UObject*>& Dependencies, UUserDefinedStruct* Struct);

	static bool IsFieldFromExcludedPackage(const UField* Field, const TSet<FName>& InExcludedModules);

protected:
	void DependenciesForHeader();
};
