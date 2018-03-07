// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RedirectCollector:  Editor-only global object that handles resolving redirectors and handling string asset cooking rules
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"

#if WITH_EDITOR

class COREUOBJECT_API FRedirectCollector
{
private:
	
	/** Helper struct for string asset reference tracking */
	struct FPackagePropertyPair
	{
		FPackagePropertyPair() : bReferencedByEditorOnlyProperty(false) {}

		FPackagePropertyPair(const FName& InPackage, const FName& InProperty)
		: Package(InPackage)
		, Property(InProperty)
		, bReferencedByEditorOnlyProperty(false)
		{}

		bool operator==(const FPackagePropertyPair& Other) const
		{
			return Package == Other.Package &&
				Property == Other.Property;
		}

		const FName& GetCachedPackageName() const
		{
			return Package;
		}

		void SetPackage(const FName& InPackage)
		{
			Package = InPackage;
		}
		void SetProperty(const FName& InProperty)
		{
			Property = InProperty;
		}
		
		void SetReferencedByEditorOnlyProperty(bool InReferencedByEditorOnlyProperty)
		{
			bReferencedByEditorOnlyProperty = InReferencedByEditorOnlyProperty;
		}

		const FName& GetPackage() const
		{
			return Package;
		}
		const FName& GetProperty() const
		{
			return Property;
		}

		bool GetReferencedByEditorOnlyProperty() const
		{
			return bReferencedByEditorOnlyProperty;
		}

	private:
		FName Package;
		FName Property;
		bool bReferencedByEditorOnlyProperty;
	};

public:

	/**
	 * Called from FSoftObjectPath::PostLoadPath, registers this for later querying
	 * @param InPath The soft object path that was loaded
	 */
	void OnSoftObjectPathLoaded(const struct FSoftObjectPath& InPath);

	/**
	 * Load all soft object paths to resolve them, add that to the remap table, and empty the array
	 * @param FilterPackage If set, only load references that were created by FilterPackage. If empty, resolve  all of them
	 */
	void ResolveAllSoftObjectPaths(FName FilterPackage = NAME_None);

	/**
	 * Returns the list of packages referenced by soft object paths loaded by FilterPackage, and remove them from the internal list
	 * @param FilterPackage Return references made by loading this package. If passed null will return all references made with no explicit package
	 * @param bGetEditorOnly If true will return references loaded by editor only objects, if false it will not
	 * @param OutReferencedPackages Return list of packages referenced by FilterPackage
	 */
	void ProcessSoftObjectPathPackageList(FName FilterPackage, bool bGetEditorOnly, TSet<FName>& OutReferencedPackages);

	/** Adds a new mapping for redirector path to destination path, this is called from the Asset Registry to register all redirects it knows about */
	void AddAssetPathRedirection(FName OriginalPath, FName RedirectedPath);

	/** Removes an asset path redirection, call this when deleting redirectors */
	void RemoveAssetPathRedirection(FName OriginalPath);

	/** Returns a remapped asset path, if it returns null there is no relevant redirector */
	FName GetAssetPathRedirection(FName OriginalPath);

	/**
	 * Do we have any references to resolve.
	 * @return true if we have references to resolve
	 */
	bool HasAnySoftObjectPathsToResolve() const
	{
		return SoftObjectPathMap.Num() > 0;
	}

	DEPRECATED(4.17, "OnStringAssetReferenceSaved is deprecated, call GetAssetPathRedirection")
	FString OnStringAssetReferenceSaved(const FString& InString);

	DEPRECATED(4.18, "OnStringAssetReferenceLoaded is deprecated, call OnSoftObjectPathLoaded")
	void OnStringAssetReferenceLoaded(const FString& InString);

	DEPRECATED(4.18, "ResolveStringAssetReference is deprecated, call ResolveAllSoftObjectPaths")
	void ResolveStringAssetReference(FName FilterPackage = NAME_None, bool bProcessAlreadyResolvedPackages = true)
	{
		ResolveAllSoftObjectPaths(FilterPackage);
	}

private:

	/** A map of assets referenced by soft object paths, with the key being the asset being referenced and the value equal to the package with the reference */
	TMultiMap<FName, FPackagePropertyPair> SoftObjectPathMap;

	/** When saving, apply this remapping to all string asset references */
	TMap<FName, FName> AssetPathRedirectionMap;

	/** For SoftObjectPackageMap map */
	FCriticalSection CriticalSection;
};

// global redirect collector callback structure
COREUOBJECT_API extern FRedirectCollector GRedirectCollector;

#endif // WITH_EDITOR