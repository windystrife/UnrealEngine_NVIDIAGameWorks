// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Misc/Attribute.h"
#include "EdGraph/EdGraphPin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/Views/STableViewBase.h"
#include "Types/SlateStructs.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "EditorStyleSet.h"
#include "EdGraphSchema_K2.h"

class SComboButton;
class SMenuOwner;
class SToolTip;
struct FObjectReferenceType;

DECLARE_DELEGATE_OneParam(FOnPinTypeChanged, const FEdGraphPinType&)


//////////////////////////////////////////////////////////////////////////
// SPinTypeSelector

typedef TSharedPtr<class UEdGraphSchema_K2::FPinTypeTreeInfo> FPinTypeTreeItem;
typedef STreeView<FPinTypeTreeItem> SPinTypeTreeView;

DECLARE_DELEGATE_TwoParams(FGetPinTypeTree, TArray<FPinTypeTreeItem >&, ETypeTreeFilter);

struct FObjectReferenceType;
typedef TSharedPtr<struct FObjectReferenceType> FObjectReferenceListItem;

/** Widget for modifying the type for a variable or pin */
class KISMETWIDGETS_API SPinTypeSelector : public SCompoundWidget
{
public:
	static TSharedRef<SWidget> ConstructPinTypeImage(const FSlateBrush* PrimaryIcon, const FSlateColor& PrimaryColor, const FSlateBrush* SecondaryIcon, const FSlateColor& SecondaryColor, TSharedPtr<SToolTip> InToolTip);
	static TSharedRef<SWidget> ConstructPinTypeImage(TAttribute<const FSlateBrush*> PrimaryIcon, TAttribute<FSlateColor> PrimaryColor, TAttribute<const FSlateBrush*> SecondaryIcon, TAttribute<FSlateColor> SecondaryColor );
	static TSharedRef<SWidget> ConstructPinTypeImage(UEdGraphPin* Pin);

	SLATE_BEGIN_ARGS( SPinTypeSelector )
		: _TargetPinType()
		, _Schema(nullptr)
		, _TypeTreeFilter(ETypeTreeFilter::None)
		, _bAllowArrays(true)
		, _TreeViewWidth(300.f)
		, _TreeViewHeight(400.f)
		, _Font( FEditorStyle::GetFontStyle( TEXT("NormalFont") ) )
		, _bCompactSelector( false )
		{}
		SLATE_ATTRIBUTE( FEdGraphPinType, TargetPinType )
		SLATE_ARGUMENT( const UEdGraphSchema_K2*, Schema )
		SLATE_ARGUMENT( ETypeTreeFilter, TypeTreeFilter )
		SLATE_ARGUMENT( bool, bAllowArrays )
		SLATE_ATTRIBUTE( FOptionalSize, TreeViewWidth )
		SLATE_ATTRIBUTE( FOptionalSize, TreeViewHeight )
		SLATE_EVENT( FOnPinTypeChanged, OnPinTypePreChanged )
		SLATE_EVENT( FOnPinTypeChanged, OnPinTypeChanged )
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
		SLATE_ARGUMENT( bool, bCompactSelector )
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, FGetPinTypeTree GetPinTypeTreeFunc);

	// SWidget interface
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	// End of SWidget interface

