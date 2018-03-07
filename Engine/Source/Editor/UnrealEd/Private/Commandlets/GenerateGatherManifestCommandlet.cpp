// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GenerateGatherManifestCommandlet.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateManifestCommandlet, Log, All);

/**
 *	UGenerateGatherManifestCommandlet
 */
UGenerateGatherManifestCommandlet::UGenerateGatherManifestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGenerateGatherManifestCommandlet::Main( const FString& Params )
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine( *Params, Tokens, Switches, ParamVals );

	// Set config file.
	const FString* ParamVal = ParamVals.Find( FString( TEXT("Config") ) );
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No config specified.") );
		return -1;
	}

	// Set config section.
	ParamVal = ParamVals.Find( FString( TEXT("Section") ) );
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No config section specified.") );
		return -1;
	}

	// Get destination path.
	FString DestinationPath;
	if( !GetPathFromConfig( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) )
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No destination path specified.") );
		return -1;
	}

	// Get manifest name.
	FString ManifestName;
	if( !GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) )
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No manifest name specified.") );
		return -1;
	}

	//Grab any manifest dependencies
	TArray<FString> ManifestDependenciesList;
	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	for (const FString& ManifestDependency : ManifestDependenciesList)
	{
		FText OutError;
		if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
		{
			UE_LOG(LogGenerateManifestCommandlet, Error, TEXT("The GenerateGatherManifest commandlet couldn't load the specified manifest dependency: '%'. %s"), *ManifestDependency, *OutError.ToString());
			return -1;
		}
	}

	// Trim the manifest to remove any entries that came from a dependency
	GatherManifestHelper->TrimManifest();
	
	const FString ManifestPath = FPaths::ConvertRelativePathToFull(DestinationPath) / ManifestName;
	FText ManifestSaveError;
	if (!GatherManifestHelper->SaveManifest(ManifestPath, &ManifestSaveError))
	{
		UE_LOG(LogGenerateManifestCommandlet, Error,TEXT("Failed to write manifest to %s. %s."), *ManifestPath, *ManifestSaveError.ToString());
		return -1;
	}

	return 0;
}
