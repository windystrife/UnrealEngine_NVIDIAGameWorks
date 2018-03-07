// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "UserInterface/PropertyEditor/SPropertySceneOutliner.h"
#include "Widgets/Layout/SBorder.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerPublicTypes.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"

#define LOCTEXT_NAMESPACE "PropertySceneOutliner"

void SPropertySceneOutliner::Construct( const FArguments& InArgs )
{
	OnActorSelected = InArgs._OnActorSelected;
	OnGetActorFilters = InArgs._OnGetActorFilters;

	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SAssignNew( SceneOutlinerAnchor, SMenuAnchor )
			.Placement( MenuPlacement_AboveAnchor )
			.OnGetMenuContent( this, &SPropertySceneOutliner::OnGenerateSceneOutliner )
		]
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SButton )
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.OnClicked( this, &SPropertySceneOutliner::OnClicked )
			.ToolTipText(LOCTEXT("PickButtonLabel", "Pick Actor"))
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable(false)
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("PropertyWindow.Button_PickActor") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]
	];
}
 
FReply SPropertySceneOutliner::OnClicked()
{	
	SceneOutlinerAnchor->SetIsOpen( true );
	return FReply::Handled();
}

TSharedRef<SWidget> SPropertySceneOutliner::OnGenerateSceneOutliner()
{
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>(TEXT("SceneOutliner"));

	SceneOutliner::FInitializationOptions InitOptions;
	InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
	OnGetActorFilters.ExecuteIfBound( InitOptions.Filters );

	TSharedRef<SWidget> MenuContent = 
		SNew(SBox)
		.HeightOverride(300)
		.WidthOverride(300)
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
			[
				SceneOutlinerModule.CreateSceneOutliner(InitOptions, FOnActorPicked::CreateSP(this, &SPropertySceneOutliner::OnActorSelectedFromOutliner))
			]
		];

	return MenuContent;
}

void SPropertySceneOutliner::OnActorSelectedFromOutliner( AActor* InActor )
{
	// Close the scene outliner
	SceneOutlinerAnchor->SetIsOpen( false );

	OnActorSelected.ExecuteIfBound( InActor );
}

#undef LOCTEXT_NAMESPACE
