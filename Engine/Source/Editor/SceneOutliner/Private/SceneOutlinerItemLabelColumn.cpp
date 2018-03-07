// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerItemLabelColumn.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SOverlay.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIconFinder.h"
#include "ClassIconFinder.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "SceneOutlinerDragDrop.h"
#include "Widgets/Views/SListView.h"
#include "SortHelper.h"
#include "EditorActorFolders.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ActorEditorUtils.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerItemLabelColumn"

namespace SceneOutliner
{

struct FCommonLabelData
{
	TWeakPtr<ISceneOutliner> WeakSceneOutliner;
	static const FLinearColor DarkColor;

	TOptional<FLinearColor> GetForegroundColor(const FTreeItemPtr& TreeItem) const
	{
		if (!TreeItem.IsValid())
		{
			return DarkColor;
		}

		UWorld* World = TreeItem->GetSharedData().RepresentingWorld;

		// Darken items that aren't suitable targets for an active drag and drop action
		if (FSlateApplication::Get().IsDragDropping() && World)
		{
			TSharedPtr<FDragDropOperation> DragDropOp = FSlateApplication::Get().GetDragDroppingContent();

			FDragDropPayload DraggedObjects;
			if (DraggedObjects.ParseDrag(*DragDropOp) && !TreeItem->ValidateDrop(DraggedObjects, *World).IsValid())
			{
				return DarkColor;
			}
		}

		if (!TreeItem->CanInteract())
		{
			return DarkColor;
		}

		return TOptional<FLinearColor>();
	}
};

const FLinearColor FCommonLabelData::DarkColor(0.3f, 0.3f, 0.3f);

struct SActorTreeLabel : FCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SActorTreeLabel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FActorTreeItem& ActorItem, ISceneOutliner& SceneOutliner, const STableRow<FTreeItemPtr>& InRow)
	{
		WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());

		TreeItemPtr = StaticCastSharedRef<FActorTreeItem>(ActorItem.AsShared());
		ActorPtr = ActorItem.Actor;

		HighlightText = SceneOutliner.GetFilterHighlightText();

		MobilityStaticBrush = FEditorStyle::GetBrush( "ClassIcon.ComponentMobilityStaticPip" );
		MobilityStationaryBrush = FEditorStyle::GetBrush( "ClassIcon.ComponentMobilityStationaryPip" );
		MobilityMovableBrush = FEditorStyle::GetBrush( "ClassIcon.ComponentMobilityMovablePip" );

		TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

		auto MainContent = SNew(SHorizontalBox)

			// Main actor label
			+ SHorizontalBox::Slot()
			[
				SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
				.Text(this, &SActorTreeLabel::GetDisplayText)
				.ToolTipText(this, &SActorTreeLabel::GetTooltipText)
				.HighlightText(HighlightText)
				.ColorAndOpacity(this, &SActorTreeLabel::GetForegroundColor)
				.OnTextCommitted(this, &SActorTreeLabel::OnLabelCommitted)
				.OnVerifyTextChanged(this, &SActorTreeLabel::OnVerifyItemLabelChanged)
				.IsSelected(FIsSelected::CreateSP(&InRow, &STableRow<FTreeItemPtr>::IsSelectedExclusively))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 0.0f, 0.f, 3.0f, 0.0f )
			[
				SNew( STextBlock )
				.Text(this, &SActorTreeLabel::GetTypeText)
				.Visibility(this, &SActorTreeLabel::GetTypeTextVisibility)
				.HighlightText(HighlightText)
			];

		TSharedRef<SOverlay> IconContent = SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SActorTreeLabel::GetIcon)
				.ToolTipText(this, &SActorTreeLabel::GetIconTooltip)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &SActorTreeLabel::GetIconOverlay)
			];


		if (ActorItem.GetSharedData().Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			// Add the component mobility icon
			IconContent->AddSlot()
			.HAlign(HAlign_Left)
			[
				SNew(SImage)
				.Image(this, &SActorTreeLabel::GetBrushForComponentMobilityIcon)
			];

			ActorItem.RenameRequestEvent.BindSP( InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode );
		}
		
		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FDefaultTreeItemMetrics::IconPadding())
			[
				SNew(SBox)
				.WidthOverride(FDefaultTreeItemMetrics::IconSize())
				.HeightOverride(FDefaultTreeItemMetrics::IconSize())
				[
					IconContent
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 2.0f)
			[
				MainContent
			]
		];
	}

