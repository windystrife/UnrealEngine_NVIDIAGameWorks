// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "ImaginaryBlueprintData.h"
#include "Misc/ScopeLock.h"


#define LOCTEXT_NAMESPACE "FindInBlueprints"

///////////////////////
// FSearchableValueInfo

FText FSearchableValueInfo::GetDisplayText(const TMap<int32, FText>& InLookupTable) const
{
	if (!DisplayText.IsEmpty() || LookupTableKey == -1)
	{
		return DisplayText;
	}
	return FindInBlueprintsHelpers::AsFText(LookupTableKey, InLookupTable);
}

////////////////////////////
// FComponentUniqueDisplay

bool FComponentUniqueDisplay::operator==(const FComponentUniqueDisplay& Other)
{
	// Two search results in the same object/sub-object should never have the same display string ({Key}: {Value} pairing)
	return SearchResult.IsValid() && Other.SearchResult.IsValid() && SearchResult->GetDisplayString().CompareTo(Other.SearchResult->GetDisplayString()) == 0;
}

///////////////////////
// FImaginaryFiBData

FCriticalSection FImaginaryFiBData::ParseChildDataCriticalSection;

FImaginaryFiBData::FImaginaryFiBData(TWeakPtr<FImaginaryFiBData> InOuter) 
	: LookupTablePtr(nullptr)
	, Outer(InOuter)
{
}

FImaginaryFiBData::FImaginaryFiBData(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr)
	: UnparsedJsonObject(InUnparsedJsonObject)
	, LookupTablePtr(InLookupTablePtr)
	, Outer(InOuter)
{
}

FSearchResult FImaginaryFiBData::CreateSearchResult(FSearchResult InParent) const
{
	FSearchResult ReturnSearchResult = CreateSearchResult_Internal(InParent);

	FText DisplayName;
	for( const TPair<FindInBlueprintsHelpers::FSimpleFTextKeyStorage, FSearchableValueInfo>& TagsAndValues : ParsedTagsAndValues )
	{
		if (TagsAndValues.Value.IsCoreDisplay() || !TagsAndValues.Value.IsSearchable())
		{
			FText Value = TagsAndValues.Value.GetDisplayText(*LookupTablePtr);
			ReturnSearchResult->ParseSearchInfo(TagsAndValues.Key.Text, Value);
		}
	}
	return ReturnSearchResult;
}

FSearchResult FImaginaryFiBData::CreateSearchTree(FSearchResult InParentSearchResult, TWeakPtr< FImaginaryFiBData > InCurrentPointer, TArray< const FImaginaryFiBData* >& InValidSearchResults, TMultiMap< const FImaginaryFiBData*, FComponentUniqueDisplay >& InMatchingSearchComponents)
{
	TSharedPtr<FImaginaryFiBData> CurrentDataPtr = InCurrentPointer.Pin();
	if (FImaginaryFiBData* CurrentData = CurrentDataPtr.Get())
	{
		FSearchResult CurrentSearchResult = CurrentData->CreateSearchResult(InParentSearchResult);
		bool bValidSearchResults = false;

		// Check all children first, to see if they are valid in the search results
		for (TSharedPtr<FImaginaryFiBData> ChildData : CurrentData->ParsedChildData)
		{
			FSearchResult Result = CreateSearchTree(CurrentSearchResult, ChildData, InValidSearchResults, InMatchingSearchComponents);
			if (Result.IsValid())
			{
				bValidSearchResults = true;
				CurrentSearchResult->Children.Add(Result);
			}
		}

		// If the children did not match the search results but this item does, then we will want to return true
		if (!bValidSearchResults && !CurrentData->IsCategory() && (InValidSearchResults.Find(CurrentData) != INDEX_NONE || InMatchingSearchComponents.Find(CurrentData)))
		{
			bValidSearchResults = true;
		}

		if (bValidSearchResults)
		{
			TArray< FComponentUniqueDisplay > SearchResultList;
			InMatchingSearchComponents.MultiFind(CurrentData, SearchResultList, true);
			CurrentSearchResult->Children.Reserve(CurrentSearchResult->Children.Num() + SearchResultList.Num());

			// Add any data that matched the search results as a child of our search result
			for (FComponentUniqueDisplay& SearchResultWrapper : SearchResultList)
			{
				SearchResultWrapper.SearchResult->Parent = CurrentSearchResult;
				CurrentSearchResult->Children.Add(SearchResultWrapper.SearchResult);
			}
			return CurrentSearchResult;
		}
	}
	return nullptr;
}

