// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SWorldHierarchy.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Widgets/SBoxPanel.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "WorldBrowserModule.h"
#include "LevelModel.h"
#include "LevelCollectionModel.h"

#include "LevelEditor.h"

#include "DragAndDrop/AssetDragDropOp.h"

#include "SWorldHierarchyImpl.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

SWorldHierarchy::SWorldHierarchy()
{
}

SWorldHierarchy::~SWorldHierarchy()
{
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnBrowseWorld.RemoveAll(this);
}

void SWorldHierarchy::Construct(const FArguments& InArgs)
{
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnBrowseWorld.AddSP(this, &SWorldHierarchy::OnBrowseWorld);

	OnBrowseWorld(InArgs._InWorld);
}

void SWorldHierarchy::OnBrowseWorld(UWorld* InWorld)
{
	// Remove all binding to an old world
	ChildSlot
	[
		SNullWidget::NullWidget
	];

	WorldModel = nullptr;
		
	// Bind to a new world
	if (InWorld)
	{
		FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
		WorldModel = WorldBrowserModule.SharedWorldModel(InWorld);

		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SHorizontalBox)
					
					// Toolbar
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						// Levels menu
						SNew( SComboButton )
						.ComboButtonStyle(FEditorStyle::Get(), "ToolbarComboButton")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0)
						.OnGetMenuContent(this, &SWorldHierarchy::GetFileButtonContent)
						//.ToolTipText(this, &SWorldHierarchy::GetNewAssetToolTipText)
						//.IsEnabled(this, &SWorldHierarchy::IsAssetPathSelected )
						.ButtonContent()
						[
							SNew(SHorizontalBox)

							// Icon
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(this, &SWorldHierarchy::GetLevelsMenuBrush)
							]

							// Text
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0,0,2,0)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
								.Text(LOCTEXT("LevelsButton", "Levels"))
							]
						]
					]

					// Button to summon level details tab
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SWorldHierarchy::OnSummonDetails)
						.ToolTipText(LOCTEXT("SummonDetailsToolTipText", "Summons level details"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Content()
						[
							SNew(SImage)
							.Image(this, &SWorldHierarchy::GetSummonDetailsBrush)
						]
					]

					// Button to summon world composition tab
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.Visibility(this, &SWorldHierarchy::GetCompositionButtonVisibility)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SWorldHierarchy::OnSummonComposition)
						.ToolTipText(LOCTEXT("SummonCompositionToolTipText", "Summons world composition"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Content()
						[
							SNew(SImage)
							.Image(this, &SWorldHierarchy::GetSummonCompositionBrush)
						]
					]
				]
			]
			
			// Hierarchy
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0,4,0,0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SWorldHierarchyImpl)
						.InWorldModel(WorldModel)
				]
			]
		];
	}
}

FReply SWorldHierarchy::OnSummonDetails()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditorModule.SummonWorldBrowserDetails();
	return FReply::Handled();
}

const FSlateBrush* SWorldHierarchy::GetLevelsMenuBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.LevelsMenuBrush");
}

const FSlateBrush* SWorldHierarchy::GetSummonDetailsBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.DetailsButtonBrush");
}

EVisibility SWorldHierarchy::GetCompositionButtonVisibility() const
{
	return WorldModel->IsTileWorld() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SWorldHierarchy::OnSummonComposition()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditorModule.SummonWorldBrowserComposition();
	return FReply::Handled();
}

const FSlateBrush* SWorldHierarchy::GetSummonCompositionBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.CompositionButtonBrush");
}

TSharedRef<SWidget> SWorldHierarchy::GetFileButtonContent()
{
	FMenuBuilder MenuBuilder(true, WorldModel->GetCommandList());

	// let current level collection model fill additional 'File' commands
	WorldModel->CustomizeFileMainMenu(MenuBuilder);

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
