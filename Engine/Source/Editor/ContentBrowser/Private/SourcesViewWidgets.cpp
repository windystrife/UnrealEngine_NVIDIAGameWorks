// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourcesViewWidgets.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "EditorFontGlyphs.h"
#include "PathViewTypes.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/CollectionDragDropOp.h"
#include "DragDropHandler.h"
#include "ContentBrowserUtils.h"
#include "CollectionViewUtils.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

//////////////////////////
// SAssetTreeItem
//////////////////////////

void SAssetTreeItem::Construct( const FArguments& InArgs )
{
	TreeItem = InArgs._TreeItem;
	OnNameChanged = InArgs._OnNameChanged;
	OnVerifyNameChanged = InArgs._OnVerifyNameChanged;
	OnAssetsOrPathsDragDropped = InArgs._OnAssetsOrPathsDragDropped;
	OnFilesDragDropped = InArgs._OnFilesDragDropped;
	IsItemExpanded = InArgs._IsItemExpanded;
	bDraggedOver = false;

	FolderOpenBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
	FolderClosedBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
	FolderOpenCodeBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpenCode");
	FolderClosedCodeBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosedCode");
	FolderDeveloperBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderDeveloper");
		
	FolderType = EFolderType::Normal;
	if( ContentBrowserUtils::IsDevelopersFolder(InArgs._TreeItem->FolderPath) )
	{
		FolderType = EFolderType::Developer;
	}
	else if( ContentBrowserUtils::IsClassPath/*IsClassRootDir*/(InArgs._TreeItem->FolderPath) )
	{
		FolderType = EFolderType::Code;
	}

	bool bIsRoot = !InArgs._TreeItem->Parent.IsValid();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SAssetTreeItem::GetBorderImage)
		.Padding( FMargin( 0, bIsRoot ? 3 : 0, 0, 0 ) )	// For root items in the tree, give them a little breathing room on the top
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 2, 0)
			.VAlign(VAlign_Center)
			[
				// Folder Icon
				SNew(SImage) 
				.Image(this, &SAssetTreeItem::GetFolderIcon)
				.ColorAndOpacity(this, &SAssetTreeItem::GetFolderColor)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
					.Text(this, &SAssetTreeItem::GetNameText)
					.ToolTipText(this, &SAssetTreeItem::GetToolTipText)
					.Font( FEditorStyle::GetFontStyle(bIsRoot ? "ContentBrowser.SourceTreeRootItemFont" : "ContentBrowser.SourceTreeItemFont") )
					.HighlightText( InArgs._HighlightText )
					.OnTextCommitted(this, &SAssetTreeItem::HandleNameCommitted)
					.OnVerifyTextChanged(this, &SAssetTreeItem::VerifyNameChanged)
					.IsSelected( InArgs._IsSelected )
					.IsReadOnly( this, &SAssetTreeItem::IsReadOnly )
			]
		]
	];

	if( InlineRenameWidget.IsValid() )
	{
		EnterEditingModeDelegateHandle = TreeItem.Pin()->OnRenamedRequestEvent.AddSP( InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode );
	}
}

SAssetTreeItem::~SAssetTreeItem()
{
	if( InlineRenameWidget.IsValid() )
	{
		TreeItem.Pin()->OnRenamedRequestEvent.Remove( EnterEditingModeDelegateHandle );
	}
}

bool SAssetTreeItem::ValidateDragDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation ) const
{
	OutIsKnownDragOperation = false;

	TSharedPtr<FTreeItem> TreeItemPinned = TreeItem.Pin();
	return TreeItemPinned.IsValid() && DragDropHandler::ValidateDragDropOnAssetFolder(MyGeometry, DragDropEvent, TreeItemPinned->FolderPath, OutIsKnownDragOperation);
}

void SAssetTreeItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
}

void SAssetTreeItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid())
	{
		Operation->SetCursorOverride(TOptional<EMouseCursor::Type>());

		if (Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(Operation);
			DragDropOp->ResetToDefaultToolTip();
		}
	}

	bDraggedOver = false;
}

FReply SAssetTreeItem::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
	return (bDraggedOver) ? FReply::Handled() : FReply::Unhandled();
}

FReply SAssetTreeItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if (ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver)) // updates bDraggedOver
	{
		bDraggedOver = false;

		TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
		if (!Operation.IsValid())
		{
			return FReply::Unhandled();
		}

		if (Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( DragDropEvent.GetOperation() );
			OnAssetsOrPathsDragDropped.ExecuteIfBound(DragDropOp->GetAssets(), DragDropOp->GetAssetPaths(), TreeItem.Pin());
			return FReply::Handled();
		}
		
		if (Operation->IsOfType<FExternalDragOperation>())
		{
			TSharedPtr<FExternalDragOperation> DragDropOp = StaticCastSharedPtr<FExternalDragOperation>( DragDropEvent.GetOperation() );
			OnFilesDragDropped.ExecuteIfBound(DragDropOp->GetFiles(), TreeItem.Pin());
			return FReply::Handled();
		}
	}

	if (bDraggedOver)
	{
		// We were able to handle this operation, but could not due to another error - still report this drop as handled so it doesn't fall through to other widgets
		bDraggedOver = false;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAssetTreeItem::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	LastGeometry = AllottedGeometry;
}

