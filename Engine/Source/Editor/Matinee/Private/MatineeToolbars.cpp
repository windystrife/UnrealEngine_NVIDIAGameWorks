// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MatineeModule.h"
#include "MatineeActions.h"
#include "Matinee.h"
#include "Widgets/Input/STextComboBox.h"


FString FMatinee::GetToolbarSnapText()
{
	FString Text;
	int32 StringIndex = 0;
	if (SnapSelectionIndex < ARRAY_COUNT(InterpEdSnapSizes))
	{
		StringIndex = SnapSelectionIndex;
		Text = FString::Printf(TEXT("%.2f"),FMatinee::InterpEdSnapSizes[StringIndex]);
	}
	else if (SnapSelectionIndex < (ARRAY_COUNT(InterpEdSnapSizes)+ARRAY_COUNT(InterpEdFPSSnapSizes)))
	{
		StringIndex = SnapSelectionIndex - ARRAY_COUNT(InterpEdSnapSizes);
		Text = GetInterpEdFPSSnapSizeLocName( StringIndex );
	}
	else
	{
		Text = NSLOCTEXT("UnrealEd", "InterpEd_Snap_Keys", "Snap to Keys").ToString();
	}

	return Text;
}

bool FMatinee::IsToolbarSnapSettingChecked(uint32 InIndex)
{
	return (SnapSelectionIndex == InIndex);
}

EVisibility FMatinee::GetLargeIconVisibility() const
{
	return FMultiBoxSettings::UseSmallToolBarIcons.Get() ? EVisibility::Collapsed : EVisibility::Visible;
}

/**
 * Builds the Matinee Tool Bar
 */