bool FImaginaryFiBData::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return true;
}

bool FImaginaryFiBData::CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	// Always compatible with the AllFilter
	return InSearchQueryFilter == ESearchQueryFilter::AllFilter;
}

void FImaginaryFiBData::ParseAllChildData_Internal(ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::Searchable*/)
{
	FScopeLock ScopeLock(&FImaginaryFiBData::ParseChildDataCriticalSection);

	if (UnparsedJsonObject.IsValid())
	{
		if (InSearchabilityOverride & ESearchableValueStatus::Searchable)
		{
			TSharedPtr< FJsonObject > MetaDataField;
			for (auto MapValues : UnparsedJsonObject->Values)
			{
				FText KeyText = FindInBlueprintsHelpers::AsFText(FCString::Atoi(*MapValues.Key), *LookupTablePtr);
				if (!KeyText.CompareTo(FFindInBlueprintSearchTags::FiBMetaDataTag))
				{
					MetaDataField = MapValues.Value->AsObject();
					break;
				}
			}

			if (MetaDataField.IsValid())
			{
				TSharedPtr<FFiBMetaData> MetaDataFiBInfo = MakeShareable(new FFiBMetaData(AsShared(), MetaDataField, LookupTablePtr));
				MetaDataFiBInfo->ParseAllChildData_Internal();

				if (MetaDataFiBInfo->IsHidden() && MetaDataFiBInfo->IsExplicit())
				{
					InSearchabilityOverride = ESearchableValueStatus::ExplicitySearchableHidden;
				}
				else if (MetaDataFiBInfo->IsExplicit())
				{
					InSearchabilityOverride = ESearchableValueStatus::ExplicitySearchable;
				}
			}
		}

		for( auto MapValues : UnparsedJsonObject->Values )
		{
			FText KeyText = FindInBlueprintsHelpers::AsFText(FCString::Atoi(*MapValues.Key), *LookupTablePtr);
			TSharedPtr< FJsonValue > JsonValue = MapValues.Value;

			if (!KeyText.CompareTo(FFindInBlueprintSearchTags::FiBMetaDataTag))
			{
				// Do not let this be processed again
				continue;
			}
			if (!TrySpecialHandleJsonValue(KeyText, JsonValue))
			{
				ParseJsonValue(KeyText, KeyText, JsonValue, false, InSearchabilityOverride);
			}
		}
	}

	UnparsedJsonObject.Reset();
}

void FImaginaryFiBData::ParseAllChildData(ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::Searchable*/)
{
	FScopeLock ScopeLock(&FImaginaryFiBData::ParseChildDataCriticalSection);
	ParseAllChildData_Internal(InSearchabilityOverride);
}

