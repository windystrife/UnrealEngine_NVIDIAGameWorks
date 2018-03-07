// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"

FBuildVersion::FBuildVersion()
	: MajorVersion(0)
	, MinorVersion(0)
	, PatchVersion(0)
	, Changelist(0)
	, CompatibleChangelist(0)
	, IsLicenseeVersion(0)
	, IsPromotedBuild(0)
{
}

FString FBuildVersion::GetDefaultFileName()
{
	return FPaths::EngineDir() / TEXT("Build/Build.version");
}

FString FBuildVersion::GetFileNameForCurrentExecutable()
{
	FString AppExecutableName = FPlatformProcess::ExecutableName();
#if PLATFORM_WINDOWS
	if(AppExecutableName.EndsWith(TEXT("-Cmd")))
	{
		AppExecutableName = AppExecutableName.Left(AppExecutableName.Len() - 4);
	}
#endif
#if IS_PROGRAM || IS_MONOLITHIC
	return FPaths::ProjectDir() / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory() / AppExecutableName + TEXT(".version");
#else
	return FPaths::EngineDir() / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory() / AppExecutableName + TEXT(".version");
#endif
}

bool FBuildVersion::TryRead(const FString& FileName, FBuildVersion& OutVersion)
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

	// Parse the object
	FJsonObject& Object = *ObjectPtr.Get();
	return TryParse(Object, OutVersion);
}

bool FBuildVersion::TryParse(const FJsonObject& Object, FBuildVersion& OutVersion)
{
	// Read the engine version information
	if(!Object.TryGetNumberField(TEXT("MajorVersion"), OutVersion.MajorVersion) || !Object.TryGetNumberField(TEXT("MinorVersion"), OutVersion.MinorVersion) || !Object.TryGetNumberField(TEXT("PatchVersion"), OutVersion.PatchVersion))
	{
		return false;
	}

	Object.TryGetNumberField(TEXT("Changelist"), OutVersion.Changelist);
	Object.TryGetNumberField(TEXT("CompatibleChangelist"), OutVersion.CompatibleChangelist);
	Object.TryGetNumberField(TEXT("IsLicenseeVersion"), OutVersion.IsLicenseeVersion);
	Object.TryGetNumberField(TEXT("IsPromotedBuild"), OutVersion.IsPromotedBuild);
	Object.TryGetStringField(TEXT("BranchName"), OutVersion.BranchName);
	Object.TryGetStringField(TEXT("BuildId"), OutVersion.BuildId);

	return true;
}

