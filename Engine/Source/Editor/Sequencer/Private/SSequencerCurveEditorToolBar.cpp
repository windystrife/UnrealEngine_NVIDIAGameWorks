// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerCurveEditorToolBar.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "SequencerCommonHelpers.h"
#include "SequencerSettings.h"
#include "RichCurveEditorCommands.h"

#define LOCTEXT_NAMESPACE "CurveEditorToolBar"

void SSequencerCurveEditorToolBar::Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer, TSharedPtr<FUICommandList> CurveEditorCommandList )
{
	Sequencer = InSequencer;
	SequencerSettings = InSequencer->GetSettings();

	FToolBarBuilder ToolBarBuilder( CurveEditorCommandList, FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true );

	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP( this, &SSequencerCurveEditorToolBar::MakeCurveEditorViewOptionsMenu, CurveEditorCommandList ),
		LOCTEXT( "CurveEditorViewOptions", "View Options" ),
		LOCTEXT( "CurveEditorViewOptionsToolTip", "View Options" ),
		TAttribute<FSlateIcon>(),
		true );

	ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().ToggleOutputSnapping);

	ToolBarBuilder.AddWidget(
		SNew( SImage )
			.Image(FEditorStyle::GetBrush("Sequencer.Value.Small")) );

	ToolBarBuilder.AddWidget(
		SNew( SBox )
			.VAlign( VAlign_Center )
			[
				SNew( SNumericDropDown<float> )
					.DropDownValues( SequencerSnapValues::GetSnapValues() )
					.ToolTipText( LOCTEXT( "ValueSnappingIntervalToolTip", "Curve value snapping interval" ) )
					.Value( this, &SSequencerCurveEditorToolBar::OnGetValueSnapInterval )
					.OnValueChanged( this, &SSequencerCurveEditorToolBar::OnValueSnapIntervalChanged )
			]);

	ToolBarBuilder.BeginSection( "Curve" );
	{
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFitHorizontal );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFitVertical );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().ZoomToFit );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection( "Interpolation" );
	{
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationCubicAuto );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationCubicUser );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationCubicBreak );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationLinear );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().InterpolationConstant );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection( "Tangents" );
	{
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().FlattenTangents );
		ToolBarBuilder.AddToolBarButton( FRichCurveEditorCommands::Get().StraightenTangents );
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP( this, &SSequencerCurveEditorToolBar::MakeCurveEditorCurveOptionsMenu, CurveEditorCommandList ),
		LOCTEXT( "CurveEditorCurveOptions", "Curves Options" ),
		LOCTEXT( "CurveEditorCurveOptionsToolTip", "Curve Options" ),
		TAttribute<FSlateIcon>(),
		true );

	ChildSlot
	[
		ToolBarBuilder.MakeWidget()
	];
}

TSharedRef<SWidget> SSequencerCurveEditorToolBar::MakeCurveEditorViewOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	FMenuBuilder MenuBuilder( true, CurveEditorCommandList );

	MenuBuilder.BeginSection( "CurveVisibility", LOCTEXT( "CurveEditorMenuCurveVisibilityHeader", "Curve Visibility" ) );
	{
		MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetAllCurveVisibility );
		MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetSelectedCurveVisibility );
		MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetAnimatedCurveVisibility );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "TangentVisibility", LOCTEXT( "CurveEditorMenuTangentVisibilityHeader", "Tangent Visibility" ) );
	{
		MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetAllTangentsVisibility );
		MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetSelectedKeysTangentVisibility );
		MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetNoTangentsVisibility );
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().ToggleAutoFrameCurveEditor);
	MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().ToggleShowCurveEditorCurveToolTips);

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencerCurveEditorToolBar::MakeCurveEditorCurveOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	struct FExtrapolationMenus
	{
		static void MakePreInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection( "Pre-Infinity Extrapolation", LOCTEXT( "CurveEditorMenuPreInfinityExtrapHeader", "Extrapolation" ) );
			{
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPreInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPreInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPreInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPreInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPreInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}

		static void MakePostInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection( "Post-Infinity Extrapolation", LOCTEXT( "CurveEditorMenuPostInfinityExtrapHeader", "Extrapolation" ) );
			{
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPostInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPostInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPostInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPostInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().SetPostInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}
	};

	FMenuBuilder MenuBuilder( true, CurveEditorCommandList );

	MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().BakeCurve);
	MenuBuilder.AddMenuEntry( FRichCurveEditorCommands::Get().ReduceCurve);

	MenuBuilder.AddSubMenu(
		LOCTEXT( "PreInfinitySubMenu", "Pre-Infinity" ),
		LOCTEXT( "PreInfinitySubMenuToolTip", "Pre-Infinity Extrapolation" ),
		FNewMenuDelegate::CreateStatic( &FExtrapolationMenus::MakePreInfinityExtrapSubMenu ) );

	MenuBuilder.AddSubMenu(
		LOCTEXT( "PostInfinitySubMenu", "Post-Infinity" ),
		LOCTEXT( "PostInfinitySubMenuToolTip", "Post-Infinity Extrapolation" ),
		FNewMenuDelegate::CreateStatic( &FExtrapolationMenus::MakePostInfinityExtrapSubMenu ) );
	
	return MenuBuilder.MakeWidget();
}

float SSequencerCurveEditorToolBar::OnGetValueSnapInterval() const
{
	return SequencerSettings->GetCurveValueSnapInterval();
}


void SSequencerCurveEditorToolBar::OnValueSnapIntervalChanged( float InInterval )
{
	SequencerSettings->SetCurveValueSnapInterval( InInterval );
}




#undef LOCTEXT_NAMESPACE
