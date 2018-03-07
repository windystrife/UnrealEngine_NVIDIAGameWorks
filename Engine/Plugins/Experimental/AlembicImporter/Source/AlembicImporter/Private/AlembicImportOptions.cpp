// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AlembicImportOptions.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "AbcImportSettings.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"

#include "STrackSelectionTableRow.h"

#define LOCTEXT_NAMESPACE "AlembicImportOptions"

void SAlembicImportOptions::Construct(const FArguments& InArgs)
{
	ImportSettings = InArgs._ImportSettings;
	WidgetWindow = InArgs._WidgetWindow;
	PolyMeshes = InArgs._PolyMeshes;

	check(ImportSettings);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(ImportSettings);

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("Import_CurrentFileTitle", "Current File: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("CurveEd.InfoFont"))
					.Text(InArgs._FullPath)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SBox)				
				.MinDesiredWidth(512.0f)
				.MaxDesiredHeight(350.0f)
				[
					SNew(SListView<FAbcPolyMeshObjectPtr>)
					.ItemHeight(24)						
					.ScrollbarVisibility(EVisibility::Visible)
					.ListItemsSource(&PolyMeshes)
					.OnMouseButtonDoubleClick(this, &SAlembicImportOptions::OnItemDoubleClicked)
					.OnGenerateRow(this, &SAlembicImportOptions::OnGenerateWidgetForList)
					.HeaderRow
					(
						SNew(SHeaderRow)

						+ SHeaderRow::Column("ShouldImport")
						.DefaultLabel(FText::FromString(TEXT("Include")))
						.FillWidth(0.1f)

						+ SHeaderRow::Column("TrackName")
						.DefaultLabel(LOCTEXT("TrackNameHeader", "Track Name"))
						.FillWidth(0.45f)
							
						+ SHeaderRow::Column("TrackFrameStart")
						.DefaultLabel(LOCTEXT("TrackFrameStartHeader", "Start Frame"))
						.FillWidth(0.15f)

						+ SHeaderRow::Column("TrackFrameEnd")
						.DefaultLabel(LOCTEXT("TrackFrameEndHeader", "End Frame"))
						.FillWidth(0.15f)

						+ SHeaderRow::Column("TrackFrameNum")
						.DefaultLabel(LOCTEXT("TrackFrameNumHeader", "Num Frames"))
						.FillWidth(0.15f)
					)
				]
			]
		]


		+ SVerticalBox::Slot()
		.Padding(2)
		.MaxHeight(500.0f)
		[
			DetailsView->AsShared()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
			+ SUniformGridPanel::Slot(0, 0)
			[
				SAssignNew(ImportButton, SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("AlembicOptionWindow_Import", "Import"))
				.IsEnabled(this, &SAlembicImportOptions::CanImport)
				.OnClicked(this, &SAlembicImportOptions::OnImport)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("AlembicOptionWindow_Cancel", "Cancel"))
				.ToolTipText(LOCTEXT("AlembicOptionWindow_Cancel_ToolTip", "Cancels importing this Alembic file"))
				.OnClicked(this, &SAlembicImportOptions::OnCancel)
			]
		]
	];
}

TSharedRef<ITableRow> SAlembicImportOptions::OnGenerateWidgetForList(FAbcPolyMeshObjectPtr InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STrackSelectionTableRow, OwnerTable)
		.PolyMesh(InItem);
}

bool SAlembicImportOptions::CanImport()  const
{
	return true;
}

void SAlembicImportOptions::OnItemDoubleClicked(FAbcPolyMeshObjectPtr ClickedItem)
{
	for (FAbcPolyMeshObjectPtr Item : PolyMeshes)
	{
		Item->bShouldImport = (Item == ClickedItem);
	}
}

#undef LOCTEXT_NAMESPACE

