// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==================================================================================================
	FixupNeedsLoadForEditorGameCommandlet.cpp: Fixes outdated NeedsLoadForEditorGame flags on exports
====================================================================================================*/

#include "Commandlets/FixupNeedsLoadForEditorGameCommandlet.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"

int32 UFixupNeedsLoadForEditorGameCommandlet::InitializeResaveParameters(const TArray<FString>& Tokens, TArray<FString>& MapPathNames)
{
	int32 Result = Super::InitializeResaveParameters(Tokens, MapPathNames);
	// We need ResaveClasses to be specified, otherwise we won't know what to update
	if (Result == 0 && !ResaveClasses.Num())
	{
		UE_LOG(LogContentCommandlet, Error, TEXT("FixupNeedsLoadForEditorGame commandlet requires at least one resave class name. Use -RESAVECLASS=ClassA,ClassB,ClassC to specify resave classes."));
		Result = 1;
	}
	else
	{
		for (FName& ClassName : ResaveClasses)
		{			
			if (!ResaveClassNeedsLoadForEditorGameValues.Contains(ClassName))
			{
				UClass* ResaveClass = FindObject<UClass>(ANY_PACKAGE, *ClassName.ToString());
				if (ResaveClass)
				{
					UObject* DefaultObject = ResaveClass->GetDefaultObject();
					ResaveClassNeedsLoadForEditorGameValues.Add(ClassName, DefaultObject->NeedsLoadForEditorGame());
				}
			}
			else if (Verbosity != UResavePackagesCommandlet::ONLY_ERRORS)
			{
				UE_LOG(LogContentCommandlet, Warning, TEXT("Resave Class \"%s\" could not be found. Make sure the class name is valid and that it's a native class."), *ClassName.ToString());
			}
		}
		if (ResaveClassNeedsLoadForEditorGameValues.Num() == 0)
		{
			UE_LOG(LogContentCommandlet, Error, TEXT("Got %d classes to resave but none of the exist."), ResaveClasses.Num());
			Result = 1;
		}
	}
	return Result;
}

void UFixupNeedsLoadForEditorGameCommandlet::PerformPreloadOperations(FLinkerLoad* PackageLinker, bool& bSavePackage)
{
	Super::PerformPreloadOperations(PackageLinker, bSavePackage);
	if (bSavePackage)
	{
		// The package contains a class we want to check but we don't want to save it unless
		// any of the exports has an outdated (compared to a CDO) NeedsLoadForEditorGame flag.
		bSavePackage = false;
		for (int32 ExportIndex = 0; ExportIndex < PackageLinker->ExportMap.Num(); ExportIndex++)
		{
			FName ExportClassName = PackageLinker->GetExportClassName(ExportIndex);
			bool* bNeedsLoadForEditorGameValuePtr = ResaveClassNeedsLoadForEditorGameValues.Find(ExportClassName);
			if (bNeedsLoadForEditorGameValuePtr)
			{
				FObjectExport& Export = PackageLinker->ExportMap[ExportIndex];
				// The condition below may seem confusing because bNotAlwaysLoadedForEditorGame = !bNeedsLoadForEditorGame
				// At the moment we only update assets that had bNotAlwaysLoadedForEditorGame set to true but NeedsLoadForEditorGame returns true 
				// and either bNotForClient or bNotForServer is true. 
				if (Export.bNotAlwaysLoadedForEditorGame &&
					  Export.bNotAlwaysLoadedForEditorGame == *bNeedsLoadForEditorGameValuePtr &&
						(Export.bNotForClient || Export.bNotForServer))
				{
					bSavePackage = true;
					break;
				}				
			}
		}
	}	
}
