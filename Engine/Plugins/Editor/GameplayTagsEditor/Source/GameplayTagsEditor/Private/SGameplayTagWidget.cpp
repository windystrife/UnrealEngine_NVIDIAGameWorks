// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagWidget.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Widgets/SWindow.h"
#include "Dialogs/Dialogs.h"
#include "GameplayTagsModule.h"
#include "ScopedTransaction.h"
#include "Textures/SlateIcon.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SSearchBox.h"
#include "GameplayTagsEditorModule.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Toolkits/AssetEditorManager.h"
#include "AssetToolsModule.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameplayTagsSettings.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "SAddNewGameplayTagWidget.h"
#include "SRenameGameplayTagDialog.h"
#include "AssetData.h"
#include "Editor/ReferenceViewer/Public/ReferenceViewer.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "GameplayTagWidget"

const FString SGameplayTagWidget::SettingsIniSection = TEXT("GameplayTagWidget");

void SGameplayTagWidget::Construct(const FArguments& InArgs, const TArray<FEditableGameplayTagContainerDatum>& EditableTagContainers)
{
	// If we're in management mode, we don't need to have editable tag containers.
	ensure(EditableTagContainers.Num() > 0 || InArgs._GameplayTagUIMode == EGameplayTagUIMode::ManagementMode);
	TagContainers = EditableTagContainers;

	OnTagChanged = InArgs._OnTagChanged;
	bReadOnly = InArgs._ReadOnly;
	TagContainerName = InArgs._TagContainerName;
	bMultiSelect = InArgs._MultiSelect;
	PropertyHandle = InArgs._PropertyHandle;
	RootFilterString = InArgs._Filter;
	GameplayTagUIMode = InArgs._GameplayTagUIMode;

	bAddTagSectionExpanded = InArgs._NewTagControlsInitiallyExpanded;
	bDelayRefresh = false;
	MaxHeight = InArgs._MaxHeight;

	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	Manager.GetFilteredGameplayRootTags(RootFilterString, TagItems);

	// Tag the assets as transactional so they can support undo/redo
	TArray<UObject*> ObjectsToMarkTransactional;
	if (PropertyHandle.IsValid())
	{
		// If we have a property handle use that to find the objects that need to be transactional
		PropertyHandle->GetOuterObjects(ObjectsToMarkTransactional);
	}
	else
	{
		// Otherwise use the owner list
		for (int32 AssetIdx = 0; AssetIdx < TagContainers.Num(); ++AssetIdx)
		{
			ObjectsToMarkTransactional.Add(TagContainers[AssetIdx].TagContainerOwner.Get());
		}
	}

	// Now actually mark the assembled objects
	for (UObject* ObjectToMark : ObjectsToMarkTransactional)
	{
		if (ObjectToMark)
		{
			ObjectToMark->SetFlags(RF_Transactional);
		}
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// Expandable UI controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SCheckBox )
					.IsChecked(this, &SGameplayTagWidget::GetAddTagSectionExpansionState) 
					.OnCheckStateChanged(this, &SGameplayTagWidget::OnAddTagSectionExpansionStateChanged)
					.CheckedImage(FEditorStyle::GetBrush("TreeArrow_Expanded"))
					.CheckedHoveredImage(FEditorStyle::GetBrush("TreeArrow_Expanded_Hovered"))
					.CheckedPressedImage(FEditorStyle::GetBrush("TreeArrow_Expanded"))
					.UncheckedImage(FEditorStyle::GetBrush("TreeArrow_Collapsed"))
					.UncheckedHoveredImage(FEditorStyle::GetBrush("TreeArrow_Collapsed_Hovered"))
					.UncheckedPressedImage(FEditorStyle::GetBrush("TreeArrow_Collapsed"))
					.Visibility( this, &SGameplayTagWidget::DetermineExpandableUIVisibility )
					[
						SNew( STextBlock )
						.Text( LOCTEXT("AddNewTag", "Add New Gameplay Tag"))
					]
				]
			]

			// Expandable UI content
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(16.0f, 0.0f)
			[
				SAssignNew( AddNewTagWidget, SAddNewGameplayTagWidget )
				.Visibility(this, &SGameplayTagWidget::DetermineAddNewTagWidgetVisibility)
				.OnGameplayTagAdded(this, &SGameplayTagWidget::OnGameplayTagAdded)
				.NewTagName(InArgs._NewTagName)
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryMiddle"))
				.Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
				.Visibility(this, &SGameplayTagWidget::DetermineAddNewTagWidgetVisibility)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("DetailsView.AdvancedDropdownBorder.Open"))
				]
			]

			// Gameplay Tag Tree controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)

				// Expand All nodes
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SGameplayTagWidget::OnExpandAllClicked)
					.Text(LOCTEXT("GameplayTagWidget_ExpandAll", "Expand All"))
				]
			
				// Collapse All nodes
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(this, &SGameplayTagWidget::OnCollapseAllClicked)
					.Text(LOCTEXT("GameplayTagWidget_CollapseAll", "Collapse All"))
				]

				// Clear selections
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(this, &SGameplayTagWidget::CanSelectTags)
					.OnClicked(this, &SGameplayTagWidget::OnClearAllClicked)
					.Text(LOCTEXT("GameplayTagWidget_ClearAll", "Clear All"))
					.Visibility(this, &SGameplayTagWidget::DetermineClearSelectionVisibility)
				]

				// Search
				+SHorizontalBox::Slot()
				.VAlign( VAlign_Center )
				.FillWidth(1.f)
				.Padding(5,1,5,1)
				[
					SAssignNew(SearchTagBox, SSearchBox)
					.HintText(LOCTEXT("GameplayTagWidget_SearchBoxHint", "Search Gameplay Tags"))
					.OnTextChanged( this, &SGameplayTagWidget::OnFilterTextChanged )
				]
			]

			// Gameplay Tags tree
			+SVerticalBox::Slot()
			.MaxHeight(MaxHeight)
			[
				SAssignNew(TagTreeContainerWidget, SBorder)
				.Padding(FMargin(4.f))
				[
					SAssignNew(TagTreeWidget, STreeView< TSharedPtr<FGameplayTagNode> >)
					.TreeItemsSource(&TagItems)
					.OnGenerateRow(this, &SGameplayTagWidget::OnGenerateRow)
					.OnGetChildren(this, &SGameplayTagWidget::OnGetChildren)
					.OnExpansionChanged( this, &SGameplayTagWidget::OnExpansionChanged)
					.SelectionMode(ESelectionMode::Multi)
				]
			]
		]
	];

	// Force the entire tree collapsed to start
	SetTagTreeItemExpansion(false);
	 
	LoadSettings();

	// Strip any invalid tags from the assets being edited
	VerifyAssetTagValidity();
}

void SGameplayTagWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bDelayRefresh)
{
		RefreshTags();
		bDelayRefresh = false;
}
	}

FVector2D SGameplayTagWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
	{
	FVector2D WidgetSize = SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);

	FVector2D TagTreeContainerSize = TagTreeContainerWidget->GetDesiredSize();

	if (TagTreeContainerSize.Y < MaxHeight)
	{
		WidgetSize.Y += MaxHeight - TagTreeContainerSize.Y;
	}

	return WidgetSize;
}

void SGameplayTagWidget::OnFilterTextChanged( const FText& InFilterText )
{
	FilterString = InFilterText.ToString();	

	if( FilterString.IsEmpty() )
	{
		TagTreeWidget->SetTreeItemsSource( &TagItems );

		for( int32 iItem = 0; iItem < TagItems.Num(); ++iItem )
		{
			SetDefaultTagNodeItemExpansion( TagItems[iItem] );
		}
	}
	else
	{
		FilteredTagItems.Empty();

		for( int32 iItem = 0; iItem < TagItems.Num(); ++iItem )
		{
			if( FilterChildrenCheck( TagItems[iItem] ) )
			{
				FilteredTagItems.Add( TagItems[iItem] );
				SetTagNodeItemExpansion( TagItems[iItem], true );
			}
			else
			{
				SetTagNodeItemExpansion( TagItems[iItem], false );
			}
		}

		TagTreeWidget->SetTreeItemsSource( &FilteredTagItems );	
	}
		
	TagTreeWidget->RequestTreeRefresh();	
}

