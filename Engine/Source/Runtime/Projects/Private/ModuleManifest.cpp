// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ModuleManifest.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"

FModuleManifest::FModuleManifest()
{
}

FString FModuleManifest::GetFileName(const FString& DirectoryName, bool bIsGameFolder)
{
	FString AppExecutableName = FPlatformProcess::ExecutableName();
#if PLATFORM_WINDOWS
	if (AppExecutableName.EndsWith(TEXT("-Cmd")))
	{
		AppExecutableName = AppExecutableName.Left(AppExecutableName.Len() - 4);
	}
#endif
	FString FileName = DirectoryName / AppExecutableName;
	if(FApp::GetBuildConfiguration() == EBuildConfigurations::DebugGame && bIsGameFolder)
	{
		FileName += FString::Printf(TEXT("-%s-DebugGame"), FPlatformProcess::GetBinariesSubdirectory());
	}
	return FileName + TEXT(".modules");
}

bool FModuleManifest::TryRead(const FString& FileName, FModuleManifest& Manifest)
{
	// Read the file to a string
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FileName))
	{
		return false;
	}

	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > ObjectPtr;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid() )
	{
		return false;
	}
	FJsonObject& Object = *ObjectPtr.Get();

	// Read the changelist and build id
	if(!Object.TryGetStringField(TEXT("BuildId"), Manifest.BuildId))
	{
		return false;
	}

	// Read the module mappings
	TSharedPtr<FJsonObject> ModulesObject = Object.GetObjectField(TEXT("Modules"));
	if(ModulesObject.IsValid())
	{
		for(TPair<FString, TSharedPtr<FJsonValue>>& Pair: ModulesObject->Values)
		{
			if(Pair.Value->Type == EJson::String)
			{
				Manifest.ModuleNameToFileName.FindOrAdd(Pair.Key) = Pair.Value->AsString();
			}
		}
	}

	return true;
}





FModuleEnumerator::FModuleEnumerator(const FString& InBuildId)
	: BuildId(InBuildId)
{
}

bool FModuleEnumerator::RegisterWithModuleManager()
{
	FModuleManager::Get().QueryModulesDelegate.BindRaw(this, &FModuleEnumerator::QueryModules);
	return true;
}

void FModuleEnumerator::QueryModules(const FString& InDirectoryName, bool bIsGameDirectory, TMap<FString, FString>& OutModules) const
{
	FModuleManifest Manifest;
	if(FModuleManifest::TryRead(FModuleManifest::GetFileName(InDirectoryName, bIsGameDirectory), Manifest) && Manifest.BuildId == BuildId)
	{
		OutModules = Manifest.ModuleNameToFileName;
	}
}
