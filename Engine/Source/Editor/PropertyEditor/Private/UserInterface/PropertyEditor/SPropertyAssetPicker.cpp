// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "UserInterface/PropertyEditor/SPropertyAssetPicker.h"
#include "Widgets/Layout/SBorder.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "PropertyAssetPicker"

void SPropertyAssetPicker::Construct( const FArguments& InArgs )
{
	OnAssetSelected = InArgs._OnAssetSelected;
	OnGetAllowedClasses = InArgs._OnGetAllowedClasses;

	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SAssignNew( AssetPickerAnchor, SMenuAnchor )
			.Placement( MenuPlacement_AboveAnchor )
			.OnGetMenuContent( this, &SPropertyAssetPicker::OnGenerateAssetPicker )
		]
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SButton )
			.ButtonStyle( FEditorStyle::Get(), "HoverHintOnly" )
			.OnClicked( this, &SPropertyAssetPicker::OnClicked )
			.ToolTipText(LOCTEXT("PickButtonLabel", "Pick Asset"))
			.ContentPadding(0)
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable(false)
			[ 
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("PropertyWindow.Button_PickAsset") )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		]
	];
}
 
FReply SPropertyAssetPicker::OnClicked()
{	
	AssetPickerAnchor->SetIsOpen( true );
	return FReply::Handled();
}

TSharedRef<SWidget> SPropertyAssetPicker::OnGenerateAssetPicker()
{
	TArray<const UClass*> AllowedClasses;
	OnGetAllowedClasses.ExecuteIfBound( AllowedClasses );

	if( AllowedClasses.Num() == 0 )
	{
		// Assume all classes are allowed
		AllowedClasses.Add( UObject::StaticClass() );
	}
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	for ( auto ClassIt = AllowedClasses.CreateConstIterator(); ClassIt; ++ClassIt )
	{
		const UClass* Class = (*ClassIt);
		AssetPickerConfig.Filter.ClassNames.Add( Class->GetFName() );
	}
	// Allow child classes
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	// Set a delegate for setting the asset from the picker
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SPropertyAssetPicker::OnAssetSelectedFromPicker);
	AssetPickerConfig.bAllowDragging = false;
	// Use the list view by default
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	TSharedRef<SWidget> MenuContent = 
		SNew(SBox)
		.HeightOverride(300)
		.WidthOverride(300)
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			]
		];

	return MenuContent;
}

void SPropertyAssetPicker::OnAssetSelectedFromPicker( const struct FAssetData& AssetData )
{
	// Close the asset picker
	AssetPickerAnchor->SetIsOpen( false );

	OnAssetSelected.ExecuteIfBound( AssetData.GetAsset() );
}

#undef LOCTEXT_NAMESPACE