bool SGameplayTagWidget::FilterChildrenCheck( TSharedPtr<FGameplayTagNode> InItem )
{
	if( !InItem.IsValid() )
	{
		return false;
	}

	auto FilterChildrenCheck_r = ([=]()
	{
		TArray< TSharedPtr<FGameplayTagNode> > Children = InItem->GetChildTagNodes();
		for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
		{
			if( FilterChildrenCheck( Children[iChild] ) )
			{
				return true;
			}
		}
		return false;
	});


	bool DelegateShouldHide = false;
	UGameplayTagsManager::Get().OnFilterGameplayTagChildren.Broadcast(RootFilterString, InItem, DelegateShouldHide);
	if (DelegateShouldHide)
	{
		// The delegate wants to hide, see if any children need to show
		return FilterChildrenCheck_r();
	}

	if( InItem->GetCompleteTagString().Contains( FilterString ) || FilterString.IsEmpty() )
	{
		return true;
	}

	return FilterChildrenCheck_r();
}

TSharedRef<ITableRow> SGameplayTagWidget::OnGenerateRow(TSharedPtr<FGameplayTagNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText TooltipText;
	if (InItem.IsValid())
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		FName TagName = InItem.Get()->GetCompleteTagName();
		FString TagComment;
		FName TagSource;

		Manager.GetTagEditorData(TagName, TagComment, TagSource);

		FString TooltipString = TagName.ToString();

		// Add Tag source in management mode
		if (GameplayTagUIMode == EGameplayTagUIMode::ManagementMode)
		{
			if (TagSource == NAME_None)
			{
				TagSource = FName(TEXT("Implicit"));
			}

			TooltipString.Append(FString::Printf(TEXT(" (%s)"), *TagSource.ToString()));
		}

		if (!TagComment.IsEmpty())
		{
			TooltipString.Append(FString::Printf(TEXT("\n\n%s"), *TagComment));
		}

		TooltipText = FText::FromString(TooltipString);
	}

	return SNew(STableRow< TSharedPtr<FGameplayTagNode> >, OwnerTable)
		.Style(FEditorStyle::Get(), "GameplayTagTreeView")
		[
			SNew( SHorizontalBox )

			// Tag Selection (selection mode only)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SGameplayTagWidget::OnTagCheckStatusChanged, InItem)
				.IsChecked(this, &SGameplayTagWidget::IsTagChecked, InItem)
				.ToolTipText(TooltipText)
				.IsEnabled(this, &SGameplayTagWidget::CanSelectTags)
				.Visibility( GameplayTagUIMode == EGameplayTagUIMode::SelectionMode ? EVisibility::Visible : EVisibility::Collapsed )
				[
					SNew(STextBlock)
					.Text(FText::FromName(InItem->GetSimpleTagName()))
				]
			]

			// Normal Tag Display (management mode only)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			[
				SNew( STextBlock )
				.ToolTip( FSlateApplication::Get().MakeToolTip(TooltipText) )
				.Text(FText::FromName( InItem->GetSimpleTagName()) )
				.Visibility( GameplayTagUIMode != EGameplayTagUIMode::SelectionMode ? EVisibility::Visible : EVisibility::Collapsed )
			]

			// Add Subtag
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew( SButton )
				.ToolTipText( LOCTEXT("AddSubtag", "Add Subtag") )
				.Visibility(this, &SGameplayTagWidget::DetermineExpandableUIVisibility)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.OnClicked( this, &SGameplayTagWidget::OnAddSubtagClicked, InItem )
				.DesiredSizeScale(FVector2D(0.75f, 0.75f))
				.ContentPadding(4.0f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsEnabled( !bReadOnly )
				.IsFocusable( false )
				[
					SNew( SImage )
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_AddToArray"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// More Actions Menu
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			[
				SNew( SComboButton )
				.ToolTipText( LOCTEXT("MoreActions", "More Actions...") )
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ContentPadding(0)
				.ForegroundColor(FSlateColor::UseForeground())
				.HasDownArrow(true)
				.MenuContent()
				[
					MakeTagActionsMenu(InItem)
				]
			]
		];
}

