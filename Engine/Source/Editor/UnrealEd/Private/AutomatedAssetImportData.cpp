// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomatedAssetImportData.h"
#include "Factories/Factory.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "UObjectIterator.h"

DEFINE_LOG_CATEGORY(LogAutomatedImport);

UAutomatedAssetImportData::UAutomatedAssetImportData()
	: GroupName()
	, bReplaceExisting(false)
	, bSkipReadOnly(false)
{

}

bool UAutomatedAssetImportData::IsValid() const
{
	// This data is valid if there is at least one filename to import, there is a valid destination path
	// and either no factory was supported (automatic factory picking) or a valid factory was found
	return Filenames.Num() > 0 && !DestinationPath.IsEmpty() && (FactoryName.IsEmpty() || Factory != nullptr);
}

void UAutomatedAssetImportData::Initialize(TSharedPtr<FJsonObject> InImportGroupJsonData)
{
	ImportGroupJsonData = InImportGroupJsonData;
	if(Filenames.Num() > 0)
	{
		// If a factory doesn't have a vaild full path assume it is an unreal internal factory
		if(!FactoryName.IsEmpty() && !FactoryName.StartsWith("/Script/"))
		{
			FName FactoryFName = *FactoryName;
			// Find the factory among loaded factory classes
			EObjectFlags ExclusionFlags = RF_NoFlags;
			for (UFactory* TestFactory : TObjectRange<UFactory>(ExclusionFlags))
			{
				if (TestFactory->GetClass()->GetFName() == FactoryFName)
				{
					// factory has been found
					FactoryName = TestFactory->GetClass()->GetPathName();
					break;
				}
			}
		}

		if(!FactoryName.IsEmpty())
		{
			UClass* FactoryClass = LoadClass<UObject>(nullptr, *FactoryName, nullptr, LOAD_None, nullptr);
			if(FactoryClass)
			{
				UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);

				if(NewFactory->bEditorImport)
				{
					// Check that every file can be imported
					TArray<FString> InvalidFilesForFactory;
					for(const FString& Filename : Filenames)
					{
						if(!NewFactory->FactoryCanImport(Filename))
						{
							InvalidFilesForFactory.Add(Filename);
						}
					}

					if(InvalidFilesForFactory.Num() > 0)
					{
						UE_LOG(LogAutomatedImport, Error, TEXT("Factory %s could not import one or more of the source files"), *FactoryName);
						for(const FString& InvalidFiles : InvalidFilesForFactory)
						{
							UE_LOG(LogAutomatedImport, Error, TEXT("    %s"), *InvalidFiles);
						}
					}
					else
					{
						// All files are valid. Use this factory
						Factory = NewFactory;
					}
				}
			}
			else
			{
				UE_LOG(LogAutomatedImport, Error, TEXT("Factory %s could not be found"), *FactoryName);
			}
		}
		else
		{
			UE_LOG(LogAutomatedImport, Log, TEXT("Factory was not specified, will be set automatically"));
		}
	}

	if(!DestinationPath.IsEmpty() && FPackageName::GetPackageMountPoint(DestinationPath) == NAME_None)
	{
		// Path doesnt have a valid moint point.  assume it is in /Game
		DestinationPath = TEXT("/Game") / DestinationPath;

		UE_LOG(LogAutomatedImport, Warning, TEXT("DestinationPath has no valid mount point.  Assuming /Game is the mount point"));
	}

	if (!LevelToLoad.IsEmpty())
	{
		FText FailReason;

		if (!FPackageName::IsValidLongPackageName(LevelToLoad, false, &FailReason))
		{
			UE_LOG(LogAutomatedImport, Error, TEXT("Invalid level specified: %s"), *FailReason.ToString());
		}
	}

	FString PackagePath;
	FString FailureReason;
	if(!FPackageName::TryConvertFilenameToLongPackageName(DestinationPath, PackagePath, &FailureReason))
	{
		UE_LOG(LogAutomatedImport, Error, TEXT("Invalid Destination Path (%s): %s"), *DestinationPath, *FailureReason);
		DestinationPath.Empty();
	}
	else
	{
		// Package path is what we will use for importing.  So use that as the dest path now that it has been created
		DestinationPath = PackagePath;
	}
}

FString UAutomatedAssetImportData::GetDisplayName() const
{
	return GroupName.Len() > 0 ? GroupName : GetName();
}