void FImaginaryFiBData::ParseJsonValue(FText InKey, FText InDisplayKey, TSharedPtr< FJsonValue > InJsonValue, bool bIsInArray/*=false*/, ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::Searchable*/)
{
	ESearchableValueStatus SearchabilityStatus = (InSearchabilityOverride == ESearchableValueStatus::Searchable)? GetSearchabilityStatus(InKey.ToString()) : InSearchabilityOverride;
	if( InJsonValue->Type == EJson::String)
	{
		ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(InKey), FSearchableValueInfo(InDisplayKey, FCString::Atoi(*InJsonValue->AsString()), SearchabilityStatus));
	}
	else if (InJsonValue->Type == EJson::Boolean)
	{
		ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(InKey), FSearchableValueInfo(InDisplayKey, FText::FromString(InJsonValue->AsString()), SearchabilityStatus));
	}
	else if( InJsonValue->Type == EJson::Array)
	{
		TSharedPtr< FCategorySectionHelper > ArrayCategory = MakeShareable(new FCategorySectionHelper(AsShared(), LookupTablePtr, InKey, true));
		ParsedChildData.Add(ArrayCategory);
		TArray<TSharedPtr< FJsonValue > > ArrayList = InJsonValue->AsArray();
		for( int32 ArrayIdx = 0; ArrayIdx < ArrayList.Num(); ++ArrayIdx)
		{
			TSharedPtr< FJsonValue > ArrayValue = ArrayList[ArrayIdx];
			ArrayCategory->ParseJsonValue(InKey, FText::FromString(FString::FromInt(ArrayIdx)), ArrayValue,/*bIsInArray=*/true, SearchabilityStatus);
		}		
	}
	else if (InJsonValue->Type == EJson::Object)
	{
		TSharedPtr< FCategorySectionHelper > SubObjectCategory = MakeShareable(new FCategorySectionHelper(AsShared(), InJsonValue->AsObject(), LookupTablePtr, InDisplayKey, bIsInArray));
		SubObjectCategory->ParseAllChildData_Internal(SearchabilityStatus);
		ParsedChildData.Add(SubObjectCategory);
	}
	else
	{
		// For everything else, there's this. Numbers come here and will be treated as strings
		ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(InKey), FSearchableValueInfo(InDisplayKey, FText::FromString(InJsonValue->AsString()), SearchabilityStatus));
	}
}

FText FImaginaryFiBData::CreateSearchComponentDisplayText(FText InKey, FText InValue) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Key"), InKey);
	Args.Add(TEXT("Value"), InValue);
	return FText::Format(LOCTEXT("ExtraSearchInfo", "{Key}: {Value}"), Args);
}

bool FImaginaryFiBData::TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode, TMultiMap< const FImaginaryFiBData*, FComponentUniqueDisplay >& InOutMatchingSearchComponents) const
{
	bool bMatchesSearchQuery = false;
	for(const TPair< FindInBlueprintsHelpers::FSimpleFTextKeyStorage, FSearchableValueInfo >& ParsedValues : ParsedTagsAndValues )
	{
		if (ParsedValues.Value.IsSearchable() && !ParsedValues.Value.IsExplicitSearchable())
		{
			FText Value = ParsedValues.Value.GetDisplayText(*LookupTablePtr);
			FString ValueAsString = Value.ToString();
			ValueAsString.ReplaceInline(TEXT(" "), TEXT(""));
			bool bMatchesSearch = TextFilterUtils::TestBasicStringExpression(ValueAsString, InValue, InTextComparisonMode) || TextFilterUtils::TestBasicStringExpression(Value.BuildSourceString(), InValue, InTextComparisonMode);
			
			if (bMatchesSearch && !ParsedValues.Value.IsCoreDisplay())
			{
				FSearchResult SearchResult(new FFindInBlueprintsResult(CreateSearchComponentDisplayText(ParsedValues.Value.GetDisplayKey(), Value), nullptr ));
				InOutMatchingSearchComponents.Add(this, FComponentUniqueDisplay(SearchResult));
			}

			bMatchesSearchQuery |= bMatchesSearch;
		}
	}
	// Any children that are treated as a TagAndValue Category should be added for independent searching
	for (const TSharedPtr< FImaginaryFiBData > Child : ParsedChildData)
	{
		if (Child->IsTagAndValueCategory())
		{
			bMatchesSearchQuery |= Child->TestBasicStringExpression(InValue, InTextComparisonMode, InOutMatchingSearchComponents);
		}
	}

	return bMatchesSearchQuery;
}

