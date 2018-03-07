// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencer.h"
#include "Engine/Blueprint.h"
#include "MovieScene.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Widgets/Layout/SBorder.h"
#include "ISequencerEditTool.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "Widgets/Navigation/SBreadcrumbTrail.h"
#include "SequencerCommands.h"
#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "SequencerCommonHelpers.h"
#include "SSequencerCurveEditor.h"
#include "SSequencerCurveEditorToolBar.h"
#include "SSequencerLabelBrowser.h"
#include "ISequencerWidgetsModule.h"
#include "ScopedTransaction.h"
#include "SequencerTimeSliderController.h"
#include "SSequencerSectionOverlay.h"
#include "SSequencerTrackArea.h"
#include "SSequencerTrackOutliner.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/ActorDragDropGraphEdOp.h"
#include "DragAndDrop/ClassDragDropOp.h"
#include "Widgets/Input/SSearchBox.h"
#include "SSequencerTreeView.h"
#include "MovieSceneSequence.h"
#include "SSequencerSplitterOverlay.h"
#include "SequencerHotspots.h"
#include "VirtualTrackArea.h"
#include "Framework/Commands/GenericCommands.h"
#include "SequencerContextMenus.h"
#include "Math/UnitConversion.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "SequencerSettings.h"
#include "SSequencerGotoBox.h"
#include "SSequencerTransformBox.h"
#include "SSequencerDebugVisualizer.h"
#include "ISequencerModule.h"
#include "IVREditorModule.h"
#include "EditorFontGlyphs.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "Sequencer"

DECLARE_DELEGATE_RetVal(bool, FOnGetShowFrames)
DECLARE_DELEGATE_RetVal(uint8, FOnGetZeroPad)


/* Numeric type interface for showing numbers as frames or times */
struct FFramesOrTimeInterface : public TNumericUnitTypeInterface<float>
{
	FFramesOrTimeInterface(FOnGetShowFrames InShowFrameNumbers,TSharedPtr<FSequencerTimeSliderController> InController, FOnGetZeroPad InOnGetZeroPad)
		: TNumericUnitTypeInterface(EUnit::Seconds)
		, ShowFrameNumbers(MoveTemp(InShowFrameNumbers))
		, TimeSliderController(MoveTemp(InController))
		, OnGetZeroPad(MoveTemp(InOnGetZeroPad))
	{}

private:
	FOnGetShowFrames ShowFrameNumbers;
	TSharedPtr<FSequencerTimeSliderController> TimeSliderController;
	FOnGetZeroPad OnGetZeroPad;

	virtual FString ToString(const float& Value) const override
	{
		if (ShowFrameNumbers.Execute())
		{
			int32 Frame = TimeSliderController->TimeToFrame(Value);
			if (OnGetZeroPad.IsBound())
			{
				return FString::Printf(*FString::Printf(TEXT("%%0%dd"), OnGetZeroPad.Execute()), Frame);
			}
			return FString::Printf(TEXT("%d"), Frame);
		}

		return FString::Printf(TEXT("%.2fs"), Value);
	}

	virtual TOptional<float> FromString(const FString& InString, const float& InExistingValue) override
	{
		bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Execute() : false;
		if (bShowFrameNumbers)
		{
			// Convert existing value to frames
			float ExistingValueInFrames = TimeSliderController->TimeToFrame(InExistingValue);
			TOptional<float> Result = TNumericUnitTypeInterface<float>::FromString(InString, ExistingValueInFrames);

			if (Result.IsSet())
			{
				int32 NewEndFrame = FMath::RoundToInt(Result.GetValue());
				return float(TimeSliderController->FrameToTime(NewEndFrame));
			}
		}

		return TNumericUnitTypeInterface::FromString(InString, InExistingValue);
	}
};


/* SSequencer interface
 *****************************************************************************/
