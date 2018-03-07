// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryState.h"
#include "AssetRegistry.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/ArrayReader.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/MetaData.h"
#include "AssetRegistryPrivate.h"
#include "ARFilter.h"
#include "DependsNode.h"
#include "PackageReader.h"
#include "NameTableArchive.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"

FAssetRegistryState::FAssetRegistryState()
{
	NumAssets = 0;
	NumDependsNodes = 0;
	NumPackageData = 0;
}

FAssetRegistryState::~FAssetRegistryState()
{
	Reset();
}

void FAssetRegistryState::Reset()
{
	// if we have preallocated all the FAssetData's in a single block, free it now, instead of one at a time
	if (PreallocatedAssetDataBuffers.Num())
	{
		for (FAssetData* Buffer : PreallocatedAssetDataBuffers)
		{
			delete[] Buffer;
		}
		PreallocatedAssetDataBuffers.Reset();

		NumAssets = 0;
	}
	else
	{
		// Delete all assets in the cache
		for (TMap<FName, FAssetData*>::TConstIterator AssetDataIt(CachedAssetsByObjectPath); AssetDataIt; ++AssetDataIt)
		{
			if (AssetDataIt.Value())
			{
				delete AssetDataIt.Value();
				NumAssets--;
			}
		}
	}

	// Make sure we have deleted all our allocated FAssetData objects
	ensure(NumAssets == 0);

	if (PreallocatedDependsNodeDataBuffers.Num())
	{
		for (FDependsNode* Buffer : PreallocatedDependsNodeDataBuffers)
		{
			delete[] Buffer;
		}
		PreallocatedDependsNodeDataBuffers.Reset();
		NumDependsNodes = 0;
	}
	else
	{
		// Delete all depends nodes in the cache
		for (TMap<FAssetIdentifier, FDependsNode*>::TConstIterator DependsIt(CachedDependsNodes); DependsIt; ++DependsIt)
		{
			if (DependsIt.Value())
			{
				delete DependsIt.Value();
				NumDependsNodes--;
			}
		}
	}

	// Make sure we have deleted all our allocated FDependsNode objects
	ensure(NumDependsNodes == 0);

	if (PreallocatedPackageDataBuffers.Num())
	{
		for (FAssetPackageData* Buffer : PreallocatedPackageDataBuffers)
		{
			delete[] Buffer;
		}
		PreallocatedPackageDataBuffers.Reset();
		NumPackageData = 0;
	}
	else
	{
		// Delete all depends nodes in the cache
		for (TMap<FName, FAssetPackageData*>::TConstIterator PackageDataIt(CachedPackageData); PackageDataIt; ++PackageDataIt)
		{
			if (PackageDataIt.Value())
			{
				delete PackageDataIt.Value();
				NumPackageData--;
			}
		}
	}

	// Make sure we have deleted all our allocated package data objects
	ensure(NumPackageData == 0);

	// Clear cache
	CachedAssetsByObjectPath.Empty();
	CachedAssetsByPackageName.Empty();
	CachedAssetsByPath.Empty();
	CachedAssetsByClass.Empty();
	CachedAssetsByTag.Empty();
	CachedDependsNodes.Empty();
	CachedPackageData.Empty();
}

