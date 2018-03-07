// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagQueryWidget.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "GameplayTagQueryWidget"

void SGameplayTagQueryWidget::Construct(const FArguments& InArgs, const TArray<FEditableGameplayTagQueryDatum>& EditableTagQueries)
{
	ensure(EditableTagQueries.Num() > 0);
	TagQueries = EditableTagQueries;

	bReadOnly = InArgs._ReadOnly;
	bAutoSave = InArgs._AutoSave;
	OnSaveAndClose = InArgs._OnSaveAndClose;
	OnCancel = InArgs._OnCancel;
	OnQueryChanged = InArgs._OnQueryChanged;

	// Tag the assets as transactional so they can support undo/redo
	for (int32 AssetIdx = 0; AssetIdx < TagQueries.Num(); ++AssetIdx)
	{
		UObject* TagQueryOwner = TagQueries[AssetIdx].TagQueryOwner.Get();
		if (TagQueryOwner)
		{
			TagQueryOwner->SetFlags(RF_Transactional);
		}
	}

	// build editable query object tree from the runtime query data
	UEditableGameplayTagQuery* const EQ = CreateEditableQuery(*TagQueries[0].TagQuery);
	EditableQuery = EQ;

	// create details view for the editable query object
	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = true;
	ViewArgs.bShowActorLabel = false;
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	Details = PropertyModule.CreateDetailView(ViewArgs);
	Details->SetObject(EQ);
	Details->OnFinishedChangingProperties().AddSP(this, &SGameplayTagQueryWidget::OnFinishedChangingProperties);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(!bReadOnly)
					.Visibility(this, &SGameplayTagQueryWidget::GetSaveAndCloseButtonVisibility)
					.OnClicked(this, &SGameplayTagQueryWidget::OnSaveAndCloseClicked)
					.Text(LOCTEXT("GameplayTagQueryWidget_SaveAndClose", "Save and Close"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility(this, &SGameplayTagQueryWidget::GetCancelButtonVisibility)
					.OnClicked(this, &SGameplayTagQueryWidget::OnCancelClicked)
					.Text(LOCTEXT("GameplayTagQueryWidget_Cancel", "Close Without Saving"))
				]
			]
			// to delete!
			+ SVerticalBox::Slot()
			[
				Details.ToSharedRef()
			]
		]
	];
}

void SGameplayTagQueryWidget::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bAutoSave)
	{
		SaveToTagQuery();
	}

	OnQueryChanged.ExecuteIfBound();
}

EVisibility SGameplayTagQueryWidget::GetSaveAndCloseButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SGameplayTagQueryWidget::GetCancelButtonVisibility() const
{
	return bAutoSave ? EVisibility::Collapsed : EVisibility::Visible;
}

UEditableGameplayTagQuery* SGameplayTagQueryWidget::CreateEditableQuery(FGameplayTagQuery& Q)
{
	UEditableGameplayTagQuery* const AnEditableQuery = Q.CreateEditableQuery();
	if (AnEditableQuery)
	{
		// prevent GC, will explicitly remove from root later
		AnEditableQuery->AddToRoot();
	}

	return AnEditableQuery;
}

SGameplayTagQueryWidget::~SGameplayTagQueryWidget()
{
	// clean up our temp editing uobjects
	UEditableGameplayTagQuery* const Q = EditableQuery.Get();
	if (Q)
	{
		Q->RemoveFromRoot();
	}
}


void SGameplayTagQueryWidget::SaveToTagQuery()
{
	// translate obj tree to token stream
	if (EditableQuery.IsValid() && !bReadOnly)
	{
		// write to all selected queries
		for (auto& TQ : TagQueries)
		{
			TQ.TagQuery->BuildFromEditableQuery(*EditableQuery.Get());
			if (TQ.TagQueryExportText != nullptr)
			{
				*TQ.TagQueryExportText = EditableQuery.Get()->GetTagQueryExportText(*TQ.TagQuery);
			}
			if (TQ.TagQueryOwner.IsValid())
			{
				TQ.TagQueryOwner->MarkPackageDirty();
			}
		}
	}
}

FReply SGameplayTagQueryWidget::OnSaveAndCloseClicked()
{
	SaveToTagQuery();

	OnSaveAndClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SGameplayTagQueryWidget::OnCancelClicked()
{
	OnCancel.ExecuteIfBound();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