private:
	TWeakPtr<FActorTreeItem> TreeItemPtr;
	TWeakObjectPtr<AActor> ActorPtr;
	TAttribute<FText> HighlightText;

	/** The component mobility brushes */
	const FSlateBrush* MobilityStaticBrush;
	const FSlateBrush* MobilityStationaryBrush;
	const FSlateBrush* MobilityMovableBrush;

	FText GetDisplayText() const
	{
		const AActor* Actor = ActorPtr.Get();
		return Actor ? FText::FromString(Actor->GetActorLabel()) : LOCTEXT("ActorLabelForMissingActor", "(Deleted Actor)");
	}

	FText GetTooltipText() const
	{
		if (const AActor* Actor = ActorPtr.Get())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ID_Name"), LOCTEXT("CustomColumnMode_InternalName", "ID Name"));
			Args.Add(TEXT("Name"), FText::FromString(Actor->GetName()));
			return FText::Format(LOCTEXT("ActorNameTooltip", "{ID_Name}: {Name}"), Args);
		}

		return FText();
	}

	FText GetTypeText() const
	{
		if (const AActor* Actor = ActorPtr.Get())
		{
			return FText::FromName(Actor->GetClass()->GetFName());
		}			

		return FText();
	}

	EVisibility GetTypeTextVisibility() const
	{
		return HighlightText.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	const FSlateBrush* GetIcon() const
	{
		if (const AActor* Actor = ActorPtr.Get())
		{
			return FClassIconFinder::FindIconForActor(Actor);
		}
		else
		{
			return nullptr;
		}
	}

	const FSlateBrush* GetIconOverlay() const
	{
		static const FName SequencerActorTag(TEXT("SequencerActor"));

		if (const AActor* Actor = ActorPtr.Get())
		{
			if (Actor->ActorHasTag(SequencerActorTag))
			{
				return FEditorStyle::GetBrush("Sequencer.SpawnableIconOverlay");
			}
		}
		return nullptr;
	}

	FText GetIconTooltip() const
	{
		auto TreeItem = TreeItemPtr.Pin();
		if (!TreeItem.IsValid())
		{
			return FText();
		}

		FText ToolTipText;
		if (AActor* Actor = ActorPtr.Get())
		{
			ToolTipText = FText::FromString( Actor->GetClass()->GetName() );
			if (TreeItem->GetSharedData().Mode == ESceneOutlinerMode::ActorBrowsing)
			{
				USceneComponent* RootComponent = Actor->GetRootComponent();
				if (RootComponent)
				{
					FFormatNamedArguments Args;
					Args.Add( TEXT("ActorClassName"), ToolTipText );

					if( RootComponent->Mobility == EComponentMobility::Static )
					{
						ToolTipText = FText::Format( LOCTEXT( "ComponentMobility_Static", "{ActorClassName} with static mobility" ), Args );
					}
					else if( RootComponent->Mobility == EComponentMobility::Stationary )
					{
						ToolTipText = FText::Format( LOCTEXT( "ComponentMobility_Stationary", "{ActorClassName} with stationary mobility" ), Args );
					}
					else if( RootComponent->Mobility == EComponentMobility::Movable )
					{
						ToolTipText = FText::Format( LOCTEXT( "ComponentMobility_Movable", "{ActorClassName} with movable mobility" ), Args );
					}
				}
			}
		}

		return ToolTipText;
	}

	FSlateColor GetForegroundColor() const
	{
		auto TreeItem = TreeItemPtr.Pin();
		if (auto BaseColor = FCommonLabelData::GetForegroundColor(TreeItem))
		{
			return BaseColor.GetValue();
		}

		AActor* Actor = ActorPtr.Get();
		if (!Actor)
		{
			// Deleted actor!
			return FLinearColor(0.2f, 0.2f, 0.25f);
		}

		if (TreeItem->GetSharedData().bRepresentingPlayWorld && !TreeItem->bExistsInCurrentWorldAndPIE)
		{
			// Highlight actors that are exclusive to PlayWorld
			return FLinearColor(0.9f, 0.8f, 0.4f);
		}

		// also darken items that are non selectable in the active mode(s)
		const bool bInSelected = true;
		const bool bSelectEvenIfHidden = true;		// @todo outliner: Is this actually OK?
		if (!GEditor->CanSelectActor(Actor, bInSelected, bSelectEvenIfHidden))
		{
			return FCommonLabelData::DarkColor;
		}

		return FSlateColor::UseForeground();
	}

	bool OnVerifyItemLabelChanged(const FText& InLabel, FText& OutErrorMessage)
	{
		return FActorEditorUtils::ValidateActorName(InLabel, OutErrorMessage);
	}

	void OnLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo)
	{
		auto* Actor = ActorPtr.Get();
		if (Actor && Actor->IsActorLabelEditable() && !InLabel.ToString().Equals(Actor->GetActorLabel(), ESearchCase::CaseSensitive))
		{
			const FScopedTransaction Transaction( LOCTEXT( "SceneOutlinerRenameActorTransaction", "Rename Actor" ) );
			FActorLabelUtilities::RenameExistingActor(Actor, InLabel.ToString());

			auto Outliner = WeakSceneOutliner.Pin();
			if (Outliner.IsValid())
			{
				Outliner->SetKeyboardFocus();
			}
		}
	}

	const FSlateBrush* GetBrushForComponentMobilityIcon() const
	{
		const FSlateBrush* IconBrush = MobilityStaticBrush;

		if (auto* Actor = ActorPtr.Get())
		{
			USceneComponent* RootComponent = ActorPtr->GetRootComponent();
			if (RootComponent)
			{
				if (RootComponent->Mobility == EComponentMobility::Stationary)
				{
					IconBrush = MobilityStationaryBrush;
				}
				else if (RootComponent->Mobility == EComponentMobility::Movable)
				{
					IconBrush = MobilityMovableBrush;
				}
			}
		}

		return IconBrush;
	}
};

