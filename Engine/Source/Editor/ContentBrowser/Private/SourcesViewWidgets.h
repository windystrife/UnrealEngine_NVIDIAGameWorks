// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetData.h"
#include "CollectionViewTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Editor/ContentBrowser/Private/SPathView.h"

class SEditableTextBox;
struct FTreeItem;

/** A single item in the asset tree. Represents a folder. */
class SAssetTreeItem : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_FourParams( FOnNameChanged, const TSharedPtr<FTreeItem>& /*TreeItem*/, const FString& /*OldPath*/, const FVector2D& /*MessageLocation*/, const ETextCommit::Type /*CommitType*/);
	DECLARE_DELEGATE_RetVal_ThreeParams( bool, FOnVerifyNameChanged, const FString& /*InName*/, FText& /*OutErrorMessage*/, const FString& /*FolderPath*/);
	DECLARE_DELEGATE_ThreeParams( FOnAssetsOrPathsDragDropped, const TArray<FAssetData>& /*AssetList*/, const TArray<FString>& /*AssetPaths*/, const TSharedPtr<FTreeItem>& /*TreeItem*/);
	DECLARE_DELEGATE_TwoParams( FOnFilesDragDropped, const TArray<FString>& /*FileNames*/, const TSharedPtr<FTreeItem>& /*TreeItem*/);

	SLATE_BEGIN_ARGS( SAssetTreeItem )
		: _TreeItem( TSharedPtr<FTreeItem>() )
		, _IsItemExpanded( false )
	{}

	/** Data for the folder this item represents */
	SLATE_ARGUMENT( TSharedPtr<FTreeItem>, TreeItem )

		/** Delegate for when the user commits a new name to the folder */
		SLATE_EVENT( FOnNameChanged, OnNameChanged )

		/** Delegate for when the user is typing a new name for the folder */
		SLATE_EVENT( FOnVerifyNameChanged, OnVerifyNameChanged )

		/** Delegate for when assets or asset paths are dropped on this folder */
		SLATE_EVENT( FOnAssetsOrPathsDragDropped, OnAssetsOrPathsDragDropped )

		/** Delegate for when a list of files is dropped on this folder from an external source */
		SLATE_EVENT( FOnFilesDragDropped, OnFilesDragDropped )

		/** True when this item has children and is expanded */
		SLATE_ATTRIBUTE( bool, IsItemExpanded )

		/** The string in the title to highlight (used when searching folders) */
		SLATE_ATTRIBUTE( FText, HighlightText)

		/** Callback to check if the widget is selected, should only be hooked up if parent widget is handling selection or focus. */
		SLATE_EVENT( FIsSelected, IsSelected )
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	~SAssetTreeItem();

	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

private:
	/** Used by OnDragEnter, OnDragOver, and OnDrop to check and update the validity of the drag operation */
	bool ValidateDragDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation ) const;

	/** Handles verifying name changes */
	bool VerifyNameChanged(const FText& InName, FText& OutError) const;

	/** Handles committing a name change */
	void HandleNameCommitted( const FText& NewText, ETextCommit::Type /*CommitInfo*/ );

	/** Returns false if this folder is in the process of being created */
	bool IsReadOnly() const;

	/** Returns true if this folder is special and not a real path (like the Classes folder) */
	bool IsValidAssetPath() const;

	/** Gets the brush used to draw the folder icon */
	const FSlateBrush* GetFolderIcon() const;

	/** Gets the color used to draw the folder icon */
	FSlateColor GetFolderColor() const;

	/** Returns the text of the folder name */
	FText GetNameText() const;

	/** Returns the text to use for the folder tooltip */
	FText GetToolTipText() const;

	/** Returns the image for the border around this item. Used for drag/drop operations */
	const FSlateBrush* GetBorderImage() const;

	/** Returns the visibility of the editable folder name */
	EVisibility GetEditableTextVisibility() const;

	/** Returns the visibility of the non-editable folder name */
	EVisibility GetStaticTextVisibility() const;

