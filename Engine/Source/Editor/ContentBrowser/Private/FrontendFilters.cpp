// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FrontendFilters.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "ObjectTools.h"
#include "AssetRegistryModule.h"
#include "SAssetView.h"

/** Helper functions for frontend filters */
namespace FrontendFilterHelper
{
	/**
	 * Get a set of dependencies as package name's from a list of assets found with the given Asset Registry Filter.
	 * @param InAssetRegistryFilter		The filter to find assets for in the asset registry.
	 * @param AssetRegistry				The Asset Registry to find assets and dependencies.
	 * @param OutDependencySet			The output of dependencies found from a set of assets.
	 */
	void GetDependencies(const FARFilter& InAssetRegistryFilter, const IAssetRegistry& AssetRegistry, TSet<FName>& OutDependencySet)
	{
		TArray<FAssetData> FoundAssets;
		AssetRegistry.GetAssets(InAssetRegistryFilter, FoundAssets);

		for (const FAssetData& AssetData : FoundAssets)
		{
			// Store all the dependencies of all the levels
			TArray<FAssetIdentifier> AssetDependencies;
			AssetRegistry.GetDependencies(FAssetIdentifier(AssetData.PackageName), AssetDependencies);
			for (const FAssetIdentifier& Dependency : AssetDependencies)
			{
				OutDependencySet.Add(Dependency.PackageName);
			}
		}
	}
}

/////////////////////////////////////////
// FFrontendFilter_Text
/////////////////////////////////////////

/** Mapping of asset property tag aliases that can be used by text searches */
class FFrontendFilter_AssetPropertyTagAliases
{
public:
	static FFrontendFilter_AssetPropertyTagAliases& Get()
	{
		static FFrontendFilter_AssetPropertyTagAliases Singleton;
		return Singleton;
	}

	/** Get the source tag for the given asset data and alias, or none if there is no match */
	FName GetSourceTagFromAlias(const FAssetData& InAssetData, const FName InAlias)
	{
		TSharedPtr<TMap<FName, FName>>& AliasToSourceTagMapping = ClassToAliasTagsMapping.FindOrAdd(InAssetData.AssetClass);

		if (!AliasToSourceTagMapping.IsValid())
		{
			static const FName NAME_DisplayName(TEXT("DisplayName"));

			AliasToSourceTagMapping = MakeShared<TMap<FName, FName>>();

			UClass* AssetClass = InAssetData.GetClass();
			if (AssetClass)
			{
				TMap<FName, UObject::FAssetRegistryTagMetadata> AssetTagMetaData;
				AssetClass->GetDefaultObject()->GetAssetRegistryTagMetadata(AssetTagMetaData);

				for (const auto& AssetTagMetaDataPair : AssetTagMetaData)
				{
					if (!AssetTagMetaDataPair.Value.DisplayName.IsEmpty())
					{
						const FName DisplayName = MakeObjectNameFromDisplayLabel(AssetTagMetaDataPair.Value.DisplayName.ToString(), NAME_None);
						AliasToSourceTagMapping->Add(DisplayName, AssetTagMetaDataPair.Key);
					}
				}

				for (const auto& KeyValuePair : InAssetData.TagsAndValues)
				{
					if (UProperty* Field = FindField<UProperty>(AssetClass, KeyValuePair.Key))
					{
						if (Field->HasMetaData(NAME_DisplayName))
						{
							const FName DisplayName = MakeObjectNameFromDisplayLabel(Field->GetMetaData(NAME_DisplayName), NAME_None);
							AliasToSourceTagMapping->Add(DisplayName, KeyValuePair.Key);
						}
					}
				}
			}
		}

		return AliasToSourceTagMapping.IsValid() ? AliasToSourceTagMapping->FindRef(InAlias) : NAME_None;
	}

private:
	/** Mapping from class name -> (alias -> source) */
	TMap<FName, TSharedPtr<TMap<FName, FName>>> ClassToAliasTagsMapping;
};

