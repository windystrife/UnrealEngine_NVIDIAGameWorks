// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "FbxOptionWindow.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "IDocumentation.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "FBXOption"

void SFbxOptionWindow::Construct(const FArguments& InArgs)
{
	ImportUI = InArgs._ImportUI;
	WidgetWindow = InArgs._WidgetWindow;
	bIsObjFormat = InArgs._IsObjFormat;
	OnPreviewFbxImport = InArgs._OnPreviewFbxImport;

	check (ImportUI);
	
	TSharedPtr<SBox> ImportTypeDisplay;
	TSharedPtr<SHorizontalBox> FbxHeaderButtons;
	TSharedPtr<SBox> InspectorBox;
	this->ChildSlot
	[
		SNew(SBox)
		.MaxDesiredHeight(InArgs._MaxWindowHeight)
		.MaxDesiredWidth(InArgs._MaxWindowWidth)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SAssignNew(ImportTypeDisplay, SBox)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("CurveEd.LabelFont"))
						.Text(LOCTEXT("Import_CurrentFileTitle", "Current File: "))
					]
					+SHorizontalBox::Slot()
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
				SAssignNew(InspectorBox, SBox)
				.MaxDesiredHeight(650.0f)
				.WidthOverride(400.0f)
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
					IDocumentation::Get()->CreateAnchor(FString("Engine/Content/FBX/ImportOptions"))
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_ImportAll", "Import All"))
					.ToolTipText(LOCTEXT("FbxOptionWindow_ImportAll_ToolTip", "Import all files with these same settings"))
					.IsEnabled(this, &SFbxOptionWindow::CanImport)
					.OnClicked(this, &SFbxOptionWindow::OnImportAll)
				]
				+ SUniformGridPanel::Slot(2, 0)
				[
					SAssignNew(ImportButton, SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_Import", "Import"))
					.IsEnabled(this, &SFbxOptionWindow::CanImport)
					.OnClicked(this, &SFbxOptionWindow::OnImport)
				]
				+ SUniformGridPanel::Slot(3, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("FbxOptionWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("FbxOptionWindow_Cancel_ToolTip", "Cancels importing this FBX file"))
					.OnClicked(this, &SFbxOptionWindow::OnCancel)
				]
			]
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InspectorBox->SetContent(DetailsView->AsShared());

	ImportTypeDisplay->SetContent(
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SFbxOptionWindow::GetImportTypeDisplayText)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SAssignNew(FbxHeaderButtons, SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(2.0f, 0.0f))
					[
						SNew(SButton)
						.Text(LOCTEXT("FbxOptionWindow_ResetOptions", "Reset to Default"))
						.OnClicked(this, &SFbxOptionWindow::OnResetToDefaultClick)
					]
				]
			]
		]
	);

	if (ImportUI->bIsReimport && OnPreviewFbxImport.IsBound())
	{
		FbxHeaderButtons->AddSlot()
		.AutoWidth()
		.Padding(FMargin(2.0f, 0.0f))
		[
			//Create the fbx import preview button
			SNew(SButton)
			.Text(LOCTEXT("FbxOptionWindow_Preview", "Preview..."))
			.OnClicked(this, &SFbxOptionWindow::OnPreviewClick)
		];
	}

	DetailsView->SetObject(ImportUI);
}

FReply SFbxOptionWindow::OnPreviewClick() const
{
	//Pop a preview window to let the user see the content of the fbx file
	OnPreviewFbxImport.ExecuteIfBound();
	return FReply::Handled();
}

FReply SFbxOptionWindow::OnResetToDefaultClick() const
{
	ImportUI->ResetToDefault();
	//Refresh the view to make sure the custom UI are updating correctly
	DetailsView->SetObject(ImportUI, true);
	return FReply::Handled();
}

FText SFbxOptionWindow::GetImportTypeDisplayText() const
{
	switch (ImportUI->MeshTypeToImport)
	{
	case EFBXImportType::FBXIT_Animation :
		return ImportUI->bIsReimport ? LOCTEXT("FbxOptionWindow_ReImportTypeAnim", "Reimport Animation") : LOCTEXT("FbxOptionWindow_ImportTypeAnim", "Import Animation");
	case EFBXImportType::FBXIT_SkeletalMesh:
		return ImportUI->bIsReimport ? LOCTEXT("FbxOptionWindow_ReImportTypeSK", "Reimport Skeletal Mesh") : LOCTEXT("FbxOptionWindow_ImportTypeSK", "Import Skeletal Mesh");
	case EFBXImportType::FBXIT_StaticMesh:
		return ImportUI->bIsReimport ? LOCTEXT("FbxOptionWindow_ReImportTypeSM", "Reimport Static Mesh") : LOCTEXT("FbxOptionWindow_ImportTypeSM", "Import Static Mesh");
	}
	return FText::GetEmpty();
}

bool SFbxOptionWindow::CanImport()  const
{
	// do test to see if we are ready to import
	if (ImportUI->MeshTypeToImport == FBXIT_Animation)
	{
		if (ImportUI->Skeleton == NULL || !ImportUI->bImportAnimations)
		{
			return false;
		}
	}

	if (ImportUI->AnimSequenceImportData->AnimationLength == FBXALIT_SetRange)
	{
		if (ImportUI->AnimSequenceImportData->FrameImportRange.Min > ImportUI->AnimSequenceImportData->FrameImportRange.Max)
		{
			return false;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