void SGameplayTagWidget::OnGetChildren(TSharedPtr<FGameplayTagNode> InItem, TArray< TSharedPtr<FGameplayTagNode> >& OutChildren)
{
	TArray< TSharedPtr<FGameplayTagNode> > FilteredChildren;
	TArray< TSharedPtr<FGameplayTagNode> > Children = InItem->GetChildTagNodes();

	for( int32 iChild = 0; iChild < Children.Num(); ++iChild )
	{
		if( FilterChildrenCheck( Children[iChild] ) )
		{
			FilteredChildren.Add( Children[iChild] );
		}
	}
	OutChildren += FilteredChildren;
}

void SGameplayTagWidget::OnTagCheckStatusChanged(ECheckBoxState NewCheckState, TSharedPtr<FGameplayTagNode> NodeChanged)
{
	if (NewCheckState == ECheckBoxState::Checked)
	{
		OnTagChecked(NodeChanged);
	}
	else if (NewCheckState == ECheckBoxState::Unchecked)
	{
		OnTagUnchecked(NodeChanged);
	}
}

void SGameplayTagWidget::OnTagChecked(TSharedPtr<FGameplayTagNode> NodeChecked)
{
	FScopedTransaction Transaction( LOCTEXT("GameplayTagWidget_AddTags", "Add Gameplay Tags") );

	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		TSharedPtr<FGameplayTagNode> CurNode(NodeChecked);
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EditableContainer = *Container;

			bool bRemoveParents = false;

			while (CurNode.IsValid())
			{
				FGameplayTag GameplayTag = CurNode->GetCompleteTag();

				if (bRemoveParents == false)
				{
					bRemoveParents = true;
					if (bMultiSelect == false)
					{
						EditableContainer.Reset();
					}
					EditableContainer.AddTag(GameplayTag);
				}
				else
				{
					EditableContainer.RemoveTag(GameplayTag);
				}

				CurNode = CurNode->GetParentTagNode();
			}
			SetContainer(Container, &EditableContainer, OwnerObj);
		}
	}
}

void SGameplayTagWidget::OnTagUnchecked(TSharedPtr<FGameplayTagNode> NodeUnchecked)
{
	FScopedTransaction Transaction( LOCTEXT("GameplayTagWidget_RemoveTags", "Remove Gameplay Tags"));
	if (NodeUnchecked.IsValid())
	{
		UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
			FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;
			FGameplayTag GameplayTag = NodeUnchecked->GetCompleteTag();

			if (Container)
			{
				FGameplayTagContainer EditableContainer = *Container;
				EditableContainer.RemoveTag(GameplayTag);

				TSharedPtr<FGameplayTagNode> ParentNode = NodeUnchecked->GetParentTagNode();
				if (ParentNode.IsValid())
				{
					// Check if there are other siblings before adding parent
					bool bOtherSiblings = false;
					for (auto It = ParentNode->GetChildTagNodes().CreateConstIterator(); It; ++It)
					{
						GameplayTag = It->Get()->GetCompleteTag();
						if (EditableContainer.HasTagExact(GameplayTag))
						{
							bOtherSiblings = true;
							break;
						}
					}
					// Add Parent
					if (!bOtherSiblings)
					{
						GameplayTag = ParentNode->GetCompleteTag();
						EditableContainer.AddTag(GameplayTag);
					}
				}

				// Uncheck Children
				for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
				{
					UncheckChildren(ChildNode, EditableContainer);
				}

				SetContainer(Container, &EditableContainer, OwnerObj);
			}
			
		}
	}
}