bool SAssetTreeItem::VerifyNameChanged(const FText& InName, FText& OutError) const
{
	if ( TreeItem.IsValid() )
	{
		TSharedPtr<FTreeItem> TreeItemPtr = TreeItem.Pin();
		if(OnVerifyNameChanged.IsBound())
		{
			return OnVerifyNameChanged.Execute(InName.ToString(), OutError, TreeItemPtr->FolderPath);
		}
	}

	return true;
}

void SAssetTreeItem::HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	if ( TreeItem.IsValid() )
	{
		TSharedPtr<FTreeItem> TreeItemPtr = TreeItem.Pin();

		if ( TreeItemPtr->bNamingFolder )
		{
			TreeItemPtr->bNamingFolder = false;

			const FString OldPath = TreeItemPtr->FolderPath;
			FString Path;
			TreeItemPtr->FolderPath.Split(TEXT("/"), &Path, NULL, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			TreeItemPtr->DisplayName = NewText;
			TreeItemPtr->FolderName = NewText.ToString();
			TreeItemPtr->FolderPath = Path + TEXT("/") + NewText.ToString();

			FVector2D MessageLoc;
			MessageLoc.X = LastGeometry.AbsolutePosition.X;
			MessageLoc.Y = LastGeometry.AbsolutePosition.Y + LastGeometry.Size.Y * LastGeometry.Scale;

			OnNameChanged.ExecuteIfBound(TreeItemPtr, OldPath, MessageLoc, CommitInfo);
		}
	}
}

bool SAssetTreeItem::IsReadOnly() const
{
	if ( TreeItem.IsValid() )
	{
		return !TreeItem.Pin()->bNamingFolder;
	}
	else
	{
		return true;
	}
}

bool SAssetTreeItem::IsValidAssetPath() const
{
	if ( TreeItem.IsValid() )
	{
		// The classes folder is not a real path
		return !ContentBrowserUtils::IsClassPath(TreeItem.Pin()->FolderPath);
	}
	else
	{
		return false;
	}
}

const FSlateBrush* SAssetTreeItem::GetFolderIcon() const
{
	switch( FolderType )
	{
	case EFolderType::Code:
		return ( IsItemExpanded.Get() ) ? FolderOpenCodeBrush : FolderClosedCodeBrush;

	case EFolderType::Developer:
		return FolderDeveloperBrush;

	default:
		return ( IsItemExpanded.Get() ) ? FolderOpenBrush : FolderClosedBrush;
	}
}

FSlateColor SAssetTreeItem::GetFolderColor() const
{
	if ( TreeItem.IsValid() )
	{
		const TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor( TreeItem.Pin()->FolderPath );
		if ( Color.IsValid() )
		{
			return *Color.Get();
		}
	}
	return ContentBrowserUtils::GetDefaultColor();
}

FText SAssetTreeItem::GetNameText() const
{
	TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin();
	if ( TreeItemPin.IsValid() )
	{
		return TreeItemPin->DisplayName;
	}
	else
	{
		return FText();
	}
}

FText SAssetTreeItem::GetToolTipText() const
{
	TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin();
	if ( TreeItemPin.IsValid() )
	{
		return FText::FromString(TreeItemPin->FolderPath);
	}
	else
	{
		return FText();
	}
}

const FSlateBrush* SAssetTreeItem::GetBorderImage() const
{
	return bDraggedOver ? FEditorStyle::GetBrush("Menu.Background") : FEditorStyle::GetBrush("NoBorder");
}



//////////////////////////
// SCollectionTreeItem
//////////////////////////