bool FImaginaryFiBData::TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode, TMultiMap< const FImaginaryFiBData*, FComponentUniqueDisplay >& InOutMatchingSearchComponents) const
{
	bool bMatchesSearchQuery = false;
	for (const TPair< FindInBlueprintsHelpers::FSimpleFTextKeyStorage, FSearchableValueInfo >& TagsValuePair : ParsedTagsAndValues)
	{
		if (TagsValuePair.Value.IsSearchable())
		{
			if (TagsValuePair.Key.Text.ToString() == InKey.ToString() || TagsValuePair.Key.Text.BuildSourceString() == InKey.ToString())
			{
				FText Value = TagsValuePair.Value.GetDisplayText(*LookupTablePtr);
				FString ValueAsString = Value.ToString();
				ValueAsString.ReplaceInline(TEXT(" "), TEXT(""));
				bool bMatchesSearch = TextFilterUtils::TestComplexExpression(ValueAsString, InValue, InComparisonOperation, InTextComparisonMode) || TextFilterUtils::TestBasicStringExpression(Value.BuildSourceString(), InValue, InTextComparisonMode);

				if (bMatchesSearch && !TagsValuePair.Value.IsCoreDisplay())
				{
					FSearchResult SearchResult(new FFindInBlueprintsResult(CreateSearchComponentDisplayText(TagsValuePair.Value.GetDisplayKey(), Value), nullptr));
					InOutMatchingSearchComponents.Add(this, FComponentUniqueDisplay(SearchResult));
				}
				bMatchesSearchQuery |= bMatchesSearch;
			}
		}
	}

	// Any children that are treated as a TagAndValue Category should be added for independent searching
	for (const TSharedPtr< FImaginaryFiBData > Child : ParsedChildData)
	{
		if (Child->IsTagAndValueCategory())
		{
			bMatchesSearchQuery |= Child->TestComplexExpression(InKey, InValue, InComparisonOperation, InTextComparisonMode, InOutMatchingSearchComponents);
		}
	}
	return bMatchesSearchQuery;
}

UObject* FImaginaryFiBData::GetObject(UBlueprint* InBlueprint) const
{
	return CreateSearchResult(nullptr)->GetObject(InBlueprint);
}

///////////////////////////
// FFiBMetaData

FFiBMetaData::FFiBMetaData(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
	, bIsHidden(false)
	, bIsExplicit(false)
{
}

bool FFiBMetaData::TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue)
{
	bool bResult = false;
	if (InKey.ToString() == FFiBMD::FiBSearchableExplicitMD)
	{
		bIsExplicit = true;
		bResult = true;
	}
	else if (InKey.ToString() == FFiBMD::FiBSearchableHiddenExplicitMD)
	{
		bIsExplicit = true;
		bIsHidden = true;
		bResult = true;
	}
	ensure(bResult);
	return bResult;
}

///////////////////////////
// FCategorySectionHelper

FCategorySectionHelper::FCategorySectionHelper(TWeakPtr<FImaginaryFiBData> InOuter, TMap<int32, FText>* InLookupTablePtr, FText InCategoryName, bool bInTagAndValueCategory)
	: FImaginaryFiBData(InOuter, nullptr, InLookupTablePtr)
	, CategoryName(InCategoryName)
	, bIsTagAndValue(bInTagAndValueCategory)
{
}

FCategorySectionHelper::FCategorySectionHelper(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, FText InCategoryName, bool bInTagAndValueCategory)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
	, CategoryName(InCategoryName)
	, bIsTagAndValue(bInTagAndValueCategory)
{
}

FCategorySectionHelper::FCategorySectionHelper(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, FText InCategoryName, bool bInTagAndValueCategory, FCategorySectionHelperCallback InSpecialHandlingCallback)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
	, SpecialHandlingCallback(InSpecialHandlingCallback)
	, CategoryName(InCategoryName)
	, bIsTagAndValue(bInTagAndValueCategory)
{

}

bool FCategorySectionHelper::CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return true;
}

FSearchResult FCategorySectionHelper::CreateSearchResult_Internal(FSearchResult InParent) const
{
	return FSearchResult(new FFindInBlueprintsResult(CategoryName, InParent));
}

void FCategorySectionHelper::ParseAllChildData_Internal(ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::Searchable*/)
{
	if (UnparsedJsonObject.IsValid() && SpecialHandlingCallback.IsBound())
	{
		SpecialHandlingCallback.Execute(UnparsedJsonObject, ParsedChildData);
		UnparsedJsonObject.Reset();
	}
	else
	{
		bool bHasMetaData = false;
		bool bHasOneOtherItem = false;
		if (UnparsedJsonObject.IsValid() && UnparsedJsonObject->Values.Num() == 2)
		{
			for( auto MapValues : UnparsedJsonObject->Values )
			{
				FText KeyText = FindInBlueprintsHelpers::AsFText(FCString::Atoi(*MapValues.Key), *LookupTablePtr);
				if (!KeyText.CompareTo(FFindInBlueprintSearchTags::FiBMetaDataTag))
				{
					bHasMetaData = true;
				}
				else
				{
					bHasOneOtherItem = true;
				}
			}

			// If we have metadata and only one other item, we should be treated like a tag and value category
			bIsTagAndValue |= (bHasOneOtherItem && bHasMetaData);
		}

		FImaginaryFiBData::ParseAllChildData_Internal(InSearchabilityOverride);
	}
}