PRAGMA_DISABLE_OPTIMIZATION
void SSequencer::Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer)
{
	SequencerPtr = InSequencer;
	bIsActiveTimerRegistered = false;
	bUserIsSelecting = false;
	CachedClampRange = TRange<float>::Empty();
	CachedViewRange = TRange<float>::Empty();

	Settings = InSequencer->GetSettings();
	InSequencer->OnActivateSequence().AddSP(this, &SSequencer::OnSequenceInstanceActivated);

	ISequencerWidgetsModule& SequencerWidgets = FModuleManager::Get().LoadModuleChecked<ISequencerWidgetsModule>( "SequencerWidgets" );

	OnPlaybackRangeBeginDrag = InArgs._OnPlaybackRangeBeginDrag;
	OnPlaybackRangeEndDrag = InArgs._OnPlaybackRangeEndDrag;
	OnSelectionRangeBeginDrag = InArgs._OnSelectionRangeBeginDrag;
	OnSelectionRangeEndDrag = InArgs._OnSelectionRangeEndDrag;

	OnReceivedFocus = InArgs._OnReceivedFocus;

	FTimeSliderArgs TimeSliderArgs;
	{
		TimeSliderArgs.ViewRange = InArgs._ViewRange;
		TimeSliderArgs.ClampRange = InArgs._ClampRange;
		TimeSliderArgs.PlaybackRange = InArgs._PlaybackRange;
		TimeSliderArgs.SelectionRange = InArgs._SelectionRange;
		TimeSliderArgs.OnPlaybackRangeChanged = InArgs._OnPlaybackRangeChanged;
		TimeSliderArgs.OnPlaybackRangeBeginDrag = OnPlaybackRangeBeginDrag;
		TimeSliderArgs.OnPlaybackRangeEndDrag = OnPlaybackRangeEndDrag;
		TimeSliderArgs.OnSelectionRangeChanged = InArgs._OnSelectionRangeChanged;
		TimeSliderArgs.OnSelectionRangeBeginDrag = OnSelectionRangeBeginDrag;
		TimeSliderArgs.OnSelectionRangeEndDrag = OnSelectionRangeEndDrag;
		TimeSliderArgs.OnViewRangeChanged = InArgs._OnViewRangeChanged;
		TimeSliderArgs.OnClampRangeChanged = InArgs._OnClampRangeChanged;
		TimeSliderArgs.OnGetNearestKey = InArgs._OnGetNearestKey;
		TimeSliderArgs.IsPlaybackRangeLocked = InArgs._IsPlaybackRangeLocked;
		TimeSliderArgs.OnTogglePlaybackRangeLocked = InArgs._OnTogglePlaybackRangeLocked;
		TimeSliderArgs.TimeSnapInterval = InArgs._TimeSnapInterval;
		TimeSliderArgs.ScrubPosition = InArgs._ScrubPosition;
		TimeSliderArgs.OnBeginScrubberMovement = InArgs._OnBeginScrubbing;
		TimeSliderArgs.OnEndScrubberMovement = InArgs._OnEndScrubbing;
		TimeSliderArgs.OnScrubPositionChanged = InArgs._OnScrubPositionChanged;
		TimeSliderArgs.PlaybackStatus = InArgs._PlaybackStatus;
		TimeSliderArgs.SubSequenceRange = InArgs._SubSequenceRange;

		TimeSliderArgs.Settings = Settings;
	}

	TimeSliderController = MakeShareable( new FSequencerTimeSliderController( TimeSliderArgs ) );
	
	TSharedRef<FSequencerTimeSliderController> TimeSliderControllerRef = TimeSliderController.ToSharedRef();

	{
		auto ShowFrameNumbersDelegate = FOnGetShowFrames::CreateSP(this, &SSequencer::ShowFrameNumbers);
		USequencerSettings* SequencerSettings = Settings;
		auto GetZeroPad = [=]() -> uint8 {
			if (SequencerSettings)
			{
				return SequencerSettings->GetZeroPadFrames();
			}
			return 0;
		};

		NumericTypeInterface = MakeShareable( new FFramesOrTimeInterface(ShowFrameNumbersDelegate, TimeSliderControllerRef, FOnGetZeroPad()) );
		ZeroPadNumericTypeInterface = MakeShareable( new FFramesOrTimeInterface(ShowFrameNumbersDelegate, TimeSliderControllerRef, FOnGetZeroPad::CreateLambda(GetZeroPad)) );
	}

	bool bMirrorLabels = false;
	
	// Create the top and bottom sliders
	TopTimeSlider = SequencerWidgets.CreateTimeSlider( TimeSliderControllerRef, bMirrorLabels );
	bMirrorLabels = true;
	TSharedRef<ITimeSlider> BottomTimeSlider = SequencerWidgets.CreateTimeSlider( TimeSliderControllerRef, TAttribute<EVisibility>(this, &SSequencer::GetBottomTimeSliderVisibility), bMirrorLabels );

	// Create bottom time range slider
	TSharedRef<ITimeSlider> BottomTimeRange = SequencerWidgets.CreateTimeRange(
		FTimeRangeArgs(
			EShowRange(EShowRange::WorkingRange | EShowRange::ViewRange),
			TimeSliderControllerRef,
			TAttribute<EVisibility>(this, &SSequencer::GetTimeRangeVisibility),
			TAttribute<bool>(this, &SSequencer::ShowFrameNumbers),
			ZeroPadNumericTypeInterface.ToSharedRef()
		),
		SequencerWidgets.CreateTimeRangeSlider(TimeSliderControllerRef, TAttribute<float>(this, &SSequencer::OnGetTimeSnapInterval))
	);

	OnGetAddMenuContent = InArgs._OnGetAddMenuContent;
	AddMenuExtender = InArgs._AddMenuExtender;
	ToolbarExtender = InArgs._ToolbarExtender;

	ColumnFillCoefficients[0] = 0.3f;
	ColumnFillCoefficients[1] = 0.7f;

	TAttribute<float> FillCoefficient_0, FillCoefficient_1;
	{
		FillCoefficient_0.Bind(TAttribute<float>::FGetter::CreateSP(this, &SSequencer::GetColumnFillCoefficient, 0));
		FillCoefficient_1.Bind(TAttribute<float>::FGetter::CreateSP(this, &SSequencer::GetColumnFillCoefficient, 1));
	}

	TSharedRef<SScrollBar> ScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(5.0f, 5.0f));

	SAssignNew(TrackOutliner, SSequencerTrackOutliner);
	SAssignNew(TrackArea, SSequencerTrackArea, TimeSliderControllerRef, InSequencer);
	SAssignNew(TreeView, SSequencerTreeView, InSequencer->GetNodeTree(), TrackArea.ToSharedRef())
		.ExternalScrollbar(ScrollBar)
		.Clipping(EWidgetClipping::ClipToBounds)
		.OnGetContextMenuContent(FOnGetContextMenuContent::CreateSP(this, &SSequencer::GetContextMenuContent));

	SAssignNew(CurveEditor, SSequencerCurveEditor, InSequencer, TimeSliderControllerRef)
		.Visibility(this, &SSequencer::GetCurveEditorVisibility)
		.OnViewRangeChanged(InArgs._OnViewRangeChanged)
		.ViewRange(InArgs._ViewRange);

	CurveEditor->SetAllowAutoFrame(SequencerPtr.Pin()->GetShowCurveEditor());
	TrackArea->SetTreeView(TreeView);

	const int32 Column0 = 0, Column1 = 1;
	const int32 Row0 = 0, Row1 = 1, Row2 = 2, Row3 = 3, Row4 = 4;

	const float CommonPadding = 3.f;
	const FMargin ResizeBarPadding(4.f, 0, 0, 0);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			
			+ SSplitter::Slot()
			.Value(0.1f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Visibility(this, &SSequencer::HandleLabelBrowserVisibility)
				[
					// track label browser
					SAssignNew(LabelBrowser, SSequencerLabelBrowser, InSequencer)
					.OnSelectionChanged(this, &SSequencer::HandleLabelBrowserSelectionChanged)
				]
			]

			+ SSplitter::Slot()
			.Value(0.9f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					// track area grid panel
					SNew( SGridPanel )
					.FillRow( 2, 1.f )
					.FillColumn( 0, FillCoefficient_0 )
					.FillColumn( 1, FillCoefficient_1 )

					// Toolbar
					+ SGridPanel::Slot( Column0, Row0, SGridPanel::Layer(10) )
					.ColumnSpan(2)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(CommonPadding, 0.f))
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								MakeToolBar()
							]

							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SSequencerCurveEditorToolBar, InSequencer, CurveEditor->GetCommands())
								.Visibility(this, &SSequencer::GetCurveEditorToolBarVisibility)
							]

							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SAssignNew(BreadcrumbTrail, SBreadcrumbTrail<FSequencerBreadcrumb>)
								.Visibility(this, &SSequencer::GetBreadcrumbTrailVisibility)
								.OnCrumbClicked(this, &SSequencer::OnCrumbClicked)
								.ButtonStyle(FEditorStyle::Get(), "FlatButton")
								.DelimiterImage(FEditorStyle::GetBrush("Sequencer.BreadcrumbIcon"))
								.TextStyle(FEditorStyle::Get(), "Sequencer.BreadcrumbText")
							]
						]
					]

					+ SGridPanel::Slot( Column0, Row1 )
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SSpacer)
						]
					]

					// outliner search box
					+ SGridPanel::Slot( Column0, Row1, SGridPanel::Layer(10) )
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						.Padding(FMargin(CommonPadding*2, CommonPadding))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.f, 0.f, CommonPadding, 0.f))
							[
								MakeAddButton()
							]

							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SAssignNew(SearchBox, SSearchBox)
								.HintText(LOCTEXT("FilterNodesHint", "Filter"))
								.OnTextChanged( this, &SSequencer::OnOutlinerSearchChanged )
							]
						]
					]

					// main sequencer area
					+ SGridPanel::Slot( Column0, Row2, SGridPanel::Layer(10) )
					.ColumnSpan(2)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						[
							SNew( SOverlay )

							+ SOverlay::Slot()
							[
								SNew(SScrollBorder, TreeView.ToSharedRef())
								[
									SNew(SHorizontalBox)

									// outliner tree
									+ SHorizontalBox::Slot()
									.FillWidth( FillCoefficient_0 )
									[
										SNew(SBox)
										[
											TreeView.ToSharedRef()
										]
									]

									// track area
									+ SHorizontalBox::Slot()
									.FillWidth( FillCoefficient_1 )
									[
										SNew(SBox)
										.Padding(ResizeBarPadding)
										.Visibility(this, &SSequencer::GetTrackAreaVisibility )
										.Clipping(EWidgetClipping::ClipToBounds)
										[
											TrackArea.ToSharedRef()
										]
									]
								]
							]

							+ SOverlay::Slot()
							.HAlign( HAlign_Right )
							[
								ScrollBar
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth( TAttribute<float>( this, &SSequencer::GetOutlinerSpacerFill ) )
						[
							SNew(SSpacer)
						]
					]

					// playback buttons
					+ SGridPanel::Slot( Column0, Row4, SGridPanel::Layer(10) )
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						//.BorderBackgroundColor(FLinearColor(.50f, .50f, .50f, 1.0f))
						.HAlign(HAlign_Center)
						[
							SequencerPtr.Pin()->MakeTransportControls(true)
						]
					]

					// Second column

					+ SGridPanel::Slot( Column1, Row1 )
					.Padding(ResizeBarPadding)
					.RowSpan(3)
					[
						SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
						[
							SNew(SSpacer)
						]
					]

					+ SGridPanel::Slot( Column1, Row1, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew( SBorder )
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						.BorderBackgroundColor( FLinearColor(.50f, .50f, .50f, 1.0f ) )
						.Padding(0)
						.Clipping(EWidgetClipping::ClipToBounds)
						[
							TopTimeSlider.ToSharedRef()
						]
					]

					// Overlay that draws the tick lines
					+ SGridPanel::Slot( Column1, Row2, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew( SSequencerSectionOverlay, TimeSliderControllerRef )
						.Visibility( EVisibility::HitTestInvisible )
						.DisplayScrubPosition( false )
						.DisplayTickLines( true )
						.Clipping(EWidgetClipping::ClipToBounds)
					]

					// Curve editor
					+ SGridPanel::Slot( Column1, Row2, SGridPanel::Layer(20) )
					.Padding(ResizeBarPadding)
					[
						CurveEditor.ToSharedRef()
					]

					// Overlay that draws the scrub position
					+ SGridPanel::Slot( Column1, Row2, SGridPanel::Layer(30) )
					.Padding(ResizeBarPadding)
					[
						SNew( SSequencerSectionOverlay, TimeSliderControllerRef )
						.Visibility( EVisibility::HitTestInvisible )
						.DisplayScrubPosition( true )
						.DisplayTickLines( false )
						.PaintPlaybackRangeArgs(this, &SSequencer::GetSectionPlaybackRangeArgs)
						.Clipping(EWidgetClipping::ClipToBounds)
					]

					// Goto box
					+ SGridPanel::Slot(Column1, Row2, SGridPanel::Layer(40))
						.Padding(ResizeBarPadding)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Top)
						[
							SAssignNew(GotoBox, SSequencerGotoBox, SequencerPtr.Pin().ToSharedRef(), *Settings, NumericTypeInterface.ToSharedRef())
						]

					// Transform box
					+ SGridPanel::Slot(Column1, Row2, SGridPanel::Layer(50))
						.Padding(ResizeBarPadding)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Top)
						[
							SAssignNew(TransformBox, SSequencerTransformBox, SequencerPtr.Pin().ToSharedRef(), *Settings, NumericTypeInterface.ToSharedRef())
						]

					// debug vis
					+ SGridPanel::Slot( Column1, Row3, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew(SSequencerDebugVisualizer, InSequencer)
						.ViewRange(FAnimatedRange::WrapAttribute(InArgs._ViewRange))
						.Visibility(this, &SSequencer::GetDebugVisualizerVisibility)
					]

					// play range sliders
					+ SGridPanel::Slot( Column1, Row4, SGridPanel::Layer(10) )
					.Padding(ResizeBarPadding)
					[
						SNew( SBorder )
						.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
						.BorderBackgroundColor( FLinearColor(.50f, .50f, .50f, 1.0f ) )
						.Clipping(EWidgetClipping::ClipToBounds)
						.Padding(0)
						[
							SNew( SOverlay )

							+ SOverlay::Slot()
							[
								BottomTimeSlider
							]

							+ SOverlay::Slot()
							[
								BottomTimeRange
							]
						]
					]
				]

				+ SOverlay::Slot()
				[
					// track area virtual splitter overlay
					SNew(SSequencerSplitterOverlay)
					.Style(FEditorStyle::Get(), "Sequencer.AnimationOutliner.Splitter")
					.Visibility(EVisibility::SelfHitTestInvisible)

					+ SSplitter::Slot()
					.Value(FillCoefficient_0)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SSequencer::OnColumnFillCoefficientChanged, 0))
					[
						SNew(SSpacer)
					]

					+ SSplitter::Slot()
					.Value(FillCoefficient_1)
					.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SSequencer::OnColumnFillCoefficientChanged, 1))
					[
						SNew(SSpacer)
					]
				]
			]
		]
	];

	InSequencer->GetSelection().GetOnKeySelectionChanged().AddSP(this, &SSequencer::HandleKeySelectionChanged);
	InSequencer->GetSelection().GetOnSectionSelectionChanged().AddSP(this, &SSequencer::HandleSectionSelectionChanged);
	InSequencer->GetSelection().GetOnOutlinerNodeSelectionChanged().AddSP(this, &SSequencer::HandleOutlinerNodeSelectionChanged);

	ResetBreadcrumbs();
}
PRAGMA_ENABLE_OPTIMIZATION

