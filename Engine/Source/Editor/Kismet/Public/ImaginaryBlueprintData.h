// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/TextFilterExpressionEvaluator.h"
#include "FindInBlueprints.h"

enum ESearchableValueStatus
{
	NotSearchable = 0x00000000, // Cannot search this value, it is used for display purposes only
	Searchable = 0x00000001, // Generically searchable, value will appear as a sub-item and has no sub-data
	Hidden = 0x00000002, // Item will not be shown
	Explicit = 0x00000004, // Item must be explicitly requested via the tag

	
	CoreDisplayItem = Hidden | Searchable, // Core display items are searchable but should not display as sub-items because their data is presented in another fashion.
	ExplicitySearchable = Explicit | Searchable, // Will only be allowed to be found if searching using a tag
	ExplicitySearchableHidden = Explicit | Searchable | Hidden, // Will only be allowed to be found if searching using a tag but will not display the tag in the results (because it is a CoreDisplayItem)
	AllSearchable = CoreDisplayItem | ExplicitySearchable, // Covers all searchability methods
};

class FSearchableValueInfo
{
public:
	FSearchableValueInfo(FText InDisplayKey, int32 InLookUpTableKey)
		: SearchableValueStatus(ESearchableValueStatus::Searchable)
		, DisplayKey(InDisplayKey)
		, LookupTableKey(InLookUpTableKey)
	{
	}

	FSearchableValueInfo(FText InDisplayKey, FText InDisplayText)
		: SearchableValueStatus(ESearchableValueStatus::Searchable)
		, DisplayKey(InDisplayKey)
		, LookupTableKey(-1)
		, DisplayText(InDisplayText)
	{

	}

	FSearchableValueInfo(FText InDisplayKey, int32 InLookUpTableKey, ESearchableValueStatus InSearchableValueStatus)
		: SearchableValueStatus(InSearchableValueStatus)
		, DisplayKey(InDisplayKey)
		, LookupTableKey(InLookUpTableKey)
	{

	}

	FSearchableValueInfo(FText InDisplayKey, FText InDisplayText, ESearchableValueStatus InSearchableValueStatus)
		: SearchableValueStatus(InSearchableValueStatus)
		, DisplayKey(InDisplayKey)
		, LookupTableKey(-1)
		, DisplayText(InDisplayText)
	{

	}

	/** Returns TRUE if the data is searchable */
	bool IsSearchable() const { return (SearchableValueStatus & ESearchableValueStatus::Searchable) != 0; }

	/** Returns TRUE if the item should be treated as a CoreDisplayItem, which is searchable but not displayed */
	bool IsCoreDisplay() const { return (SearchableValueStatus & ESearchableValueStatus::CoreDisplayItem) == ESearchableValueStatus::CoreDisplayItem; }

	/** Returns TRUE if the item should only be searchable if explicitly searched for using the tag */
	bool IsExplicitSearchable() const { return (SearchableValueStatus & ESearchableValueStatus::ExplicitySearchable) == ESearchableValueStatus::ExplicitySearchable; }

	/** Returns the display text to use for this item */
	FText GetDisplayText(const TMap<int32, FText>& InLookupTable) const;

	/** Returns the display key for this item */
	FText GetDisplayKey() const
	{
		return DisplayKey;
	}

protected:
	/** The searchability status of this item */
	ESearchableValueStatus SearchableValueStatus;

	/** Key that this item is associated with, used for display purposes */
	FText DisplayKey;

	/** Key to use to lookup into a table if DisplayText does not override */
	int32 LookupTableKey;

	/** Text value to use instead of a lookup into the table */
	FText DisplayText;
};

/** Struct to contain search results and help compare them for uniqueness. */
struct FComponentUniqueDisplay
{
	FComponentUniqueDisplay( FSearchResult InSearchResult )
		: SearchResult(InSearchResult)
	{}

	bool operator==(const FComponentUniqueDisplay& Other);

	/** Search result contained and used for comparing of uniqueness */
	FSearchResult SearchResult;
};

class FImaginaryFiBData : public ITextFilterExpressionContext, public TSharedFromThis<FImaginaryFiBData>
{
public:
	FImaginaryFiBData(TWeakPtr<FImaginaryFiBData> InOuter);
	FImaginaryFiBData(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr);

	/** ITextFilterExpressionContext Interface */
	// We don't actually use these overrides, see FFiBContextHelper for how we call the alternate functions. These will assert if they are accidentally called.
	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override { ensure(0); return false; };
	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override { ensure(0); return false; };
	/** End ITextFilterExpressionContext Interface */

	/** Returns TRUE if this item is a category type, which helps to organize child data */
	virtual bool IsCategory() const { return false; }

	/** Returns TRUE if this item is considered a Tag and Value category, where it's contents should be considered no different than the parent owner */
	virtual bool IsTagAndValueCategory() { return false; }

	/** Checks if the filter is compatible with the current object, returns TRUE by default */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const;

	/** Checks if the filter can call functions for the passed filter, returns FALSE by default if the filter is not the AllFilter */
	virtual bool CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const;

	/** Parses, in a thread-safe manner, all child data, non-recursively, so children will be left in an unparsed Json state */
	void ParseAllChildData(ESearchableValueStatus InSearchabilityOverride = ESearchableValueStatus::Searchable);

	/** Test the given value against the strings extracted from the current item. Will return the matching search components if any (can return TRUE without having any if the search components are hidden) */
	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode, TMultiMap< const FImaginaryFiBData*, FComponentUniqueDisplay >& InOutMatchingSearchComponents) const;

	/** Perform a complex expression test for the current item. Will return the matching search components if any (can return TRUE without having any if the search components are hidden) */
	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode, TMultiMap< const FImaginaryFiBData*, FComponentUniqueDisplay >& InOutMatchingSearchComponents) const;

	/** Returns the UObject represented by this Imaginary data give the UBlueprint owner. */
	virtual UObject* GetObject(UBlueprint* InBlueprint) const;

	/** This will return and force load the UBlueprint that owns this object data. */
	virtual UBlueprint* GetBlueprint() const
	{
		if (Outer.IsValid())
		{
			return Outer.Pin()->GetBlueprint();
		}
		return nullptr;
	}

	/** Requests internal creation of the search result and properly initializes the visual representation of the result */
	FSearchResult CreateSearchResult(FSearchResult InParent) const;

	/** Accessor for the parsed child data for this item */
	const TArray< TSharedPtr<FImaginaryFiBData> >& GetAllParsedChildData() const
	{
		return ParsedChildData;
	}

	/** Adds a KeyValue pair to the ParsedTagAndValues map */
	void AddKeyValuePair(FText InKey, FSearchableValueInfo& InValue)
	{
		ParsedTagsAndValues.Add(FindInBlueprintsHelpers::FSimpleFTextKeyStorage(InKey), InValue);
	}

	/** Returns the Outer of this Imaginary data that directly owns it */
	TWeakPtr< FImaginaryFiBData > GetOuter() const
	{
		return Outer;
	}

	/** Builds a SearchTree ready to be displayed in the Find-in-Blueprints window */
	static FSearchResult CreateSearchTree(FSearchResult InParentSearchResult, TWeakPtr< FImaginaryFiBData > InCurrentPointer, TArray< const FImaginaryFiBData* >& InValidSearchResults, TMultiMap< const FImaginaryFiBData*, FComponentUniqueDisplay >& InMatchingSearchComponents);

protected:
	/**
	 * Checks if the Key has any special handling to be done, such as making a Pin out of it
	 *
	 * @param InKey			Key that the JsonValue was stored under
	 * @param InJsonValue	JsonValue to be specially parsed
	 * @return				TRUE if the JsonValue was specially handled, will not be further handled
	 */
	virtual bool TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue) { return false; }

	/** Returns the searchability status of a passed in Key, all Keys are searchable by default */
	virtual ESearchableValueStatus GetSearchabilityStatus(FString InKey) { return ESearchableValueStatus::Searchable; };

	/** Protected internal function which builds the search result for this item */
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const = 0;

	/** Creates a display string for this item in search results */
	FText CreateSearchComponentDisplayText(FText InKey, FText InValue) const;

	/** Helper function for parsing Json values into usable properties */
	void ParseJsonValue(FText InKey, FText InDisplayKey, TSharedPtr< FJsonValue > InJsonValue, bool bIsInArray=false, ESearchableValueStatus InSearchabilityOverride = ESearchableValueStatus::Searchable);

	/** Internal version of the ParseAllChildData function, handles the bulk of the work */
	virtual void ParseAllChildData_Internal(ESearchableValueStatus InSearchabilityOverride = ESearchableValueStatus::Searchable);