void SGameplayTagWidget::UncheckChildren(TSharedPtr<FGameplayTagNode> NodeUnchecked, FGameplayTagContainer& EditableContainer)
{
	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

	FGameplayTag GameplayTag = NodeUnchecked->GetCompleteTag();
	EditableContainer.RemoveTag(GameplayTag);

	// Uncheck Children
	for (const auto& ChildNode : NodeUnchecked->GetChildTagNodes())
	{
		UncheckChildren(ChildNode, EditableContainer);
	}
}

ECheckBoxState SGameplayTagWidget::IsTagChecked(TSharedPtr<FGameplayTagNode> Node) const
{
	int32 NumValidAssets = 0;
	int32 NumAssetsTagIsAppliedTo = 0;

	if (Node.IsValid())
	{
		UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

		for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
		{
			FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;
			if (Container)
			{
				NumValidAssets++;
				FGameplayTag GameplayTag = Node->GetCompleteTag();
				if (GameplayTag.IsValid())
				{
					if (Container->HasTag(GameplayTag))
					{
						++NumAssetsTagIsAppliedTo;
					}
				}
			}
		}
	}

	if (NumAssetsTagIsAppliedTo == 0)
	{
		return ECheckBoxState::Unchecked;
	}
	else if (NumAssetsTagIsAppliedTo == NumValidAssets)
	{
		return ECheckBoxState::Checked;
	}
	else
	{
		return ECheckBoxState::Undetermined;
	}
}

FReply SGameplayTagWidget::OnClearAllClicked()
{
	FScopedTransaction Transaction( LOCTEXT("GameplayTagWidget_RemoveAllTags", "Remove All Gameplay Tags") );

	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EmptyContainer;
			SetContainer(Container, &EmptyContainer, OwnerObj);
		}
	}
	return FReply::Handled();
}

FReply SGameplayTagWidget::OnExpandAllClicked()
{
	SetTagTreeItemExpansion(true);
	return FReply::Handled();
}

FReply SGameplayTagWidget::OnCollapseAllClicked()
{
	SetTagTreeItemExpansion(false);
	return FReply::Handled();
}

FReply SGameplayTagWidget::OnAddSubtagClicked(TSharedPtr<FGameplayTagNode> InTagNode)
{
	if (!bReadOnly && InTagNode.IsValid())
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		FString TagName = InTagNode->GetCompleteTagString();
		FString TagComment;
		FName TagSource;

		Manager.GetTagEditorData(InTagNode->GetCompleteTagName(), TagComment, TagSource);

		if (AddNewTagWidget.IsValid())
		{
			bAddTagSectionExpanded = true; 
			AddNewTagWidget->AddSubtagFromParent(TagName, TagSource);
		}
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SGameplayTagWidget::MakeTagActionsMenu(TSharedPtr<FGameplayTagNode> InTagNode)
{
	bool bShowManagement = (GameplayTagUIMode == EGameplayTagUIMode::ManagementMode && !bReadOnly);
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	if (!Manager.ShouldImportTagsFromINI())
	{
		bShowManagement = false;
	}

	FMenuBuilder MenuBuilder(true, NULL);

	// Rename
	if (bShowManagement)
	{
		FExecuteAction RenameAction = FExecuteAction::CreateSP(this, &SGameplayTagWidget::OnRenameTag, InTagNode);

		MenuBuilder.AddMenuEntry(LOCTEXT("GameplayTagWidget_RenameTag", "Rename"), LOCTEXT("GameplayTagWidget_RenameTagTooltip", "Rename this tag"), FSlateIcon(), FUIAction(RenameAction));
	}

	// Delete
	if (bShowManagement)
	{
		FExecuteAction DeleteAction = FExecuteAction::CreateSP(this, &SGameplayTagWidget::OnDeleteTag, InTagNode);

		MenuBuilder.AddMenuEntry(LOCTEXT("GameplayTagWidget_DeleteTag", "Delete"), LOCTEXT("GameplayTagWidget_DeleteTagTooltip", "Delete this tag"), FSlateIcon(), FUIAction(DeleteAction));
	}

	// Search for References
	if (IReferenceViewerModule::IsAvailable())
	{
		FExecuteAction SearchForReferencesAction = FExecuteAction::CreateSP(this, &SGameplayTagWidget::OnSearchForReferences, InTagNode);

		MenuBuilder.AddMenuEntry(LOCTEXT("GameplayTagWidget_SearchForReferences", "Search For References"), LOCTEXT("GameplayTagWidget_SearchForReferencesTooltip", "Find references for this tag"),
										 FSlateIcon(), FUIAction(SearchForReferencesAction));
	}

	return MenuBuilder.MakeWidget();
}

void SGameplayTagWidget::OnRenameTag(TSharedPtr<FGameplayTagNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		OpenRenameGameplayTagDialog( InTagNode );
	}
}