//////////////////////////////////////////
// FImaginaryBlueprint

FImaginaryBlueprint::FImaginaryBlueprint(FString InBlueprintName, FString InBlueprintPath, FString InBlueprintParentClass, TArray<FString>& InInterfaces, FString InUnparsedStringData, bool bInIsVersioned/*=true*/)
	: FImaginaryFiBData(nullptr)
	, BlueprintPath(InBlueprintPath)
	, UnparsedStringData(InUnparsedStringData)
{
	ParseToJson(bInIsVersioned);
	LookupTablePtr = &LookupTable;
	ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(FFindInBlueprintSearchTags::FiB_Name), FSearchableValueInfo(FFindInBlueprintSearchTags::FiB_Name, FText::FromString(InBlueprintName), ESearchableValueStatus::ExplicitySearchable));
	ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(FFindInBlueprintSearchTags::FiB_Path), FSearchableValueInfo(FFindInBlueprintSearchTags::FiB_Path, FText::FromString(InBlueprintPath), ESearchableValueStatus::ExplicitySearchable));
	ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(FFindInBlueprintSearchTags::FiB_ParentClass), FSearchableValueInfo(FFindInBlueprintSearchTags::FiB_ParentClass, FText::FromString(InBlueprintParentClass), ESearchableValueStatus::ExplicitySearchable));

	TSharedPtr< FCategorySectionHelper > InterfaceCategory = MakeShareable(new FCategorySectionHelper(nullptr, &LookupTable, FFindInBlueprintSearchTags::FiB_Interfaces, true));
	for( int32 InterfaceIdx = 0; InterfaceIdx < InInterfaces.Num(); ++InterfaceIdx)
	{
		FString& Interface = InInterfaces[InterfaceIdx];
		FText Key = FText::FromString(FString::FromInt(InterfaceIdx));
		FSearchableValueInfo Value(Key, FText::FromString(Interface), ESearchableValueStatus::ExplicitySearchable);
		InterfaceCategory->AddKeyValuePair(FFindInBlueprintSearchTags::FiB_Interfaces, Value);
	}		
	ParsedChildData.Add(InterfaceCategory);
}

FSearchResult FImaginaryBlueprint::CreateSearchResult_Internal(FSearchResult InParent) const
{
	return FSearchResult(new FFindInBlueprintsResult(ParsedTagsAndValues.Find(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(FFindInBlueprintSearchTags::FiB_Path))->GetDisplayText(LookupTable)));
}

UBlueprint* FImaginaryBlueprint::GetBlueprint() const
{
	return Cast<UBlueprint>(GetObject(nullptr));
}

bool FImaginaryBlueprint::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::AllFilter || InSearchQueryFilter == ESearchQueryFilter::BlueprintFilter;
}

bool FImaginaryBlueprint::CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::NodesFilter ||
		InSearchQueryFilter == ESearchQueryFilter::PinsFilter ||
		InSearchQueryFilter == ESearchQueryFilter::GraphsFilter ||
		InSearchQueryFilter == ESearchQueryFilter::UberGraphsFilter ||
		InSearchQueryFilter == ESearchQueryFilter::FunctionsFilter ||
		InSearchQueryFilter == ESearchQueryFilter::MacrosFilter ||
		InSearchQueryFilter == ESearchQueryFilter::PropertiesFilter ||
		InSearchQueryFilter == ESearchQueryFilter::VariablesFilter ||
		InSearchQueryFilter == ESearchQueryFilter::ComponentsFilter ||
		FImaginaryFiBData::CanCallFilter(InSearchQueryFilter);
}

