// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerCurveEditor.h"
#include "SequencerCommonHelpers.h"
#include "SequencerCurveOwner.h"
#include "SequencerSettings.h"


#define LOCTEXT_NAMESPACE "SequencerCurveEditor"


void SSequencerCurveEditor::Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer, TSharedRef<ITimeSliderController> InTimeSliderController )
{
	Sequencer = InSequencer;
	SequencerSettings = InSequencer->GetSettings();

	TimeSliderController = InTimeSliderController;
	NodeTreeSelectionChangedHandle = Sequencer.Pin()->GetSelection().GetOnOutlinerNodeSelectionChanged().AddSP(this, &SSequencerCurveEditor::NodeTreeSelectionChanged);

	// @todo sequencer: switch to lambda capture expressions when support is introduced?
	auto ViewRange = InArgs._ViewRange;
	auto OnViewRangeChanged = InArgs._OnViewRangeChanged;

	SCurveEditor::Construct(
		SCurveEditor::FArguments()
		.ViewMinInput_Lambda( [=]{ return ViewRange.Get().GetLowerBoundValue(); } )
		.ViewMaxInput_Lambda( [=]{ return ViewRange.Get().GetUpperBoundValue(); } )
		.OnSetInputViewRange_Lambda( [=](float InLowerBound, float InUpperBound){ OnViewRangeChanged.ExecuteIfBound(TRange<float>(InLowerBound, InUpperBound), EViewRangeInterpolation::Immediate); } )
		.HideUI( false )
		.ZoomToFitHorizontal( true )
		.ShowCurveSelector( false )
		.ShowZoomButtons( false )
		.ShowInputGridNumbers( false )
		.ShowTimeInFrames(this, &SSequencerCurveEditor::GetShowTimeInFrames)
		.InputSnappingEnabled(this, &SSequencerCurveEditor::GetInputCurveSnapEnabled)
		.InputSnap( this, &SSequencerCurveEditor::GetCurveTimeSnapInterval )
		.OutputSnap( this, &SSequencerCurveEditor::GetCurveValueSnapInterval )
		.TimelineLength( 0.0f )
		.GridColor( FLinearColor( 0.3f, 0.3f, 0.3f, 0.3f ) )
	);

	GetSettings()->GetOnCurveEditorCurveVisibilityChanged().AddSP(this, &SSequencerCurveEditor::OnCurveEditorCurveVisibilityChanged);
}

FReply SSequencerCurveEditor::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Curve Editor takes precedence
	FReply Reply = SCurveEditor::OnMouseButtonDown( MyGeometry, MouseEvent );
	if ( Reply.IsEventHandled() )
	{
		return Reply;
	}

	return TimeSliderController->OnMouseButtonDown( *this, MyGeometry, MouseEvent );
}


FReply SSequencerCurveEditor::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Curve Editor takes precedence
	FReply Reply = SCurveEditor::OnMouseButtonUp( MyGeometry, MouseEvent );
	if ( Reply.IsEventHandled() )
	{
		return Reply;
	}

	return TimeSliderController->OnMouseButtonUp( *this, MyGeometry, MouseEvent );
}


FReply SSequencerCurveEditor::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = SCurveEditor::OnMouseMove( MyGeometry, MouseEvent );
	if ( Reply.IsEventHandled() )
	{
		return Reply;
	}

	return TimeSliderController->OnMouseMove( *this, MyGeometry, MouseEvent );
}

FReply SSequencerCurveEditor::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = TimeSliderController->OnMouseWheel( *this, MyGeometry, MouseEvent );
	if ( Reply.IsEventHandled() )
	{
		return Reply;
	}

	return SCurveEditor::OnMouseWheel( MyGeometry, MouseEvent );
}

template<class ParentNodeType>
TSharedPtr<ParentNodeType> GetParentOfType( TSharedRef<FSequencerDisplayNode> InNode, ESequencerNode::Type InNodeType )
{
	TSharedPtr<FSequencerDisplayNode> CurrentNode = InNode->GetParent();
	while ( CurrentNode.IsValid() )
	{
		if ( CurrentNode->GetType() == InNodeType )
		{
			break;
		}
		CurrentNode = CurrentNode->GetParent();
	}
	return StaticCastSharedPtr<ParentNodeType>( CurrentNode );
}

