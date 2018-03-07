// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMatineeRecorder.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SViewport.h"
#include "EditorStyleSet.h"
#include "LevelEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "Viewports.h"
#include "IMatinee.h"
#include "Widgets/Input/STextComboBox.h"

#define LOCTEXT_NAMESPACE "SMatineeRecorder"

//////////////////////////////////////////////////////////////////////////
// SMatineeRecorder

SMatineeRecorder::~SMatineeRecorder()
{
	// Safely stop recording if it is in progress
	if (ParentMatineeWindow.IsValid() && ParentMatineeWindow.Pin()->IsRecordingInterpValues())
	{
		ToggleRecord();
	}

	// Close viewport
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = NULL;
	}

	Viewport.Reset();

	// Release our reference to the viewport client
	LevelViewportClient.Reset();
}

void SMatineeRecorder::RefreshViewport()
{
	Viewport->InvalidateDisplay();
}

bool SMatineeRecorder::IsVisible() const
{
	return ViewportWidget.IsValid();
}

void SMatineeRecorder::Construct(const FArguments& InArgs)
{
	if( InArgs._MatineeWindow.IsValid() )
	{
		ParentMatineeWindow = InArgs._MatineeWindow; 
	}

	CameraModeOptions.Add( MakeShareable( new FString(LOCTEXT("NewCameraMode", "New Camera Mode").ToString() ) ) );
	CameraModeOptions.Add( MakeShareable( new FString(LOCTEXT("NewAttachedCameraMode", "New Attached Camera Mode").ToString() ) ) );
	CameraModeOptions.Add( MakeShareable( new FString(LOCTEXT("DuplicateSelectedTracks", "Duplicate Selected Tracks").ToString() ) ) );
	CameraModeOptions.Add( MakeShareable( new FString(LOCTEXT("ReplaceSelectedTracks", "Replace Selected Tracks").ToString() ) ) );

	this->ChildSlot
	[
		SNew(SBorder)
		. HAlign(HAlign_Fill)
		. VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			. HAlign(HAlign_Fill)
			. VAlign(VAlign_Fill)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew( RecordButton, SButton )
					.OnClicked( this, &SMatineeRecorder::ToggleRecord )
					.ToolTipText( LOCTEXT("StartStopRecording", "Start/Stop Recording") )
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SImage)
							.Image( this, &SMatineeRecorder::GetRecordImageDelegate )
						]
					]
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew( CameraModeComboBox, STextComboBox )
					.OptionsSource( &CameraModeOptions )
					.OnSelectionChanged( this, &SMatineeRecorder::SelectCameraMode )
					.ToolTipText( LOCTEXT("ChangeCameraMode", "Change Camera Mode") )
					.InitiallySelectedItem( CameraModeOptions[ 0 ] )
				]
			]

			// Build the viewport.
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1)
			[
				SAssignNew( ViewportWidget, SViewport )
				.EnableGammaCorrection( false )
				.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
				.ShowEffectWhenDisabled( false )		
			]
		]
	];

	// Create an animation viewport client
	LevelViewportClient = MakeShareable( new FLevelEditorViewportClient(nullptr) );

	Viewport = MakeShareable( new FSceneViewport( LevelViewportClient.Get(), ViewportWidget ) );
	LevelViewportClient->Viewport = Viewport.Get();

	LevelViewportClient->ViewportType = LVT_Perspective;
	LevelViewportClient->bSetListenerPosition = false;
	LevelViewportClient->SetViewLocation( EditorViewportDefs::DefaultPerspectiveViewLocation );
	LevelViewportClient->SetViewRotation( EditorViewportDefs::DefaultPerspectiveViewRotation );

	LevelViewportClient->SetRealtime( true );
	LevelViewportClient->SetAllowCinematicPreview( true );
	LevelViewportClient->Viewport->SetUserFocus(true);
	LevelViewportClient->SetMatineeRecordingWindow( ParentMatineeWindow.Pin().Get() );

	LevelViewportClient->VisibilityDelegate.BindSP( this, &SMatineeRecorder::IsVisible );


	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface( Viewport.ToSharedRef() );
}

/** Gets the current image that should be displayed for the Record/Stop button based on the status of the InterpEditor. */
const FSlateBrush* SMatineeRecorder::GetRecordImageDelegate(void) const
{
	if( ParentMatineeWindow.IsValid() && !ParentMatineeWindow.Pin()->IsRecordingInterpValues() )
	{
		return FEditorStyle::GetBrush( TEXT("MatineeRecorder.Record") );
	}
	else
	{
		return FEditorStyle::GetBrush( TEXT("MatineeRecorder.Stop") );
	}
}

/** Function call when pressing the Record/Stop button. Will toggle the InterpEditor to record/stop. */
FReply SMatineeRecorder::ToggleRecord()
{
	if(ParentMatineeWindow.IsValid())
	{
		ParentMatineeWindow.Pin()->ToggleRecordInterpValues();
	}
	return FReply::Handled();
}

/** When selecting an item in the drop down list it will call this function to relay the information to InterpEditor. */
void SMatineeRecorder::SelectCameraMode( TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/ )
{
	if(ParentMatineeWindow.IsValid())
	{
		for(int32 ItemID = 0; ItemID < CameraModeOptions.Num(); ItemID++)
		{
			if( CameraModeOptions[ ItemID ] == NewSelection )
			{
				ParentMatineeWindow.Pin()->SetRecordMode( ItemID );
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