struct SWorldTreeLabel : FCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SWorldTreeLabel){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FWorldTreeItem& WorldItem, ISceneOutliner& SceneOutliner, const STableRow<FTreeItemPtr>& InRow)
	{
		TreeItemPtr = StaticCastSharedRef<FWorldTreeItem>(WorldItem.AsShared());
		WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());

		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FDefaultTreeItemMetrics::IconPadding())
			[
				SNew(SBox)
				.WidthOverride(FDefaultTreeItemMetrics::IconSize())
				.HeightOverride(FDefaultTreeItemMetrics::IconSize())
				[
					SNew(SImage)
					.Image(FSlateIconFinder::FindIconBrushForClass(UWorld::StaticClass()))
					.ToolTipText(LOCTEXT("WorldIcon_Tooltip", "World"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SWorldTreeLabel::GetDisplayText)
				.HighlightText(SceneOutliner.GetFilterHighlightText())
				.ColorAndOpacity(this, &SWorldTreeLabel::GetForegroundColor)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
					TAttribute<FText>(this, &SWorldTreeLabel::GetTooltipText),
					nullptr,
					TEXT("Shared/LevelEditor/SceneOutliner"),
					TEXT("WorldSettingsLabel")))
			]
		];
	}

private:
	TWeakPtr<FWorldTreeItem> TreeItemPtr;

	FText GetDisplayText() const
	{
		auto Item = TreeItemPtr.Pin();
		return Item.IsValid() ? FText::FromString(Item->GetDisplayString()) : FText();
	}

	FText GetTooltipText() const
	{
		auto Item = TreeItemPtr.Pin();
		FText PersistentLevelDisplayName = Item.IsValid() ? FText::FromString(Item->GetWorldName()) : FText();
		if(Item->CanInteract())
		{
			return FText::Format(LOCTEXT("WorldLabel_Tooltip", "The world settings for {0}, double-click to edit"), PersistentLevelDisplayName);
		}
		else
		{
			return FText::Format(LOCTEXT("WorldLabel_TooltipNonInteractive", "The world {0}"), PersistentLevelDisplayName);
		}
	}

	FSlateColor GetForegroundColor() const
	{
		if (auto BaseColor = FCommonLabelData::GetForegroundColor(TreeItemPtr.Pin()))
		{
			return BaseColor.GetValue();
		}
		
		return FSlateColor::UseForeground();
	}
};