void SGameplayTagWidget::OnDeleteTag(TSharedPtr<FGameplayTagNode> InTagNode)
{
	if (InTagNode.IsValid())
	{
		IGameplayTagsEditorModule& TagsEditor = IGameplayTagsEditorModule::Get();

		const bool bDeleted = TagsEditor.DeleteTagFromINI(InTagNode->GetCompleteTagString());

		if (bDeleted)
		{
			OnTagChanged.ExecuteIfBound();
		}
	}
}

void SGameplayTagWidget::OnSearchForReferences(TSharedPtr<FGameplayTagNode> InTagNode)
{
	if (InTagNode.IsValid() && IReferenceViewerModule::IsAvailable())
	{
		IReferenceViewerModule& ReferenceViewer = IReferenceViewerModule::Get();

		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Add(FAssetIdentifier(FGameplayTag::StaticStruct(), InTagNode->GetCompleteTagName()));

		ReferenceViewer.InvokeReferenceViewerTab(AssetIdentifiers);
	}
}

void SGameplayTagWidget::SetTagTreeItemExpansion(bool bExpand)
{
	TArray< TSharedPtr<FGameplayTagNode> > TagArray;
	UGameplayTagsManager::Get().GetFilteredGameplayRootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		SetTagNodeItemExpansion(TagArray[TagIdx], bExpand);
	}
	
}

void SGameplayTagWidget::SetTagNodeItemExpansion(TSharedPtr<FGameplayTagNode> Node, bool bExpand)
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		TagTreeWidget->SetItemExpansion(Node, bExpand);

		const TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			SetTagNodeItemExpansion(ChildTags[ChildIdx], bExpand);
		}
	}
}

void SGameplayTagWidget::VerifyAssetTagValidity()
{
	FGameplayTagContainer LibraryTags;

	// Create a set that is the library of all valid tags
	TArray< TSharedPtr<FGameplayTagNode> > NodeStack;

	UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
	
	TagsManager.GetFilteredGameplayRootTags(TEXT(""), NodeStack);

	while (NodeStack.Num() > 0)
	{
		TSharedPtr<FGameplayTagNode> CurNode = NodeStack.Pop();
		if (CurNode.IsValid())
		{
			LibraryTags.AddTag(CurNode->GetCompleteTag());
			NodeStack.Append(CurNode->GetChildTagNodes());
		}
	}

	// Find and remove any tags on the asset that are no longer in the library
	for (int32 ContainerIdx = 0; ContainerIdx < TagContainers.Num(); ++ContainerIdx)
	{
		UObject* OwnerObj = TagContainers[ContainerIdx].TagContainerOwner.Get();
		FGameplayTagContainer* Container = TagContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EditableContainer = *Container;

			// Use a set instead of a container so we can find and remove None tags
			TSet<FGameplayTag> InvalidTags;

			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				FGameplayTag TagToCheck = *It;

				// Check redirectors, these will get fixed on load time
				UGameplayTagsManager::Get().RedirectSingleGameplayTag(TagToCheck, nullptr);

				if (!LibraryTags.HasTagExact(TagToCheck))
				{
					InvalidTags.Add(*It);
				}
			}
			if (InvalidTags.Num() > 0)
			{
				FString InvalidTagNames;

				for (auto InvalidIter = InvalidTags.CreateConstIterator(); InvalidIter; ++InvalidIter)
				{
					EditableContainer.RemoveTag(*InvalidIter);
					InvalidTagNames += InvalidIter->ToString() + TEXT("\n");
				}
				SetContainer(Container, &EditableContainer, OwnerObj);

				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Objects"), FText::FromString( InvalidTagNames ));
				FText DialogText = FText::Format( LOCTEXT("GameplayTagWidget_InvalidTags", "Invalid Tags that have been removed: \n\n{Objects}"), Arguments );
				OpenMsgDlgInt( EAppMsgType::Ok, DialogText, LOCTEXT("GameplayTagWidget_Warning", "Warning") );
			}
		}
	}
}

