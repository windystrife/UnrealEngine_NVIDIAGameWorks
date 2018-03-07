// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerInfoColumn.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "ISceneOutliner.h"
#include "Sequencer.h"
#include "LevelEditorSequencerIntegration.h"

#include "EditorClassUtils.h"
#include "SortHelper.h"

#define LOCTEXT_NAMESPACE "SequencerInfoColumn"

namespace Sequencer
{

struct FGetInfo : SceneOutliner::TTreeItemGetter<FString>
{
	FGetInfo(const FSequencerInfoColumn& InColumn)
		: WeakColumn(StaticCastSharedRef<const FSequencerInfoColumn>(InColumn.AsShared()))
	{}

	virtual FString Get(const SceneOutliner::FActorTreeItem& ActorItem) const override
	{
		if (!WeakColumn.IsValid())
		{
			return FString();
		}

		AActor* Actor = ActorItem.Actor.Get();
		if (!Actor)
		{
			return FString();
		}

		const FSequencerInfoColumn& Column = *WeakColumn.Pin();

		return Column.GetTextForActor(Actor);
	}


	/** Weak reference to the sequencer info column */
	TWeakPtr< const FSequencerInfoColumn > WeakColumn;
};


FSequencerInfoColumn::FSequencerInfoColumn(ISceneOutliner& InSceneOutliner, FSequencer& InSequencer, const FLevelEditorSequencerBindingData& InBindingData)
	: WeakSceneOutliner(StaticCastSharedRef<ISceneOutliner>(InSceneOutliner.AsShared())) 
	, WeakSequencer(StaticCastSharedRef<FSequencer>(InSequencer.AsShared()))
	, WeakBindingData(ConstCastSharedRef<FLevelEditorSequencerBindingData>(InBindingData.AsShared()))
{
}

FSequencerInfoColumn::FSequencerInfoColumn(ISceneOutliner& InSceneOutliner)
	: WeakSceneOutliner(StaticCastSharedRef<ISceneOutliner>(InSceneOutliner.AsShared()))
{
}

FSequencerInfoColumn::~FSequencerInfoColumn()
{
}

FName FSequencerInfoColumn::GetID()
{
	static FName IDName("Sequence");
	return IDName;
}


FName FSequencerInfoColumn::GetColumnID()
{
	return GetID();
}


SHeaderRow::FColumn::FArguments FSequencerInfoColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
		.DefaultLabel(LOCTEXT("ItemLabel_HeaderText", "Sequence"))
		.FillWidth( 5.0f );
}

const TSharedRef< SWidget > FSequencerInfoColumn::ConstructRowWidget( SceneOutliner::FTreeItemRef TreeItem, const STableRow<SceneOutliner::FTreeItemPtr>& Row )
{
	auto SceneOutliner = WeakSceneOutliner.Pin();
	check(SceneOutliner.IsValid());

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	TSharedRef<STextBlock> MainText = SNew( STextBlock )
		.Text( this, &FSequencerInfoColumn::GetTextForItem, TWeakPtr<SceneOutliner::ITreeItem>(TreeItem) )
		.HighlightText( SceneOutliner->GetFilterHighlightText() )
		.ColorAndOpacity( FSlateColor::UseSubduedForeground() );

	HorizontalBox->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	[
		MainText
	];

	return HorizontalBox;
}


void FSequencerInfoColumn::PopulateSearchStrings( const SceneOutliner::ITreeItem& Item, TArray< FString >& OutSearchStrings ) const
{
	OutSearchStrings.Add(Item.GetDisplayString());
}


void FSequencerInfoColumn::SortItems(TArray<SceneOutliner::FTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const
{
	if (WeakBindingData.IsValid())
	{
		SceneOutliner::FSortHelper<FString>()
			.Primary(FGetInfo(*this), SortMode)
			.Sort(OutItems);
	}
}

FString FSequencerInfoColumn::GetTextForActor(AActor* InActor) const
{
	if (WeakBindingData.IsValid() && WeakSequencer.IsValid())
	{
		return WeakBindingData.Pin()->GetLevelSequencesForActor(WeakSequencer.Pin(), InActor);
	}

	return FString();
}

FText FSequencerInfoColumn::GetTextForItem( TWeakPtr<SceneOutliner::ITreeItem> TreeItem ) const
{
	auto Item = TreeItem.Pin();
	return Item.IsValid() && WeakBindingData.IsValid() ? FText::FromString(Item->Get(FGetInfo(*this))) : FText::GetEmpty();
}

}

#undef LOCTEXT_NAMESPACE