void FImaginaryBlueprint::ParseToJson(bool bInIsVersioned)
{
	UnparsedJsonObject = FFindInBlueprintSearchManager::ConvertJsonStringToObject(bInIsVersioned, UnparsedStringData, LookupTable);
}

bool FImaginaryBlueprint::TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue)
{
	bool bResult = false;

	if(!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Properties))
	{
		// Pulls out all properties (variables) for this Blueprint
		TArray<TSharedPtr< FJsonValue > > PropertyList = InJsonValue->AsArray();
		for( TSharedPtr< FJsonValue > PropertyValue : PropertyList )
		{
			ParsedChildData.Add(MakeShareable(new FImaginaryProperty(AsShared(), PropertyValue->AsObject(), &LookupTable)));
		}
		bResult = true;
	}
	else if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Functions))
	{
		ParseGraph(InJsonValue, FFindInBlueprintSearchTags::FiB_Functions.ToString(), GT_Function);
		bResult = true;
	}
	else if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Macros))
	{
		ParseGraph(InJsonValue, FFindInBlueprintSearchTags::FiB_Macros.ToString(), GT_Macro);
		bResult = true;
	}
	else if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_UberGraphs))
	{
		ParseGraph(InJsonValue, FFindInBlueprintSearchTags::FiB_UberGraphs.ToString(), GT_Ubergraph);
		bResult = true;
	}
	else if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_SubGraphs))
	{
		ParseGraph(InJsonValue, FFindInBlueprintSearchTags::FiB_SubGraphs.ToString(), GT_Ubergraph);
		bResult = true;
	}
	else if(!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Components))
	{
		TArray<TSharedPtr< FJsonValue > > ComponentList = InJsonValue->AsArray();
		TSharedPtr< FJsonObject > ComponentsWrapperObject(new FJsonObject);
		ComponentsWrapperObject->Values.Add(FFindInBlueprintSearchTags::FiB_Components.ToString(), InJsonValue);
		ParsedChildData.Add(MakeShareable(new FCategorySectionHelper(AsShared(), ComponentsWrapperObject, &LookupTable, FFindInBlueprintSearchTags::FiB_Components, false, FCategorySectionHelper::FCategorySectionHelperCallback::CreateRaw(this, &FImaginaryBlueprint::ParseComponents))));
		bResult = true;
	}

	if (!bResult)
	{
		bResult = FImaginaryFiBData::TrySpecialHandleJsonValue(InKey, InJsonValue);
	}
	return bResult;
}

void FImaginaryBlueprint::ParseGraph( TSharedPtr< FJsonValue > InJsonValue, FString InCategoryTitle, EGraphType InGraphType )
{
	TArray<TSharedPtr< FJsonValue > > GraphList = InJsonValue->AsArray();
	for( TSharedPtr< FJsonValue > GraphValue : GraphList )
	{
		ParsedChildData.Add(MakeShareable(new FImaginaryGraph(AsShared(), GraphValue->AsObject(), &LookupTable, InGraphType)));
	}
}

void FImaginaryBlueprint::ParseComponents(TSharedPtr< FJsonObject > InJsonObject, TArray< TSharedPtr<FImaginaryFiBData> >& OutParsedChildData)
{
	// Pulls out all properties (variables) for this Blueprint
	TArray<TSharedPtr< FJsonValue > > ComponentList = InJsonObject->GetArrayField(FFindInBlueprintSearchTags::FiB_Components.ToString());
	for( TSharedPtr< FJsonValue > ComponentValue : ComponentList )
	{
		OutParsedChildData.Add(MakeShareable(new FImaginaryComponent(AsShared(), ComponentValue->AsObject(), &LookupTable)));
	}
}

//////////////////////////
// FImaginaryGraph

FImaginaryGraph::FImaginaryGraph(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, EGraphType InGraphType)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
	, GraphType(InGraphType)
{
}

FSearchResult FImaginaryGraph::CreateSearchResult_Internal(FSearchResult InParent) const
{
	return FSearchResult(new FFindInBlueprintsGraph(FText::GetEmpty(), InParent, GraphType));
}