void SSequencer::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings)
{
	auto CanPasteFromHistory = [this]{
		if (!HasFocusedDescendants() && !HasKeyboardFocus())
		{
			return false;
		}

		return SequencerPtr.Pin()->GetClipboardStack().Num() != 0;
	};
	
	SequencerCommandBindings->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SSequencer::OnPaste),
		FCanExecuteAction::CreateSP(this, &SSequencer::CanPaste)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().PasteFromHistory,
		FExecuteAction::CreateSP(this, &SSequencer::PasteFromHistory),
		FCanExecuteAction::CreateLambda(CanPasteFromHistory)
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ToggleShowGotoBox,
		FExecuteAction::CreateLambda([this]{ GotoBox->ToggleVisibility(); })
	);

	SequencerCommandBindings->MapAction(
		FSequencerCommands::Get().ToggleShowTransformBox,
		FExecuteAction::CreateLambda([this]{ TransformBox->ToggleVisibility(); })
	);
}
	
const ISequencerEditTool* SSequencer::GetEditTool() const
{
	return TrackArea->GetEditTool();
}


void SSequencer::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged)
{
	// @todo sequencer: is this still needed?
}


/* SSequencer implementation
 *****************************************************************************/

TSharedRef<INumericTypeInterface<float>> SSequencer::GetNumericTypeInterface()
{
	return NumericTypeInterface.ToSharedRef();
}

TSharedRef<INumericTypeInterface<float>> SSequencer::GetZeroPadNumericTypeInterface()
{
	return ZeroPadNumericTypeInterface.ToSharedRef();
}


/* SSequencer callbacks
 *****************************************************************************/

void SSequencer::HandleKeySelectionChanged()
{
}


void SSequencer::HandleLabelBrowserSelectionChanged(FString NewLabel, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (NewLabel.IsEmpty())
	{
		SearchBox->SetText(FText::GetEmpty());
	}
	else
	{
		SearchBox->SetText(FText::FromString(NewLabel));
	}
}


EVisibility SSequencer::HandleLabelBrowserVisibility() const
{
	if (Settings->GetLabelBrowserVisible())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}


void SSequencer::HandleSectionSelectionChanged()
{
}


void SSequencer::HandleOutlinerNodeSelectionChanged()
{
	const TSet<TSharedRef<FSequencerDisplayNode>>& OutlinerSelection = SequencerPtr.Pin()->GetSelection().GetSelectedOutlinerNodes();

	if ( OutlinerSelection.Num() == 1 )
	{
		for ( auto& Node : OutlinerSelection )
		{
			TreeView->RequestScrollIntoView( Node );
			break;
		}
	}
}


TSharedRef<SWidget> SSequencer::MakeAddButton()
{
	if (SequencerPtr.Pin()->IsReadOnly())
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SComboButton)
	.OnGetMenuContent(this, &SSequencer::MakeAddMenu)
	.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
	.ContentPadding(FMargin(2.0f, 1.0f))
	.HasDownArrow(false)
	.ButtonContent()
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "NormalText.Important")
			.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
			.Text(FEditorFontGlyphs::Plus)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "NormalText.Important")
			.Text(LOCTEXT("Track", "Track"))
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(4, 0, 0, 0)
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "NormalText.Important")
			.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
			.Text(FEditorFontGlyphs::Caret_Down)
		]
	];
}

TSharedRef<SWidget> SSequencer::MakeToolBar()
{
	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
	TSharedPtr<FExtender> Extender = SequencerModule.GetToolBarExtensibilityManager()->GetAllExtenders();
	if (ToolbarExtender.IsValid())
	{
		Extender = FExtender::Combine({ Extender, ToolbarExtender });
	}

	FToolBarBuilder ToolBarBuilder( SequencerPtr.Pin()->GetCommandBindings(), FMultiBoxCustomization::None, Extender, Orient_Horizontal, true);

	const bool bIsReadOnly = SequencerPtr.Pin()->IsReadOnly();

	ToolBarBuilder.BeginSection("Base Commands");
	{
		// General 
		if (SequencerPtr.Pin()->IsLevelEditorSequencer())
		{
			ToolBarBuilder.AddToolBarButton(
				FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnSaveMovieSceneClicked)),
				NAME_None,
				LOCTEXT("SaveDirtyPackages", "Save"),
				LOCTEXT("SaveDirtyPackagesTooltip", "Saves the current sequence"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Save")
			);

			ToolBarBuilder.AddToolBarButton(
				FUIAction(FExecuteAction::CreateSP(this, &SSequencer::OnSaveMovieSceneAsClicked)),
				NAME_None,
				LOCTEXT("SaveAs", "Save As"),
				LOCTEXT("SaveAsTooltip", "Saves the current sequence under a different name"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.SaveAs")
			);

			//ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().DiscardChanges );
			ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().FindInContentBrowser );
			ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().CreateCamera );
			ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().RenderMovie );
			ToolBarBuilder.AddSeparator("Level Sequence Separator");
		}

		ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().RestoreAnimatedState );

		ToolBarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &SSequencer::MakeGeneralMenu),
			LOCTEXT("GeneralOptions", "General Options"),
			LOCTEXT("GeneralOptionsToolTip", "General Options"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.GeneralOptions")
		);

		if (!bIsReadOnly)
		{
			ToolBarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateSP(this, &SSequencer::MakePlaybackMenu),
				LOCTEXT("PlaybackOptions", "Playback Options"),
				LOCTEXT("PlaybackOptionsToolTip", "Playback Options"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.PlaybackOptions")
			);

			ToolBarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateSP(this, &SSequencer::MakeSelectEditMenu),
				LOCTEXT("SelectEditOptions", "Select/Edit Options"),
				LOCTEXT("SelectEditOptionsToolTip", "Select/Edit Options"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.SelectEditOptions")
			);

			ToolBarBuilder.AddSeparator();

			if( SequencerPtr.Pin()->IsLevelEditorSequencer() )
			{
				TAttribute<FSlateIcon> KeyAllIcon;
				KeyAllIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda([&]{
					static FSlateIcon KeyAllEnabledIcon(FEditorStyle::GetStyleSetName(), "Sequencer.KeyAllEnabled");
					static FSlateIcon KeyAllDisabledIcon(FEditorStyle::GetStyleSetName(), "Sequencer.KeyAllDisabled");

					return SequencerPtr.Pin()->GetKeyAllEnabled() ? KeyAllEnabledIcon : KeyAllDisabledIcon;
				}));

				ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleKeyAllEnabled, NAME_None, TAttribute<FText>(), TAttribute<FText>(), KeyAllIcon );
			}

			if (IVREditorModule::Get().IsVREditorModeActive())
			{
				TAttribute<FSlateIcon> AutoChangeModeIcon;
				AutoChangeModeIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda( [&] {
					switch ( SequencerPtr.Pin()->GetAutoChangeMode() )
					{
					case EAutoChangeMode::AutoKey:
						return FSequencerCommands::Get().SetAutoKey->GetIcon();
					case EAutoChangeMode::AutoTrack:
						return FSequencerCommands::Get().SetAutoTrack->GetIcon();
					case EAutoChangeMode::All:
						return FSequencerCommands::Get().SetAutoChangeAll->GetIcon();
					default: // EAutoChangeMode::None
						return FSequencerCommands::Get().SetAutoChangeNone->GetIcon();
					}
				} ) );

				TAttribute<FText> AutoChangeModeToolTip;
				AutoChangeModeToolTip.Bind( TAttribute<FText>::FGetter::CreateLambda( [&] {
					switch ( SequencerPtr.Pin()->GetAutoChangeMode() )
					{
					case EAutoChangeMode::AutoKey:
						return FSequencerCommands::Get().SetAutoKey->GetDescription();
					case EAutoChangeMode::AutoTrack:
						return FSequencerCommands::Get().SetAutoTrack->GetDescription();
					case EAutoChangeMode::All:
						return FSequencerCommands::Get().SetAutoChangeAll->GetDescription();
					default: // EAutoChangeMode::None
						return FSequencerCommands::Get().SetAutoChangeNone->GetDescription();
					}
				} ) );
			
				ToolBarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateSP(this, &SSequencer::MakeAutoChangeMenu),
					LOCTEXT("AutoChangeMode", "Auto-Change Mode"),
					AutoChangeModeToolTip,
					AutoChangeModeIcon);
			}
			else
			{
				TAttribute<FSlateIcon> AutoKeyIcon;
				AutoKeyIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda([&]{
					static FSlateIcon AutoKeyEnabledIcon(FEditorStyle::GetStyleSetName(), "Sequencer.SetAutoKey");
					static FSlateIcon AutoKeyDisabledIcon(FEditorStyle::GetStyleSetName(), "Sequencer.SetAutoChangeNone");

					return SequencerPtr.Pin()->GetAutoChangeMode() == EAutoChangeMode::None ? AutoKeyDisabledIcon : AutoKeyEnabledIcon;
				}));

				ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleAutoKeyEnabled, NAME_None, TAttribute<FText>(), TAttribute<FText>(), AutoKeyIcon );
			}

			if( SequencerPtr.Pin()->IsLevelEditorSequencer() )
			{
				TAttribute<FSlateIcon> AllowEditsModeIcon;
				AllowEditsModeIcon.Bind(TAttribute<FSlateIcon>::FGetter::CreateLambda( [&] {
					switch ( SequencerPtr.Pin()->GetAllowEditsMode() )
					{
					case EAllowEditsMode::AllEdits:
						return FSequencerCommands::Get().AllowAllEdits->GetIcon();
					case EAllowEditsMode::AllowSequencerEditsOnly:
						return FSequencerCommands::Get().AllowSequencerEditsOnly->GetIcon();
					default: // EAllowEditsMode::AllowLevelEditsOnly
						return FSequencerCommands::Get().AllowLevelEditsOnly->GetIcon();
					}
				} ) );

				TAttribute<FText> AllowEditsModeToolTip;
				AllowEditsModeToolTip.Bind( TAttribute<FText>::FGetter::CreateLambda( [&] {
					switch ( SequencerPtr.Pin()->GetAllowEditsMode() )
					{
					case EAllowEditsMode::AllEdits:
						return FSequencerCommands::Get().AllowAllEdits->GetDescription();
					case EAllowEditsMode::AllowSequencerEditsOnly:
						return FSequencerCommands::Get().AllowSequencerEditsOnly->GetDescription();
					default: // EAllowEditsMode::AllowLevelEditsOnly
						return FSequencerCommands::Get().AllowLevelEditsOnly->GetDescription();
					}
				} ) );

				ToolBarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateSP(this, &SSequencer::MakeAllowEditsMenu),
					LOCTEXT("AllowMode", "Allow Edits"),
					AllowEditsModeToolTip,
					AllowEditsModeIcon);
			}
		}
	}
	ToolBarBuilder.EndSection();


	ToolBarBuilder.BeginSection("Snapping");
	{
		ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleIsSnapEnabled, NAME_None, TAttribute<FText>( FText::GetEmpty() ) );

		ToolBarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP( this, &SSequencer::MakeSnapMenu ),
			LOCTEXT( "SnapOptions", "Options" ),
			LOCTEXT( "SnapOptionsToolTip", "Snapping Options" ),
			TAttribute<FSlateIcon>(),
			true );

		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddWidget(
			SNew( SImage )
				.Image(FEditorStyle::GetBrush("Sequencer.Time.Small")) );

		ToolBarBuilder.AddWidget(
			SNew( SBox )
				.VAlign( VAlign_Center )
				[
					SNew( SNumericDropDown<float> )
						.DropDownValues( SequencerSnapValues::GetTimeSnapValues() )
						.bShowNamedValue(true)
						.ToolTipText( LOCTEXT( "TimeSnappingIntervalToolTip", "Time snapping interval" ) )
						.Value( this, &SSequencer::OnGetTimeSnapInterval )
						.OnValueChanged( this, &SSequencer::OnTimeSnapIntervalChanged )
				]);
	}
	ToolBarBuilder.EndSection();

	if (!bIsReadOnly)
	{
		// Curve editor doesn't have any notion of read-only at the moment
		ToolBarBuilder.BeginSection("Curve Editor");
		{
			ToolBarBuilder.AddToolBarButton( FSequencerCommands::Get().ToggleShowCurveEditor );
		}
		ToolBarBuilder.EndSection();
	}

	return ToolBarBuilder.MakeWidget();
}