/** Expression context which gathers up the names of any dynamic collections being referenced by the current query */
class FFrontendFilter_GatherDynamicCollectionsExpressionContext : public ITextFilterExpressionContext
{
public:
	FFrontendFilter_GatherDynamicCollectionsExpressionContext(TArray<FCollectionNameType>& OutReferencedDynamicCollections)
		: AvailableDynamicCollections()
		, ReferencedDynamicCollections(OutReferencedDynamicCollections)
		, CurrentRecursionDepth(0)
		, CollectionKeyName("Collection")
		, TagKeyName("Tag")
	{
		if (FCollectionManagerModule::IsModuleAvailable())
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			TArray<FCollectionNameType> AvailableCollections;
			CollectionManagerModule.Get().GetCollections(AvailableCollections);

			for (const FCollectionNameType& AvailableCollection : AvailableCollections)
			{
				// Only care about dynamic collections
				ECollectionStorageMode::Type StorageMode = ECollectionStorageMode::Static;
				CollectionManagerModule.Get().GetCollectionStorageMode(AvailableCollection.Name, AvailableCollection.Type, StorageMode);
				if (StorageMode != ECollectionStorageMode::Dynamic)
				{
					continue;
				}

				AvailableDynamicCollections.Add(AvailableCollection);
			}
		}
	}

	~FFrontendFilter_GatherDynamicCollectionsExpressionContext()
	{
		// Sort and populate the final list of referenced dynamic collections
		FoundDynamicCollections.Sort([](const FDynamicCollectionNameAndDepth& A, const FDynamicCollectionNameAndDepth& B)
		{
			return A.RecursionDepth > B.RecursionDepth;
		});

		ReferencedDynamicCollections.Reset();
		ReferencedDynamicCollections.Reserve(FoundDynamicCollections.Num());
		for (const auto& FoundDynamicCollection : FoundDynamicCollections)
		{
			ReferencedDynamicCollections.Add(FoundDynamicCollection.Collection);
		}
	}

	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		TestAgainstAvailableCollections(InValue, InTextComparisonMode);
		return false;
	}

	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		// Special case for collections, as these aren't contained within the asset registry meta-data
		if (InKey == CollectionKeyName || InKey == TagKeyName)
		{
			// Collections can only work with Equal or NotEqual type tests
			if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
			{
				return false;
			}

			TestAgainstAvailableCollections(InValue, InTextComparisonMode);
		}

		return false;
	}

private:
	bool TestAgainstAvailableCollections(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const
	{
		for (const FCollectionNameType& DynamicCollection : AvailableDynamicCollections)
		{
			const FString DynamicCollectionNameStr = DynamicCollection.Name.ToString();
			if (TextFilterUtils::TestBasicStringExpression(DynamicCollectionNameStr, InValue, InTextComparisonMode))
			{
				const bool bCollectionAlreadyProcessed = FoundDynamicCollections.ContainsByPredicate([&](const FDynamicCollectionNameAndDepth& Other)
				{
					return DynamicCollection == Other.Collection;
				});

				if (!bCollectionAlreadyProcessed)
				{
					FoundDynamicCollections.Add(FDynamicCollectionNameAndDepth(DynamicCollection, CurrentRecursionDepth));

					if (FCollectionManagerModule::IsModuleAvailable())
					{
						FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
						
						// Also need to gather any collections referenced by this dynamic collection
						++CurrentRecursionDepth;
						bool bUnused = false;
						CollectionManagerModule.Get().TestDynamicQuery(DynamicCollection.Name, DynamicCollection.Type, *this, bUnused);
						--CurrentRecursionDepth;
					}
				}

				return true;
			}
		}

		return false;
	}

	/** Contains a collection name along with its recursion depth in the dynamic query - used so we can test them depth first */
	struct FDynamicCollectionNameAndDepth
	{
		FDynamicCollectionNameAndDepth(FCollectionNameType InCollection, const int32 InRecursionDepth)
			: Collection(InCollection)
			, RecursionDepth(InRecursionDepth)
		{
		}

		FCollectionNameType Collection;
		int32 RecursionDepth;
	};

	/** The currently available dynamic collections */
	TArray<FCollectionNameType> AvailableDynamicCollections;

	/** This will be populated with any dynamic collections that are being referenced by the current query - these collections may not all match when tested against the actual asset data */
	TArray<FCollectionNameType>& ReferencedDynamicCollections;

	/** Dynamic collections that have currently be found as part of the query (or recursive sub-query) */
	mutable TArray<FDynamicCollectionNameAndDepth> FoundDynamicCollections;

	/** Incremented when we test a sub-query, decremented once we're done */
	mutable int32 CurrentRecursionDepth;

	/** Keys used by TestComplexExpression */
	const FName CollectionKeyName;
	const FName TagKeyName;
};

/** Expression context to test the given asset data against the current text filter */
class FFrontendFilter_TextFilterExpressionContext : public ITextFilterExpressionContext
{
public:
	typedef TRemoveReference<FAssetFilterType>::Type* FAssetFilterTypePtr;

	FFrontendFilter_TextFilterExpressionContext(const TArray<FCollectionNameType>& InReferencedDynamicCollections)
		: ReferencedDynamicCollections(InReferencedDynamicCollections)
		, AssetPtr(nullptr)
		, bIncludeClassName(true)
		, NameKeyName("Name")
		, PathKeyName("Path")
		, ClassKeyName("Class")
		, TypeKeyName("Type")
		, CollectionKeyName("Collection")
		, TagKeyName("Tag")
	{
	}