bool FImaginaryGraph::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::AllFilter || 
		InSearchQueryFilter == ESearchQueryFilter::GraphsFilter ||
		(GraphType == GT_Ubergraph && InSearchQueryFilter == ESearchQueryFilter::UberGraphsFilter) ||
		(GraphType == GT_Function && InSearchQueryFilter == ESearchQueryFilter::FunctionsFilter) ||
		(GraphType == GT_Macro && InSearchQueryFilter == ESearchQueryFilter::MacrosFilter);
}

bool FImaginaryGraph::CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::PinsFilter ||
		InSearchQueryFilter == ESearchQueryFilter::NodesFilter ||
		(GraphType == GT_Function && InSearchQueryFilter == ESearchQueryFilter::PropertiesFilter) ||
		(GraphType == GT_Function && InSearchQueryFilter == ESearchQueryFilter::VariablesFilter) ||
		FImaginaryFiBData::CanCallFilter(InSearchQueryFilter);
}

ESearchableValueStatus FImaginaryGraph::GetSearchabilityStatus(FString InKey)
{
	// This is a non-ideal way to assign searchability vs being a core display item and will be resolved in future versions of the FiB data in the AR
	if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_Name, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_NativeName, InKey)
		)
	{
		return ESearchableValueStatus::CoreDisplayItem;
	}

	return ESearchableValueStatus::Searchable;
}

bool FImaginaryGraph::TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue)
{
	if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Nodes))
	{
		TArray< TSharedPtr< FJsonValue > > NodeList = InJsonValue->AsArray();

		for( TSharedPtr< FJsonValue > NodeValue : NodeList )
		{
			ParsedChildData.Add(MakeShareable(new FImaginaryGraphNode(AsShared(), NodeValue->AsObject(), LookupTablePtr)));
		}
		return true;
	}
	else if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Properties))
	{
		// Pulls out all properties (local variables) for this graph
		TArray<TSharedPtr< FJsonValue > > PropertyList = InJsonValue->AsArray();
		for( TSharedPtr< FJsonValue > PropertyValue : PropertyList )
		{
			ParsedChildData.Add(MakeShareable(new FImaginaryProperty(AsShared(), PropertyValue->AsObject(), LookupTablePtr)));
		}
		return true;
	}
	return false;
}

//////////////////////////////////////
// FImaginaryGraphNode

FImaginaryGraphNode::FImaginaryGraphNode(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
{
}

FSearchResult FImaginaryGraphNode::CreateSearchResult_Internal(FSearchResult InParent) const
{
	return FSearchResult(new FFindInBlueprintsGraphNode(FText::GetEmpty(), InParent));
}

bool FImaginaryGraphNode::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::AllFilter || InSearchQueryFilter == ESearchQueryFilter::NodesFilter;
}

bool FImaginaryGraphNode::CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::PinsFilter ||
		FImaginaryFiBData::CanCallFilter(InSearchQueryFilter);
}

ESearchableValueStatus FImaginaryGraphNode::GetSearchabilityStatus(FString InKey)
{
	// This is a non-ideal way to assign searchability vs being a core display item and will be resolved in future versions of the FiB data in the AR
	if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_Name, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_NativeName, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_Comment, InKey)
		)
	{
		return ESearchableValueStatus::CoreDisplayItem;
	}
	else if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_Glyph, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_GlyphStyleSet, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_GlyphColor, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_NodeGuid, InKey)
		)
	{
		return ESearchableValueStatus::NotSearchable;
	}
	else if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_ClassName, InKey))
	{
		return ESearchableValueStatus::ExplicitySearchable;
	}
	return ESearchableValueStatus::Searchable;
}

bool FImaginaryGraphNode::TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue)
{
	if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_Pins))
	{
		TArray< TSharedPtr< FJsonValue > > PinsList = InJsonValue->AsArray();

		for( TSharedPtr< FJsonValue > Pin : PinsList )
		{
			ParsedChildData.Add(MakeShareable(new FImaginaryPin(AsShared(), Pin->AsObject(), LookupTablePtr, SchemaName)));
		}
		return true;
	}
	else if (!InKey.CompareTo(FFindInBlueprintSearchTags::FiB_SchemaName))
	{
		// Previously extracted
		return true;
	}

	return false;
}