void SSequencer::GetContextMenuContent(FMenuBuilder& MenuBuilder)
{
	// let toolkits populate the menu
	MenuBuilder.BeginSection("MainMenu");
	OnGetAddMenuContent.ExecuteIfBound(MenuBuilder, SequencerPtr.Pin().ToSharedRef());
	MenuBuilder.EndSection();

	// let track editors & object bindings populate the menu
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	// Always create the section so that we afford extension
	MenuBuilder.BeginSection("ObjectBindings");
	if (Sequencer.IsValid())
	{
		Sequencer->BuildAddObjectBindingsMenu(MenuBuilder);
	}
	MenuBuilder.EndSection();

	// Always create the section so that we afford extension
	MenuBuilder.BeginSection("AddTracks");
	if (Sequencer.IsValid())
	{
		Sequencer->BuildAddTrackMenu(MenuBuilder);
	}
	MenuBuilder.EndSection();
}


TSharedRef<SWidget> SSequencer::MakeAddMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr, AddMenuExtender);
	{
		GetContextMenuContent(MenuBuilder);
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeGeneralMenu()
{
	FMenuBuilder MenuBuilder( true, SequencerPtr.Pin()->GetCommandBindings() );
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	// view options
	MenuBuilder.BeginSection( "ViewOptions", LOCTEXT( "ViewMenuHeader", "View" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleLabelBrowser );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleCombinedKeyframes );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleChannelColors );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowPreAndPostRoll );

		if (Sequencer->IsLevelEditorSequencer())
		{
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().FindInContentBrowser );
		}

		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleExpandCollapseNodes );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleExpandCollapseNodesAndDescendants );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ExpandAllNodesAndDescendants );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().CollapseAllNodesAndDescendants );
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleShowGotoBox);

	MenuBuilder.AddMenuSeparator();

	if (SequencerPtr.Pin()->IsLevelEditorSequencer())
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().FixActorReferences);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().RebindPossessableReferences);
	}
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().FixFrameTiming);

	if ( SequencerPtr.Pin()->IsLevelEditorSequencer() )
	{
		MenuBuilder.AddMenuSeparator();
		
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ImportFBX );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ExportFBX );
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakePlaybackMenu()
{
	FMenuBuilder MenuBuilder( true, SequencerPtr.Pin()->GetCommandBindings() );

	// playback range options
	MenuBuilder.BeginSection("PlaybackThisSequence", LOCTEXT("PlaybackThisSequenceHeader", "Playback - This Sequence"));
	{
		// Menu entry for the start position
		auto OnStartChanged = [=](float NewValue){
			float Upper = SequencerPtr.Pin()->GetPlaybackRange().GetUpperBoundValue();
			SequencerPtr.Pin()->SetPlaybackRange(TRange<float>(FMath::Min(NewValue, Upper), Upper));
		};

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpinBox<float>)
						.TypeInterface(NumericTypeInterface)
						.IsEnabled_Lambda([=]() {
							return !SequencerPtr.Pin()->IsPlaybackRangeLocked();
						})
						.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
						.OnValueCommitted_Lambda([=](float Value, ETextCommit::Type){ OnStartChanged(Value); })
						.OnValueChanged_Lambda(OnStartChanged)
						.OnBeginSliderMovement(OnPlaybackRangeBeginDrag)
						.OnEndSliderMovement_Lambda([=](float Value){ OnStartChanged(Value); OnPlaybackRangeEndDrag.ExecuteIfBound(); })
						.MinValue_Lambda([=]() -> float {
							return SequencerPtr.Pin()->GetClampRange().GetLowerBoundValue(); 
						})
						.MaxValue_Lambda([=]() -> float {
							return SequencerPtr.Pin()->GetPlaybackRange().GetUpperBoundValue(); 
						})
						.Value_Lambda([=]() -> float {
							return SequencerPtr.Pin()->GetPlaybackRange().GetLowerBoundValue();
						})
				],
			LOCTEXT("PlaybackStartLabel", "Start"));

		// Menu entry for the end position
		auto OnEndChanged = [=](float NewValue){
			float Lower = SequencerPtr.Pin()->GetPlaybackRange().GetLowerBoundValue();
			SequencerPtr.Pin()->SetPlaybackRange(TRange<float>(Lower, FMath::Max(NewValue, Lower)));
		};

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpinBox<float>)
						.TypeInterface(NumericTypeInterface)
						.IsEnabled_Lambda([=]() {
							return !SequencerPtr.Pin()->IsPlaybackRangeLocked();
						})
						.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
						.OnValueCommitted_Lambda([=](float Value, ETextCommit::Type){ OnEndChanged(Value); })
						.OnValueChanged_Lambda(OnEndChanged)
						.OnBeginSliderMovement(OnPlaybackRangeBeginDrag)
						.OnEndSliderMovement_Lambda([=](float Value){ OnEndChanged(Value); OnPlaybackRangeEndDrag.ExecuteIfBound(); })
						.MinValue_Lambda([=]() -> float {
							return SequencerPtr.Pin()->GetPlaybackRange().GetLowerBoundValue(); 
						})
						.MaxValue_Lambda([=]() -> float {
							return SequencerPtr.Pin()->GetClampRange().GetUpperBoundValue(); 
						})
						.Value_Lambda([=]() -> float {
							return SequencerPtr.Pin()->GetPlaybackRange().GetUpperBoundValue();
						})
				],
			LOCTEXT("PlaybackStartEnd", "End"));

		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().TogglePlaybackRangeLocked );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleForceFixedFrameIntervalPlayback );

		if (SequencerPtr.Pin()->IsLevelEditorSequencer())
		{
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleRerunConstructionScripts );
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "PlaybackAllSequences", LOCTEXT( "PlaybackRangeAllSequencesHeader", "Playback Range - All Sequences" ) );
	{
		if (SequencerPtr.Pin()->IsLevelEditorSequencer())
		{
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleEvaluateSubSequencesInIsolation );
		}

		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleKeepCursorInPlaybackRangeWhileScrubbing );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleKeepCursorInPlaybackRange );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleKeepPlaybackRangeInSectionBounds );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleLinkCurveEditorTimeRange );

		// Menu entry for zero padding
		auto OnZeroPadChanged = [=](uint8 NewValue){
			Settings->SetZeroPadFrames(NewValue);
		};

		MenuBuilder.AddWidget(
			SNew(SHorizontalBox)	
			+ SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpinBox<uint8>)
					.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
					.OnValueCommitted_Lambda([=](uint8 Value, ETextCommit::Type){ OnZeroPadChanged(Value); })
					.OnValueChanged_Lambda(OnZeroPadChanged)
					.MinValue(0)
					.MaxValue(8)
					.Value_Lambda([=]() -> uint8 {
						return Settings->GetZeroPadFrames();
					})
				],
			LOCTEXT("ZeroPaddingText", "Zero Pad Frame Numbers"));
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SSequencer::MakeSelectEditMenu()
{
	FMenuBuilder MenuBuilder( true, SequencerPtr.Pin()->GetCommandBindings() );
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ToggleShowTransformBox);

	// selection range options
	MenuBuilder.BeginSection("SelectionRange", LOCTEXT("SelectionRangeHeader", "Selection Range"));
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetSelectionRangeStart);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetSelectionRangeEnd);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().ResetSelectionRange);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SelectKeysInSelectionRange);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SelectSectionsInSelectionRange);
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SelectAllInSelectionRange);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