private:
	enum class EFolderType : uint8
	{
		Normal,
		Code,
		Developer,
	};

	/** The data for this item */
	TWeakPtr<FTreeItem> TreeItem;

	/** The name of the asset as an editable text box */
	TSharedPtr<SEditableTextBox> EditableName;

	/** Delegate for when a list of assets or asset paths are dropped on this folder */
	FOnAssetsOrPathsDragDropped OnAssetsOrPathsDragDropped;

	/** Delegate for when a list of files is dropped on this folder from an external source */
	FOnFilesDragDropped OnFilesDragDropped;

	/** Delegate for when the user commits a new name to the folder */
	FOnNameChanged OnNameChanged;

	/** Delegate for when a user is typing a name for the folder */
	FOnVerifyNameChanged OnVerifyNameChanged;

	/** True when this item has children and is expanded */
	TAttribute<bool> IsItemExpanded;

	/** The geometry last frame. Used when telling popup messages where to appear. */
	FGeometry LastGeometry;

	/** Brushes for the different folder states */
	const FSlateBrush* FolderOpenBrush;
	const FSlateBrush* FolderClosedBrush;
	const FSlateBrush* FolderOpenCodeBrush;
	const FSlateBrush* FolderClosedCodeBrush;
	const FSlateBrush* FolderDeveloperBrush;

	/** True when a drag is over this item with a drag operation that we know how to handle. The operation itself may not be valid to drop. */
	bool bDraggedOver;

	/** What type of stuff does this folder hold */
	EFolderType FolderType;

	/** Widget to display the name of the asset item and allows for renaming */
	TSharedPtr< SInlineEditableTextBlock > InlineRenameWidget;

	/** Handle to the registered EnterEditingMode delegate. */
	FDelegateHandle EnterEditingModeDelegateHandle;
};

/** A single item in the collection tree. */
class SCollectionTreeItem : public SCompoundWidget
{
public:
	/** Delegates for when a collection is renamed. If returning false, OutWarningMessage will be displayed over the collection. */
	DECLARE_DELEGATE_OneParam( FOnBeginNameChange, const TSharedPtr<FCollectionItem>& /*Item*/);
	DECLARE_DELEGATE_RetVal_FourParams( bool, FOnNameChangeCommit, const TSharedPtr<FCollectionItem>& /*Item*/, const FString& /*NewName*/, bool /*bChangeConfirmed*/, FText& /*OutWarningMessage*/);
	DECLARE_DELEGATE_RetVal_FourParams( bool, FOnVerifyRenameCommit, const TSharedPtr<FCollectionItem>& /*Item*/, const FString& /*NewName*/, const FSlateRect& /*MessageAnchor*/, FText& /*OutErrorMessage*/)
	
	/** Delegates for when a collection item has something dropped into it */
	DECLARE_DELEGATE_RetVal_FourParams( bool, FOnValidateDragDrop, TSharedRef<FCollectionItem> /*CollectionItem*/, const FGeometry& /*Geometry*/, const FDragDropEvent& /*DragDropEvent*/, bool& /*OutIsKnownDragOperation*/ );
	DECLARE_DELEGATE_RetVal_ThreeParams( FReply, FOnHandleDragDrop, TSharedRef<FCollectionItem> /*CollectionItem*/, const FGeometry& /*Geometry*/, const FDragDropEvent& /*DragDropEvent*/ );

	SLATE_BEGIN_ARGS( SCollectionTreeItem )
		: _CollectionItem( TSharedPtr<FCollectionItem>() )
		, _ParentWidget()
	{}

		/** Data for the collection this item represents */
		SLATE_ARGUMENT( TSharedPtr<FCollectionItem>, CollectionItem )

		/** The parent widget */
		SLATE_ARGUMENT( TSharedPtr<SWidget>, ParentWidget )

		/** Delegate for when the user begins to rename the item */
		SLATE_EVENT( FOnBeginNameChange, OnBeginNameChange )

		/** Delegate for when the user commits a new name to the folder */
		SLATE_EVENT( FOnNameChangeCommit, OnNameChangeCommit )