struct SFolderTreeLabel : FCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SFolderTreeLabel){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FFolderTreeItem& FolderItem, ISceneOutliner& SceneOutliner, const STableRow<FTreeItemPtr>& InRow)
	{
		TreeItemPtr = StaticCastSharedRef<FFolderTreeItem>(FolderItem.AsShared());
		WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());

		TSharedPtr<SInlineEditableTextBlock> InlineTextBlock = SNew(SInlineEditableTextBlock)
			.Text(this, &SFolderTreeLabel::GetDisplayText)
			.HighlightText(SceneOutliner.GetFilterHighlightText())
			.ColorAndOpacity(this, &SFolderTreeLabel::GetForegroundColor)
			.OnTextCommitted(this, &SFolderTreeLabel::OnLabelCommitted)
			.OnVerifyTextChanged(this, &SFolderTreeLabel::OnVerifyItemLabelChanged)
			.IsSelected(FIsSelected::CreateSP(&InRow, &STableRow<FTreeItemPtr>::IsSelectedExclusively));

		if (SceneOutliner.GetSharedData().Mode == ESceneOutlinerMode::ActorBrowsing)
		{
			FolderItem.RenameRequestEvent.BindSP( InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode );
		}

		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FDefaultTreeItemMetrics::IconPadding())
			[
				SNew(SBox)
				.WidthOverride(FDefaultTreeItemMetrics::IconSize())
				[
					SNew(SImage)
					.Image(this, &SFolderTreeLabel::GetIcon)
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 2.0f)
			[
				InlineTextBlock.ToSharedRef()
			]
		];
	}

private:
	TWeakPtr<FFolderTreeItem> TreeItemPtr;

	FText GetDisplayText() const
	{
		auto Folder = TreeItemPtr.Pin();
		return Folder.IsValid() ? FText::FromName(Folder->LeafName) : FText();
	}

	const FSlateBrush* GetIcon() const
	{	
		auto TreeItem = TreeItemPtr.Pin();
		if (!TreeItem.IsValid())
		{
			return FEditorStyle::Get().GetBrush(TEXT("SceneOutliner.FolderClosed"));
		}

		if (TreeItem->Flags.bIsExpanded && TreeItem->GetChildren().Num())
		{
			return FEditorStyle::Get().GetBrush(TEXT("SceneOutliner.FolderOpen"));
		}
		else
		{
			return FEditorStyle::Get().GetBrush(TEXT("SceneOutliner.FolderClosed"));
		}
	}

	FSlateColor GetForegroundColor() const
	{
		if (auto BaseColor = FCommonLabelData::GetForegroundColor(TreeItemPtr.Pin()))
		{
			return BaseColor.GetValue();
		}

		return FSlateColor::UseForeground();
	}

	bool OnVerifyItemLabelChanged( const FText& InLabel, FText& OutErrorMessage)
	{
		auto TreeItem = TreeItemPtr.Pin();
		if (!TreeItem.IsValid())
		{
			OutErrorMessage = LOCTEXT( "RenameFailed_TreeItemDeleted", "Tree item no longer exists" );
			return false;
		}

		FText TrimmedLabel = FText::TrimPrecedingAndTrailing(InLabel);

		if (TrimmedLabel.IsEmpty())
		{
			OutErrorMessage = LOCTEXT( "RenameFailed_LeftBlank", "Names cannot be left blank" );
			return false;
		}

		if (TrimmedLabel.ToString().Len() >= NAME_SIZE)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add( TEXT("CharCount"), NAME_SIZE );
			OutErrorMessage = FText::Format(LOCTEXT("RenameFailed_TooLong", "Names must be less than {CharCount} characters long."), Arguments);
			return false;
		}

		FString LabelString = TrimmedLabel.ToString();
		if (TreeItem->LeafName.ToString() == LabelString)
		{
			return true;
		}

		int32 Dummy = 0;
		if (LabelString.FindChar('/', Dummy) || LabelString.FindChar('\\', Dummy))
		{
			OutErrorMessage = LOCTEXT("RenameFailed_InvalidChar", "Folder names cannot contain / or \\.");
			return false;
		}

		// Validate that this folder doesn't exist already
		FName NewPath = GetParentPath(TreeItem->Path);
		if (NewPath.IsNone())
		{
			NewPath = FName(*LabelString);
		}
		else
		{
			NewPath = FName(*(NewPath.ToString() / LabelString));
		}

		if (FActorFolders::Get().GetFolderProperties(*GWorld, NewPath))
		{
			OutErrorMessage = LOCTEXT("RenameFailed_AlreadyExists", "A folder with this name already exists at this level");
			return false;
		}

		return true;
	}

	void OnLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo)
	{
		auto TreeItem = TreeItemPtr.Pin();
		if (TreeItem.IsValid() && !InLabel.ToString().Equals(TreeItem->LeafName.ToString(), ESearchCase::CaseSensitive))
		{
			// Rename the item
			FName NewPath = GetParentPath(TreeItem->Path);
			if (NewPath.IsNone())
			{
				NewPath = FName(*InLabel.ToString());
			}
			else
			{
				NewPath = FName(*(NewPath.ToString() / InLabel.ToString()));
			}

			FActorFolders::Get().RenameFolderInWorld(*GWorld, TreeItem->Path, NewPath);

			auto Outliner = WeakSceneOutliner.Pin();
			if (Outliner.IsValid())
			{
				Outliner->SetKeyboardFocus();
			}
		}
	}
};