TSharedRef<SWidget> SSequencer::MakeSnapMenu()
{
	FMenuBuilder MenuBuilder( false, SequencerPtr.Pin()->GetCommandBindings() );

	MenuBuilder.BeginSection("FramesRanges", LOCTEXT("SnappingMenuFrameRangesHeader", "Frame Ranges") );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleAutoScroll );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowFrameNumbers );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleShowRangeSlider );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "KeySnapping", LOCTEXT( "SnappingMenuKeyHeader", "Key Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeyTimesToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapKeyTimesToKeys );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "SectionSnapping", LOCTEXT( "SnappingMenuSectionHeader", "Section Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapSectionTimesToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapSectionTimesToSections );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "PlayTimeSnapping", LOCTEXT( "SnappingMenuPlayTimeHeader", "Play Time Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToInterval );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToKeys );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToPressedKey );
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapPlayTimeToDraggedKey );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "CurveSnapping", LOCTEXT( "SnappingMenuCurveHeader", "Curve Snapping" ) );
	{
		MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ToggleSnapCurveValueToInterval );
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


TSharedRef<SWidget> SSequencer::MakeAutoChangeMenu()
{
	FMenuBuilder MenuBuilder(false, SequencerPtr.Pin()->GetCommandBindings());

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoKey);

	if (SequencerPtr.Pin()->IsLevelEditorSequencer())
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoTrack);
	}

	if (IVREditorModule::Get().IsVREditorModeActive())
	{
		MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoChangeAll);
	}

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().SetAutoChangeNone);

	return MenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SSequencer::MakeAllowEditsMenu()
{
	FMenuBuilder MenuBuilder(false, SequencerPtr.Pin()->GetCommandBindings());

	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().AllowAllEdits);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().AllowSequencerEditsOnly);
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().AllowLevelEditsOnly);

	return MenuBuilder.MakeWidget();

}

TSharedRef<SWidget> SSequencer::MakeTimeRange(const TSharedRef<SWidget>& InnerContent, bool bShowWorkingRange, bool bShowViewRange, bool bShowPlaybackRange)
{
	ISequencerWidgetsModule& SequencerWidgets = FModuleManager::Get().LoadModuleChecked<ISequencerWidgetsModule>( "SequencerWidgets" );

	EShowRange ShowRange = EShowRange::None;
	if (bShowWorkingRange)
	{
		ShowRange |= EShowRange::WorkingRange;
	}
	if (bShowViewRange)
	{
		ShowRange |= EShowRange::ViewRange;
	}
	if (bShowPlaybackRange)
	{
		ShowRange |= EShowRange::PlaybackRange;
	}

	FTimeRangeArgs Args(
		ShowRange,
		TimeSliderController.ToSharedRef(),
		EVisibility::Visible,
		TAttribute<bool>(this, &SSequencer::ShowFrameNumbers),
		GetZeroPadNumericTypeInterface()
		);
	return SequencerWidgets.CreateTimeRange(Args, InnerContent);
}

TSharedPtr<ITimeSlider> SSequencer::GetTopTimeSliderWidget() const
{
	return TopTimeSlider;
}

SSequencer::~SSequencer()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
}


void SSequencer::RegisterActiveTimerForPlayback()
{
	if (!bIsActiveTimerRegistered)
	{
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSequencer::EnsureSlateTickDuringPlayback));
	}
}


EActiveTimerReturnType SSequencer::EnsureSlateTickDuringPlayback(double InCurrentTime, float InDeltaTime)
{
	if (SequencerPtr.IsValid())
	{
		auto PlaybackStatus = SequencerPtr.Pin()->GetPlaybackStatus();
		if (PlaybackStatus == EMovieScenePlayerStatus::Playing || PlaybackStatus == EMovieScenePlayerStatus::Recording || PlaybackStatus == EMovieScenePlayerStatus::Scrubbing)
		{
			return EActiveTimerReturnType::Continue;
		}
	}

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}


void RestoreSelectionState(const TArray<TSharedRef<FSequencerDisplayNode>>& DisplayNodes, TSet<FString>& SelectedPathNames, FSequencerSelection& SequencerSelection)
{
	for (TSharedRef<FSequencerDisplayNode> DisplayNode : DisplayNodes)
	{
		if (SelectedPathNames.Contains(DisplayNode->GetPathName()))
		{
			SequencerSelection.AddToSelection(DisplayNode);
		}

		RestoreSelectionState(DisplayNode->GetChildNodes(), SelectedPathNames, SequencerSelection);
	}
}

void RestoreSectionSelection(const TSet<TWeakObjectPtr<UMovieSceneSection> >& SelectedSections, FSequencerSelection& Selection)
{
	for (auto Section : SelectedSections)
	{
		if (Section.IsValid())
		{
			Selection.AddToSelection(Section.Get());
		}
	}
}

/** Attempt to restore key selection from the specified set of selected keys. Only works for key areas that have the same key handles as their expired counterparts (this is generally the case) */
void RestoreKeySelection(const TSet<FSequencerSelectedKey>& OldKeys, FSequencerSelection& Selection, FSequencerNodeTree& Tree)
{
	// Store a map of previous section/key area pairs to their current pairs
	TMap<FSequencerSelectedKey, FSequencerSelectedKey> OldToNew;

	for (FSequencerSelectedKey OldKeyTemplate : OldKeys)
	{
		// Cache of this key's handle for assignment to the new handle
		TOptional<FKeyHandle> OldKeyHandle = OldKeyTemplate.KeyHandle;
		// Reset the key handle so we can reuse cached section/key area pairs
		OldKeyTemplate.KeyHandle.Reset();

		FSequencerSelectedKey NewKeyTemplate = OldToNew.FindRef(OldKeyTemplate);
		if (!NewKeyTemplate.Section)
		{
			// Not cached yet, so we'll need to search for it
			for (const TSharedRef<FSequencerDisplayNode>& RootNode : Tree.GetRootNodes())
			{
				auto FindKeyArea =
					[&](FSequencerDisplayNode& InNode)
					{
						FSequencerSectionKeyAreaNode* KeyAreaNode = nullptr;

						if (InNode.GetType() == ESequencerNode::KeyArea)
						{
							KeyAreaNode = static_cast<FSequencerSectionKeyAreaNode*>(&InNode);
						}
						else if (InNode.GetType() == ESequencerNode::Track)
						{
							KeyAreaNode = static_cast<FSequencerTrackNode&>(InNode).GetTopLevelKeyNode().Get();
						}

						if (KeyAreaNode)
						{
							for (const TSharedRef<IKeyArea>& KeyArea : KeyAreaNode->GetAllKeyAreas())
							{
								if (KeyArea->GetOwningSection() == OldKeyTemplate.Section)
								{
									NewKeyTemplate.Section = OldKeyTemplate.Section;
									NewKeyTemplate.KeyArea = KeyArea;
									OldToNew.Add(OldKeyTemplate, NewKeyTemplate);
									// stop iterating
									return false;
								}
							}
						}
						return true;
					};
				
				// If the traversal returned false, we've found what we're looking for - no need to look at any more nodes
				if (!RootNode->Traverse_ParentFirst(FindKeyArea))
				{
					break;
				}
			}
		}

		// If we've got a curretn section/key area pair, we can add this key to the selection
		if (NewKeyTemplate.Section)
		{
			NewKeyTemplate.KeyHandle = OldKeyHandle;
			Selection.AddToSelection(NewKeyTemplate);
		}
	}
}

