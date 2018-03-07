// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextCommandletBase.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "EngineGlobals.h"
#include "AssetData.h"
#include "Editor.h"
#include "IAssetRegistry.h"
#include "ARFilter.h"
#include "PackageHelperFunctions.h"
#include "ObjectTools.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextCommandletBase, Log, All);

//////////////////////////////////////////////////////////////////////////
//UGatherTextCommandletBase

UGatherTextCommandletBase::UGatherTextCommandletBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGatherTextCommandletBase::Initialize( const TSharedPtr< FLocTextHelper >& InGatherManifestHelper, const TSharedPtr< FLocalizationSCC >& InSourceControlInfo )
{
	GatherManifestHelper = InGatherManifestHelper;
	SourceControlInfo = InSourceControlInfo;
}

void UGatherTextCommandletBase::CreateCustomEngine(const FString& Params)
{
	GEngine = GEditor = NULL;//Force a basic default engine. 
}

bool UGatherTextCommandletBase::GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename )
{
	bool bSuccess = GConfig->GetBool( Section, Key, OutValue, Filename );
	
	if( !bSuccess )
	{
		bSuccess = GConfig->GetBool( TEXT("CommonSettings"), Key, OutValue, Filename );
	}
	return bSuccess;
}

bool UGatherTextCommandletBase::GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename )
{
	bool bSuccess = GConfig->GetString( Section, Key, OutValue, Filename );

	if( !bSuccess )
	{
		bSuccess = GConfig->GetString( TEXT("CommonSettings"), Key, OutValue, Filename );
	}
	return bSuccess;
}

bool UGatherTextCommandletBase::GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename )
{
	bool bSuccess = GetStringFromConfig( Section, Key, OutValue, Filename );

	if( bSuccess )
	{
		if (FPaths::IsRelative(OutValue))
		{
			if (!FPaths::ProjectDir().IsEmpty())
			{
				OutValue = FPaths::Combine( *( FPaths::ProjectDir() ), *OutValue );
			}
			else
			{
				OutValue = FPaths::Combine( *( FPaths::EngineDir() ), *OutValue );
			}
		}
	}
	return bSuccess;
}

int32 UGatherTextCommandletBase::GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename )
{
	int32 count = GConfig->GetArray( Section, Key, OutArr, Filename );

	if( count == 0 )
	{
		count = GConfig->GetArray( TEXT("CommonSettings"), Key, OutArr, Filename );
	}
	return count;
}

int32 UGatherTextCommandletBase::GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename )
{
	int32 count = GetStringArrayFromConfig( Section, Key, OutArr, Filename );

	for (int32 i = 0; i < count; ++i)
	{
		if (FPaths::IsRelative(OutArr[i]))
		{
			const FString ProjectBasePath = FPaths::ProjectDir().IsEmpty() ? FPaths::EngineDir() : FPaths::ProjectDir();
			OutArr[i] = FPaths::Combine( *ProjectBasePath, *OutArr[i] );
			OutArr[i] = FPaths::ConvertRelativePathToFull(OutArr[i]);
		}
		FPaths::CollapseRelativeDirectories(OutArr[i]);
	}
	return count;
}


FFuzzyPathMatcher::FFuzzyPathMatcher(const TArray<FString>& InIncludePathFilters, const TArray<FString>& InExcludePathFilters)
{
	FuzzyPaths.Reserve(InIncludePathFilters.Num() + InExcludePathFilters.Num());

	for (const FString& IncludePath : InIncludePathFilters)
	{
		FuzzyPaths.Add(FFuzzyPath(IncludePath, EPathType::Include));
	}

	for (const FString& ExcludePath : InExcludePathFilters)
	{
		FuzzyPaths.Add(FFuzzyPath(ExcludePath, EPathType::Exclude));
	}

	// Sort the paths so that deeper paths with fewer wildcards appear first in the list
	FuzzyPaths.Sort([](const FFuzzyPath& PathOne, const FFuzzyPath& PathTwo) -> bool
	{
		auto GetFuzzRating = [](const FFuzzyPath& InFuzzyPath) -> int32
		{
			int32 PathDepth = 0;
			int32 PathFuzz = 0;
			for (const TCHAR Char : InFuzzyPath.PathFilter)
			{
				if (Char == TEXT('/') || Char == TEXT('\\'))
				{
					++PathDepth;
				}
				else if (Char == TEXT('*') || Char == TEXT('?'))
				{
					++PathFuzz;
				}
			}

			return (100 - PathDepth) + (PathFuzz * 1000);
		};

		const int32 PathOneFuzzRating = GetFuzzRating(PathOne);
		const int32 PathTwoFuzzRating = GetFuzzRating(PathTwo);
		return PathOneFuzzRating < PathTwoFuzzRating;
	});
}

FFuzzyPathMatcher::EPathMatch FFuzzyPathMatcher::TestPath(const FString& InPathToTest) const
{
	for (const FFuzzyPath& FuzzyPath : FuzzyPaths)
	{
		if (InPathToTest.MatchesWildcard(FuzzyPath.PathFilter))
		{
			return (FuzzyPath.PathType == EPathType::Include) ? EPathMatch::Included : EPathMatch::Excluded;
		}
	}

	return EPathMatch::NoMatch;
}