	void SetAsset(FAssetFilterTypePtr InAsset)
	{
		AssetPtr = InAsset;
		
		AssetPtr->PackageName.AppendString(AssetFullPath);
		AssetPtr->GetExportTextName(AssetExportTextName);

		// Test each piece of the path name, apart from the first
		{
			const TCHAR* Ptr = AssetFullPath.GetCharArray().GetData();
			if (Ptr)
			{
				// Test each piece of the path name, apart from the first
				bool bIsFirst = true;
				while (const TCHAR* Delimiter = FCString::Strchr(Ptr, '/'))
				{
					const int32 Length = Delimiter - Ptr;

					if (Length > 0)
					{
						if (bIsFirst)
						{
							bIsFirst = false;
						}
						else
						{
							AssetSplitPath.Emplace(Length, Ptr);
						}
					}

					Ptr += (Length + 1);
				}

				if (*Ptr != 0)
				{
					AssetSplitPath.Emplace(Ptr);
				}
			}
		}

		if (FCollectionManagerModule::IsModuleAvailable())
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			CollectionManagerModule.Get().GetCollectionsContainingObject(AssetPtr->ObjectPath, ECollectionShareType::CST_All, AssetCollectionNames, ECollectionRecursionFlags::SelfAndChildren);

			// Test the dynamic collections from the active query against the current asset
			// We can do this as a flat list since FFrontendFilter_GatherDynamicCollectionsExpressionContext has already taken care of processing the recursion
			for (const FCollectionNameType& DynamicCollection : ReferencedDynamicCollections)
			{
				bool bPassesCollectionFilter = false;
				CollectionManagerModule.Get().TestDynamicQuery(DynamicCollection.Name, DynamicCollection.Type, *this, bPassesCollectionFilter);
				if (bPassesCollectionFilter)
				{
					AssetCollectionNames.AddUnique(DynamicCollection.Name);
				}
			}
		}
	}

	void ClearAsset()
	{
		AssetPtr = nullptr;
		AssetFullPath.Reset();
		AssetExportTextName.Reset();
		AssetSplitPath.Reset();
		AssetCollectionNames.Reset();
	}

	void SetIncludeClassName(const bool InIncludeClassName)
	{
		bIncludeClassName = InIncludeClassName;
	}

	bool GetIncludeClassName() const
	{
		return bIncludeClassName;
	}

	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		for (const FString& AssetPathPart : AssetSplitPath)
		{
			if (TextFilterUtils::TestBasicStringExpression(AssetPathPart, InValue, InTextComparisonMode))
			{
				return true;
			}
		}

		if (bIncludeClassName)
		{
			if (TextFilterUtils::TestBasicStringExpression(AssetPtr->AssetClass, InValue, InTextComparisonMode))
			{
				return true;
			}

			// Only test this if we're searching the class name too, as the exported text contains the type in the string
			if (TextFilterUtils::TestBasicStringExpression(AssetExportTextName, InValue, InTextComparisonMode))
			{
				return true;
			}
		}

		for (const FName& AssetCollectionName : AssetCollectionNames)
		{
			if (TextFilterUtils::TestBasicStringExpression(AssetCollectionName, InValue, InTextComparisonMode))
			{
				return true;
			}
		}

		return false;
	}

	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		// Special case for the asset name, as this isn't contained within the asset registry meta-data
		if (InKey == NameKeyName)
		{
			// Names can only work with Equal or NotEqual type tests
			if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
			{
				return false;
			}

			const bool bIsMatch = TextFilterUtils::TestBasicStringExpression(AssetPtr->AssetName, InValue, InTextComparisonMode);
			return (InComparisonOperation == ETextFilterComparisonOperation::Equal) ? bIsMatch : !bIsMatch;
		}

		// Special case for the asset path, as this isn't contained within the asset registry meta-data
		if (InKey == PathKeyName)
		{
			// Paths can only work with Equal or NotEqual type tests
			if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
			{
				return false;
			}

			// If the comparison mode is partial, then we only need to test the ObjectPath as that contains the other two as sub-strings
			bool bIsMatch = false;
			if (InTextComparisonMode == ETextFilterTextComparisonMode::Partial)
			{
				bIsMatch = TextFilterUtils::TestBasicStringExpression(AssetPtr->ObjectPath, InValue, InTextComparisonMode);
			}
			else
			{
				bIsMatch = TextFilterUtils::TestBasicStringExpression(AssetPtr->ObjectPath, InValue, InTextComparisonMode)
					|| TextFilterUtils::TestBasicStringExpression(AssetPtr->PackageName, InValue, InTextComparisonMode)
					|| TextFilterUtils::TestBasicStringExpression(AssetPtr->PackagePath, InValue, InTextComparisonMode);
			}
			return (InComparisonOperation == ETextFilterComparisonOperation::Equal) ? bIsMatch : !bIsMatch;
		}

		// Special case for the asset type, as this isn't contained within the asset registry meta-data
		if (InKey == ClassKeyName || InKey == TypeKeyName)
		{
			// Class names can only work with Equal or NotEqual type tests
			if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
			{
				return false;
			}

			const bool bIsMatch = TextFilterUtils::TestBasicStringExpression(AssetPtr->AssetClass, InValue, InTextComparisonMode);
			return (InComparisonOperation == ETextFilterComparisonOperation::Equal) ? bIsMatch : !bIsMatch;
		}

		// Special case for collections, as these aren't contained within the asset registry meta-data
		if (InKey == CollectionKeyName || InKey == TagKeyName)
		{
			// Collections can only work with Equal or NotEqual type tests
			if (InComparisonOperation != ETextFilterComparisonOperation::Equal && InComparisonOperation != ETextFilterComparisonOperation::NotEqual)
			{
				return false;
			}

			bool bFoundMatch = false;
			for (const FName& AssetCollectionName : AssetCollectionNames)
			{
				if (TextFilterUtils::TestBasicStringExpression(AssetCollectionName, InValue, InTextComparisonMode))
				{
					bFoundMatch = true;
					break;
				}
			}

			return (InComparisonOperation == ETextFilterComparisonOperation::Equal) ? bFoundMatch : !bFoundMatch;
		}

		// Generic handling for anything in the asset meta-data
		{
			auto GetMetaDataValue = [this, &InKey](FString& OutMetaDataValue) -> bool
			{
				// Check for a literal key
				if (AssetPtr->GetTagValue(InKey, OutMetaDataValue))
				{
					return true;
				}

				// Check for an alias key
				const FName LiteralKey = FFrontendFilter_AssetPropertyTagAliases::Get().GetSourceTagFromAlias(*AssetPtr, InKey);
				if (!LiteralKey.IsNone() && AssetPtr->GetTagValue(LiteralKey, OutMetaDataValue))
				{
					return true;
				}

				return false;
			};

			FString MetaDataValue;
			if (GetMetaDataValue(MetaDataValue))
			{
				return TextFilterUtils::TestComplexExpression(MetaDataValue, InValue, InComparisonOperation, InTextComparisonMode);
			}
		}

		return false;
	}