void SSequencer::UpdateLayoutTree()
{
	TrackArea->Empty();

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid() )
	{
		// Cache the selected path names so selection can be restored after the update.
		TSet<FString> SelectedPathNames;
		// Cache selected keys
		TSet<FSequencerSelectedKey> SelectedKeys = Sequencer->GetSelection().GetSelectedKeys();
		TSet<TWeakObjectPtr<UMovieSceneSection> > SelectedSections = Sequencer->GetSelection().GetSelectedSections();

		for (TSharedRef<const FSequencerDisplayNode> SelectedDisplayNode : Sequencer->GetSelection().GetSelectedOutlinerNodes().Array())
		{
			FString PathName = SelectedDisplayNode->GetPathName();
			if ( FName(*PathName).IsNone() == false )
			{
				SelectedPathNames.Add(PathName);
			}
		}

		// Suspend broadcasting selection changes because we don't want unnecessary rebuilds.
		Sequencer->GetSelection().SuspendBroadcast();
		Sequencer->GetSelection().Empty();

		// Update the node tree
		Sequencer->GetNodeTree()->Update();

		// Restore the selection state.
		RestoreSelectionState(Sequencer->GetNodeTree()->GetRootNodes(), SelectedPathNames, SequencerPtr.Pin()->GetSelection());	// Update to actor selection.

		// This must come after the selection state has been restored so that the tree and curve editor are populated with the correctly selected nodes
		TreeView->Refresh();
		CurveEditor->SetSequencerNodeTree(Sequencer->GetNodeTree());

		RestoreKeySelection(SelectedKeys, Sequencer->GetSelection(), *Sequencer->GetNodeTree());
		RestoreSectionSelection(SelectedSections, Sequencer->GetSelection());

		// Continue broadcasting selection changes
		Sequencer->GetSelection().ResumeBroadcast();
	}
}


void SSequencer::UpdateBreadcrumbs()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FMovieSceneSequenceID FocusedID = Sequencer->GetFocusedTemplateID();
	if (BreadcrumbTrail->PeekCrumb().BreadcrumbType == FSequencerBreadcrumb::ShotType)
	{
		BreadcrumbTrail->PopCrumb();
	}

	if( BreadcrumbTrail->PeekCrumb().BreadcrumbType == FSequencerBreadcrumb::MovieSceneType && BreadcrumbTrail->PeekCrumb().SequenceID != FocusedID )
	{
		FText CrumbName = Sequencer->GetFocusedMovieSceneSequence()->GetDisplayName();
		// The current breadcrumb is not a moviescene so we need to make a new breadcrumb in order return to the parent moviescene later
		BreadcrumbTrail->PushCrumb( CrumbName, FSequencerBreadcrumb( FocusedID ) );
	}
}


void SSequencer::ResetBreadcrumbs()
{
	BreadcrumbTrail->ClearCrumbs();
	BreadcrumbTrail->PushCrumb(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SSequencer::GetRootAnimationName)), FSequencerBreadcrumb(MovieSceneSequenceID::Root));
}

void SSequencer::PopBreadcrumb()
{
	BreadcrumbTrail->PopCrumb();
}

void SSequencer::OnOutlinerSearchChanged( const FText& Filter )
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid())
	{
		const FString FilterString = Filter.ToString();

		Sequencer->GetNodeTree()->FilterNodes( FilterString );
		TreeView->Refresh();

		if ( FilterString.StartsWith( TEXT( "label:" ) ) )
		{
			LabelBrowser->SetSelectedLabel(FilterString);
		}
		else
		{
			LabelBrowser->SetSelectedLabel( FString() );
		}
	}
}


float SSequencer::OnGetTimeSnapInterval() const
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid() )
	{
		return Sequencer->GetFixedFrameInterval();
	}	

	return 1.f;
}


void SSequencer::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// @todo sequencer: Add drop validity cue
}


void SSequencer::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	// @todo sequencer: Clear drop validity cue
}


FReply SSequencer::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bool bIsDragSupported = false;

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid() && (
		Operation->IsOfType<FAssetDragDropOp>() ||
		Operation->IsOfType<FClassDragDropOp>() ||
		Operation->IsOfType<FUnloadedClassDragDropOp>() ||
		Operation->IsOfType<FActorDragDropGraphEdOp>() ) )
	{
		bIsDragSupported = true;
	}

	return bIsDragSupported ? FReply::Handled() : FReply::Unhandled();
}


FReply SSequencer::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	bool bWasDropHandled = false;

	// @todo sequencer: Get rid of hard-code assumptions about dealing with ACTORS at this level?

	// @todo sequencer: We may not want any actor-specific code here actually.  We need systems to be able to
	// register with sequencer to support dropping assets/classes/actors, or OTHER types!

	// @todo sequencer: Handle drag and drop from other FDragDropOperations, including unloaded classes/asset and external drags!

	// @todo sequencer: Consider allowing drops into the level viewport to add to the MovieScene as well.
	//		- Basically, when Sequencer is open it would take over drops into the level and auto-add puppets for these instead of regular actors
	//		- This would let people drag smoothly and precisely into the view to drop assets/classes into the scene

	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();

	if (Operation.IsValid() )
	{
		if ( Operation->IsOfType<FAssetDragDropOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>( Operation );

			OnAssetsDropped( *DragDropOp );
			bWasDropHandled = true;
		}
		else if( Operation->IsOfType<FClassDragDropOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FClassDragDropOp>( Operation );

			OnClassesDropped( *DragDropOp );
			bWasDropHandled = true;
		}
		else if( Operation->IsOfType<FUnloadedClassDragDropOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FUnloadedClassDragDropOp>( Operation );

			OnUnloadedClassesDropped( *DragDropOp );
			bWasDropHandled = true;
		}
		else if( Operation->IsOfType<FActorDragDropGraphEdOp>() )
		{
			const auto& DragDropOp = StaticCastSharedPtr<FActorDragDropGraphEdOp>( Operation );

			OnActorsDropped( *DragDropOp );
			bWasDropHandled = true;
		}
	}

	return bWasDropHandled ? FReply::Handled() : FReply::Unhandled();
}