void SCollectionTreeItem::Construct( const FArguments& InArgs )
{
	ParentWidget = InArgs._ParentWidget;
	CollectionItem = InArgs._CollectionItem;
	OnBeginNameChange = InArgs._OnBeginNameChange;
	OnNameChangeCommit = InArgs._OnNameChangeCommit;
	OnVerifyRenameCommit = InArgs._OnVerifyRenameCommit;
	OnValidateDragDrop = InArgs._OnValidateDragDrop;
	OnHandleDragDrop = InArgs._OnHandleDragDrop;
	bDraggedOver = false;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SCollectionTreeItem::GetBorderImage)
		.Padding(0)
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 2, 0)
			[
				SNew(SCheckBox)
				.Visibility( InArgs._IsCollectionChecked.IsSet() ? EVisibility::Visible : EVisibility::Collapsed )
				.IsEnabled( InArgs._IsCheckBoxEnabled )
				.IsChecked( InArgs._IsCollectionChecked )
				.OnCheckStateChanged( InArgs._OnCollectionCheckStateChanged )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 2, 0)
			[
				// Share Type Icon
				SNew(SImage)
				.Image( FEditorStyle::GetBrush( ECollectionShareType::GetIconStyleName(InArgs._CollectionItem->CollectionType) ) )
				.ColorAndOpacity( this, &SCollectionTreeItem::GetCollectionColor )
				.ToolTipText(ECollectionShareType::GetDescription(InArgs._CollectionItem->CollectionType))
			]

			+SHorizontalBox::Slot()
			[
				SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
				.Text( this, &SCollectionTreeItem::GetNameText )
				.HighlightText( InArgs._HighlightText )
				.Font( FEditorStyle::GetFontStyle("ContentBrowser.SourceListItemFont") )
				.OnBeginTextEdit(this, &SCollectionTreeItem::HandleBeginNameChange)
				.OnTextCommitted(this, &SCollectionTreeItem::HandleNameCommitted)
				.OnVerifyTextChanged(this, &SCollectionTreeItem::HandleVerifyNameChanged)
				.IsSelected( InArgs._IsSelected )
				.IsReadOnly( InArgs._IsReadOnly )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 3, 0)
			[
				// Storage Mode Icon
				SNew(SBox)
				.WidthOverride(16)
				.HeightOverride(16)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
					.Text(this, &SCollectionTreeItem::GetCollectionStorageModeIconText)
					.ColorAndOpacity(FLinearColor::Gray)
					.ToolTipText(this, &SCollectionTreeItem::GetCollectionStorageModeToolTipText)
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 2, 0)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("ContentBrowser.CollectionStatus"))
				.ColorAndOpacity(this, &SCollectionTreeItem::GetCollectionStatusColor)
				.ToolTipText(this, &SCollectionTreeItem::GetCollectionStatusToolTipText)
			]
		]
	];

	if(InlineRenameWidget.IsValid())
	{
		// This is broadcast when the context menu / input binding requests a rename
		EnterEditingModeDelegateHandle = CollectionItem.Pin()->OnRenamedRequestEvent.AddSP(InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
	}
}

SCollectionTreeItem::~SCollectionTreeItem()
{
	if(InlineRenameWidget.IsValid())
	{
		CollectionItem.Pin()->OnRenamedRequestEvent.Remove( EnterEditingModeDelegateHandle );
	}
}

void SCollectionTreeItem::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Cache this widget's geometry so it can pop up warnings over itself
	CachedGeometry = AllottedGeometry;
}

bool SCollectionTreeItem::ValidateDragDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation ) const
{
	OutIsKnownDragOperation = false;

	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();
	if (OnValidateDragDrop.IsBound() && CollectionItemPtr.IsValid())
	{
		return OnValidateDragDrop.Execute( CollectionItemPtr.ToSharedRef(), MyGeometry, DragDropEvent, OutIsKnownDragOperation );
	}

	return false;
}

void SCollectionTreeItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
}

void SCollectionTreeItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid())
	{
		Operation->SetCursorOverride(TOptional<EMouseCursor::Type>());

		if (Operation->IsOfType<FCollectionDragDropOp>() || Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FDecoratedDragDropOp> DragDropOp = StaticCastSharedPtr<FDecoratedDragDropOp>(Operation);
			DragDropOp->ResetToDefaultToolTip();
		}
	}

	bDraggedOver = false;
}

FReply SCollectionTreeItem::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
	return (bDraggedOver) ? FReply::Handled() : FReply::Unhandled();
}

FReply SCollectionTreeItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if (ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver) && OnHandleDragDrop.IsBound()) // updates bDraggedOver
	{
		bDraggedOver = false;
		return OnHandleDragDrop.Execute( CollectionItem.Pin().ToSharedRef(), MyGeometry, DragDropEvent );
	}

	if (bDraggedOver)
	{
		// We were able to handle this operation, but could not due to another error - still report this drop as handled so it doesn't fall through to other widgets
		bDraggedOver = false;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCollectionTreeItem::HandleBeginNameChange( const FText& OldText )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		// If we get here via a context menu or input binding, bRenaming will already be set on the item.
		// If we got here by double clicking the editable text field, we need to set it now.
		CollectionItemPtr->bRenaming = true;

		OnBeginNameChange.ExecuteIfBound( CollectionItemPtr );
	}
}