void FMatinee::ExtendToolbar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, TSharedRef<SWidget> InterpolationBox, TSharedRef<SWidget> SpeedBox, TSharedRef<SWidget> SnapSettingBox)
		{
			ToolbarBuilder.BeginSection("CurveMode");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().AddKey);
			}
			ToolbarBuilder.EndSection();
		
			ToolbarBuilder.BeginSection("Interpolation");
			{
				ToolbarBuilder.AddWidget(InterpolationBox);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Play");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().Play);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().PlayLoop);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().Stop);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().PlayReverse);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Camera");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().CreateCameraActor);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Speed");
			{
				ToolbarBuilder.AddWidget(SpeedBox);
			}
			ToolbarBuilder.EndSection();

			//ToolbarBuilder.BeginSection("History");
			//{
			//	ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().Undo);
			//	ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().Redo);
			//}
			//ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("SnapSetting");
			{
				ToolbarBuilder.AddWidget(SnapSettingBox);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("Curve");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().ToggleCurveEditor);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Snap");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().ToggleSnap);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().ToggleSnapTimeToFrames);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().FixedTimeStepPlayback);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("View");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().FitSequence);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().FitViewToSelected);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().FitLoop);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().FitLoopSequence);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().ViewEndofTrack);
			}
			ToolbarBuilder.EndSection();
	
			ToolbarBuilder.BeginSection("Record");
			{
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().LaunchRecordWindow);
				ToolbarBuilder.AddToolBarButton(FMatineeCommands::Get().CreateMovie);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	InitialInterpModeStrings.Empty();
	InitialInterpModeStrings.Add( MakeShareable( new FString(NSLOCTEXT("Matinee", "Linear", "Linear").ToString()) ) );				
	InitialInterpModeStrings.Add( MakeShareable( new FString(NSLOCTEXT("Matinee", "CurveAuto", "CurveAuto").ToString()) ) );			
	InitialInterpModeStrings.Add( MakeShareable( new FString(NSLOCTEXT("Matinee", "Constant", "Constant").ToString()) ) );				
	InitialInterpModeStrings.Add( MakeShareable( new FString(NSLOCTEXT("Matinee", "CurveUser", "CurveUser").ToString()) ) );			
	InitialInterpModeStrings.Add( MakeShareable( new FString(NSLOCTEXT("Matinee", "CurveBreak", "CurveBreak").ToString()) ) );			
	InitialInterpModeStrings.Add( MakeShareable( new FString(NSLOCTEXT("Matinee", "CurveAutoClamped", "CurveAutoClamped").ToString()) ) );		

	InitialInterpModeComboBox = 
		SNew( STextComboBox )
		.OptionsSource(&InitialInterpModeStrings)
		.InitiallySelectedItem(InitialInterpModeStrings[0])
		.OnSelectionChanged(this, &FMatinee::OnChangeInitialInterpMode)
		.ToolTipText(NSLOCTEXT("Matinee", "ToolTipInitialInterp", "Initial Interp Mode | Selects the curve interpolation mode for newly created keys"))
		;

	TSharedRef<SWidget> InterpolationBox = 
		SNew(SBox)
		.WidthOverride(150)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Matinee.Toolbar", "InterpMode", "Interpolation:"))
				.Visibility( this, &FMatinee::GetLargeIconVisibility )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4,0)
			[
				InitialInterpModeComboBox.ToSharedRef()
			]
		];

	SpeedSettingStrings.Empty();
	SpeedSettingStrings.Add( MakeShareable( new FString(NSLOCTEXT("UnrealEd", "FullSpeed", "100%").ToString()) ) );
	SpeedSettingStrings.Add( MakeShareable( new FString(NSLOCTEXT("UnrealEd", "50Speed", "50%").ToString()) ) );
	SpeedSettingStrings.Add( MakeShareable( new FString(NSLOCTEXT("UnrealEd", "25Speed", "25%").ToString()) ) );
	SpeedSettingStrings.Add( MakeShareable( new FString(NSLOCTEXT("UnrealEd", "10Speed", "10%").ToString()) ) );
	SpeedSettingStrings.Add( MakeShareable( new FString(NSLOCTEXT("UnrealEd", "1Speed", "1%").ToString()) ) );

	SpeedCombo = 
		SNew(STextComboBox)
		.OptionsSource(&SpeedSettingStrings)
		.InitiallySelectedItem(SpeedSettingStrings[0])
		.OnSelectionChanged(this, &FMatinee::OnChangePlaySpeed)
		;

	TSharedRef<SWidget> SpeedBox = 
		SNew(SBox)
		.WidthOverride(103)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Matinee.Toolbar", "PlaybackSpeed", "Playback Speed:"))
				.Visibility( this, &FMatinee::GetLargeIconVisibility )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4,0)
			[
				SpeedCombo.ToSharedRef()
			]
		];

	// Append Second Snap Times
	SnapComboStrings.Empty();
	for(int32 i=0; i<ARRAY_COUNT(InterpEdSnapSizes); i++)
	{
		FString SnapCaption = FString::Printf( TEXT("%1.2f"), InterpEdSnapSizes[i] );
		SnapComboStrings.Add( MakeShareable( new FString(SnapCaption) ) );
	}
	// Append FPS Snap Times
	for(int32 i=0; i<ARRAY_COUNT(InterpEdFPSSnapSizes); i++)
	{
		FString SnapCaption = GetInterpEdFPSSnapSizeLocName( i );
		SnapComboStrings.Add( MakeShareable( new FString(SnapCaption) ) );
	}
	// Add option for snapping to other keys.
	SnapComboStrings.Add( MakeShareable( new FString(NSLOCTEXT("UnrealEd", "InterpEd_Snap_Keys", "Snap to Keys" ).ToString()) ) );

	SnapCombo = 
		SNew(STextComboBox)
		.OptionsSource(&SnapComboStrings)
		.InitiallySelectedItem(SnapComboStrings[2])
		.OnSelectionChanged(this, &FMatinee::OnChangeSnapSize)
		.ToolTipText( NSLOCTEXT("Matinee", "SnapComboToolTip", "Snap Size | Selects the timeline granularity for snapping and visualization purposes") )
		;

	TSharedRef<SWidget> SnapSettingBox = 
		SNew(SBox)
		.WidthOverride(155)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(4)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Matinee.Toolbar", "SnapSetting", "Snap Setting:"))
				.Visibility( this, &FMatinee::GetLargeIconVisibility )
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4,0)
			[
				SnapCombo.ToSharedRef()
			]
		];

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic( &Local::FillToolbar, InterpolationBox, SpeedBox, SnapSettingBox)
		);

	AddToolbarExtender(ToolbarExtender);

	IMatineeModule* MatineeModule = &FModuleManager::LoadModuleChecked<IMatineeModule>( "Matinee" );
	AddToolbarExtender(MatineeModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}