FReply SSequencer::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) 
{
	// A toolkit tab is active, so direct all command processing to it
	if( SequencerPtr.Pin()->GetCommandBindings()->ProcessCommandBindings( InKeyEvent ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSequencer::OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent )
{
	if (NewWidgetPath.ContainsWidget(AsShared()))
	{
		OnReceivedFocus.ExecuteIfBound();
	}
}

void SSequencer::OnAssetsDropped( const FAssetDragDropOp& DragDropOp )
{
	FSequencer& SequencerRef = *SequencerPtr.Pin();

	bool bObjectAdded = false;
	TArray< UObject* > DroppedObjects;
	bool bAllAssetsWereLoaded = true;

	for (const FAssetData& AssetData : DragDropOp.GetAssets())
	{
		UObject* Object = AssetData.GetAsset();

		if ( Object != nullptr )
		{
			DroppedObjects.Add( Object );
		}
		else
		{
			bAllAssetsWereLoaded = false;
		}
	}

	const TSet< TSharedRef<FSequencerDisplayNode> >& SelectedNodes = SequencerPtr.Pin()->GetSelection().GetSelectedOutlinerNodes();
	FGuid TargetObjectGuid;
	// if exactly one object node is selected, we have a target object guid
	TSharedPtr<const FSequencerDisplayNode> DisplayNode;
	if (SelectedNodes.Num() == 1)
	{
		for (TSharedRef<const FSequencerDisplayNode> SelectedNode : SelectedNodes )
		{
			DisplayNode = SelectedNode;
		}
		if (DisplayNode.IsValid() && DisplayNode->GetType() == ESequencerNode::Object)
		{
			TSharedPtr<const FSequencerObjectBindingNode> ObjectBindingNode = StaticCastSharedPtr<const FSequencerObjectBindingNode>(DisplayNode);
			TargetObjectGuid = ObjectBindingNode->GetObjectBinding();
		}
	}

	for( auto CurObjectIter = DroppedObjects.CreateConstIterator(); CurObjectIter; ++CurObjectIter )
	{
		UObject* CurObject = *CurObjectIter;

		if (!SequencerRef.OnHandleAssetDropped(CurObject, TargetObjectGuid))
		{
			SequencerRef.MakeNewSpawnable( *CurObject );
		}
		bObjectAdded = true;
	}

	if( bObjectAdded )
	{
		// Update the sequencers view of the movie scene data when any object is added
		SequencerRef.NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );

		// Update the tree and synchronize selection
		UpdateLayoutTree();

		SequencerRef.SynchronizeSequencerSelectionWithExternalSelection();
	}
}


void SSequencer::OnClassesDropped( const FClassDragDropOp& DragDropOp )
{
	FSequencer& SequencerRef = *SequencerPtr.Pin();

	for( auto ClassIter = DragDropOp.ClassesToDrop.CreateConstIterator(); ClassIter; ++ClassIter )
	{
		UClass* Class = ( *ClassIter ).Get();
		if( Class != nullptr )
		{
			UObject* Object = Class->GetDefaultObject();

			FGuid NewGuid = SequencerRef.MakeNewSpawnable( *Object );
		}
	}
}


void SSequencer::OnUnloadedClassesDropped( const FUnloadedClassDragDropOp& DragDropOp )
{
	FSequencer& SequencerRef = *SequencerPtr.Pin();
	for( auto ClassDataIter = DragDropOp.AssetsToDrop->CreateConstIterator(); ClassDataIter; ++ClassDataIter )
	{
		auto& ClassData = *ClassDataIter;

		// Check to see if the asset can be found, otherwise load it.
		UObject* Object = FindObject<UObject>( nullptr, *ClassData.AssetName );
		if( Object == nullptr )
		{
			Object = FindObject<UObject>(nullptr, *FString::Printf(TEXT("%s.%s"), *ClassData.GeneratedPackageName, *ClassData.AssetName));
		}

		if( Object == nullptr )
		{
			// Load the package.
			GWarn->BeginSlowTask( LOCTEXT("OnDrop_FullyLoadPackage", "Fully Loading Package For Drop"), true, false );
			UPackage* Package = LoadPackage(nullptr, *ClassData.GeneratedPackageName, LOAD_NoRedirects );
			if( Package != nullptr )
			{
				Package->FullyLoad();
			}
			GWarn->EndSlowTask();

			Object = FindObject<UObject>(Package, *ClassData.AssetName);
		}

		if( Object != nullptr )
		{
			// Check to see if the dropped asset was a blueprint
			if(Object->IsA(UBlueprint::StaticClass()))
			{
				// Get the default object from the generated class.
				Object = Cast<UBlueprint>(Object)->GeneratedClass->GetDefaultObject();
			}
		}

		if( Object != nullptr )
		{
			FGuid NewGuid = SequencerRef.MakeNewSpawnable( *Object );
		}
	}
}


void SSequencer::OnActorsDropped( FActorDragDropGraphEdOp& DragDropOp )
{
	SequencerPtr.Pin()->OnActorsDropped( DragDropOp.Actors );
}


void SSequencer::OnCrumbClicked(const FSequencerBreadcrumb& Item)
{
	if (Item.BreadcrumbType != FSequencerBreadcrumb::ShotType)
	{
		if( SequencerPtr.Pin()->GetFocusedTemplateID() == Item.SequenceID ) 
		{
			// then do zooming
		}
		else
		{
			if (SequencerPtr.Pin()->GetShowCurveEditor())
			{
				SequencerPtr.Pin()->SetShowCurveEditor(false);
			}

			SequencerPtr.Pin()->PopToSequenceInstance( Item.SequenceID );
		}
	}
}


FText SSequencer::GetRootAnimationName() const
{
	return SequencerPtr.Pin()->GetRootMovieSceneSequence()->GetDisplayName();
}


TSharedPtr<SSequencerTreeView> SSequencer::GetTreeView() const
{
	return TreeView;
}


TArray<FSectionHandle> SSequencer::GetSectionHandles(const TSet<TWeakObjectPtr<UMovieSceneSection>>& DesiredSections) const
{
	TArray<FSectionHandle> SectionHandles;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer.IsValid())
	{
		// @todo sequencer: this is potentially slow as it traverses the entire tree - there's scope for optimization here
		for (auto& Node : Sequencer->GetNodeTree()->GetRootNodes())
		{
			Node->Traverse_ParentFirst([&](FSequencerDisplayNode& InNode) {
				if (InNode.GetType() == ESequencerNode::Track)
				{
					FSequencerTrackNode& TrackNode = static_cast<FSequencerTrackNode&>(InNode);

					const auto& AllSections = TrackNode.GetSections();
					for (int32 Index = 0; Index < AllSections.Num(); ++Index)
					{
						if (DesiredSections.Contains(TWeakObjectPtr<UMovieSceneSection>(AllSections[Index]->GetSectionObject())))
						{
							SectionHandles.Emplace(StaticCastSharedRef<FSequencerTrackNode>(TrackNode.AsShared()), Index);
						}
					}
				}
				return true;
			});
		}
	}

	return SectionHandles;
}


void SSequencer::OnSaveMovieSceneClicked()
{
	SequencerPtr.Pin()->SaveCurrentMovieScene();
}


void SSequencer::OnSaveMovieSceneAsClicked()
{
	SequencerPtr.Pin()->SaveCurrentMovieSceneAs();
}


void SSequencer::StepToNextKey()
{
	StepToKey(true, false);
}


void SSequencer::StepToPreviousKey()
{
	StepToKey(false, false);
}


void SSequencer::StepToNextCameraKey()
{
	StepToKey(true, true);
}


void SSequencer::StepToPreviousCameraKey()
{
	StepToKey(false, true);
}


void SSequencer::StepToKey(bool bStepToNextKey, bool bCameraOnly)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid() )
	{
		TSet< TSharedRef<FSequencerDisplayNode> > Nodes;

		if ( bCameraOnly )
		{
			TSet<TSharedRef<FSequencerDisplayNode>> RootNodes( Sequencer->GetNodeTree()->GetRootNodes() );

			TSet<TWeakObjectPtr<AActor> > LockedActors;
			for ( int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i )
			{
				FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
				if ( LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown )
				{
					TWeakObjectPtr<AActor> ActorLock = LevelVC->GetActiveActorLock();
					if ( ActorLock.IsValid() )
					{
						LockedActors.Add( ActorLock );
					}
				}
			}

			for ( auto RootNode : RootNodes )
			{
				TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = StaticCastSharedRef<FSequencerObjectBindingNode>( RootNode );

				for (TWeakObjectPtr<>& Object : Sequencer->FindObjectsInCurrentSequence(ObjectBindingNode->GetObjectBinding()))
				{
					AActor* RuntimeActor = Cast<AActor>( Object.Get() );
					if ( RuntimeActor != nullptr && LockedActors.Contains( RuntimeActor ) )
					{
						Nodes.Add( RootNode );
					}
				}
			}
		}
		else
		{
			const TSet< TSharedRef<FSequencerDisplayNode> >& SelectedNodes = Sequencer->GetSelection().GetSelectedOutlinerNodes();
			Nodes = SelectedNodes;

			if ( Nodes.Num() == 0 )
			{
				TSet<TSharedRef<FSequencerDisplayNode>> RootNodes( Sequencer->GetNodeTree()->GetRootNodes() );
				for ( auto RootNode : RootNodes )
				{
					Nodes.Add( RootNode );

					SequencerHelpers::GetDescendantNodes( RootNode, Nodes );
				}
			}
		}

		if ( Nodes.Num() > 0 )
		{
			float ClosestKeyDistance = MAX_FLT;
			float CurrentTime = Sequencer->GetLocalTime();
			float StepToTime = 0;
			bool StepToKeyFound = false;

			auto It = Nodes.CreateConstIterator();
			bool bExpand = !( *It ).Get().IsExpanded();

			for ( auto Node : Nodes )
			{
				TArray<float> AllTimes;

				TSet<TSharedPtr<IKeyArea>> KeyAreas;
				SequencerHelpers::GetAllKeyAreas( Node, KeyAreas );
				for ( TSharedPtr<IKeyArea> KeyArea : KeyAreas )
				{
					for ( FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles() )
					{
						float KeyTime = KeyArea->GetKeyTime( KeyHandle );
						AllTimes.Add(KeyTime);
					}
				}

				TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
				SequencerHelpers::GetAllSections( Node, Sections );				
				for ( TWeakObjectPtr<UMovieSceneSection> Section : Sections )
				{
					if (Section.IsValid() && !Section->IsInfinite())
					{
						AllTimes.Add(Section->GetStartTime());
						AllTimes.Add(Section->GetEndTime());
					}
				}

				for (float Time : AllTimes)
				{
					if ( bStepToNextKey )
					{
						if ( Time > CurrentTime && Time - CurrentTime < ClosestKeyDistance )
						{
							StepToTime = Time;
							ClosestKeyDistance = Time - CurrentTime;
							StepToKeyFound = true;
						}
					}
					else
					{
						if ( Time < CurrentTime && CurrentTime - Time < ClosestKeyDistance )
						{
							StepToTime = Time;
							ClosestKeyDistance = CurrentTime - Time;
							StepToKeyFound = true;
						}
					}
				}
			}

			if ( StepToKeyFound )
			{
				Sequencer->SetLocalTime( StepToTime );
			}
		}
	}
}


EVisibility SSequencer::GetBreadcrumbTrailVisibility() const
{
	return SequencerPtr.Pin()->IsLevelEditorSequencer() ? EVisibility::Visible : EVisibility::Collapsed;
}


EVisibility SSequencer::GetCurveEditorToolBarVisibility() const
{
	return SequencerPtr.Pin()->GetShowCurveEditor() ? EVisibility::Visible : EVisibility::Collapsed;
}