void FAssetRegistryState::InitializeFromExisting(const TMap<FName, FAssetData*>& AssetDataMap, const TMap<FAssetIdentifier, FDependsNode*>& DependsNodeMap, const TMap<FName, FAssetPackageData*>& AssetPackageDataMap, const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting)
{
	if (!bRefreshExisting)
	{
		Reset();
	}

	for (const TPair<FName, FAssetData*>& Pair : AssetDataMap)
	{
		FAssetData* ExistingData = nullptr;

		if (bRefreshExisting)
		{
			ExistingData = CachedAssetsByObjectPath.FindRef(Pair.Key);
			if (!ExistingData)
			{
				continue;
			}
		}

		if (Pair.Value)
		{
			// Filter asset registry tags now
			const FAssetData& AssetData = *Pair.Value;

			static FName WildcardName(TEXT("*"));
			const TSet<FName>* AllClassesFilterlist = Options.CookFilterlistTagsByClass.Find(WildcardName);
			const TSet<FName>* ClassSpecificFilterlist = Options.CookFilterlistTagsByClass.Find(AssetData.AssetClass);

			// Exclude blacklisted tags or include only whitelisted tags, based on how we were configured in ini
			FAssetDataTagMap LocalTagsAndValues;
			for (const TPair<FName, FString>& TagPair : AssetData.TagsAndValues)
			{
				const bool bInAllClasseslist = AllClassesFilterlist && (AllClassesFilterlist->Contains(TagPair.Key) || AllClassesFilterlist->Contains(WildcardName));
				const bool bInClassSpecificlist = ClassSpecificFilterlist && (ClassSpecificFilterlist->Contains(TagPair.Key) || ClassSpecificFilterlist->Contains(WildcardName));
				if (Options.bUseAssetRegistryTagsWhitelistInsteadOfBlacklist)
				{
					// It's a whitelist, only include it if it is in the all classes list or in the class specific list
					if (bInAllClasseslist || bInClassSpecificlist)
					{
						// It is in the whitelist. Keep it.
						LocalTagsAndValues.Add(TagPair.Key, TagPair.Value);
					}
				}
				else
				{
					// It's a blacklist, include it unless it is in the all classes list or in the class specific list
					if (!bInAllClasseslist && !bInClassSpecificlist)
					{
						// It isn't in the blacklist. Keep it.
						LocalTagsAndValues.Add(TagPair.Key, TagPair.Value);
					}
				}
			}

			if (bRefreshExisting)
			{
				// Only modify tags
				if (LocalTagsAndValues != ExistingData->TagsAndValues.GetMap())
				{
					FAssetData TempData = *ExistingData;
					TempData.TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(LocalTagsAndValues));
					UpdateAssetData(ExistingData, TempData);
				}
			}
			else
			{
				FAssetData* NewData = new FAssetData(AssetData.PackageName, AssetData.PackagePath, AssetData.AssetName,
					AssetData.AssetClass, LocalTagsAndValues, AssetData.ChunkIDs, AssetData.PackageFlags);
				AddAssetData(NewData);
			}
		}
	}

	TSet<FAssetIdentifier> ScriptPackages;

	if (!bRefreshExisting)
	{
		for (const TPair<FName, FAssetPackageData*>& Pair : AssetPackageDataMap)
		{
			bool bIsScriptPackage = FPackageName::IsScriptPackage(Pair.Key.ToString());

			if (Pair.Value)
			{
				// Only add if also in asset data map, or script package
				if (bIsScriptPackage)
				{
					ScriptPackages.Add(Pair.Key);

					FAssetPackageData* NewData = CreateOrGetAssetPackageData(Pair.Key);
					*NewData = *Pair.Value;
				}
				else if (CachedAssetsByPackageName.Find(Pair.Key) || bIsScriptPackage)
				{
					FAssetPackageData* NewData = CreateOrGetAssetPackageData(Pair.Key);
					*NewData = *Pair.Value;
				}
			}
		}

		for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : DependsNodeMap)
		{
			FDependsNode* OldNode = Pair.Value;
			FDependsNode* NewNode = CreateOrFindDependsNode(Pair.Key);

			Pair.Value->IterateOverDependencies([this, &DependsNodeMap, &ScriptPackages, OldNode, NewNode](FDependsNode* InDependency, EAssetRegistryDependencyType::Type InDependencyType) {
				const FAssetIdentifier& Identifier = InDependency->GetIdentifier();
				if (DependsNodeMap.Find(Identifier) || ScriptPackages.Contains(Identifier))
				{
					// Only add if this node is in the incoming map
					FDependsNode* NewDependency = CreateOrFindDependsNode(Identifier);
					NewNode->AddDependency(NewDependency, InDependencyType, true);
					NewDependency->AddReferencer(NewNode);
				}
			});
		}
	}
}

void FAssetRegistryState::PruneAssetData(const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, bool bFilterAssetDataWithNoTags)
{
	// Generate list up front as the maps will get cleaned up
	TArray<FAssetData*> AllAssetData;
	CachedAssetsByObjectPath.GenerateValueArray(AllAssetData);

	for (FAssetData* AssetData : AllAssetData)
	{
		if (RequiredPackages.Num() > 0 && !RequiredPackages.Contains(AssetData->PackageName))
		{
			RemoveAssetData(AssetData);
		}
		else if (RemovePackages.Contains(AssetData->PackageName))
		{
			RemoveAssetData(AssetData);
		}
		else if (bFilterAssetDataWithNoTags && AssetData->TagsAndValues.Num() == 0)
		{
			RemoveAssetData(AssetData);
		}
	}

	// Remove any orphaned depends nodes. This will leave cycles in but those might represent useful data
	TArray<FDependsNode*> AllDependsNodes;
	CachedDependsNodes.GenerateValueArray(AllDependsNodes);

	for (FDependsNode* DependsNode : AllDependsNodes)
	{
		if (DependsNode->GetConnectionCount() == 0 && !DependsNode->GetIdentifier().IsPackage())
		{
			RemoveDependsNode(DependsNode->GetIdentifier());
		}
	}
}

bool FAssetRegistryState::HasAssets(const FName PackagePath) const
{
	const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByPath.Find(PackagePath);
	return FoundAssetArray && FoundAssetArray->Num() > 0;
}