void SGameplayTagWidget::LoadSettings()
{
	TArray< TSharedPtr<FGameplayTagNode> > TagArray;
	UGameplayTagsManager::Get().GetFilteredGameplayRootTags(TEXT(""), TagArray);
	for (int32 TagIdx = 0; TagIdx < TagArray.Num(); ++TagIdx)
	{
		LoadTagNodeItemExpansion(TagArray[TagIdx] );
	}
}

void SGameplayTagWidget::SetDefaultTagNodeItemExpansion( TSharedPtr<FGameplayTagNode> Node )
{
	if ( Node.IsValid() && TagTreeWidget.IsValid() )
	{
		bool bExpanded = false;

		if ( IsTagChecked(Node) == ECheckBoxState::Checked )
		{
			bExpanded = true;
		}
		TagTreeWidget->SetItemExpansion(Node, bExpanded);

		const TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = Node->GetChildTagNodes();
		for ( int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx )
		{
			SetDefaultTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SGameplayTagWidget::LoadTagNodeItemExpansion( TSharedPtr<FGameplayTagNode> Node )
{
	if (Node.IsValid() && TagTreeWidget.IsValid())
	{
		bool bExpanded = false;

		if( GConfig->GetBool(*SettingsIniSection, *(TagContainerName + Node->GetCompleteTagString() + TEXT(".Expanded")), bExpanded, GEditorPerProjectIni) )
		{
			TagTreeWidget->SetItemExpansion( Node, bExpanded );
		}
		else if( IsTagChecked( Node ) == ECheckBoxState::Checked ) // If we have no save data but its ticked then we probably lost our settings so we shall expand it
		{
			TagTreeWidget->SetItemExpansion( Node, true );
		}

		const TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = Node->GetChildTagNodes();
		for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
		{
			LoadTagNodeItemExpansion(ChildTags[ChildIdx]);
		}
	}
}

void SGameplayTagWidget::OnExpansionChanged( TSharedPtr<FGameplayTagNode> InItem, bool bIsExpanded )
{
	// Save the new expansion setting to ini file
	GConfig->SetBool(*SettingsIniSection, *(TagContainerName + InItem->GetCompleteTagString() + TEXT(".Expanded")), bIsExpanded, GEditorPerProjectIni);
}

void SGameplayTagWidget::SetContainer(FGameplayTagContainer* OriginalContainer, FGameplayTagContainer* EditedContainer, UObject* OwnerObj)
{
	if (PropertyHandle.IsValid() && bMultiSelect)
	{
		// Case for a tag container 
		PropertyHandle->SetValueFromFormattedString(EditedContainer->ToString());
	}
	else if (PropertyHandle.IsValid() && !bMultiSelect)
	{
		// Case for a single Tag		
		FString FormattedString = TEXT("(TagName=\"");
		FormattedString += EditedContainer->First().GetTagName().ToString();
		FormattedString += TEXT("\")");
		PropertyHandle->SetValueFromFormattedString(FormattedString);
	}
	else
	{
		// Not sure if we should get here, means the property handle hasnt been setup which could be right or wrong.
		if (OwnerObj)
		{
			OwnerObj->PreEditChange(PropertyHandle.IsValid() ? PropertyHandle->GetProperty() : NULL);
		}

		*OriginalContainer = *EditedContainer;

		if (OwnerObj)
		{
			OwnerObj->PostEditChange();
		}
	}	

	if (!PropertyHandle.IsValid())
	{
		OnTagChanged.ExecuteIfBound();
	}
}

void SGameplayTagWidget::OnGameplayTagAdded(const FString& TagName, const FString& TagComment, const FName& TagSource)
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	RefreshTags();
	TagTreeWidget->RequestTreeRefresh();

	if (GameplayTagUIMode == EGameplayTagUIMode::SelectionMode)
	{
		TSharedPtr<FGameplayTagNode> TagNode = Manager.FindTagNode(FName(*TagName));
		if (TagNode.IsValid())
		{
			OnTagChecked(TagNode);
		}

		// Filter on the new tag
		SearchTagBox->SetText(FText::FromString(TagName));

		// Close the Add New Tag UI
		bAddTagSectionExpanded = false;
	}
}

void SGameplayTagWidget::RefreshTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	Manager.GetFilteredGameplayRootTags(RootFilterString, TagItems);

	TagTreeWidget->SetTreeItemsSource(&TagItems);
}

EVisibility SGameplayTagWidget::DetermineExpandableUIVisibility() const
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	if ( !Manager.ShouldImportTagsFromINI() )
	{
		// If we can't support adding tags from INI files, we should never see this widget
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SGameplayTagWidget::DetermineAddNewTagWidgetVisibility() const
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	if ( !Manager.ShouldImportTagsFromINI() || !bAddTagSectionExpanded )
	{
		// If we can't support adding tags from INI files, we should never see this widget
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SGameplayTagWidget::DetermineClearSelectionVisibility() const
{
	return CanSelectTags() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SGameplayTagWidget::CanSelectTags() const
{
	return !bReadOnly && GameplayTagUIMode == EGameplayTagUIMode::SelectionMode;
}

bool SGameplayTagWidget::IsAddingNewTag() const
{
	return AddNewTagWidget.IsValid() && AddNewTagWidget->IsAddingNewTag();
}

ECheckBoxState SGameplayTagWidget::GetAddTagSectionExpansionState() const
{
	return bAddTagSectionExpanded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SGameplayTagWidget::OnAddTagSectionExpansionStateChanged(ECheckBoxState NewState)
{
	bAddTagSectionExpanded = NewState == ECheckBoxState::Checked;
}

void SGameplayTagWidget::RefreshOnNextTick()
{
	bDelayRefresh = true;
}

void SGameplayTagWidget::OnGameplayTagRenamed(FString OldTagName, FString NewTagName)
{
	OnTagChanged.ExecuteIfBound();
}

void SGameplayTagWidget::OpenRenameGameplayTagDialog(TSharedPtr<FGameplayTagNode> GameplayTagNode) const
{
	TSharedRef<SWindow> RenameTagWindow =
		SNew(SWindow)
		.Title(LOCTEXT("RenameTagWindowTitle", "Rename Gameplay Tag"))
		.ClientSize(FVector2D(320.0f, 110.0f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SRenameGameplayTagDialog> RenameTagDialog =
		SNew(SRenameGameplayTagDialog)
		.GameplayTagNode(GameplayTagNode)
		.OnGameplayTagRenamed(this, &SGameplayTagWidget::OnGameplayTagRenamed);

	RenameTagWindow->SetContent(RenameTagDialog);

	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow( AsShared(), WidgetPath );

	FSlateApplication::Get().AddModalWindow(RenameTagWindow, CurrentWindow);
}

#undef LOCTEXT_NAMESPACE
