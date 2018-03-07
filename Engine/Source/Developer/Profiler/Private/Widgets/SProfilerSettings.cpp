// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "ProfilerManager.h"


#define LOCTEXT_NAMESPACE "SProfilerSettings"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProfilerSettings::Construct( const FArguments& InArgs )
{
	OnClose = InArgs._OnClose;
	SettingPtr = InArgs._SettingPtr;

	const TSharedRef<SGridPanel> SettingsGrid = SNew(SGridPanel);
	int CurrentRowPos = 0;

	AddTitle( LOCTEXT("ProfilerSettingTitle","Profiler settings"), SettingsGrid, CurrentRowPos );
	AddSeparator( SettingsGrid, CurrentRowPos );
	AddHeader( LOCTEXT("MiscTitle","Miscellaneous"), SettingsGrid, CurrentRowPos );
	AddOption
	( 
		LOCTEXT("bShowCoalescedViewModesInEventGraph_T","Show Coalesced View Modes In Event Graph"), 
		LOCTEXT("bShowCoalescedViewModesInEventGraph_TT","If True, coalesced view modes related functionality will be added to the event graph"), 
		SettingPtr->bShowCoalescedViewModesInEventGraph, 
		SettingPtr->GetDefaults().bShowCoalescedViewModesInEventGraph,
		SettingsGrid, 
		CurrentRowPos 
	);
	AddSeparator( SettingsGrid, CurrentRowPos );
	AddFooter( SettingsGrid, CurrentRowPos );

	ChildSlot
	[
		SettingsGrid
	];

	SettingPtr->EnterEditMode();
}

void SProfilerSettings::AddTitle( const FText& TitleText, const TSharedRef<SGridPanel>& Grid, int32& RowPos )
{
	Grid->AddSlot( 0, RowPos++ )
	.Padding( 2.0f )
	[
		SNew(STextBlock)
		.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18) )
		.Text( TitleText )	
	];
	RowPos++;
}

void SProfilerSettings::AddSeparator( const TSharedRef<SGridPanel>& Grid, int32& RowPos )
{
	Grid->AddSlot( 0, RowPos++ )
	.Padding( 2.0f )
	.ColumnSpan( 2 )
	[
		SNew(SSeparator)
		.Orientation( Orient_Horizontal )
	];
	RowPos++;
}

void SProfilerSettings::AddHeader( const FText& HeaderText, const TSharedRef<SGridPanel>& Grid, int32& RowPos )
{
	Grid->AddSlot( 0, RowPos++ )
	.Padding( 2.0f )
	[
		SNew(STextBlock)
		.Font( FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14) )
		.Text( HeaderText )	
	];
	RowPos++;
}

void SProfilerSettings::AddOption(const FText& OptionName, const FText& OptionDesc, bool& Value, const bool& Default, const TSharedRef<SGridPanel>& Grid, int32& RowPos)
{
	Grid->AddSlot( 0, RowPos )
	.Padding( 2.0f )
	.HAlign( HAlign_Left )
	.VAlign( VAlign_Center )
	[
		SNew(STextBlock)
		.Text( OptionName )
		.ToolTipText( OptionDesc )
	];

	Grid->AddSlot( 1, RowPos )
	.Padding( 2.0f )
	.HAlign( HAlign_Center )
	.VAlign( VAlign_Fill )
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew(SCheckBox)
			.IsChecked( this, &SProfilerSettings::OptionValue_IsChecked, (const bool*)&Value )
			.OnCheckStateChanged( this, &SProfilerSettings::OptionValue_OnCheckStateChanged, (bool*)&Value )
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew(SButton)
			.ToolTipText( LOCTEXT("ResetToDefaultToolTip", "Reset to default") )
			.ButtonStyle( FEditorStyle::Get(), TEXT("NoBorder") )
			.ContentPadding( 0.0f ) 
			.Visibility( this, &SProfilerSettings::OptionDefault_GetDiffersFromDefaultAsVisibility, (const bool*)&Value, (const bool*)&Default )
			.OnClicked( this, &SProfilerSettings::OptionDefault_OnClicked, (bool*)&Value, (const bool*)&Default )
			.Content()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault") )
			]
		]
	];

	RowPos++;
}

void SProfilerSettings::AddFooter( const TSharedRef<SGridPanel>& Grid, int32& RowPos )
{
	Grid->AddSlot( 0, RowPos )
	.Padding( 2.0f )
	.ColumnSpan( 2 )
	.HAlign( HAlign_Right )
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth( 132 )
		.HAlign( HAlign_Center )
		[
			SNew(SButton)
			.OnClicked( this, &SProfilerSettings::SaveAndClose_OnClicked )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				[
					SNew( SImage )
					.Image( FEditorStyle::GetBrush("Profiler.Misc.Save16") )
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text( LOCTEXT("SaveAndCloseTitle","Save and close") )
				]
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth( 132 )
		.HAlign( HAlign_Center )
		[
			SNew(SButton)
			.OnClicked( this, &SProfilerSettings::ResetToDefaults_OnClicked )
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Center )
				[
					SNew( SImage )
					.Image( FEditorStyle::GetBrush("Profiler.Misc.Reset16") )
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text( LOCTEXT("ResetToDefaultsTitle","Reset to defaults") )
				]
			]
		]
	];

	RowPos++;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SProfilerSettings::SaveAndClose_OnClicked()
{
	OnClose.ExecuteIfBound();
	SettingPtr->ExitEditMode();
	SettingPtr->SaveToConfig();

	return FReply::Handled();
}

FReply SProfilerSettings::ResetToDefaults_OnClicked()
{
	const FProfilerSettings& Defaults = SettingPtr->GetDefaults();

	SettingPtr->bShowCoalescedViewModesInEventGraph = Defaults.bShowCoalescedViewModesInEventGraph;

	return FReply::Handled();
}

void SProfilerSettings::OptionValue_OnCheckStateChanged( ECheckBoxState CheckBoxState, bool* ValuePtr )
{
	*ValuePtr = CheckBoxState == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SProfilerSettings::OptionValue_IsChecked( const bool* ValuePtr ) const
{
	return *ValuePtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SProfilerSettings::OptionDefault_GetDiffersFromDefaultAsVisibility( const bool* ValuePtr, const bool* DefaultPtr ) const
{
	return *ValuePtr != *DefaultPtr ? EVisibility::Visible : EVisibility::Hidden;
}

FReply SProfilerSettings::OptionDefault_OnClicked( bool* ValuePtr, const bool* DefaultPtr )
{
	*ValuePtr = *DefaultPtr;

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