private:
	/** An array of dynamic collections that are being referenced by the current query. These should be tested against each asset when it's looking for collections that contain it */
	const TArray<FCollectionNameType>& ReferencedDynamicCollections;

	/** Pointer to the asset we're currently filtering */
	FAssetFilterTypePtr AssetPtr;

	/** Full path of the current asset */
	FString AssetFullPath;

	/** The export text name of the current asset */
	FString AssetExportTextName;

	/** Split path of the current asset */
	TArray<FString> AssetSplitPath;

	/** Names of the collections that the current asset is in */
	TArray<FName> AssetCollectionNames;

	/** Are we supposed to include the class name in our basic string tests? */
	bool bIncludeClassName;

	/** Keys used by TestComplexExpression */
	const FName NameKeyName;
	const FName PathKeyName;
	const FName ClassKeyName;
	const FName TypeKeyName;
	const FName CollectionKeyName;
	const FName TagKeyName;
};

FFrontendFilter_Text::FFrontendFilter_Text()
	: FFrontendFilter(nullptr)
	, ReferencedDynamicCollections()
	, TextFilterExpressionContext(MakeShareable(new FFrontendFilter_TextFilterExpressionContext(ReferencedDynamicCollections)))
	, TextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::Complex)
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// We need to watch for collection changes so that we can keep ReferencedDynamicCollections up-to-date
	OnCollectionCreatedHandle = CollectionManagerModule.Get().OnCollectionCreated().AddRaw(this, &FFrontendFilter_Text::HandleCollectionCreated);
	OnCollectionDestroyedHandle = CollectionManagerModule.Get().OnCollectionDestroyed().AddRaw(this, &FFrontendFilter_Text::HandleCollectionDestroyed);
	OnCollectionRenamedHandle = CollectionManagerModule.Get().OnCollectionRenamed().AddRaw(this, &FFrontendFilter_Text::HandleCollectionRenamed);
	OnCollectionUpdatedHandle = CollectionManagerModule.Get().OnCollectionUpdated().AddRaw(this, &FFrontendFilter_Text::HandleCollectionUpdated);
}

FFrontendFilter_Text::~FFrontendFilter_Text()
{
	// Check IsModuleAvailable as we might be in the process of shutting down...
	if (FCollectionManagerModule::IsModuleAvailable())
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

		CollectionManagerModule.Get().OnCollectionCreated().Remove(OnCollectionCreatedHandle);
		CollectionManagerModule.Get().OnCollectionDestroyed().Remove(OnCollectionDestroyedHandle);
		CollectionManagerModule.Get().OnCollectionRenamed().Remove(OnCollectionRenamedHandle);
		CollectionManagerModule.Get().OnCollectionUpdated().Remove(OnCollectionUpdatedHandle);
	}
}

bool FFrontendFilter_Text::PassesFilter(FAssetFilterType InItem) const
{
	TextFilterExpressionContext->SetAsset(&InItem);
	const bool bMatched = TextFilterExpressionEvaluator.TestTextFilter(*TextFilterExpressionContext);
	TextFilterExpressionContext->ClearAsset();
	return bMatched;
}

FText FFrontendFilter_Text::GetRawFilterText() const
{
	return TextFilterExpressionEvaluator.GetFilterText();
}

void FFrontendFilter_Text::SetRawFilterText(const FText& InFilterText)
{
	if (TextFilterExpressionEvaluator.SetFilterText(InFilterText))
	{
		RebuildReferencedDynamicCollections();

		// Will trigger a re-filter with the new text
		BroadcastChangedEvent();
	}
}

