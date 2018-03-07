// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "BlueprintEditor.h"

class FImaginaryFiBData;
class SComboButton;
class SFindInBlueprints;
struct FMemberReference;

class FTargetReplaceReferences
{
public:
	virtual ~FTargetReplaceReferences()
	{}

	/** Returns a generated widget to represent this target item reference */
	virtual TSharedRef<SWidget> CreateWidget() const = 0;

	/**
	 * Retrieves the MemberReference represented by this item, if any
	 *
	 * @param OutVariableReference		The output MemberReference represented by this item
	 * @return							TRUE if successful at having a MemberReference
	 */
	virtual bool GetMemberReference(FMemberReference& OutVariableReference) const = 0;

	/** Returns the display title for this item */
	virtual FText GetDisplayTitle() const = 0;

	/** TRUE if this item is a category and nothing else */
	virtual bool IsCategory() const { return false; }

	/** Returns the Icon representing this reference */
	virtual const struct FSlateBrush* GetIcon() const { return nullptr; }

	/** Returns the Icon Color of this reference */
	virtual FSlateColor GetIconColor() const { return FLinearColor::White; }

public:
	/** Children members to sub-list in the tree */
	TArray< TSharedPtr<FTargetReplaceReferences> > Children;
};

class SReplaceNodeReferences : public SCompoundWidget
{
protected:
	typedef TSharedPtr< FTargetReplaceReferences > FTreeViewItem;
	typedef STreeView<FTreeViewItem>  SReplaceReferencesTreeViewType;
public:
	SLATE_BEGIN_ARGS( SReplaceNodeReferences ) {}
	SLATE_END_ARGS()

		SReplaceNodeReferences()
		: SourceProperty(nullptr)
	{

	}

	void Construct(const FArguments& InArgs, TSharedPtr<class FBlueprintEditor> InBlueprintEditor);
	~SReplaceNodeReferences();

	/** Forces a refresh on this widget when things in the Blueprint Editor have changed */
	void Refresh();

	/** Sets a source variable reference to replace */
	void SetSourceVariable(UProperty* InProperty);

protected:

	/** Callback for determining if the SourceReference is visible */
	EVisibility GetSourceReferenceVisibility() const
	{
		return SourceProperty == nullptr? EVisibility::Collapsed : EVisibility::Visible;
	}

	/** Callback for determining if the message to inform the user a SourceReference is needed is visible */
	EVisibility GetPickSourceReferenceVisibility() const
	{
		return SourceProperty == nullptr? EVisibility::Visible : EVisibility::Collapsed;
	}

	/* Called when a new row is being generated */
	TSharedRef<ITableRow> OnGenerateRow(FTreeViewItem InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/* Get the children of a row */
	void OnGetChildren( FTreeViewItem InItem, TArray< FTreeViewItem >& OutChildren );

	/* Returns the menu content for the Target Reference drop down section of the combo button */
	TSharedRef<SWidget>	GetMenuContent();

	/** Callback when selection in the combo button has changed */
	void OnSelectionChanged(FTreeViewItem Selection, ESelectInfo::Type SelectInfo);

	/** 
	 * Submits a search query and potentially does a mass replace on results
	 *
	 * @param bInFindAndReplace		TRUE if it's a Find-and-Replace action, FALSE if it should do a search of items that will be replaced
	 */
	void OnSubmitSearchQuery(bool bFindAndReplace);

	/** Callback for "Find All" button */
	FReply OnFindAll();

	/** Callback for "Find and Replace All" button */
	FReply OnFindAndReplaceAll();

	/** Callback when the search for "Find and Replace All" is complete so that the replacements can begin */
	void FindAllReplacementsComplete(TArray<TSharedPtr<class FImaginaryFiBData>>& InRawDataList);

	/** Recursively gathers all available Blueprint Variable references to replace with */
	void GatherAllAvailableBlueprintVariables(UClass* InTargetClass);

	/** Returns the display text for the target reference */
	FText GetTargetDisplayText() const;

	/** Returns the icon for the target reference */
	const struct FSlateBrush* GetTargetIcon() const;

	/** Returns the icon color for the target reference */
	FSlateColor GetTargetIconColor() const;

	/** Returns the display text for the source reference */
	FText GetSourceDisplayText() const;

	/** Returns the icon for the source reference */
	const FSlateBrush* GetSourceReferenceIcon() const;

	/** Returns the icon color for the source reference */
	FSlateColor GetSourceReferenceIconColor() const;

protected:
	/** Combo box for selecting the target reference */
	TSharedPtr< SComboButton > TargetReferencesComboBox;

	/** Tree view for display available target references */
	TSharedPtr< SReplaceReferencesTreeViewType > AvailableTargetReferencesTreeView;

	/** List of items used for the root of the tree */
	TArray<FTreeViewItem> BlueprintVariableList;

	/** Target SKEL_ class that is being referenced by this window */
	UClass* TargetClass;

	/** Blueprint editor that owns this window */
	TWeakPtr<FBlueprintEditor> BlueprintEditor;

	/** Cached SourcePinType for the property the user wants to replace */
	FEdGraphPinType SourcePinType;

	/** Cached SourceProperty that the user wants to replace */
	UProperty* SourceProperty;

	/** Find-in-Blueprints window used for making search queries and presenting results to the user */
	TSharedPtr< SFindInBlueprints > FindInBlueprints;

	/** Currently selected target reference */
	FTreeViewItem SelectedTargetReferenceItem;
};