EVisibility SSequencer::GetBottomTimeSliderVisibility() const
{
	return Settings->GetShowRangeSlider() ? EVisibility::Hidden : EVisibility::Visible;
}


EVisibility SSequencer::GetTimeRangeVisibility() const
{
	return Settings->GetShowRangeSlider() ? EVisibility::Visible : EVisibility::Hidden;
}


bool SSequencer::ShowFrameNumbers() const
{
	return SequencerPtr.Pin()->CanShowFrameNumbers() && Settings->GetShowFrameNumbers();
}


float SSequencer::GetOutlinerSpacerFill() const
{
	const float Column1Coeff = GetColumnFillCoefficient(1);
	return SequencerPtr.Pin()->GetShowCurveEditor() ? Column1Coeff / (1 - Column1Coeff) : 0.f;
}


void SSequencer::OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex)
{
	ColumnFillCoefficients[ColumnIndex] = FillCoefficient;
}


EVisibility SSequencer::GetTrackAreaVisibility() const
{
	return SequencerPtr.Pin()->GetShowCurveEditor() ? EVisibility::Collapsed : EVisibility::Visible;
}


EVisibility SSequencer::GetCurveEditorVisibility() const
{
	return SequencerPtr.Pin()->GetShowCurveEditor() ? EVisibility::Visible : EVisibility::Collapsed;
}


void SSequencer::OnCurveEditorVisibilityChanged()
{
	if (CurveEditor.IsValid())
	{
		if (!Settings->GetLinkCurveEditorTimeRange())
		{
			TRange<float> ClampRange = SequencerPtr.Pin()->GetClampRange();
			if (CachedClampRange.IsEmpty())
			{
				CachedClampRange = ClampRange;
			}
			SequencerPtr.Pin()->SetClampRange(CachedClampRange);
			CachedClampRange = ClampRange;

			TRange<float> ViewRange = SequencerPtr.Pin()->GetViewRange();
			if (CachedViewRange.IsEmpty())
			{
				CachedViewRange = ViewRange;
			}
			SequencerPtr.Pin()->SetViewRange(CachedViewRange);
			CachedViewRange = ViewRange;
		}

		// Only zoom horizontally if the editor is visible
		CurveEditor->SetAllowAutoFrame(SequencerPtr.Pin()->GetShowCurveEditor());

		if (CurveEditor->GetAutoFrame())
		{
			CurveEditor->ZoomToFit();
		}
	}

	TreeView->UpdateTrackArea();
}


void SSequencer::OnTimeSnapIntervalChanged(float InInterval)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid() )
	{
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		if (!FMath::IsNearlyEqual(MovieScene->GetFixedFrameInterval(), InInterval))
		{
			FScopedTransaction SetFixedFrameIntervalTransaction( NSLOCTEXT( "Sequencer", "SetFixedFrameInterval", "Set scene fixed frame interval" ) );
			MovieScene->Modify();
			MovieScene->SetFixedFrameInterval( InInterval );

			// Update the current time to the new interval
			float NewTime = SequencerHelpers::SnapTimeToInterval(Sequencer->GetLocalTime(), InInterval);
			Sequencer->SetLocalTime(NewTime);
		}
	}
}


FPaintPlaybackRangeArgs SSequencer::GetSectionPlaybackRangeArgs() const
{
	if (GetBottomTimeSliderVisibility() == EVisibility::Visible)
	{
		static FPaintPlaybackRangeArgs Args(FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_L"), FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_R"), 6.f);
		return Args;
	}
	else
	{
		static FPaintPlaybackRangeArgs Args(FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L"), FEditorStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R"), 6.f);
		return Args;
	}
}


FVirtualTrackArea SSequencer::GetVirtualTrackArea() const
{
	return FVirtualTrackArea(*SequencerPtr.Pin(), *TreeView.Get(), TrackArea->GetCachedGeometry());
}

FPasteContextMenuArgs SSequencer::GeneratePasteArgs(float PasteAtTime, TSharedPtr<FMovieSceneClipboard> Clipboard)
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Settings->GetIsSnapEnabled())
	{
		PasteAtTime = SequencerHelpers::SnapTimeToInterval(PasteAtTime, Sequencer->GetFixedFrameInterval());
	}

	// Open a paste menu at the current mouse position
	FSlateApplication& Application = FSlateApplication::Get();
	FVector2D LocalMousePosition = TrackArea->GetCachedGeometry().AbsoluteToLocal(Application.GetCursorPos());

	FVirtualTrackArea VirtualTrackArea = GetVirtualTrackArea();

	// Paste into the currently selected sections, or hit test the mouse position as a last resort
	TArray<TSharedRef<FSequencerDisplayNode>> PasteIntoNodes;
	{
		TSet<TWeakObjectPtr<UMovieSceneSection>> Sections = Sequencer->GetSelection().GetSelectedSections();
		for (const FSequencerSelectedKey& Key : Sequencer->GetSelection().GetSelectedKeys())
		{
			Sections.Add(Key.Section);
		}

		for (const FSectionHandle& Handle : GetSectionHandles(Sections))
		{
			PasteIntoNodes.Add(Handle.TrackNode.ToSharedRef());
		}
	}

	if (PasteIntoNodes.Num() == 0)
	{
		TSharedPtr<FSequencerDisplayNode> Node = VirtualTrackArea.HitTestNode(LocalMousePosition.Y);
		if (Node.IsValid())
		{
			PasteIntoNodes.Add(Node.ToSharedRef());
		}
	}

	return FPasteContextMenuArgs::PasteInto(MoveTemp(PasteIntoNodes), PasteAtTime, Clipboard);
}

void SSequencer::OnPaste()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Sequencer->GetSelection().GetSelectedOutlinerNodes();
	if (SelectedNodes.Num() == 0)
	{
		if (OpenPasteMenu())
		{
			return;
		}
	}

	PasteTracks();
}

bool SSequencer::CanPaste()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	TSet<TSharedRef<FSequencerDisplayNode>> SelectedNodes = Sequencer->GetSelection().GetSelectedOutlinerNodes();
	if (SelectedNodes.Num() != 0)
	{
		FString TexttoImport;
		FPlatformApplicationMisc::ClipboardPaste(TexttoImport);

		if (Sequencer->CanPaste(TexttoImport))
		{
			TArray<UMovieSceneTrack*> ImportedTrack;
			Sequencer->ImportTracksFromText(TexttoImport, ImportedTrack);
			if (ImportedTrack.Num() == 0)
			{
				return false;
			}

			for (TSharedRef<FSequencerDisplayNode> Node : SelectedNodes)
			{
				if (Node->GetType() == ESequencerNode::Object)
				{
					return true;
				}
			}
			return false;
		}
	}

	return SequencerPtr.Pin()->GetClipboardStack().Num() != 0;
}

void SSequencer::PasteTracks()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	Sequencer->PasteCopiedTracks();
}

bool SSequencer::OpenPasteMenu()
{
	TSharedPtr<FPasteContextMenu> ContextMenu;

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer->GetClipboardStack().Num() != 0)
	{
		FPasteContextMenuArgs Args = GeneratePasteArgs(Sequencer->GetLocalTime(), Sequencer->GetClipboardStack().Last());
		ContextMenu = FPasteContextMenu::CreateMenu(*Sequencer, Args);
	}

	if (!ContextMenu.IsValid() || !ContextMenu->IsValidPaste())
	{
		return false;
	}
	else if (ContextMenu->AutoPaste())
	{
		return false;
	}

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, SequencerPtr.Pin()->GetCommandBindings());

	ContextMenu->PopulateMenu(MenuBuilder);

	FWidgetPath Path;
	FSlateApplication::Get().FindPathToWidget(AsShared(), Path);
	
	FSlateApplication::Get().PushMenu(
		AsShared(),
		Path,
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

	return true;
}

void SSequencer::PasteFromHistory()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if (Sequencer->GetClipboardStack().Num() == 0)
	{
		return;
	}

	FPasteContextMenuArgs Args = GeneratePasteArgs(Sequencer->GetLocalTime());
	TSharedPtr<FPasteFromHistoryContextMenu> ContextMenu = FPasteFromHistoryContextMenu::CreateMenu(*Sequencer, Args);

	if (ContextMenu.IsValid())
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Sequencer->GetCommandBindings());

		ContextMenu->PopulateMenu(MenuBuilder);

		FWidgetPath Path;
		FSlateApplication::Get().FindPathToWidget(AsShared(), Path);
		
		FSlateApplication::Get().PushMenu(
			AsShared(),
			Path,
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
	}
}

void SSequencer::OnSequenceInstanceActivated( FMovieSceneSequenceIDRef ActiveInstanceID )
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	if ( Sequencer.IsValid() )
	{
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		if ( MovieScene->GetFixedFrameInterval() == 0 )
		{
			MovieScene->Modify();
			MovieScene->SetFixedFrameInterval(Settings->GetTimeSnapInterval());

			// Update the current time to the new interval
			float NewTime = SequencerHelpers::SnapTimeToInterval(Sequencer->GetLocalTime(), Settings->GetTimeSnapInterval());
			Sequencer->SetLocalTime(NewTime);
		}
	}
}

EVisibility SSequencer::GetDebugVisualizerVisibility() const
{
	return Settings->ShouldShowDebugVisualization() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE

