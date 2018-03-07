// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FComponentClassComboEntry;
class SToolTip;

typedef TSharedPtr<class FComponentClassComboEntry> FComponentClassComboEntryPtr;

//////////////////////////////////////////////////////////////////////////

namespace EComponentCreateAction
{
	enum Type
	{
		/** Create a new C++ class based off the specified ActorComponent class and then add it to the tree */
		CreateNewCPPClass,
		/** Create a new blueprint class based off the specified ActorComponent class and then add it to the tree */
		CreateNewBlueprintClass,
		/** Spawn a new instance of the specified ActorComponent class and then add it to the tree */
		SpawnExistingClass,
	};
}


DECLARE_DELEGATE_OneParam(FOnComponentCreated, UActorComponent*);

DECLARE_DELEGATE_RetVal_ThreeParams( UActorComponent*, FComponentClassSelected, TSubclassOf<UActorComponent>, EComponentCreateAction::Type, UObject*);

struct FComponentEntryCustomizationArgs
{
	/** Specific asset to use instead of the selected asset in the content browser */
	TWeakObjectPtr<UObject> AssetOverride;
	/** Custom name to display */
	FString ComponentNameOverride;
	/** Callback when a new component is created */
	FOnComponentCreated OnComponentCreated;
	/** Brush icon to use instead of the class icon */
	FName IconOverrideBrushName;
	/** Custom sort priority to use (smaller means sorted first) */
	int32 SortPriority;

	FComponentEntryCustomizationArgs()
		: AssetOverride( nullptr )
		, ComponentNameOverride()
		, OnComponentCreated()
		, IconOverrideBrushName( NAME_None )
		, SortPriority(0)
	{
	
	}
};
class FComponentClassComboEntry: public TSharedFromThis<FComponentClassComboEntry>
{
public:
	FComponentClassComboEntry( const FString& InHeadingText, TSubclassOf<UActorComponent> InComponentClass, bool InIncludedInFilter, EComponentCreateAction::Type InComponentCreateAction, FComponentEntryCustomizationArgs InCustomizationArgs = FComponentEntryCustomizationArgs() )
		: ComponentClass(InComponentClass)
		, IconClass(InComponentClass)
		, ComponentName()
		, ComponentPath()
		, HeadingText(InHeadingText)
		, bIncludedInFilter(InIncludedInFilter)
		, ComponentCreateAction(InComponentCreateAction)
		, CustomizationArgs(InCustomizationArgs)
	{}

	FComponentClassComboEntry(const FString& InHeadingText, const FString& InComponentName, FName InComponentPath, const UClass* InIconClass, bool InIncludedInFilter)
		: ComponentClass(nullptr)
		, IconClass(InIconClass)
		, ComponentName(InComponentName)
		, ComponentPath(InComponentPath)
		, HeadingText(InHeadingText)
		, bIncludedInFilter(InIncludedInFilter)
		, ComponentCreateAction(EComponentCreateAction::SpawnExistingClass)
	{}

	FComponentClassComboEntry( const FString& InHeadingText )
		: ComponentClass(nullptr)
		, IconClass(nullptr)
		, ComponentName()
		, ComponentPath()
		, HeadingText(InHeadingText)
		, bIncludedInFilter(false)
	{}

	FComponentClassComboEntry()
		: ComponentClass(nullptr)
		, IconClass(nullptr)
	{}


	TSubclassOf<UActorComponent> GetComponentClass() const { return ComponentClass; }

	const UClass* GetIconClass() const { return IconClass; }

	const FString& GetHeadingText() const { return HeadingText; }

	bool IsHeading() const
	{
		return ((ComponentClass == NULL && ComponentName == FString()) && !HeadingText.IsEmpty());
	}
	bool IsSeparator() const
	{
		return ((ComponentClass == NULL && ComponentName == FString()) && HeadingText.IsEmpty());
	}
	
	bool IsClass() const
	{
		return (ComponentClass != NULL || ComponentName != FString());
	}
	
	bool IsIncludedInFilter() const
	{
		return bIncludedInFilter;
	}
	
	const FString& GetComponentNameOverride() const
	{
		return CustomizationArgs.ComponentNameOverride;
	}

	EComponentCreateAction::Type GetComponentCreateAction() const
	{
		return ComponentCreateAction;
	}

	FOnComponentCreated& GetOnComponentCreated()
	{
		return CustomizationArgs.OnComponentCreated;
	}
	FString GetClassName() const;
	FString GetComponentPath() const { return ComponentPath.ToString(); }

	UObject* GetAssetOverride()
	{
		return CustomizationArgs.AssetOverride.Get();
	}

	FName GetIconOverrideBrushName() const { return CustomizationArgs.IconOverrideBrushName; }

	int32 GetSortPriority() const { return CustomizationArgs.SortPriority; }
private:
	TSubclassOf<UActorComponent> ComponentClass;
	const UClass* IconClass;
	// For components that are not loaded we just keep the name of the component,
	// loading occurs when the blueprint is spawned, which should also trigger a refresh
	// of the component list:
	FString ComponentName;
	FName ComponentPath;
	FString HeadingText;
	bool bIncludedInFilter;
	EComponentCreateAction::Type ComponentCreateAction;
	FComponentEntryCustomizationArgs CustomizationArgs;
};

//////////////////////////////////////////////////////////////////////////

class UNREALED_API SComponentClassCombo : public SComboButton
{
public:
	SLATE_BEGIN_ARGS( SComponentClassCombo )
		: _IncludeText(true)
	{}

		SLATE_ATTRIBUTE(bool, IncludeText)
		SLATE_EVENT( FComponentClassSelected, OnComponentClassSelected )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	~SComponentClassCombo();

	/** Clear the current combo list selection */
	void ClearSelection();

	/**
	 * Updates the filtered list of component names.
	 * @param InSearchText The search text from the search control.
	 */
	void GenerateFilteredComponentList(const FString& InSearchText);

	FText GetCurrentSearchString() const;

	/**
	 * Called when the user changes the text in the search box.
	 * @param InSearchText The new search string.
	 */
	void OnSearchBoxTextChanged( const FText& InSearchText );

	/** Callback when the user commits the text in the searchbox */
	void OnSearchBoxTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	void OnAddComponentSelectionChanged( FComponentClassComboEntryPtr InItem, ESelectInfo::Type SelectInfo );

	TSharedRef<ITableRow> GenerateAddComponentRow( FComponentClassComboEntryPtr Entry, const TSharedRef<STableViewBase> &OwnerTable ) const;

	/** Update list of component classes */
	void UpdateComponentClassList();

	/** Returns a component name without the substring "Component" and sanitized for display */
	static FString GetSanitizedComponentName(FComponentClassComboEntryPtr Entry);

private:

	FText GetFriendlyComponentName(FComponentClassComboEntryPtr Entry) const;

	TSharedRef<SToolTip> GetComponentToolTip(FComponentClassComboEntryPtr Entry) const;

	FComponentClassSelected OnComponentClassSelected;

	/** List of component class names used by combo box */
	TArray<FComponentClassComboEntryPtr>* ComponentClassList;

	/** List of component class names, filtered by the current search string */
	TArray<FComponentClassComboEntryPtr> FilteredComponentClassList;

	/** The current search string */
	FText CurrentSearchString;

	/** The search box control - part of the combo drop down */
	TSharedPtr<SSearchBox> SearchBox;

	/** The component list control - part of the combo drop down */
	TSharedPtr< SListView<FComponentClassComboEntryPtr> > ComponentClassListView;

	/** Cached selection index used to skip over unselectable items */
	int32 PrevSelectedIndex;
};