FText FFrontendFilter_Text::GetFilterErrorText() const
{
	return TextFilterExpressionEvaluator.GetFilterErrorText();
}

void FFrontendFilter_Text::SetIncludeClassName(const bool InIncludeClassName)
{
	if (TextFilterExpressionContext->GetIncludeClassName() != InIncludeClassName)
	{
		TextFilterExpressionContext->SetIncludeClassName(InIncludeClassName);

		// Will trigger a re-filter with the new setting
		BroadcastChangedEvent();
	}
}

void FFrontendFilter_Text::HandleCollectionCreated(const FCollectionNameType& Collection)
{
	RebuildReferencedDynamicCollections();

	// Will trigger a re-filter with the new collections
	BroadcastChangedEvent();
}

void FFrontendFilter_Text::HandleCollectionDestroyed(const FCollectionNameType& Collection)
{
	if (ReferencedDynamicCollections.Contains(Collection))
	{
		RebuildReferencedDynamicCollections();

		// Will trigger a re-filter with the new collections
		BroadcastChangedEvent();
	}
}

void FFrontendFilter_Text::HandleCollectionRenamed(const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection)
{
	int32 FoundIndex = INDEX_NONE;
	if (ReferencedDynamicCollections.Find(OriginalCollection, FoundIndex))
	{
		ReferencedDynamicCollections[FoundIndex] = NewCollection;
	}
}

void FFrontendFilter_Text::HandleCollectionUpdated(const FCollectionNameType& Collection)
{
	RebuildReferencedDynamicCollections();

	// Will trigger a re-filter with the new collections
	BroadcastChangedEvent();
}

void FFrontendFilter_Text::RebuildReferencedDynamicCollections()
{
	TextFilterExpressionEvaluator.TestTextFilter(FFrontendFilter_GatherDynamicCollectionsExpressionContext(ReferencedDynamicCollections));
}


/////////////////////////////////////////
// FFrontendFilter_CheckedOut
/////////////////////////////////////////

FFrontendFilter_CheckedOut::FFrontendFilter_CheckedOut(TSharedPtr<FFrontendFilterCategory> InCategory) 
	: FFrontendFilter(InCategory)
{
	
}

void FFrontendFilter_CheckedOut::ActiveStateChanged(bool bActive)
{
	if(bActive)
	{
		RequestStatus();
	}
}

bool FFrontendFilter_CheckedOut::PassesFilter(FAssetFilterType InItem) const
{
	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(SourceControlHelpers::PackageFilename(InItem.PackageName.ToString()), EStateCacheUsage::Use);
	return SourceControlState.IsValid() && (SourceControlState->IsCheckedOut() || SourceControlState->IsAdded());
}

void FFrontendFilter_CheckedOut::RequestStatus()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		// Request the opened files at filter construction time to make sure checked out files have the correct state for the filter
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOperation->SetGetOpenedOnly(true);
		SourceControlProvider.Execute(UpdateStatusOperation, EConcurrency::Asynchronous, FSourceControlOperationComplete::CreateSP(this, &FFrontendFilter_CheckedOut::SourceControlOperationComplete) );
	}
}

void FFrontendFilter_CheckedOut::SourceControlOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	BroadcastChangedEvent();
}

/////////////////////////////////////////
// FFrontendFilter_Modified
/////////////////////////////////////////