bool FAssetRegistryState::GetAssets(const FARFilter& Filter, const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData) const
{
	// Verify filter input. If all assets are needed, use GetAllAssets() instead.
	if (!IsFilterValid(Filter, false) || Filter.IsEmpty())
	{
		return false;
	}

	// Prepare a set of each filter component for fast searching
	TSet<FName> FilterPackageNames(Filter.PackageNames);
	TSet<FName> FilterPackagePaths(Filter.PackagePaths);
	TSet<FName> FilterClassNames(Filter.ClassNames);
	TSet<FName> FilterObjectPaths(Filter.ObjectPaths);

	// Form a set of assets matched by each filter
	TArray<TArray<FAssetData*> > DiskFilterSets;

	// On disk package names
	if (FilterPackageNames.Num())
	{
		TArray<FAssetData*>& PackageNameFilter = DiskFilterSets[DiskFilterSets.AddDefaulted()];

		for (FName PackageName : FilterPackageNames)
		{
			const TArray<FAssetData*>* PackageAssets = CachedAssetsByPackageName.Find(PackageName);

			if (PackageAssets != nullptr)
			{
				PackageNameFilter.Append(*PackageAssets);
			}
		}
	}

	// On disk package paths
	if (FilterPackagePaths.Num())
	{
		TArray<FAssetData*>& PathFilter = DiskFilterSets[DiskFilterSets.AddDefaulted()];

		for (FName PackagePath : FilterPackagePaths)
		{
			const TArray<FAssetData*>* PathAssets = CachedAssetsByPath.Find(PackagePath);

			if (PathAssets != nullptr)
			{
				PathFilter.Append(*PathAssets);
			}
		}
	}

	// On disk classes
	if (FilterClassNames.Num())
	{
		TArray<FAssetData*>& ClassFilter = DiskFilterSets[DiskFilterSets.AddDefaulted()];

		for (FName ClassName : FilterClassNames)
		{
			const TArray<FAssetData*>* ClassAssets = CachedAssetsByClass.Find(ClassName);

			if (ClassAssets != nullptr)
			{
				ClassFilter.Append(*ClassAssets);
			}
		}
	}

	// On disk object paths
	if (FilterObjectPaths.Num())
	{
		TArray<FAssetData*>& ObjectPathsFilter = DiskFilterSets[DiskFilterSets.AddDefaulted()];

		for (FName ObjectPath : FilterObjectPaths)
		{
			FAssetData* AssetDataPtr = CachedAssetsByObjectPath.FindRef(ObjectPath);

			if (AssetDataPtr != nullptr)
			{
				ObjectPathsFilter.Add(AssetDataPtr);
			}
		}
	}

	// On disk tags and values
	if (Filter.TagsAndValues.Num())
	{
		TArray<FAssetData*>& TagAndValuesFilter = DiskFilterSets[DiskFilterSets.AddDefaulted()];

		for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
		{
			const FName Tag = FilterTagIt.Key();
			const FString& Value = FilterTagIt.Value();

			const TArray<FAssetData*>* TagAssets = CachedAssetsByTag.Find(Tag);

			if (TagAssets != nullptr)
			{
				for (FAssetData* AssetData : *TagAssets)
				{
					if (AssetData != nullptr)
					{
						const FString* TagValue = AssetData->TagsAndValues.Find(Tag);
						if (TagValue != nullptr && *TagValue == Value)
						{
							TagAndValuesFilter.Add(AssetData);
						}
					}
				}
			}
		}
	}

	// If we have any filter sets, add the assets which are contained in the sets to OutAssetData
	if (DiskFilterSets.Num() > 0)
	{
		// Initialize the combined filter set to the first set, in case we can skip combining.
		TArray<FAssetData*>* CombinedFilterSet = DiskFilterSets.GetData();
		TArray<FAssetData*> Intersection;

		// If we have more than one set, we must combine them. We take the intersection
		if (DiskFilterSets.Num() > 1)
		{
			// Sort each set for the intersection algorithm
			struct FCompareFAssetData
			{
				FORCEINLINE bool operator()(const FAssetData& A, const FAssetData& B) const { return A.ObjectPath.Compare(B.ObjectPath) < 0; }
			};

			for (TArray<FAssetData*>& DiskFilter : DiskFilterSets)
			{
				DiskFilter.Sort(FCompareFAssetData());
			}

			// Set the "current" intersection set to the first filter set
			Intersection = DiskFilterSets[0];

			// Now iterate over every set beyond the first and intersect it with the current
			for (int32 SetIdx = 1; SetIdx < DiskFilterSets.Num(); ++SetIdx)
			{
				TArray<FAssetData*> NewIntersection;
				const TArray<FAssetData*>& SetA = Intersection;
				const TArray<FAssetData*>& SetB = DiskFilterSets[SetIdx];
				int32 AIdx = 0;
				int32 BIdx = 0;

				// Do intersection
				while (AIdx < SetA.Num() && BIdx < SetB.Num())
				{
					if (SetA[AIdx]->ObjectPath.Compare(SetB[BIdx]->ObjectPath) < 0)
						++AIdx;
					else if (SetB[BIdx]->ObjectPath.Compare(SetA[AIdx]->ObjectPath) < 0)
						++BIdx;
					else
					{
						NewIntersection.Add(SetA[AIdx]);
						AIdx++;
						BIdx++;
					}
				}

				// Update the "current" intersection with the results
				Intersection = NewIntersection;
			}

			// Set the CombinedFilterSet pointer to the full intersection of all sets
			CombinedFilterSet = &Intersection;
		}

		// Iterate over the final combined filter set to add to OutAssetData
		for (FAssetData* AssetData : *CombinedFilterSet)
		{
			if (PackageNamesToSkip.Contains(AssetData->PackageName))
			{
				// Skip assets in passed in package list
				continue;
			}

			OutAssetData.Add(*AssetData);
		}
	}

	return true;
}

bool FAssetRegistryState::GetAllAssets(const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData) const
{
	// All unloaded disk assets
	for (const TPair<FName, FAssetData*>& AssetDataPair : CachedAssetsByObjectPath)
	{
		const FAssetData* AssetData = AssetDataPair.Value;

		if (AssetData != nullptr)
		{
			// Make sure the asset's package was not loaded then the object was deleted/renamed
			if (!PackageNamesToSkip.Contains(AssetData->PackageName))
			{
				OutAssetData.Emplace(*AssetData);
			}
		}
	}
	return true;
}