FName FItemLabelColumn::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FItemLabelColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
		.DefaultLabel(LOCTEXT("ItemLabel_HeaderText", "Label"))
		.FillWidth( 5.0f );
}
const TSharedRef<SWidget> FItemLabelColumn::ConstructRowWidget(FTreeItemRef TreeItem, const STableRow<FTreeItemPtr>& Row)
{
	FColumnWidgetGenerator Generator(*this, Row);
	TreeItem->Visit(Generator);
	return Generator.Widget.ToSharedRef();
}

void FItemLabelColumn::PopulateSearchStrings( const ITreeItem& Item, TArray< FString >& OutSearchStrings ) const
{
	OutSearchStrings.Add(Item.GetDisplayString());
}

void FItemLabelColumn::SortItems(TArray<FTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const
{
	typedef FSortHelper<int32, FNumericStringWrapper> FSort;

	FSort()
		.Primary([](const ITreeItem& Item){ return Item.GetTypeSortPriority(); }, 	SortMode)
		.Secondary([](const ITreeItem& Item){ return FNumericStringWrapper(Item.GetDisplayString()); }, 	SortMode)
		.Sort(OutItems);
}

TSharedRef<SWidget> FItemLabelColumn::GenerateWidget( FActorTreeItem& TreeItem, const STableRow<FTreeItemPtr>& InRow )
{
	ISceneOutliner* Outliner = WeakSceneOutliner.Pin().Get();
	check(Outliner);
	return SNew(SActorTreeLabel, TreeItem, *Outliner, InRow);
}

TSharedRef<SWidget> FItemLabelColumn::GenerateWidget( FWorldTreeItem& TreeItem, const STableRow<FTreeItemPtr>& InRow )
{
	ISceneOutliner* Outliner = WeakSceneOutliner.Pin().Get();
	check(Outliner);
	return SNew(SWorldTreeLabel, TreeItem, *Outliner, InRow);
}

TSharedRef<SWidget> FItemLabelColumn::GenerateWidget( FFolderTreeItem& TreeItem, const STableRow<FTreeItemPtr>& InRow )
{
	ISceneOutliner* Outliner = WeakSceneOutliner.Pin().Get();
	check(Outliner);
	return SNew(SFolderTreeLabel, TreeItem, *Outliner, InRow);
}

}	// namespace SceneOutliner

#undef LOCTEXT_NAMESPACE