protected:
	/** The unparsed Json object representing this item. Auto-cleared after parsing */
	TSharedPtr< FJsonObject > UnparsedJsonObject;

	/** All parsed child data for this item */
	TArray< TSharedPtr<FImaginaryFiBData> > ParsedChildData;

	/** A mapping of tags to their values and searchability status */
	TMultiMap< FindInBlueprintsHelpers::FSimpleFTextKeyStorage, FSearchableValueInfo > ParsedTagsAndValues;

	/** Pointer to the lookup table to decompressed the Json strings back into fully formed FTexts */
	TMap<int32, FText>* LookupTablePtr;

	/** Outer of this object that owns it, used for climbing up the hierarchy */
	TWeakPtr<FImaginaryFiBData> Outer;

	/** Allows for thread-safe parsing of the imaginary data. Only a single Imaginary data can be parsed at a time */
	static FCriticalSection ParseChildDataCriticalSection;
};

class FFiBMetaData : public FImaginaryFiBData
{
public:
	FFiBMetaData(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr);
	
	/** FImaginaryFiBData Interface */
	virtual bool TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue) override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override
	{
		return nullptr;
	}
	/** End FImaginaryFiBData Interface */

	/** Returns TRUE if the metadata is informing that the UProperty and children should be hidden */
	bool IsHidden() const
	{
		// While handled separately, when hidden it should always be explicit
		ensure(!bIsHidden || (bIsHidden && bIsExplicit));
		return bIsHidden;
	}

	/** Returns TRUE if the metadata is informing that the UProperty and children should be explicit */
	bool IsExplicit() const
	{
		return bIsExplicit;
	}
protected:
	/** TRUE if the UProperty this metadata represents is hidden */
	bool bIsHidden;

	/** TRUE if the UProperty this metadata represents is explicit, should always be true if bIsHidden is true */
	bool bIsExplicit;
};

class FCategorySectionHelper : public FImaginaryFiBData
{
public:
	/** Callback declaration for handling special parsing of the items in the category */
	DECLARE_DELEGATE_TwoParams(FCategorySectionHelperCallback, TSharedPtr< FJsonObject >, TArray< TSharedPtr<FImaginaryFiBData> >&);

	FCategorySectionHelper(TWeakPtr<FImaginaryFiBData> InOuter, TMap<int32, FText>* InLookupTablePtr, FText InCategoryName, bool bInTagAndValueCategory);
	FCategorySectionHelper(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, FText InCategoryName, bool bInTagAndValueCategory);
	FCategorySectionHelper(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, FText InCategoryName, bool bInTagAndValueCategory, FCategorySectionHelperCallback InSpecialHandlingCallback);

	/** FImaginaryFiBData Interface */
	virtual void ParseAllChildData_Internal(ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::ESearchableValueStatus::Searchable*/) override;
	virtual bool IsCategory() const override { return true; }
	virtual bool IsTagAndValueCategory() override { return bIsTagAndValue; }
	/** End FImaginaryFiBData Interface */

	/** Returns the category name prepared for checking as a function */
	FString GetCategoryFunctionName() const
	{
		return CategoryName.BuildSourceString();
	}

protected:
	/** FImaginaryFiBData Interface */
	virtual bool CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** Callback to specially handle parsing of the Json Object instead of using generic handling */
	FCategorySectionHelperCallback SpecialHandlingCallback;

	/** The display text for this item in the search results */
	FText CategoryName;

	/** TRUE if this category should be considered no different than a normal Tag and Value in it's parent */
	bool bIsTagAndValue;
};

/** An "imaginary" representation of a UBlueprint, featuring raw strings or other imaginary objects in the place of more structured substances */
class FImaginaryBlueprint : public FImaginaryFiBData
{
public:
	FImaginaryBlueprint(FString InBlueprintName, FString InBlueprintPath, FString InBlueprintParentClass, TArray<FString>& InInterfaces, FString InUnparsedStringData, bool bInIsVersioned = true);