bool FAssetRegistryState::GetDependencies(const FAssetIdentifier& AssetIdentifier,
										  TArray<FAssetIdentifier>& OutDependencies,
										  EAssetRegistryDependencyType::Type InDependencyType) const
{
	const FDependsNode* const* NodePtr = CachedDependsNodes.Find(AssetIdentifier);
	const FDependsNode* Node = nullptr;
	if (NodePtr != nullptr)
	{
		Node = *NodePtr;
	}

	if (Node != nullptr)
	{
		Node->GetDependencies(OutDependencies, InDependencyType);

		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistryState::GetReferencers(const FAssetIdentifier& AssetIdentifier,
										 TArray<FAssetIdentifier>& OutReferencers,
										 EAssetRegistryDependencyType::Type InReferenceType) const
{
	const FDependsNode* const* NodePtr = CachedDependsNodes.Find(AssetIdentifier);
	const FDependsNode* Node = nullptr;
	bool bShowingAllReferences = InReferenceType == EAssetRegistryDependencyType::All;

	if (NodePtr != nullptr)
	{
		Node = *NodePtr;
	}

	if (Node != nullptr)
	{
		TArray<FDependsNode*> DependencyNodes;
		Node->GetReferencers(DependencyNodes);

		for (FDependsNode* DependencyNode : DependencyNodes)
		{
			if (!bShowingAllReferences)
			{
				TArray<FDependsNode*> DependenciesFromReferencer;
				DependencyNode->GetDependencies(DependenciesFromReferencer, InReferenceType);

				for (FDependsNode* Dependency : DependenciesFromReferencer)
				{
					if (Dependency == Node)
					{
						OutReferencers.Add(DependencyNode->GetIdentifier());
						break;
					}
				}
			}
			else
			{
				OutReferencers.Add(DependencyNode->GetIdentifier());
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistryState::Serialize(FArchive& OriginalAr, FAssetRegistrySerializationOptions& Options)
{
	// This is only used for the runtime version of the AssetRegistry
	if (OriginalAr.IsSaving())
	{
		check(CachedAssetsByObjectPath.Num() == NumAssets);

		FAssetRegistryVersion::Type Version = FAssetRegistryVersion::LatestVersion;
		FAssetRegistryVersion::SerializeVersion(OriginalAr, Version);

		// Set up name table archive
		FNameTableArchiveWriter Ar(OriginalAr);

		// serialize number of objects
		int32 AssetCount = CachedAssetsByObjectPath.Num();
		Ar << AssetCount;

		TArray<FDependsNode*> Dependencies;
		TMap<FAssetIdentifier, int32> DependsIndexMap;
		DependsIndexMap.Reserve(CachedAssetsByObjectPath.Num());

		// Write asset data first
		for (TPair<FName, FAssetData*>& Pair : CachedAssetsByObjectPath)
		{
			FAssetData& AssetData(*Pair.Value);
			AssetData.SerializeForCache(Ar);
		}

		if (Options.bSerializeDependencies)
		{
			// Scan dependency nodes, we won't save all of them if we filter out certain types
			for (TPair<FAssetIdentifier, FDependsNode*>& Pair : CachedDependsNodes)
			{
				FDependsNode* Node = Pair.Value;

				if (Node->GetIdentifier().IsPackage() 
					|| (Options.bSerializeSearchableNameDependencies && Node->GetIdentifier().IsValue())
					|| (Options.bSerializeManageDependencies && Node->GetIdentifier().GetPrimaryAssetId().IsValid()))
				{
					DependsIndexMap.Add(Node->GetIdentifier(), Dependencies.Num());
					Dependencies.Add(Node);
				}
			}
		}

		int32 NumDependencies = Dependencies.Num();
		Ar << NumDependencies;

		TArray<FDependsNode*> ProcessedDependencies;
		TMap<EAssetRegistryDependencyType::Type, int32> DepCounts;
		TMap<FDependsNode*, FDependsNode*> RedirectCache;

		for (FDependsNode* DependentNode : Dependencies)
		{
			FAssetIdentifier Identifier = DependentNode->GetIdentifier();

			ProcessedDependencies.Empty();
			DepCounts.Empty();
			DepCounts.Add(EAssetRegistryDependencyType::Hard, 0);
			DepCounts.Add(EAssetRegistryDependencyType::Soft, 0);
			DepCounts.Add(EAssetRegistryDependencyType::SearchableName, 0);
			DepCounts.Add(EAssetRegistryDependencyType::Manage, 0);
			DepCounts.Add(EAssetRegistryDependencyType::None, 0); // Referencers

			auto DependencyProcessor = [&](FDependsNode* InDependency, EAssetRegistryDependencyType::Type InDependencyType)
			{
				FDependsNode* RedirectedDependency = ResolveRedirector(InDependency, CachedAssetsByObjectPath, RedirectCache);

				if (RedirectedDependency && DependsIndexMap.Contains(RedirectedDependency->GetIdentifier()))
				{
					ProcessedDependencies.Add(InDependency);
					DepCounts[InDependencyType]++;
				}
			};

			DependentNode->IterateOverDependencies(DependencyProcessor, EAssetRegistryDependencyType::Hard);
			DependentNode->IterateOverDependencies(DependencyProcessor, EAssetRegistryDependencyType::Soft);

			if (Options.bSerializeSearchableNameDependencies)
			{
				DependentNode->IterateOverDependencies(DependencyProcessor, EAssetRegistryDependencyType::SearchableName);
			}
			if (Options.bSerializeManageDependencies)
			{
				DependentNode->IterateOverDependencies(DependencyProcessor, EAssetRegistryDependencyType::Manage);
			}

			DependentNode->IterateOverReferencers([&](FDependsNode* InReferencer) 
			{
				if (DependsIndexMap.Contains(InReferencer->GetIdentifier()))
				{
					ProcessedDependencies.Add(InReferencer);
					DepCounts[EAssetRegistryDependencyType::None]++;
				}
			});

			Ar << Identifier;
			Ar << DepCounts[EAssetRegistryDependencyType::Hard];
			Ar << DepCounts[EAssetRegistryDependencyType::Soft];
			Ar << DepCounts[EAssetRegistryDependencyType::SearchableName];
			Ar << DepCounts[EAssetRegistryDependencyType::Manage];
			Ar << DepCounts[EAssetRegistryDependencyType::None];

			for (FDependsNode* Dependency : ProcessedDependencies)
			{
				int32 Index = DependsIndexMap[Dependency->GetIdentifier()];
				Ar << Index;
			}
		}

		int32 PackageDataCount = 0;
		if (Options.bSerializePackageData)
		{
			PackageDataCount = CachedPackageData.Num();
			Ar << PackageDataCount;

			for (TPair<FName, FAssetPackageData*>& Pair : CachedPackageData)
			{
				Ar << Pair.Key;
				Pair.Value->SerializeForCache(Ar);
			}
		}
		else
		{
			Ar << PackageDataCount;
		}
	}
	// load in by building the TMap
	else
	{
		FAssetRegistryVersion::Type Version = FAssetRegistryVersion::LatestVersion;
		FAssetRegistryVersion::SerializeVersion(OriginalAr, Version);

		if (Version < FAssetRegistryVersion::RemovedMD5Hash)
		{
			// Cannot read states before this version
			return false;
		}

		// Set up name table archive
		FNameTableArchiveReader Ar(OriginalAr);

		// serialize number of objects
		int32 LocalNumAssets = 0;
		Ar << LocalNumAssets;

		// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
		FAssetData* PreallocatedAssetDataBuffer = new FAssetData[LocalNumAssets];
		PreallocatedAssetDataBuffers.Add(PreallocatedAssetDataBuffer);

		for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; AssetIndex++)
		{
			// make a new asset data object
			FAssetData* NewAssetData = &PreallocatedAssetDataBuffer[AssetIndex];

			// load it
			NewAssetData->SerializeForCache(Ar);

			AddAssetData(NewAssetData);
		}

		int32 LocalNumDependsNodes = 0;
		Ar << LocalNumDependsNodes;

		FDependsNode* PreallocatedDependsNodeDataBuffer = nullptr;
		if (Options.bSerializeDependencies && LocalNumDependsNodes > 0)
		{
			PreallocatedDependsNodeDataBuffer = new FDependsNode[LocalNumDependsNodes];
			PreallocatedDependsNodeDataBuffers.Add(PreallocatedDependsNodeDataBuffer);
			CachedDependsNodes.Reserve(LocalNumDependsNodes);		
		}

		TMap<EAssetRegistryDependencyType::Type, int32> DepCounts;

		for (int32 DependsNodeIndex = 0; DependsNodeIndex < LocalNumDependsNodes; DependsNodeIndex++)
		{
			FAssetIdentifier AssetIdentifier;
			Ar << AssetIdentifier;

			DepCounts.Empty();

			Ar << DepCounts.FindOrAdd(EAssetRegistryDependencyType::Hard);
			Ar << DepCounts.FindOrAdd(EAssetRegistryDependencyType::Soft);
			Ar << DepCounts.FindOrAdd(EAssetRegistryDependencyType::SearchableName);
			Ar << DepCounts.FindOrAdd(EAssetRegistryDependencyType::Manage);
			Ar << DepCounts.FindOrAdd(EAssetRegistryDependencyType::None); // Referencers
						
			// Create the node if we're actually saving dependencies, otherwise just fake serialize
			FDependsNode* NewDependsNodeData = (Options.bSerializeDependencies ? &PreallocatedDependsNodeDataBuffer[DependsNodeIndex] : nullptr);

			if (NewDependsNodeData)
			{
				NewDependsNodeData->SetIdentifier(AssetIdentifier);
				NewDependsNodeData->Reserve(DepCounts[EAssetRegistryDependencyType::Hard], DepCounts[EAssetRegistryDependencyType::Soft], DepCounts[EAssetRegistryDependencyType::SearchableName], DepCounts[EAssetRegistryDependencyType::Manage], DepCounts[EAssetRegistryDependencyType::None]);
				CachedDependsNodes.Add(NewDependsNodeData->GetIdentifier(), NewDependsNodeData);
			}

			auto SerializeDependencyType = [&](EAssetRegistryDependencyType::Type InDependencyType, bool bShouldAdd)
			{
				for (int32 DependencyIndex = 0; DependencyIndex < DepCounts[InDependencyType]; ++DependencyIndex)
				{
					int32 Index = 0;
					Ar << Index;

					if (Index < 0 || Index >= LocalNumDependsNodes)
					{
						Ar.SetError();
						return;
					}
					if (bShouldAdd)
					{
						if (InDependencyType == EAssetRegistryDependencyType::None)
						{
							NewDependsNodeData->AddReferencer(&PreallocatedDependsNodeDataBuffer[Index], true);
						}
						else
						{
							NewDependsNodeData->AddDependency(&PreallocatedDependsNodeDataBuffer[Index], InDependencyType, true);
						}
					}
				}
			};

			// Serialize each type, don't do anything if serializing that type isn't allowed
			SerializeDependencyType(EAssetRegistryDependencyType::Hard, Options.bSerializeDependencies);
			SerializeDependencyType(EAssetRegistryDependencyType::Soft, Options.bSerializeDependencies);
			SerializeDependencyType(EAssetRegistryDependencyType::SearchableName, Options.bSerializeDependencies && Options.bSerializeSearchableNameDependencies);
			SerializeDependencyType(EAssetRegistryDependencyType::Manage, Options.bSerializeDependencies && Options.bSerializeManageDependencies);
			SerializeDependencyType(EAssetRegistryDependencyType::None, Options.bSerializeDependencies);
		}

		int32 LocalNumPackageData = 0;
		Ar << LocalNumPackageData;
		FAssetPackageData* PreallocatedPackageDataBuffer = nullptr;
		if (Options.bSerializePackageData && LocalNumPackageData > 0)
		{
			PreallocatedPackageDataBuffer = new FAssetPackageData[LocalNumPackageData];
			PreallocatedPackageDataBuffers.Add(PreallocatedPackageDataBuffer);
			CachedPackageData.Reserve(LocalNumPackageData);
		}

		for (int32 PackageDataIndex = 0; PackageDataIndex < LocalNumPackageData; PackageDataIndex++)
		{
			FName PackageName;
			Ar << PackageName;
			
			if (Options.bSerializePackageData)
			{
				FAssetPackageData& NewPackageData = PreallocatedPackageDataBuffer[PackageDataIndex];
				NewPackageData.SerializeForCache(Ar);
				CachedPackageData.Add(PackageName, &NewPackageData);
			}
			else
			{
				FAssetPackageData FakeData;
				FakeData.SerializeForCache(Ar);
			}
		}
	}

	return !OriginalAr.IsError();
}

uint32 FAssetRegistryState::GetAllocatedSize(bool bLogDetailed) const
{
	uint32 TotalBytes = 0;

	uint32 MapMemory = CachedAssetsByObjectPath.GetAllocatedSize();
	MapMemory += CachedAssetsByPackageName.GetAllocatedSize();
	MapMemory += CachedAssetsByPath.GetAllocatedSize();
	MapMemory += CachedAssetsByClass.GetAllocatedSize();
	MapMemory += CachedAssetsByTag.GetAllocatedSize();
	MapMemory += CachedDependsNodes.GetAllocatedSize();
	MapMemory += CachedPackageData.GetAllocatedSize();

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("Index Size: %dk"), MapMemory / 1024);
	}

	uint32 AssetDataSize = 0, TagOverHead = 0, TotalTagSize = 0;
	TMap<FName, uint32> TagSizes;

	for (const TPair<FName, FAssetData*>& AssetDataPair : CachedAssetsByObjectPath)
	{
		const FAssetData& AssetData = *AssetDataPair.Value;
		
		AssetDataSize += sizeof(AssetData);
		AssetDataSize += AssetData.ChunkIDs.GetAllocatedSize();

		TagOverHead += AssetData.TagsAndValues.GetAllocatedSize();

		for (const TPair<FName, FString>& TagPair : AssetData.TagsAndValues)
		{
			uint32 StringSize = TagPair.Value.GetAllocatedSize();
			
			TotalTagSize += StringSize;
			TagSizes.FindOrAdd(TagPair.Key) += StringSize;
		}
	}

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetData Count: %d"), CachedAssetsByObjectPath.Num());
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetData Static Size: %dk"), AssetDataSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetData Tag Overhead: %dk"), TagOverHead / 1024);
		
		for (const TPair<FName, uint32>& SizePair : TagSizes)
		{
			UE_LOG(LogAssetRegistry, Log, TEXT("Tag %s Size: %dk"), *SizePair.Key.ToString(), SizePair.Value / 1024);
		}
	}

	uint32 DependNodesSize = 0, DependenciesSize = 0;

	for (const TPair<FAssetIdentifier, FDependsNode*>& DependsNodePair : CachedDependsNodes)
	{
		const FDependsNode& DependsNode = *DependsNodePair.Value;
		DependNodesSize += sizeof(DependsNode);

		DependenciesSize += DependsNode.GetAllocatedSize();
	}

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("Dependency Node Count: %d"), CachedDependsNodes.Num());
		UE_LOG(LogAssetRegistry, Log, TEXT("Dependency Node Static Size: %dk"), DependNodesSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("Dependency Arrays Size: %dk"), DependenciesSize / 1024);
	}

	uint32 PackageDataSize = CachedPackageData.Num() * sizeof(FAssetPackageData);

	TotalBytes = MapMemory + AssetDataSize + TagOverHead + TotalTagSize + DependNodesSize + DependenciesSize + PackageDataSize;

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("PackageData Count: %d"), CachedPackageData.Num());
		UE_LOG(LogAssetRegistry, Log, TEXT("PackageData Static Size: %dk"), PackageDataSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("Total State Size: %dk"), TotalBytes / 1024);
	}
	return TotalBytes;
}