void FImaginaryGraphNode::ParseAllChildData_Internal(ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::Searchable*/)
{
	if (UnparsedJsonObject.IsValid())
	{
		TSharedPtr< FJsonObject > JsonObject = UnparsedJsonObject;
		// Very important to get the schema first, other bits of data depend on it
		for (auto MapValues : UnparsedJsonObject->Values)
		{
			FText KeyText = FindInBlueprintsHelpers::AsFText(FCString::Atoi(*MapValues.Key), *LookupTablePtr);
			if (!KeyText.CompareTo(FFindInBlueprintSearchTags::FiB_SchemaName))
			{
				TSharedPtr< FJsonValue > SchemaNameValue = MapValues.Value;
				SchemaName = FindInBlueprintsHelpers::AsFText(SchemaNameValue, *LookupTablePtr).ToString();
				break;
			}
		}

		FImaginaryFiBData::ParseAllChildData_Internal(InSearchabilityOverride);
	}
}

///////////////////////////////////////////
// FImaginaryProperty

FImaginaryProperty::FImaginaryProperty(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
{
}

bool FImaginaryProperty::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::AllFilter || 
		InSearchQueryFilter == ESearchQueryFilter::PropertiesFilter || 
		InSearchQueryFilter == ESearchQueryFilter::VariablesFilter;
}

FSearchResult FImaginaryProperty::CreateSearchResult_Internal(FSearchResult InParent) const
{
	return FSearchResult(new FFindInBlueprintsProperty(FText::GetEmpty(), InParent));
}

ESearchableValueStatus FImaginaryProperty::GetSearchabilityStatus(FString InKey)
{
	// This is a non-ideal way to assign searchability vs being a core display item and will be resolved in future versions of the FiB data in the AR
	if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_Name, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_NativeName, InKey)
		)
	{
		return ESearchableValueStatus::CoreDisplayItem;
	}
	else if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_PinCategory, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_PinSubCategory, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_ObjectClass, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_IsArray, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_IsReference, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_IsSCSComponent, InKey)
		)
	{
		return ESearchableValueStatus::ExplicitySearchableHidden;
	}
	return ESearchableValueStatus::Searchable;
}

//////////////////////////////
// FImaginaryComponent

FImaginaryComponent::FImaginaryComponent(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr)
	: FImaginaryProperty(InOuter, InUnparsedJsonObject, InLookupTablePtr)
{
}

bool FImaginaryComponent::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return FImaginaryProperty::IsCompatibleWithFilter(InSearchQueryFilter) || InSearchQueryFilter == ESearchQueryFilter::ComponentsFilter;
}

//////////////////////////////
// FImaginaryPin

FImaginaryPin::FImaginaryPin(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, FString InSchemaName)
	: FImaginaryFiBData(InOuter, InUnparsedJsonObject, InLookupTablePtr)
	, SchemaName(InSchemaName)
{
}

bool FImaginaryPin::IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const
{
	return InSearchQueryFilter == ESearchQueryFilter::AllFilter || InSearchQueryFilter == ESearchQueryFilter::PinsFilter;
}

FSearchResult FImaginaryPin::CreateSearchResult_Internal(FSearchResult InParent) const
{
	return FSearchResult(new FFindInBlueprintsPin(FText::GetEmpty(), InParent, SchemaName));
}

ESearchableValueStatus FImaginaryPin::GetSearchabilityStatus(FString InKey)
{
	if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_Name, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_NativeName, InKey)
		)
	{
		return ESearchableValueStatus::CoreDisplayItem;
	}
	else if (FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_PinCategory, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_PinSubCategory, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_ObjectClass, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_IsArray, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_IsReference, InKey)
		|| FindInBlueprintsHelpers::IsTextEqualToString(FFindInBlueprintSearchTags::FiB_IsSCSComponent, InKey)
		)
	{
		return ESearchableValueStatus::ExplicitySearchableHidden;
	}
	return ESearchableValueStatus::Searchable;
}

bool FImaginaryPin::TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue)
{
	return false;
}

#undef LOCTEXT_NAMESPACE
