// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/RedirectCollector.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogRedirectors, Log, All);

void FRedirectCollector::OnSoftObjectPathLoaded(const FSoftObjectPath& InPath)
{
	if (InPath.IsNull() || !GIsEditor)
	{
		// No need to track empty strings, or in standalone builds
		return;
	}

	FPackagePropertyPair ContainingPackageAndProperty;
	FSoftObjectPathThreadContext& ThreadContext = FSoftObjectPathThreadContext::Get();

	FName PackageName, PropertyName;
	ESoftObjectPathCollectType CollectType = ESoftObjectPathCollectType::AlwaysCollect;

	ThreadContext.GetSerializationOptions(PackageName, PropertyName, CollectType);

	if (CollectType == ESoftObjectPathCollectType::NeverCollect)
	{
		// Do not track
		return;
	}

	if (PackageName != NAME_None)
	{
		ContainingPackageAndProperty.SetPackage(PackageName);
		if (PropertyName != NAME_None)
		{
			ContainingPackageAndProperty.SetProperty(PropertyName);
		}
		ContainingPackageAndProperty.SetReferencedByEditorOnlyProperty(CollectType == ESoftObjectPathCollectType::EditorOnlyCollect);
	}

	FScopeLock ScopeLock(&CriticalSection);

	SoftObjectPathMap.AddUnique(InPath.GetAssetPathName(), ContainingPackageAndProperty);
}

void FRedirectCollector::OnStringAssetReferenceLoaded(const FString& InString)
{
	FSoftObjectPath Path(InString);
	OnSoftObjectPathLoaded(Path);
}

FString FRedirectCollector::OnStringAssetReferenceSaved(const FString& InString)
{
	FScopeLock ScopeLock(&CriticalSection);

	FName Found = GetAssetPathRedirection(FName(*InString));

	if (Found != NAME_None)
	{
		return Found.ToString();
	}
	return InString;
}

void FRedirectCollector::ResolveAllSoftObjectPaths(FName FilterPackage)
{	
	FScopeLock ScopeLock(&CriticalSection);

	TMultiMap<FName, FPackagePropertyPair> SkippedReferences;
	SkippedReferences.Empty(SoftObjectPathMap.Num());
	while (SoftObjectPathMap.Num())
	{
		TMultiMap<FName, FPackagePropertyPair> CurrentReferences;
		Swap(SoftObjectPathMap, CurrentReferences);

		for (const TPair<FName, FPackagePropertyPair>& CurrentReference : CurrentReferences)
		{
			const FName& ToLoadFName = CurrentReference.Key;
			const FPackagePropertyPair& RefFilenameAndProperty = CurrentReference.Value;

			if ((FilterPackage != NAME_None) && // not using a filter
				(FilterPackage != RefFilenameAndProperty.GetCachedPackageName()) && // this is the package we are looking for
				(RefFilenameAndProperty.GetCachedPackageName() != NAME_None) // if we have an empty package name then process it straight away
				)
			{
				// If we have a valid filter and it doesn't match, skip this reference
				SkippedReferences.Add(ToLoadFName, RefFilenameAndProperty);
				continue;
			}

			const FString ToLoad = ToLoadFName.ToString();

			if (ToLoad.Len() > 0 )
			{
				UE_LOG(LogRedirectors, Verbose, TEXT("String Asset Reference '%s'"), *ToLoad);
				UE_CLOG(RefFilenameAndProperty.GetProperty().ToString().Len(), LogRedirectors, Verbose, TEXT("    Referenced by '%s'"), *RefFilenameAndProperty.GetProperty().ToString());

				int32 DotIndex = ToLoad.Find(TEXT("."));
				FString PackageName = DotIndex != INDEX_NONE ? ToLoad.Left(DotIndex) : ToLoad;

				// If is known missing don't try
				if (FLinkerLoad::IsKnownMissingPackage(FName(*PackageName)))
				{
					continue;
				}

				UObject *Loaded = LoadObject<UObject>(NULL, *ToLoad, NULL, RefFilenameAndProperty.GetReferencedByEditorOnlyProperty() ? LOAD_EditorOnly | LOAD_NoWarn : LOAD_NoWarn, NULL);

				if (Loaded)
				{
					FString Dest = Loaded->GetPathName();
					UE_LOG(LogRedirectors, Verbose, TEXT("    Resolved to '%s'"), *Dest);
					if (Dest != ToLoad)
					{
						AssetPathRedirectionMap.Add(ToLoadFName, FName(*Dest));
					}
				}
				else
				{
					const FString Referencer = RefFilenameAndProperty.GetProperty().ToString().Len() ? RefFilenameAndProperty.GetProperty().ToString() : TEXT("Unknown");
					UE_LOG(LogRedirectors, Warning, TEXT("String Asset Reference '%s' was not found! (Referencer '%s')"), *ToLoad, *Referencer);
				}
			}
		}
	}

	check(SoftObjectPathMap.Num() == 0);
	// Add any skipped references back into the map for the next time this is called
	Swap(SoftObjectPathMap, SkippedReferences);
	// we shouldn't have any references left if we decided to resolve them all
	check((SoftObjectPathMap.Num() == 0) || (FilterPackage != NAME_None));
}

void FRedirectCollector::ProcessSoftObjectPathPackageList(FName FilterPackage, bool bGetEditorOnly, TSet<FName>& OutReferencedPackages)
{
	FScopeLock ScopeLock(&CriticalSection);

	// Iterate map, remove all matching and potentially add to OutReferencedPackages
	for (auto It = SoftObjectPathMap.CreateIterator(); It; ++It)
	{
		const FName& ToLoadFName = It->Key;
		const FPackagePropertyPair& RefFilenameAndProperty = It->Value;

		// Package name may be null, if so return the set of things not associated with a package
		if (FilterPackage == RefFilenameAndProperty.GetCachedPackageName())
		{
			FString PackageNameString = FPackageName::ObjectPathToPackageName(ToLoadFName.ToString());

			if (!RefFilenameAndProperty.GetReferencedByEditorOnlyProperty() || bGetEditorOnly)
			{
				OutReferencedPackages.Add(FName(*PackageNameString));
			}

			It.RemoveCurrent();
		}
	}

	SoftObjectPathMap.Compact();
}

void FRedirectCollector::AddAssetPathRedirection(FName OriginalPath, FName RedirectedPath)
{
	FScopeLock ScopeLock(&CriticalSection);

	// This replaces an existing mapping, can happen in the editor if things are renamed twice
	AssetPathRedirectionMap.Add(OriginalPath, RedirectedPath);
}

void FRedirectCollector::RemoveAssetPathRedirection(FName OriginalPath)
{
	FScopeLock ScopeLock(&CriticalSection);

	FName* Found = AssetPathRedirectionMap.Find(OriginalPath);

	if (ensureMsgf(Found, TEXT("Cannot remove redirection from %s, it was not registered"), *OriginalPath.ToString()))
	{
		AssetPathRedirectionMap.Remove(OriginalPath);
	}
}

FName FRedirectCollector::GetAssetPathRedirection(FName OriginalPath)
{
	FScopeLock ScopeLock(&CriticalSection);

	return AssetPathRedirectionMap.FindRef(OriginalPath);
}

FRedirectCollector GRedirectCollector;

#endif // WITH_EDITOR