FDependsNode* FAssetRegistryState::ResolveRedirector(FDependsNode* InDependency,
													 TMap<FName, FAssetData*>& InAllowedAssets,
													 TMap<FDependsNode*, FDependsNode*>& InCache)
{
	if (InCache.Contains(InDependency))
	{
		return InCache[InDependency];
	}

	FDependsNode* CurrentDependency = InDependency;
	FDependsNode* Result = nullptr;

	static TSet<FName> EncounteredDependencies;
	EncounteredDependencies.Empty();

	while (Result == nullptr)
	{
		checkSlow(CurrentDependency);

		if (EncounteredDependencies.Contains(CurrentDependency->GetPackageName()))
		{
			break;
		}

		EncounteredDependencies.Add(CurrentDependency->GetPackageName());

		if (CachedAssetsByPackageName.Contains(CurrentDependency->GetPackageName()))
		{
			// Get the list of assets contained in this package
			TArray<FAssetData*>& Assets = CachedAssetsByPackageName[CurrentDependency->GetPackageName()];

			for (FAssetData* Asset : Assets)
			{
				if (Asset->IsRedirector())
				{
					FDependsNode* ChainedRedirector = nullptr;
					// This asset is a redirector, so we want to look at its dependencies and find the asset that it is redirecting to
					CurrentDependency->IterateOverDependencies([&](FDependsNode* InDepends, EAssetRegistryDependencyType::Type) {
						if (InAllowedAssets.Contains(InDepends->GetPackageName()))
						{
							// This asset is in the allowed asset list, so take this as the redirect target
							Result = InDepends;
						}
						else if (CachedAssetsByPackageName.Contains(InDepends->GetPackageName()))
						{
							// This dependency isn't in the allowed list, but it is a valid asset in the registry.
							// Because this is a redirector, this should mean that the redirector is pointing at ANOTHER
							// redirector (or itself in some horrible situations) so we'll move to that node and try again
							ChainedRedirector = InDepends;
						}
					});

					if (ChainedRedirector)
					{
						// Found a redirector, break for loop
						CurrentDependency = ChainedRedirector;
						break;
					}
				}
				else
				{
					Result = CurrentDependency;
				}

				if (Result)
				{
					// We found an allowed asset from the original dependency node. We're finished!
					break;
				}
			}
		}
		else
		{
			Result = CurrentDependency;
		}
	}

	InCache.Add(InDependency, Result);
	return Result;
}