protected:
	/** Gets the icon (value, array, set, or map) for the type being manipulated */
	const FSlateBrush* GetTypeIconImage() const;

	/** Gets the secondary icon (for maps, otherwise null) for the type being manipulated */
	const FSlateBrush* GetSecondaryTypeIconImage() const;

	/** Gets the type-specific color for the type being manipulated */
	FSlateColor GetTypeIconColor() const;

	/** Gets the secondary type-specific color for the type being manipulated */
	FSlateColor GetSecondaryTypeIconColor() const;

	/** Gets a succinct type description for the type being manipulated */
	FText GetTypeDescription() const;

	/** Gets the secondary type description. E.g. the value type for TMaps */
	FText GetSecondaryTypeDescription() const;

	TSharedPtr<SComboButton>		TypeComboButton;
	TSharedPtr<SComboButton>		SecondaryTypeComboButton;
	TSharedPtr<SSearchBox>			FilterTextBox;
	TSharedPtr<SPinTypeTreeView>	TypeTreeView;
	
	/** The pin attribute that we're modifying with this widget */
	TAttribute<FEdGraphPinType>		TargetPinType;

	/** Delegate that is called every time the pin type changes (before and after). */
	FOnPinTypeChanged			OnTypeChanged;
	FOnPinTypeChanged			OnTypePreChanged;

	/** Delegate for the type selector to retrieve the pin type tree (passed into the Construct so the tree can depend on the situation) */
	FGetPinTypeTree				GetPinTypeTree;

	/** Schema in charge of determining available types for this pin */
	UEdGraphSchema_K2*			Schema;

	/** UEdgraphSchema::ETypeTreeFilter flags for filtering available types*/
	ETypeTreeFilter				TypeTreeFilter;

	/** Desired width of the tree view widget */
	TAttribute<FOptionalSize>	TreeViewWidth;

	/** Desired height of the tree view widget */
	TAttribute<FOptionalSize>	TreeViewHeight;

	/** TRUE when the right mouse button is pressed, keeps from handling a right click that does not begin in the widget */
	bool bIsRightMousePressed;

	/** TRUE if displaying a compact selector widget, some functionality is enabled in different ways if this is TRUE */
	bool bIsCompactSelector;

	/** Holds a cache of the allowed Object Reference types for the last sub-menu opened. */
	TArray<FObjectReferenceListItem> AllowedObjectReferenceTypes;
	TWeakPtr<SListView<FObjectReferenceListItem>> WeakListView;
	TWeakPtr<SMenuOwner> PinTypeSelectorMenuOwner;

	/** Array checkbox support functions */
	ECheckBoxState IsArrayChecked() const;
	void OnArrayCheckStateChanged(ECheckBoxState NewState);

	/** Toggles the variable type as an array */
	void OnArrayStateToggled();

	/** Updates the variable container type: */
	void OnContainerTypeSelectionChanged(EPinContainerType PinContainerType);

	/** Array containing the unfiltered list of all supported types this pin could possibly have */
	TArray<FPinTypeTreeItem>		TypeTreeRoot;
	/** Array containing a filtered list, according to the text in the searchbox */
	TArray<FPinTypeTreeItem>		FilteredTypeTreeRoot;

	/** Treeview support functions */
	virtual TSharedRef<ITableRow> GenerateTypeTreeRow(FPinTypeTreeItem InItem, const TSharedRef<STableViewBase>& OwnerTree, bool bForSecondaryType);
	void OnTypeSelectionChanged(FPinTypeTreeItem Selection, ESelectInfo::Type SelectInfo, bool bForSecondaryType);
	void GetTypeChildren(FPinTypeTreeItem InItem, TArray<FPinTypeTreeItem>& OutChildren);

	/** Listview support functions for sub-menu */
	TSharedRef<ITableRow> GenerateObjectReferenceTreeRow(FObjectReferenceListItem InItem, const TSharedRef<STableViewBase>& OwnerTree);
	void OnObjectReferenceSelectionChanged(FObjectReferenceListItem InItem, ESelectInfo::Type SelectInfo, bool bForSecondaryType);

	/** Reference to the menu content that's displayed when the type button is clicked on */
	TSharedPtr<SMenuOwner> MenuContent;
	virtual TSharedRef<SWidget>	GetMenuContent(bool bForSecondaryType);

	/** Type searching support */
	FText SearchText;
	void OnFilterTextChanged(const FText& NewText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	/** Helper to generate the filtered list of types, based on the search string matching */
	bool GetChildrenMatchingSearch(const FText& SearchText, const TArray<FPinTypeTreeItem>& UnfilteredList, TArray<FPinTypeTreeItem>& OutFilteredList);

	/** Callback to get the tooltip text for the pin type combo box */
	FText GetToolTipForComboBoxType() const;

	/** Callback to get the tooltip text for the secondary pin type combo box */
	FText GetToolTipForComboBoxSecondaryType() const;

	/** Callback to get the tooltip for the array button widget */
	FText GetToolTipForArrayWidget() const;

	/** Callback to get the tooltip for the container type dropdown widget */
	FText GetToolTipForContainerWidget() const;

	/**
	 * Helper function to create widget for the sub-menu
	 *
	 * @param InItem				Tree item to use for the callback when a menu item is selected
	 * @param InPinType				Pin type for generation of the widget to display for the menu entry
	 * @param InIconBrush			Brush icon to use for the menu entry item
	 * @param InTooltip				The simple tooltip to use for the menu item, an advanced tooltip link will be auto-generated based on the PinCategory
	 */
	TSharedRef<SWidget> CreateObjectReferenceWidget(FPinTypeTreeItem InItem, FEdGraphPinType& InPinType, const FSlateBrush* InIconBrush, FText InSimpleTooltip) const;

	/** Gets the allowable object types for an tree item, used for building the sub-menu */
	TSharedRef< SWidget > GetAllowedObjectTypes(FPinTypeTreeItem InItem, bool bForSecondaryType);
	
	/**
	 * When a pin type is selected, handle it
	 *
	 * @param InItem				Item selected
	 * @param InPinCategory			This is the PinType's category, must be provided separately as the PinType in the tree item is always Object Types for any object related type.
	 */
	void OnSelectPinType(FPinTypeTreeItem InItem, FString InPinCategory, bool bForSecondaryType);
};