	/** FImaginaryFiBData Interface */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	virtual bool CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	virtual UBlueprint* GetBlueprint() const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** FImaginaryFiBData Interface */
	virtual bool TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue) override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override;
	/** End FImaginaryFiBData Interface */

	/** Helper function to parse an array of Json Object representing graphs */
	void ParseGraph( TSharedPtr< FJsonValue > InJsonValue, FString InCategoryTitle, EGraphType InGraphType );
	
	/** Callback to specially parse an array of Json Objects representing components */
	void ParseComponents(TSharedPtr< FJsonObject > InJsonObject, TArray< TSharedPtr<FImaginaryFiBData> >& OutParsedChildData);

	/** Parses a raw string of Json to a Json object hierarchy */
	void ParseToJson(bool bInIsVersioned);

protected:
	/** The path for this Blueprint */
	FString BlueprintPath;

	/** The raw Json string yet to be parsed */
	FString UnparsedStringData;

	/** Lookup table used as a compression tool for the FTexts stored in the Json object */
	TMap<int32, FText> LookupTable;
};

/** An "imaginary" representation of a UEdGraph, featuring raw strings or other imaginary objects in the place of more structured substances */
class FImaginaryGraph : public FImaginaryFiBData
{
public:
	FImaginaryGraph(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, EGraphType InGraphType);

	/** FImaginaryFiBData Interface */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	virtual bool CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** FImaginaryFiBData Interface */
	virtual bool TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue) override;
	virtual ESearchableValueStatus GetSearchabilityStatus(FString InKey) override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** The graph type being represented */
	EGraphType GraphType;
};

/** An "imaginary" representation of a UEdGraphNode, featuring raw strings or other imaginary objects in the place of more structured substances */
class FImaginaryGraphNode : public FImaginaryFiBData
{
public:
	FImaginaryGraphNode(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr);

	/** FImaginaryFiBData Interface */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	virtual bool CanCallFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	virtual void ParseAllChildData_Internal(ESearchableValueStatus InSearchabilityOverride/* = ESearchableValueStatus::Searchable*/) override;
	/** End FImaginaryFiBData Interface */

protected:
	/** FImaginaryFiBData Interface */
	virtual bool TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue) override;
	virtual ESearchableValueStatus GetSearchabilityStatus(FString InKey) override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** Schema name that manages this node */
	FString SchemaName;
};

/** An "imaginary" representation of a UProperty, featuring raw strings or other imaginary objects in the place of more structured substances */
class FImaginaryProperty : public FImaginaryFiBData
{
public:
	FImaginaryProperty(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr);

	/** FImaginaryFiBData Interface */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	/** End FImaginaryFiBData Interface */
	
protected:
	/** FImaginaryFiBData Interface */
	virtual ESearchableValueStatus GetSearchabilityStatus(FString InKey) override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override;
	/** End FImaginaryFiBData Interface */
};

/** An "imaginary" representation of a UProperty of an instanced component, featuring raw strings or other imaginary objects in the place of more structured substances */
class FImaginaryComponent : public FImaginaryProperty
{
public:
	FImaginaryComponent(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr);

	/** FImaginaryFiBData Interface */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	/** End FImaginaryFiBData Interface */
};

/** An "imaginary" representation of a UEdGraphPin, featuring raw strings or other imaginary objects in the place of more structured substances */
class FImaginaryPin : public FImaginaryFiBData
{
public:
	FImaginaryPin(TWeakPtr<FImaginaryFiBData> InOuter, TSharedPtr< FJsonObject > InUnparsedJsonObject, TMap<int32, FText>* InLookupTablePtr, FString InSchemaName);

	/** FImaginaryFiBData Interface */
	virtual bool IsCompatibleWithFilter(ESearchQueryFilter InSearchQueryFilter) const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** FImaginaryFiBData Interface */
	virtual bool TrySpecialHandleJsonValue(FText InKey, TSharedPtr< FJsonValue > InJsonValue);
	virtual ESearchableValueStatus GetSearchabilityStatus(FString InKey) override;
	virtual FSearchResult CreateSearchResult_Internal(FSearchResult InParent) const override;
	/** End FImaginaryFiBData Interface */

protected:
	/** Schema name that manages this pin */
	FString SchemaName;
};