void FAssetRegistryState::AddAssetData(FAssetData* AssetData)
{
	NumAssets++;

	TArray<FAssetData*>& PackageAssets = CachedAssetsByPackageName.FindOrAdd(AssetData->PackageName);
	TArray<FAssetData*>& PathAssets = CachedAssetsByPath.FindOrAdd(AssetData->PackagePath);
	TArray<FAssetData*>& ClassAssets = CachedAssetsByClass.FindOrAdd(AssetData->AssetClass);

	CachedAssetsByObjectPath.Add(AssetData->ObjectPath, AssetData);
	PackageAssets.Add(AssetData);
	PathAssets.Add(AssetData);
	ClassAssets.Add(AssetData);

	for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
	{
		FName Key = TagIt.Key();

		TArray<FAssetData*>& TagAssets = CachedAssetsByTag.FindOrAdd(Key);
		TagAssets.Add(AssetData);
	}
}

void FAssetRegistryState::UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData)
{
	// Determine if tags need to be remapped
	bool bTagsChanged = AssetData->TagsAndValues.Num() != NewAssetData.TagsAndValues.Num();

	// If the old and new asset data has the same number of tags, see if any are different (its ok if values are different)
	if (!bTagsChanged)
	{
		for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			if (!NewAssetData.TagsAndValues.Contains(TagIt.Key()))
			{
				bTagsChanged = true;
				break;
			}
		}
	}

	// Update ObjectPath
	if (AssetData->PackageName != NewAssetData.PackageName || AssetData->AssetName != NewAssetData.AssetName)
	{
		CachedAssetsByObjectPath.Remove(AssetData->ObjectPath);
		CachedAssetsByObjectPath.Add(NewAssetData.ObjectPath, AssetData);
	}

	// Update PackageName
	if (AssetData->PackageName != NewAssetData.PackageName)
	{
		TArray<FAssetData*>* OldPackageAssets = CachedAssetsByPackageName.Find(AssetData->PackageName);
		TArray<FAssetData*>& NewPackageAssets = CachedAssetsByPackageName.FindOrAdd(NewAssetData.PackageName);

		OldPackageAssets->Remove(AssetData);
		NewPackageAssets.Add(AssetData);
	}

	// Update PackagePath
	if (AssetData->PackagePath != NewAssetData.PackagePath)
	{
		TArray<FAssetData*>* OldPathAssets = CachedAssetsByPath.Find(AssetData->PackagePath);
		TArray<FAssetData*>& NewPathAssets = CachedAssetsByPath.FindOrAdd(NewAssetData.PackagePath);

		OldPathAssets->Remove(AssetData);
		NewPathAssets.Add(AssetData);
	}

	// Update AssetClass
	if (AssetData->AssetClass != NewAssetData.AssetClass)
	{
		TArray<FAssetData*>* OldClassAssets = CachedAssetsByClass.Find(AssetData->AssetClass);
		TArray<FAssetData*>& NewClassAssets = CachedAssetsByClass.FindOrAdd(NewAssetData.AssetClass);

		OldClassAssets->Remove(AssetData);
		NewClassAssets.Add(AssetData);
	}

	// Update Tags
	if (bTagsChanged)
	{
		for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			const FName FNameKey = TagIt.Key();
			TArray<FAssetData*>* OldTagAssets = CachedAssetsByTag.Find(FNameKey);

			OldTagAssets->Remove(AssetData);
		}

		for (auto TagIt = NewAssetData.TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			const FName FNameKey = TagIt.Key();
			TArray<FAssetData*>& NewTagAssets = CachedAssetsByTag.FindOrAdd(FNameKey);

			NewTagAssets.Add(AssetData);
		}
	}

	// Copy in new values
	*AssetData = NewAssetData;
}