		/** Delegate for when a collection name has been entered for an item to verify the name before commit */
		SLATE_EVENT( FOnVerifyRenameCommit, OnVerifyRenameCommit )

		/** Delegate to validate a drag drop operation on this collection item */
		SLATE_EVENT( FOnValidateDragDrop, OnValidateDragDrop )

		/** Delegate to handle a drag drop operation on this collection item */
		SLATE_EVENT( FOnHandleDragDrop, OnHandleDragDrop )

		/** Callback to check if the widget is selected, should only be hooked up if parent widget is handling selection or focus. */
		SLATE_EVENT( FIsSelected, IsSelected )

		/** True if the item is read-only. It will not be able to be renamed if read-only */
		SLATE_ATTRIBUTE( bool, IsReadOnly )

		/** Text to highlight for this item */
		SLATE_ATTRIBUTE( FText, HighlightText )

		/** True if the check box of the collection item is enabled */
		SLATE_ATTRIBUTE( bool, IsCheckBoxEnabled )

		/** Whether the check box of the collection item is currently in a checked state (if unset, no check box will be shown) */
		SLATE_ATTRIBUTE( ECheckBoxState, IsCollectionChecked )

		/** Delegate for when the checked state of the collection item check box is changed */
		SLATE_EVENT( FOnCheckStateChanged, OnCollectionCheckStateChanged )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs );

	~SCollectionTreeItem();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

private:
	/** Used by OnDragEnter, OnDragOver, and OnDrop to check and update the validity of the drag operation */
	bool ValidateDragDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation ) const;

	/** Handles beginning a name change */
	void HandleBeginNameChange( const FText& OldText );

	/** Handles committing a name change */
	void HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo );

	/** Handles verifying a name change */
	bool HandleVerifyNameChanged( const FText& NewText, FText& OutErrorMessage );

	/** Returns the text of the collection name */
	FText GetNameText() const;

	/** Returns the color of the collection name */
	FSlateColor GetCollectionColor() const;

	/** Returns the image for the border around this item. Used for drag/drop operations */
	const FSlateBrush* GetBorderImage() const;

	/** Get the FontAwesome icon corresponding to the current collection storage mode */
	FText GetCollectionStorageModeIconText() const;

	/** Get the tooltip corresponding to the current collection storage mode */
	FText GetCollectionStorageModeToolTipText() const;

	/** Get the color to use for the collection item status */
	FSlateColor GetCollectionStatusColor() const;

	/** Get the tooltip corresponding to the current collection item status */
	FText GetCollectionStatusToolTipText() const;

private:
	/** A shared pointer to the parent widget. */
	TSharedPtr<SWidget> ParentWidget;

	/** The data for this item */
	TWeakPtr<FCollectionItem> CollectionItem;

	/** The name of the asset as an editable text box */
	TSharedPtr<SEditableTextBox> EditableName;

	/** True when a drag is over this item with a drag operation that we know how to handle. The operation itself may not be valid to drop. */
	bool bDraggedOver;

	/** Delegate to validate a drag drop operation on this collection item */
	FOnValidateDragDrop OnValidateDragDrop;

	/** Delegate to handle a drag drop operation on this collection item */
	FOnHandleDragDrop OnHandleDragDrop;

	/** The geometry as of the last frame. Used to open warning messages over the item */
	FGeometry CachedGeometry;

	/** Delegate for when the user starts to rename an item */
	FOnBeginNameChange OnBeginNameChange;

	/** Delegate for when the user commits a new name to the collection */
	FOnNameChangeCommit OnNameChangeCommit;

	/** Delegate for when a collection name has been entered for an item to verify the name before commit */
	FOnVerifyRenameCommit OnVerifyRenameCommit;

	/** Widget to display the name of the collection item and allows for renaming */
	TSharedPtr< SInlineEditableTextBlock > InlineRenameWidget;

	/** Handle to the registered EnterEditingMode delegate. */
	FDelegateHandle EnterEditingModeDelegateHandle;
};
