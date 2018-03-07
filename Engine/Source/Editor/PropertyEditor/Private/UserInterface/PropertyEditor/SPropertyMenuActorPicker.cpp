// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UserInterface/PropertyEditor/SPropertyMenuActorPicker.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "Editor.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerPublicTypes.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "AssetRegistryModule.h"
#include "UserInterface/PropertyEditor/PropertyEditorAssetConstants.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SceneOutlinerPublicTypes.h"

#define LOCTEXT_NAMESPACE "PropertyEditor"

void SPropertyMenuActorPicker::Construct( const FArguments& InArgs )
{
	CurrentActor = InArgs._InitialActor;
	bAllowClear = InArgs._AllowClear;
	ActorFilter = InArgs._ActorFilter;
	OnSet = InArgs._OnSet;
	OnClose = InArgs._OnClose;
	OnUseSelected = InArgs._OnUseSelected;

	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationsHeader", "Current Actor"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("UseSelected", "Use Selected"), 
			LOCTEXT("UseSelected_Tooltip", "Use the currently selected Actor"),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SPropertyMenuActorPicker::HandleUseSelected ) ) );

		if( CurrentActor )
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditAsset", "Edit"), 
				LOCTEXT("EditAsset_Tooltip", "Edit this asset"),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &SPropertyMenuActorPicker::OnEdit ) ) );
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CopyAsset", "Copy"),
			LOCTEXT("CopyAsset_Tooltip", "Copies the asset to the clipboard"),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &SPropertyMenuActorPicker::OnCopy ) )
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("PasteAsset", "Paste"),
			LOCTEXT("PasteAsset_Tooltip", "Pastes an asset from the clipboard to this field"),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateSP( this, &SPropertyMenuActorPicker::OnPaste ),
				FCanExecuteAction::CreateSP( this, &SPropertyMenuActorPicker::CanPaste ) )
		);

		if( bAllowClear )
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ClearAsset", "Clear"),
				LOCTEXT("ClearAsset_ToolTip", "Clears the asset set on this field"),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &SPropertyMenuActorPicker::OnClear ) )
				);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("BrowseHeader", "Browse"));
	{
		TSharedPtr<SWidget> MenuContent;

		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::Get().LoadModuleChecked<FSceneOutlinerModule>(TEXT("SceneOutliner"));

		SceneOutliner::FInitializationOptions InitOptions;
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
		InitOptions.Filters->AddFilterPredicate(ActorFilter);
		InitOptions.bFocusSearchBoxWhenOpened = true;

		InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::Label(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 0) );
		InitOptions.ColumnMap.Add(SceneOutliner::FBuiltInColumnTypes::ActorInfo(), SceneOutliner::FColumnInfo(SceneOutliner::EColumnVisibility::Visible, 10) );
		
		MenuContent =
			SNew(SBox)
			.WidthOverride(PropertyEditorAssetConstants::SceneOutlinerWindowSize.X)
			.HeightOverride(PropertyEditorAssetConstants::SceneOutlinerWindowSize.Y)
			[
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
				[
					SceneOutlinerModule.CreateSceneOutliner(InitOptions, FOnActorPicked::CreateSP(this, &SPropertyMenuActorPicker::OnActorSelected))
				]
			];

		MenuBuilder.AddWidget(MenuContent.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	ChildSlot
	[
		MenuBuilder.MakeWidget()
	];
}

void SPropertyMenuActorPicker::HandleUseSelected()
{
	OnUseSelected.ExecuteIfBound();
}

void SPropertyMenuActorPicker::OnEdit()
{
	if( CurrentActor )
	{
		GEditor->EditObject( CurrentActor );
	}
	OnClose.ExecuteIfBound();
}

void SPropertyMenuActorPicker::OnCopy()
{
	FAssetData CurrentAssetData( CurrentActor );

	if( CurrentAssetData.IsValid() )
	{
		FPlatformApplicationMisc::ClipboardCopy(*CurrentAssetData.GetExportTextName());
	}
	OnClose.ExecuteIfBound();
}

void SPropertyMenuActorPicker::OnPaste()
{
	FString DestPath;
	FPlatformApplicationMisc::ClipboardPaste(DestPath);

	if(DestPath == TEXT("None"))
	{
		SetValue(NULL);
	}
	else
	{
		AActor* Actor = LoadObject<AActor>(NULL, *DestPath);
		if(Actor && (ActorFilter.IsBound() || ActorFilter.Execute(Actor)))
		{
			SetValue(Actor);
		}
	}
	OnClose.ExecuteIfBound();
}

bool SPropertyMenuActorPicker::CanPaste()
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);

	FString Class;
	FString PossibleObjectPath = ClipboardText;
	if( ClipboardText.Split( TEXT("'"), &Class, &PossibleObjectPath ) )
	{
		// Remove the last item
		PossibleObjectPath = PossibleObjectPath.LeftChop( 1 );
	}

	bool bCanPaste = false;

	if( PossibleObjectPath == TEXT("None") )
	{
		bCanPaste = true;
	}
	else
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		bCanPaste = PossibleObjectPath.Len() < NAME_SIZE && AssetRegistryModule.Get().GetAssetByObjectPath( *PossibleObjectPath ).IsValid();
	}

	return bCanPaste;
}

void SPropertyMenuActorPicker::OnClear()
{
	SetValue(NULL);
	OnClose.ExecuteIfBound();
}

void SPropertyMenuActorPicker::OnActorSelected( AActor* InActor )
{
	SetValue(InActor);
	OnClose.ExecuteIfBound();
}

void SPropertyMenuActorPicker::SetValue( AActor* InActor )
{
	OnSet.ExecuteIfBound(InActor);
}

#undef LOCTEXT_NAMESPACE