FFrontendFilter_Modified::FFrontendFilter_Modified(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
	, bIsCurrentlyActive(false)
{
	UPackage::PackageDirtyStateChangedEvent.AddRaw(this, &FFrontendFilter_Modified::OnPackageDirtyStateUpdated);
}

FFrontendFilter_Modified::~FFrontendFilter_Modified()
{
	UPackage::PackageDirtyStateChangedEvent.RemoveAll(this);
}

void FFrontendFilter_Modified::ActiveStateChanged(bool bActive)
{
	bIsCurrentlyActive = bActive;
}

bool FFrontendFilter_Modified::PassesFilter(FAssetFilterType InItem) const
{
	UPackage* Package = FindPackage(NULL, *InItem.PackageName.ToString());

	if ( Package != NULL )
	{
		return Package->IsDirty();
	}

	return false;
}

void FFrontendFilter_Modified::OnPackageDirtyStateUpdated(UPackage* Package)
{
	if (bIsCurrentlyActive)
	{
		BroadcastChangedEvent();
	}
}

/////////////////////////////////////////
// FFrontendFilter_ReplicatedBlueprint
/////////////////////////////////////////

bool FFrontendFilter_ReplicatedBlueprint::PassesFilter(FAssetFilterType InItem) const
{
	const int32 NumReplicatedProperties = InItem.GetTagValueRef<int32>("NumReplicatedProperties");
	return NumReplicatedProperties > 0;
}

/////////////////////////////////////////
// FFrontendFilter_ArbitraryComparisonOperation
/////////////////////////////////////////

#define LOCTEXT_NAMESPACE "ContentBrowser"

FFrontendFilter_ArbitraryComparisonOperation::FFrontendFilter_ArbitraryComparisonOperation(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
	, TagName(TEXT("TagName"))
	, TargetTagValue(TEXT("Value"))
	, ComparisonOp(ETextFilterComparisonOperation::NotEqual)
{
}

FString FFrontendFilter_ArbitraryComparisonOperation::GetName() const
{
	return TEXT("CompareTags");
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetDisplayName() const
{
	return FText::Format(LOCTEXT("FFrontendFilter_CompareOperation", "Compare Tags ({0} {1} {2})"),
		FText::FromName(TagName),
		FText::AsCultureInvariant(ConvertOperationToString(ComparisonOp)),
		FText::AsCultureInvariant(TargetTagValue));
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetToolTipText() const
{
	return LOCTEXT("FFrontendFilter_CompareOperation_ToolTip", "Compares AssetRegistrySearchable values on assets with a target value.");
}

bool FFrontendFilter_ArbitraryComparisonOperation::PassesFilter(FAssetFilterType InItem) const
{
	FString TagValue;
	if (InItem.GetTagValue(TagName, TagValue))
	{
		return TextFilterUtils::TestComplexExpression(TagValue, TargetTagValue, ComparisonOp, ETextFilterTextComparisonMode::Exact);
	}
	else
	{
		// Failed to find the tag, can't pass the filter
		//@TODO: Maybe we should succeed here if the operation is !=
		return false;
	}
}

void FFrontendFilter_ArbitraryComparisonOperation::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	FUIAction Action;

	MenuBuilder.BeginSection(TEXT("ComparsionSection"), LOCTEXT("ComparisonSectionHeading", "AssetRegistrySearchable Comparison"));

	TSharedRef<SWidget> KeyWidget =
		SNew(SEditableTextBox)
		.Text_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::GetKeyValueAsText)
		.OnTextCommitted_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::OnKeyValueTextCommitted)
		.MinDesiredWidth(100.0f);
	TSharedRef<SWidget> ValueWidget = SNew(SEditableTextBox)
		.Text_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::GetTargetValueAsText)
		.OnTextCommitted_Raw(this, &FFrontendFilter_ArbitraryComparisonOperation::OnTargetValueTextCommitted)
		.MinDesiredWidth(100.0f);

	MenuBuilder.AddWidget(KeyWidget, LOCTEXT("KeyMenuDesc", "Tag"));
	MenuBuilder.AddWidget(ValueWidget, LOCTEXT("ValueMenuDesc", "Target Value"));

#define UE_SET_COMP_OP(Operation) \
	MenuBuilder.AddMenuEntry(FText::AsCultureInvariant(ConvertOperationToString(Operation)), \
		LOCTEXT("SwitchOpsTooltip", "Switch comparsion type"), \
		FSlateIcon(), \
		FUIAction(FExecuteAction::CreateRaw(this, &FFrontendFilter_ArbitraryComparisonOperation::SetComparisonOperation, Operation), FCanExecuteAction(), FIsActionChecked::CreateRaw(this, &FFrontendFilter_ArbitraryComparisonOperation::IsComparisonOperationEqualTo, Operation)), \
		NAME_None, \
		EUserInterfaceActionType::RadioButton);

	UE_SET_COMP_OP(ETextFilterComparisonOperation::Equal);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::NotEqual);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::Less);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::LessOrEqual);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::Greater);
	UE_SET_COMP_OP(ETextFilterComparisonOperation::GreaterOrEqual);
#undef UE_SET_COMP_OP

	MenuBuilder.EndSection();
}

void FFrontendFilter_ArbitraryComparisonOperation::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".Key")), *TagName.ToString(), IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".Value")), *TargetTagValue, IniFilename);
	GConfig->SetString(*IniSection, *(SettingsString + TEXT(".Op")), *FString::FromInt((int32)ComparisonOp), IniFilename);
}

void FFrontendFilter_ArbitraryComparisonOperation::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	FString TagNameAsString;
	if (GConfig->GetString(*IniSection, *(SettingsString + TEXT(".Key")), TagNameAsString, IniFilename))
	{
		TagName = *TagNameAsString;
	}

	GConfig->GetString(*IniSection, *(SettingsString + TEXT(".Value")), TargetTagValue, IniFilename);

	int32 OpAsInteger;
	if (GConfig->GetInt(*IniSection, *(SettingsString + TEXT(".Op")), OpAsInteger, IniFilename))
	{
		ComparisonOp = (ETextFilterComparisonOperation)OpAsInteger;
	}
}

void FFrontendFilter_ArbitraryComparisonOperation::SetComparisonOperation(ETextFilterComparisonOperation NewOp)
{
	ComparisonOp = NewOp;
	BroadcastChangedEvent();
}

bool FFrontendFilter_ArbitraryComparisonOperation::IsComparisonOperationEqualTo(ETextFilterComparisonOperation TestOp) const
{
	return (ComparisonOp == TestOp);
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetKeyValueAsText() const
{
	return FText::FromName(TagName);
}

FText FFrontendFilter_ArbitraryComparisonOperation::GetTargetValueAsText() const
{
	return FText::AsCultureInvariant(TargetTagValue);
}

void FFrontendFilter_ArbitraryComparisonOperation::OnKeyValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (!InText.IsEmpty())
	{
		TagName = *InText.ToString();
		BroadcastChangedEvent();
	}
}

