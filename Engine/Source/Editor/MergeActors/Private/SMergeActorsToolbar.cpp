// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMergeActorsToolbar.h"
#include "IMergeActorsTool.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "IDocumentation.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "SMergeActorsToolbar"


//////////////////////////////////////////////////////////////

void SMergeActorsToolbar::Construct(const FArguments& InArgs)
{
	// Important: We use raw bindings here because we are releasing our binding in our destructor (where a weak pointer would be invalid)
	// It's imperative that our delegate is removed in the destructor for the level editor module to play nicely with reloading.

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnActorSelectionChanged().AddRaw(this, &SMergeActorsToolbar::OnActorSelectionChanged);

	RegisteredTools = InArgs._ToolsToRegister;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0, 0, 0, 0)
		[
			SAssignNew(ToolbarContainer, SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.Padding(FMargin(4, 0, 0, 0))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2, 0, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.f)
			.IsEnabled(this, &SMergeActorsToolbar::GetContentEnabledState)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(4, 4, 4, 4)
				[
					SNew(SScrollBox)
					+SScrollBox::Slot()
					[
						SAssignNew(InlineContentHolder, SBox)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(4, 4, 10, 4)
				[
					SNew(SButton)
					.Text(LOCTEXT("MergeActors", "Merge Actors"))
					.OnClicked(this, &SMergeActorsToolbar::OnMergeActorsClicked)
					.IsEnabled_Lambda([this]() -> bool { return this->RegisteredTools[CurrentlySelectedTool]->CanMerge();})
				]
			]
		]
	];

	UpdateToolbar();

	// Update selected actor state for the first time
	GUnrealEd->UpdateFloatingPropertyWindows();
}


SMergeActorsToolbar::~SMergeActorsToolbar()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnActorSelectionChanged().RemoveAll(this);
}


void SMergeActorsToolbar::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	SelectedObjects = NewSelection;
	bIsContentEnabled = (NewSelection.Num() > 0);
}


void SMergeActorsToolbar::OnToolSelectionChanged(const ECheckBoxState NewCheckedState, int32 ToolIndex)
{
	if (NewCheckedState == ECheckBoxState::Checked)
	{
		CurrentlySelectedTool = ToolIndex;
		UpdateInlineContent();
	}
}


ECheckBoxState SMergeActorsToolbar::OnIsToolSelected(int32 ToolIndex) const
{
	return (CurrentlySelectedTool == ToolIndex) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


FReply SMergeActorsToolbar::OnMergeActorsClicked()
{
	if (CurrentlySelectedTool >= 0 && CurrentlySelectedTool < RegisteredTools.Num())
	{
		const FString DefaultPackageName = RegisteredTools[CurrentlySelectedTool]->GetDefaultPackageName();
		const FString DefaultPath = FPackageName::GetLongPackagePath(DefaultPackageName);
		const FString DefaultName = FPackageName::GetShortName(DefaultPackageName);

		// Initialize SaveAssetDialog config
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("CreateMergedActorTitle", "Create Merged Actor");
		SaveAssetDialogConfig.DefaultPath = DefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = DefaultName;
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			const FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);

			RegisteredTools[CurrentlySelectedTool]->RunMerge(PackageName);
		}
	}

	return FReply::Handled();
}


bool SMergeActorsToolbar::GetContentEnabledState() const
{
	return bIsContentEnabled;
}


void SMergeActorsToolbar::AddTool(IMergeActorsTool* Tool)
{
	check(!RegisteredTools.Contains(Tool));
	RegisteredTools.Add(Tool);
	UpdateToolbar();
}


void SMergeActorsToolbar::RemoveTool(IMergeActorsTool* Tool)
{
	int32 IndexToRemove = RegisteredTools.Find(Tool);
	if (IndexToRemove != INDEX_NONE)
	{
		RegisteredTools.RemoveAt(IndexToRemove);

		if (CurrentlySelectedTool > IndexToRemove)
		{
			CurrentlySelectedTool--;
		}
		UpdateToolbar();
	}
}


void SMergeActorsToolbar::UpdateToolbar()
{
	const ISlateStyle& StyleSet = FEditorStyle::Get();

	TSharedRef<SHorizontalBox> HorizontalBox =
		SNew(SHorizontalBox);

	for (int32 ToolIndex = 0; ToolIndex < RegisteredTools.Num(); ToolIndex++)
	{
		const IMergeActorsTool* Tool = RegisteredTools[ToolIndex];

		HorizontalBox->AddSlot()
		.Padding(StyleSet.GetMargin("EditorModesToolbar.SToolBarButtonBlock.Padding"))
		[
			SNew(SCheckBox)
			.Style(&StyleSet, "EditorModesToolbar.ToggleButton")
			.OnCheckStateChanged(this, &SMergeActorsToolbar::OnToolSelectionChanged, ToolIndex)
			.IsChecked(this, &SMergeActorsToolbar::OnIsToolSelected, ToolIndex)
			.Padding(StyleSet.GetMargin("EditorModesToolbar.SToolBarButtonBlock.CheckBox.Padding"))
			.ToolTip(IDocumentation::Get()->CreateToolTip(Tool->GetTooltipText(), nullptr, FString(), FString()))
			[
				SNew(SImage)
				.Image(StyleSet.GetBrush(Tool->GetIconName()))
			]
		];
	}

	TSharedRef<SBorder> ToolbarContent =
		SNew(SBorder)
		.Padding(0)
		.BorderImage(StyleSet.GetBrush("NoBorder"))
		[
			HorizontalBox
		];

	ToolbarContainer->SetContent(ToolbarContent);

	UpdateInlineContent();
}


void SMergeActorsToolbar::UpdateInlineContent()
{
	if (CurrentlySelectedTool >= 0 && CurrentlySelectedTool < RegisteredTools.Num())
	{
		InlineContentHolder->SetContent(RegisteredTools[CurrentlySelectedTool]->GetWidget());
	}
}


#undef LOCTEXT_NAMESPACE