bool FAssetRegistryState::RemoveAssetData(FAssetData* AssetData)
{
	bool bRemoved = false;

	if (ensure(AssetData))
	{
		TArray<FAssetData*>* OldPackageAssets = CachedAssetsByPackageName.Find(AssetData->PackageName);
		TArray<FAssetData*>* OldPathAssets = CachedAssetsByPath.Find(AssetData->PackagePath);
		TArray<FAssetData*>* OldClassAssets = CachedAssetsByClass.Find(AssetData->AssetClass);

		CachedAssetsByObjectPath.Remove(AssetData->ObjectPath);
		OldPackageAssets->RemoveSingleSwap(AssetData);
		OldPathAssets->RemoveSingleSwap(AssetData);
		OldClassAssets->RemoveSingleSwap(AssetData);

		for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			TArray<FAssetData*>* OldTagAssets = CachedAssetsByTag.Find(TagIt.Key());
			OldTagAssets->RemoveSingleSwap(AssetData);
		}

		// We need to update the cached dependencies references cache so that they know we no
		// longer exist and so don't reference them.
		RemoveDependsNode(AssetData->PackageName);

		// Remove the package data as well
		RemovePackageData(AssetData->PackageName);

		// if the assets were preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
		if (PreallocatedAssetDataBuffers.Num() == 0)
		{
			delete AssetData;
		}
		NumAssets--;
	}

	return bRemoved;
}