void SSequencerCurveEditor::SetSequencerNodeTree( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree )
{
	SequencerNodeTree = InSequencerNodeTree;
	UpdateCurveOwner();
}

void SSequencerCurveEditor::UpdateCurveOwner()
{
	TSharedRef<FSequencerCurveOwner> NewCurveOwner = MakeShareable( new FSequencerCurveOwner( SequencerNodeTree, GetSettings()->GetCurveVisibility() ) );

	bool bAllFound = false;
	if (CurveOwner.IsValid())
	{
		TSet<FName> NewCurveOwnerCurveNames;
		for (auto Curve : NewCurveOwner->GetCurves())
		{
			NewCurveOwnerCurveNames.Add(Curve.CurveName);
		}

		TSet<FName> CurveOwnerCurveNames;
		if (CurveOwner->GetCurves().Num() == NewCurveOwner->GetCurves().Num())
		{
			bAllFound = true;
			for (auto Curve : CurveOwner->GetCurves())
			{
				if (!NewCurveOwnerCurveNames.Contains(Curve.CurveName))
				{
					bAllFound = false;
					break;
				}
			}
		}
	}

	if (!bAllFound)
	{
		CurveOwner = NewCurveOwner;
		SetCurveOwner( CurveOwner.Get() );
	}
	
	UpdateCurveViewModelSelection();
}

bool SSequencerCurveEditor::GetInputCurveSnapEnabled() const
{
	return SequencerSettings->GetIsSnapEnabled();
}

float SSequencerCurveEditor::GetCurveTimeSnapInterval() const
{
	if (Sequencer.IsValid() )
	{
		return Sequencer.Pin()->GetFixedFrameInterval();
	}

	return 1.f;
}

float SSequencerCurveEditor::GetCurveValueSnapInterval() const
{
	return SequencerSettings->GetSnapCurveValueToInterval()
		? SequencerSettings->GetCurveValueSnapInterval()
		: 0;
}

bool SSequencerCurveEditor::GetShowTimeInFrames() const
{
	return SequencerSnapValues::IsTimeSnapIntervalFrameRate(GetCurveTimeSnapInterval());
}

void SSequencerCurveEditor::NodeTreeSelectionChanged()
{
	if (SequencerNodeTree.IsValid())
	{
		ValidateSelection();

		if (GetSettings()->GetCurveVisibility() == ECurveEditorCurveVisibility::SelectedCurves)
		{
			UpdateCurveOwner();
		}

		if (GetAutoFrame())
		{
			ZoomToFit();
		}

		UpdateCurveViewModelSelection();
	}
}

void SSequencerCurveEditor::UpdateCurveViewModelSelection()
{
	ClearSelectedCurveViewModels();
	for (auto SelectedCurve : CurveOwner->GetSelectedCurves())
	{
		SetSelectedCurveViewModel(SelectedCurve);
	}
}

void SSequencerCurveEditor::OnCurveEditorCurveVisibilityChanged()
{
	UpdateCurveOwner();
}

TArray<FRichCurve*> SSequencerCurveEditor::GetCurvesToFit() const
{
	TArray<FRichCurve*> FitCurves;

	for(auto CurveViewModel : CurveViewModels)
	{
		if (CurveViewModel->bIsVisible)
		{
			if (CurveOwner->GetSelectedCurves().Contains(CurveViewModel->CurveInfo.CurveToEdit))
			{
				FitCurves.Add(CurveViewModel->CurveInfo.CurveToEdit);
			}
		}
	}

	if (FitCurves.Num() > 0)
	{
		return FitCurves;
	}

	return SCurveEditor::GetCurvesToFit();
}


SSequencerCurveEditor::~SSequencerCurveEditor()
{
	if ( Sequencer.IsValid() )
	{
		Sequencer.Pin()->GetSelection().GetOnOutlinerNodeSelectionChanged().Remove( NodeTreeSelectionChangedHandle );
	}

	GetSettings()->GetOnCurveEditorCurveVisibilityChanged().RemoveAll(this);
}

#undef LOCTEXT_NAMESPACE