void FFrontendFilter_ArbitraryComparisonOperation::OnTargetValueTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	TargetTagValue = InText.ToString();
	BroadcastChangedEvent();
}

FString FFrontendFilter_ArbitraryComparisonOperation::ConvertOperationToString(ETextFilterComparisonOperation Op)
{
	switch (Op)
	{
	case ETextFilterComparisonOperation::Equal:
		return TEXT("==");
	case ETextFilterComparisonOperation::NotEqual:
		return TEXT("!=");
	case ETextFilterComparisonOperation::Less:
		return TEXT("<");
	case ETextFilterComparisonOperation::LessOrEqual:
		return TEXT("<=");
	case ETextFilterComparisonOperation::Greater:
		return TEXT(">");
	case ETextFilterComparisonOperation::GreaterOrEqual:
		return TEXT(">=");
	default:
		check(false);
		return TEXT("op");
	};
}

#undef LOCTEXT_NAMESPACE

/////////////////////////////////////////
// FFrontendFilter_ShowOtherDevelopers
/////////////////////////////////////////

FFrontendFilter_ShowOtherDevelopers::FFrontendFilter_ShowOtherDevelopers(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
	, BaseDeveloperPath(FPackageName::FilenameToLongPackageName(FPaths::GameDevelopersDir()))
	, UserDeveloperPath(FPackageName::FilenameToLongPackageName(FPaths::GameUserDeveloperDir()))
	, bIsOnlyOneDeveloperPathSelected(false)
	, bShowOtherDeveloperAssets(false)
{
}

void FFrontendFilter_ShowOtherDevelopers::SetCurrentFilter(const FARFilter& InFilter)
{
	if ( InFilter.PackagePaths.Num() == 1 )
	{
		const FString PackagePath = InFilter.PackagePaths[0].ToString() + TEXT("/");
		
		// If the path starts with the base developer path, and is not the path itself then only one developer path is selected
		bIsOnlyOneDeveloperPathSelected = PackagePath.StartsWith(BaseDeveloperPath) && PackagePath.Len() != BaseDeveloperPath.Len();
	}
	else
	{
		// More or less than one path is selected
		bIsOnlyOneDeveloperPathSelected = false;
	}
}

bool FFrontendFilter_ShowOtherDevelopers::PassesFilter(FAssetFilterType InItem) const
{
	// Pass all assets if other developer assets are allowed
	if ( !bShowOtherDeveloperAssets )
	{
		// Never hide developer assets when a single developer folder is selected.
		if ( !bIsOnlyOneDeveloperPathSelected )
		{
			// If selecting multiple folders, the Developers folder/parent folder, or "All Assets", hide assets which are found in the development folder unless they are in the current user's folder
			const FString PackagePath = InItem.PackagePath.ToString() + TEXT("/");
			const bool bPackageInDeveloperFolder = PackagePath.StartsWith(BaseDeveloperPath) && PackagePath.Len() != BaseDeveloperPath.Len();

			if ( bPackageInDeveloperFolder )
			{
				const bool bPackageInUserDeveloperFolder = PackagePath.StartsWith(UserDeveloperPath);
				if ( !bPackageInUserDeveloperFolder )
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FFrontendFilter_ShowOtherDevelopers::SetShowOtherDeveloperAssets(bool bValue)
{
	if ( bShowOtherDeveloperAssets != bValue )
	{
		bShowOtherDeveloperAssets = bValue;
		BroadcastChangedEvent();
	}
}

bool FFrontendFilter_ShowOtherDevelopers::GetShowOtherDeveloperAssets() const
{
	return bShowOtherDeveloperAssets;
}

/////////////////////////////////////////
// FFrontendFilter_ShowRedirectors
/////////////////////////////////////////

FFrontendFilter_ShowRedirectors::FFrontendFilter_ShowRedirectors(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
{
	bAreRedirectorsInBaseFilter = false;
	RedirectorClassName = UObjectRedirector::StaticClass()->GetFName();
}

void FFrontendFilter_ShowRedirectors::SetCurrentFilter(const FARFilter& InFilter)
{
	bAreRedirectorsInBaseFilter = InFilter.ClassNames.Contains(RedirectorClassName);
}

bool FFrontendFilter_ShowRedirectors::PassesFilter(FAssetFilterType InItem) const
{
	// Never hide redirectors if they are explicitly searched for
	if ( !bAreRedirectorsInBaseFilter )
	{
		return InItem.AssetClass != RedirectorClassName;
	}

	return true;
}

/////////////////////////////////////////
// FFrontendFilter_InUseByLoadedLevels
/////////////////////////////////////////

FFrontendFilter_InUseByLoadedLevels::FFrontendFilter_InUseByLoadedLevels(TSharedPtr<FFrontendFilterCategory> InCategory) 
	: FFrontendFilter(InCategory)
	, bIsCurrentlyActive(false)
{
	FEditorDelegates::MapChange.AddRaw(this, &FFrontendFilter_InUseByLoadedLevels::OnEditorMapChange);

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.OnAssetPostRename().AddRaw(this, &FFrontendFilter_InUseByLoadedLevels::OnAssetPostRename);
}

FFrontendFilter_InUseByLoadedLevels::~FFrontendFilter_InUseByLoadedLevels()
{
	FEditorDelegates::MapChange.RemoveAll(this);

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	AssetTools.OnAssetPostRename().RemoveAll(this);
}

void FFrontendFilter_InUseByLoadedLevels::ActiveStateChanged( bool bActive )
{
	bIsCurrentlyActive = bActive;

	if ( bActive )
	{
		ObjectTools::TagInUseObjects(ObjectTools::SO_LoadedLevels);
	}
}

void FFrontendFilter_InUseByLoadedLevels::OnAssetPostRename(const TArray<FAssetRenameData>& AssetsAndNames)
{
	// Update the tags identifying objects currently used by loaded levels
	ObjectTools::TagInUseObjects(ObjectTools::SO_LoadedLevels);
}

bool FFrontendFilter_InUseByLoadedLevels::PassesFilter(FAssetFilterType InItem) const
{
	bool bObjectInUse = false;
	
	if ( InItem.IsAssetLoaded() )
	{
		UObject* Asset = InItem.GetAsset();

		const bool bUnreferenced = !Asset->HasAnyMarks( OBJECTMARK_TagExp );
		const bool bIndirectlyReferencedObject = Asset->HasAnyMarks( OBJECTMARK_TagImp );
		const bool bRejectObject =
			Asset->GetOuter() == NULL || // Skip objects with null outers
			Asset->HasAnyFlags( RF_Transient ) || // Skip transient objects (these shouldn't show up in the CB anyway)
			Asset->IsPendingKill() || // Objects that will be garbage collected 
			bUnreferenced || // Unreferenced objects 
			bIndirectlyReferencedObject; // Indirectly referenced objects

		if( !bRejectObject && Asset->HasAnyFlags( RF_Public ) )
		{
			// The object is in use 
			bObjectInUse = true;
		}
	}

	return bObjectInUse;
}

void FFrontendFilter_InUseByLoadedLevels::OnEditorMapChange( uint32 MapChangeFlags )
{
	if ( MapChangeFlags == MapChangeEventFlags::NewMap && bIsCurrentlyActive )
	{
		ObjectTools::TagInUseObjects(ObjectTools::SO_LoadedLevels);
		BroadcastChangedEvent();
	}
}

/////////////////////////////////////////
// FFrontendFilter_InUseByAnyLevel
/////////////////////////////////////////

FFrontendFilter_UsedInAnyLevel::FFrontendFilter_UsedInAnyLevel(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
{
	// Prepare asset registry.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistry = &AssetRegistryModule.Get();
	check (AssetRegistry != nullptr);
}

FFrontendFilter_UsedInAnyLevel::~FFrontendFilter_UsedInAnyLevel()
{
	AssetRegistry = nullptr;
}

void FFrontendFilter_UsedInAnyLevel::ActiveStateChanged(bool bActive)
{
	LevelsDependencies.Empty();

	if (bActive)
	{
		// Find all the levels
		FARFilter Filter;
		Filter.ClassNames.Add(UWorld::StaticClass()->GetFName());
		FrontendFilterHelper::GetDependencies(Filter, *AssetRegistry, LevelsDependencies);
	}
}

bool FFrontendFilter_UsedInAnyLevel::PassesFilter(FAssetFilterType InItem) const	
{
	return LevelsDependencies.Contains(InItem.PackageName);
}

/////////////////////////////////////////
// FFrontendFilter_NotUsedInAnyLevel
/////////////////////////////////////////

FFrontendFilter_NotUsedInAnyLevel::FFrontendFilter_NotUsedInAnyLevel(TSharedPtr<FFrontendFilterCategory> InCategory)
	: FFrontendFilter(InCategory)
{
	// Prepare asset registry.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistry = &AssetRegistryModule.Get();
	check (AssetRegistry != nullptr);
}


FFrontendFilter_NotUsedInAnyLevel::~FFrontendFilter_NotUsedInAnyLevel()
{
	AssetRegistry = nullptr;
}

void FFrontendFilter_NotUsedInAnyLevel::ActiveStateChanged(bool bActive)
{
	LevelsDependencies.Empty();
	
	if (bActive)
	{
		// Find all the levels
		FARFilter Filter;
		Filter.ClassNames.Add(UWorld::StaticClass()->GetFName());
		FrontendFilterHelper::GetDependencies(Filter, *AssetRegistry, LevelsDependencies);
	}
}

bool FFrontendFilter_NotUsedInAnyLevel::PassesFilter(FAssetFilterType InItem) const
{
	return !LevelsDependencies.Contains(InItem.PackageName);
}