FDependsNode* FAssetRegistryState::FindDependsNode(const FAssetIdentifier& Identifier)
{
	FDependsNode** FoundNode = CachedDependsNodes.Find(Identifier);
	if (FoundNode)
	{
		return *FoundNode;
	}
	else
	{
		return nullptr;
	}
}

FDependsNode* FAssetRegistryState::CreateOrFindDependsNode(const FAssetIdentifier& Identifier)
{
	FDependsNode* FoundNode = FindDependsNode(Identifier);
	if (FoundNode)
	{
		return FoundNode;
	}

	FDependsNode* NewNode = new FDependsNode(Identifier);
	NumDependsNodes++;
	CachedDependsNodes.Add(Identifier, NewNode);

	return NewNode;
}

bool FAssetRegistryState::RemoveDependsNode(const FAssetIdentifier& Identifier)
{
	FDependsNode** NodePtr = CachedDependsNodes.Find(Identifier);

	if (NodePtr != nullptr)
	{
		FDependsNode* Node = *NodePtr;
		if (Node != nullptr)
		{
			TArray<FDependsNode*> DependencyNodes;
			Node->GetDependencies(DependencyNodes);

			// Remove the reference to this node from all dependencies
			for (FDependsNode* DependencyNode : DependencyNodes)
			{
				DependencyNode->RemoveReferencer(Node);
			}

			TArray<FDependsNode*> ReferencerNodes;
			Node->GetReferencers(ReferencerNodes);

			// Remove the reference to this node from all referencers
			for (FDependsNode* ReferencerNode : ReferencerNodes)
			{
				ReferencerNode->RemoveDependency(Node);
			}

			// Remove the node and delete it
			CachedDependsNodes.Remove(Identifier);
			NumDependsNodes--;

			// if the depends nodes were preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
			if (PreallocatedDependsNodeDataBuffers.Num() == 0)
			{
				delete Node;
			}

			return true;
		}
	}

	return false;
}

const FAssetPackageData* FAssetRegistryState::GetAssetPackageData(FName PackageName) const
{
	FAssetPackageData* const* FoundData = CachedPackageData.Find(PackageName);
	if (FoundData)
	{
		return *FoundData;
	}
	else
	{
		return nullptr;
	}
}

FAssetPackageData* FAssetRegistryState::CreateOrGetAssetPackageData(FName PackageName)
{
	FAssetPackageData** FoundData = CachedPackageData.Find(PackageName);
	if (FoundData)
	{
		return *FoundData;
	}

	FAssetPackageData* NewData = new FAssetPackageData();
	NumPackageData++;
	CachedPackageData.Add(PackageName, NewData);

	return NewData;
}

bool FAssetRegistryState::RemovePackageData(FName PackageName)
{
	FAssetPackageData** DataPtr = CachedPackageData.Find(PackageName);

	if (DataPtr != nullptr)
	{
		FAssetPackageData* Data = *DataPtr;
		if (Data != nullptr)
		{
			CachedPackageData.Remove(PackageName);
			NumPackageData--;

			// if the package data was preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
			if (PreallocatedPackageDataBuffers.Num() == 0)
			{
				delete Data;
			}

			return true;
		}
	}
	return false;
}

bool FAssetRegistryState::IsFilterValid(const FARFilter& Filter, bool bAllowRecursion)
{
	for (int32 NameIdx = 0; NameIdx < Filter.PackageNames.Num(); ++NameIdx)
	{
		if (Filter.PackageNames[NameIdx] == NAME_None)
		{
			return false;
		}
	}

	for (int32 PathIdx = 0; PathIdx < Filter.PackagePaths.Num(); ++PathIdx)
	{
		if (Filter.PackagePaths[PathIdx] == NAME_None)
		{
			return false;
		}
	}

	for (int32 ObjectPathIdx = 0; ObjectPathIdx < Filter.ObjectPaths.Num(); ++ObjectPathIdx)
	{
		if (Filter.ObjectPaths[ObjectPathIdx] == NAME_None)
		{
			return false;
		}
	}

	for (int32 ClassIdx = 0; ClassIdx < Filter.ClassNames.Num(); ++ClassIdx)
	{
		if (Filter.ClassNames[ClassIdx] == NAME_None)
		{
			return false;
		}
	}

	for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
	{
		if (FilterTagIt.Key() == NAME_None)
		{
			return false;
		}
	}

	if (!bAllowRecursion && (Filter.bRecursiveClasses || Filter.bRecursivePaths))
	{
		return false;
	}

	return true;
}