void SCollectionTreeItem::HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		if ( CollectionItemPtr->bRenaming )
		{
			CollectionItemPtr->bRenaming = false;

			if ( OnNameChangeCommit.IsBound() )
			{
				FText WarningMessage;
				bool bIsCommitted = (CommitInfo != ETextCommit::OnCleared);
				if ( !OnNameChangeCommit.Execute(CollectionItemPtr, NewText.ToString(), bIsCommitted, WarningMessage) && ParentWidget.IsValid() && bIsCommitted )
				{
					// Failed to rename/create a collection, display a warning.
					ContentBrowserUtils::DisplayMessage(WarningMessage, CachedGeometry.GetLayoutBoundingRect(), ParentWidget.ToSharedRef());
				}
			}				
		}
	}
}

bool SCollectionTreeItem::HandleVerifyNameChanged( const FText& NewText, FText& OutErrorMessage )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		return !OnVerifyRenameCommit.IsBound() || OnVerifyRenameCommit.Execute(CollectionItemPtr, NewText.ToString(), CachedGeometry.GetLayoutBoundingRect(), OutErrorMessage);
	}

	return true;
}

FText SCollectionTreeItem::GetNameText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		return FText::FromName(CollectionItemPtr->CollectionName);
	}
	else
	{
		return FText();
	}
}

FSlateColor SCollectionTreeItem::GetCollectionColor() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		const TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor(CollectionItemPtr->CollectionName.ToString(), CollectionItemPtr->CollectionType);
		if( Color.IsValid() )
		{
			return *Color.Get();
		}
	}
	
	return CollectionViewUtils::GetDefaultColor();
}

const FSlateBrush* SCollectionTreeItem::GetBorderImage() const
{
	return bDraggedOver ? FEditorStyle::GetBrush("Menu.Background") : FEditorStyle::GetBrush("NoBorder");
}

FText SCollectionTreeItem::GetCollectionStorageModeIconText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		switch(CollectionItemPtr->StorageMode)
		{
		case ECollectionStorageMode::Static:
			return FEditorFontGlyphs::List_Alt;

		case ECollectionStorageMode::Dynamic:
			return FEditorFontGlyphs::Bolt;

		default:
			break;
		}
	}

	return FText::GetEmpty();
}

FText SCollectionTreeItem::GetCollectionStorageModeToolTipText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		return ECollectionStorageMode::GetDescription(CollectionItemPtr->StorageMode);
	}

	return FText::GetEmpty();
}

FSlateColor SCollectionTreeItem::GetCollectionStatusColor() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		switch(CollectionItemPtr->CurrentStatus)
		{
		case ECollectionItemStatus::IsUpToDateAndPopulated:
			return FLinearColor(0.10616, 0.48777, 0.10616); // Green

		case ECollectionItemStatus::IsUpToDateAndEmpty:
			return FLinearColor::Gray;

		case ECollectionItemStatus::IsOutOfDate:
			return FLinearColor(0.87514, 0.42591, 0.07383); // Orange

		case ECollectionItemStatus::IsCheckedOutByAnotherUser:
		case ECollectionItemStatus::IsConflicted:
		case ECollectionItemStatus::IsMissingSCCProvider:
			return FLinearColor(0.70117, 0.08464, 0.07593); // Red

		case ECollectionItemStatus::HasLocalChanges:
			return FLinearColor(0.10363, 0.53564, 0.7372); // Blue

		default:
			break;
		}
	}

	return FLinearColor::White;
}

FText SCollectionTreeItem::GetCollectionStatusToolTipText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		switch(CollectionItemPtr->CurrentStatus)
		{
		case ECollectionItemStatus::IsUpToDateAndPopulated:
			return LOCTEXT("CollectionStatus_IsUpToDateAndPopulated", "Collection is up-to-date");

		case ECollectionItemStatus::IsUpToDateAndEmpty:
			return LOCTEXT("CollectionStatus_IsUpToDateAndEmpty", "Collection is empty");

		case ECollectionItemStatus::IsOutOfDate:
			return LOCTEXT("CollectionStatus_IsOutOfDate", "Collection is not at the latest revision");

		case ECollectionItemStatus::IsCheckedOutByAnotherUser:
			return LOCTEXT("CollectionStatus_IsCheckedOutByAnotherUser", "Collection is checked out by another user");

		case ECollectionItemStatus::IsConflicted:
			return LOCTEXT("CollectionStatus_IsConflicted", "Collection is conflicted - please use your external source control provider to resolve this conflict");

		case ECollectionItemStatus::IsMissingSCCProvider:
			return LOCTEXT("CollectionStatus_IsMissingSCCProvider", "Collection is missing its source control provider - please check your source control settings");

		case ECollectionItemStatus::HasLocalChanges:
			return LOCTEXT("CollectionStatus_HasLocalChanges", "Collection has local unsaved or uncomitted changes");

		default:
			break;
		}
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
